#include "stdafx.h"

#include "common/capture_serialization.h"
#include "common/capture_types.h"
#include "common/pipe_protocol.h"
#include "viewer/capture_client.h"
#include "viewer/pipe_server.h"

#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace Tattler
{
CaptureClient::CaptureClient()
{
    if (!m_pipeServer.Create())
    {
        throw std::runtime_error("Failed to create pipe server");
    }
}

CaptureClient::~CaptureClient()
{
    Stop();
}

auto CaptureClient::Start() -> void
{
    m_stopping = false;

    // Connect to the pipe on a background thread so we don't block rendering.
    // After a client disconnects the loop resets the pipe and waits for the
    // next one, so re-launching the target app always finds a listener.
    m_pipeThread = std::thread(
        [this]()
        {
            while (!m_stopping)
            {
                {
                    std::lock_guard lock(m_pipeMutex);
                    if (!m_pipeServer.Connect())
                        break;
                }

                m_pipeConnected = true;

                while (!m_stopping)
                {
                    PipeProtocol::MessageType type;
                    std::vector<uint8_t> payload;

                    bool received;
                    {
                        std::lock_guard lock(m_pipeMutex);

                        // Use PeekNamedPipe to avoid blocking indefinitely
                        DWORD bytesAvail = 0;
                        if (!PeekNamedPipe(m_pipeServer.GetHandle(), nullptr, 0,
                                           nullptr, &bytesAvail, nullptr))
                            break; // pipe error

                        if (bytesAvail == 0)
                        {
                            // No data available, sleep briefly and continue
                            // This allows Send() calls from other threads to
                            // proceed
                            std::this_thread::sleep_for(
                                std::chrono::milliseconds(1));
                            continue;
                        }

                        received = m_pipeServer.Receive(type, &payload);
                    }

                    if (!received)
                    {
                        break; // pipe closed or error
                    }

                    if (type == PipeProtocol::MessageType::CaptureData)
                    {
                        CaptureSnapshot snapshot;
                        if (Deserialize(payload, &snapshot))
                        {
                            std::lock_guard lock(m_snapshotMutex);
                            m_snapshot = std::move(snapshot);
                        }
                    }
                }

                m_pipeConnected = false;
                {
                    std::lock_guard lock(m_pipeMutex);
                    // reset pipe handle for next client
                    m_pipeServer.Disconnect();
                }
            }
        });
}

auto CaptureClient::Stop() -> void
{
    m_stopping = true;

    if (m_pipeThread.joinable())
    {
        // If the thread is blocked in ConnectNamedPipe waiting for a client,
        // open a temporary connection to unblock it. If a real client is
        // already connected this will fail harmlessly — Disconnect/Destroy
        // below will then unblock the ReadFile in the Receive loop instead.
        HANDLE dummy =
            CreateFileW(PipeProtocol::PIPE_NAME, GENERIC_READ | GENERIC_WRITE,
                        0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (dummy != INVALID_HANDLE_VALUE)
            CloseHandle(dummy);
    }

    {
        std::lock_guard lock(m_pipeMutex);
        m_pipeServer.Disconnect();
        m_pipeServer.Destroy();
    }

    if (m_pipeThread.joinable())
    {
        m_pipeThread.join();
    }
}

auto CaptureClient::SendStartCapture() -> void
{
    if (m_pipeConnected)
    {
        m_pipeServer.Send(PipeProtocol::MessageType::StartCapture, nullptr, 0);
    }
}

auto CaptureClient::SendStopCapture() -> void
{
    if (m_pipeConnected)
    {
        m_pipeServer.Send(PipeProtocol::MessageType::StopCapture, nullptr, 0);
    }
}

} // namespace Tattler
