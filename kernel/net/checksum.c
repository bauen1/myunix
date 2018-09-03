#include <stdint.h>
#include <stddef.h>

#include <net/checksum.h>
/* net/net.h for ntohs macro */
#include <net/net.h>

/* checksum helpers */
uint16_t net_calc_checksum(void *data, size_t size) {
	uint32_t v = 0;
	uint16_t *s = (uint16_t *)data;
	for (unsigned int i = 0; i < (size / sizeof(uint16_t)); i++) {
		v += ntohs(s[i]);
	}

	if (v > 0xFFFF) {
		v = ((v & 0xFFFF0000) >> 16) + (v & 0xFFFF);
	}

	uint16_t checksum = (~( v & 0xFFFF)) & 0xFFFF;
	return checksum;
}
