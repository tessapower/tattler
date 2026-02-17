#pragma once

#include <d3d12.h>

#include <cassert>
#include <vector>

namespace tattler
{
struct SrvDescriptorAllocator
{
    ID3D12DescriptorHeap* m_heap = nullptr;
    D3D12_DESCRIPTOR_HEAP_TYPE m_type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart = {};
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart = {};
    UINT m_incrementSize = 0;
    ImVector<int> m_freeIndices;

    // Snapshot the heap's base handles and build a free-list of every slot.
    // Slots are pushed in descending order so that index 0 is the first
    // allocated, keeping the heap packed from the front.
    void Create(ID3D12Device* device, ID3D12DescriptorHeap* heap, UINT cap)
    {
        IM_ASSERT(m_heap == nullptr && m_freeIndices.empty());
        m_heap = heap;
        D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
        m_type = desc.Type;
        m_cpuStart = heap->GetCPUDescriptorHandleForHeapStart();
        m_gpuStart = heap->GetGPUDescriptorHandleForHeapStart();
        m_incrementSize = device->GetDescriptorHandleIncrementSize(m_type);
        m_freeIndices.reserve((int)desc.NumDescriptors);
        for (int n = desc.NumDescriptors; n > 0; n--)
            m_freeIndices.push_back(n - 1);
    }

    // Pop the next free slot and compute its CPU/GPU handles by offsetting from
    // the heap start.
    void Alloc(D3D12_CPU_DESCRIPTOR_HANDLE* outCpu,
               D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
    {
        if (m_freeIndices.empty())
            return;

        int idx = m_freeIndices.back();
        m_freeIndices.pop_back();

        outCpu->ptr = m_cpuStart.ptr + static_cast<SIZE_T>(idx) * m_incrementSize;
        outGpu->ptr = m_gpuStart.ptr + static_cast<UINT64>(idx) * m_incrementSize;
    }

    // Recover the slot index from the CPU handle and return it to the
    // free-list.
    void Free(D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu)
    {
        int cpu_idx = (int)((cpu.ptr - m_cpuStart.ptr) / m_incrementSize);
        int gpu_idx = (int)((gpu.ptr - m_gpuStart.ptr) / m_incrementSize);
        IM_ASSERT(cpu_idx == gpu_idx);

        m_freeIndices.push_back(cpu_idx);
    }

    auto Destroy() -> void
    {
        m_heap = nullptr;
        m_freeIndices.clear();
    }
};
} // namespace tattler
