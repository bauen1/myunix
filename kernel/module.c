#include <assert.h>

#include <console.h>
#include <module.h>

// XXX: ensure the special section gets created, even if not modules are there
__attribute__((used))
static __attribute__((section("mod_info"))) unsigned int _a[0];

void modules_init() {
	printf("%s()\n", __func__);
	uintptr_t mod_info_start = (uintptr_t)&__start_mod_info;
	uintptr_t mod_info_stop = (uintptr_t)&__stop_mod_info;
	uintptr_t mod_info_length = (mod_info_stop - mod_info_start);
	for (uintptr_t i = 0; i < mod_info_length; i += sizeof(module_info_t)) {
		uintptr_t v_addr = mod_info_start + i;
		module_info_t *mod_info = (module_info_t *)(v_addr);
		if (mod_info->name == NULL) {
			printf("%s: name == NULL, assuming end\n", __func__);
			break;
		}
		assert(mod_info->name != NULL);
		assert(mod_info->init != NULL);
		printf("%s: init(%s)\n", __func__, mod_info->name);
		mod_info->init();
	}
}
