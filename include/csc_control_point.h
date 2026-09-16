#pragma once

#include <cstddef>
#include <cstdint>

namespace csc {

constexpr uint8_t kResponseCode = 0x10;
constexpr uint8_t kSuccess = 0x01;
constexpr uint8_t kNotSupported = 0x02;
constexpr uint8_t kInvalidParameter = 0x03;

struct ControlPointState {
    uint32_t cumulativeWheelRevolutions = 0;
};

// Build the standard response for the supported CSC Control Point procedure.
inline std::size_t buildResponse(const uint8_t* request,
                                  std::size_t requestLength,
                                  uint8_t* response,
                                  std::size_t responseCapacity,
                                  ControlPointState& state) {
    if (!request || requestLength == 0 || !response || responseCapacity < 3) {
        return 0;
    }

    const uint8_t opcode = request[0];
    uint8_t status = kNotSupported;

    switch (opcode) {
        case 0x01: // Set Cumulative Value
            if (requestLength != 5) {
                status = kInvalidParameter;
                break;
            }
            state.cumulativeWheelRevolutions =
                static_cast<uint32_t>(request[1]) |
                (static_cast<uint32_t>(request[2]) << 8) |
                (static_cast<uint32_t>(request[3]) << 16) |
                (static_cast<uint32_t>(request[4]) << 24);
            status = kSuccess;
            break;
        default:
            break;
    }

    response[0] = kResponseCode;
    response[1] = opcode;
    response[2] = status;
    return 3;
}

} // namespace csc
