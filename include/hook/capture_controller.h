#pragma once

#include "common/capture_types.h"
#include "hook/pipe_client.h"

#include <atomic>

namespace tattler
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

  private:
    PipeClient m_pipeClient;
    std::atomic<bool> m_pipeConnected = false;
    std::atomic<bool> m_isCapturing = false;
    // CaptureBuffer m_buffer;
};

} // namespace tattler
