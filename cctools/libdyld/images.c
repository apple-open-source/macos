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
#import "stdlib.h"
#import "libc.h"
#import "mach-o/dyld.h"
#import "mach-o/loader.h"
#ifndef __OPENSTEP__
extern const struct section *getsectbyname(
	const char *segname, 
	const char *sectname);
#endif

unsigned long
_dyld_present(
void)
{
	if(getsectbyname("__DATA", "__dyld") != NULL)
	    return(1);
	else
	    return(0);
}

unsigned long
_dyld_image_count(
void)
{
    static unsigned long (*p)(void) = NULL;

	if(_dyld_present() == 0)
	    return(0);
	if(p == NULL)
	    _dyld_func_lookup("__dyld_image_count", (unsigned long *)&p);
	return(p());
}

struct mach_header *
_dyld_get_image_header(
unsigned long image_index)
{
    static struct mach_header * (*p)(unsigned long image_index) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_get_image_header", (unsigned long *)&p);
	return(p(image_index));
}

unsigned long
_dyld_get_image_vmaddr_slide(
unsigned long image_index)
{
    static unsigned long (*p)(unsigned long image_index) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_get_image_vmaddr_slide",
			      (unsigned long *)&p);
	return(p(image_index));
}

char *
_dyld_get_image_name(
unsigned long image_index)
{
    static char * (*p)(unsigned long image_index) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_get_image_name",
			      (unsigned long *)&p);
	return(p(image_index));
}

enum bool
_dyld_image_containing_address(
unsigned long address)
{
    static enum bool (*p)(unsigned long) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_image_containing_address",
			      (unsigned long *)&p);
	return(p(address));
}

void _dyld_moninit(
void (*monaddition)(char *lowpc, char *highpc))
{
    static void (*p)(void (*monaddition)(char *lowpc, char *highpc)) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_moninit", (unsigned long *)&p);
	p(monaddition);
}

enum bool _dyld_launched_prebound(
void)
{
    static enum bool (*p)(void) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_launched_prebound", (unsigned long *)&p);
	return(p());
}
