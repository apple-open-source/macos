#include <cstdint>

#define _CALL_10(expr) \
		(expr); \
		(expr); \
		(expr); \
		(expr); \
		(expr); \
		(expr); \
		(expr); \
		(expr); \
		(expr); \
		(expr)

#define CALL_100(expr) \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr); \
		_CALL_10(expr)

#define N_UNIQUE_CALLSITES 200

// must match constant above
#define CALL_N_CALLSITES(expr) \
		CALL_100(expr); \
		CALL_100(expr)

extern "C" {

struct test_struct {
	uint64_t a[8];
};

void
call_n_cpp_new(void **ptrs)
{
	int i = 0;
	CALL_N_CALLSITES(({ ptrs[i] = (void *)(new test_struct()); i++; }));
}

void
delete_n_cpp(void **ptrs)
{
	for (int i = 0; i < N_UNIQUE_CALLSITES; i++) {
		delete (test_struct *)ptrs[i];
	}
}

}
