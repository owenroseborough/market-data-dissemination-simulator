#ifndef ORDERBOOK_H
#define ORDERBOOK_H

#include "common_includes.h"

enum class OrderType {
	GoodTillCancel,
	FillAndKill
};

enum class Side {
	Buy,
	Sell
};

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderId = std::uint64_t;


struct LevelInfo {
	Price price_;
	Quantity quantity_;
};

using LevelInfos = std::vector<LevelInfo>;

class OrderBookLevelInfos {
private:
	LevelInfos bids_;
	LevelInfos asks_;
public:
	OrderBookLevelInfos(const LevelInfos& bids, const LevelInfos& asks);

	const LevelInfos& GetBids() const;
	const LevelInfos& GetAsks() const;
};

class Order {
private:
	OrderType orderType_;
	OrderId orderId_;
	Side side_;
	Price price_;
	Quantity quantity_;
	Quantity initialQuantity_;
	Quantity remainingQuantity_;
public:
	Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity);

	OrderId GetOrderId() const;
	Side GetSide() const;
	Price GetPrice() const;
	OrderType GetOrderType() const;
	Quantity GetInitialQuantity() const;
	Quantity GetRemainingQuantity() const;
	Quantity GetFilledQuantity() const;
	bool IsFilled() const;

	void Fill(Quantity quantity);
};

class OrderModify {
private:
	OrderType orderType_;
	OrderId orderId_;
	Side side_;
	Price price_;
	Quantity quantity_;
public:
	OrderModify(OrderId orderId, Side side, Price price, Quantity quantity);
	OrderId GetOrderId() const;
	Side GetSide() const;
	Price GetPrice() const;
	Quantity GetQuantity() const;

	OrderPointer ToOrderPointer(OrderType type) const;
};

class Trade {
private:
	TradeInfo bidTrade_;
	TradeInfo askTrade_;
public:
	Trade(const TradeInfo& bidTrade, const TradeInfo& askTrade);
	const TradeInfo& GetBidTrade() const;
	const TradeInfo& GetAskTrade() const;
};

using Trades = std::vector<Trade>;
using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

struct TradeInfo {
	OrderId orderId_;
	Price price_;
	Quantity quantity_;
};

class OrderBook {
private:
	struct OrderEntry {
		OrderPointer order_{ nullptr };
		OrderPointers::iterator location_;

	};

	std::map<Price, OrderPointers, std::greater<Price>> bids_;
	std::map<Price, OrderPointers, std::less<Price>> asks_;
	std::unordered_map<OrderId, OrderEntry> orders_;

	bool CanMatch(Side side, Price price) const;
	Trades MatchOrders();
public:
	Trades AddOrder(OrderPointer order);
	void CancelOrder(OrderId orderId);
	Trades MatchOrder(OrderModify order);
	std::size_t Size() const;
	OrderBookLevelInfos GetOrderInfos() const;
};

#endif // ORDERBOOK_H

