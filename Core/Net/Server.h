#pragma once

#include "IProtocolHandler.h"
#include <asio.hpp>
#include <functional>

using HandlerFactory = std::function<std::unique_ptr<IProtocolHandler>()>;

class Server
{
public:
    Server(asio::io_context& ioContext, unsigned short port, HandlerFactory handlerFactory);

private:
    void doAccept();

    asio::ip::tcp::acceptor m_acceptor;
    HandlerFactory m_factory;
};