unsigned int oct2bin(char *str, unsigned int size) {
	unsigned int r = 0;
	for (unsigned int i = 0; (i < size) && (str[i] != 0); i++) {
		r = r << 3;
		if ((str[i] >= '0') && (str[i] <= '7')) {
			r += str[i] - '0';
		}
	}

	return r;
}
