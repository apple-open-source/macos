#ifndef __KEXT_TYPES_H__
#define __KEXT_TYPES_H__

#include <mach/kmod.h>
#include <sys/param.h>

typedef int kext_result_t;
typedef char kext_bundle_id_t[KMOD_MAX_NAME];
typedef char posix_path_t[MAXPATHLEN];
typedef char property_key_t[128];
typedef char * xmlDataOut_t;
typedef char * xmlDataIn_t;

#endif __KEXT_TYPES_H__
