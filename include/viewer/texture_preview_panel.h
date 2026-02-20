#pragma once

#include "common/capture_types.h"

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
    void Draw(const CapturedEvent* selectedEvent, const CaptureSnapshot* snapshot);
};

} // namespace Tattler
