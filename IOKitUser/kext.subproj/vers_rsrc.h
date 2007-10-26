#if !__LP64__

#ifndef _LIBSA_VERS_H_
#define _LIBSA_VERS_H_

#include <sys/cdefs.h>

__BEGIN_DECLS

#ifndef KERNEL
#include <sys/types.h>
#include <libc.h>
#include <CoreFoundation/CoreFoundation.h>
#else
#include <libkern/OSTypes.h>
#endif KERNEL

typedef SInt64 VERS_version;
VERS_version VERS_parse_string(const char * vers_string);
int VERS_string(char * buffer, UInt32 length, VERS_version vers);

__END_DECLS

#endif _LIBSA_VERS_H_
#endif // !__LP64__
