#include "stdafx.h"

#include "viewer/texture_preview_panel.h"

#include "imgui.h"

namespace Tattler
{

void TexturePreviewPanel::Draw(const CapturedEvent* selectedEvent,
                                const CaptureSnapshot* /*snapshot*/)
{
    ImGui::Begin("Details");

    if (!selectedEvent)
    {
        ImGui::TextDisabled("Select an event to see details.");
        ImGui::End();
        return;
    }

    // --- Event identity ---
    ImGui::SeparatorText("Event");
    ImGui::Text("Frame:  %u", selectedEvent->frameIndex);
    ImGui::Text("Index:  %u", selectedEvent->eventIndex);

    // --- Type ---
    const char* typeName = "Unknown";
    switch (selectedEvent->type)
    {
    case EventType::Draw:            typeName = "Draw";            break;
    case EventType::DrawIndexed:     typeName = "DrawIndexed";     break;
    case EventType::Dispatch:        typeName = "Dispatch";        break;
    case EventType::ResourceBarrier: typeName = "ResourceBarrier"; break;
    case EventType::ClearRTV:        typeName = "ClearRTV";        break;
    case EventType::ClearDSV:        typeName = "ClearDSV";        break;
    case EventType::CopyResource:    typeName = "CopyResource";    break;
    case EventType::Present:         typeName = "Present";         break;
    }
    ImGui::Text("Type:   %s", typeName);

    // --- GPU timing ---
    ImGui::SeparatorText("GPU Timing");
    ImGui::Text("Begin:  %llu ticks", selectedEvent->timestampBegin);
    ImGui::Text("End:    %llu ticks", selectedEvent->timestampEnd);

    // --- Bound resources ---
    ImGui::SeparatorText("Resources");
    ImGui::Text("Command list:   0x%016llX", selectedEvent->commandList);
    ImGui::Text("Pipeline state: 0x%016llX", selectedEvent->pipelineState);
    ImGui::Text("Render target:  0x%016llX", selectedEvent->renderTarget);

    // --- Params ---
    ImGui::SeparatorText("Parameters");
    std::visit(
        [](const auto& p)
        {
            using T = std::decay_t<decltype(p)>;

            if constexpr (std::is_same_v<T, DrawParams>)
            {
                ImGui::Text("Vertex count:   %u", p.vertexCount);
                ImGui::Text("Instance count: %u", p.instanceCount);
            }
            else if constexpr (std::is_same_v<T, DrawIndexedParams>)
            {
                ImGui::Text("Index count:    %u", p.indexCount);
                ImGui::Text("Instance count: %u", p.instanceCount);
            }
            else if constexpr (std::is_same_v<T, DispatchParams>)
            {
                ImGui::Text("Groups: %u x %u x %u", p.x, p.y, p.z);
            }
            else if constexpr (std::is_same_v<T, BarrierParams>)
            {
                ImGui::Text("Resource: 0x%016llX", p.resource);
                ImGui::Text("Before:   0x%08X",    static_cast<uint32_t>(p.before));
                ImGui::Text("After:    0x%08X",    static_cast<uint32_t>(p.after));
            }
            else if constexpr (std::is_same_v<T, ClearRtvParams>)
            {
                ImGui::Text("Render target: 0x%016llX", p.renderTarget);
                ImGui::Text("Color: (%.2f, %.2f, %.2f, %.2f)",
                            p.color[0], p.color[1], p.color[2], p.color[3]);
            }
            else if constexpr (std::is_same_v<T, ClearDsvParams>)
            {
                ImGui::Text("Depth stencil: 0x%016llX", p.depthStencil);
                ImGui::Text("Depth:   %.3f", p.depth);
                ImGui::Text("Stencil: %u",   p.stencil);
            }
            else if constexpr (std::is_same_v<T, CopyParams>)
            {
                ImGui::Text("Src: 0x%016llX", p.src);
                ImGui::Text("Dst: 0x%016llX", p.dst);
            }
            else if constexpr (std::is_same_v<T, PresentParams>)
            {
                ImGui::Text("Sync interval: %u", p.syncInterval);
                ImGui::Text("Flags:         0x%08X", p.flags);
            }
        },
        selectedEvent->params);

    ImGui::End();
}

} // namespace Tattler
