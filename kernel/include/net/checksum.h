#ifndef CHECKSUM_H
#define CHECKSUM_H 1

#include <stdint.h>
#include <stddef.h>

uint16_t net_calc_checksum(void *data, size_t size);

#endif
