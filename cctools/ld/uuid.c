/*
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#if defined(__MWERKS__) && !defined(__private_extern__)
#define __private_extern__ __declspec(private_extern)
#endif

#include <sys/types.h>
#include <string.h>
#include <stdarg.h>
#include <mach/mach.h>
#include <mach-o/loader.h>
#if !(defined(KLD) && defined(__STATIC__))
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <mach-o/dyld.h>
#endif /* !(defined(KLD) && defined(__STATIC__)) */
#include "ld.h"

/*
 * uuid() is called to set the uuid[] bytes for the uuid load command.
 */
__private_extern__
void
uuid(
uint8_t *uuid)
{
    struct uuid_command u;
#if !(defined(KLD) && defined(__STATIC__))
    void (*uuid_func)(uint8_t *out);
    NSSymbol nssymbol;
    int fd;
    ssize_t n;

	/*
	 * We would like to just #include <uuid/uuid.h> and but that header
	 * file did not exist on system until Mac OS 10.4 .  So instead we
	 * dynamically lookup uuid_generate_random() and if it is defined we
	 * call it indirectly.
	 */
	if(NSIsSymbolNameDefined("_uuid_generate_random")){
	    nssymbol = (void *)NSLookupAndBindSymbol("_uuid_generate_random");
	    uuid_func = NSAddressOfSymbol(nssymbol);
	    uuid_func(uuid);
	}
	/*
	 * Since we don't have uuid_generate() just read bytes from /dev/urandom
	 */
	else{
	    fd = open("/dev/urandom", O_RDONLY, 0);
	    if(fd == -1){
		system_warning("can't open: /dev/urandom to fill in uuid load "
		    "command (using bytes of zero)");
		memset(uuid, '\0', sizeof(u.uuid));
	    }
	    else{
		n = read(fd, uuid, sizeof(u.uuid));
		if(n != sizeof(u.uuid)){
		    system_warning("can't read bytes from: /dev/urandom to "
			"fill in uuid load command (using bytes of zero)");
		    memset(uuid, '\0', sizeof(u.uuid));
		}
		(void)close(fd);
	    }
	}
#else /* defined(KLD) && defined(__STATIC__) */
	memset(uuid, '\0', sizeof(u.uuid));
#endif /* !(defined(KLD) && defined(__STATIC__)) */
}
