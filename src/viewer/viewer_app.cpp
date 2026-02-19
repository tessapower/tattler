#include "stdafx.h"

#include "IconsFontAwesome6.h"
#include "common/capture_types.h"
#include "common/pipe_protocol.h"
#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
#include "imgui_internal.h" // DockBuilder* API
#include "viewer/d3d12_renderer.h"
#include "viewer/pipe_server.h"
#include "viewer/style.h"
#include "viewer/viewer_app.h"

// Forward-declared in imgui_impl_win32.h behind a comment block
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM,
                                                             LPARAM);

namespace tattler
{
ViewerApp::ViewerApp()
{
    m_renderer = std::make_unique<D3D12Renderer>();
}

// Defined here (not in header) because unique_ptr needs the full D3D12Renderer
// type to call its destructor, and the header only forward-declares it.
ViewerApp::~ViewerApp() = default;

auto ViewerApp::Init(HINSTANCE hInstance, LPCTSTR title) -> bool
{
    m_hinstance = hInstance;

    // Enable DPI awareness for better scaling on high-DPI displays
    ImGui_ImplWin32_EnableDpiAwareness();
    // Get monitor scale
    float mainScale = ImGui_ImplWin32_GetDpiScaleForMonitor(
        MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY));

    InitWindow(mainScale, title);

    // Create D3D12 device — swap chain size must match the actual pixel
    // dimensions of the client area, not the logical (pre-DPI) size
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);
    if (!m_renderer->Init(
            m_hwnd, static_cast<UINT>(clientRect.right - clientRect.left),
            static_cast<UINT>(clientRect.bottom - clientRect.top)))
    {
        m_renderer->Shutdown();
        CleanupWindow();
        return false;
    }

    ShowWindow(m_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(m_hwnd);

    if (!InitImGui(mainScale))
    {
        m_renderer->Shutdown();
        CleanupWindow();

        return false;
    }

    if (!m_pipeServer.Create())
        throw std::runtime_error("Failed to create pipe server");

    // Connect to the pipe immediately — the hook will wait until a connection
    // is made before sending messages, so this ensures we don't miss any
    // messages sent early in the capture process (e.g. StartCapture)
    // We do this on a separate thread so we don't block the main thread if the
    // hook isn't injected yet and isn't connecting to the pipe.
    m_pipeThread = std::thread(
        [this]()
        {
            OutputDebugString(TEXT("Waiting for pipe connection...\n"));
            if (m_pipeServer.Connect())
            {
                OutputDebugString(TEXT("Pipe client connected!\n"));
                m_pipeConnected = true;
            }
            else
            {
                OutputDebugString(TEXT("Failed to connect to pipe client\n"));
            }
        });

    return true;
}

auto ViewerApp::InitWindow(float mainScale, LPCTSTR title) -> void
{
    constexpr DWORD WINDOW_STYLE =
        WS_OVERLAPPED | WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU;
    constexpr DWORD WINDOW_EX_STYLE = WS_EX_OVERLAPPEDWINDOW;

    // Register window class
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = &ViewerApp::HandleMsgSetup;
    wc.hInstance = m_hinstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = m_className;

    if (!RegisterClassEx(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        throw std::runtime_error("Failed to register window class: " +
                                 std::to_string(GetLastError()));
    }

    // Calculate window size (includes borders and title bar)
    RECT viewport{0, 0, static_cast<long>(m_width * mainScale),
                  static_cast<long>(m_height * mainScale)};
    AdjustWindowRectEx(&viewport, WINDOW_STYLE, false, WINDOW_EX_STYLE);

    // Create window
    m_hwnd = CreateWindowEx(
        WINDOW_EX_STYLE,                // Extended window style(s)
        m_className,                    // Window class name
        title,                          // Window name in title bar
        WINDOW_STYLE,                   // Window style
        CW_USEDEFAULT,                  // x position of window
        CW_USEDEFAULT,                  // y position of window
        viewport.right - viewport.left, // Client width
        viewport.bottom - viewport.top, // Client height
        nullptr,                        // Handle to parent window
        nullptr,                        // Handle to menu
        m_hinstance, // Handle to instance to be associated with window
        this);       // Pointer to this ViewerApp, retrieved in HandleMsgSetup

    if (!m_hwnd)
    {
        throw std::runtime_error("Failed to create window: " +
                                 std::to_string(GetLastError()));
    }
}

auto ViewerApp::CleanupWindow() -> void
{
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
        UnregisterClass(m_className, m_hinstance);
    }

    OutputDebugString(TEXT("Window destroyed\n"));
}

