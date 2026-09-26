#include "rtl8139.h"

#include "io.h"

#define PCI_CONFIG_ADDRESS 0xcf8
#define PCI_CONFIG_DATA 0xcfc
#define RTL8139_VENDOR 0x10ecu
#define RTL8139_DEVICE 0x8139u
#define RTL8139_RX_BUFFER_SIZE 8192u
#define RTL8139_TX_BUFFER_SIZE 1536u

static uint16_t rtl8139_io_base;
static uint8_t rx_buffer[RTL8139_RX_BUFFER_SIZE + 16u + 1500u];
static uint8_t tx_buffers[4][RTL8139_TX_BUFFER_SIZE];
static uint16_t current_rx;
static uint8_t current_tx;

static uint32_t pci_read(uint8_t slot, uint8_t offset)
{
    uint32_t address = 0x80000000u | ((uint32_t)slot << 11) |
                       ((uint32_t)offset & 0xfcu);
    io_outl(PCI_CONFIG_ADDRESS, address);
    return io_inl(PCI_CONFIG_DATA);
}

static void copy_bytes(uint8_t *destination, const uint8_t *source, size_t length)
{
    for (size_t index = 0; index < length; ++index)
    {
        destination[index] = source[index];
    }
}

static int rtl8139_send(void *context, const void *frame, size_t length)
{
    uint16_t base = *(uint16_t *)context;
    uint8_t index = current_tx;

    if (frame == NULL || length < ORTOS_ETHERNET_MIN_FRAME_SIZE ||
        length > RTL8139_TX_BUFFER_SIZE)
    {
        return -1;
    }
    copy_bytes(tx_buffers[index], (const uint8_t *)frame, length);
    io_outl((uint16_t)(base + 0x20u + index * 4u), (uint32_t)(uintptr_t)tx_buffers[index]);
    io_outl((uint16_t)(base + 0x10u + index * 4u), (uint32_t)length);
    current_tx = (uint8_t)((current_tx + 1u) & 3u);
    return 0;
}

static int rtl8139_receive(void *context, void *frame, size_t capacity, size_t *length)
{
    uint16_t base = *(uint16_t *)context;
    uint16_t status;
    uint16_t received_length;

    if (frame == NULL || length == NULL || capacity == 0u)
    {
        return -1;
    }
    status = io_inw((uint16_t)(base + 0x36u));
    if (!(status & 1u))
    {
        return 1;
    }
    received_length = io_inw((uint16_t)(base + 0x30u + current_rx)) - 4u;
    if (received_length > capacity || received_length > ORTOS_ETHERNET_MAX_FRAME_SIZE)
    {
        return -1;
    }
    copy_bytes((uint8_t *)frame, rx_buffer + current_rx + 4u, received_length);
    current_rx = (uint16_t)((current_rx + received_length + 4u + 3u) & ~3u);
    io_outw((uint16_t)(base + 0x38u), (uint16_t)(current_rx - 16u));
    *length = received_length;
    return 0;
}

int rtl8139_init(ortos_ethernet_device_t *device)
{
    uint32_t identity;
    uint32_t command;
    uint32_t bar;
    static uint16_t context_base;

    if (device == NULL)
    {
        return -1;
    }
    for (uint8_t slot = 0; slot < 32u; ++slot)
    {
        identity = pci_read(slot, 0);
        if ((identity & 0xffffu) != RTL8139_VENDOR ||
            ((identity >> 16) & 0xffffu) != RTL8139_DEVICE)
        {
            continue;
        }
        bar = pci_read(slot, 0x10);
        if ((bar & 1u) == 0u || (bar & 0xfffcu) == 0u)
        {
            return -1;
        }
        rtl8139_io_base = (uint16_t)(bar & 0xfffcu);
        command = pci_read(slot, 4) | 0x5u;
        io_outl(PCI_CONFIG_ADDRESS, 0x80000000u | ((uint32_t)slot << 11) | 4u);
        io_outl(PCI_CONFIG_DATA, command);
        io_outb((uint16_t)(rtl8139_io_base + 0x37u), 0x10u);
        for (volatile unsigned int wait = 0; wait < 100000u; ++wait) { }
        io_outb((uint16_t)(rtl8139_io_base + 0x37u), 0x0cu);
        io_outl((uint16_t)(rtl8139_io_base + 0x30u), (uint32_t)(uintptr_t)rx_buffer);
        io_outl((uint16_t)(rtl8139_io_base + 0x44u), 0x0000000fu);
        io_outw((uint16_t)(rtl8139_io_base + 0x3cu), 0);
        context_base = rtl8139_io_base;
        for (uint8_t index = 0; index < 6u; ++index)
        {
            device->address[index] = io_inb((uint16_t)(rtl8139_io_base + index));
        }
        device->context = &context_base;
        device->mtu = 1500u;
        device->link_up = 1;
        device->name = "rtl8139";
        device->send = rtl8139_send;
        device->receive = rtl8139_receive;
        current_rx = 0;
        current_tx = 0;
        return 0;
    }
    return -1;
}