#ifndef PIC_H
#define PIC_H 1

#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_COMMAND PIC1
#define PIC1_DATA (PIC1+1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA (PIC2+1)

#define PIC_EOI 0x20

void pic_init();


#define pic_get_irq_from_isr(isr) ((unsigned int)(((unsigned int)isr)-32))
void pic_send_eoi(unsigned int irq);

#endif
