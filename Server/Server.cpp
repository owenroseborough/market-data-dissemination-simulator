//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: WebSocket server, asynchronous
//
//------------------------------------------------------------------------------

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <cstdlib>
#include <functional>
#include <thread>
#include "common_includes.h"
#include "OrderBookManager.h"

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

using namespace std;

shared_ptr<OrderBookManager> orderBookManager;
unique_ptr<vector<string>> clientSubList;

//------------------------------------------------------------------------------

// Report a failure
void
fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Echoes back all received WebSocket messages
class session : public std::enable_shared_from_this<session>
{
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;

public:
    // Take ownership of the socket
    explicit
        session(tcp::socket&& socket)
        : ws_(std::move(socket))
    {
    }

    // Get on the correct executor
    void
        run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        net::dispatch(ws_.get_executor(),
            beast::bind_front_handler(
                &session::on_run,
                shared_from_this()));
    }

    // Start the asynchronous operation
    void
        on_run()
    {
        // Set suggested timeout settings for the websocket
        ws_.set_option(
            websocket::stream_base::timeout::suggested(
                beast::role_type::server));

        // Set a decorator to change the Server of the handshake
        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::response_type& res)
            {
                res.set(http::field::server,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                    " websocket-server-async");
            }));
        // Accept the websocket handshake
        ws_.async_accept(
            beast::bind_front_handler(
                &session::on_accept,
                shared_from_this()));
    }

    void
        on_accept(beast::error_code ec)
    {
        if (ec)
            return fail(ec, "accept");

        // Read a message
        do_read();
    }

    void
        do_read()
    {
        // Read a message into our buffer
        ws_.async_read(
            buffer_,
            beast::bind_front_handler(
                &session::on_read,
                shared_from_this()));
    }

    void
        on_read(
            beast::error_code ec,
            std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This indicates that the session was closed
        if (ec == websocket::error::closed)
            return;

        if (ec)
            return fail(ec, "read");

        string bufferAsString = buffers_to_string(buffer_);

        // "subscribe:SYMBOL"
        if (bufferAsString.starts_with("subscribe")) {
            string symbol = bufferAsString.substr(10);
            if (orderBookManager->GetOrderBook(symbol)) {
                clientSubList->push_back(symbol);
                write_snapshot(symbol);
            }
        }
        // "unsubscribe:SYMBOL"
        else if (bufferAsString.starts_with("unsubscribe")) {
            string symbol = bufferAsString.substr(12);
            auto it = find(clientSubList->begin(), clientSubList->end(), symbol);
            if (it != clientSubList->end()) {
                clientSubList->erase(it);
            }
        }
    }

    void
        do_write()
    {
        // Echo the message
        ws_.text(ws_.got_text());
        ws_.async_write(
            buffer_.data(),
            beast::bind_front_handler(
                &session::on_write,
                shared_from_this()));
    }
    void
        on_write(
            beast::error_code ec,
            std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return fail(ec, "write");
    }
    void
        write_trades(vector<Trade> trades)
    {
        // Clear the buffer
        buffer_.consume(buffer_.size());

        // write trades to buffer
        string tradesString{""};

        for (auto trade : trades) {
            tradesString += "Bid: ";
            tradesString += to_string(trade.GetBidTrade().orderId_);
            tradesString += " Price: ";
            tradesString += to_string(trade.GetBidTrade().price_);
            tradesString += " Quantity: ";
            tradesString += to_string(trade.GetBidTrade().quantity_);
            tradesString += " | ";
            tradesString += "Ask: ";
            tradesString += to_string(trade.GetAskTrade().orderId_);
            tradesString += " Price: ";
            tradesString += to_string(trade.GetAskTrade().price_);
            tradesString += " Quantity: ";
            tradesString += to_string(trade.GetAskTrade().quantity_);
            tradesString += ",";
        }

        // Allocate space in the buffer
        auto size = tradesString.size();
        auto buffer_space = buffer_.prepare(size);
        // Copy the data into the buffer
        memcpy(buffer_space.data(), tradesString.data(), size);
        // Commit the data to the buffer
        buffer_.commit(size);

        ws_.async_write(
            buffer_.data(),
            beast::bind_front_handler(
                &session::on_write,
                shared_from_this()));
    }
    void
        write_snapshot(Symbol symbol)
    {
        shared_ptr<OrderBook> orderBook = orderBookManager->GetOrderBook(symbol);
        size_t orderBookDepth = orderBookManager->GetOrderBookDepth(symbol);

        // we have our orderbook for a certain symbol, we now need to make a snapshot to send to client
        // orderBookDepth has the number of levels on the bids and asks that we will desseminate to client
        OrderBookLevelInfos levelInfos = orderBook->GetOrderInfos();

        const LevelInfos& BidLevelInfos = levelInfos.GetBids();
        const LevelInfos& AskLevelInfos = levelInfos.GetAsks();

        orderBookDepth = min(orderBookDepth, BidLevelInfos.size());
        orderBookDepth = min(orderBookDepth, AskLevelInfos.size());

        string snapshot = format(" {}    \t\t  {}   \n", "Bids", "Asks");
        
        size_t counter = 0;
        while (counter < orderBookDepth) {
            BidLevelInfos[counter].price_;
            BidLevelInfos[counter].quantity_;

            AskLevelInfos[counter].price_;
            AskLevelInfos[counter].quantity_;

            snapshot += format("${}:{} \t\t ${}:{}\n", to_string(BidLevelInfos[counter].price_), to_string(BidLevelInfos[counter].quantity_), to_string(AskLevelInfos[counter].price_), to_string(AskLevelInfos[counter].quantity_));
            counter++;
        }

        // now desseminate snapshot
        // Allocate space in the buffer
        auto size = snapshot.size();
        auto buffer_space = buffer_.prepare(size);
        // Copy the data into the buffer
        memcpy(buffer_space.data(), snapshot.data(), size);
        // Commit the data to the buffer
        buffer_.commit(size);

        ws_.async_write(
            buffer_.data(),
            beast::bind_front_handler(
                &session::on_write,
                shared_from_this()));
    }
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
    net::io_context& ioc_;
    tcp::acceptor acceptor_;

