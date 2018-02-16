#ifndef PIT_H
#define PIT_H 1

extern unsigned long ticks;

void pit_init();

void _sleep(unsigned long miliseconds);

#endif
