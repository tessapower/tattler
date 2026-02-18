#pragma once

#include "common/pipe_protocol.h"

namespace tattler
{

class PipeServer : public PipeProtocol::Pipe
{
  public:
    PipeServer() = default;
    ~PipeServer()
    {
        Destroy();
    }

    auto Create() -> bool
    {
        m_handle =
            CreateNamedPipeW(PipeProtocol::PIPE_NAME, // Pipe name
                             PIPE_ACCESS_DUPLEX,      // Read and write access
                             PIPE_TYPE_BYTE, // Send/receive data as bytes
                             1, // Only allow 1 instance of this pipe
                             PipeProtocol::BUFFER_SIZE, // Outbound buf.
                             PipeProtocol::BUFFER_SIZE, // Inbound buf.
                             0,      // Use default wait time,
                             nullptr // Use default security

            );

        if (m_handle == INVALID_HANDLE_VALUE || m_handle == nullptr)
        {
            return false;
        }

        return true;
    }

    auto Connect() -> bool override
    {
        bool res = ConnectNamedPipe(m_handle, nullptr)
                       ? true
                       : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (!res)
        {
            Disconnect();
        }

        return res;
    }

    auto Disconnect() -> void override {
        if (m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr)
        {
            FlushFileBuffers(m_handle);
            DisconnectNamedPipe(m_handle);
        }
    }

    auto Destroy() -> void
    {
        if (m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr)
        {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }
};
} // namespace tattler
