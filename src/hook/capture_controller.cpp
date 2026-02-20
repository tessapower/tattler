#include "stdafx.h"

#include <common/capture_types.h>
#include "hook/capture_controller.h"

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

auto CaptureController::SubmitEvent(CapturedEvent const& event) -> void {
    // TODO: Submit to buffer, need to implement capture buffer
}

auto CaptureController::FlushFrame() -> void {
    //if (m_pipeConnected)
        // m_pipeClient.Send(PipeProtocol::MessageType::CaptureData, m_buffer, sizeof(m_buffer));
}

} // namespace Tattler
