#include "internal.h"
#include <setjmp.h>

bool
malloc_tracing_enabled = false;

malloc_zero_policy_t malloc_zero_policy = MALLOC_ZERO_POLICY_DEFAULT;
unsigned malloc_zero_on_free_sample_period = 0;

void
malloc_zone_check_fail(const char *msg, const char *fmt, ...)
{
	__builtin_trap();
}

void
malloc_error_break(void)
{
	__builtin_trap();
}

jmp_buf *zone_error_expected_jmp;

void
malloc_zone_error(uint32_t flags, bool is_corruption, const char *fmt, ...)
{
	if (!zone_error_expected_jmp || !is_corruption) {
		__builtin_trap();
	}

	longjmp(zone_error_expected_jmp, 1);
}

void
find_zone_and_free(void *ptr, bool known_non_default)
{
	__builtin_trap();
}
