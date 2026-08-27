#pragma once

#include <stddef.h>
#include <stdint.h>

/* Layer contracts. Hardware and protocol implementations are added below this boundary. */
typedef struct {
    uint8_t address[6];
    uint16_t mtu;
    int up;
} ortos_net_interface_t;

typedef struct {
    uint8_t address[16];
    uint8_t prefix_length;
} ortos_ip_address_t;

enum {
    ORTOS_NET_OK = 0,
    ORTOS_NET_ERR_INVAL = -1,
    ORTOS_NET_ERR_NOT_SUPPORTED = -2,
    ORTOS_NET_ERR_NO_INTERFACE = -3
};

int ortos_net_register_interface(const ortos_net_interface_t *interface);
int ortos_net_send_ipv4(const ortos_ip_address_t *destination,
                        const void *payload, size_t length);
int ortos_net_send_ipv6(const ortos_ip_address_t *destination,
                        const void *payload, size_t length);
int ortos_net_socket_open(int protocol);
int ortos_net_firewall_allows(uint16_t source_port, uint16_t destination_port,
                              int inbound);