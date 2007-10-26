/*
 * FILE: kextmanager_types.h
 * AUTH: I/O Kit Team (Copyright Apple Computer, 2002, 2006-7)
 * DATE: June 2002, September 2006, August 2007
 * DESC: typedefs for the kextmanager_mig.defs's MiG-generated code 
 *
 */

#ifndef __KEXT_TYPES_H__
#define __KEXT_TYPES_H__

#include <mach/mach_types.h>        // allows to compile standalone
#include <mach/kmod.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <uuid/uuid.h>              // uuid_t

#define KEXTD_SERVER_NAME       "com.apple.KernelExtensionServer"
#define PROPERTYKEY_LEN         128

typedef int kext_result_t;
typedef char mountpoint_t[MNAMELEN];
typedef char property_key_t[PROPERTYKEY_LEN];
typedef char kext_bundle_id_t[KMOD_MAX_NAME];
typedef char posix_path_t[MAXPATHLEN];
typedef char * xmlDataOut_t;
typedef char * xmlDataIn_t;

#endif __KEXT_TYPES_H__
