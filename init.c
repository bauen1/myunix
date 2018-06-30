#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

int main(int argc, char *argv[]) {
	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);
	printf("hello from /init!\n");
	printf("argc: %i\n", argc);
	for (int i = 0; i < argc; i++) {
		printf("argv[%i]: '%s'\n", i, argv[i]);
	}
	while(1) {
		putchar(getchar());
	}
	return EXIT_FAILURE;
}
