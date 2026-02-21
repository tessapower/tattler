#include "stdafx.h"

#include "common/capture_serialization.h"
#include "common/capture_types.h"
#include "common/pipe_protocol.h"
#include "hook/capture_controller.h"

#include <cstdint>
#include <utility>
#include <vector>

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
            m_snapshot = {};
            m_frameIndex = 0;
            LARGE_INTEGER qpc, freq;
            QueryPerformanceCounter(&qpc);
            QueryPerformanceFrequency(&freq);
            m_captureStartTimeUs =
                (qpc.QuadPart * 1'000'000ULL) / freq.QuadPart;
            m_lastFrameTimeUs = m_captureStartTimeUs;
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
    m_buffer.AddEvent(event);
}

auto CaptureController::FlushFrame() -> void
{
    if (!m_pipeConnected)
        return;

    // Get the length of the snapshot in microsecs
    LARGE_INTEGER qpc, freq;
    QueryPerformanceCounter(&qpc);
    QueryPerformanceFrequency(&freq);
    uint64_t nowUs = (qpc.QuadPart * 1'000'000ULL) / freq.QuadPart;
    // Calculate duration and convert to seconds
    m_snapshot.captureDurationSec =
        (nowUs - m_captureStartTimeUs) / 1'000'000.0;

    std::vector<uint8_t> buffer;
    // Serialize the snapshot to send to the viewer
    if (Serialize(m_snapshot, &buffer))
        m_pipeClient.Send(PipeProtocol::MessageType::CaptureData, buffer.data(),
                          static_cast<uint32_t>(buffer.size()));

    // Clear the snapshot
    m_snapshot = {};
}

auto CaptureController::EndFrame(const std::vector<uint64_t>& timestampResults,
                                 uint64_t frequency) -> void
{
    // Get all the events
    auto events = m_buffer.Flush();

    // Figure out current timestamp
    LARGE_INTEGER qpc, freq;
    QueryPerformanceCounter(&qpc);
    QueryPerformanceFrequency(&freq);
    // QuadPart = QWORD i.e. the full 64-bit value
    uint64_t nowUs = (qpc.QuadPart * 1'000'000ULL) / freq.QuadPart;

    // Match up gpu-side timestamps with events
    for (uint32_t i = 0; i < static_cast<uint32_t>(events.size()); ++i)
    {
        auto& event = events[i];
        event.frameIndex = m_frameIndex;
        event.eventIndex = i;
        if (event.timestampBegin < timestampResults.size())
            event.timestampBegin = timestampResults[event.timestampBegin];
        if (event.timestampEnd < timestampResults.size())
            event.timestampEnd = timestampResults[event.timestampEnd];
    }

    // Put together ther frame and add it to the snapshot's list of frames
    CapturedFrame frame{};
    frame.frameNumber = m_frameIndex;
    frame.cpuFrameStartUs = m_lastFrameTimeUs;
    frame.cpuFrameEndUs = nowUs;
    frame.gpuFrequency = frequency;
    frame.events = std::move(events);
    m_snapshot.frames.push_back(std::move(frame));

    m_lastFrameTimeUs = nowUs;
    ++m_frameIndex;
}

} // namespace Tattler
