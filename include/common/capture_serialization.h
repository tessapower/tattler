#pragma once

#include "common/capture_types.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace tattler
{

template <typename T>
void Write(std::vector<uint8_t>& buf, const T& value)
{
    auto* bytes = reinterpret_cast<const uint8_t*>(&value);
    buf.insert(buf.end(), bytes, bytes + sizeof(T));
}

template <typename T>
bool Read(const std::vector<uint8_t>& buf, size_t& offset, T& value)
{
    if (offset + sizeof(T) > buf.size())
        return false;

    memcpy(&value, buf.data() + offset, sizeof(T));
    offset += sizeof(T);

    return true;
}

/// <summary> Flatten a CaptureSnapshot into a byte buffer suitable for sending over the pipe. </summary>
auto Serialize(const CaptureSnapshot& snapshot, std::vector<uint8_t>* outBuffer) -> bool;

/// <summary> Reconstruct a CaptureSnapshot from a flat byte buffer received over the pipe. </summary>
auto Deserialize(const std::vector<uint8_t>& buffer, CaptureSnapshot* outSnapshot) -> bool;

} // namespace tattler
