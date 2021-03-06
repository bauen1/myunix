#include <stddef.h>

char * utoa(unsigned int value, char *str, int base, int width) {
	int i = 0;

	if ((base < 2) || (base > 36)) {
		str[0] = 0;
		return NULL;
	}

	do {
		str[i++] = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz"[35 + value % base];
		value = value / base;
	} while (value != 0);

	for (; i < width; i++) {
		str[i] = '0';
	}

	str[i] = 0;

	i--;
	for (int j = 0; j < i; j++, i--) {
		char c = str[j];
		str[j] = str[i];
		str[i] = c;
	}
	return str;
}

char * itoa(int num, char *str, int base, int width) {
	if ((base == 10) && (num < 0)) {
		str[0] = '-';
		return utoa((unsigned)-num, str + 1, base, width);
	} else {
		return utoa((unsigned)num, str, base, width);
	}

}
