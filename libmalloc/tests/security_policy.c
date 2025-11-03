#include <darwintest.h>

#include <malloc_private.h>

T_DECL(security_policy_default,
		"Ensure that internal security is not enabled by default",
		T_META_TAG_VM_NOT_PREFERRED, T_META_TAG_NO_ALLOCATOR_OVERRIDE)
{
	T_ASSERT_FALSE(malloc_allows_internal_security_4test(),
			"Internal security should be disabled by default");
}

T_DECL(security_policy_envvar,
		"Ensure that internal security can be enabled via environment",
		T_META_TAG_VM_NOT_PREFERRED, T_META_TAG_NO_ALLOCATOR_OVERRIDE,
		T_META_ENVVAR("MallocAllowInternalSecurity=1"))
{
	T_ASSERT_TRUE(malloc_allows_internal_security_4test(),
			"Internal security should be enabled by the environment");
}
