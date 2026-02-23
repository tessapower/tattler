#include "stdafx.h"

#include "common/capture_types.h"
#include "hook/hook_state.h"
#include "hook/swapchain_hooks.h"
#include "hook/vtable_hooks.h"
#include "hook/vtable_slots.h"

#include <d3d12.h>
#include <dxgi.h>
#include <dxgi1_4.h>

#include <cstring>
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

// Readback resources for backbuffer capture
static Microsoft::WRL::ComPtr<ID3D12CommandAllocator> s_readbackAllocator;
static Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> s_readbackCmdList;
static Microsoft::WRL::ComPtr<ID3D12Resource> s_readbackBuffer;
static UINT64 s_readbackBufferSize = 0;

//--------------------------------------------------------------- HELPERS --//

static void CaptureBackbuffer(IDXGISwapChain* pSwapChain)
{
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    if (FAILED(g_commandQueue->GetDevice(IID_PPV_ARGS(&device))))
        return;

    // Lazy-init readback command allocator + list
    if (!s_readbackAllocator)
    {
        if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                  IID_PPV_ARGS(&s_readbackAllocator))))
            return;
        if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                             s_readbackAllocator.Get(), nullptr,
                                             IID_PPV_ARGS(&s_readbackCmdList))))
            return;
        s_readbackCmdList->Close();
    }

    // Get the current backbuffer index (IDXGISwapChain3 preferred; fall back to 0)
    UINT idx = 0;
    {
        Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain3;
        if (SUCCEEDED(pSwapChain->QueryInterface(IID_PPV_ARGS(&swapChain3))))
            idx = swapChain3->GetCurrentBackBufferIndex();
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> backBuffer;
    if (FAILED(pSwapChain->GetBuffer(idx, IID_PPV_ARGS(&backBuffer))))
        return;

    D3D12_RESOURCE_DESC desc = backBuffer->GetDesc();

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT numRows = 0;
    UINT64 rowSizeBytes = 0;
    UINT64 totalBytes = 0;
    device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &numRows,
                                  &rowSizeBytes, &totalBytes);

    // (Re)allocate readback buffer if the required size has grown
    if (!s_readbackBuffer || s_readbackBufferSize < totalBytes)
    {
        s_readbackBuffer.Reset();

        D3D12_HEAP_PROPERTIES readbackHeap{};
        readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;

        D3D12_RESOURCE_DESC bufDesc{};
        bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Width = totalBytes;
        bufDesc.Height = 1;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.MipLevels = 1;
        bufDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufDesc.SampleDesc.Count = 1;
        bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        if (FAILED(device->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE,
                                                   &bufDesc, D3D12_RESOURCE_STATE_COPY_DEST,
                                                   nullptr, IID_PPV_ARGS(&s_readbackBuffer))))
            return;
        s_readbackBufferSize = totalBytes;
    }

    // Record: PRESENT → COPY_SOURCE, copy, COPY_SOURCE → PRESENT
    s_readbackAllocator->Reset();
    s_readbackCmdList->Reset(s_readbackAllocator.Get(), nullptr);

    D3D12_RESOURCE_BARRIER toCopy{};
    toCopy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toCopy.Transition.pResource = backBuffer.Get();
    toCopy.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    toCopy.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    toCopy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    s_readbackCmdList->ResourceBarrier(1, &toCopy);

    D3D12_TEXTURE_COPY_LOCATION srcLoc{};
    srcLoc.pResource = backBuffer.Get();
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    srcLoc.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION dstLoc{};
    dstLoc.pResource = s_readbackBuffer.Get();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dstLoc.PlacedFootprint = footprint;

    s_readbackCmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

    D3D12_RESOURCE_BARRIER toPresent = toCopy;
    std::swap(toPresent.Transition.StateBefore, toPresent.Transition.StateAfter);
    s_readbackCmdList->ResourceBarrier(1, &toPresent);

    s_readbackCmdList->Close();

    ID3D12CommandList* cmdLists[] = {s_readbackCmdList.Get()};
    g_commandQueue->ExecuteCommandLists(1, cmdLists);

    // Reuse the existing frame fence to wait for the copy
    ++s_fenceValue;
    g_commandQueue->Signal(s_fence.Get(), s_fenceValue);
    if (s_fence->GetCompletedValue() < s_fenceValue)
    {
        s_fence->SetEventOnCompletion(s_fenceValue, s_fenceEvent);
        WaitForSingleObject(s_fenceEvent, INFINITE);
    }

    // Map and copy pixels (strip the row padding from the readback buffer)
    void* mapped = nullptr;
    D3D12_RANGE readRange{0, static_cast<SIZE_T>(totalBytes)};
    if (FAILED(s_readbackBuffer->Map(0, &readRange, &mapped)))
        return;

    StagedTexture staged{};
    staged.sourceResource = reinterpret_cast<ResourceId>(backBuffer.Get());
    staged.width = static_cast<uint32_t>(desc.Width);
    staged.height = numRows;
    staged.format = desc.Format;
    staged.subresource = 0;

    const UINT srcRowPitch = footprint.Footprint.RowPitch;
    const UINT dstRowBytes = static_cast<UINT>(rowSizeBytes);
    staged.pixels.resize(static_cast<size_t>(numRows) * dstRowBytes);
    const auto* src = static_cast<const uint8_t*>(mapped);
    for (UINT row = 0; row < numRows; ++row)
        std::memcpy(staged.pixels.data() + row * dstRowBytes, src + row * srcRowPitch, dstRowBytes);

    D3D12_RANGE noWrite{0, 0};
    s_readbackBuffer->Unmap(0, &noWrite);

    g_captureController.AddTexture(std::move(staged));
}

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
        if (s_fenceEvent && s_fence->GetCompletedValue() < s_fenceValue)
        {
            s_fence->SetEventOnCompletion(s_fenceValue, s_fenceEvent);
            WaitForSingleObject(s_fenceEvent, INFINITE);
        }

        // Read resolved timestamps and GPU tick frequency
        auto results = g_timestampManager.ReadResults();
        uint64_t frequency = g_timestampManager.GetFrequency(g_commandQueue);

        // Patch slot indices → real ticks and ship to viewer
        g_captureController.EndFrame(results, frequency);

        // Capture the current backbuffer for texture preview (once per capture)
        if (g_captureController.IsCapturing())
            CaptureBackbuffer(pThis);

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
