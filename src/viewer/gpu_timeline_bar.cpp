#include "stdafx.h"

#include "common/capture_types.h"
#include "imgui.h"
#include "viewer/gpu_timeline_bar.h"
#include "viewer/style.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Tattler
{

static auto EventColor(EventType type) -> ImVec4
{
    const Palette& p = GetCurrentPalette();
    switch (type)
    {
    case EventType::Draw:
        return p.pine;
    case EventType::DrawIndexed:
        return p.foam;
    case EventType::Dispatch:
        return p.iris;
    case EventType::ResourceBarrier:
        return p.gold;
    case EventType::ClearRTV:
        return p.rose;
    case EventType::ClearDSV:
        return p.love;
    case EventType::CopyResource:
        return p.muted;
    case EventType::Present:
        return p.subtle;
    default:
        return p.hlHigh;
    }
}

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

auto GpuTimelineBar::Draw(const CaptureSnapshot* snapshot) -> Action
{
    Action action;

    ImGui::Begin("Timeline");

    if (!snapshot || snapshot->frames.empty())
    {
        ImGui::TextDisabled("No capture data.");
        ImGui::End();
        return action;
    }

    // Clamp selected frame index
    m_selectedFrameIndex = std::clamp(
        m_selectedFrameIndex, 0, static_cast<int>(snapshot->frames.size()) - 1);
    const CapturedFrame& currentFrame = snapshot->frames[m_selectedFrameIndex];

    // Resolve highlight indices to a stable pointer into this frame's copy.
    // We store indices rather than a raw pointer because GetSnapshot() returns
    // a by-value copy each frame, so cross-frame pointers are unreliable.
    const CapturedEvent* highlightEvent = nullptr;
    if (m_highlightFrameIdx >= 0 &&
        m_highlightFrameIdx < static_cast<int>(snapshot->frames.size()))
    {
        const auto& hlFrame = snapshot->frames[m_highlightFrameIdx];
        if (m_highlightEventIdx >= 0 &&
            m_highlightEventIdx < static_cast<int>(hlFrame.events.size()))
        {
            highlightEvent = &hlFrame.events[m_highlightEventIdx];
        }
    }

    // Frame navigation
    if (ImGui::Button("<< Prev"))
    {
        const int prev = std::max(0, m_selectedFrameIndex - 1);
        m_scrollOffset = 0.0f;
        if (prev != m_selectedFrameIndex)
        {
            m_selectedFrameIndex = prev;
            m_highlightFrameIdx = -1;
            m_highlightEventIdx = -1;
            m_pendingScrollToEvent = false;
            action = {Action::Type::FrameChanged, m_selectedFrameIndex,
                      nullptr};
        }
    }
    ImGui::SameLine();
    ImGui::Text("Frame %u / %zu", currentFrame.frameNumber,
                snapshot->frames.size());
    ImGui::SameLine();
    if (ImGui::Button("Next >>"))
    {
        const int next = std::min(static_cast<int>(snapshot->frames.size()) - 1,
                                  m_selectedFrameIndex + 1);
        m_scrollOffset = 0.0f;
        if (next != m_selectedFrameIndex)
        {
            m_selectedFrameIndex = next;
            m_highlightFrameIdx = -1;
            m_highlightEventIdx = -1;
            m_pendingScrollToEvent = false;
            action = {Action::Type::FrameChanged, m_selectedFrameIndex,
                      nullptr};
        }
    }
    ImGui::SameLine();
    ImGui::Dummy({20.0f, 0.0f}); // Spacing
    ImGui::SameLine();

    // Find the timestamp range for the current frame.
    // Use the 5th-percentile begin timestamp as the lower bound so that
    // outlier events (e.g., barriers recorded on an early init command list
    // with near-zero GPU clock values) don't compress all real work into a
    // thin sliver at the far right of the bar.
    std::vector<uint64_t> sortedBegin;
    sortedBegin.reserve(currentFrame.events.size());
    for (const auto& event : currentFrame.events)
        sortedBegin.push_back(event.timestampBegin);
    std::sort(sortedBegin.begin(), sortedBegin.end());

    const size_t n = sortedBegin.size();
    uint64_t minTs = sortedBegin[n / 20]; // 5th-percentile begin

    uint64_t maxTs = 0;
    for (const auto& event : currentFrame.events)
        maxTs = std::max(maxTs, event.timestampEnd);

    // Fall back to true minimum if trimming emptied the range
    if (minTs >= maxTs)
        minTs = sortedBegin.front();

    if (minTs >= maxTs)
    {
        ImGui::TextDisabled("No GPU timing data for this frame.");
        ImGui::End();
        return action;
    }

    const float totalRange = static_cast<float>(maxTs - minTs);

    // Zoom controls
    if (ImGui::Button("-"))
    {
        m_zoomLevel = std::max(0.1f, m_zoomLevel * 0.8f);
    }
    ImGui::SameLine();
    ImGui::Text("%.0f%%", m_zoomLevel * 100.0f);
    ImGui::SameLine();
    if (ImGui::Button("+"))
    {
        m_zoomLevel = std::min(500.0f, m_zoomLevel * 1.25f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset"))
    {
        m_zoomLevel = 1.0f;
        m_scrollOffset = 0.0f;
    }
    ImGui::Separator();

    const float rowHeight = 20.0f;
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Handle mouse wheel zoom when hovering the timeline area
    if (ImGui::IsWindowHovered())
    {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f)
        {
            float oldZoom = m_zoomLevel;
            m_zoomLevel *= (1.0f + wheel * 0.1f);
            m_zoomLevel = std::clamp(m_zoomLevel, 0.1f, 500.0f);

            // Adjust scroll to zoom toward mouse position
            ImVec2 mousePos = ImGui::GetMousePos();
            ImVec2 windowPos = ImGui::GetWindowPos();
            float relativeX =
                (mousePos.x - windowPos.x) / ImGui::GetWindowWidth();
            m_scrollOffset += (m_zoomLevel - oldZoom) * relativeX *
                              ImGui::GetContentRegionAvail().x;
        }
    }

    // Timeline bar setup
    ImVec2 barOrigin = ImGui::GetCursorScreenPos();
    float barWidth = ImGui::GetContentRegionAvail().x;
    float zoomedWidth = barWidth * m_zoomLevel;

    // Scroll to centre the highlighted event on the first Draw()
    //  after it is set
    if (m_pendingScrollToEvent && highlightEvent &&
        static_cast<int>(highlightEvent->frameIndex) == m_selectedFrameIndex &&
        highlightEvent->timestampBegin >= minTs)
    {
        const float eventCenter =
            static_cast<float>(highlightEvent->timestampBegin - minTs) /
            totalRange * zoomedWidth;
        m_scrollOffset = eventCenter - barWidth * 0.5f;
        m_pendingScrollToEvent = false;
    }

    // Clamp scroll offset
    float maxScroll = std::max(0.0f, zoomedWidth - barWidth);
    m_scrollOffset = std::clamp(m_scrollOffset, 0.0f, maxScroll);

    // Track whether the user clicked anywhere inside the bar area
    const ImVec2 barEnd = {barOrigin.x + barWidth, barOrigin.y + rowHeight};
    bool clickedAnEvent = false;

    for (int eventIdx = 0;
         eventIdx < static_cast<int>(currentFrame.events.size()); ++eventIdx)
    {
        const auto& event = currentFrame.events[eventIdx];

        // Clamp to minTs before subtraction to guard against unsigned underflow
        // for outlier events whose timestamps fall below the display minimum
        const uint64_t clampedBegin = std::max(event.timestampBegin, minTs);
        const uint64_t clampedEnd = std::max(event.timestampEnd, minTs);

        float x0 = barOrigin.x +
                   static_cast<float>(clampedBegin - minTs) / totalRange *
                       zoomedWidth -
                   m_scrollOffset;
        float x1 =
            barOrigin.x +
            static_cast<float>(clampedEnd - minTs) / totalRange * zoomedWidth -
            m_scrollOffset;
        x1 = std::max(x1, x0 + 2.0f); // ensure minimum visible width

        // Skip drawing events that are completely outside the visible area
        if (x1 < barOrigin.x || x0 > barOrigin.x + barWidth)
            continue;

        // Clip to visible area
        x0 = std::max(x0, barOrigin.x);
        x1 = std::min(x1, barOrigin.x + barWidth);

        const bool isHighlighted =
            (m_highlightFrameIdx == m_selectedFrameIndex &&
             m_highlightEventIdx == eventIdx);

        ImVec4 col = EventColor(event.type);
        if (m_highlightFrameIdx >= 0 && !isHighlighted)
            col.w *= 0.25f; // dim when another event is highlighted
        drawList->AddRectFilled({x0, barOrigin.y},
                                {x1, barOrigin.y + rowHeight - 2.0f},
                                ImGui::ColorConvertFloat4ToU32(col), 2.0f);

        if (isHighlighted)
        {
            const Palette& p = GetCurrentPalette();
            drawList->AddRect(
                {x0, barOrigin.y}, {x1, barOrigin.y + rowHeight - 2.0f},
                ImGui::ColorConvertFloat4ToU32(p.love), 2.0f, 0, 2.0f);
        }

        if (ImGui::IsMouseHoveringRect({x0, barOrigin.y},
                                       {x1, barOrigin.y + rowHeight - 2.0f}))
        {
            double us =
                currentFrame.gpuFrequency > 0
                    ? static_cast<double>(event.timestampEnd -
                                          event.timestampBegin) /
                          static_cast<double>(currentFrame.gpuFrequency) *
                          1'000'000.0
                    : 0.0;
            ImGui::SetTooltip("%s\nFrame %u · Event %u\n%.3f us",
                              EventTypeName(event.type), event.frameIndex,
                              event.eventIndex, us);

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                // Toggle behavior: if already highlighted, unhighlight it
                if (isHighlighted)
                {
                    m_highlightFrameIdx = -1;
                    m_highlightEventIdx = -1;
                    action = {Action::Type::EmptySpaceClicked, -1, nullptr};
                }
                else
                {
                    m_highlightFrameIdx = m_selectedFrameIndex;
                    m_highlightEventIdx = eventIdx;
                    action = {Action::Type::EventClicked, -1, &event};
                }
                clickedAnEvent = true;
            }
        }
    }

    // Advance cursor past the drawn bar
    ImGui::Dummy({barWidth, rowHeight});

    // If the user clicked anywhere in the Timeline window but not on any event,
    // clear the highlight
    if (!clickedAnEvent && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
    {
        m_highlightFrameIdx = -1;
        m_highlightEventIdx = -1;
        action = {Action::Type::EmptySpaceClicked, -1, nullptr};
    }

    // Handle horizontal scroll when zoomed in
    if (m_zoomLevel > 1.0f)
    {
        ImGui::SameLine();
        ImGui::SetCursorPosX(barOrigin.x - ImGui::GetWindowPos().x);
        ImGui::InvisibleButton("##timeline_scroll", {barWidth, rowHeight});
        if (ImGui::IsItemActive() &&
            ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            m_scrollOffset -= ImGui::GetIO().MouseDelta.x;
        }
    }

    ImGui::End();
    return action;
}

auto GpuTimelineBar::SetHighlight(const CapturedEvent* event) -> void
{
    if (event)
    {
        m_highlightFrameIdx = static_cast<int>(event->frameIndex);
        m_highlightEventIdx = static_cast<int>(event->eventIndex);
    }
    else
    {
        m_highlightFrameIdx = -1;
        m_highlightEventIdx = -1;
    }
    m_pendingScrollToEvent = (event != nullptr);
}

auto GpuTimelineBar::SyncToFrame(int frameIndex) -> void
{
    m_selectedFrameIndex = frameIndex;
    m_scrollOffset = 0.0f;
}

} // namespace Tattler
