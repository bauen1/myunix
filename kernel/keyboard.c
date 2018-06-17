/* keyboard.c simple keyboard driver */
#include <keyboard.h>

#include <console.h>
#include <cpu.h>
#include <isr.h>
#include <keyboard.h>
#include <pic.h>
#include <ringbuffer.h>

static unsigned char keyboard_map[128] = {
  0, 27,
  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
  '-', '=',
  '\b', // backspace
  '\t', // tab
  'q', 'w', 'e', 'r',
  't', 'y', 'u', 'i', 'o', 'p',
  '[', ']', '\n', // newline
  0, // control
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
  '\'', '`', 0,
  '\\', 'z', 'x', 'c', 'v', 'b', 'n',
  'm', ',', '.', '/', 0,
  '*',
  0, // alt
  ' ', // space
  0, // caps lock
  0, // f1
  0, // f2
  0, // f3
  0, // f4
  0, // f5
  0, // f6
  0, // f7
  0, // f8
  0, // f9
  0, // f10
  0, // numlock
  0, // scroll lock
  0, // home key
  0, // up arrow
  0, // page up
  '-',
  0, // left arrow
  0,
  0, // right arrow
  '+',
  0, // end key
  0, // down arrow
  0, // page down
  0, // insert key
  0, // delete key
  0, 0, 0,
  0, // f11
  0, // f12
  0, // TODO: implement the rest
  // 92:
  0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

#define KEYBUFFER_LENGTH 1024
static ringbuffer_t keyboard_ringbuffer;
static unsigned char keybuffer[KEYBUFFER_LENGTH];

static void irq1_handler(registers_t *regs, void *extra) {
	uint8_t status = inb(0x64);
	if (status & 0x01) {
		char keycode = inb(0x60);
		if (keycode < 0) {
			irq_ack(regs->isr_num);
			return;
		}
		unsigned char c = keyboard_map[(unsigned int)keycode];
		ringbuffer_write_byte(&keyboard_ringbuffer, c);
	}

	irq_ack(regs->isr_num);
}

void keyboard_init() {
	ringbuffer_init(&keyboard_ringbuffer, keybuffer, KEYBUFFER_LENGTH);
	isr_set_handler(isr_from_irq(1), irq1_handler, NULL);
}

unsigned char keyboard_getc() {
	return ringbuffer_read_byte(&keyboard_ringbuffer);
}

