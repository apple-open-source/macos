/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
#ifdef SHLIB
#include "shlib.h"
#endif
#include "stdlib.h"
#include "stuff/bool.h"
#include "mach-o/dyld.h"

void
_dyld_lookup_and_bind(
const char *symbol_name,
unsigned long *address,
void **module)
{
    static void (*p)(const char *, unsigned long *, void **) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_lookup_and_bind", (unsigned long *)&p);
	p(symbol_name, address, module);
}

void
_dyld_lookup_and_bind_with_hint(
const char *symbol_name,
const char *library_name_hint,
unsigned long *address,
void **module)
{
    static void (*p)(const char *, const char *,
		     unsigned long *, void **) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_lookup_and_bind_with_hint",
			      (unsigned long *)&p);
	p(symbol_name, library_name_hint, address, module);
}

void
_dyld_lookup_and_bind_objc(
const char *symbol_name,
unsigned long *address,
void **module)
{
    static void (*p)(const char *, unsigned long *, void **) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_lookup_and_bind_objc",
			      (unsigned long *)&p);
	p(symbol_name, address, module);
}

void
_dyld_lookup_and_bind_fully(
const char *symbol_name,
unsigned long *address,
void **module)
{
    static void (*p)(const char *, unsigned long *, void **) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_lookup_and_bind_fully",
		(unsigned long *)&p);
	p(symbol_name, address, module);
}

enum bool
_dyld_bind_fully_image_containing_address(
unsigned long *address)
{
    static enum bool (*p)(unsigned long *) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_bind_fully_image_containing_address",
			      (unsigned long *)&p);
	return(p(address));
}
