#include "stdafx.h"

#include "viewer/viewer_app.h"
#include "viewer/d3d12_renderer.h"

#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"

// Forward-declared in imgui_impl_win32.h behind a comment block
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT,
                                                              WPARAM, LPARAM);

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
    if (!m_renderer->Init(m_hwnd,
                          static_cast<UINT>(clientRect.right - clientRect.left),
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
        this         // Pointer to this ViewerApp, retrieved in HandleMsgSetup
    );

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

    // Load Roboto at the DPI-scaled pixel size with oversampling for sharpness
    io.Fonts->AddFontFromFileTTF("resources/fonts/Roboto-Regular.ttf",
                                  16.0f * mainScale);

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

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
        auto* allocator =
            static_cast<SrvDescriptorAllocator*>(info->UserData);
        allocator->Alloc(out_cpu_handle, out_gpu_handle);
    };

    initInfo.SrvDescriptorFreeFn =
        [](ImGui_ImplDX12_InitInfo* info,
           D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle,
           D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
    {
        auto* allocator =
            static_cast<SrvDescriptorAllocator*>(info->UserData);
        allocator->Free(cpu_handle, gpu_handle);
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
        if (ImGui::Button("Launch App..."))
        {
            // m_processLauncher.ShowDialog();
        }
        if (ImGui::Button("Capture"))
        {
            // m_captureClient.StartCapture();
        }
        ImGui::EndMainMenuBar();
    }

    // Panels
    // m_frameTree.Draw(m_captureClient.GetSnapshot());
    // m_texturePreview.Draw(m_frameTree.GetSelectedEvent(),
    //                       m_captureClient.GetSnapshot());
    // m_timeline.Draw(m_captureClient.GetSnapshot(),
    //                 m_frameTree.GetSelectedEvent());

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
        // Retrieve the lpParam we passed in when creating the hWnd
        const CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        auto pWindow = static_cast<ViewerApp*>(pCreate->lpCreateParams);

        // Set the USERDATA to point to this window
        SetWindowLongPtr(hwnd, GWLP_USERDATA,
                         reinterpret_cast<LONG_PTR>(pWindow));
        // Now set the WNDPROC to point to handleMsgThunk
        SetWindowLongPtr(hwnd, GWLP_WNDPROC,
                         reinterpret_cast<LONG_PTR>(&ViewerApp::MsgThunk));

        return pWindow->HandleMsg(hwnd, uMsg, wParam, lParam);
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

auto __stdcall ViewerApp::MsgThunk(HWND hwnd, UINT uMsg, WPARAM wParam,
                                LPARAM lParam) noexcept -> LRESULT
{
    // Get a pointer to the window associated with the given h_wnd
    const auto pWindow =
        reinterpret_cast<ViewerApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    // Forward on the message to the Window instance
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

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    m_renderer->Shutdown();
    CleanupWindow();
}

} // namespace tattler
