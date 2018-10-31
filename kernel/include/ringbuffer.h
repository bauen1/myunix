#ifndef RINGBUFFER_H
#define RINGBUFFER_H 1

#include <stdint.h>
#include <stddef.h>
#include <task.h>

typedef struct {
	unsigned char *buffer;
	size_t size;
	size_t write_head;
	size_t read_head;
	semaphore_t sem;
} ringbuffer_t;

ringbuffer_t *ringbuffer_init(ringbuffer_t *ringbuffer, unsigned char *buffer, size_t size);
size_t ringbuffer_unread(ringbuffer_t *ringbuffer);
unsigned char ringbuffer_read_byte(ringbuffer_t *ringbuffer);
void ringbuffer_write_byte(ringbuffer_t *ringbuffer, unsigned char c);

#endif
