#include "Server.h"
#include "Session.h"
#include <memory>

using asio::ip::tcp;

Server::Server(asio::io_context& ioContext, unsigned short port, HandlerFactory handlerFactory)
    : m_acceptor(ioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
    , m_factory(std::move(handlerFactory))
{
    doAccept();
}

void Server::doAccept()
{
    m_acceptor.async_accept(
        [this](asio::error_code ec, tcp::socket socket)
        {
            if (!ec)
            {
                std::make_shared<Session>(std::move(socket), m_factory())->start();
            }
            doAccept();
        });
}