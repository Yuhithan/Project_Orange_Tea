#include "net/stack.h"

int ortos_net_register_interface(const ortos_net_interface_t *interface)
{
    (void)interface;
    return ORTOS_NET_ERR_NOT_SUPPORTED;
}

int ortos_net_send_ipv4(const ortos_ip_address_t *destination,
                        const void *payload, size_t length)
{
    (void)destination;
    (void)payload;
    (void)length;
    return ORTOS_NET_ERR_NO_INTERFACE;
}

int ortos_net_send_ipv6(const ortos_ip_address_t *destination,
                        const void *payload, size_t length)
{
    (void)destination;
    (void)payload;
    (void)length;
    return ORTOS_NET_ERR_NO_INTERFACE;
}

int ortos_net_socket_open(int protocol)
{
    (void)protocol;
    return ORTOS_NET_ERR_NOT_SUPPORTED;
}

int ortos_net_firewall_allows(uint16_t source_port, uint16_t destination_port,
                              int inbound)
{
    (void)source_port;
    (void)destination_port;
    (void)inbound;
    return 0;
}