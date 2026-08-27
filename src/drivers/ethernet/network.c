#include "network.h"
#include <stddef.h>

static int network_enabled = 0;
static int ethernet_available = 0;
static int wifi_connected = 0;
static char wifi_ssid[32];

void enable_network(void)
{
    network_enabled = 1;
    ethernet_available = 0;
    wifi_connected = 0;
    wifi_ssid[0] = '\0';
}

int network_has_ethernet(void)
{
    return network_enabled && ethernet_available;
}

int network_is_wifi_connected(void)
{
    return network_enabled && wifi_connected;
}

const char* network_get_wifi_ssid(void)
{
    return wifi_connected ? wifi_ssid : "";
}

int network_connect_wifi(const char* ssid)
{
    if (!network_enabled || ssid == NULL || ssid[0] == '\0')
    {
        return NETWORK_ERR_INVAL;
    }
    (void)ssid;
    return NETWORK_ERR_NOT_SUPPORTED;
}

void network_disconnect_wifi(void)
{
    wifi_connected = 0;
    wifi_ssid[0] = '\0';
}

int network_ping(const char* host, int use_wifi)
{
    if (!network_enabled || host == NULL || host[0] == '\0')
    {
        return NETWORK_ERR_INVAL;
    }

    if (use_wifi)
    {
        if (!wifi_connected)
        {
            return NETWORK_ERR_NOT_CONNECTED;
        }
    }
    else
    {
        if (!ethernet_available)
        {
            return NETWORK_ERR_NOT_SUPPORTED;
        }
    }

    return NETWORK_ERR_NOT_SUPPORTED;
}
