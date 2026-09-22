#include "BoundedWriteSession.h"

BoundedWriteSession::BoundedWriteSession(
    std::unique_ptr<IWriteSession> inner,
    std::function<void(size_t)> onCommitted,
    std::function<void(size_t)> onAborted)
    : m_inner(std::move(inner))
    , m_onCommitted(std::move(onCommitted))
    , m_onAborted(std::move(onAborted))
{
}

bool BoundedWriteSession::writeChunk(std::string_view chunk)
{
    m_bytesWritten += chunk.size();
    return m_inner->writeChunk(chunk);
}

PutResult BoundedWriteSession::finish()
{
    if (m_finalized)
    {
        return PutResult::FailedToWrite;
    }

    PutResult result = m_inner->finish();
    m_finalized = true;
    if (result == PutResult::Ok)
    {
        m_onCommitted(m_bytesWritten);
    }
    else
    {
        m_onAborted(m_bytesWritten);
    }
    return result;
}

void BoundedWriteSession::abort()
{
    if (m_finalized)
    {
        return;
    }

    m_finalized = true;
    m_inner->abort();
    m_onAborted(m_bytesWritten);
}
