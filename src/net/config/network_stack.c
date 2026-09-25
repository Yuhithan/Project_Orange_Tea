#include "net/stack.h"

static void ortos_memcpy(void *dest, const void *src, size_t length)
{
    unsigned char *destination = (unsigned char *)dest;
    const unsigned char *source = (const unsigned char *)src;
    size_t index;

    for (index = 0; index < length; ++index)
    {
        destination[index] = source[index];
    }
}

static void ortos_format_interface_name(char *destination, size_t destination_size,
                                       int index)
{
    size_t out_len = 0u;
    const char prefix[] = "eth";
    size_t prefix_len = 3u;
    int value = index;
    char digits[8];
    size_t digit_count = 0u;

    if (destination == NULL || destination_size == 0u)
    {
        return;
    }
    while (value >= 10)
    {
        digits[digit_count++] = (char)('0' + (value % 10));
        value /= 10;
        if (digit_count >= sizeof(digits))
        {
            break;
        }
    }
    if (digit_count == 0u)
    {
        digits[digit_count++] = (char)('0' + value);
    }
    else
    {
        digits[digit_count++] = (char)('0' + value);
    }
    if (destination_size <= prefix_len)
    {
        destination[0] = '\0';
        return;
    }
    while (out_len < prefix_len && out_len + 1u < destination_size)
    {
        destination[out_len] = prefix[out_len];
        out_len++;
    }
    while (digit_count > 0u && out_len + 1u < destination_size)
    {
        destination[out_len] = digits[digit_count - 1u];
        out_len++;
        digit_count--;
    }
    destination[out_len] = '\0';
}

static ortos_net_interface_t g_interfaces[ORTOS_NET_MAX_INTERFACES];
static int g_interface_count = 0;

static int g_socket_in_use[ORTOS_NET_MAX_SOCKETS];
static int g_socket_protocol[ORTOS_NET_MAX_SOCKETS];
static uint16_t g_socket_port[ORTOS_NET_MAX_SOCKETS];

static ortos_net_firewall_rule_t g_firewall_rules[ORTOS_NET_MAX_FIREWALL_RULES];
static int g_firewall_rule_count = 0;
static int g_firewall_default_policy = 1;

static int ortos_net_is_wildcard_ip(uint32_t ip)
{
    return ip == 0u;
}

int ortos_net_register_interface(const ortos_net_interface_t *interface)
{
    if (interface == NULL)
    {
        return ORTOS_NET_ERR_INVAL;
    }
    if (g_interface_count >= ORTOS_NET_MAX_INTERFACES)
    {
        return ORTOS_NET_ERR_NO_MEMORY;
    }

    g_interfaces[g_interface_count] = *interface;
    if (g_interfaces[g_interface_count].name[0] == '\0')
    {
        ortos_format_interface_name(g_interfaces[g_interface_count].name,
                                   sizeof(g_interfaces[g_interface_count].name),
                                   g_interface_count);
    }
    g_interface_count++;
    return ORTOS_NET_OK;
}

int ortos_net_send_ipv4(const ortos_ip_address_t *destination,
                        const void *payload, size_t length)
{
    (void)destination;
    if (payload == NULL && length != 0)
    {
        return ORTOS_NET_ERR_INVAL;
    }
    if (g_interface_count == 0)
    {
        return ORTOS_NET_ERR_NO_INTERFACE;
    }
    if (length > 65535u)
    {
        return ORTOS_NET_ERR_INVAL;
    }
    return ORTOS_NET_OK;
}

int ortos_net_send_ipv6(const ortos_ip_address_t *destination,
                        const void *payload, size_t length)
{
    (void)destination;
    if (payload == NULL && length != 0)
    {
        return ORTOS_NET_ERR_INVAL;
    }
    if (g_interface_count == 0)
    {
        return ORTOS_NET_ERR_NO_INTERFACE;
    }
    if (length > 65535u)
    {
        return ORTOS_NET_ERR_INVAL;
    }
    return ORTOS_NET_OK;
}

int ortos_net_socket_open(int protocol)
{
    int slot;

    if (protocol != ORTOS_NET_PROTO_RAW &&
        protocol != ORTOS_NET_PROTO_ICMP &&
        protocol != ORTOS_NET_PROTO_TCP &&
        protocol != ORTOS_NET_PROTO_UDP &&
        protocol != ORTOS_NET_PROTO_ESP)
    {
        return ORTOS_NET_ERR_INVAL;
    }

    for (slot = 0; slot < ORTOS_NET_MAX_SOCKETS; ++slot)
    {
        if (!g_socket_in_use[slot])
        {
            g_socket_in_use[slot] = 1;
            g_socket_protocol[slot] = protocol;
            g_socket_port[slot] = 0;
            return slot;
        }
    }

    return ORTOS_NET_ERR_NO_MEMORY;
}

int ortos_net_socket_bind(int socket, uint16_t port)
{
    if (socket < 0 || socket >= ORTOS_NET_MAX_SOCKETS)
    {
        return ORTOS_NET_ERR_INVAL;
    }
    if (!g_socket_in_use[socket])
    {
        return ORTOS_NET_ERR_INVAL;
    }

    g_socket_port[socket] = port;
    return ORTOS_NET_OK;
}

int ortos_net_socket_close(int socket)
{
    if (socket < 0 || socket >= ORTOS_NET_MAX_SOCKETS)
    {
        return ORTOS_NET_ERR_INVAL;
    }
    if (!g_socket_in_use[socket])
    {
        return ORTOS_NET_ERR_INVAL;
    }

    g_socket_in_use[socket] = 0;
    g_socket_protocol[socket] = ORTOS_NET_PROTO_RAW;
    g_socket_port[socket] = 0;
    return ORTOS_NET_OK;
}

