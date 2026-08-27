#pragma once

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
