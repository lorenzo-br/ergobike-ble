#pragma once

#include <cstddef>
#include <cstdint>

namespace ftms {

constexpr uint8_t kResponseCode = 0x80;
constexpr uint8_t kSuccess = 0x01;
constexpr uint8_t kNotSupported = 0x02;
constexpr uint8_t kInvalidParameter = 0x03;
constexpr uint8_t kControlNotPermitted = 0x05;
constexpr uint8_t kStatusReset = 0x00;
constexpr uint8_t kStatusStoppedOrPausedByUser = 0x01;
constexpr uint8_t kStatusStartedOrResumedByUser = 0x03;

struct ControlPointState {
    bool controlGranted = false;
    bool running = false;
};

// Build the standard three-byte response for an FTMS Control Point request.
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
        case 0x00: // Request Control
            if (requestLength != 1) {
                status = kInvalidParameter;
                break;
            }
            state.controlGranted = true;
            status = kSuccess;
            break;
        case 0x01: // Reset
            if (requestLength != 1) {
                status = kInvalidParameter;
                break;
            }
            state = {};
            status = kSuccess;
            break;
        case 0x07: // Start/Resume
            if (requestLength != 1) {
                status = kInvalidParameter;
            } else if (!state.controlGranted) {
                status = kControlNotPermitted;
            } else {
                state.running = true;
                status = kSuccess;
            }
            break;
        case 0x08: // Stop/Pause
            if (requestLength != 2 || (request[1] != 0x01 && request[1] != 0x02)) {
                status = kInvalidParameter;
            } else if (!state.controlGranted) {
                status = kControlNotPermitted;
            } else {
                state.running = false;
                status = kSuccess;
            }
            break;
        default:
            break;
    }

    response[0] = kResponseCode;
    response[1] = opcode;
    response[2] = status;
    return 3;
}

} // namespace ftms
