#pragma once

#include <d3d12.h>
#include <dxgiformat.h>

#include <cstdint>
#include <variant>
#include <vector>

namespace Tattler
{
// Using a simple uint64_t for resource IDs, COM pointers are 8 bytes so we can
// store them directly. The viewer app will never deference these directly, it
// just aims to correlate events and snapshots using the same resource.
using ResourceId = uint64_t;

enum class EventType : uint32_t
{
    Draw,
    DrawIndexed,
    Dispatch,
    ResourceBarrier,
    ClearRTV,
    ClearDSV,
    CopyResource,
    Present,
    // etc. Add more as needed.
};

struct StagedTexture
{
    ResourceId sourceResource;
    uint32_t width, height;
    DXGI_FORMAT format;
    std::vector<uint8_t> pixels; // CPU-side copy after readback
    uint32_t subresource;
};

struct DrawParams
{
    uint32_t vertexCount, instanceCount;
};

struct DrawIndexedParams
{
    uint32_t indexCount, instanceCount;
};

struct DispatchParams
{
    uint32_t x, y, z;
};

struct CopyParams
{
    ResourceId src, dst;
};

struct BarrierParams
{
    D3D12_RESOURCE_STATES before, after;
    ResourceId resource;
};

struct ClearRtvParams
{
    ResourceId renderTarget;
    float color[4]; // RGBA clear colour
};

struct ClearDsvParams
{
    ResourceId depthStencil;
    float depth;             // clear depth value (typically 1.0f)
    uint8_t stencil;         // clear stencil value (typically 0)
    D3D12_CLEAR_FLAGS flags; // depth, stencil, or both
};

struct PresentParams
{
    uint32_t syncInterval; // 0 = no vsync, 1 = vsync
    uint32_t flags;        // DXGI_PRESENT flags e.g. DXGI_PRESENT_ALLOW_TEARING
};

using EventParams =
    std::variant<DrawParams, DrawIndexedParams, DispatchParams, CopyParams,
                 BarrierParams, ClearRtvParams, ClearDsvParams, PresentParams>;

struct CapturedEvent
{
    uint32_t frameIndex; // Which frame within the capture window
    uint32_t eventIndex; // Sequential within frame
    EventType type;
    uint64_t timestampBegin;  // GPU timestamp (ticks)
    uint64_t timestampEnd;    // GPU timestamp (ticks)
    ResourceId commandList;   // Which command list
    ResourceId pipelineState; // Bound PSO at time of call
    ResourceId renderTarget;  // Bound RTV (for draws)
    EventParams params;       // Parameters specific to the event type
};

struct CapturedFrame
{
    uint32_t frameNumber;
    uint64_t cpuFrameStartUs; // from QueryPerformanceCounter
    uint64_t cpuFrameEndUs;
    uint64_t gpuFrequency; // from GetTimestampFrequency
    std::vector<CapturedEvent> events;
};

struct CaptureSnapshot
{
    double captureDurationSec;
    std::vector<CapturedFrame> frames;
    std::vector<StagedTexture> renderTargetSnapshots; // readback copies
};
} // namespace Tattler
