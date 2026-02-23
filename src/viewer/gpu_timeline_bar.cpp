#include "stdafx.h"

#include "common/capture_types.h"
#include "imgui.h"
#include "viewer/gpu_timeline_bar.h"
#include "viewer/style.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace Tattler
{

static ImVec4 EventColor(EventType type)
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

void GpuTimelineBar::Draw(const CaptureSnapshot* snapshot,
                          const CapturedEvent* selectedEvent)
{
    ImGui::Begin("Timeline");

    if (!snapshot || snapshot->frames.empty())
    {
        ImGui::TextDisabled("No capture data.");
        ImGui::End();
        return;
    }

    // Clamp selected frame index
    m_selectedFrameIndex = std::clamp(
        m_selectedFrameIndex, 0, static_cast<int>(snapshot->frames.size()) - 1);
    const CapturedFrame& currentFrame = snapshot->frames[m_selectedFrameIndex];

    // Frame navigation
    if (ImGui::Button("<< Prev"))
    {
        m_selectedFrameIndex = std::max(0, m_selectedFrameIndex - 1);
        m_scrollOffset = 0.0f; // Reset scroll when changing frames
    }
    ImGui::SameLine();
    ImGui::Text("Frame %u / %zu", currentFrame.frameNumber,
                snapshot->frames.size());
    ImGui::SameLine();
    if (ImGui::Button("Next >>"))
    {
        m_selectedFrameIndex =
            std::min(static_cast<int>(snapshot->frames.size()) - 1,
                     m_selectedFrameIndex + 1);
        m_scrollOffset = 0.0f; // Reset scroll when changing frames
    }
    ImGui::SameLine();
    ImGui::Dummy({20.0f, 0.0f}); // Spacing
    ImGui::SameLine();

    // Find the timestamp range for the current frame (needed for zoom
    // calculations)
    uint64_t minTs = UINT64_MAX;
    uint64_t maxTs = 0;
    for (const auto& event : currentFrame.events)
    {
        minTs = std::min(minTs, event.timestampBegin);
        maxTs = std::max(maxTs, event.timestampEnd);
    }

    if (minTs >= maxTs)
    {
        ImGui::TextDisabled("No GPU timing data for this frame.");
        ImGui::End();
        return;
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

    // Clamp scroll offset
    float maxScroll = std::max(0.0f, zoomedWidth - barWidth);
    m_scrollOffset = std::clamp(m_scrollOffset, 0.0f, maxScroll);

    for (const auto& event : currentFrame.events)
    {
        float x0 = barOrigin.x +
                   static_cast<float>(event.timestampBegin - minTs) /
                       totalRange * zoomedWidth -
                   m_scrollOffset;
        float x1 = barOrigin.x +
                   static_cast<float>(event.timestampEnd - minTs) / totalRange *
                       zoomedWidth -
                   m_scrollOffset;
        x1 = std::max(x1, x0 + 2.0f); // ensure minimum visible width

        // Skip drawing events that are completely outside the visible area
        if (x1 < barOrigin.x || x0 > barOrigin.x + barWidth)
            continue;

        // Clip to visible area
        x0 = std::max(x0, barOrigin.x);
        x1 = std::min(x1, barOrigin.x + barWidth);

        ImVec4 col = EventColor(event.type);
        if (selectedEvent != nullptr && selectedEvent != &event)
            col.w *= 0.25f; // dim unselected events when a selection is active
        drawList->AddRectFilled({x0, barOrigin.y},
                                {x1, barOrigin.y + rowHeight - 2.0f},
                                ImGui::ColorConvertFloat4ToU32(col), 2.0f);

        if (selectedEvent == &event)
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
        }
    }

    // Advance cursor past the drawn bar
    ImGui::Dummy({barWidth, rowHeight});

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
}

} // namespace Tattler
