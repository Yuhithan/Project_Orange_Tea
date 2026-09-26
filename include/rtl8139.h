#pragma once

#include "net/ethernet.h"

/* Initializes the first RTL8139 found on PCI bus 0 using polled I/O. */
int rtl8139_init(ortos_ethernet_device_t *device);