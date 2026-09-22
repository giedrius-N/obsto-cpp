#include "Session.h"
#include <iostream>

using asio::ip::tcp;

Session::Session(tcp::socket socket,
                 std::unique_ptr<IProtocolHandler> handler)
    : m_socket(std::move(socket))
    , m_handler(std::move(handler))
    , m_idleTimer(m_socket.get_executor())
{
}

void Session::start()
{
    armIdleTimer();
    doRead();
}

void Session::armIdleTimer()
{
    auto self = shared_from_this();
    m_idleTimer.expires_after(kIdleTimeout);
    m_idleTimer.async_wait([this, self](asio::error_code ec)
    {
        if (ec)
        {
            return;
        }
        closeSocket();
    });
}

void Session::closeSocket()
{
    asio::error_code ec;
    m_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    m_socket.close(ec);
}

void Session::doRead()
{
    auto self = shared_from_this();
    m_socket.async_read_some(
        asio::buffer(m_buffer),
        [this, self](asio::error_code ec, std::size_t bytesRead)
        {
            if (ec)
            {
                m_idleTimer.cancel();
                return;
            }

            m_idleTimer.expires_after(kIdleTimeout);

            bool keepGoing = false;
            try
            {
                keepGoing = m_handler->onBytesReceived(
                    std::string_view(m_buffer.data(), bytesRead));
            }
            catch (const std::exception& e)
            {
                std::cerr << "[Session] Exception in onBytesReceived: " << e.what() << "\n";
                m_idleTimer.cancel();
                closeSocket();
                return;
            }
            catch (...)
            {
                std::cerr << "[Session] Unknown exception in onBytesReceived\n";
                m_idleTimer.cancel();
                closeSocket();
                return;
            }

            doWrite(keepGoing);
        });
}

void Session::doWrite(bool keepGoingAfterWrite)
{
    auto self = shared_from_this();

    std::vector<std::byte> outgoingData;
    try
    {
        outgoingData = m_handler->takeOutgoingBytes();
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Session] Exception in takeOutgoingBytes: " << e.what() << "\n";
        m_idleTimer.cancel();
        closeSocket();
        return;
    }
    catch (...)
    {
        std::cerr << "[Session] Unknown exception in takeOutgoingBytes\n";
        m_idleTimer.cancel();
        closeSocket();
        return;
    }

    auto outgoing = std::make_shared<std::vector<std::byte>>(std::move(outgoingData));

    if (outgoing->empty())
    {
        if (m_handler->hasMoreToSend())
        {
            asio::post(m_socket.get_executor(), [this, self, keepGoingAfterWrite]
            {
                doWrite(keepGoingAfterWrite);
            });
            return;
        }

        if (keepGoingAfterWrite)
        {
            doRead();
        }
        else
        {
            m_idleTimer.cancel();
            drainAndClose();
        }
        return;
    }

    asio::async_write(
        m_socket, asio::buffer(*outgoing),
        [this, self, outgoing, keepGoingAfterWrite](asio::error_code ec, std::size_t)
        {
            if (ec)
            {
                m_idleTimer.cancel();
                return;
            }

            bool hasMore = false;
            try
            {
                hasMore = m_handler->hasMoreToSend();
            }
            catch (...)
            {
                m_idleTimer.cancel();
                closeSocket();
                return;
            }

            if (hasMore)
            {
                doWrite(keepGoingAfterWrite);
                return;
            }

            if (keepGoingAfterWrite)
            {
                doRead();
            }
            else
            {
                m_idleTimer.cancel();
                drainAndClose();
            }
        });
}

void Session::drainAndClose()
{
    auto self = shared_from_this();

    asio::error_code shutdownEc;
    static_cast<void>(m_socket.shutdown(asio::ip::tcp::socket::shutdown_send, shutdownEc));

    m_socket.async_read_some(
        asio::buffer(m_buffer),
        [this, self](asio::error_code ec, std::size_t /*bytesRead*/)
        {
            if (ec)
            {
                return;
            }
            drainAndClose();
        });
}
