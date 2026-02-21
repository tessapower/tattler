#include "stdafx.h"

#include "hook/hook_state.h"
#include "hook/queue_hooks.h"
#include "hook/vtable_hooks.h"
#include "hook/vtable_slots.h"

#include <d3d12.h>

#include <unordered_set>

namespace Tattler
{
// Tracks hooked vtables
static std::unordered_set<void**> s_hookedVTables;

using PFN_ExecuteCommandLists = void(WINAPI*)(ID3D12CommandQueue*, UINT,
                                              ID3D12CommandList* const*);

static PFN_ExecuteCommandLists s_origExecuteCommandLists = nullptr;

//------------------------------------------------------------------- HOOK --//

static void WINAPI
HookedExecuteCommandLists(ID3D12CommandQueue* pThis, UINT NumCommandLists,
                          ID3D12CommandList* const* ppCommandLists)
{
    g_commandQueue = pThis;

    s_origExecuteCommandLists(pThis, NumCommandLists, ppCommandLists);
}

//---------------------------------------------------------------- INSTALL --//

auto InstallCommandQueueHooks(ID3D12CommandQueue* queue) -> void
{
    void** vtable = VTableHooks::GetVTable(queue);

    // If already hooked, return early
    if (s_hookedVTables.contains(vtable))
        return;

    s_hookedVTables.insert(vtable);

    // Only assign if we haven't already
    if (!s_origExecuteCommandLists)
    {
        s_origExecuteCommandLists =
            VTableHooks::HookVTableEntry<PFN_ExecuteCommandLists>(
                vtable, VTableSlots::CmdQueue::ExecuteCommandLists,
                HookedExecuteCommandLists);
    }
}

} // namespace Tattler
