#include "stdafx.h"

#include "viewer/d3d12_renderer.h"
#include "viewer/d3dx12.h"

namespace Tattler
{
auto D3D12Renderer::Init(HWND hwnd, UINT width, UINT height) -> bool
{
    // Create Factory
    UINT createFactoryFlags = 0;
    if (FAILED(
            CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&m_factory))))
        return false;

    // Enumerate Adapters (find best GPU)
    if (FAILED(m_factory->EnumAdapterByGpuPreference(
            0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&m_adapter))))
    {
        // Fallback to first available adapter
        if (FAILED(m_factory->EnumAdapters1(0, &m_adapter)))
            return false;
    }

    // Create device
    if (FAILED(D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_12_1,
                                 IID_PPV_ARGS(&m_device))))
        return false;

    // Create command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;
    if (FAILED(m_device->CreateCommandQueue(&queueDesc,
                                            IID_PPV_ARGS(&m_commandQueue))))
        return false;

    // Check tearing support before creating swap chain (affects flags)
    BOOL allowTearing = FALSE;
    m_factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                   &allowTearing, sizeof(allowTearing));
    m_swapChainAllowTearing = (allowTearing == TRUE);

    // Create swap chain (as IDXGISwapChain1, then QI to IDXGISwapChain3)
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = BUFFER_COUNT;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.Stereo = FALSE;
    if (m_swapChainAllowTearing)
        swapChainDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1;
    if (FAILED(m_factory->CreateSwapChainForHwnd(m_commandQueue.Get(), hwnd,
                                                 &swapChainDesc, nullptr,
                                                 nullptr, &swapChain1)))
        return false;

    // QI up to IDXGISwapChain3 for GetCurrentBackBufferIndex()
    if (FAILED(swapChain1.As(&m_swapChain)))
        return false;

    // Disable Alt+Enter fullscreen toggle when tearing is supported
    if (m_swapChainAllowTearing)
        m_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    m_swapChain->SetMaximumFrameLatency(BUFFER_COUNT);
    m_swapChainWaitableObject = m_swapChain->GetFrameLatencyWaitableObject();

    // Create RTV descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = BUFFER_COUNT;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    if (FAILED(m_device->CreateDescriptorHeap(&rtvHeapDesc,
                                              IID_PPV_ARGS(&m_rtvHeap))))
        return false;

    // Cache per-buffer RTV handles
    SIZE_T rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < BUFFER_COUNT; i++)
    {
        m_rtvDescriptors[i] = rtvHandle;
        rtvHandle.ptr += rtvDescriptorSize;
    }

    // Create SRV descriptor heap (shader-visible for ImGui rendering)
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = SRV_HEAP_SIZE;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    srvHeapDesc.NodeMask = 0;
    if (FAILED(m_device->CreateDescriptorHeap(&srvHeapDesc,
                                              IID_PPV_ARGS(&m_srvHeap))))
        return false;

    m_srvHeapAllocator.Create(m_device.Get(), m_srvHeap.Get(), SRV_HEAP_SIZE);

    // Create per-frame command allocators
    for (UINT i = 0; i < BUFFER_COUNT; i++)
        if (FAILED(m_device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&m_commandAllocators[i]))))
            return false;

    // Create command list (starts closed — BeginFrame will reset it)
    if (FAILED(m_device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[0].Get(),
            nullptr, IID_PPV_ARGS(&m_commandList))))
        return false;
    m_commandList->Close();

    // Create fence and event for CPU/GPU synchronisation
    if (FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                     IID_PPV_ARGS(&m_fence))))
        return false;

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == nullptr)
        return false;

    // Get back buffer resources and create RTVs
    for (UINT i = 0; i < BUFFER_COUNT; i++)
    {
        if (FAILED(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]))))
            return false;
        m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr,
                                         m_rtvDescriptors[i]);
    }

    return true;
}

