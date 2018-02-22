#ifndef USERSPACE_H
#define USERSPACE_H 1

__attribute__((noreturn))
void __jump_to_userspace(void *stack, void *code, void *directory);
extern void __hello_userspace();

#endif
