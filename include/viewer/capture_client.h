#pragma once

#include "common/capture_types.h"
#include "viewer/pipe_server.h"

#include <atomic>
#include <mutex>
#include <thread>

namespace Tattler
{

/// <summary>
/// Owns the named pipe server and background thread that receives
/// captured data from the hook DLL. The viewer uses this to send
/// capture commands and retrieve the latest snapshot.
/// </summary>
class CaptureClient
{
  public:
    CaptureClient();
    ~CaptureClient();

    /// <summary>
    /// Starts the capture client.
    /// </summary>
    auto Start() -> void;

    /// <summary>
    /// Stops the capture client and destroys the pipe server.
    /// </summary>
    auto Stop() -> void;

    auto SendStartCapture() -> void;

    auto SendStopCapture() -> void;

    auto IsConnected() const -> bool
    {
        return m_pipeConnected;
    }

    auto GetSnapshot() -> CaptureSnapshot
    {
        std::lock_guard lock(m_snapshotMutex);

        return m_snapshot;
    }

  private:
    PipeServer m_pipeServer;
    std::thread m_pipeThread;
    std::atomic<bool> m_pipeConnected = false;
    std::mutex m_snapshotMutex;
    CaptureSnapshot m_snapshot;
};

} // namespace Tattler