auto D3D12Renderer::BeginFrame(HWND hwnd) -> bool
{
    // Skip rendering when occluded or minimized
    if ((m_swapChainOccluded &&
         m_swapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) ||
        IsIconic(hwnd))
    {
        Sleep(10);
        return false;
    }

    m_swapChainOccluded = false;

    // Wait until this frame's command allocator is safe to reuse
    UINT idx = m_swapChain->GetCurrentBackBufferIndex();
    if (m_fence->GetCompletedValue() < m_fenceValues[idx])
    {
        m_fence->SetEventOnCompletion(m_fenceValues[idx], m_fenceEvent);
        HANDLE waitables[] = {m_swapChainWaitableObject, m_fenceEvent};
        WaitForMultipleObjects(2, waitables, TRUE, INFINITE);
    }
    else
    {
        WaitForSingleObject(m_swapChainWaitableObject, INFINITE);
    }

    // Now safe to reset and record
    m_commandAllocators[idx]->Reset();
    m_commandList->Reset(m_commandAllocators[idx].Get(), nullptr);

    // Get back buffer
    ID3D12Resource* backBuffer = m_backBuffers[idx].Get();

    // Transition the back buffer to a render target
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        backBuffer, D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &barrier);

    // Clear the render target
    const float clearColor[] = {0.1f, 0.1f, 0.1f, 1.0f};
    m_commandList->ClearRenderTargetView(m_rtvDescriptors[idx], clearColor, 0,
                                         nullptr);

    // Set the render target and SRV heap
    m_commandList->OMSetRenderTargets(1, &m_rtvDescriptors[idx], FALSE,
                                      nullptr);
    // ImGui rendering expects a single SRV heap to be set on the command list
    ID3D12DescriptorHeap* descriptorHeaps[] = {m_srvHeap.Get()};
    m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps),
                                      descriptorHeaps);

    return true;
}

auto D3D12Renderer::EndFrame() -> void
{
    UINT idx = m_swapChain->GetCurrentBackBufferIndex();
    ID3D12Resource* backBuffer = m_backBuffers[idx].Get();

    // Transition back buffer from render target to present
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    m_commandList->ResourceBarrier(1, &barrier);

    // Close and execute the command list
    m_commandList->Close();
    ID3D12CommandList* commandLists[] = {m_commandList.Get()};
    m_commandQueue->ExecuteCommandLists(1, commandLists);

    // Present (vsync on, or tearing if supported)
    UINT syncInterval = m_swapChainAllowTearing ? 0 : 1;
    UINT presentFlags =
        m_swapChainAllowTearing ? DXGI_PRESENT_ALLOW_TEARING : 0;
    HRESULT hr = m_swapChain->Present(syncInterval, presentFlags);
    m_swapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);

    // Signal the fence *after* execute so the value represents this frame's
    // work
    UINT64 fenceValue = m_fenceValues[idx] + 1;
    m_commandQueue->Signal(m_fence.Get(), fenceValue);

    // Store the fence value on the *next* back buffer index — that's the one
    // BeginFrame will wait on before reusing its allocator
    UINT nextIdx = m_swapChain->GetCurrentBackBufferIndex();
    m_fenceValues[nextIdx] = fenceValue;
}

auto D3D12Renderer::WaitForGpu() -> void
{
    // Pick a value higher than anything we've signaled so far
    UINT64 waitValue = 0;
    for (UINT i = 0; i < BUFFER_COUNT; i++)
        waitValue = std::max(waitValue, m_fenceValues[i]);
    waitValue++;

    m_commandQueue->Signal(m_fence.Get(), waitValue);
    m_fence->SetEventOnCompletion(waitValue, m_fenceEvent);
    WaitForSingleObject(m_fenceEvent, INFINITE);

    // Sync all frame fence values so no frame thinks it's still in-flight
    for (UINT i = 0; i < BUFFER_COUNT; i++)
        m_fenceValues[i] = waitValue + 1;
}

auto D3D12Renderer::Shutdown() -> void
{
    WaitForGpu();

    // Close kernel handles that ComPtr doesn't manage
    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }
    if (m_swapChainWaitableObject)
    {
        CloseHandle(m_swapChainWaitableObject);
        m_swapChainWaitableObject = nullptr;
    }

    // ComPtr destructors release all D3D12/DXGI objects automatically
}

} // namespace Tattler
