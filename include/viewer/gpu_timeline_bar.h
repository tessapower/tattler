#pragma once

#include "common/capture_types.h"

namespace Tattler
{

/// <summary>
/// Bottom panel containing horizontal GPU timeline showing event durations
/// per frame.
/// </summary>
class GpuTimelineBar
{
  public:
    /// <summary>
    /// Draw the panel. Should be called between ImGui::NewFrame
    /// and ImGui::Render.
    /// </summary>
    void Draw(const CaptureSnapshot* snapshot, const CapturedEvent* selectedEvent);
};

} // namespace Tattler
