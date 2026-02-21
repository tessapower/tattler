#include "stdafx.h"

#include "hook/pipe_client.h"
#include "viewer/pipe_server.h"

#include <gtest/gtest.h>

#include <cstring>
#include <thread>

class PipeTest : public ::testing::Test
{
  protected:
    Tattler::PipeServer server;
    Tattler::PipeClient client;

    void SetUp() override
    {
        ASSERT_TRUE(server.Create());

        std::thread serverThread([this]() { server.Connect(); });
        ASSERT_TRUE(client.Connect());
        serverThread.join();
    }

    void TearDown() override
    {
        server.Disconnect();
        client.Disconnect();
        server.Destroy();
    }
};

TEST_F(PipeTest, ServerSendsNoPayload_ClientReceives)
{
    ASSERT_TRUE(
        server.Send(PipeProtocol::MessageType::StartCapture, nullptr, 0));

    PipeProtocol::MessageType type{};
    std::vector<uint8_t> payload;
    ASSERT_TRUE(client.Receive(type, &payload));

    EXPECT_EQ(type, PipeProtocol::MessageType::StartCapture);
    EXPECT_TRUE(payload.empty());
}

TEST_F(PipeTest, ClientSendsPayload_ServerReceives)
{
    const char* testData = "hello from hook";
    ASSERT_TRUE(client.Send(PipeProtocol::MessageType::CaptureData,
                                   testData,
                                   static_cast<uint32_t>(strlen(testData))));

    PipeProtocol::MessageType type{};
    std::vector<uint8_t> payload;
    ASSERT_TRUE(server.Receive(type, &payload));

    EXPECT_EQ(type, PipeProtocol::MessageType::CaptureData);
    ASSERT_EQ(payload.size(), strlen(testData));
    EXPECT_EQ(memcmp(payload.data(), testData, payload.size()), 0);
}

TEST_F(PipeTest, ReceiveWithNullPayload_SkipsData)
{
    const char* testData = "discard me";
    ASSERT_TRUE(server.Send(PipeProtocol::MessageType::CaptureData,
                                   testData,
                                   static_cast<uint32_t>(strlen(testData))));

    PipeProtocol::MessageType type{};
    ASSERT_TRUE(client.Receive(type, nullptr));

    EXPECT_EQ(type, PipeProtocol::MessageType::CaptureData);
}

TEST_F(PipeTest, RoundTrip_StartCaptureThenCaptureData)
{
    // Server sends StartCapture
    ASSERT_TRUE(
        server.Send(PipeProtocol::MessageType::StartCapture, nullptr, 0));

    PipeProtocol::MessageType type{};
    std::vector<uint8_t> payload;
    ASSERT_TRUE(client.Receive(type, &payload));
    EXPECT_EQ(type, PipeProtocol::MessageType::StartCapture);

    // Client responds with CaptureData
    const char* response = "frame data";
    ASSERT_TRUE(client.Send(PipeProtocol::MessageType::CaptureData,
                                   response,
                                   static_cast<uint32_t>(strlen(response))));

    type = {};
    payload.clear();
    ASSERT_TRUE(server.Receive(type, &payload));
    EXPECT_EQ(type, PipeProtocol::MessageType::CaptureData);
    ASSERT_EQ(payload.size(), strlen(response));
    EXPECT_EQ(memcmp(payload.data(), response, payload.size()), 0);
}
