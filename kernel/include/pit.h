#ifndef PIT_H
#define PIT_H 1

#include <stdint.h>

#define FREQUENCY 1000

extern uint64_t timer_ticks;
void pit_init(void);

#endif
