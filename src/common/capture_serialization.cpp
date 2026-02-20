#include "stdafx.h"

#include "common/capture_serialization.h"

#include <cstring>

namespace Tattler
{

auto Serialize(const CaptureSnapshot& snapshot, std::vector<uint8_t>* outBuffer) -> bool
{
    // Flatten snapshot into binary format for pipe.
    // ⚠️ This must mirror Deserialize's format exactly!

    // Duration
    Write(*outBuffer, snapshot.captureDurationSec);

    // Frames
    Write(*outBuffer, static_cast<uint32_t>(snapshot.frames.size()));
    for (const auto& frame : snapshot.frames)
    {
        Write(*outBuffer, frame.frameNumber);
        Write(*outBuffer, frame.cpuFrameStartUs);
        Write(*outBuffer, frame.cpuFrameEndUs);
        Write(*outBuffer, frame.gpuFrequency);

        Write(*outBuffer, static_cast<uint32_t>(frame.events.size()));
        for (const auto& event : frame.events)
        {
            Write(*outBuffer, event.frameIndex);
            Write(*outBuffer, event.eventIndex);
            Write(*outBuffer, event.type);
            Write(*outBuffer, event.timestampBegin);
            Write(*outBuffer, event.timestampEnd);
            Write(*outBuffer, event.commandList);
            Write(*outBuffer, event.pipelineState);
            Write(*outBuffer, event.renderTarget);
            std::visit([&outBuffer](const auto& params) { Write(*outBuffer, params); },
                       event.params);
        }
    }

    // Render target snapshots
    Write(*outBuffer, static_cast<uint32_t>(snapshot.renderTargetSnapshots.size()));
    for (const auto& rtSnapshot : snapshot.renderTargetSnapshots)
    {
        Write(*outBuffer, rtSnapshot.sourceResource);
        Write(*outBuffer, rtSnapshot.width);
        Write(*outBuffer, rtSnapshot.height);
        Write(*outBuffer, static_cast<uint32_t>(rtSnapshot.format));
        Write(*outBuffer, static_cast<uint32_t>(rtSnapshot.pixels.size()));
        outBuffer->insert(outBuffer->end(), rtSnapshot.pixels.begin(),
                          rtSnapshot.pixels.end());
        Write(*outBuffer, rtSnapshot.subresource);
    }

    return true;
}

auto Deserialize(const std::vector<uint8_t>& buffer, CaptureSnapshot* outSnapshot) -> bool
{
    // Parse binary format back into CaptureSnapshot.
    // ⚠️ This must mirror Serialize's format exactly!

    size_t offset = 0;

    // Duration
    double captureDurationSec = 0.0;
    if (!Read(buffer, offset, captureDurationSec))
        return false;

    // Frames
    uint32_t frameCount = 0;
    if (!Read(buffer, offset, frameCount))
        return false;

    std::vector<CapturedFrame> frames(frameCount);
    for (auto& frame : frames)
    {
        if (!Read(buffer, offset, frame.frameNumber))   return false;
        if (!Read(buffer, offset, frame.cpuFrameStartUs)) return false;
        if (!Read(buffer, offset, frame.cpuFrameEndUs))   return false;
        if (!Read(buffer, offset, frame.gpuFrequency))    return false;

        uint32_t eventCount = 0;
        if (!Read(buffer, offset, eventCount))
            return false;

        frame.events.resize(eventCount);
        for (auto& event : frame.events)
        {
            if (!Read(buffer, offset, event.frameIndex))     return false;
            if (!Read(buffer, offset, event.eventIndex))     return false;
            if (!Read(buffer, offset, event.type))           return false;
            if (!Read(buffer, offset, event.timestampBegin)) return false;
            if (!Read(buffer, offset, event.timestampEnd))   return false;
            if (!Read(buffer, offset, event.commandList))    return false;
            if (!Read(buffer, offset, event.pipelineState))  return false;
            if (!Read(buffer, offset, event.renderTarget))   return false;

            switch (event.type)
            {
            case EventType::Draw:
            {
                DrawParams p;
                if (!Read(buffer, offset, p)) return false;
                event.params = p;
                break;
            }
            case EventType::DrawIndexed:
            {
                DrawIndexedParams p;
                if (!Read(buffer, offset, p)) return false;
                event.params = p;
                break;
            }
            case EventType::Dispatch:
            {
                DispatchParams p;
                if (!Read(buffer, offset, p)) return false;
                event.params = p;
                break;
            }
            case EventType::ResourceBarrier:
            {
                BarrierParams p;
                if (!Read(buffer, offset, p)) return false;
                event.params = p;
                break;
            }
            case EventType::ClearRTV:
            {
                ClearRtvParams p;
                if (!Read(buffer, offset, p)) return false;
                event.params = p;
                break;
            }
            case EventType::ClearDSV:
            {
                ClearDsvParams p;
                if (!Read(buffer, offset, p)) return false;
                event.params = p;
                break;
            }
            case EventType::CopyResource:
            {
                CopyParams p;
                if (!Read(buffer, offset, p)) return false;
                event.params = p;
                break;
            }
            case EventType::Present:
            {
                PresentParams p;
                if (!Read(buffer, offset, p)) return false;
                event.params = p;
                break;
            }
            }
        }
    }

    // Render target snapshots
    uint32_t snapshotCount = 0;
    if (!Read(buffer, offset, snapshotCount))
        return false;

    std::vector<StagedTexture> renderTargetSnapshots(snapshotCount);
    for (auto& rtSnapshot : renderTargetSnapshots)
    {
        if (!Read(buffer, offset, rtSnapshot.sourceResource)) return false;
        if (!Read(buffer, offset, rtSnapshot.width))          return false;
        if (!Read(buffer, offset, rtSnapshot.height))         return false;
        if (!Read(buffer, offset, rtSnapshot.format))         return false;

        uint32_t pixelCount{};
        if (!Read(buffer, offset, pixelCount))
            return false;

        if (offset + pixelCount > buffer.size())
            return false;
        rtSnapshot.pixels.resize(pixelCount);
        memcpy(rtSnapshot.pixels.data(), buffer.data() + offset, pixelCount);
        offset += pixelCount;

        if (!Read(buffer, offset, rtSnapshot.subresource)) return false;
    }

    outSnapshot->captureDurationSec    = captureDurationSec;
    outSnapshot->frames                = std::move(frames);
    outSnapshot->renderTargetSnapshots = std::move(renderTargetSnapshots);

    return true;
}

} // namespace Tattler
