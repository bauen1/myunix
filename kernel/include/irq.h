#ifndef IRQ_H
#define IRQ_H 1

#define IRQ_IGNORED 0
#define IRQ_HANDLED 1

typedef unsigned int (*irq_handler_t)(unsigned int irq, void *extra);

void irq_set_handler(unsigned int irq, irq_handler_t irq_handler, void *extra);

void irq_ack(unsigned int irq);

void irq_init(void);

#endif
