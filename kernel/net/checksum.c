/* checksum helpers */

#include <stdint.h>
#include <stddef.h>

#include <net/checksum.h>
/* ntohs macros */
#include <net/net.h>

__attribute__((pure)) uint16_t net_calc_checksum(const void *data, size_t size) {
	uint32_t v = 0;
	const uint16_t *s = (uint16_t *)data;
	for (size_t i = 0; i < (size / sizeof(uint16_t)); i++) {
		v += ntohs(s[i]);
	}

	v = ((v >> 16) & 0xFFFF) + (v & 0xFFFF);
	v += (v >> 16) & 0xFFFF;

	return (uint16_t)~v;
}

__attribute__((pure)) uint16_t net_calc_checksum2(const void *data1, size_t size1, const void *data2, size_t size2, uint16_t checksum) {
	assert((size1 % 2) == 0);

	uint32_t v = 0;
	{
		const uint16_t *s = (uint16_t *)data1;
		for (size_t i = 0; i < (size1 / sizeof(uint16_t)); i++) {
			v += ntohs(s[i]);
		}
	}
	if (v > 0xFFFF) {
		v = ((v & 0xFFFF0000) >> 16) + (v & 0xFFFF);
	}
	{
		const uint16_t *s = (uint16_t *)data2;
		for (size_t i = 0; i < (size2 / sizeof(uint16_t)); i++) {
			v += ntohs(s[i]);
		}
		v -= checksum;

		if (size2 % sizeof(uint16_t) != 0) {
			uint8_t *t = (uint8_t *)data2;
			uint8_t tmp[2];
			tmp[0] = t[(size2 / sizeof(uint16_t)) * sizeof(uint16_t)];
			tmp[1] = 0;
			uint16_t *f = (uint16_t *)tmp;
			v += ntohs(f[0]);
		}
	}
	v = ((v >> 16) & 0xFFFF) + (v & 0xFFFF);
	v += (v >> 16) & 0xFFFF;

	return (uint16_t)~v;
}
