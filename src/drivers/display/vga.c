#include "drivers/vga.h"

#include "drivers/io.h"

#define VGA_COLUMNS 80
#define VGA_ROWS 25
#define VGA_MEMORY ((volatile uint16_t *)0xB8000)

void vga_set_cursor(size_t column, size_t row)
{
    if (column >= VGA_COLUMNS) column = VGA_COLUMNS - 1;
    if (row >= VGA_ROWS) row = VGA_ROWS - 1;

    uint16_t position = (uint16_t)(row * VGA_COLUMNS + column);
    io_outb(0x3D4, 0x0E);
    io_outb(0x3D5, (uint8_t)(position >> 8));
    io_outb(0x3D4, 0x0F);
    io_outb(0x3D5, (uint8_t)position);
}

void vga_init(void)
{
    for (size_t index = 0; index < VGA_COLUMNS * VGA_ROWS; index++)
        VGA_MEMORY[index] = (uint16_t)(0x7000u | ' ');

    io_outb(0x3D4, 0x0A);
    io_outb(0x3D5, 0x0E);
    io_outb(0x3D4, 0x0B);
    io_outb(0x3D5, 0x0F);
    vga_set_cursor(0, 0);
}
