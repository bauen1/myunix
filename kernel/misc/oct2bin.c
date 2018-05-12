unsigned int oct2bin(unsigned char *str, int size) {
	int n = 0;
	while (size-- > 0) {
		n *= 8;
		n += *str - '0';
		str++;
	}
	return n;
}
