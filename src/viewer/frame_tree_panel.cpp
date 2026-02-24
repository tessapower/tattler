#include "stdafx.h"

#include "common/capture_types.h"
#include "imgui.h"
#include "viewer/frame_tree_panel.h"

#include <cstdint>

namespace Tattler
{

static auto EventTypeName(EventType type) -> const char*
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
/// Converts raw GPU timestamp ticks to wall-clock microseconds using the
/// frequency reported by the GPU. Returns 0 if the frequency is unknown or
/// the timestamps are inverted (which can happen on the first captured event).
/// </summary>
/// <param name="e">The event whose duration to calculate.</param>
/// <param name="gpuFrequency">The GPU timestamp frequency in Hz.</param>
/// <returns>The duration of the event in microseconds, or 0 if it can't be
/// calculated.</returns>
static auto GpuDurationUs(const CapturedEvent& e, uint64_t gpuFrequency) -> double
{
    if (gpuFrequency == 0 || e.timestampEnd <= e.timestampBegin)
        return 0.0;

    return static_cast<double>(e.timestampEnd - e.timestampBegin) /
           static_cast<double>(gpuFrequency) * 1'000'000.0;
}

auto FrameTreePanel::Draw(const CaptureSnapshot* snapshot) -> Action
{
    Action action;

    // Cache the snapshot pointer so we can validate indices against the same
    // data we rendered with this frame
    m_lastSnapshot = snapshot;

    ImGui::Begin("Draw Calls");

    if (!snapshot || snapshot->frames.empty())
    {
        ImGui::TextDisabled("No capture data.");
        ImGui::End();
        return action;
    }

    // Toggle button for expanding/collapsing all frames
    if (ImGui::Button(m_expandAll ? "Collapse All" : "Expand All"))
    {
        m_expandAll = !m_expandAll;
        if (!m_expandAll)
            m_pendingCollapseAll = true;
    }
    ImGui::Separator();

    for (int frameIdx = 0; frameIdx < static_cast<int>(snapshot->frames.size());
         ++frameIdx)
    {
        const auto& frame = snapshot->frames[frameIdx];

        // Sum all event durations so we can show the total GPU time in the
        // collapsible frame header without the user having to expand it
        double totalUs = 0.0;
        for (const auto& e : frame.events)
            totalUs += GpuDurationUs(e, frame.gpuFrequency);

        // ### suffix makes ImGui use only the part after ### as the
        // stable ID, so the visible label (which changes as data updates)
        // doesn't cause the tree node to collapse on every redraw
        char nodeLabel[64];
        snprintf(nodeLabel, sizeof(nodeLabel),
                 "Frame %u  (%zu events, %.2f us)###frame%d", frame.frameNumber,
                 frame.events.size(), totalUs, frameIdx);

        const bool isPendingOpen = (m_pendingOpenFrame == frameIdx);

        // Force open/close based on button state or one-shot requests.
        // SetNextItemOpen with ImGuiCond_Always overrides user-toggled state.
        if (m_pendingCollapseAll)
            ImGui::SetNextItemOpen(false, ImGuiCond_Always);
        else if (m_expandAll || (m_pendingSync && m_selectedFrame == frameIdx) ||
                 isPendingOpen)
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);

        if (ImGui::TreeNodeEx(nodeLabel, 0))
        {
            // Report FrameActivated only when the user manually expands the
            // node (not when we programmatically open it)
            if (ImGui::IsItemToggledOpen() && !m_pendingSync && !isPendingOpen &&
                !m_expandAll)
            {
                action = {Action::Type::FrameActivated, frameIdx, -1, nullptr};
            }

            for (int eventIdx = 0;
                 eventIdx < static_cast<int>(frame.events.size()); ++eventIdx)
            {
                const auto& event = frame.events[eventIdx];
                bool selected = (m_selectedFrame == frameIdx &&
                                 m_selectedEvent == eventIdx);

                char rowLabel[64];
                snprintf(rowLabel, sizeof(rowLabel), "[%u] %-16s  %.3f us",
                         event.eventIndex, EventTypeName(event.type),
                         GpuDurationUs(event, frame.gpuFrequency));

                if (ImGui::Selectable(rowLabel, selected))
                {
                    // Clicking an already-selected row does nothing
                    if (!selected)
                    {
                        m_selectedFrame = frameIdx;
                        m_selectedEvent = eventIdx;
                        action = {Action::Type::EventSelected, frameIdx,
                                  eventIdx, &event};
                    }
                }

                if (m_pendingSync && m_selectedFrame == frameIdx &&
                    m_selectedEvent == eventIdx)
                {
                    ImGui::SetScrollHereY();
                    m_pendingSync = false;
                }
            }

            ImGui::TreePop();
        }

        // Clear the one-shot open request after the node has been processed
        if (isPendingOpen)
            m_pendingOpenFrame = -1;
    }

    // One-shot flags are consumed after a single pass through all nodes
    m_pendingCollapseAll = false;

    ImGui::End();
    return action;
}

auto FrameTreePanel::SetSelection(uint32_t frameIndex, uint32_t eventIndex) -> void
{
    m_selectedFrame = static_cast<int>(frameIndex);
    m_selectedEvent = static_cast<int>(eventIndex);
    m_pendingSync = true;
}

auto FrameTreePanel::ClearSelection() -> void
{
    m_selectedFrame = -1;
    m_selectedEvent = -1;
    m_pendingSync = false;
}

auto FrameTreePanel::ExpandFrame(int frameIndex) -> void
{
    m_pendingOpenFrame = frameIndex;
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
