#include "net/ethernet.h"

static void copy_bytes(uint8_t *destination, const uint8_t *source, size_t length)
{
    size_t index;

    for (index = 0; index < length; ++index)
    {
        destination[index] = source[index];
    }
}

int ortos_ethernet_frame_parse(const void *buffer, size_t length,
                               ortos_ethernet_frame_t *frame)
{
    const uint8_t *bytes = (const uint8_t *)buffer;

    if (buffer == NULL || frame == NULL)
    {
        return ORTOS_ETHERNET_ERR_INVAL;
    }
    if (length < ORTOS_ETHERNET_HEADER_SIZE)
    {
        return ORTOS_ETHERNET_ERR_TOO_SMALL;
    }
    if (length > ORTOS_ETHERNET_MAX_FRAME_SIZE)
    {
        return ORTOS_ETHERNET_ERR_TOO_LARGE;
    }

    copy_bytes(frame->destination, bytes, ORTOS_ETHERNET_ADDRESS_SIZE);
    copy_bytes(frame->source, bytes + 6, ORTOS_ETHERNET_ADDRESS_SIZE);
    frame->ether_type = (uint16_t)(((uint16_t)bytes[12] << 8) | bytes[13]);
    frame->payload = bytes + ORTOS_ETHERNET_HEADER_SIZE;
    frame->payload_length = length - ORTOS_ETHERNET_HEADER_SIZE;
    return ORTOS_ETHERNET_OK;
}

int ortos_ethernet_frame_build(void *buffer, size_t capacity,
                               const uint8_t destination[ORTOS_ETHERNET_ADDRESS_SIZE],
                               const uint8_t source[ORTOS_ETHERNET_ADDRESS_SIZE],
                               uint16_t ether_type, const void *payload,
                               size_t payload_length, size_t *frame_length)
{
    uint8_t *bytes = (uint8_t *)buffer;
    size_t length = ORTOS_ETHERNET_HEADER_SIZE + payload_length;

    if (buffer == NULL || destination == NULL || source == NULL ||
        (payload == NULL && payload_length != 0u))
    {
        return ORTOS_ETHERNET_ERR_INVAL;
    }
    if (length > ORTOS_ETHERNET_MAX_FRAME_SIZE)
    {
        return ORTOS_ETHERNET_ERR_TOO_LARGE;
    }
    if (capacity < ORTOS_ETHERNET_MIN_FRAME_SIZE)
    {
        return ORTOS_ETHERNET_ERR_TOO_SMALL;
    }
    if (length < ORTOS_ETHERNET_MIN_FRAME_SIZE)
    {
        length = ORTOS_ETHERNET_MIN_FRAME_SIZE;
    }

    copy_bytes(bytes, destination, ORTOS_ETHERNET_ADDRESS_SIZE);
    copy_bytes(bytes + 6, source, ORTOS_ETHERNET_ADDRESS_SIZE);
    bytes[12] = (uint8_t)(ether_type >> 8);
    bytes[13] = (uint8_t)ether_type;
    copy_bytes(bytes + ORTOS_ETHERNET_HEADER_SIZE, (const uint8_t *)payload,
               payload_length);
    for (size_t index = ORTOS_ETHERNET_HEADER_SIZE + payload_length;
         index < length; ++index)
    {
        bytes[index] = 0;
    }
    if (frame_length != NULL)
    {
        *frame_length = length;
    }
    return ORTOS_ETHERNET_OK;
}