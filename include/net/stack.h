#pragma once

#include <stddef.h>
#include <stdint.h>

#define ORTOS_NET_MAX_INTERFACES 8
#define ORTOS_NET_MAX_FIREWALL_RULES 16
#define ORTOS_NET_MAX_SOCKETS 32

/* Link-layer types that the project can represent without assuming a specific NIC. */
enum {
    ORTOS_NET_IFACE_ETHERNET = 1,
    ORTOS_NET_IFACE_WIFI = 2,
    ORTOS_NET_IFACE_LOOPBACK = 3
};

enum {
    ORTOS_NET_OK = 0,
    ORTOS_NET_ERR_INVAL = -1,
    ORTOS_NET_ERR_NOT_SUPPORTED = -2,
    ORTOS_NET_ERR_NO_INTERFACE = -3,
    ORTOS_NET_ERR_NO_MEMORY = -4,
    ORTOS_NET_ERR_TIMEOUT = -5,
    ORTOS_NET_ERR_INVALID_CHECKSUM = -6
};

enum {
    ORTOS_NET_PROTO_RAW = 0,
    ORTOS_NET_PROTO_ICMP = 1,
    ORTOS_NET_PROTO_TCP = 6,
    ORTOS_NET_PROTO_UDP = 17,
    ORTOS_NET_PROTO_ESP = 50
};

/* Layer contracts. Hardware and protocol implementations are added below this boundary. */
typedef struct {
    uint8_t address[6];
    uint16_t mtu;
    int up;
    int type;
    uint8_t ipv4[4];
    uint8_t ipv4_netmask[4];
    uint8_t ipv4_gateway[4];
    char name[16];
} ortos_net_interface_t;

typedef struct {
    uint8_t address[16];
    uint8_t prefix_length;
} ortos_ip_address_t;

typedef struct {
    uint8_t version_ihl;
    uint8_t dscp_ecn;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment_offset;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t header_checksum;
    uint8_t source[4];
    uint8_t destination[4];
} ortos_ipv4_header_t;

typedef struct {
    uint16_t source_port;
    uint16_t destination_port;
    uint16_t length;
    uint16_t checksum;
} ortos_udp_header_t;

typedef struct {
    uint16_t source_port;
    uint16_t destination_port;
    uint32_t sequence_number;
    uint32_t acknowledgement_number;
    uint16_t data_offset_flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_pointer;
} ortos_tcp_header_t;

typedef struct {
    uint16_t transaction_id;
    uint16_t flags;
    uint16_t questions;
    uint16_t answers;
    uint16_t authority_records;
    uint16_t additional_records;
} ortos_dns_header_t;

typedef struct {
    uint32_t source_ip;
    uint32_t destination_ip;
    uint8_t protocol;
    uint16_t source_port;
    uint16_t destination_port;
    int inbound;
    int allow;
} ortos_net_firewall_rule_t;

int ortos_net_register_interface(const ortos_net_interface_t *interface);
int ortos_net_send_ipv4(const ortos_ip_address_t *destination,
                        const void *payload, size_t length);
int ortos_net_send_ipv6(const ortos_ip_address_t *destination,
                        const void *payload, size_t length);
int ortos_net_socket_open(int protocol);
int ortos_net_socket_bind(int socket, uint16_t port);
int ortos_net_socket_close(int socket);
void ortos_net_firewall_set_default_policy(int allow);
int ortos_net_firewall_add_rule(uint32_t source_ip, uint32_t destination_ip,
                               uint8_t protocol, uint16_t source_port,
                               uint16_t destination_port, int inbound, int allow);
int ortos_net_firewall_allows(uint16_t source_port, uint16_t destination_port,
                              int inbound);
uint16_t ortos_net_checksum16(const void *data, size_t length);
int ortos_net_ipv4_packet_validate(const void *packet, size_t packet_length,
                                  ortos_ipv4_header_t *header,
                                  size_t *payload_offset);
int ortos_net_dns_parse_name(const uint8_t *packet, size_t packet_length,
                            size_t offset, char *out, size_t out_size);