#pragma once

#include "viewer/srv_descriptor_allocator.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#include <wrl.h>

namespace tattler
{

class D3D12Renderer
{
  public:
    static constexpr UINT BUFFER_COUNT = 2;

    D3D12Renderer() = default;
    ~D3D12Renderer() = default;

    D3D12Renderer(const D3D12Renderer&) = delete;
    D3D12Renderer& operator=(const D3D12Renderer&) = delete;

    /// <summary>
    /// Create the DXGI factory, adapter, D3D12 device, command queue,
    /// swap chain, descriptor heaps, command allocators, command list, and
    /// fence.
    /// </summary>
    auto Init(HWND hwnd, UINT width, UINT height) -> bool;

    /// <summary>
    /// Wait for the previous frame, reset the command allocator and list,
    /// transition the back buffer to a render target, and clear it.
    /// The command list is left open for ImGui and panel draw calls.
    /// </summary>
    auto BeginFrame(HWND hwnd) -> bool;

    /// <summary>
    /// Transition the back buffer to present state, close and execute the
    /// command list, then call Present() on the swap chain.
    /// </summary>
    auto EndFrame() -> void;

    /// <summary>
    /// Signal the fence and block until the GPU has finished all queued work.
    /// Used at shutdown and before resource destruction.
    /// </summary>
    auto WaitForGpu() -> void;

    /// <summary>
    /// Drain the GPU queue and release all D3D12/DXGI resources.
    /// </summary>
    auto Shutdown() -> void;

    auto GetDevice() const -> ID3D12Device*
    {
        return m_device.Get();
    }

    auto GetCommandQueue() const -> ID3D12CommandQueue*
    {
        return m_commandQueue.Get();
    }

    auto GetSrvHeap() const -> ID3D12DescriptorHeap*
    {
        return m_srvHeap.Get();
    }

    // Non-const: ImGui writes through this pointer during alloc/free callbacks
    auto GetSrvHeapAllocator() -> SrvDescriptorAllocator*
    {
        return &m_srvHeapAllocator;
    }

    auto GetCommandList() const -> ID3D12GraphicsCommandList*
    {
        return m_commandList.Get();
    }

  private:
    // Core DXGI/D3D12 objects
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<IDXGIFactory6> m_factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> m_adapter;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;

    // Descriptor heaps
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    SrvDescriptorAllocator m_srvHeapAllocator;
    static const int SRV_HEAP_SIZE = 64;

    // Per-frame command recording
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>
        m_commandAllocators[BUFFER_COUNT];

    // Back buffer resources and their RTV handles
    Microsoft::WRL::ComPtr<ID3D12Resource> m_backBuffers[BUFFER_COUNT];
    D3D12_CPU_DESCRIPTOR_HANDLE m_rtvDescriptors[BUFFER_COUNT] = {};

    // CPU/GPU synchronisation
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    HANDLE m_fenceEvent = nullptr;
    UINT64 m_fenceValues[BUFFER_COUNT] = {};

    // Lets us wait for the swap chain to be ready before rendering
    HANDLE m_swapChainWaitableObject = nullptr;
    // Avoid rendering when the window is occluded
    bool m_swapChainOccluded = false;
    bool m_swapChainAllowTearing = false;
};

} // namespace tattler
