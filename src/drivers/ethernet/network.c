#include "network.h"
#include "net/stack.h"
#include "rtl8139.h"
#include <stddef.h>

static size_t ortos_strlen(const char *text)
{
    size_t length = 0u;

    if (text == NULL)
    {
        return 0u;
    }
    while (text[length] != '\0')
    {
        length++;
    }
    return length;
}

static void ortos_strcpy(char *destination, const char *source)
{
    size_t index = 0u;

    if (destination == NULL || source == NULL)
    {
        return;
    }
    while (source[index] != '\0')
    {
        destination[index] = source[index];
        index++;
    }
    destination[index] = '\0';
}

static int network_enabled = 0;
static int ethernet_available = 0;
static int wifi_supported = 0;
static int wifi_connected = 0;
static char wifi_ssid[32];
static ortos_ethernet_device_t ethernet_device;

void enable_network(void)
{
    network_enabled = 1;
    ethernet_available = rtl8139_init(&ethernet_device) == 0;
    wifi_supported = 0;
    wifi_connected = 0;
    wifi_ssid[0] = '\0';

    if (ethernet_available)
    {
        ortos_net_interface_t interface = {0};
        for (size_t index = 0; index < sizeof(interface.address); ++index)
        {
            interface.address[index] = ethernet_device.address[index];
        }
        interface.mtu = ethernet_device.mtu;
        interface.up = ethernet_device.link_up;
        interface.type = ORTOS_NET_IFACE_ETHERNET;
        interface.name[0] = 'e'; interface.name[1] = 't'; interface.name[2] = 'h';
        interface.name[3] = '0'; interface.name[4] = '\0';
        (void)ortos_net_register_interface(&interface);
    }
}

int network_has_ethernet(void)
{
    return network_enabled && ethernet_available;
}

int network_is_wifi_connected(void)
{
    return network_enabled && wifi_supported && wifi_connected;
}

const char* network_get_wifi_ssid(void)
{
    return wifi_connected ? wifi_ssid : "";
}

int network_set_ethernet_enabled(int enabled)
{
    if (!network_enabled)
    {
        return NETWORK_ERR_INVAL;
    }
    ethernet_available = enabled ? 1 : 0;
    return NETWORK_OK;
}

int network_set_wifi_capability(int supported)
{
    if (!network_enabled)
    {
        return NETWORK_ERR_INVAL;
    }
    wifi_supported = supported ? 1 : 0;
    if (!wifi_supported)
    {
        wifi_connected = 0;
        wifi_ssid[0] = '\0';
    }
    return NETWORK_OK;
}

int network_get_link_status(int *ethernet_up, int *wifi_up)
{
    if (ethernet_up != NULL)
    {
        *ethernet_up = network_has_ethernet();
    }
    if (wifi_up != NULL)
    {
        *wifi_up = network_is_wifi_connected();
    }
    return NETWORK_OK;
}

int network_connect_wifi(const char* ssid)
{
    if (!network_enabled || ssid == NULL || ssid[0] == '\0')
    {
        return NETWORK_ERR_INVAL;
    }

    if (!wifi_supported)
    {
        return NETWORK_ERR_NOT_SUPPORTED;
    }

    if (ortos_strlen(ssid) >= sizeof(wifi_ssid))
    {
        return NETWORK_ERR_INVAL;
    }

    ortos_strcpy(wifi_ssid, ssid);
    wifi_connected = 1;
    return NETWORK_OK;
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
        if (!wifi_supported)
        {
            return NETWORK_ERR_NOT_SUPPORTED;
        }
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
