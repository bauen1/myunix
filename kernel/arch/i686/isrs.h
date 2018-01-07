#ifndef ISRS_H
#define ISRS_H 1

#include <stdint.h>

typedef struct registers {
	uint32_t gs __attribute__((packed));
	uint32_t fs __attribute__((packed));
	uint32_t es __attribute__((packed));
	uint32_t ds __attribute__((packed));

	uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax __attribute__((packed)); /* pushed by pusha */

	uint32_t isr_num __attribute__((packed));
	uint32_t err_code __attribute__((packed));

	uint32_t eip __attribute__((packed));
	uint32_t cs __attribute__((packed));
	uint32_t eflags __attribute__((packed));
	uint32_t usersp __attribute__((packed));
	uint32_t ss __attribute__((packed));
} __attribute__((packed)) registers_t;


// exceptions
extern void _isr0();
extern void _isr1();
extern void _isr2();
extern void _isr3();
extern void _isr4();
extern void _isr5();
extern void _isr6();
extern void _isr7();
extern void _isr8();
extern void _isr9();
extern void _isr10();
extern void _isr11();
extern void _isr12();
extern void _isr13();
extern void _isr14();
extern void _isr15();
extern void _isr16();
extern void _isr17();
extern void _isr18();
extern void _isr19();
extern void _isr20();
extern void _isr21();
extern void _isr22();
extern void _isr23();
extern void _isr24();
extern void _isr25();
extern void _isr26();
extern void _isr27();
extern void _isr28();
extern void _isr29();
extern void _isr30();
extern void _isr31();

// IRQs
extern void _isr32();
extern void _isr33();
extern void _isr34();
extern void _isr35();
extern void _isr36();
extern void _isr37();
extern void _isr38();
extern void _isr39();
extern void _isr40();
extern void _isr41();
extern void _isr42();
extern void _isr43();
extern void _isr44();
extern void _isr45();
extern void _isr46();
extern void _isr47();

// syscall
extern void _isr128();

#endif
