#pragma once

#include <stddef.h>
#include <stdint.h>

#define ORTOS_ETHERNET_ADDRESS_SIZE 6u
#define ORTOS_ETHERNET_HEADER_SIZE 14u
#define ORTOS_ETHERNET_MIN_FRAME_SIZE 60u
#define ORTOS_ETHERNET_MAX_FRAME_SIZE 1518u

enum {
    ORTOS_ETHERNET_OK = 0,
    ORTOS_ETHERNET_ERR_INVAL = -1,
    ORTOS_ETHERNET_ERR_TOO_SMALL = -2,
    ORTOS_ETHERNET_ERR_TOO_LARGE = -3
};

typedef struct {
    uint8_t destination[ORTOS_ETHERNET_ADDRESS_SIZE];
    uint8_t source[ORTOS_ETHERNET_ADDRESS_SIZE];
    uint16_t ether_type;
    const uint8_t *payload;
    size_t payload_length;
} ortos_ethernet_frame_t;

typedef struct {
    void *context;
    uint8_t address[ORTOS_ETHERNET_ADDRESS_SIZE];
    uint16_t mtu;
    int link_up;
    const char *name;
    int (*send)(void *context, const void *frame, size_t length);
    int (*receive)(void *context, void *frame, size_t capacity, size_t *length);
} ortos_ethernet_device_t;

int ortos_ethernet_frame_parse(const void *buffer, size_t length,
                               ortos_ethernet_frame_t *frame);
int ortos_ethernet_frame_build(void *buffer, size_t capacity,
                               const uint8_t destination[ORTOS_ETHERNET_ADDRESS_SIZE],
                               const uint8_t source[ORTOS_ETHERNET_ADDRESS_SIZE],
                               uint16_t ether_type, const void *payload,
                               size_t payload_length, size_t *frame_length);