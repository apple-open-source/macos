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
#import <stdlib.h>
#import <string.h>
#import <stdio.h>
#ifndef __OPENSTEP__
#import <crt_externs.h>
#endif
#import <mach-o/dyld.h>
#import <mach-o/ldsyms.h>

static enum bool names_match(
    char *install_name,
    const char *libraryName);

void NSInstallLinkEditErrorHandlers(
NSLinkEditErrorHandlers *handlers)
{
    static void (*p)(
	void     (*undefined)(const char *symbol_name),
	NSModule (*multiple)(NSSymbol s, NSModule old, NSModule new),
	void     (*linkEdit)(NSLinkEditErrors c, int errorNumber,
		     const char *fileName, const char *errorString)) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_install_handlers", (unsigned long *)&p);
	p(handlers->undefined, handlers->multiple, handlers->linkEdit);
}

const char *
NSNameOfModule(
NSModule module)
{
    static char * (*p)(NSModule module) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_NSNameOfModule", (unsigned long *)&p);
	return(p(module));
} 

const char *
NSLibraryNameForModule(
NSModule module)
{
    static char * (*p)(NSModule module) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_NSLibraryNameForModule",
			      (unsigned long *)&p);
	return(p(module));
}

enum bool
NSIsSymbolNameDefined(
const char *symbolName)
{
    static enum bool (*p)(const char *symbolName) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_NSIsSymbolNameDefined",
			      (unsigned long *)&p);
	return(p(symbolName));
}

enum bool
NSIsSymbolNameDefinedWithHint(
const char *symbolName,
const char *libraryNameHint)
{
    static enum bool (*p)(const char *symbolName,
			  const char *libraryNameHint) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_NSIsSymbolNameDefinedWithHint",
			      (unsigned long *)&p);
	return(p(symbolName, libraryNameHint));
}

enum bool
NSIsSymbolNameDefinedInImage(
const struct mach_header *image,
const char *symbolName)
{
    static enum bool (*p)(const struct mach_header *image,
			  const char *symbolName) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_NSIsSymbolNameDefinedInImage",
			      (unsigned long *)&p);
	return(p(image, symbolName));
}

NSSymbol
NSLookupAndBindSymbol(
const char *symbolName)
{
    static NSSymbol (*p)(const char *symbolName) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_NSLookupAndBindSymbol",
			      (unsigned long *)&p);
	return(p(symbolName));
}

NSSymbol
NSLookupAndBindSymbolWithHint(
const char *symbolName,
const char *libraryNameHint)
{
    static NSSymbol (*p)(const char *symbolName,
			 const char *libraryNameHint) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_NSLookupAndBindSymbolWithHint",
			      (unsigned long *)&p);
	return(p(symbolName, libraryNameHint));
}

NSSymbol
NSLookupSymbolInModule(
NSModule module,
const char *symbolName)
{
    static NSSymbol (*p)(NSModule module, const char *symbolName) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_NSLookupSymbolInModule",
			      (unsigned long *)&p);
	return(p(module, symbolName));
}

NSSymbol
NSLookupSymbolInImage(
const struct mach_header *image,
const char *symbolName,
unsigned long options)
{
    static NSSymbol (*p)(const struct mach_header *image,
			 const char *symbolName,
			 unsigned long options) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_NSLookupSymbolInImage",
			      (unsigned long *)&p);
	return(p(image, symbolName, options));
}

const char *
NSNameOfSymbol(
NSSymbol symbol)
{
    static char * (*p)(NSSymbol symbol) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_NSNameOfSymbol", (unsigned long *)&p);
	return(p(symbol));
}

void *
NSAddressOfSymbol(
NSSymbol symbol)
{
    static void * (*p)(NSSymbol symbol) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_NSAddressOfSymbol", (unsigned long *)&p);
	return(p(symbol));
}

NSModule
NSModuleForSymbol(
NSSymbol symbol)
{
    static NSModule (*p)(NSSymbol symbol) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_NSModuleForSymbol", (unsigned long *)&p);
	return(p(symbol));
}

enum bool
NSAddLibrary(
const char *pathName)
{
    static enum bool (*p)(const char *pathName) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_NSAddLibrary", (unsigned long *)&p);
	return(p(pathName));
}

enum bool
NSAddLibraryWithSearching(
const char *pathName)
{
    static enum bool (*p)(const char *pathName) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_NSAddLibraryWithSearching",
			      (unsigned long *)&p);
	return(p(pathName));
}

const struct mach_header *
NSAddImage(
const char *image_name,
unsigned long options)
{
    static const struct mach_header * (*p)(const char *image_name,
					   unsigned long options) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_NSAddImage",
			      (unsigned long *)&p);
	return(p(image_name, options));
}

