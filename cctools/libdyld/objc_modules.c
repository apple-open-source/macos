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

/*
 * _dyld_get_objc_module_sect_for_module is passed a module and sets a 
 * pointer to the (__OBJC,__module) section and its size for the specified
 * module.
 */
void
_dyld_get_objc_module_sect_for_module(
NSModule module,
void **objc_module,
unsigned long *size)
{
    static void (*p)(NSModule module,
		     void **objc_module,
		     unsigned long *size) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_get_objc_module_sect_for_module",
			      (unsigned long *)&p);
	p(module, objc_module, size);
}

/*
 * _dyld_bind_objc_module() is passed a pointer to something in an (__OBJC,
 * __module) section and causes the module that is associated with that address
 * to be bound.
 */
void
_dyld_bind_objc_module(
void *objc_module)
{
    static void (*p)(void *objc_module) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_bind_objc_module", (unsigned long *)&p);
	p(objc_module);
}