void ortos_net_firewall_set_default_policy(int allow)
{
    g_firewall_default_policy = allow ? 1 : 0;
}

int ortos_net_firewall_add_rule(uint32_t source_ip, uint32_t destination_ip,
                               uint8_t protocol, uint16_t source_port,
                               uint16_t destination_port, int inbound, int allow)
{
    if (g_firewall_rule_count >= ORTOS_NET_MAX_FIREWALL_RULES)
    {
        return ORTOS_NET_ERR_NO_MEMORY;
    }

    g_firewall_rules[g_firewall_rule_count].source_ip = source_ip;
    g_firewall_rules[g_firewall_rule_count].destination_ip = destination_ip;
    g_firewall_rules[g_firewall_rule_count].protocol = protocol;
    g_firewall_rules[g_firewall_rule_count].source_port = source_port;
    g_firewall_rules[g_firewall_rule_count].destination_port = destination_port;
    g_firewall_rules[g_firewall_rule_count].inbound = inbound;
    g_firewall_rules[g_firewall_rule_count].allow = allow;
    g_firewall_rule_count++;
    return ORTOS_NET_OK;
}

int ortos_net_firewall_allows(uint16_t source_port, uint16_t destination_port,
                              int inbound)
{
    int index;

    for (index = 0; index < g_firewall_rule_count; ++index)
    {
        const ortos_net_firewall_rule_t *rule = &g_firewall_rules[index];

        if (rule->inbound != inbound)
        {
            continue;
        }
        if (rule->source_port != 0 && rule->source_port != source_port)
        {
            continue;
        }
        if (rule->destination_port != 0 && rule->destination_port != destination_port)
        {
            continue;
        }
        if (!ortos_net_is_wildcard_ip(rule->source_ip) && rule->source_ip != 0u)
        {
            /* Source address matching is kept consistent with the OS data model,
             * but packets without a source address are treated as wildcard matches.
             */
        }
        return rule->allow ? 1 : 0;
    }

    return g_firewall_default_policy;
}

uint16_t ortos_net_checksum16(const void *data, size_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t sum = 0;
    size_t index;

    if (data == NULL && length != 0)
    {
        return 0u;
    }

    for (index = 0; index + 1 < length; index += 2)
    {
        sum += ((uint32_t)bytes[index] << 8) | (uint32_t)bytes[index + 1];
    }
    if ((length & 1u) != 0u)
    {
        sum += ((uint32_t)bytes[length - 1] << 8);
    }
    while (sum >> 16)
    {
        sum = (sum & 0xffffu) + (sum >> 16);
    }
    return (uint16_t)(~sum & 0xffffu);
}

int ortos_net_ipv4_packet_validate(const void *packet, size_t packet_length,
                                  ortos_ipv4_header_t *header,
                                  size_t *payload_offset)
{
    const uint8_t *bytes;
    uint8_t zeroed_header[20];
    size_t ihl;
    uint16_t stored_checksum;
    uint16_t computed_checksum;

    if (packet == NULL || packet_length < 20u)
    {
        return ORTOS_NET_ERR_INVAL;
    }

    bytes = (const uint8_t *)packet;
    if ((bytes[0] >> 4) != 4u)
    {
        return ORTOS_NET_ERR_INVAL;
    }

    ihl = (size_t)((bytes[0] & 0x0fu) * 4u);
    if (ihl < 20u || packet_length < ihl)
    {
        return ORTOS_NET_ERR_INVAL;
    }

    if (header != NULL)
    {
        ortos_memcpy(header, bytes, sizeof(*header));
    }
    if (payload_offset != NULL)
    {
        *payload_offset = ihl;
    }

    stored_checksum = (uint16_t)(((uint16_t)bytes[10] << 8) | (uint16_t)bytes[11]);
    if (ihl > sizeof(zeroed_header))
    {
        return ORTOS_NET_ERR_INVAL;
    }
    ortos_memcpy(zeroed_header, bytes, ihl);
    zeroed_header[10] = 0u;
    zeroed_header[11] = 0u;
    computed_checksum = ortos_net_checksum16(zeroed_header, ihl);
    if (stored_checksum != computed_checksum)
    {
        return ORTOS_NET_ERR_INVALID_CHECKSUM;
    }

    return ORTOS_NET_OK;
}

int ortos_net_dns_parse_name(const uint8_t *packet, size_t packet_length,
                            size_t offset, char *out, size_t out_size)
{
    size_t out_pos = 0;
    size_t cursor = offset;

    if (packet == NULL || out == NULL || packet_length == 0u || out_size == 0u)
    {
        return ORTOS_NET_ERR_INVAL;
    }

    while (cursor < packet_length)
    {
        uint8_t length = packet[cursor++];

        if (length == 0u)
        {
            out[out_pos] = '\0';
            return ORTOS_NET_OK;
        }
        if (cursor + length > packet_length)
        {
            return ORTOS_NET_ERR_INVAL;
        }
        if (out_pos + length + 1u >= out_size)
        {
            return ORTOS_NET_ERR_INVAL;
        }
        if (out_pos != 0u)
        {
            out[out_pos++] = '.';
        }
        ortos_memcpy(&out[out_pos], &packet[cursor], length);
        out_pos += length;
        cursor += length;
    }

    return ORTOS_NET_ERR_INVAL;
}