#include <stdlib.h>

#include <darwintest.h>

T_DECL(asan_sanity, "ASan Sanity Check",
		T_META_CHECK_LEAKS(NO)){
	void *ptr = malloc(16);
	free(ptr);
	T_PASS("I didn't crash!");
}