public:
    listener(
        net::io_context& ioc,
        tcp::endpoint endpoint)
        : ioc_(ioc)
        , acceptor_(ioc)
    {
        beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if (ec)
        {
            fail(ec, "open");
            return;
        }

        // Allow address reuse
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec)
        {
            fail(ec, "set_option");
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if (ec)
        {
            fail(ec, "bind");
            return;
        }

        // Start listening for connections
        acceptor_.listen(
            net::socket_base::max_listen_connections, ec);
        if (ec)
        {
            fail(ec, "listen");
            return;
        }
    }

    // Start accepting incoming connections
    void
        run()
    {
        do_accept();
    }
    shared_ptr<session> getSession() {
        return session_;
    }

private:
    shared_ptr<session> session_;
    void
        do_accept()
    {
        // The new connection gets its own strand
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(
                &listener::on_accept,
                shared_from_this()));
    }

    void
        on_accept(beast::error_code ec, tcp::socket socket)
    {
        if (ec)
        {
            fail(ec, "accept");
        }
        else
        {
            // Create the session and run it
            session_ = std::make_shared<session>(std::move(socket));
            session_->run();
        }

        // Accept another connection
        do_accept();
    }
};

//------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    // Check command line arguments.
    if (argc != 4)
    {
        std::cerr <<
            "Usage: websocket-server-async <address> <port> <threads>\n" <<
            "Example:\n" <<
            "    websocket-server-async 0.0.0.0 8080 1\n";
        return EXIT_FAILURE;
    }
    auto const address = net::ip::make_address(argv[1]);
    auto const port = static_cast<unsigned short>(std::atoi(argv[2]));
    auto const threads = std::max<int>(1, std::atoi(argv[3]));

    // The io_context is required for all I/O
    net::io_context ioc{ threads };

    // Create and launch a listening port
    shared_ptr<listener> listener_ = std::make_shared<listener>(ioc, tcp::endpoint{ address, port });
    listener_->run();

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for (auto i = threads - 1; i > 0; --i)
        v.emplace_back(
            [&ioc]
            {
                ioc.run();
            });
    ioc.run();

    orderBookManager = make_shared<OrderBookManager>();
    clientSubList = make_unique<vector<string>>();

    
    orderBookManager->AddSymbol("META", 5);

    shared_ptr<OrderBook> orderBook = orderBookManager->GetOrderBook("META");

    while (1) {
        vector<Trade> trades = orderBook->GenerateRandomOrder();
        if (!trades.empty()) {
            auto session = listener_->getSession();
            session->write_trades(trades);
        }
        this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return EXIT_SUCCESS;
}