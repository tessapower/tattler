#pragma once

#include "common/capture_types.h"
#include "viewer/capture_client.h"
#include "viewer/frame_tree_panel.h"
#include "viewer/gpu_timeline_bar.h"
#include "viewer/style.h"
#include "viewer/texture_preview_panel.h"

#include <memory>

namespace tattler
{
class ViewerApp
{
  public:
    ViewerApp();
    ~ViewerApp();

    ViewerApp(const ViewerApp&) = delete;
    ViewerApp& operator=(const ViewerApp&) = delete;

    /// <summary>
    /// Initialise the Win32 window, D3D12 renderer, and Dear ImGui context.
    /// </summary>
    auto Init(HINSTANCE hInstance, LPCTSTR title) -> bool;

    /// <summary>
    /// Enter the main message loop. Returns the exit code from WM_QUIT.
    /// </summary>
    auto Run() -> int;

    /// <summary>
    /// Tear down ImGui, the renderer, and the Win32 window in order.
    /// </summary>
    auto Shutdown() -> void;

  private:
    //-------------------------------------------------------------- WIN32 --//
    HWND m_hwnd = nullptr;
    HINSTANCE m_hinstance = nullptr;
    UINT m_width = 960;
    UINT m_height = 540;
    static constexpr LPCTSTR m_className = TEXT("ViewerAppWindowClass");

    auto InitWindow(float mainScale, LPCTSTR title) -> void;

    auto CleanupWindow() -> void;

    static auto WINAPI HandleMsgSetup(HWND hwnd, UINT uMsg, WPARAM wParam,
                                      LPARAM lParam) noexcept -> LRESULT;

    static auto WINAPI MsgThunk(HWND hwnd, UINT uMsg, WPARAM wParam,
                                LPARAM lParam) noexcept -> LRESULT;

    auto HandleMsg(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) noexcept
        -> LRESULT;

    //----------------------------------------------------------- RENDERER --//
    std::unique_ptr<class D3D12Renderer> m_renderer;

    auto RenderFrame() -> void;

    //----------------------------------------------------- CAPTURE CLIENT --//
    CaptureClient m_captureClient;

    //-------------------------------------------------------------- IMGUI --//
    auto InitImGui(float mainScale) -> bool;

    // Panels
    FrameTreePanel m_frameTree;
    GpuTimelineBar m_gpuTimeline;
    TexturePreviewPanel m_details;

    // Active colour theme toggled from the toolbar
    Theme m_theme = Theme::RosePine;
};

} // namespace tattler
