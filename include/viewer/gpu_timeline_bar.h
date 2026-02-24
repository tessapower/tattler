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
    struct Action
    {
        enum class Type
        {
            None,
            FrameChanged,      // user clicked Prev / Next
            EventClicked,      // user clicked an event bar
            EmptySpaceClicked, // user clicked in the timeline but not on an event
        };
        Type type = Type::None;
        int frameIndex = -1;               // valid when FrameChanged
        const CapturedEvent* event = nullptr; // valid when EventClicked
    };

    /// <summary>
    /// Draw the panel. Returns an action describing any user interaction.
    /// Should be called between ImGui::NewFrame and ImGui::Render.
    /// </summary>
    auto Draw(const CaptureSnapshot* snapshot) -> Action;

    /// <summary>
    /// Set the visually highlighted event (shown with an outline).
    /// Also triggers a scroll so the event is centred on the next Draw().
    /// Pass nullptr to clear the highlight without scrolling.
    /// </summary>
    auto SetHighlight(const CapturedEvent* event) -> void;

    /// <summary>
    /// Jump to the given frame index and reset the scroll offset.
    /// Does not change the current highlight.
    /// </summary>
    auto SyncToFrame(int frameIndex) -> void;

  private:
    float m_zoomLevel = 1.0f;            // 1.0 = 100%
    float m_scrollOffset = 0.0f;         // horizontal scroll when zoomed
    int m_selectedFrameIndex = 0;        // currently displayed frame
    int m_highlightFrameIdx = -1;        // frame index of the highlighted event
    int m_highlightEventIdx = -1;        // event index of the highlighted event
    bool m_pendingScrollToEvent = false; // centre on highlight next Draw()
};

} // namespace Tattler
