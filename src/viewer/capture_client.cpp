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

namespace tattler
{
CaptureClient::CaptureClient()
{
    if (!m_pipeServer.Create())
        throw std::runtime_error("Failed to create pipe server");
}

CaptureClient::~CaptureClient()
{
    Stop();
}

auto CaptureClient::Start() -> void
{
    // Connect to the pipe immediately, we do this on a separate thread so we
    // don't block the main thread if the hook isn't injected yet and/or isn't
    // connecting to the pipe.
    m_pipeThread = std::thread(
        [this]()
        {
            if (!m_pipeServer.Connect())
                return;

            m_pipeConnected = true;

            while (true)
            {
                PipeProtocol::MessageType type;
                std::vector<uint8_t> payload;

                if (!m_pipeServer.Receive(type, &payload))
                {
                    m_pipeConnected = false;
                    break; // pipe closed or error
                }

                switch (type)
                {
                case PipeProtocol::MessageType::CaptureData:
                {
                    // payload already contains the full serialized snapshot
                    CaptureSnapshot snapshot;
                    if (Deserialize(payload, &snapshot))
                    {
                        std::lock_guard lock(m_snapshotMutex);
                        m_snapshot = std::move(snapshot);
                    }
                    break;
                }
                default:
                    // Unexpected message type, ignore
                    break;
                }
            }
        });
}

auto CaptureClient::Stop() -> void
{
    m_pipeServer.Disconnect();
    m_pipeServer.Destroy();

    if (m_pipeThread.joinable())
        m_pipeThread.join();
}

auto CaptureClient::SendStartCapture() -> void
{
    if (m_pipeConnected)
        m_pipeServer.Send(PipeProtocol::MessageType::StartCapture,
                                 nullptr, 0);
}

auto CaptureClient::SendStopCapture() -> void
{
    if (m_pipeConnected)
        m_pipeServer.Send(PipeProtocol::MessageType::StopCapture,
                                 nullptr, 0);
}

} // namespace tattler
