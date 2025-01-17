
#include "common_includes.h"
#include "OrderBook.h"

using namespace std;

OrderBookLevelInfos::OrderBookLevelInfos(const LevelInfos& bids, const LevelInfos& asks)
	: bids_{ bids }
	, asks_{ asks }
{}
const LevelInfos& OrderBookLevelInfos::GetBids() const { return bids_; }
const LevelInfos& OrderBookLevelInfos::GetAsks() const { return asks_; }

Order::Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity)
	: orderType_{ orderType }
	, orderId_{ orderId }
	, side_{ side }
	, price_{ price }
	, initialQuantity_{ quantity }
	, remainingQuantity_{ quantity }
{ }
OrderId Order::GetOrderId() const { return orderId_; }
Side Order::GetSide() const { return side_; }
Price Order::GetPrice() const { return price_; }
OrderType Order::GetOrderType() const { return orderType_; }
Quantity Order::GetInitialQuantity() const { return initialQuantity_; }
Quantity Order::GetRemainingQuantity() const { return remainingQuantity_; }
Quantity Order::GetFilledQuantity() const { return GetInitialQuantity() - GetRemainingQuantity(); }
bool Order::IsFilled() const { return GetRemainingQuantity() == 0; }

void Order::Fill(Quantity quantity) {
	if (quantity > GetRemainingQuantity()) {
		throw std::logic_error(std::format("Order ({}) cannot be filled for more than its remaining quantity.", GetOrderId()));
	}
	remainingQuantity_ -= quantity;
}

OrderModify::OrderModify(OrderId orderId, Side side, Price price, Quantity quantity)
	: orderId_{ orderId }
	, side_{ side }
	, price_{ price }
	, quantity_{ quantity }
{ }
OrderId OrderModify::GetOrderId() const { return orderId_; }
Side OrderModify::GetSide() const { return side_; }
Price OrderModify::GetPrice() const { return price_; }
Quantity OrderModify::GetQuantity() const { return quantity_; }

OrderPointer OrderModify::ToOrderPointer(OrderType type) const {
	return std::make_shared<Order>(type, GetOrderId(), GetSide(), GetPrice(), GetQuantity());
}

Trade::Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade)
	: bidTrade_{ bidTrade }
	, askTrade_{ askTrade }
{ }
const TradeInfo& Trade::GetBidTrade() const { return bidTrade_; }
const TradeInfo& Trade::GetAskTrade() const { return askTrade_; }

using Trades = std::vector<Trade>;
using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

bool OrderBook::CanMatch(Side side, Price price) const {
	if (side == Side::Buy) {
		if (asks_.empty()) 
			return false;
			
		const auto& [bestAsk, _] = *asks_.begin();
		return price >= bestAsk;
	}
	else {
		if (bids_.empty())
			return false;

		const auto& [bestBid, _] = *bids_.begin();
		return price <= bestBid;
	}
}
Trades OrderBook::MatchOrders() {
	Trades trades;
	trades.reserve(orders_.size());

	while (true) {
		if (bids_.empty() || asks_.empty())
			break;

		auto& [bidPrice, bids] = *bids_.begin();
		auto& [askPrice, asks] = *asks_.begin();

		if (bidPrice < askPrice)
			break;

		while (bids.size() && asks.size()) {
			auto& bid = bids.front();
			auto& ask = asks.front();

			Quantity quantity = std::min(bid->GetRemainingQuantity(), ask->GetRemainingQuantity());

			bid->Fill(quantity);
			ask->Fill(quantity);

			if (bid->IsFilled())
			{
				bids.pop_front();
				orders_.erase(bid->GetOrderId());
			}

			if (ask->IsFilled())
			{
				asks.pop_front();
				orders_.erase(ask->GetOrderId());
			}

			if (bids.empty())
				bids_.erase(bidPrice);

			if (asks.empty())
				asks_.erase(askPrice);

			trades.push_back(Trade{ 
				TradeInfo{ bid->GetOrderId(), bid->GetPrice(), quantity },
				TradeInfo{ ask->GetOrderId(), ask->GetPrice(), quantity }
				});
		}
	}
	if (!bids_.empty()) {

		auto& [_, bids] = *bids_.begin();
		auto& order = bids.front();

		if (order->GetOrderType() == OrderType::FillAndKill)
			CancelOrder(order->GetOrderId());
	}
	if (!asks_.empty()) {

		auto& [_, asks] = *asks_.begin();
		auto& order = asks.front();

		if (order->GetOrderType() == OrderType::FillAndKill)
			CancelOrder(order->GetOrderId());
	}
	return trades;
}

