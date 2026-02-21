#pragma once

#include <d3d12.h>

namespace Tattler
{

/// <summary>
/// Patches the vtable of a command queue to intercept ExecuteCommandLists
/// which is where we resolve GPU timestamps. Right before submission we
/// record a ResolveQueryData call so the GPU writes all timestamps to the
/// readback buffer in the same submission.
///
/// Safe to call multiple times. Skips patching if already hooked.
/// </summary>
auto InstallCommandQueueHooks(ID3D12CommandQueue* queue) -> void;

} // namespace Tattler
