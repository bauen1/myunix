#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

int main(int argc, char *argv[]) {
	setvbuf(stdin, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);

	printf("hello from /init!\n");
	printf("argv: %p\n", argv);
	printf("argc: %i (0x%08x)\n", argc, argc);
	for (int i = 0; i < argc; i++) {
		printf("argv[%i]: '%s'\n", i, argv[i]);
	}

/*	printf("mmap: 0x%x\n", (uintptr_t)mmap(0, 0x2000, 3, MAP_ANONYMOUS, -1, 0));

	char **c = malloc(20*sizeof(char *));
	printf("allocating 20 times 10kb\n");
	for (int i = 0; i<20;i++) {
		c[i] = malloc(10*1024);
	}
	printf("freeing\n");
	for (int i = 0; i < 20; i++) {
		free(c[i]);
	}
	free(c);
*/
	while(1) {
		putchar(getchar());
	}

	return EXIT_FAILURE;
}
