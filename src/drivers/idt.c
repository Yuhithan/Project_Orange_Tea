#include <stddef.h>
#include <stdint.h>
#include "x86_64/gdt.h"
#include "x86_64/idt.h"
#include "x86_64/pic.h"

#define IDT_IRQ0_TIMER 0x20
#define IDT_IRQ1_KEYBOARD 0x21

#define IDT_GATE_PRESENT (1 << 7)
#define IDT_GATE_DPL0 (0b00 << 5)
#define IDT_GATE_DPL1 (0b01 << 5)
#define IDT_GATE_DPL2 (0b10 << 5)
#define IDT_GATE_DPL3 (0b11 << 5)
#define IDT_GATE_TYPE_INTERRUPT 0xE

#define IDT_ENTRY_TYPE_INTERRUPT (IDT_GATE_PRESENT | IDT_GATE_DPL0 | IDT_GATE_TYPE_INTERRUPT)

struct IdtEntry {
	uint16_t offset_low;
	uint16_t selector;
	uint8_t  ist;
	uint8_t  type;
	uint16_t offset_mid;
	uint32_t offset_high;
	uint32_t reserved;
} __attribute__((packed));

struct IdtPtr {
	uint16_t limit;
	uint64_t base;
} __attribute__((packed));

struct IdtEntry idt[256] __attribute__((aligned(16)));
struct IdtPtr idt_ptr;

void (*idt_handler_keyboard_user)();

extern void idt_load(struct IdtPtr* idt_ptr);
extern void isr0(void); extern void isr1(void); extern void isr2(void); extern void isr3(void);
extern void isr4(void); extern void isr5(void); extern void isr6(void); extern void isr7(void);
extern void isr8(void); extern void isr9(void); extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);
extern void idt_handler_keyboard_wrapped(void);

void idt_set_entry(uint8_t vector, uint64_t isr_addr, uint16_t selector, uint8_t type) {
	idt[vector] = (struct IdtEntry) {
		.offset_low = (uint16_t) (isr_addr >> 0),
		.selector = selector,
		.ist = 0,
		.type = type,
		.offset_mid = (uint16_t) (isr_addr >> 16),
		.offset_high = (uint32_t) (isr_addr >> 32),
		.reserved = 0,
	};
}

void idt_handler_keyboard() {
	if (idt_handler_keyboard_user != NULL) {
		idt_handler_keyboard_user();
	}
	
	pic_eoi_master();
}

void idt_init() {
	asm volatile("cli");
	pic_remap();

	void (*exceptions[32])(void) = {
		isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
		isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
		isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
		isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
	};
	for (uint8_t vector = 0; vector < 32; vector++) {
		idt_set_entry(vector, (uint64_t)(uintptr_t)exceptions[vector],
			GDT_SELECTOR_CS_KERNEL, IDT_ENTRY_TYPE_INTERRUPT);
	}

	idt_ptr.limit = (sizeof(struct IdtEntry) * 256) - 1;
	idt_ptr.base = (uint64_t) &idt;

	idt_set_entry(IDT_IRQ1_KEYBOARD, (uint64_t)(uintptr_t)idt_handler_keyboard_wrapped,
		GDT_SELECTOR_CS_KERNEL, IDT_ENTRY_TYPE_INTERRUPT);

	idt_load(&idt_ptr);

	asm volatile("sti");
}

void idt_set_handler_keyboard(void (*handler)()) {
	idt_handler_keyboard_user = handler;
}
