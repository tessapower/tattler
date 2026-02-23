#pragma once

#include "common/capture_types.h"
#include "imgui.h"

namespace Tattler
{

/// <summary>
/// Main panel containing details for the selected event render target preview.
/// </summary>
class TexturePreviewPanel
{
  public:
    /// <summary>
    /// Draw the panel. Should be called between ImGui::NewFrame
    /// and ImGui::Render.
    /// </summary>
    /// <param name="selectedEvent">The currently selected event, or nullptr if
    /// no selection.</param>
    /// <param name="frameTexture">The ImGui texture ID for the selected event's
    /// render target, or 0 if no texture is available.</param>
    void Draw(const CapturedEvent* selectedEvent, ImTextureID frameTexture);
};

} // namespace Tattler
