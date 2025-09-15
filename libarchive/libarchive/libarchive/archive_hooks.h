#pragma once

#include <os/variant_private.h>

__private_extern__ void
__call_test_hook(const char *name);

#define CALL_TEST_HOOK(name)								\
	do {										\
		if (os_variant_has_internal_content("com.apple.libarchive.tests"))	\
			__call_test_hook("__test_hook_"#name);				\
	} while(0)

#define DEFINE_TEST_HOOK(name)					\
	volatile int __test_hook_##name##_counter;		\
	void  __attribute__ ((__visibility__ ("default")))	\
	__test_hook_##name(void)


#define RECORD_TEST_HOOK_RUN(name)				\
	__test_hook_##name##_counter = 1;			\

#define assertHookRan(name)					\
	do {							\
		assert(__test_hook_##name##_counter != 0 &&	\
			   "Hook " #name " did not run");	\
		__test_hook_##name##_counter = 0;		\
	} while(0)
