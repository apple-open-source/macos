/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifdef SHLIB
#include "shlib.h"
#endif
#import <stdio.h>
#import <mach-o/dyld.h>

#import "ofi.h"

NSModule
NSLinkModule(
NSObjectFileImage objectFileImage, 
const char *moduleName,
unsigned long options)
{
    static NSModule (*p)(char *object_addr,
			 unsigned long object_size,
			 const char *moduleName,
			 unsigned long options) = NULL;
    struct ofi *ofi;

	ofi = (struct ofi *)objectFileImage;
	if(p == NULL)
	    _dyld_func_lookup("__dyld_link_module", (unsigned long *)&p);
	return(p(ofi->ofile.object_addr, ofi->ofile.object_size, moduleName,
		 options));
}

void
NSLinkEditError(
NSLinkEditErrors *c,
int *errorNumber, 
const char **fileName,
const char **errorString)
{
    static void (*p)(NSLinkEditErrors *c,
		     int *errorNumber, 
		     const char **fileName,
		     const char **errorString) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_link_edit_error", (unsigned long *)&p);
	if(p != NULL)
	    p(c, errorNumber, fileName, errorString);
}

enum bool
NSUnLinkModule(
NSModule module, 
unsigned long options)
{
    static enum bool (*p)(NSModule module,
    			  unsigned long options) = NULL;

	if(p == NULL){
	    _dyld_func_lookup("__dyld_unlink_module", (unsigned long *)&p);
	}
	if(p != NULL){
/*
printf("_dyld_func_lookup of __dyld_unlink_module worked\n");
*/
	    return(p(module, options));
	}
	else{
/*
printf("_dyld_func_lookup of __dyld_unlink_module failed\n");
*/
	    return(FALSE);
	}
}

NSModule
NSReplaceModule(
NSModule moduleToReplace,
NSObjectFileImage newObjectFileImage, 
unsigned long options)
{
	return(NULL);
}
