#pragma once

#include <d3d12.h>

#include <cstdint>
#include <vector>
#include <wrl/client.h>

namespace Tattler
{

/// NOTE TO SELF:
// GPU timestamps are written by inserting EndQuery calls around each hooked
// command (begin + end = one pair per event). After ExecuteCommandLists,
// ResolveAll() copies the raw ticks into a readback buffer. After the GPU
// finishes (signalled by a fence), ReadResults() maps the readback buffer
// and returns the resolved tick values.

/// <summary>
/// Manages a D3D12 timestamp query heap and its associated readback buffer.
/// </summary>
class TimestampManager
{
  public:
    static constexpr UINT MAX_TIMESTAMPS = 2048; // 1024 events max per frame

    /// <summary>
    /// Creates the query heap and readback buffer on the given device. Call
    /// once after the D3D12 device is available.
    /// </summary>
    auto Init(ID3D12Device* device) -> bool;

    /// <summary>
    /// Releases the query heap and readback buffer.
    /// </summary>
    auto Shutdown() -> void;

    /// <summary>
    /// Resets the slot counter. Call at the start of each new frame before
    /// recording any events.
    /// </summary>
    auto Reset() -> void;

    /// <summary>
    /// Allocates two consecutive timestamp slots (begin, end) for one event.
    /// </summary>
    /// <param name="outBeginSlot">Receives the begin slot index.</param>
    /// <param name="outEndSlot">Receives the end slot index.</param>
    /// <returns>False if the heap is full.</returns>
    auto AllocatePair(UINT& outBeginSlot, UINT& outEndSlot) -> bool;

    /// <summary>
    /// Records a timestamp into the given slot on the command list.
    /// Call this immediately before and after the hooked command.
    /// Uses D3D12_QUERY_TYPE_TIMESTAMP and EndQuery.
    /// </summary>
    auto InsertTimestamp(ID3D12GraphicsCommandList* cmdList, UINT slot) -> void;

    /// <summary>
    /// Resolves all allocated timestamp slots into the readback buffer.
    /// Call once after all commands for the frame have been recorded,
    /// just before ExecuteCommandLists.
    /// Uses ResolveQueryData.
    /// </summary>
    auto ResolveAll(ID3D12GraphicsCommandList* cmdList) -> void;

    /// <summary>
    /// Maps the readback buffer and returns all timestamp values as raw ticks.
    /// The index into the returned vector corresponds to the slot index.
    /// Call only after the GPU has finished executing (fence wait).
    /// </summary>
    auto ReadResults() -> std::vector<uint64_t>;

    /// <summary>
    /// Queries the GPU timestamp frequency (ticks per second) from the
    /// command queue. Used to convert raw ticks to milliseconds.
    /// </summary>
    auto GetFrequency(ID3D12CommandQueue* queue) -> uint64_t;

  private:
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_queryHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_readbackBuffer;
    UINT m_nextSlot = 0;
};

} // namespace Tattler
