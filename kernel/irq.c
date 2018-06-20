#include <assert.h>
#include <stdint.h>

#include <console.h>
#include <irq.h>
#include <cpu.h>
#include <isr.h>
#include <pic.h>

// TODO: handle spurious IRQs correctly

// 16 IRQs 8 irq handlers depth
typedef struct {
	irq_handler_t handler;
	void *extra;
} irq_entry_t;

static irq_entry_t irq_handlers[16][8];

// FIXME: rename to irq_add_handler ?
void irq_set_handler(unsigned int irq, irq_handler_t irq_handler, void *extra) {
//	printf("irq_set_handler(irq: %u, irq_handler: 0x%x, extra: 0x%x)\n", irq, (uintptr_t)irq_handler, (uintptr_t)extra);
	assert(irq_handler != NULL);
	// assert(extra != NULL);
	for (unsigned int i = 0; i < 8; i++) {
		if (irq_handlers[irq][i].handler == NULL) {
			irq_handlers[irq][i].handler = irq_handler;
			irq_handlers[irq][i].extra = extra;
			return;
		}
	}
	// failed to find a slot, most likely fatal
	assert(0);
}

void irq_ack(unsigned int irq) {
	// FIXME: does this handle spurious IRQ7's correctly ?
	pic_send_eoi(irq);
}

static void irq_handler(registers_t *regs) {
	assert((regs->isr_num >= isr_from_irq(0)) && (regs->isr_num <= isr_from_irq(16)));
	unsigned int irq = regs->err_code;

	irq_entry_t *entries = irq_handlers[irq];
	for (unsigned int i = 0; i < 8; i++) {
		if (entries[i].handler != NULL) {
			unsigned int status = entries[i].handler(irq, entries[i].extra);
			// FIXME: irq_ack after it has been handled instead of every handler ?
			if (status != IRQ_IGNORED) {
				// handled
				return;
			}
		} else {
			break;
		}
	}

	if (irq == 7) {
		// spurious irq
		// TODO: handle
		return;
	}

	if (irq == 15) {
		// spurious irq
		// TODO: handle
		return;
	}

	printf("unhandled irq %u!\n", irq);
	assert(0);
}

void irq_init(void) {
	for (unsigned int i = 0; i < 16; i++) {
		isr_set_handler(isr_from_irq(i), irq_handler);
	}
}
