#include "stdafx.h"

#include "hook/cmdlist_hooks.h"
#include "hook/device_hooks.h"
#include "hook/hook_state.h"
#include "hook/queue_hooks.h"
#include "hook/swapchain_hooks.h"
#include "hook/vtable_hooks.h"
#include "hook/vtable_slots.h"

#include <d3d12.h>
#include <d3dcommon.h>
#include <dxgi.h>
#include <dxgi1_2.h>

#include <detours/detours.h>

namespace Tattler
{

//------------------------------------------------------------ DLL EXPORTS --//
// Detours requires these to be initialised to the real function address.
// They are updated by DetourAttach to point to the trampoline.
static auto(WINAPI* s_origD3D12CreateDevice)(
    IUnknown* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid,
    void** ppDevice) = D3D12CreateDevice;

static auto(WINAPI* s_origCreateDXGIFactory2)(
    UINT Flags, REFIID riid, void** ppFactory) = CreateDXGIFactory2;

//------------------------------------------------------ ORIGINAL POINTERS --//

// Vtable hook function pointer types saved by HookVTableEntry, used as
// trampolines.
using PFN_CreateCommandQueue =
    HRESULT(WINAPI*)(ID3D12Device* pThis, const D3D12_COMMAND_QUEUE_DESC* pDesc,
                     REFIID riid, void** ppCommandQueue);

using PFN_CreateCommandList = HRESULT(WINAPI*)(
    ID3D12Device* pThis, UINT nodeMask, D3D12_COMMAND_LIST_TYPE type,
    ID3D12CommandAllocator* pCommandAllocator,
    ID3D12PipelineState* pInitialState, REFIID riid, void** ppCommandList);

using PFN_CreateSwapChainForHwnd = HRESULT(WINAPI*)(
    IDXGIFactory2* pThis, IUnknown* pDevice, HWND hWnd,
    const DXGI_SWAP_CHAIN_DESC1* pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
    IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain);

static PFN_CreateCommandQueue s_origCreateCommandQueue = nullptr;
static PFN_CreateCommandList s_origCreateCommandList = nullptr;
static PFN_CreateSwapChainForHwnd s_origCreateSwapChainForHwnd = nullptr;

//------------------------------------------------------------------ HOOKS --//

/// <summary>
/// Called whenever the game creates a queue. Creates the queue normally,
/// then hooks its vtable.
/// </summary>
static HRESULT WINAPI HookedCreateCommandQueue(
    ID3D12Device* pThis, const D3D12_COMMAND_QUEUE_DESC* pDesc, REFIID riid,
    void** ppCommandQueue)
{
    HRESULT hr = s_origCreateCommandQueue(pThis, pDesc, riid, ppCommandQueue);

    // Return early if we failed to create the command queue
    if (FAILED(hr) || !ppCommandQueue)
        return hr;

    // Install all the command queue hooks!
    InstallCommandQueueHooks(static_cast<ID3D12CommandQueue*>(*ppCommandQueue));

    return hr;
}

/// <summary>
/// Called whenever the game creates a command list. Creates the command list
/// normally, then hooks its vtable.
/// </summary>
static HRESULT WINAPI HookedCreateCommandList(
    ID3D12Device* pThis, UINT nodeMask, D3D12_COMMAND_LIST_TYPE type,
    ID3D12CommandAllocator* pCommandAllocator,
    ID3D12PipelineState* pInitialState, REFIID riid, void** ppCommandList)
{
    HRESULT hr =
        s_origCreateCommandList(pThis, nodeMask, type, pCommandAllocator,
                                pInitialState, riid, ppCommandList);

    // Return early if we failed to create the command list
    if (FAILED(hr) || !ppCommandList)
        return hr;

    // Install the command list hooks!
    InstallCommandListHooks(
        static_cast<ID3D12GraphicsCommandList*>(*ppCommandList));

    return hr;
}

/// <summary>
/// Creates the device normally, then hooks the device's vtable to intercept
/// CreateCommandQueue and CreateCommandList so we can hook future objects.
/// </summary>
static HRESULT WINAPI HookedD3D12CreateDevice(
    IUnknown* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid,
    void** ppDevice)
{
    // Call s_origD3D12CreateDevice with same arguments
    HRESULT hr =
        s_origD3D12CreateDevice(pAdapter, MinimumFeatureLevel, riid, ppDevice);

    // Return early if we failed to create the device
    if (FAILED(hr) || !ppDevice)
        return hr;

    ID3D12Device* device = static_cast<ID3D12Device*>(*ppDevice);

    // Init timestamp manager with device
    g_timestampManager.Init(device);

    // Get the device's vtable
    void** vtable = VTableHooks::GetVTable(device);

    // Hook CreateCommandQueue
    s_origCreateCommandQueue =
        VTableHooks::HookVTableEntry<PFN_CreateCommandQueue>(
            vtable, VTableSlots::Device::CreateCommandQueue,
            HookedCreateCommandQueue);

    // Hook CreateCommandList
    s_origCreateCommandList =
        VTableHooks::HookVTableEntry<PFN_CreateCommandList>(
            vtable, VTableSlots::Device::CreateCommandList,
            HookedCreateCommandList);

    return hr;
}

/// <summary>
/// Hooked CreateSwapChainForHwnd — called when the game creates a swap chain.
/// Creates the swap chain normally, then hooks its Present method.
/// </summary>
static HRESULT WINAPI HookedCreateSwapChainForHwnd(
    IDXGIFactory2* pThis, IUnknown* pDevice, HWND hWnd,
    const DXGI_SWAP_CHAIN_DESC1* pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
    IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain)
{
    HRESULT hr = s_origCreateSwapChainForHwnd(pThis, pDevice, hWnd, pDesc,
                                              pFullscreenDesc,
                                              pRestrictToOutput, ppSwapChain);

    // Return early if we couldn't create a swapchain for hwnd
    if (FAILED(hr) || !ppSwapChain)
        return hr;

    InstallSwapChainHooks(*ppSwapChain);

    return hr;
}

/// <summary>
/// Hooked CreateDXGIFactory2 — called instead of the real one.
/// Creates the factory normally, then hooks CreateSwapChainForHwnd so we
/// can intercept the swap chain as soon as it is created.
/// </summary>
static HRESULT WINAPI HookedCreateDXGIFactory2(UINT Flags, REFIID riid,
                                               void** ppFactory)
{
    HRESULT hr = s_origCreateDXGIFactory2(Flags, riid, ppFactory);

    // Return early if we failed to create the factory
    if (FAILED(hr) || !ppFactory)
        return hr;

    IDXGIFactory2* factory2 = nullptr;
    // QI for IDXGIFactory2 so we can hook CreateSwapChainForHwnd on it
    if (FAILED(static_cast<IUnknown*>(*ppFactory)
                   ->QueryInterface(IID_PPV_ARGS(&factory2))))
        return hr;

    // Get the factory's vtable
    void** vtable = VTableHooks::GetVTable(factory2);

    // Hook CreateSwapChainForHwnd
    s_origCreateSwapChainForHwnd =
        VTableHooks::HookVTableEntry<PFN_CreateSwapChainForHwnd>(
            vtable, VTableSlots::DXGIFactory2::CreateSwapChainForHwnd,
            HookedCreateSwapChainForHwnd);

    factory2->Release();

    return hr;
}

//--------------------------------------------------------- INSTALL/REMOVE --//

auto InstallInitialHooks() -> void
{
    DetourTransactionBegin();

    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)s_origD3D12CreateDevice, HookedD3D12CreateDevice);
    DetourAttach(&(PVOID&)s_origCreateDXGIFactory2, HookedCreateDXGIFactory2);

    DetourTransactionCommit();
}

auto UninstallInitialHooks() -> void
{
    DetourTransactionBegin();

    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)s_origD3D12CreateDevice, HookedD3D12CreateDevice);
    DetourDetach(&(PVOID&)s_origCreateDXGIFactory2, HookedCreateDXGIFactory2);

    DetourTransactionCommit();
}

} // namespace Tattler
