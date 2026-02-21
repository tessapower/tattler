#include "stdafx.h"

#include "hook/capture_controller.h"

#include <common/capture_types.h>

namespace Tattler
{
auto CaptureController::Run() -> void
{
    if (!m_pipeClient.Connect())
        return;

    m_pipeConnected = true;

    while (true)
    {
        PipeProtocol::MessageType type;
        if (!m_pipeClient.Receive(type, nullptr))
            break;

        switch (type)
        {
        case PipeProtocol::MessageType::StartCapture:
        {
            m_isCapturing = true;
            break;
        }

        case PipeProtocol::MessageType::StopCapture:
        {
            m_isCapturing = false;
            // Serialize events and send capture data
            FlushFrame();
            break;
        }

        default:
            break;
        }
    }

    m_pipeConnected = false;
}

auto CaptureController::SubmitEvent(CapturedEvent const& event) -> void
{
    // TODO: Submit to buffer, need to implement capture buffer
}

auto CaptureController::FlushFrame() -> void
{
    // if (m_pipeConnected)
    //  m_pipeClient.Send(PipeProtocol::MessageType::CaptureData, m_buffer,
    //  sizeof(m_buffer));
}

auto CaptureController::EndFrame(const std::vector<uint64_t>& timestampResults,
                                 uint64_t frequency) -> void
{
    auto events = m_buffer.Flush();

    for (auto& event : events)
    {
        if (event.timestampBegin < timestampResults.size())
            event.timestampBegin = timestampResults[event.timestampBegin];
        if (event.timestampEnd < timestampResults.size())
            event.timestampEnd = timestampResults[event.timestampEnd];
    }

    // TODO: build CapturedFrame with frequency + events, serialize, send via
    // pipe
}

} // namespace Tattler
