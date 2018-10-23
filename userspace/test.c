#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[], char **envp) {
	printf("test: pid %u\n", getpid());
	printf("argv: %p\n", argv);
	printf("argc: %i (0x%08x)\n", argc, argc);
	for (int i = 0; i < argc; i++) {
		printf("argv[%i]: %p: '%s'\n", i, argv[i], argv[i]);
	}
	for (char **env = envp; *env != 0; env++) {
		char *v = *env;
		printf("%s\n", v);
	}

	return EXIT_SUCCESS;
}
