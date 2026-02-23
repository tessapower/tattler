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

  private:
    float m_zoomLevel = 1.0f;  // 1.0 = 100%, higher = more zoomed in
    float m_scrollOffset = 0.0f; // horizontal scroll position when zoomed
    int m_selectedFrameIndex = 0; // currently displayed frame
};

} // namespace Tattler
