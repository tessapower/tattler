#pragma once

#include "common/capture_types.h"

namespace tattler
{

/// <summary>
/// Left panel containing the tree of captured frames and their events.
/// </summary>
class FrameTreePanel
{
  public:
    /// <summary>
    /// Draw the panel. Should be called between ImGui::NewFrame
    /// and ImGui::Render.
    /// </summary>
    void Draw(const CaptureSnapshot* snapshot);

    /// <summary>
    /// Returns the currently selected event, or nullptr if none.
    /// </summary>
    auto GetSelectedEvent() const -> const CapturedEvent*;

  private:
    int m_selectedFrame = -1;
    int m_selectedEvent = -1;
    const CaptureSnapshot* m_lastSnapshot = nullptr;
};

} // namespace tattler
