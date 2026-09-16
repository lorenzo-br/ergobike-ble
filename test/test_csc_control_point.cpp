#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "config.h"
#include "csc_control_point.h"

static void expectResponse(const uint8_t* request,
                           std::size_t requestLength,
                           uint8_t expectedStatus,
                           csc::ControlPointState& state) {
    uint8_t response[3] = {};

    assert(csc::buildResponse(request, requestLength, response, sizeof(response), state) == 3);
    assert(response[0] == csc::kResponseCode);
    assert(response[1] == request[0]);
    assert(response[2] == expectedStatus);
}

int main() {
    assert(std::strcmp(UUID_SERVICE_CSC, "1816") == 0);
    assert(std::strcmp(UUID_CHAR_CSC_CONTROL_POINT, "2A55") == 0);
    assert(CSC_APPEARANCE == 0x0485);
    assert(CSC_SENSOR_LOCATION == 0x05);

    csc::ControlPointState state;
    const uint8_t setCumulative[] = {0x01, 0x78, 0x56, 0x34, 0x12};
    expectResponse(setCumulative, sizeof(setCumulative), csc::kSuccess, state);
    assert(state.cumulativeWheelRevolutions == 0x12345678);

    const uint8_t malformed[] = {0x01, 0x78};
    expectResponse(malformed, sizeof(malformed), csc::kInvalidParameter, state);

    const uint8_t unsupported[] = {0x02};
    expectResponse(unsupported, sizeof(unsupported), csc::kNotSupported, state);

    uint8_t response[3] = {0xFF, 0xFF, 0xFF};
    assert(csc::buildResponse(nullptr, 0, response, sizeof(response), state) == 0);

    return 0;
}
