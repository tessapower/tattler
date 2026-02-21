#include "stdafx.h"

#include "common/capture_types.h"
#include "hook/cmdlist_hooks.h"
#include "hook/hook_state.h"
#include "hook/vtable_hooks.h"
#include "hook/vtable_slots.h"

#include <d3d12.h>

#include <unordered_set>

namespace Tattler
{
/// NOTE TO SELF:
// Store the vtable pointer of every command list we have already patched.
// Should be checked before writing any slots!!

static std::unordered_set<void**> s_hookedVTables;

// -----------------------------------------------------ORIGINAL FUNCTIONS --//
using PFN_Close = HRESULT(WINAPI*)(ID3D12GraphicsCommandList*);

using PFN_DrawInstanced = void(WINAPI*)(ID3D12GraphicsCommandList*, UINT, UINT,
                                        UINT, UINT);

using PFN_DrawIndexedInstanced = void(WINAPI*)(ID3D12GraphicsCommandList*, UINT,
                                               UINT, UINT, INT, UINT);

using PFN_Dispatch = void(WINAPI*)(ID3D12GraphicsCommandList*, UINT, UINT,
                                   UINT);

using PFN_CopyResource = void(WINAPI*)(ID3D12GraphicsCommandList*,
                                       ID3D12Resource*, ID3D12Resource*);

using PFN_ResourceBarrier = void(WINAPI*)(ID3D12GraphicsCommandList*, UINT,
                                          const D3D12_RESOURCE_BARRIER*);

using PFN_ClearRenderTargetView = void(WINAPI*)(ID3D12GraphicsCommandList*,
                                                D3D12_CPU_DESCRIPTOR_HANDLE,
                                                const FLOAT[4], UINT,
                                                const D3D12_RECT*);

using PFN_ClearDepthStencilView = void(WINAPI*)(ID3D12GraphicsCommandList*,
                                                D3D12_CPU_DESCRIPTOR_HANDLE,
                                                D3D12_CLEAR_FLAGS, FLOAT, UINT8,
                                                UINT, const D3D12_RECT*);

static PFN_Close s_origClose = nullptr;
static PFN_DrawInstanced s_origDrawInstanced = nullptr;
static PFN_DrawIndexedInstanced s_origDrawIndexedInstanced = nullptr;
static PFN_Dispatch s_origDispatch = nullptr;
static PFN_CopyResource s_origCopyResource = nullptr;
static PFN_ResourceBarrier s_origResourceBarrier = nullptr;
static PFN_ClearRenderTargetView s_origClearRenderTargetView = nullptr;
static PFN_ClearDepthStencilView s_origClearDepthStencilView = nullptr;

//---------------------------------------------------------------- HELPERS --//

/// <summary>
/// Allocates a timestamp pair, inserts the begin timestamp, calls the
/// original function, inserts the end timestamp, then submits the event
/// to the capture buffer.
/// </summary>
/// <param name="cmdList">The command list being recorded on.</param>
/// <param name="type">The EventType to tag this event with.</param>
/// <param name="params">The EventParams variant for this event.</param>
/// <param name="record">Lambda that calls the original function: [&]() { ...
/// }</param>
template <typename Fn>
static void RecordEvent(ID3D12GraphicsCommandList* cmdList, EventType type,
                        EventParams params, Fn&& record)
{
    // If we're not capturing just call the original function and return
    if (!g_captureController.IsCapturing())
    {
        record();

        return;
    }

    // Allocate timestamp pair
    UINT beginSlot, endSlot;
    // If the heap is full, just call the original function and return
    if (!g_timestampManager.AllocatePair(beginSlot, endSlot))
    {
        record();

        return;
    }

    // Insert begin timestamp
    g_timestampManager.InsertTimestamp(cmdList, beginSlot);

    // Call original function
    record();

    // Insert end timestamp
    g_timestampManager.InsertTimestamp(cmdList, endSlot);

    CapturedEvent event{};
    event.type = type;
    event.params = params;
    event.timestampBegin = beginSlot; // patched later with real ticks
    event.timestampEnd = endSlot;
    /// NOTE TO SELF:
    // frameIndex/eventIndex will be filled in by the capture controller!

    g_captureController.SubmitEvent(event);
}

//------------------------------------------------------------------ HOOKS --//

static HRESULT WINAPI HookedClose(ID3D12GraphicsCommandList* pThis)
{
    if (g_captureController.IsCapturing())
        g_timestampManager.ResolveAll(pThis);

    return s_origClose(pThis);
}

static void WINAPI HookedDrawInstanced(ID3D12GraphicsCommandList* pThis,
                                       UINT VertexCountPerInstance,
                                       UINT InstanceCount,
                                       UINT StartVertexLocation,
                                       UINT StartInstanceLocation)
{
    RecordEvent(pThis, EventType::Draw,
                DrawParams{VertexCountPerInstance, InstanceCount},
                [&]()
                {
                    s_origDrawInstanced(pThis, VertexCountPerInstance,
                                        InstanceCount, StartVertexLocation,
                                        StartInstanceLocation);
                });
}

static void WINAPI HookedDrawIndexedInstanced(ID3D12GraphicsCommandList* pThis,
                                              UINT IndexCountPerInstance,
                                              UINT InstanceCount,
                                              UINT StartIndexLocation,
                                              INT BaseVertexLocation,
                                              UINT StartInstanceLocation)
{
    RecordEvent(pThis, EventType::DrawIndexed,
                DrawIndexedParams{IndexCountPerInstance, InstanceCount},
                [&]()
                {
                    s_origDrawIndexedInstanced(
                        pThis, IndexCountPerInstance, InstanceCount,
                        StartIndexLocation, BaseVertexLocation,
                        StartInstanceLocation);
                });
}

static void WINAPI HookedDispatch(ID3D12GraphicsCommandList* pThis,
                                  UINT ThreadGroupCountX,
                                  UINT ThreadGroupCountY,
                                  UINT ThreadGroupCountZ)
{
    RecordEvent(
        pThis, EventType::Dispatch,
        DispatchParams{ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ},
        [&]()
        {
            s_origDispatch(pThis, ThreadGroupCountX, ThreadGroupCountY,
                           ThreadGroupCountZ);
        });
}

static void WINAPI HookedCopyResource(ID3D12GraphicsCommandList* pThis,
                                      ID3D12Resource* pDstResource,
                                      ID3D12Resource* pSrcResource)
{
    RecordEvent(pThis, EventType::CopyResource,
                CopyParams{(ResourceId)pDstResource, (ResourceId)pSrcResource},
                [&]()
                { s_origCopyResource(pThis, pDstResource, pSrcResource); });
}

static void WINAPI
HookedResourceBarrier(ID3D12GraphicsCommandList* pThis, UINT NumBarriers,
                      const D3D12_RESOURCE_BARRIER* pBarriers)
{
    RecordEvent(pThis, EventType::ResourceBarrier,
                /// NOTE TO SELF:
                // This section of code only handles the most likely case of
                // transition barriers. Only transition barriers have
                // before/after states, we record index 0 as a representative
                // for the batch. UAV/aliasing barriers will show garbage
                // states.
                BarrierParams{pBarriers[0].Transition.StateBefore,
                              pBarriers[0].Transition.StateAfter,
                              (ResourceId)pBarriers[0].Transition.pResource},
                [&]()
                { s_origResourceBarrier(pThis, NumBarriers, pBarriers); });
}

static void WINAPI HookedClearRenderTargetView(
    ID3D12GraphicsCommandList* pThis,
    D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4],
    UINT NumRects, const D3D12_RECT* pRects)
{
    RecordEvent(pThis, EventType::ClearRTV,
                ClearRtvParams{
                    ResourceId(RenderTargetView.ptr),
                    {ColorRGBA[0], ColorRGBA[1], ColorRGBA[2], ColorRGBA[3]}},
                [&]()
                {
                    s_origClearRenderTargetView(pThis, RenderTargetView,
                                                ColorRGBA, NumRects, pRects);
                });
}

