#include "stdafx.h"

#include "hook/timestamp_manager.h"

#include <d3d12.h>
#include <dxgiformat.h>

#include <string.h>

namespace Tattler
{

auto TimestampManager::Init(ID3D12Device* device) -> bool
{
    D3D12_QUERY_HEAP_DESC queryHeapDesc{};
    queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    queryHeapDesc.Count = MAX_TIMESTAMPS;
    queryHeapDesc.NodeMask = 0;

    if (FAILED(device->CreateQueryHeap(&queryHeapDesc,
                                       IID_PPV_ARGS(&m_queryHeap))))
    {
        return false;
    }

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_READBACK;

    D3D12_RESOURCE_DESC bufferDesc{};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = MAX_TIMESTAMPS * sizeof(uint64_t);
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDesc.SampleDesc.Count = 1;

    if (FAILED(device->CreateCommittedResource(
            &heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
            IID_PPV_ARGS(&m_readbackBuffer))))
        return false;

    return true;
}

auto TimestampManager::Shutdown() -> void
{
    m_readbackBuffer.Reset();
    m_queryHeap.Reset();
}

auto TimestampManager::Reset() -> void
{
    m_nextSlot = 0;
}

auto TimestampManager::AllocatePair(UINT& outBeginSlot, UINT& outEndSlot)
    -> bool
{
    // Return false if full
    if (m_nextSlot + 2 > MAX_TIMESTAMPS)
        return false;

    outBeginSlot = m_nextSlot++;
    outEndSlot = m_nextSlot++;

    return true;
}

auto TimestampManager::InsertTimestamp(ID3D12GraphicsCommandList* cmdList,
                                       UINT slot) -> void
{
    cmdList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot);
}

auto TimestampManager::ResolveAll(ID3D12GraphicsCommandList* cmdList) -> void
{
    // Return early if no timestamps to resolve
    if (m_nextSlot == 0)
        return;

    cmdList->ResolveQueryData(m_queryHeap.Get(),          // Query heap
                              D3D12_QUERY_TYPE_TIMESTAMP, // Query type
                              0,                          // start slot
                              m_nextSlot, // number of slots to resolve
                              m_readbackBuffer.Get(), // destination buffer
                              0);                     // byte offset
}

auto TimestampManager::ReadResults() -> std::vector<uint64_t>
{
    // No results, return empty vector
    if (m_nextSlot == 0)
        return {};

    std::vector<uint64_t> results;

    // Get the range of the data: [start, end)
    D3D12_RANGE range = {0, m_nextSlot * sizeof(uint64_t)};
    void* data = nullptr;
    m_readbackBuffer->Map(0, &range, &data);

    results.resize(m_nextSlot);
    memcpy(results.data(), data, m_nextSlot * sizeof(uint64_t));

    D3D12_RANGE writeRange = {0, 0};
    m_readbackBuffer->Unmap(0, &writeRange);

    return results;
}

auto TimestampManager::GetFrequency(ID3D12CommandQueue* queue) -> uint64_t
{
    uint64_t frequency = 0;
    queue->GetTimestampFrequency(&frequency);

    return frequency;
}

} // namespace Tattler
