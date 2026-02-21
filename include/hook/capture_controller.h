#pragma once

#include "common/capture_types.h"
#include "hook/pipe_client.h"

#include <atomic>

namespace Tattler
{
/// <summary>
/// Manages the capture process
/// </summary>
class CaptureController
{
  public:
    CaptureController() = default;
    ~CaptureController() = default;

    /// <summary>
    /// Starts the capture process.
    /// </summary>
    auto Run() -> void;

    auto IsConnected() const -> bool
    {
        return m_pipeConnected;
    }

    auto IsCapturing() const -> bool
    {
        return m_isCapturing;
    }

    auto SubmitEvent(CapturedEvent const& event) -> void;

    auto FlushFrame() -> void;

    /// <summary>
    /// Flushes the event buffer, patches each event's timestampBegin/End
    /// from slot indices to real GPU ticks, then ships the frame to the viewer.
    /// Call after the GPU fence wait at the end of each captured Present.
    /// </summary>
    auto EndFrame(const std::vector<uint64_t>& timestampResults,
                  uint64_t frequency) -> void;

  private:
    PipeClient m_pipeClient;
    std::atomic<bool> m_pipeConnected = false;
    std::atomic<bool> m_isCapturing = false;
    // CaptureBuffer m_buffer;
};

} // namespace Tattler
