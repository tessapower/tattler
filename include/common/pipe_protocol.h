#pragma once

#include <cstdint>
#include <vector>

namespace PipeProtocol
{

static constexpr uint32_t BUFFER_SIZE = 64 * 1024;
static constexpr const wchar_t* PIPE_NAME = L"\\\\.\\pipe\\tattler";

enum class MessageType : uint32_t
{
    StartCapture, // viewer → hook (no payload)
    StopCapture,  // viewer → hook (no payload)
    CaptureData,  // hook → viewer (payload: serialised events)
};

constexpr uint32_t PROTOCOL_VERSION = 1;

struct MessageHeader
{
    uint32_t version = 1; // For future compatibility, currently always 1
    MessageType type;
    uint32_t payloadSize;
};

/// <summary>
/// Helper to read exactly N bytes
/// </summary>
/// <param name="pipe">Pipe handle to read from</param>
/// <param name="buffer">Buffer to read into</param>
/// <param name="size">Number of bytes to read</param>
inline auto ReadExact(HANDLE pipe, void* buffer, uint32_t size) -> bool
{
    auto* ptr = static_cast<uint8_t*>(buffer);
    uint32_t remaining = size;
    while (remaining > 0)
    {
        DWORD bytesRead = 0;
        if (!ReadFile(pipe, ptr, remaining, &bytesRead, nullptr) ||
            bytesRead == 0)
            return false;
        ptr += bytesRead;
        remaining -= bytesRead;
    }

    return true;
}

/// <summary>
/// Helper to write exactly N bytes
/// </summary>
/// <param name="pipe">Pipe handle to write to</param>
/// <param name="buffer">Buffer to write from</param>
/// <param name="size">Number of bytes to write</param>
inline auto WriteExact(HANDLE pipe, const void* buffer, uint32_t size) -> bool
{
    auto* ptr = static_cast<const uint8_t*>(buffer);
    uint32_t remaining = size;
    while (remaining > 0)
    {
        DWORD bytesWritten = 0;
        if (!WriteFile(pipe, ptr, remaining, &bytesWritten, nullptr) ||
            bytesWritten == 0)
            return false;
        ptr += bytesWritten;
        remaining -= bytesWritten;
    }

    return true;
}

class Pipe
{
  public:
    Pipe() = default;
    virtual ~Pipe() = default;

    virtual auto Connect() -> bool = 0;

    /// <summary>
    /// Write header + payload (if any) to the pipe
    /// </summary>
    /// <param name="type">Message type</param>
    /// <param name="data">Pointer to the payload data</param>
    /// <param name="size">Size of the payload data</param>
    /// <returns>True if the message was successfully sent, false otherwise</returns>
    auto Send(PipeProtocol::MessageType type, const void* data,
              uint32_t size) const -> bool
    {
        PipeProtocol::MessageHeader header{PROTOCOL_VERSION, type, size};
        if (!WriteExact(m_handle, &header, sizeof(header)))
            return false;

        if (size > 0 && !WriteExact(m_handle, data, size))
            return false;

        return true;
    }

    /// <summary>
    /// Read header, allocate, read payload
    /// </summary>
    /// <param name="outType">Output message type</param>
    /// <param name="outPayload">Output payload buffer, only allocated if not
    /// null</param> <returns>True if the message was successfully received,
    /// false otherwise</returns>
    auto Receive(PipeProtocol::MessageType& outType,
                 std::vector<uint8_t>* outPayload) const -> bool
    {
        MessageHeader header{};
        if (!ReadExact(m_handle, &header, sizeof(header)))
            return false;

        outType = header.type;

        if (header.payloadSize > 0)
        {
            if (outPayload == nullptr)
            {
                // Caller doesn't care about the payload, just skip it
                std::vector<uint8_t> tempBuffer(header.payloadSize);
                return ReadExact(m_handle, tempBuffer.data(),
                                 header.payloadSize);
            }
            // Otherwise, allocate the output buffer and read into it
            outPayload->resize(header.payloadSize);
            // Only read if we have a payload to read, otherwise leave empty
            if (!ReadExact(m_handle, outPayload->data(), header.payloadSize))
                return false;
        }

        return true;
    }

    /// <summary>
    /// Disconnects the pipe (server: disconnects client, client: disconnects
    /// from server)
    /// </summary>
    virtual auto Disconnect() -> void = 0;

  protected:
    HANDLE m_handle = INVALID_HANDLE_VALUE;
};

} // namespace PipeProtocol
