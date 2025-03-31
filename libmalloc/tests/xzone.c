#include <darwintest.h>
#include <darwintest_utils.h>

#include <malloc_private.h>

#include "../src/platform.h"

T_DECL(xzone_debug_dylib, "Ensure xzone malloc tests run with debug dylib",
		T_META_TAG_VM_NOT_ELIGIBLE,
		T_META_TAG_XZONE_ONLY)
{
	T_ASSERT_TRUE(malloc_variant_is_debug_4test(),
			"Test is running with the debug dylib");
}
