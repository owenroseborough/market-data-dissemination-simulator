#ifndef ORDERBOOK_MANAGER_H
#define ORDERBOOK_MANAGER_H

#include "common_includes.h"
#include "OrderBook.h"

using namespace std;
using Symbol = string;

class OrderBookManager {
private:
	unordered_map<Symbol, shared_ptr<OrderBook>> orderBookMap;
	unordered_map<Symbol, size_t>                orderBookDepth;   // how many levels on bids/asks to desseminate to client
public:
	bool AddSymbol(Symbol symbol, size_t depth);
	bool RemoveSymbol(Symbol symbol);
	shared_ptr<OrderBook> GetOrderBook(Symbol symbol) const;
	size_t GetOrderBookDepth(Symbol symbol) const;
};

#endif
