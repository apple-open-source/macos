#include "efence.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <mach/mach.h>

#if (!defined __GNUC__ || __GNUC__ < 2 || __GNUC_MINOR__ < (defined __cplusplus ? 6 : 4))
#define __MACH_CHECK_FUNCTION ((__const char *) 0)
#else
#define __MACH_CHECK_FUNCTION __PRETTY_FUNCTION__
#endif

#define MACH_CHECK_ERROR(ret) \
mach_check_error (ret, __FILE__, __LINE__, __MACH_CHECK_FUNCTION);

#define MACH_CHECK_NOERROR(ret) \
mach_check_noerror (ret, __FILE__, __LINE__, __MACH_CHECK_FUNCTION);

static void mach_check_error (kern_return_t ret, const char *file, unsigned int line, const char *func)
{
  if (ret == KERN_SUCCESS) {
    return;
  }
  if (func == NULL) {
    func = "[UNKNOWN]";
  }

  EF_Exit ("fatal Mach error on line %u of \"%s\" in function \"%s\": %s\n",
	   line, file, func, mach_error_string (ret));
}

void *Page_Create (size_t size)
{
  kern_return_t kret;
  vm_address_t address = 0;

  kret = vm_allocate (mach_task_self(), &address, size, 1);
  MACH_CHECK_ERROR (kret);
  
  return ((void *) address);
}

void Page_AllowAccess (void *address, size_t size)
{
  kern_return_t kret;
  
  kret = vm_protect (mach_task_self(), (vm_address_t) address, size, 0, VM_PROT_READ | VM_PROT_WRITE);
  MACH_CHECK_ERROR (kret);
}

void Page_DenyAccess (void *address, size_t size)
{
  kern_return_t kret;

  kret = vm_protect (mach_task_self(), (vm_address_t) address, size, 0, VM_PROT_NONE);
  MACH_CHECK_ERROR (kret);
}

void Page_Delete (void *address, size_t size)
{
  kern_return_t kret;
  
  kret = vm_deallocate (mach_task_self(), (vm_address_t) address, size);
  MACH_CHECK_ERROR (kret);
}

size_t Page_Size (void)
{
#if defined (__MACH30__)
    kern_return_t result;
    vm_size_t page_size = 0;

    result = host_page_size (mach_host_self(), &page_size);
    MACH_CHECK_ERROR(result);

    return (size_t) page_size;
#else /* ! __MACH30__ */
  static struct vm_statistics stats_data;
  static struct vm_statistics *stats = NULL;
  kern_return_t kret;

  if (stats == NULL) {
    kret = vm_statistics (mach_task_self(), &stats_data);
    MACH_CHECK_ERROR (kret);

    stats = &stats_data;
  }

  return stats->pagesize;
#endif /* ! __MACH30__ */
}
