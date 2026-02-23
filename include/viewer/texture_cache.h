#pragma once

#include "common/capture_types.h"
#include "imgui.h"

#include <d3d12.h>

#include <cstdint>
#include <list>
#include <unordered_map>
#include <utility>
#include <wrl/client.h>

namespace Tattler
{

class D3D12Renderer;

/// <summary>
/// LRU cache for frame textures. Keeps recently accessed textures in memory
/// (up to maxMemoryMB), evicting least recently used when full. Supports
/// loading from disk for textures written by the capture controller.
/// </summary>
class TextureCache
{
  public:
    explicit TextureCache(D3D12Renderer* renderer, size_t maxMemoryMB = 512);

    /// <summary>
    /// Get texture for the given frame index. Loads from disk if needed.
    /// </summary>
    /// <returns>0 if texture doesn't exist or loading failed.</returns>
    auto Get(uint32_t frameIndex, const StagedTexture& stagingInfo)
        -> ImTextureID;

    /// <summary>
    /// Clear all cached textures and free GPU resources.
    /// </summary>
    auto Clear() -> void;

  private:
    struct CachedEntry
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
        size_t sizeBytes;
    };

    auto Evict() -> void;
    auto LoadFromDisk(uint32_t frameIndex, const StagedTexture& stagingInfo)
        -> ImTextureID;

    D3D12Renderer* m_renderer;
    size_t m_maxBytes;
    size_t m_currentBytes = 0;

    // LRU: front = most recently used, back = least recently used
    std::list<uint32_t> m_lru;
    std::unordered_map<uint32_t,
                       std::pair<CachedEntry, std::list<uint32_t>::iterator>>
        m_cache;
};

} // namespace Tattler
