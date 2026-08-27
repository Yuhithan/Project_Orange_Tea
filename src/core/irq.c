#include "irq.h"
#include "timer.h"
#include "keyboard.h"
#include "mouse.h"
#include "imp.h"
#include <stdint.h>

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t attributes;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_pointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static irq_handler_t handlers[256];

extern void isr0(void); extern void isr1(void); extern void isr2(void); extern void isr3(void);
extern void isr4(void); extern void isr5(void); extern void isr6(void); extern void isr7(void);
extern void isr8(void); extern void isr9(void); extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);
extern void irq0_stub(void); extern void irq1_stub(void); extern void irq12_stub(void);

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static void idt_set_gate(uint8_t vector, void (*handler)(void))
{
    uint64_t address = (uint64_t)(uintptr_t)handler;
    idt[vector].offset_low = (uint16_t)address;
    idt[vector].selector = 0x08;
    idt[vector].ist = 0;
    idt[vector].attributes = 0x8E;
    idt[vector].offset_middle = (uint16_t)(address >> 16);
    idt[vector].offset_high = (uint32_t)(address >> 32);
    idt[vector].reserved = 0;
}

static void pic_remap(void)
{
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    /* PIT, PS/2 keyboard, cascade (IRQ2), and PS/2 mouse (IRQ12) are active. */
    outb(0x21, 0xF8);
    outb(0xA1, 0xEF);
}

void irq_init(void)
{
    void (*exceptions[32])(void) = { isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
        isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15, isr16, isr17,
        isr18, isr19, isr20, isr21, isr22, isr23, isr24, isr25, isr26, isr27,
        isr28, isr29, isr30, isr31 };
    for (int i = 0; i < 256; i++) { handlers[i] = 0; idt[i].attributes = 0; }
    for (int i = 0; i < 32; i++) idt_set_gate((uint8_t)i, exceptions[i]);
    idt_set_gate(32, irq0_stub);
    idt_set_gate(33, irq1_stub);
    idt_set_gate(44, irq12_stub);
    pic_remap();
    struct idt_pointer pointer = { sizeof(idt) - 1, (uint64_t)(uintptr_t)idt };
    __asm__ volatile ("lidt %0" : : "m"(pointer));
    __asm__ volatile ("sti");
}

void irq_register_handler(uint8_t vector, irq_handler_t handler) { handlers[vector] = handler; }

void irq_dispatch(uint8_t vector)
{
    if (handlers[vector]) handlers[vector](vector);
    else if (vector == 32) timer_handler();
    else if (vector == 33) keyboard_handle_irq();
    else if (vector == 44) mouse_handle_irq();
    if (vector >= 40 && vector < 48) outb(0xA0, 0x20);
    if (vector >= 32 && vector < 48) outb(0x20, 0x20);
}

void irq_exception(uint64_t vector, uint64_t error_code)
{
    imp_text("EXCEPTION vector="); imp_uint64_dec(vector);
    imp_text(" error=0x"); imp_uint64_hex(error_code); imp_text("\nSystem halted.\n");
    __asm__ volatile ("cli");
    for (;;) __asm__ volatile ("hlt");
}

void irq0_handler(void)
{
    irq_dispatch(32);
}
