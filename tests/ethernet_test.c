#include <assert.h>
#include <stdint.h>

#include "net/ethernet.h"

int main(void)
{
    static const uint8_t destination[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    static const uint8_t source[6] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };
    static const uint8_t payload[] = { 1, 2, 3, 4 };
    uint8_t buffer[ORTOS_ETHERNET_MAX_FRAME_SIZE];
    ortos_ethernet_frame_t frame;
    size_t length = 0;

    assert(ortos_ethernet_frame_build(buffer, sizeof(buffer), destination, source,
                                      0x0800, payload, sizeof(payload), &length) == ORTOS_ETHERNET_OK);
    assert(length == ORTOS_ETHERNET_MIN_FRAME_SIZE);
    assert(ortos_ethernet_frame_parse(buffer, length, &frame) == ORTOS_ETHERNET_OK);
    assert(frame.ether_type == 0x0800);
    assert(frame.payload_length == length - ORTOS_ETHERNET_HEADER_SIZE);
    assert(frame.payload[0] == 1 && frame.payload[3] == 4);
    assert(ortos_ethernet_frame_parse(buffer, 13, &frame) == ORTOS_ETHERNET_ERR_TOO_SMALL);
    assert(ortos_ethernet_frame_build(buffer, 20, destination, source, 0x0800,
                                      payload, sizeof(payload), &length) == ORTOS_ETHERNET_ERR_TOO_SMALL);
    return 0;
}