auto ViewerApp::InitImGui(float mainScale) -> bool
{
    // Set up Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Load Roboto at the DPI-scaled pixel size
    io.Fonts->AddFontFromFileTTF("resources/fonts/Roboto-Regular.ttf",
                                 16.0f * mainScale);

    // Merge Font Awesome Solid icons on top of Roboto
    ImFontConfig faCfg;
    faCfg.MergeMode = true;
    faCfg.PixelSnapH = true;
    faCfg.GlyphMinAdvanceX = 16.0f * mainScale; // keep icons monospaced
    static const ImWchar faRanges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
    io.Fonts->AddFontFromFileTTF("resources/fonts/fa-solid-900.ttf",
                                 16.0f * mainScale, &faCfg, faRanges);

    // Apply colour scheme before ScaleAllSizes so rounding values
    // are scaled correctly
    ApplyTheme(m_theme);

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(mainScale);

    // Setup platform/renderer backends
    ImGui_ImplWin32_Init(m_hwnd);

    ImGui_ImplDX12_InitInfo initInfo = {};
    initInfo.Device = m_renderer->GetDevice();
    initInfo.CommandQueue = m_renderer->GetCommandQueue();
    initInfo.NumFramesInFlight = D3D12Renderer::BUFFER_COUNT;
    initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
    initInfo.SrvDescriptorHeap = m_renderer->GetSrvHeap();
    // Use the renderer's SRV allocator via UserData so lambdas stay captureless
    initInfo.UserData = m_renderer->GetSrvHeapAllocator();

    initInfo.SrvDescriptorAllocFn =
        [](ImGui_ImplDX12_InitInfo* info,
           D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle,
           D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
    {
        static_cast<SrvDescriptorAllocator*>(info->UserData)
            ->Alloc(out_cpu_handle, out_gpu_handle);
    };

    initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info,
                                      D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle,
                                      D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
    {
        static_cast<SrvDescriptorAllocator*>(info->UserData)
            ->Free(cpu_handle, gpu_handle);
    };

    ImGui_ImplDX12_Init(&initInfo);

    return true;
}

auto ViewerApp::RenderFrame() -> void
{
    if (!m_renderer->BeginFrame(m_hwnd))
        return;

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Toolbar
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::Button(ICON_FA_FOLDER_OPEN " Launch App"))
        {
            // m_processLauncher.ShowDialog();
        }

        if (ImGui::Button(ICON_FA_PLAY " Capture"))
        {
            if (m_pipeConnected)
                m_pipeServer.SendMessage(
                    PipeProtocol::MessageType::StartCapture, nullptr, 0);
        }

        // Theme toggle — right-aligned
        const char* themeLabel =
            m_theme == Theme::RosePineDawn ? ICON_FA_SUN : ICON_FA_MOON;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                             ImGui::GetContentRegionAvail().x -
                             ImGui::CalcTextSize(themeLabel).x -
                             ImGui::GetStyle().FramePadding.x * 2.0f);
        if (ImGui::Button(themeLabel))
        {
            m_theme = (m_theme == Theme::RosePineDawn) ? Theme::RosePine
                                                       : Theme::RosePineDawn;
            ApplyTheme(m_theme);
        }

        ImGui::EndMainMenuBar();
    }

    // Full-screen dock space below the menu bar
    ImGuiID dockspaceId =
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

    // Build default layout once and imgui.ini takes over after first run.
    // DockSpaceOverViewport always creates the node, so we check IsLeafNode()
    // to detect a fresh unsplit space rather than checking for nullptr.
    ImGuiDockNode* dockNode = ImGui::DockBuilderGetNode(dockspaceId);
    if (dockNode == nullptr || dockNode->IsLeafNode())
    {
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId,
                                      ImGui::GetMainViewport()->WorkSize);

        ImGuiID dockMain = dockspaceId;
        ImGuiID dockBottom = ImGui::DockBuilderSplitNode(
            dockMain, ImGuiDir_Down, 0.30f, nullptr, &dockMain);
        ImGuiID dockLeft = ImGui::DockBuilderSplitNode(
            dockMain, ImGuiDir_Left, 0.30f, nullptr, &dockMain);

        ImGui::DockBuilderDockWindow("Timeline", dockBottom);
        ImGui::DockBuilderDockWindow("Draw Calls", dockLeft);
        ImGui::DockBuilderDockWindow("Details", dockMain);

        ImGui::DockBuilderFinish(dockspaceId);
    }

    // Draw panels
    m_frameTree.Draw(&m_snapshot);
    const CapturedEvent* selected = m_frameTree.GetSelectedEvent();
    m_gpuTimeline.Draw(&m_snapshot, selected);
    m_details.Draw(selected, &m_snapshot);

    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(),
                                  m_renderer->GetCommandList());

    m_renderer->EndFrame();
}

