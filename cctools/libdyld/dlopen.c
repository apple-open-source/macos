/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "stuff/bool.h"
#include "mach-o/dyld.h"
#include "ofi.h"
#include "dlfcn.h"

/*
 * The structure of a dlopen() handle.
 */
struct dlopen_handle {
    dev_t dev;		/* the path's device and inode number from stat(2) */
    ino_t ino; 
    int dlopen_mode;	/* current dlopen mode for this handle */
    int dlopen_count;	/* number of times dlopen() called on this handle */
    NSModule module;	/* the NSModule returned by NSLinkModule() */
    struct dlopen_handle *prev;
    struct dlopen_handle *next;
};
static struct dlopen_handle *dlopen_handles = NULL;
static const struct dlopen_handle main_program_handle = {NULL};
static char *dlerror_pointer = NULL;

/*
 * NSMakePrivateModulePublic() is not part of the public dyld API so we define
 * it here.  The internal dyld function pointer for
 * __dyld_NSMakePrivateModulePublic is returned so thats all that maters to get
 * the functionality need to implement the dlopen() interfaces.
 */
static
enum bool
NSMakePrivateModulePublic(
NSModule module)
{
    static enum bool (*p)(NSModule module) = NULL;

	if(p == NULL)
	    _dyld_func_lookup("__dyld_NSMakePrivateModulePublic",
			      (unsigned long *)&p);
	if(p == NULL){
#ifdef DEBUG
	    printf("_dyld_func_lookup of __dyld_NSMakePrivateModulePublic "
		   "failed\n");
#endif
	    return(FALSE);
	}
	return(p(module));
}

/*
 * dlopen() the MacOS X version of the FreeBSD dlopen() interface.
 */
