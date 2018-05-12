#ifndef ITOA_H
#define ITOA_H 1

// found in misc/itoa.c
char * utoa(unsigned int value, char *str, int base, int width);
char * itoa(int num, char *str, int base, int width);

#endif
