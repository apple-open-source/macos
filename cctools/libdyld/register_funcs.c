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
 * _dyld_register_func_for_add_image registers the specified function to be
 * called when a new image is added (a bundle or a dynamic shared library) to
 * the program.  When this function is first registered it is called for once
 * for each image that is currently part of the program.
 */
void
_dyld_register_func_for_add_image(
void (*func)(struct mach_header *mh, unsigned long vmaddr_slide))
{
    static void (*p)(void (*func)(struct mach_header *mh,
			     unsigned long vmaddr_slide)) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_register_func_for_add_image",
			      (unsigned long *)&p);
	p(func);
}

/*
 * _dyld_register_func_for_remove_image registers the specified function to be
 * called when an image is removed (a bundle or a dynamic shared library) from
 * the program.
 */
void
_dyld_register_func_for_remove_image(
void (*func)(struct mach_header *mh, unsigned long vmaddr_slide))
{
    static void (*p)(void (*func)(struct mach_header *mh,
			     unsigned long vmaddr_slide)) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_register_func_for_remove_image",
			      (unsigned long *)&p);
	p(func);
}

/*
 * _dyld_register_func_for_link_module registers the specified function to be
 * called when a module is bound into the program.  When this function is first
 * registered it is called for once for each module that is currently bound into
 * the program.
 */
void
_dyld_register_func_for_link_module(
void (*func)(NSModule module))
{
    static void (*p)(void (*func)(NSModule module)) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_register_func_for_link_module",
			      (unsigned long *)&p);
	p(func);
}

/*
 * _dyld_register_func_for_unlink_module registers the specified function to be
 * called when a module is unbound from the program.
 */
void
_dyld_register_func_for_unlink_module(
void (*func)(NSModule module))
{
    static void (*p)(void (*func)(NSModule module)) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_register_func_for_unlink_module",
			      (unsigned long *)&p);
	p(func);
}

/*
 * _dyld_register_func_for_replace_module registers the specified function to be
 * called when a module is to be replace with another module in the program.
 */
void
_dyld_register_func_for_replace_module(
void (*func)(NSModule oldmodule, NSModule newmodule))
{
    static void (*p)(void (*func)(NSModule oldmodule,
				  NSModule newmodule)) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_register_func_for_replace_module",
			      (unsigned long *)&p);
	p(func);
}