void *
dlopen(
const char *path,
int mode)
{
    void *retval;
    struct stat stat_buf;
    NSObjectFileImage objectFileImage;
    NSObjectFileImageReturnCode ofile_result_code;
    NSModule module;
    NSLinkEditErrors NSLinkEditError_errorClass;
    int NSLinkEditError_errorNumber;
    char *NSLinkEditError_fileName, *NSLinkEditError_errorString;
    struct dlopen_handle *p;
    unsigned long options;
    NSSymbol NSSymbol;
    void (*init)(void);

	dlerror_pointer = NULL;
	/*
	 * A NULL path is to indicate the caller wants a handle for the
	 * main program.
 	 */
	if(path == NULL){
	    retval = (void *)&main_program_handle;
	    return(retval);
	}

	/* see if the path exists and if so get the device and inode number */
	if(stat(path, &stat_buf) == -1){
	    dlerror_pointer = strerror(errno);
	    return(NULL);
	}

	/*
	 * If we don't want an unshared handle see if we already have a handle
	 * for this path.
	 */
	if((mode & RTLD_UNSHARED) != RTLD_UNSHARED){
	    p = dlopen_handles;
	    while(p != NULL){
		if(p->dev == stat_buf.st_dev && p->ino == stat_buf.st_ino){
		    /* skip unshared handles */
		    if((p->dlopen_mode & RTLD_UNSHARED) == RTLD_UNSHARED)
			continue;
		    /*
		     * We have already created a handle for this path.  The
		     * caller might be trying to promote an RTLD_LOCAL handle
		     * to a RTLD_GLOBAL.  Or just looking it up with
		     * RTLD_NOLOAD.
		     */
		    if((p->dlopen_mode & RTLD_LOCAL) == RTLD_LOCAL &&
		       (mode & RTLD_GLOBAL) == RTLD_GLOBAL){
			/* promote the handle */
			if(NSMakePrivateModulePublic(p->module) == TRUE){
			    p->dlopen_mode &= ~RTLD_LOCAL;
			    p->dlopen_mode |= RTLD_GLOBAL;
			    p->dlopen_count++;
			    return(p);
			}
			else{
			    dlerror_pointer = "can't promote handle from "
					      "RTLD_LOCAL to RTLD_GLOBAL";
			    return(NULL);
			}
		    }
		    p->dlopen_count++;
		    return(p);
		}
		p = p->next;
	    }
	}
	
	/*
	 * We do not have a handle for this path if we were just trying to
	 * look it up return NULL to indicate we don't have it.
	 */
	if((mode & RTLD_NOLOAD) == RTLD_NOLOAD){
	    dlerror_pointer = "no existing handle for path RTLD_NOLOAD test";
	    return(NULL);
	}

	/* try to create an object file image from this path */
	ofile_result_code = NSCreateObjectFileImageFromFile(path,
							    &objectFileImage);
	if(ofile_result_code != NSObjectFileImageSuccess){
	    switch(ofile_result_code){
	    case NSObjectFileImageFailure:
		dlerror_pointer = "object file setup failure";
		return(NULL);
	    case NSObjectFileImageInappropriateFile:
		dlerror_pointer = "not a Mach-O MH_BUNDLE file type";
		return(NULL);
	    case NSObjectFileImageArch:
		dlerror_pointer = "no object for this architecture";
		return(NULL);
	    case NSObjectFileImageFormat:
		dlerror_pointer = "bad object file format";
		return(NULL);
	    case NSObjectFileImageAccess:
		dlerror_pointer = "can't read object file";
		return(NULL);
	    default:
		dlerror_pointer = "unknown error from "
				  "NSCreateObjectFileImageFromFile()";
		return(NULL);
	    }
	}

	/* try to link in this object file image */
	options = NSLINKMODULE_OPTION_PRIVATE |
		  NSLINKMODULE_OPTION_RETURN_ON_ERROR;
	if((mode & RTLD_NOW) == RTLD_NOW)
	    options |= NSLINKMODULE_OPTION_BINDNOW;
	module = NSLinkModule(objectFileImage, path, options);
	NSDestroyObjectFileImage(objectFileImage);
	if(module == NULL){
	    NSLinkEditError(&NSLinkEditError_errorClass,
			    &NSLinkEditError_errorNumber,
    			    &NSLinkEditError_fileName,
			    &NSLinkEditError_errorString);
	    if(NSLinkEditError_errorClass == NSLinkEditUnixResourceError)
		errno = NSLinkEditError_errorNumber;
	    dlerror_pointer = NSLinkEditError_errorString;
	    return(NULL);
	}

	/*
	 * If the handle is to be global promote the handle.  It is done this
	 * way to avoid multiply defined symbols.
	 */
	if((mode & RTLD_GLOBAL) == RTLD_GLOBAL){
	    if(NSMakePrivateModulePublic(module) == FALSE){
		dlerror_pointer = "can't promote handle from RTLD_LOCAL to "
				  "RTLD_GLOBAL";
		return(NULL);
	    }
	}

	p = malloc(sizeof(struct dlopen_handle));
	if(p == NULL){
	    dlerror_pointer = "can't allocate memory for the dlopen handle";
	    return(NULL);
	}

	/* fill in the handle */
	p->dev = stat_buf.st_dev;
        p->ino = stat_buf.st_ino;
	if(mode & RTLD_GLOBAL)
	    p->dlopen_mode = RTLD_GLOBAL;
	else
	    p->dlopen_mode = RTLD_LOCAL;
	p->dlopen_mode |= (mode & RTLD_UNSHARED) |
			  (mode & RTLD_NODELETE) |
			  (mode & RTLD_LAZY_UNDEF);
	p->dlopen_count = 1;
	p->module = module;
	p->prev = NULL;
	p->next = dlopen_handles;
	if(dlopen_handles != NULL)
	    dlopen_handles->prev = p;
	dlopen_handles = p;

	/* call the init function if one exists */
	NSSymbol = NSLookupSymbolInModule(p->module, "__init");
	if(NSSymbol != NULL){
	    init = NSAddressOfSymbol(NSSymbol);
	    init();
	}
	
	return(p);
}

