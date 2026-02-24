#pragma once

#include "common/capture_types.h"

namespace Tattler
{

/// <summary>
/// Left panel containing the tree of captured frames and their events.
/// </summary>
class FrameTreePanel
{
  public:
    struct Action
    {
        enum class Type
        {
            None,
            FrameActivated, // user expanded a frame node
            EventSelected,  // user clicked a (different) event row
        };
        Type type = Type::None;
        int frameIndex = -1;
        int eventIndex = -1;
        const CapturedEvent* event = nullptr; // non-null for EventSelected
    };

    /// <summary>
    /// Draw the panel. Returns an action describing any user interaction.
    /// Should be called between ImGui::NewFrame and ImGui::Render.
    /// </summary>
    Action Draw(const CaptureSnapshot* snapshot);

    /// <summary>
    /// Returns the currently selected event, or nullptr if none.
    /// </summary>
    auto GetSelectedEvent() const -> const CapturedEvent*;

    /// <summary>
    /// Programmatically select an event and scroll the tree to show it.
    /// On the next Draw() the target frame node will be expanded and the
    /// event row will be scrolled into view.
    /// </summary>
    auto SetSelection(uint32_t frameIndex, uint32_t eventIndex) -> void;

    /// <summary>
    /// Clears the current event selection without scrolling.
    /// </summary>
    auto ClearSelection() -> void;

    /// <summary>
    /// Force-expands the given frame on the next Draw() without selecting
    /// any event.
    /// </summary>
    auto ExpandFrame(int frameIndex) -> void;

  private:
    int m_selectedFrame = -1;
    int m_selectedEvent = -1;
    const CaptureSnapshot* m_lastSnapshot = nullptr;
    bool m_expandAll = false;
    bool m_pendingCollapseAll = false; // force-close all nodes on next Draw()
    bool m_pendingSync = false;        // expand + scroll to event on next Draw()
    int m_pendingOpenFrame = -1;       // frame to force-expand (no event scroll)
};

} // namespace Tattler
