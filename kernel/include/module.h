#ifndef MODULE_H
#define MODULE_H 1

#include <boot.h>

typedef struct {
	volatile char *name;
	void (* init)(void);
} module_info_t;

#define MODULE_INFO(n, i) \
	__attribute__((used)) __attribute__((section("mod_info"))) module_info_t module_info_##n = { \
		.name = #n, \
		.init = &i \
	}

void modules_init(void);

#endif
