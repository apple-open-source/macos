#include <../src/platform.h>

#if defined(__arm64__) && defined(__LP64__) && CONFIG_MTE && !MALLOC_TARGET_EXCLAVES
#define MALLOC_MTE_TESTING_SUPPORTED 1
#else
#define MALLOC_MTE_TESTING_SUPPORTED 0
#endif

#if MALLOC_MTE_TESTING_SUPPORTED

#include <os/base.h>
#include <stddef.h>
#include <_simple.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <darwintest.h>

// Given a pointer, strip the tag from bits 56-59 and return the result
#define PTR_STRIP_TAG(p) \
	((__typeof__(p))(((uintptr_t)(p)) & ~((uintptr_t)0xf << 56)))
// Given a pointer, return the logical tag stored in bits 56-59
#define PTR_EXTRACT_TAG(p) ((unsigned int)((((uintptr_t)(p)) >> 56) & 0xf))
// Return whether a pointer has a non-canonical tag in bits 56-59
#define PTR_IS_TAGGED(p) (PTR_EXTRACT_TAG(p) != 0)
// Given a pointer p, return a pointer with bits 56-59 set to t
#define PTR_SET_TAG(p, t) (__typeof__(p))(((uintptr_t)t << 56) | ((uintptr_t)p))

OS_ENUM(sec_transition_config, uint8_t,
	SEC_TRANSITION_NONE,
	SEC_TRANSITION_EMULATED,
	SEC_TRANSITION_HARDWARE,
);

static inline const char **
get_apple_array(void)
{
	// The kernel sets up the stack with the following memory layout
	//   ┌────────┐
	//   │  argc  │
	//   ├────────┤
	//   │  argv  │
	//   ├────────┤
	//   │  envp  │
	//   ├────────┤
	//   │  Apple │
	//   └────────┘
	// Each array of pointers is terminated by a NULL value; thus, in theory we
	// should be able to find the Apple array right after the NULL pointer at
	// the end of envp.
	// However, libsanitizers has its own unsetenv implementation which it uses
	// at initialization time to consume the environment variables it uses. This
	// changes the layout by moving all environment variables "down", and thus
	// leaving more than a single NULL pointer between envp and the Apple array.
	// This applies to all variables used by libsanitizers, so after getting to
	// the end of envp, we move forward until we find a non-NULL pointer.
	extern char **environ;
	char **p = environ;
	while (*p != NULL) {
		p++;
	}
	p++;
	while (*p == NULL) {
		p++;
	}
	return (const char **)p;
}

static int
process_is_translated(void)
{
	int ret = 0;
	size_t size = sizeof(ret);
	if (sysctlbyname("sysctl.proc_translated", &ret, &size, NULL, 0) == -1) {
		if (errno == ENOENT) {
			return 0;
		}
		return -1;
	}
	return ret;
}

static inline sec_transition_config_t
get_sec_transition_config(void)
{
	const char **apple = get_apple_array();
	const char *flag = _simple_getenv(apple, "has_sec_transition");
	if (flag) {
		uint64_t value = (uint64_t)strtoull(flag, NULL, 0);
		if (value == 1) {
			if (process_is_translated() == 1) {
				return SEC_TRANSITION_EMULATED;
			}
			return SEC_TRANSITION_HARDWARE;
		}
	}
	return SEC_TRANSITION_NONE;
}

#define T_SKIP_REQUIRES_SEC_TRANSITION() \
	do { \
		if (get_sec_transition_config() == SEC_TRANSITION_NONE) { \
			T_SKIP("Requires has_sec_transition=1"); \
		} \
	} while (0)

#define T_SKIP_REQUIRES_SEC_TRANSITION_HARDWARE() \
	do { \
		if (get_sec_transition_config() != SEC_TRANSITION_HARDWARE) { \
			T_SKIP("Requires has_sec_transition=1 on supported hardware"); \
		} \
	} while (0)

#endif // MALLOC_MTE_TESTING_SUPPORTED
