#pragma once

#include <dxgi.h>

namespace Tattler
{

/// <summary>
/// Patches the vtable of a DXGI swap chain to intercept Present, which is the
/// end-of-frame boundary. Safe to call multiple times. Skips patching if
/// already hooked.
/// </summary>
auto InstallSwapChainHooks(IDXGISwapChain* swapChain) -> void;

} // namespace Tattler
