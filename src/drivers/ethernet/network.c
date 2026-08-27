#include "network.h"
#include "imp.h"
#include <stddef.h>

static int network_enabled = 0;
static int ethernet_available = 1;
static int wifi_connected = 0;
static char wifi_ssid[32];

static void network_copy_string(char* dst, const char* src, int max_len)
{
    int i = 0;
    while (src[i] != '\0' && i < max_len - 1)
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void enable_network(void)
{
    network_enabled = 1;
    ethernet_available = 1;
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

void network_connect_wifi(const char* ssid)
{
    if (!network_enabled || ssid == NULL || ssid[0] == '\0')
    {
        return;
    }

    network_copy_string(wifi_ssid, ssid, sizeof(wifi_ssid));
    wifi_connected = 1;
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
        return 0;
    }

    if (use_wifi)
    {
        if (!wifi_connected)
        {
            return 0;
        }
    }
    else
    {
        if (!ethernet_available)
        {
            return 0;
        }
    }

    imp_text("Pinging ");
    imp_text(host);
    imp_text(use_wifi ? " over Wi-Fi...\n" : " over Ethernet...\n");
    imp_text("Reply from ");
    imp_text(host);
    imp_text(": bytes=32 time=1ms TTL=64\n");

    return 1;
}