/*
 * This routine returns the current version of the named shared library the
 * executable it was built with.  The libraryName parameter is the same as the
 * -lx or -framework Foo argument passed to the static link editor when building
 * the executable (with -lx it would be "x" and with -framework Foo it would be
 * "Foo").  If this the executable was not built against the specified library
 * it returns -1.  It should be noted that if this only returns the value the
 * current version of the named shared library the executable was built with
 * and not a list of current versions that dependent libraries and bundles the
 * program is using were built with.
 */
long
NSVersionOfLinkTimeLibrary(
const char *libraryName)
{
    unsigned long i;
    struct load_command *load_commands, *lc;
    struct dylib_command *dl;
    char *install_name;
#ifndef __OPENSTEP__
    static struct mach_header *mh = NULL;
	if(mh == NULL)
	    mh = _NSGetMachExecuteHeader();
#else /* defined(__OPENSTEP__) */
#ifdef __DYNAMIC__
    static struct mach_header *mh = NULL;
	if(mh == NULL)
	    _dyld_lookup_and_bind("__mh_execute_header",
		(unsigned long *)&mh, NULL);
#else
    struct mach_header *mh;
	mh = (struct mach_header *)&_mh_execute_header;
#endif
#endif /* __OPENSTEP__ */
	load_commands = (struct load_command *)
			((char *)mh + sizeof(struct mach_header));
	lc = load_commands;
	for(i = 0; i < mh->ncmds; i++){
	    if(lc->cmd == LC_LOAD_DYLIB){
		dl = (struct dylib_command *)lc;
		install_name = (char *)dl + dl->dylib.name.offset;
		if(names_match(install_name, libraryName) == TRUE)
		    return(dl->dylib.current_version);
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	return(-1);
}

/*
 * This routine returns the current version of the named shared library the
 * program it is running against.  The libraryName parameter is the same as
 * would be static link editor using the -lx or -framework Foo flags (with -lx
 * it would be "x" and with -framework Foo it would be "Foo").  If the program
 * is not using the specified library it returns -1.
 */
long
NSVersionOfRunTimeLibrary(
const char *libraryName)
{
    unsigned long i, j, n;
    char *install_name;
    struct load_command *load_commands, *lc;
    struct dylib_command *dl;
    struct mach_header *mh;

	n = _dyld_image_count();
	for(i = 0; i < n; i++){
	    mh = _dyld_get_image_header(i);
	    if(mh->filetype != MH_DYLIB)
		continue;
	    load_commands = (struct load_command *)
			    ((char *)mh + sizeof(struct mach_header));
	    lc = load_commands;
	    for(j = 0; j < mh->ncmds; j++){
		if(lc->cmd == LC_ID_DYLIB){
		    dl = (struct dylib_command *)lc;
		    install_name = (char *)dl + dl->dylib.name.offset;
		    if(names_match(install_name, libraryName) == TRUE)
			return(dl->dylib.current_version);
		}
		lc = (struct load_command *)((char *)lc + lc->cmdsize);
	    }
	}
	return(-1);
}

/*
 * names_match() takes an install_name from an LC_LOAD_DYLIB command and a
 * libraryName (which is -lx or -framework Foo argument passed to the static
 * link editor for the same library) and determines if they match.  This depends
 * on conventional use of names including major versioning.
 */
static
enum bool
names_match(
char *install_name,
const char *libraryName)
{
    char *basename;
    unsigned long n;

	/*
	 * Conventional install names have these forms:
	 *	/System/Library/Frameworks/AppKit.framework/Versions/A/Appkit
	 *	/Local/Library/Frameworks/AppKit.framework/Appkit
	 *	/lib/libsys_s.A.dylib
	 *	/usr/lib/libsys_s.dylib
	 */
	basename = strrchr(install_name, '/');
	if(basename == NULL)
	    basename = install_name;
	else
	    basename++;

	/*
	 * By checking the base name matching the library name we take care
	 * of the -framework cases.
	 */
	if(strcmp(basename, libraryName) == 0)
	    return(TRUE);

	/*
	 * Now check the base name for "lib" if so proceed to check for the
	 * -lx case dealing with a possible .X.dylib and a .dylib extension.
	 */
	if(strncmp(basename, "lib", 3) ==0){
	    n = strlen(libraryName);
	    if(strncmp(basename+3, libraryName, n) == 0){
		if(strncmp(basename+3+n, ".dylib", 6) == 0)
		    return(TRUE);
		if(basename[3+n] == '.' &&
		   basename[3+n+1] != '\0' &&
		   strncmp(basename+3+n+2, ".dylib", 6) == 0)
		    return(TRUE);
	    }
	}
	return(FALSE);
}

