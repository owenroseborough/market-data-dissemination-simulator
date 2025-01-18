// the order book manager keeps a map of orderbooks
// orderbook map keyed on symbol, value as orderbook
// returns pointer to correct orderbook for server to use

#include "common_includes.h"
#include "OrderBook.h"
#include "OrderBookManager.h"

using namespace std;
using Symbol = string;

bool OrderBookManager::AddSymbol(Symbol symbol, size_t depth) {

	auto result = orderBookMap.insert(pair(symbol, make_shared<OrderBook>()));
	orderBookDepth.insert(pair(symbol, depth));
	if (result.second) {
		return true;
	}
	else {
		return false;
	}
}
bool OrderBookManager::RemoveSymbol(Symbol symbol) {

	if (orderBookMap.contains(symbol) && orderBookDepth.contains(symbol)) {
		orderBookMap.erase(symbol);
		orderBookDepth.erase(symbol);
		return true;
	}
	else {
		return false;
	}
}
shared_ptr<OrderBook> OrderBookManager::GetOrderBook(Symbol symbol) const {
	if (orderBookMap.contains(symbol)) {
		return orderBookMap.at(symbol);
	}
	else {
		return {};
	}
}
size_t OrderBookManager::GetOrderBookDepth(Symbol symbol) const {
	if (orderBookDepth.contains(symbol)) {
		return orderBookDepth.at(symbol);
	}
	else {
		return {};
	}
}
