#pragma once

#include "common/capture_types.h"

#include <mutex>
#include <vector>

namespace tattler
{

/// <summary>
/// Thread-safe accumulator for GPU events recorded during a capture session.
///
/// The hook wrappers on the app's render thread call AddEvent() for every
/// intercepted draw/dispatch/etc. The capture controller calls Flush() at the
/// end of a capture to take ownership of all accumulated events at once.
///
/// At the start of each new frame, Reset() should be called to clear stale
/// data so events don't bleed across frames.
/// </summary>
class CaptureBuffer
{
  public:
    /// <summary>
    /// Appends a captured event to the buffer. Safe to call from any thread!
    /// </summary>
    auto AddEvent(CapturedEvent event) -> void;

    /// <summary>
    /// Moves all accumulated events out of the buffer and returns them.
    /// Safe to call from any thread!
    /// </summary>
    auto Flush() -> std::vector<CapturedEvent>;

    /// <summary>
    /// Discards all accumulated events without returning them. Call at the
    /// start of each new frame to clear stale data. Safe to call from any
    /// thread!
    /// </summary>
    auto Reset() -> void;

    /// <summary>
    /// Returns the number of events currently in the buffer.
    /// </summary>
    auto Size() const -> size_t;

  private:
    mutable std::mutex m_mutex;
    std::vector<CapturedEvent> m_events;
};

} // namespace tattler
