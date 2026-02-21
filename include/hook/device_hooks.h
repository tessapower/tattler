#pragma once

/// <summary>
/// Hooks for D3D12 device and factory creation functions.
///
/// This is the entry point of the entire hook chain. We hook D3D12CreateDevice
/// so we can intercept the device as soon as it is created. From the device we
/// hook CreateCommandQueue and CreateCommandList, which in turn lets us hook
/// the vtables of every queue and command list the game creates.
///
/// The swap chain is created through DXGI, so we also hook CreateDXGIFactory2
/// (and its variants) to intercept the factory, then hook
/// CreateSwapChainForHwnd (or CreateSwapChain) on the resulting factory.
/// </summary>
namespace Tattler
{

/// <summary>
/// Installs Detours hooks on D3D12CreateDevice and CreateDXGIFactory2 Call
/// once from the background thread before entering the message loop.
/// </summary>
auto InstallInitialHooks() -> void;

/// <summary>
/// Removes the Detours hooks installed by InstallInitialHooks(). Call from
/// DLL_PROCESS_DETACH to restore original behaviour.
/// </summary>
auto UninstallInitialHooks() -> void;

} // namespace Tattler
