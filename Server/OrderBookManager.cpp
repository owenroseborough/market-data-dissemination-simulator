// the order book manager keeps a map of orderbooks
// orderbook map keyed on symbol, value as orderbook
// returns pointer to correct orderbook for server to use

#include "common_includes.h"
#include "OrderBook.h"

using namespace std;
using Symbol = string;

class OrderBookManager {
private:
	unordered_map<Symbol, shared_ptr<OrderBook>> orderBookMap;
	unordered_map<Symbol, shared_ptr<size_t>>    orderBookDepth;   // how many levels on bids/asks to desseminate to client
public:
	bool AddSymbol(Symbol symbol, size_t depth) {

		auto result = orderBookMap.insert({ symbol, make_shared<OrderBook>() });
		orderBookDepth.insert({ symbol, make_shared<size_t>(depth) });
		if (result.second) {
			return true;
		}
		else {
			return false;
		}
	}
	bool RemoveSymbol(Symbol symbol) {

		if (orderBookMap.contains(symbol)) {
			orderBookMap.erase(symbol);
			return true;
		}
		else {
			return false;
		}
	}
	shared_ptr<OrderBook> GetOrderBook(Symbol) {

	}
};

