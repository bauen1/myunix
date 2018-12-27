#ifndef CHECKSUM_H
#define CHECKSUM_H 1

#include <stdint.h>
#include <stddef.h>

__attribute__((pure)) uint16_t net_calc_checksum(const void *data, size_t size);
__attribute__((pure)) uint16_t net_calc_checksum2(const void *data1, size_t size1, const void *data2, size_t size2, uint16_t checksum);

#endif
