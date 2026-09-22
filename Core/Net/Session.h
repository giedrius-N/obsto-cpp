#pragma once

#include "IProtocolHandler.h"
#include <asio.hpp>
#include <memory>
#include <array>
#include <chrono>

class Session : public std::enable_shared_from_this<Session>
{
public:
    explicit Session(asio::ip::tcp::socket socket,
                     std::unique_ptr<IProtocolHandler> handler
    );

    void start();

private:
    void doRead();
    void doWrite(bool keepGoingAfterWrite);
    void drainAndClose();
    void armIdleTimer();
    void closeSocket();

    asio::ip::tcp::socket m_socket;
    std::array<char, 4096> m_buffer;
    std::unique_ptr<IProtocolHandler> m_handler;
    asio::steady_timer m_idleTimer;

    static constexpr std::chrono::seconds kIdleTimeout{300}; // 5 minutes
};