auto __stdcall ViewerApp::HandleMsgSetup(HWND hwnd, UINT uMsg, WPARAM wParam,
                                         LPARAM lParam) noexcept -> LRESULT
{
    if (uMsg == WM_NCCREATE)
    {
        // Retrieve the lpParam we passed in when creating the hwnd
        const CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        auto pWindow = static_cast<ViewerApp*>(pCreate->lpCreateParams);

        // Set the USERDATA to point to this window
        SetWindowLongPtr(hwnd, GWLP_USERDATA,
                         reinterpret_cast<LONG_PTR>(pWindow));
        // Now set the WNDPROC to point to MsgThunk
        SetWindowLongPtr(hwnd, GWLP_WNDPROC,
                         reinterpret_cast<LONG_PTR>(&ViewerApp::MsgThunk));

        return pWindow->HandleMsg(hwnd, uMsg, wParam, lParam);
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

auto __stdcall ViewerApp::MsgThunk(HWND hwnd, UINT uMsg, WPARAM wParam,
                                   LPARAM lParam) noexcept -> LRESULT
{
    // Get a pointer to the window associated with the given hwnd
    const auto pWindow =
        reinterpret_cast<ViewerApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    // Forward on the message to the ViewerApp instance
    return pWindow->HandleMsg(hwnd, uMsg, wParam, lParam);
}

auto ViewerApp::HandleMsg(HWND hwnd, UINT uMsg, WPARAM wParam,
                          LPARAM lParam) noexcept -> LRESULT
{
    // Let ImGui process input first — if it handles the event, skip our logic
    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
        return true;

    switch (uMsg)
    {
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_NCDESTROY:
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

auto ViewerApp::Run() -> int
{
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            RenderFrame();
        }
    }

    return static_cast<int>(msg.wParam);
}

auto ViewerApp::Shutdown() -> void
{
    m_renderer->WaitForGpu();

    // Shut down the pipe server thread before releasing D3D resources, in case
    // the thread is in the middle of sending a message and using the renderer's
    // SRV heap (e.g. sending CaptureData with texture SRVs)
    if (m_pipeThread.joinable())
        m_pipeThread.join();

    // Disconnect and destroy the pipe server
    m_pipeServer.Disconnect();
    m_pipeServer.Destroy();

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    m_renderer->Shutdown();
    CleanupWindow();
}

} // namespace tattler
