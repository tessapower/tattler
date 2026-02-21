#pragma once

/// <summary>
/// Vtable slot indices for the D3D12 and DXGI COM interfaces we hook.
///
/// COM vtables lay out methods in declaration order, starting with the
/// inherited IUnknown methods (counting starts at 0).
///
/// To verify a slot, open d3d12.h/dxgi.h and count from IUnknown downward
/// through each interface in the inheritance chain.
/// </summary>

namespace VTableSlots
{

/// ID3D12Device vtable slots.
/// Inheritance chain:
///   IUnknown     (0–2)
///   ID3D12Object (3–6)
///   ID3D12Device (7+)
namespace Device
{
inline constexpr int CreateCommandQueue = 8;
inline constexpr int CreateCommandList = 12;
} // namespace Device

/// DXGIFactory2 vtable slots.
/// Inheritance chain (slot 0 at top):
///   IUnknown               (0–2)
///   IDXGIObject            (3–6)
///   IDXGIFactory           (7–11)
///   IDXGIFactory1          (12–13)
///   IDXGIFactory2          (14+)
///   CreateSwapChainForHwnd (15)
namespace DXGIFactory2
{
inline constexpr int CreateSwapChainForHwnd = 15;
}

/// ID3D12GraphicsCommandList vtable slots.
/// Inheritance chain (slot 0 at top):
///   IUnknown                  (0–2)
///   ID3D12Object              (3–6)
///   ID3D12DeviceChild         (7)
///   ID3D12CommandList         (8)
///   ID3D12GraphicsCommandList (9+)
namespace CmdList
{
inline constexpr int Close = 9;
inline constexpr int DrawInstanced = 12;
inline constexpr int DrawIndexedInstanced = 13;
inline constexpr int Dispatch = 14;
inline constexpr int CopyResource = 17;
inline constexpr int ResourceBarrier = 26;
inline constexpr int ClearDepthStencilView = 47;
inline constexpr int ClearRenderTargetView = 48;
} // namespace CmdList

/// ID3D12CommandQueue vtable slots.
/// Inheritance chain:
///   IUnknown           (0–2)
///   ID3D12Object       (3–6)
///   ID3D12DeviceChild  (7)
///   ID3D12CommandQueue (8+)
namespace CmdQueue
{
inline constexpr int ExecuteCommandLists = 10;
}

/// IDXGISwapChain vtable slots.
/// Inheritance chain:
///   IUnknown               (0–2)
///   IDXGIObject            (3–6)
///   IDXGIDeviceSubObject   (7)
///   IDXGISwapChain         (8+)
namespace SwapChain
{
inline constexpr int Present = 8;
}

} // namespace VTableSlots
