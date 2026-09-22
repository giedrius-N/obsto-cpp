#pragma once

#include "IWriteSession.h"
#include <functional>
#include <memory>

class BoundedWriteSession : public IWriteSession
{
public:
    BoundedWriteSession(std::unique_ptr<IWriteSession> inner,
                         std::function<void(size_t)> onCommitted,
                         std::function<void(size_t)> onAborted);

    bool writeChunk(std::string_view chunk) override;
    PutResult finish() override;
    void abort() override;

private:
    std::unique_ptr<IWriteSession> m_inner;
    std::function<void(size_t)> m_onCommitted;
    std::function<void(size_t)> m_onAborted;
    bool m_finalized = false;
    size_t m_bytesWritten = 0;
};
