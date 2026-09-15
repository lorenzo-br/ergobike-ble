#include <cassert>
#include <cstddef>
#include <cstdint>

#include "ftms_control_point.h"

static void expectResponse(const uint8_t* request,
                           std::size_t requestLength,
                           uint8_t expectedStatus,
                           ftms::ControlPointState& state) {
    uint8_t response[3] = {};

    assert(ftms::buildResponse(request, requestLength, response, sizeof(response), state) == 3);
    assert(response[0] == 0x80);
    assert(response[1] == request[0]);
    assert(response[2] == expectedStatus);
}

int main() {
    ftms::ControlPointState state;
    const uint8_t requestControl[] = {0x00};
    expectResponse(requestControl, sizeof(requestControl), 0x01, state);
    assert(state.controlGranted);

    const uint8_t reset[] = {0x01};
    expectResponse(reset, sizeof(reset), 0x01, state);
    assert(!state.controlGranted);
    assert(!state.running);

    const uint8_t start[] = {0x07};
    expectResponse(start, sizeof(start), 0x05, state); // Control not permitted

    expectResponse(requestControl, sizeof(requestControl), 0x01, state);
    expectResponse(start, sizeof(start), 0x01, state);
    assert(state.running);

    const uint8_t stop[] = {0x08, 0x01};
    expectResponse(stop, sizeof(stop), 0x01, state); // Stop
    assert(!state.running);

    const uint8_t pause[] = {0x08, 0x02};
    expectResponse(pause, sizeof(pause), 0x01, state); // Pause

    const uint8_t malformedStop[] = {0x08};
    expectResponse(malformedStop, sizeof(malformedStop), 0x03, state);

    const uint8_t invalidPause[] = {0x08, 0x03};
    expectResponse(invalidPause, sizeof(invalidPause), 0x03, state);

    const uint8_t unsupported[] = {0x04};
    expectResponse(unsupported, sizeof(unsupported), 0x02, state);

    uint8_t response[3] = {0xFF, 0xFF, 0xFF};
    assert(ftms::buildResponse(nullptr, 0, response, sizeof(response), state) == 0);

    return 0;
}
