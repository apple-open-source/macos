#ifndef _NEXTSTEP_NAT_DYLD_PATH_H_
#define _NEXTSTEP_NAT_DYLD_PATH_H_

#include "defs.h"

typedef struct dyld_path_info {
  char *framework_path;
  char *library_path;
  char *image_suffix;
  char *fallback_framework_path;
  char *fallback_library_path;

  char *insert_libraries;
} dyld_path_info;

void dyld_library_basename
(const char *path, const char **s, unsigned int *len, int *is_framework, int *is_bundle);

char *dyld_resolve_image
(const struct dyld_path_info *d, const char *dylib_name);

void dyld_init_paths (dyld_path_info *d);

#endif /* _NEXTSTEP_NAT_DYLD_PATH_H_ */
