#pragma once

void enable_network(void);
int network_has_ethernet(void);
int network_is_wifi_connected(void);
const char* network_get_wifi_ssid(void);
void network_connect_wifi(const char* ssid);
void network_disconnect_wifi(void);
int network_ping(const char* host, int use_wifi);
