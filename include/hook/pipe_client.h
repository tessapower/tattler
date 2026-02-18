#pragma once

#include "common/pipe_protocol.h"

namespace tattler
{

class PipeClient : public PipeProtocol::Pipe
{
  public:

    PipeClient() = default;
    ~PipeClient() {
        Disconnect();
    }

    auto Connect() -> bool override {
        while (true)
        {
            m_handle =
                CreateFileW(PipeProtocol::PIPE_NAME,
                           GENERIC_READ | GENERIC_WRITE, // Read-write access
                           0,                            // No sharing
                           nullptr,       // Default security attributes
                           OPEN_EXISTING, // Opens existing pipe
                           0,             // Default attributes
                           nullptr        // No template file
                );

              if (m_handle != INVALID_HANDLE_VALUE)
                return true;

            // Only reachable if CreateFile failed
            if (GetLastError() != ERROR_PIPE_BUSY)
                return false;

            if (!WaitNamedPipeW(PipeProtocol::PIPE_NAME, 5000))
                return false;
            // loop back and retry CreateFile
        }
    }

    auto Disconnect() -> void override {
        if (m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr)
        {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }
};
} // namespace tattler
