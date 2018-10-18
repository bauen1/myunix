#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	printf("test: pid %u\n", getpid());
	printf("argv: %p\n", argv);
	printf("argc: %i (0x%08x)\n", argc, argc);
	for (int i = 0; i < argc; i++) {
		printf("argv[%i]: %p: '%s'\n", i, argv[i], argv[i]);
	}

	return EXIT_SUCCESS;
}
