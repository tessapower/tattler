#include "stdafx.h"

#include "common/capture_types.h"
#include "imgui.h"
#include "viewer/frame_tree_panel.h"

#include <cstdint>

namespace Tattler
{

static const char* EventTypeName(EventType type)
{
    switch (type)
    {
    case EventType::Draw:
        return "Draw";
    case EventType::DrawIndexed:
        return "DrawIndexed";
    case EventType::Dispatch:
        return "Dispatch";
    case EventType::ResourceBarrier:
        return "ResourceBarrier";
    case EventType::ClearRTV:
        return "ClearRTV";
    case EventType::ClearDSV:
        return "ClearDSV";
    case EventType::CopyResource:
        return "CopyResource";
    case EventType::Present:
        return "Present";
    default:
        return "Unknown";
    }
}

/// <summary>
/// Converts raw GPU timestamp ticks to wall-clock milliseconds using the
/// frequency reported by the GPU. Returns 0 if the frequency is unknown or
/// the timestamps are inverted (which can happen on the first captured event).
/// </summary>
/// <param name="e">The event whose duration to calculate.</param>
/// <param name="gpuFrequency">The GPU timestamp frequency in Hz.</param>
/// <returns>The duration of the event in milliseconds, or 0 if it can't be
/// calculated.</returns>
static double GpuDurationMs(const CapturedEvent& e, uint64_t gpuFrequency)
{
    if (gpuFrequency == 0 || e.timestampEnd <= e.timestampBegin)
        return 0.0;

    return static_cast<double>(e.timestampEnd - e.timestampBegin) /
           static_cast<double>(gpuFrequency) * 1000.0;
}

void FrameTreePanel::Draw(const CaptureSnapshot* snapshot)
{
    // Cache the snapshot pointer so we can validate indices against the same
    // data we rendered with this frame
    m_lastSnapshot = snapshot;

    ImGui::Begin("Draw Calls");

    if (!snapshot || snapshot->frames.empty())
    {
        ImGui::TextDisabled("No capture data.");
        ImGui::End();
        return;
    }

    for (int frameIdx = 0; frameIdx < static_cast<int>(snapshot->frames.size());
         ++frameIdx)
    {
        const auto& frame = snapshot->frames[frameIdx];

        // Sum all event durations so we can show the total GPU time in the
        // collapsible frame header without the user having to expand it
        double totalMs = 0.0;
        for (const auto& e : frame.events)
            totalMs += GpuDurationMs(e, frame.gpuFrequency);

        // ### suffix makes ImGui use only the part after ### as the
        // stable ID, so the visible label (which changes as data updates)
        // doesn't cause the tree node to collapse on every redraw
        char nodeLabel[64];
        snprintf(nodeLabel, sizeof(nodeLabel),
                 "Frame %u  (%zu events, %.2f ms)###frame%d", frame.frameNumber,
                 frame.events.size(), totalMs, frameIdx);

        if (ImGui::TreeNodeEx(nodeLabel, ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (int eventIdx = 0;
                 eventIdx < static_cast<int>(frame.events.size()); ++eventIdx)
            {
                const auto& event = frame.events[eventIdx];
                bool selected = (m_selectedFrame == frameIdx &&
                                 m_selectedEvent == eventIdx);

                char rowLabel[64];
                snprintf(rowLabel, sizeof(rowLabel), "[%u] %-16s  %.3f ms",
                         event.eventIndex, EventTypeName(event.type),
                         GpuDurationMs(event, frame.gpuFrequency));

                if (ImGui::Selectable(rowLabel, selected))
                {
                    // Clicking an already-selected row clears the selection,
                    // restoring the timeline to unfiltered view
                    if (selected)
                    {
                        m_selectedFrame = -1;
                        m_selectedEvent = -1;
                    }
                    else
                    {
                        m_selectedFrame = frameIdx;
                        m_selectedEvent = eventIdx;
                    }
                }
            }

            ImGui::TreePop();
        }
    }

    ImGui::End();
}

auto FrameTreePanel::GetSelectedEvent() const -> const CapturedEvent*
{
    // m_selectedFrame/Event are initialised to -1 and reset to -1 on
    // deselection, so a negative value always means "nothing selected"
    if (!m_lastSnapshot || m_selectedFrame < 0 || m_selectedEvent < 0)
        return nullptr;

    // Guard against stale indices if the snapshot is replaced mid-session
    if (m_selectedFrame >= static_cast<int>(m_lastSnapshot->frames.size()))
        return nullptr;

    const auto& frame = m_lastSnapshot->frames[m_selectedFrame];
    if (m_selectedEvent >= static_cast<int>(frame.events.size()))
        return nullptr;

    return &frame.events[m_selectedEvent];
}

} // namespace Tattler
