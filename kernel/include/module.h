#ifndef MODULE_H
#define MODULE_H 1

typedef struct {
	volatile char *name;
	void (* init)(void);
} module_info_t;

#define MODULE_INFO(n, i) \
	__attribute__((used)) __attribute__((section("mod_info"))) module_info_t module_info_##n = { \
		.name = #n, \
		.init = &i \
	}

extern  void *__start_mod_info;
extern void *__stop_mod_info;

void modules_init();

#endif
