#include "stdafx.h"

#include "imgui.h"
#include "viewer/d3d12_renderer.h"
#include "viewer/texture_cache.h"

#include <fstream>

namespace Tattler
{
TextureCache::TextureCache(D3D12Renderer* renderer, size_t maxMemoryMB)
    : m_renderer(renderer), m_maxBytes(maxMemoryMB * 1024 * 1024)
{
}

auto TextureCache::Get(uint32_t frameIndex, const StagedTexture& stagingInfo)
    -> ImTextureID
{
    auto it = m_cache.find(frameIndex);
    if (it != m_cache.end())
    {
        // Move to front of LRU (most recently used)
        m_lru.splice(m_lru.begin(), m_lru, it->second.second);
        return static_cast<ImTextureID>(it->second.first.gpuHandle.ptr);
    }

    // Not in cache - need to load
    ImTextureID result = 0;

    if (stagingInfo.isOnDisk)
    {
        result = LoadFromDisk(frameIndex, stagingInfo);
    }
    else if (!stagingInfo.pixels.empty())
    {
        // In-memory texture - upload to GPU
        D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
        m_renderer->GetSrvHeapAllocator()->Alloc(&cpu, &gpu);

        auto resource = m_renderer->UploadTexture(stagingInfo, cpu, gpu);
        if (resource)
        {
            const size_t sizeBytes = stagingInfo.pixels.size();

            // Evict old entries if needed
            while (m_currentBytes + sizeBytes > m_maxBytes && !m_lru.empty())
                Evict();

            // Add to cache
            CachedEntry entry;
            entry.resource = resource;
            entry.cpuHandle = cpu;
            entry.gpuHandle = gpu;
            entry.sizeBytes = sizeBytes;

            m_lru.push_front(frameIndex);
            m_cache[frameIndex] = {std::move(entry), m_lru.begin()};
            m_currentBytes += sizeBytes;

            result = static_cast<ImTextureID>(gpu.ptr);
        }
        else
        {
            m_renderer->GetSrvHeapAllocator()->Free(cpu, gpu);
        }
    }

    return result;
}

auto TextureCache::LoadFromDisk(uint32_t frameIndex,
                                const StagedTexture& stagingInfo) -> ImTextureID
{
    // Load pixel data from disk
    std::ifstream file(stagingInfo.diskPath, std::ios::binary | std::ios::ate);
    if (!file)
        return 0;

    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> pixels(fileSize);
    file.read(reinterpret_cast<char*>(pixels.data()), fileSize);

    if (!file)
        return 0;

    // Create temporary StagedTexture for upload
    StagedTexture temp = stagingInfo;
    temp.pixels = std::move(pixels);

    D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
    m_renderer->GetSrvHeapAllocator()->Alloc(&cpu, &gpu);

    auto resource = m_renderer->UploadTexture(temp, cpu, gpu);
    if (!resource)
    {
        m_renderer->GetSrvHeapAllocator()->Free(cpu, gpu);
        return 0;
    }

    const size_t sizeBytes = temp.pixels.size();

    // Evict old entries if needed
    while (m_currentBytes + sizeBytes > m_maxBytes && !m_lru.empty())
        Evict();

    // Add to cache
    CachedEntry entry;
    entry.resource = resource;
    entry.cpuHandle = cpu;
    entry.gpuHandle = gpu;
    entry.sizeBytes = sizeBytes;

    m_lru.push_front(frameIndex);
    m_cache[frameIndex] = {std::move(entry), m_lru.begin()};
    m_currentBytes += sizeBytes;

    return static_cast<ImTextureID>(gpu.ptr);
}

auto TextureCache::Evict() -> void
{
    if (m_lru.empty())
        return;

    // Remove least recently used (back of list)
    uint32_t frameIndex = m_lru.back();
    m_lru.pop_back();

    auto it = m_cache.find(frameIndex);
    if (it != m_cache.end())
    {
        m_currentBytes -= it->second.first.sizeBytes;
        m_renderer->GetSrvHeapAllocator()->Free(it->second.first.cpuHandle,
                                                it->second.first.gpuHandle);
        m_cache.erase(it);
    }
}

auto TextureCache::Clear() -> void
{
    m_renderer->FlushQueue();
    for (auto& [frameIdx, entry] : m_cache)
    {
        m_renderer->GetSrvHeapAllocator()->Free(entry.first.cpuHandle,
                                                entry.first.gpuHandle);
    }
    m_cache.clear();
    m_lru.clear();
    m_currentBytes = 0;
}

} // namespace Tattler
