#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "net/stack.h"

static void test_ipv4_validation(void)
{
    uint8_t packet[24] = {0};
    ortos_ipv4_header_t header;
    size_t payload_offset = 0;
    uint16_t checksum;

    packet[0] = 0x45;
    packet[1] = 0x00;
    packet[2] = 0x00;
    packet[3] = 0x1c;
    packet[4] = 0x00;
    packet[5] = 0x01;
    packet[6] = 0x40;
    packet[7] = 0x00;
    packet[8] = 0x40;
    packet[9] = 0x06;
    packet[12] = 0xc0;
    packet[13] = 0xa8;
    packet[14] = 0x00;
    packet[15] = 0x01;
    packet[16] = 0xc0;
    packet[17] = 0xa8;
    packet[18] = 0x00;
    packet[19] = 0x02;

    checksum = ortos_net_checksum16(packet, 20);
    packet[10] = (uint8_t)(checksum >> 8);
    packet[11] = (uint8_t)(checksum & 0xffu);

    assert(ortos_net_ipv4_packet_validate(packet, sizeof(packet), &header, &payload_offset) == ORTOS_NET_OK);
    assert(payload_offset == 20u);
    assert(header.version_ihl == 0x45);
    assert(header.protocol == 0x06);
}

static void test_dns_name(void)
{
    uint8_t dns_name[] = { 3u, 'w', 'w', 'w', 7u, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 3u, 'c', 'o', 'm', 0u };
    char parsed[64];

    assert(ortos_net_dns_parse_name(dns_name, sizeof(dns_name), 0, parsed, sizeof(parsed)) == ORTOS_NET_OK);
    assert(strcmp(parsed, "www.example.com") == 0);
}

static void test_firewall_and_sockets(void)
{
    int socket;
    ortos_net_interface_t iface = {
        .address = { 0x00, 0x10, 0x5a, 0x00, 0x00, 0x01 },
        .mtu = 1500,
        .up = 1,
        .type = ORTOS_NET_IFACE_ETHERNET,
        .ipv4 = { 10, 0, 0, 2 },
        .ipv4_netmask = { 255, 255, 255, 0 },
        .ipv4_gateway = { 10, 0, 0, 1 }
    };

    assert(ortos_net_register_interface(&iface) == ORTOS_NET_OK);
    assert(ortos_net_send_ipv4(&(ortos_ip_address_t){ { 10, 0, 0, 1 }, 24 }, "OK", 2) == ORTOS_NET_OK);

    socket = ortos_net_socket_open(ORTOS_NET_PROTO_TCP);
    assert(socket >= 0);
    assert(ortos_net_socket_bind(socket, 8080) == ORTOS_NET_OK);
    assert(ortos_net_socket_close(socket) == ORTOS_NET_OK);
    assert(ortos_net_socket_close(socket) == ORTOS_NET_ERR_INVAL);

    ortos_net_firewall_set_default_policy(1);
    assert(ortos_net_firewall_add_rule(0u, 0u, ORTOS_NET_PROTO_TCP, 0, 22, 1, 0) == ORTOS_NET_OK);
    assert(ortos_net_firewall_allows(5000, 22, 1) == 0);
    assert(ortos_net_firewall_allows(5000, 80, 1) == 1);
}

int main(void)
{
    test_ipv4_validation();
    test_dns_name();
    test_firewall_and_sockets();
    return 0;
}
