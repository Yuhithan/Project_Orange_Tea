#pragma once

#include <stddef.h>
#include <stdint.h>

void enable_network(void);
int network_has_ethernet(void);
int network_is_wifi_connected(void);
const char* network_get_wifi_ssid(void);
enum {
	NETWORK_OK = 0,
	NETWORK_ERR_INVAL = -1,
	NETWORK_ERR_NOT_CONNECTED = -2,
	NETWORK_ERR_NOT_SUPPORTED = -3
};
int network_connect_wifi(const char* ssid);
void network_disconnect_wifi(void);
int network_ping(const char* host, int use_wifi);

/* Network configuration and diagnostics for the generic OS runtime. */
int network_set_ethernet_enabled(int enabled);
int network_set_wifi_capability(int supported);
int network_get_link_status(int *ethernet_up, int *wifi_up);