static void WINAPI HookedClearDepthStencilView(
    ID3D12GraphicsCommandList* pThis,
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags,
    FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT* pRects)
{
    RecordEvent(pThis, EventType::ClearDSV,
                ClearDsvParams{ResourceId(DepthStencilView.ptr), Depth, Stencil,
                               ClearFlags},
                [&]()
                {
                    s_origClearDepthStencilView(pThis, DepthStencilView,
                                                ClearFlags, Depth, Stencil,
                                                NumRects, pRects);
                });
}

//---------------------------------------------------------------- INSTALL --//

auto InstallCommandListHooks(ID3D12GraphicsCommandList* cmdList) -> void
{
    // 1. Get the vtable:
    void** vtable = VTableHooks::GetVTable(cmdList);

    // If already hooked, return early
    if (s_hookedVTables.contains(vtable))
        return;

    s_hookedVTables.insert(vtable);

    // Only assign if we haven't already
    if (!s_origClose)
    {
        s_origClose = VTableHooks::HookVTableEntry<PFN_Close>(
            vtable, VTableSlots::CmdList::Close, HookedClose);
    }

    if (!s_origDrawInstanced)
    {
        s_origDrawInstanced = VTableHooks::HookVTableEntry<PFN_DrawInstanced>(
            vtable, VTableSlots::CmdList::DrawInstanced, HookedDrawInstanced);
    }

    if (!s_origDrawIndexedInstanced)
    {
        s_origDrawIndexedInstanced =
            VTableHooks::HookVTableEntry<PFN_DrawIndexedInstanced>(
                vtable, VTableSlots::CmdList::DrawIndexedInstanced,
                HookedDrawIndexedInstanced);
    }

    if (!s_origDispatch)
    {
        s_origDispatch = VTableHooks::HookVTableEntry<PFN_Dispatch>(
            vtable, VTableSlots::CmdList::Dispatch, HookedDispatch);
    }

    if (!s_origCopyResource)
    {
        s_origCopyResource = VTableHooks::HookVTableEntry<PFN_CopyResource>(
            vtable, VTableSlots::CmdList::CopyResource, HookedCopyResource);
    }

    if (!s_origResourceBarrier)
    {
        s_origResourceBarrier =
            VTableHooks::HookVTableEntry<PFN_ResourceBarrier>(
                vtable, VTableSlots::CmdList::ResourceBarrier,
                HookedResourceBarrier);
    }

    if (!s_origClearRenderTargetView)
    {
        s_origClearRenderTargetView =
            VTableHooks::HookVTableEntry<PFN_ClearRenderTargetView>(
                vtable, VTableSlots::CmdList::ClearRenderTargetView,
                HookedClearRenderTargetView);
    }

    if (!s_origClearDepthStencilView)
    {
        s_origClearDepthStencilView =
            VTableHooks::HookVTableEntry<PFN_ClearDepthStencilView>(
                vtable, VTableSlots::CmdList::ClearDepthStencilView,
                HookedClearDepthStencilView);
    }
}

} // namespace Tattler
