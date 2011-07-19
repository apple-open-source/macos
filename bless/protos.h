/*
 * Copyright (c) 2006-2007 Apple Inc. All Rights Reserved.
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
/*
 *  protos.h
 *  bless
 *
 *  Created by Shantonu Sen on 6/6/06.
 *  Copyright 2006-2007 Apple Inc. All Rights Reserved.
 *
 */

#include "enums.h"
#include "structs.h"
#include <stdbool.h>
#include <sys/cdefs.h>

int modeInfo(BLContextPtr context, struct clarg actargs[klast]);
int modeDevice(BLContextPtr context, struct clarg actargs[klast]);
int modeFolder(BLContextPtr context, struct clarg actargs[klast]);
int modeFirmware(BLContextPtr context, struct clarg actargs[klast]);
int modeNetboot(BLContextPtr context, struct clarg actargs[klast]);
int modeUnbless(BLContextPtr context, struct clarg actargs[klast]);

int blesslog(void *context, int loglevel, const char *string);
int blesscontextprintf(BLContextPtr context, int loglevel, char const *fmt, ...) __printflike(3, 4);

void usage(void);
void usage_short(void);

void addPayload(const char *path);


extern int setboot(BLContextPtr context, char *device, CFDataRef bootxData,
				   CFDataRef labelData);
extern int setefidevice(BLContextPtr context, const char * bsdname, int bootNext,
                        int bootLegacy, const char *legacyHint, const char *optionalData,
                        bool shortForm);
extern int setefifilepath(BLContextPtr context, const char * path, int bootNext,
                          const char *optionalData, bool shortForm);
extern int setefilegacypath(BLContextPtr context, const char * path, int bootNext,
                          const char *legacyHint, const char *optionalData);
extern int setefinetworkpath(BLContextPtr context, CFStringRef booterXML,
							 CFStringRef kernelXML, CFStringRef mkextXML,
                             CFStringRef kernelcacheXML, int bootNext);
extern int efinvramcleanup(BLContextPtr context);
