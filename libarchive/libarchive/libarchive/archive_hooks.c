#include "archive_hooks.h"

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>

static void *dlhandle = NULL;
static int enable_hooks = -1;

__private_extern__ void
__call_test_hook(const char *name)
{
	void (*hook)(void);

	if (enable_hooks == -1)
		enable_hooks = getenv("__WANT_TEST_HOOKS") != NULL;

	if (!enable_hooks)
		return;

	if (dlhandle == NULL)
		dlhandle = dlopen(NULL, RTLD_LAZY);
	hook = dlsym(dlhandle, name);
	if (hook != NULL)
		hook();
}
