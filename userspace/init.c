#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>

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
	pid_t cpid = fork();
	if (cpid == -1) {
		perror("fork");
	} else if (cpid == 0) {
		// child
		printf("%s: child (pid: %u)\n", __func__, getpid());
		_exit(1111);
	} else {
		int wstatus;
		do {
			pid_t w = waitpid(cpid, &wstatus, WUNTRACED | WCONTINUED);
			if (w == -1) {
				perror("waitpid");
				break;
			}
			if (WIFEXITED(wstatus)) {
				printf("child exited, status=%d\n", WEXITSTATUS(wstatus));
			} else if (WIFSIGNALED(wstatus)) {
				printf("child killed by signal=%d\n", WTERMSIG(wstatus));
			} else if (WIFSTOPPED(wstatus)) {
				printf("child stopped by signal=%d\n", WSTOPSIG(wstatus));
			} else if (WIFCONTINUED(wstatus)) {
				printf("child continued\n");
			}
		} while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));
	}

	printf("putenv: %d\n", putenv("TEST=1234"));
	printf("setenv: %d\n", setenv("HOME", "/", 1));
	printf("setenv2: %d\n", setenv("HI", "hello guys", 1));
	printf("env['HI']: '%s'\n", getenv("HI"));
	const char *a[] = {"/sh", NULL};
	execv(a[0], a);
	printf("Could not run /sh !\nLooping forever ...\n");
	while (1) {
		// ...
	};
}
