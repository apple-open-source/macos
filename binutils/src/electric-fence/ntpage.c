#include "efence.h"

#include <stdlib.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <fcntl.h>

#if (!defined __GNUC__ || __GNUC__ < 2 || __GNUC_MINOR__ < (defined __cplusplus ? 6 : 4))
#define __MACH_CHECK_FUNCTION ((__const char *) 0)
#else
#define __MACH_CHECK_FUNCTION __PRETTY_FUNCTION__
#endif

#define MACH_CHECK_ERROR(ret) \
mach_check_error (ret, __FILE__, __LINE__, __MACH_CHECK_FUNCTION);

#define MACH_CHECK_NOERROR(ret) \
mach_check_noerror (ret, __FILE__, __LINE__, __MACH_CHECK_FUNCTION);

static void mach_check_error
(unsigned int ret, const char *file, unsigned int line, const char *func)
{
  if (ret == 0) {
    return;
  }
  if (func == NULL) {
    func = "[UNKNOWN]";
  }

  EF_Exit ("fatal NT error on line %u of \"%s\" in function \"%s\": %s\n",
	   line, file, func, "[UNKNOWN]");
}

void *Page_Create (size_t size)
{
  return NULL;
}

void Page_AllowAccess (void *address, size_t size)
{
}

void Page_DenyAccess (void *address, size_t size)
{
}

void Page_Delete (void *address, size_t size)
{
}

size_t Page_Size (void)
{
  return 4096;
}
