#include "stdafx.h"

#include "hook/hook_state.h"
#include "hook/swapchain_hooks.h"
#include "hook/vtable_hooks.h"
#include "hook/vtable_slots.h"

#include <d3d12.h>
#include <dxgi.h>

#include <unordered_set>
#include <wrl/client.h>

namespace Tattler
{
// Tracks hooked vtables
static std::unordered_set<void**> s_hookedVTables;

using PFN_Present = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);

static PFN_Present s_origPresent = nullptr;

static Microsoft::WRL::ComPtr<ID3D12Fence> s_fence;
static HANDLE s_fenceEvent = nullptr;
static uint64_t s_fenceValue = 0;

//------------------------------------------------------------------- HOOK --//

static HRESULT WINAPI HookedPresent(IDXGISwapChain* pThis, UINT SyncInterval,
                                    UINT Flags)
{
    if (g_captureController.IsCapturing() && g_commandQueue)
    {
        // Lazy-initialize the fence on the first captured frame
        if (!s_fence)
        {
            Microsoft::WRL::ComPtr<ID3D12Device> device;
            g_commandQueue->GetDevice(IID_PPV_ARGS(&device));
            device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                IID_PPV_ARGS(&s_fence));

            s_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);

            if (!s_fenceEvent)
                // Keep the two in sync, skip capture this frame
                s_fence.Reset();
        }

        if (!s_fence)
        {
            return s_origPresent(pThis, SyncInterval, Flags);
        }

        // Signal the fence after all work submitted this frame
        ++s_fenceValue;
        g_commandQueue->Signal(s_fence.Get(), s_fenceValue);

        // Block until the GPU has finished (ensures ResolveQueryData is done)
        if (s_fence->GetCompletedValue() < s_fenceValue)
        {
            s_fence->SetEventOnCompletion(s_fenceValue, s_fenceEvent);
            WaitForSingleObject(s_fenceEvent, INFINITE);
        }

        // Read resolved timestamps and GPU tick frequency
        auto results = g_timestampManager.ReadResults();
        uint64_t frequency = g_timestampManager.GetFrequency(g_commandQueue);

        // Patch slot indices → real ticks and ship to viewer
        g_captureController.EndFrame(results, frequency);

        // Reset timestamp slots for next frame
        g_timestampManager.Reset();
    }

    return s_origPresent(pThis, SyncInterval, Flags);
}

//---------------------------------------------------------------- INSTALL --//

auto InstallSwapChainHooks(IDXGISwapChain* swapChain) -> void
{
    void** vtable = VTableHooks::GetVTable(swapChain);

    // If already hooked, return early
    if (s_hookedVTables.contains(vtable))
        return;

    s_hookedVTables.insert(vtable);

    // Only assign if we haven't already
    if (!s_origPresent)
    {
        s_origPresent = VTableHooks::HookVTableEntry<PFN_Present>(
            vtable, VTableSlots::SwapChain::Present, HookedPresent);
    }
}

} // namespace Tattler
