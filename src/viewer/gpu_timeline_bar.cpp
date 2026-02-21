#include "stdafx.h"

#include "imgui.h"
#include "viewer/gpu_timeline_bar.h"
#include "viewer/style.h"

#include <algorithm>
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
        ImGui::TextDisabled("No capture data :(");
        ImGui::End();
        return;
    }

    // Find the overall timestamp range across all frames
    uint64_t minTs = UINT64_MAX;
    uint64_t maxTs = 0;
    for (const auto& frame : snapshot->frames)
        for (const auto& event : frame.events)
        {
            minTs = std::min(minTs, event.timestampBegin);
            maxTs = std::max(maxTs, event.timestampEnd);
        }

    if (minTs >= maxTs)
    {
        ImGui::TextDisabled("No GPU timing data :(");
        ImGui::End();
        return;
    }

    const float rowHeight = 20.0f;
    const float totalRange = static_cast<float>(maxTs - minTs);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (const auto& frame : snapshot->frames)
    {
        // Frame label on the left
        char label[16];
        snprintf(label, sizeof(label), "F%u", frame.frameNumber);
        ImGui::TextUnformatted(label);
        ImGui::SameLine();

        // Remaining row width is the timeline bar
        ImVec2 barOrigin = ImGui::GetCursorScreenPos();
        float barWidth = ImGui::GetContentRegionAvail().x;

        for (const auto& event : frame.events)
        {
            float x0 =
                barOrigin.x + static_cast<float>(event.timestampBegin - minTs) /
                                  totalRange * barWidth;
            float x1 =
                barOrigin.x + static_cast<float>(event.timestampEnd - minTs) /
                                  totalRange * barWidth;
            x1 = std::max(x1, x0 + 2.0f); // ensure minimum visible width

            ImVec4 col = EventColor(event.type);
            if (selectedEvent != nullptr && selectedEvent != &event)
                col.w *=
                    0.25f; // dim unselected events when a selection is active
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

            if (ImGui::IsMouseHoveringRect(
                    {x0, barOrigin.y}, {x1, barOrigin.y + rowHeight - 2.0f}))
            {
                double ms = frame.gpuFrequency > 0
                                ? static_cast<double>(event.timestampEnd -
                                                      event.timestampBegin) /
                                      static_cast<double>(frame.gpuFrequency) *
                                      1000.0
                                : 0.0;
                ImGui::SetTooltip("%s\nFrame %u · Event %u\n%.3f ms",
                                  EventTypeName(event.type), event.frameIndex,
                                  event.eventIndex, ms);
            }
        }

        // Advance cursor past the drawn bar
        ImGui::Dummy({barWidth, rowHeight});
    }

    ImGui::End();
}

} // namespace Tattler