/*
 * dlsym() the MacOS X version of the FreeBSD dlopen() interface.
 */
void *
dlsym(
void * handle,
const char *symbol)
{
    struct dlopen_handle *dlopen_handle, *p;
    NSSymbol NSSymbol;
    void *address;
    char *_symbol;

	_symbol = malloc(strlen(symbol) + 2);
	if(_symbol == NULL){
	    dlerror_pointer = "can't allocate memory for symbol name with "
			      "leading underbar";
	    return(NULL);
	}
	strcpy(_symbol, "_");
	strcat(_symbol, symbol);

	dlopen_handle = (struct dlopen_handle *)handle;
	/*
	 * If this is the handle for the main program do a global lookup.
	 */
	if(dlopen_handle == (struct dlopen_handle *)&main_program_handle){
	    if(NSIsSymbolNameDefined(_symbol) == TRUE){
		NSSymbol = NSLookupAndBindSymbol(_symbol);
		address = NSAddressOfSymbol(NSSymbol);
		dlerror_pointer = NULL;
		free(_symbol);
		return(address);
	    }
	    else{
		dlerror_pointer = "symbol not found";
		free(_symbol);
		return(NULL);
	    }
	}

	/*
	 * Find this handle and do a lookup in just this module.
	 */
	p = dlopen_handles;
	while(p != NULL){
	    if(dlopen_handle == p){
		NSSymbol = NSLookupSymbolInModule(p->module, _symbol);
		if(NSSymbol != NULL){
		    address = NSAddressOfSymbol(NSSymbol);
		    dlerror_pointer = NULL;
		    free(_symbol);
		    return(address);
		}
		else{
		    dlerror_pointer = "symbol not found";
		    free(_symbol);
		    return(NULL);
		}
	    }
	    p = p->next;
	}

	dlerror_pointer = "bad handle passed to dlsym()";
	free(_symbol);
	return(NULL);
}

/*
 * dlerror() the MacOS X version of the FreeBSD dlopen() interface.
 */
const char *
dlerror(
void)
{
    const char *p;

	p = (const char *)dlerror_pointer;
	dlerror_pointer = NULL;
	return(p);
}

/*
 * dlclose() the MacOS X version of the FreeBSD dlopen() interface.
 */
int
dlclose(
void * handle)
{
    struct dlopen_handle *p, *q;
    unsigned long options;
    NSSymbol NSSymbol;
    void (*fini)(void);

	dlerror_pointer = NULL;
	q = (struct dlopen_handle *)handle;
	p = dlopen_handles;
	while(p != NULL){
	    if(p == q){
		/* if the dlopen() count is not zero we are done */
		p->dlopen_count--;
		if(p->dlopen_count != 0)
		    return(0);

		/* call the fini function if one exists */
		NSSymbol = NSLookupSymbolInModule(p->module, "__fini");
		if(NSSymbol != NULL){
		    fini = NSAddressOfSymbol(NSSymbol);
		    fini();
		}

		/* unlink the module for this handle */
		options = 0;
		if(p->dlopen_mode & RTLD_NODELETE)
		    options |= NSUNLINKMODULE_OPTION_KEEP_MEMORY_MAPPED;
		if(p->dlopen_mode & RTLD_LAZY_UNDEF)
		    options |= NSUNLINKMODULE_OPTION_RESET_LAZY_REFERENCES;
		if(NSUnLinkModule(p->module, options) == FALSE){
		    dlerror_pointer = "NSUnLinkModule() failed for dlclose()";
		    return(-1);
		}
		if(p->prev != NULL)
		    p->prev->next = p->next;
		if(p->next != NULL)
		    p->next->prev = p->prev;
		if(dlopen_handles == p)
		    dlopen_handles = p->next;
		free(p);
		return(0);
	    }
	    p = p->next;
	}
	dlerror_pointer = "invalid handle passed to dlclose()";
	return(-1);
}
