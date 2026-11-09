#include "core/print.h"
#include "core/irq.h"
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

extern void isr0(void); extern void isr1(void); extern void isr2(void); extern void isr3(void);
extern void isr4(void); extern void isr5(void); extern void isr6(void); extern void isr7(void);
extern void isr8(void); extern void isr9(void); extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);

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

void irq_init(void)
{
    __asm__ volatile ("cli");

    void (*exceptions[32])(void) = { isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
        isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15, isr16, isr17,
        isr18, isr19, isr20, isr21, isr22, isr23, isr24, isr25, isr26, isr27,
        isr28, isr29, isr30, isr31 };
    for (int i = 0; i < 256; i++) idt[i].attributes = 0;
    for (int i = 0; i < 32; i++) idt_set_gate((uint8_t)i, exceptions[i]);

    struct idt_pointer pointer = { sizeof(idt) - 1, (uint64_t)(uintptr_t)idt };
    __asm__ volatile ("lidt %0" : : "m"(pointer));
}

void irq_exception(uint64_t vector, uint64_t error_code)
{
    print_str("EXCEPTION vector=");
    print_uint64_dec(vector);
    print_str(" error=0x");
    print_uint64_hex(error_code);
    print_str("\nSystem halted.\n");
    __asm__ volatile ("cli");
    for (;;) __asm__ volatile ("hlt");
}
