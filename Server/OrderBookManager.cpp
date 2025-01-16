// the order book manager keeps a map of orderbooks
// orderbook map keyed on symbol, value as orderbook
// returns pointer to correct orderbook for server to use

#include "common_includes.h"
#include "OrderBook.h"

using namespace std;
using Symbol = string;

class OrderBookManager {
private:
	unordered_map<Symbol, OrderBook> orderBookMap;
	unordered_map<Symbol, size_t>    orderBookDepth;   // how many levels on bids/asks to deseminate to client
public:
	OrderBookManager() {}
	bool AddSymbol(Symbol, size_t) {
		OrderBook 
	}
};

