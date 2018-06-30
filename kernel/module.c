#include <assert.h>

#include <console.h>
#include <module.h>

__attribute__((used))
static __attribute__((section("mod_info"))) unsigned int _a[0];

void modules_init() {
	printf("modules init!\n");
	uintptr_t mod_info_start = (uintptr_t)&__start_mod_info;
	uintptr_t mod_info_stop = (uintptr_t)&__stop_mod_info;
	uintptr_t mod_info_length = (mod_info_stop - mod_info_start);
	for (uintptr_t i = 0; i < mod_info_length; i += sizeof(module_info_t)) {
		uintptr_t v_addr = mod_info_start + i;
		module_info_t *mod_info = (module_info_t *)(v_addr);
		if (mod_info->name == NULL) {
			printf("name == NULL, assuming end\n");
			break;
		}
		assert(mod_info->name != NULL);
		assert(mod_info->init != NULL);
		printf("init: %s\n", mod_info->name);
		mod_info->init();
	}
}
