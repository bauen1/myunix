#include <stddef.h>

#include <ringbuffer.h>
#include <cpu.h>

static size_t ringbuffer_unread(ringbuffer_t *ringbuffer) {
	if (ringbuffer->write_head < ringbuffer->read_head) {
		return (ringbuffer->size - ringbuffer->read_head) + ringbuffer->write_head;
	} else /*if (ringbuffer->write_head >= ringbuffer->read_head)*/ {
		return ringbuffer->write_head - ringbuffer->read_head;
	}
}

ringbuffer_t *ringbuffer_init(ringbuffer_t *ringbuffer, unsigned char *buffer, size_t size) {
	ringbuffer->buffer = buffer;
	ringbuffer->size = size;
	ringbuffer->write_head = 0;
	ringbuffer->read_head = 0;
	return ringbuffer;
}
unsigned char ringbuffer_read_byte(ringbuffer_t *ringbuffer) {
	while (ringbuffer_unread(ringbuffer) <= 0) {
		hlt();
	}
	unsigned char c = ringbuffer->buffer[ringbuffer->read_head];
	ringbuffer->read_head += 1;
	ringbuffer->read_head %= ringbuffer->size;
	return c;
}

void ringbuffer_write_byte(ringbuffer_t *ringbuffer, unsigned char c) {
	ringbuffer->buffer[ringbuffer->write_head] = c;
	ringbuffer->write_head += 1;
	ringbuffer->write_head %= ringbuffer->size;
}
