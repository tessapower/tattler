#include "stdafx.h"

#include "common/capture_serialization.h"
#include "common/capture_types.h"
#include "common/pipe_protocol.h"
#include "hook/capture_controller.h"

#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <utility>
#include <vector>

namespace Tattler
{
CaptureController::~CaptureController()
{
    if (!m_tempDirectory.empty())
        std::filesystem::remove_all(m_tempDirectory);
}

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
            m_textureMemoryBytes = 0;
            m_eventMemoryBytes = 0;
            m_tempDirectory.clear();
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

    // Check memory limits before processing
    if (!CheckMemoryLimits(events.size()))
    {
        // Limits exceeded, capture has been auto-stopped
        return;
    }

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

        // Track memory usage for events
        m_eventMemoryBytes += EstimateEventSize(event);
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

auto CaptureController::AddTexture(StagedTexture tex) -> void
{
    tex.frameIndex = m_frameIndex;

    const size_t texSizeBytes = tex.pixels.size();
    const size_t currentMemoryMB = m_textureMemoryBytes / (1024 * 1024);

    // First 100 frames (or until we hit 512MB): keep in memory
    if (m_snapshot.renderTargetSnapshots.size() < MAX_IN_MEMORY_FRAMES &&
        currentMemoryMB < MAX_TEXTURE_MEMORY_MB)
    {
        m_textureMemoryBytes += texSizeBytes;
        m_snapshot.renderTargetSnapshots.push_back(std::move(tex));
        return;
    }

    // Subsequent frames: write to disk to avoid OOM
    if (m_tempDirectory.empty())
    {
        wchar_t tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);
        m_tempDirectory = std::wstring(tempPath) + L"tattler_capture\\";

        try
        {
            std::filesystem::create_directories(m_tempDirectory);
        }
        catch (const std::filesystem::filesystem_error&)
        {
            OutputDebugStringW(
                L"[Tattler] Failed to create temp directory for textures\n");
            // Continue capture but skip this texture
            return;
        }
    }

    std::wstring filename =
        std::format(L"{}frame_{:06}.bin", m_tempDirectory, m_frameIndex);

    std::ofstream file(filename, std::ios::binary);
    if (file)
    {
        try
        {
            file.write(reinterpret_cast<const char*>(tex.pixels.data()),
                       texSizeBytes);
            file.close();

            if (file.good())
            {
                tex.diskPath = filename;
                tex.pixels.clear();
                tex.isOnDisk = true;
                m_snapshot.renderTargetSnapshots.push_back(std::move(tex));
            }
            else
            {
                OutputDebugStringW(
                    L"[Tattler] Failed to write texture to disk\n");
            }
        }
        catch (const std::exception&)
        {
            OutputDebugStringW(
                L"[Tattler] Exception writing texture to disk\n");
        }
    }
    else
    {
        OutputDebugStringW(
            L"[Tattler] Failed to open texture file for writing\n");
    }
}

auto CaptureController::EstimateEventSize(const CapturedEvent& event) const
    -> size_t
{
    // Base size of CapturedEvent structure
    size_t size = sizeof(CapturedEvent);

    // Add estimate for variant params based on type
    switch (event.type)
    {
    case EventType::Draw:
    {
        size += sizeof(DrawParams);
        break;
    }
    case EventType::DrawIndexed:
    {
        size += sizeof(DrawIndexedParams);
        break;
    }
    case EventType::Dispatch:
    {
        size += sizeof(DispatchParams);
        break;
    }
    case EventType::ResourceBarrier:
    {
        size += sizeof(BarrierParams);
        break;
    }
    case EventType::ClearRTV:
    {
        size += sizeof(ClearRtvParams);
        break;
    }
    case EventType::ClearDSV:
    {
        size += sizeof(ClearDsvParams);
        break;
    }
    case EventType::CopyResource:
    {
        size += sizeof(CopyParams);
        break;
    }
    case EventType::Present:
    {
        size += sizeof(PresentParams);
        break;
    }
    default:
    {
        break;
    }
    }

    return size;
}

auto CaptureController::CheckMemoryLimits(size_t frameEventCount) -> bool
{
    // Check frame count limit
    if (m_frameIndex >= MAX_CAPTURE_FRAMES)
    {
        OutputDebugStringW(
            L"[Tattler] Capture auto-stopped: reached maximum frame count\n");
        m_isCapturing = false;
        FlushFrame();

        return false;
    }

    // Check events per frame limit
    if (frameEventCount > MAX_EVENTS_PER_FRAME)
    {
        OutputDebugStringW(L"[Tattler] Capture auto-stopped: frame exceeded "
                           L"maximum event count\n");
        m_isCapturing = false;
        FlushFrame();

        return false;
    }

    // Check total event memory limit
    const size_t currentMemoryMB = m_eventMemoryBytes / (1024 * 1024);
    if (currentMemoryMB >= MAX_TOTAL_EVENT_MEMORY_MB)
    {
        OutputDebugStringW(
            L"[Tattler] Capture auto-stopped: exceeded maximum event memory\n");
        m_isCapturing = false;
        FlushFrame();

        return false;
    }

    return true;
}

} // namespace Tattler
