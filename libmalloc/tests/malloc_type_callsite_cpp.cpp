#include <cstdint>
#include <cstdlib>

#include "tmo_test_defs.h"

extern "C" {

struct test_struct {
	uint64_t a[64];
};

void **
cpp_new_fallback(void)
{
	void **ptrs = (void **)calloc(N_UNIQUE_CALLSITES, sizeof(void *));
	if (!ptrs) {
		return NULL;
	}

	int i = 0;
	CALL_N_CALLSITES(({ ptrs[i] = (void *)(new test_struct()); i++; }));

	return ptrs;
}

void
cpp_delete_fallback(void **ptrs)
{
	for (int i = 0; i < N_UNIQUE_CALLSITES; i++) {
		delete (test_struct *)ptrs[i];
	}

	free(ptrs);
}

}
