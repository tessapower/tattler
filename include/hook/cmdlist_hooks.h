#pragma once

#include <d3d12.h>

namespace Tattler
{

/// <summary>
/// Patches the vtable of a command list to intercept draw, dispatch, barrier,
/// clear, and copy commands.
///
/// Safe to call multiple times on different instances. Skips patching if the
/// vtable has already been hooked.
/// </summary>
auto InstallCommandListHooks(ID3D12GraphicsCommandList* cmdList) -> void;

} // namespace Tattler
