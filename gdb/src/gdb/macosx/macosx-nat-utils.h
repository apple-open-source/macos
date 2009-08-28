#ifndef __GDB_MACOSX_NAT_UTILS_H__
#define __GDB_MACOSX_NAT_UTILS_H__

#include <mach/mach.h>

#if (defined __GNUC__)
#define __MACH_CHECK_FUNCTION __PRETTY_FUNCTION__
#else
#define __MACH_CHECK_FUNCTION ((__const char *) 0)
#endif

#define MACH_PROPAGATE_ERROR(ret) \
{ MACH_WARN_ERROR(ret); if ((ret) != KERN_SUCCESS) { return ret; } }

#define MACH_CHECK_ERROR(ret) \
  mach_check_error (ret, __FILE__, __LINE__, __MACH_CHECK_FUNCTION);

#define MACH_WARN_ERROR(ret) \
  mach_warn_error (ret, __FILE__, __LINE__, __MACH_CHECK_FUNCTION);

#define MACH_ERROR_STRING(ret) \
  (mach_error_string (ret) ? mach_error_string (ret) : "[UNKNOWN]")

const void *macosx_parse_plist (const char *plist_path);

void macosx_free_plist (const void **plist);

const char *macosx_get_plist_posix_value (const void *plist, const char* key);

const char *macosx_get_plist_string_value (const void *plist, const char* key);

char * macosx_filename_in_bundle (const char *filename, int mainline);

void mach_check_error (kern_return_t ret, const char *file, unsigned int line,
                       const char *func);

void mach_warn_error (kern_return_t ret, const char *file, unsigned int line,
                      const char *func);

#endif /* __GDB_MACOSX_NAT_UTILS_H__ */
