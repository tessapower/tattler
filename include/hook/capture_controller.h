#pragma once

#include "common/capture_types.h"
#include "hook/capture_buffer.h"
#include "hook/pipe_client.h"

#include <atomic>
#include <string>

namespace Tattler
{
/// <summary>
/// Manages the capture process.
/// </summary>
class CaptureController
{
  public:
    CaptureController() = default;
    ~CaptureController();

    /// <summary>
    /// Starts the capture process.
    /// </summary>
    auto Run() -> void;

    /// <summary>
    /// Returns true if the pipe connection to the viewer is active.
    /// </summary>
    auto IsConnected() const -> bool
    {
        return m_pipeConnected;
    }

    /// <summary>
    /// Returns true if a capture is currently in progress.
    /// Hook wrappers should check this before calling SubmitEvent.
    /// </summary>
    auto IsCapturing() const -> bool
    {
        return m_isCapturing;
    }

    /// <summary>
    /// Submits a captured event to the event buffer. Does NOT check if we're
    /// currently capturing, that should be done first with IsCapturing() to
    /// avoid corrupting capture data with pre-capture events.
    /// </summary>
    auto SubmitEvent(CapturedEvent const& event) -> void;

    /// <summary>
    /// Serializes the accumulated CaptureSnapshot and sends it to the viewer
    /// over the pipe. Called when the viewer sends a StopCapture message.
    /// Clears the snapshot after sending.
    /// </summary>
    auto FlushFrame() -> void;

    /// <summary>
    /// Flushes the event buffer, patches each event's timestampBegin/End
    /// from slot indices to real GPU ticks, then ships the frame to the viewer.
    /// Call after the GPU fence wait at the end of each captured Present.
    /// </summary>
    auto EndFrame(const std::vector<uint64_t>& timestampResults,
                  uint64_t frequency) -> void;

    /// <summary>
    /// Stores a readback texture. First 100 frames (up to 512MB) are kept in
    /// memory. Subsequent frames are written to disk to avoid OOM.
    /// </summary>
    auto AddTexture(StagedTexture tex) -> void;

  private:
    static constexpr size_t MAX_IN_MEMORY_FRAMES = 100;
    static constexpr size_t MAX_TEXTURE_MEMORY_MB = 512;
    // Stop after 10k frames
    static constexpr size_t MAX_CAPTURE_FRAMES = 10000;
    // Stop if single frame exceeds 100k events
    static constexpr size_t MAX_EVENTS_PER_FRAME = 100000;
    // Stop if event data exceeds 1GB
    static constexpr size_t MAX_TOTAL_EVENT_MEMORY_MB = 1024;

    PipeClient m_pipeClient;
    std::atomic<bool> m_pipeConnected = false;
    std::atomic<bool> m_isCapturing = false;
    CaptureBuffer m_buffer;
    CaptureSnapshot m_snapshot;
    uint32_t m_frameIndex = 0;
    uint64_t m_captureStartTimeUs = 0;
    uint64_t m_lastFrameTimeUs = 0;
    size_t m_textureMemoryBytes = 0;
    size_t m_eventMemoryBytes = 0;
    std::wstring m_tempDirectory;

    /// <summary>
    /// Estimates the memory footprint of a captured event, including its
    /// type-specific parameters. Used for tracking total event memory usage
    /// against the MAX_TOTAL_EVENT_MEMORY_MB limit.
    /// </summary>
    /// <param name="event">The event to measure.</param>
    /// <returns>The estimated size in bytes.</returns>
    auto EstimateEventSize(const CapturedEvent& event) const -> size_t;

    /// <summary>
    /// Checks whether the capture has exceeded any of its memory or frame
    /// limits. If a limit is breached, automatically stops the capture and
    /// flushes remaining data to the viewer.
    /// </summary>
    /// <param name="frameEventCount">Number of events in the current
    /// frame.</param>
    /// <returns>True if capture should continue, false if it was
    /// auto-stopped.</returns>
    auto CheckMemoryLimits(size_t frameEventCount) -> bool;
};

} // namespace Tattler