Trades OrderBook::AddOrder(OrderPointer order) {
	if (orders_.contains(order->GetOrderId()))
		return {};

	if (order->GetOrderType() == OrderType::FillAndKill && !CanMatch(order->GetSide(), order->GetPrice()))
		return {};

	OrderPointers::iterator iterator;

	if (order->GetSide() == Side::Buy) {
		auto& orders = bids_[order->GetPrice()];
		orders.push_back(order);
		iterator = std::next(orders.begin(), orders.size() - 1);
	}
	else {
		auto& orders = asks_[order->GetPrice()];
		orders.push_back(order);
		iterator = std::next(orders.begin(), orders.size() - 1);
	}

	orders_.insert({ order->GetOrderId(), OrderEntry{ order, iterator} });

	return MatchOrders();
}
void OrderBook::CancelOrder(OrderId orderId) {
	if (!orders_.contains(orderId))
		return;

	const auto& [order, orderIterator] = orders_.at(orderId);
	orders_.erase(orderId);

	if (order->GetSide() == Side::Sell) {
		auto price = order->GetPrice();
		auto& orders = asks_.at(price);
		orders.erase(orderIterator);
		if (orders.empty())
			asks_.erase(price);
	}
	else {
		auto price = order->GetPrice();
		auto& orders = bids_.at(price);
		orders.erase(orderIterator);
		if (orders.empty())
			bids_.erase(price);
	}
}
Trades OrderBook::MatchOrder(OrderModify order) {
	if (!orders_.contains(order.GetOrderId()))
		return {};
		
	const auto& [existingOrder, _] = orders_.at(order.GetOrderId());
	CancelOrder(order.GetOrderId());
	return AddOrder(order.ToOrderPointer(existingOrder->GetOrderType()));
}
std::size_t OrderBook::Size() const { return orders_.size(); }

OrderBookLevelInfos OrderBook::GetOrderInfos() const {
	LevelInfos bidInfos, askInfos;
	bidInfos.reserve(orders_.size());
	askInfos.reserve(orders_.size());

	auto CreateLevelInfos = [](Price price, const OrderPointers& orders) {
		return LevelInfo{ price, std::accumulate(orders.begin(), orders.end(), (Quantity)0,
			[](Quantity runningSum, const OrderPointer& order)
			{return runningSum + order->GetRemainingQuantity(); }) };
		};

	for (const auto& [price, orders] : bids_) {
		bidInfos.push_back(CreateLevelInfos(price, orders));
	}
	for (const auto& [price, orders] : asks_) {
		askInfos.push_back(CreateLevelInfos(price, orders));
	}

	return OrderBookLevelInfos{ bidInfos, askInfos};
}

auto OrderBook::GenerateRandomOrder() {

	srand(time(0));
	size_t randomType = rand() % 2;
	OrderType orderType = (randomType == 0) ? OrderType::FillAndKill : OrderType::GoodTillCancel;
	
	uint64_t orderId = rand() % 1001;
	
	size_t randomSide = rand() % 2;
	Side orderSide = (randomSide == 0) ? Side::Buy : Side::Sell;
	
	int32_t orderPrice = (rand() % 10) + 1;

	uint32_t orderQuantity = (rand() % 100) + 1;
	
	shared_ptr<Order> randomOrder = make_shared<Order>(orderType, orderId, orderSide, orderPrice, orderQuantity);

	return AddOrder(randomOrder);
}

