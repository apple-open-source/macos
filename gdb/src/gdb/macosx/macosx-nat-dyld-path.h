#ifndef __GDB_MACOSX_NAT_DYLD_PATH_H__
#define __GDB_MACOSX_NAT_DYLD_PATH_H__

#include "defs.h"

typedef struct dyld_path_info
{
  char *framework_path;
  char *library_path;
  char *image_suffix;
  char *fallback_framework_path;
  char *fallback_library_path;

  char *insert_libraries;
} dyld_path_info;

void dyld_library_basename (const char *path, const char **s, int *len, 
                            int *is_framework, int *is_bundle);

char *dyld_resolve_image (const struct dyld_path_info *d, 
                          const char *dylib_name);

void dyld_zero_path_info (dyld_path_info *d);

void dyld_init_paths (dyld_path_info * d);

#endif /* __GDB_MACOSX_NAT_DYLD_PATH_H__ */
