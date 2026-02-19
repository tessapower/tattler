#pragma once

#include "common/capture_types.h"
#include "viewer/frame_tree_panel.h"
#include "viewer/gpu_timeline_bar.h"
#include "viewer/pipe_server.h"
#include "viewer/style.h"
#include "viewer/texture_preview_panel.h"

#include <thread>
#include <atomic>

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
    // Win32
    HWND m_hwnd = nullptr;
    HINSTANCE m_hinstance = nullptr;
    UINT m_width = 960;
    UINT m_height = 540;
    static constexpr LPCTSTR m_className = TEXT("ViewerAppWindowClass");
    PipeServer m_pipeServer;
    std::thread m_pipeThread;
    std::atomic<bool> m_pipeConnected = false;

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

    //-------------------------------------------------------------- IMGUI --//
    auto InitImGui(float mainScale) -> bool;

    // Panels
    FrameTreePanel m_frameTree;
    GpuTimelineBar m_gpuTimeline;
    TexturePreviewPanel m_details;

    // Active colour theme toggled from the toolbar
    Theme m_theme = Theme::RosePine;

    // Capture data: replaced with real data once hook is connected
    CaptureSnapshot m_snapshot;
};

} // namespace tattler
