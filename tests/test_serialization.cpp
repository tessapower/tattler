#include "stdafx.h"

#include "common/capture_serialization.h"

#include <algorithm>

#include <gtest/gtest.h>

using namespace tattler;

//-------------------------------------------------------------- TEST DATA --//

static auto MakeDrawEvent(uint32_t frameIndex, uint32_t eventIndex) -> CapturedEvent
{
    CapturedEvent e{};
    e.frameIndex     = frameIndex;
    e.eventIndex     = eventIndex;
    e.type           = EventType::Draw;
    e.timestampBegin = 1000;
    e.timestampEnd   = 2000;
    e.commandList    = 0xAABBCCDD11223344;
    e.pipelineState  = 0x1122334455667788;
    e.renderTarget   = 0xDEADBEEFCAFEBABE;
    e.params         = DrawParams{6, 1};
    return e;
}

static auto MakeBarrierEvent(uint32_t frameIndex, uint32_t eventIndex) -> CapturedEvent
{
    CapturedEvent e{};
    e.frameIndex     = frameIndex;
    e.eventIndex     = eventIndex;
    e.type           = EventType::ResourceBarrier;
    e.timestampBegin = 500;
    e.timestampEnd   = 501;
    e.commandList    = 0xAABBCCDD11223344;
    e.params         = BarrierParams{D3D12_RESOURCE_STATE_RENDER_TARGET,
                                     D3D12_RESOURCE_STATE_PRESENT,
                                     0xFEEDFACE00000000};
    return e;
}

static auto MakeFrame(uint32_t frameNumber) -> CapturedFrame
{
    CapturedFrame f{};
    f.frameNumber     = frameNumber;
    f.cpuFrameStartUs = 1'000'000;
    f.cpuFrameEndUs   = 1'016'667;
    f.gpuFrequency    = 240'000'000;
    f.events.push_back(MakeBarrierEvent(frameNumber, 0));
    f.events.push_back(MakeDrawEvent(frameNumber, 1));
    return f;
}

static auto MakeTexture() -> StagedTexture
{
    StagedTexture t{};
    t.sourceResource = 0xCAFEBABE12345678;
    t.width          = 1920;
    t.height         = 1080;
    t.format         = DXGI_FORMAT_R8G8B8A8_UNORM;
    t.subresource    = 0;
    t.pixels.assign(1920 * 1080 * 4, 0x80); // grey
    return t;
}

//--------------------------------------------------------- Start of Tests --//

TEST(Serialization, EmptySnapshot_RoundTrips)
{
    CaptureSnapshot original{};
    original.captureDurationSec = 0.0;

    std::vector<uint8_t> buf;
    ASSERT_TRUE(Serialize(original, &buf));
    EXPECT_FALSE(buf.empty());

    CaptureSnapshot result{};
    ASSERT_TRUE(Deserialize(buf, &result));

    EXPECT_DOUBLE_EQ(result.captureDurationSec, 0.0);
    EXPECT_TRUE(result.frames.empty());
    EXPECT_TRUE(result.renderTargetSnapshots.empty());
}

TEST(Serialization, SingleFrame_RoundTrips)
{
    CaptureSnapshot original{};
    original.captureDurationSec = 1.5;
    original.frames.push_back(MakeFrame(0));

    std::vector<uint8_t> buf;
    ASSERT_TRUE(Serialize(original, &buf));

    CaptureSnapshot result{};
    ASSERT_TRUE(Deserialize(buf, &result));

    EXPECT_DOUBLE_EQ(result.captureDurationSec, 1.5);
    ASSERT_EQ(result.frames.size(), 1u);

    const auto& f = result.frames[0];
    EXPECT_EQ(f.frameNumber,     0u);
    EXPECT_EQ(f.cpuFrameStartUs, 1'000'000u);
    EXPECT_EQ(f.cpuFrameEndUs,   1'016'667u);
    EXPECT_EQ(f.gpuFrequency,    240'000'000u);
    ASSERT_EQ(f.events.size(),   2u);
}

