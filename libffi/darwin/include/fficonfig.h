/* Specify which architecture libffi is configured for. */
#ifdef __arm__
#include <fficonfig_armv7.h>
#endif

#ifdef __arm64__
#ifdef __arm64e__
#include <fficonfig_arm64e.h>
#else
#include <fficonfig_arm64.h>
#endif
#endif

#ifdef __i386__
#include <fficonfig_i386.h>
#endif

#ifdef __x86_64__
#include <fficonfig_x86_64.h>
#endif
