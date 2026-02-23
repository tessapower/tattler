#include "stdafx.h"

#include "common/capture_types.h"
#include "hook/capture_buffer.h"

#include <mutex>
#include <utility>

namespace Tattler
{

auto CaptureBuffer::AddEvent(CapturedEvent event) -> void
{
    // Lock mutex
    std::lock_guard lock(m_mutex);
    // Move the event into buffer
    m_events.push_back(std::move(event));
}

auto CaptureBuffer::Flush() -> std::vector<CapturedEvent>
{
    // Lock mutex
    std::lock_guard lock(m_mutex);

    // Create local copy of events, clear out buffer
    std::vector<CapturedEvent> events = std::move(m_events);

    // Return local copy
    return events;
}

} // namespace Tattler