TEST(Serialization, EventFields_PreservedExactly)
{
    CaptureSnapshot original{};
    original.frames.push_back(MakeFrame(7));

    std::vector<uint8_t> buf;
    ASSERT_TRUE(Serialize(original, &buf));

    CaptureSnapshot result{};
    ASSERT_TRUE(Deserialize(buf, &result));

    const auto& draw = result.frames[0].events[1];
    EXPECT_EQ(draw.frameIndex,     7u);
    EXPECT_EQ(draw.eventIndex,     1u);
    EXPECT_EQ(draw.type,           EventType::Draw);
    EXPECT_EQ(draw.timestampBegin, 1000u);
    EXPECT_EQ(draw.timestampEnd,   2000u);
    EXPECT_EQ(draw.commandList,    0xAABBCCDD11223344u);
    EXPECT_EQ(draw.pipelineState,  0x1122334455667788u);
    EXPECT_EQ(draw.renderTarget,   0xDEADBEEFCAFEBABEu);

    const auto& params = std::get<DrawParams>(draw.params);
    EXPECT_EQ(params.vertexCount,   6u);
    EXPECT_EQ(params.instanceCount, 1u);
}

TEST(Serialization, BarrierParams_PreservedExactly)
{
    CaptureSnapshot original{};
    original.frames.push_back(MakeFrame(0));

    std::vector<uint8_t> buf;
    ASSERT_TRUE(Serialize(original, &buf));

    CaptureSnapshot result{};
    ASSERT_TRUE(Deserialize(buf, &result));

    const auto& barrier = result.frames[0].events[0];
    EXPECT_EQ(barrier.type, EventType::ResourceBarrier);

    const auto& params = std::get<BarrierParams>(barrier.params);
    EXPECT_EQ(params.before,   D3D12_RESOURCE_STATE_RENDER_TARGET);
    EXPECT_EQ(params.after,    D3D12_RESOURCE_STATE_PRESENT);
    EXPECT_EQ(params.resource, 0xFEEDFACE00000000u);
}

TEST(Serialization, MultipleFrames_RoundTrip)
{
    CaptureSnapshot original{};
    original.captureDurationSec = 3.0;
    for (uint32_t i = 0; i < 5; ++i)
        original.frames.push_back(MakeFrame(i));

    std::vector<uint8_t> buf;
    ASSERT_TRUE(Serialize(original, &buf));

    CaptureSnapshot result{};
    ASSERT_TRUE(Deserialize(buf, &result));

    ASSERT_EQ(result.frames.size(), 5u);
    for (uint32_t i = 0; i < 5; ++i)
        EXPECT_EQ(result.frames[i].frameNumber, i);
}

TEST(Serialization, RenderTargetSnapshot_RoundTrips)
{
    CaptureSnapshot original{};
    original.renderTargetSnapshots.push_back(MakeTexture());

    std::vector<uint8_t> buf;
    ASSERT_TRUE(Serialize(original, &buf));

    CaptureSnapshot result{};
    ASSERT_TRUE(Deserialize(buf, &result));

    ASSERT_EQ(result.renderTargetSnapshots.size(), 1u);
    const auto& t = result.renderTargetSnapshots[0];
    EXPECT_EQ(t.sourceResource, 0xCAFEBABE12345678u);
    EXPECT_EQ(t.width,          1920u);
    EXPECT_EQ(t.height,         1080u);
    EXPECT_EQ(t.format,         DXGI_FORMAT_R8G8B8A8_UNORM);
    EXPECT_EQ(t.subresource,    0u);
    ASSERT_EQ(t.pixels.size(),  1920u * 1080u * 4u);
    EXPECT_TRUE(std::all_of(t.pixels.begin(), t.pixels.end(),
                            [](uint8_t v) { return v == 0x80; }));
}

TEST(Serialization, TruncatedBuffer_ReturnsFalse)
{
    CaptureSnapshot original{};
    original.captureDurationSec = 1.0;
    original.frames.push_back(MakeFrame(0));

    std::vector<uint8_t> buf;
    ASSERT_TRUE(Serialize(original, &buf));

    // Lop off the last quarter of the buffer
    buf.resize(buf.size() * 3 / 4);

    CaptureSnapshot result{};
    EXPECT_FALSE(Deserialize(buf, &result));
}

TEST(Serialization, EmptyBuffer_ReturnsFalse)
{
    std::vector<uint8_t> buf;
    CaptureSnapshot result{};
    EXPECT_FALSE(Deserialize(buf, &result));
}
