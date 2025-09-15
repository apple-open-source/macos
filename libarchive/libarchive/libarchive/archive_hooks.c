#include "archive_hooks.h"

#include <sys/types.h>
#include <unistd.h>
#include <dlfcn.h>

static void *dlhandle = NULL;

__private_extern__ void
__call_test_hook(const char *name)
{
	void (*hook)(void);

	if (dlhandle == NULL)
		dlhandle = dlopen(NULL, RTLD_LAZY);
	hook = dlsym(dlhandle, name);
	if (hook != NULL)
		hook();
}
