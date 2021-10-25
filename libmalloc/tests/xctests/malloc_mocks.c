#include "internal.h"

bool
malloc_tracing_enabled = false;

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
