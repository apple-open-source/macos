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
#import <stdlib.h>
#import <stdarg.h>
#import <string.h>
#import <errno.h>
#ifndef __OPENSTEP__
#import <crt_externs.h>
#endif
#import <mach/mach.h>
#import "stuff/openstep_mach.h"
#include <unistd.h>
#ifdef _POSIX_THREADS
#include <pthread.h>
#else
#import <mach/cthreads.h>
#endif
#import <mach/mach_error.h>

#import "stuff/bool.h"
#import "mach-o/dyld.h"
#import "stuff/ofile.h"
#import "stuff/arch.h"
#import "stuff/errors.h"

#import "ofi.h"

/*
 * These variables: progname and errors are along with the functions: vprint(),
 * print(), error(), system_error(), my_mach_error(), savestr() are needed to
 * use ofile_map() from ofile.c in libstuff.
 */
__private_extern__ char *progname = NULL;
__private_extern__ unsigned long errors = 0;

/*
 * The mutex ofi_error_printing_mutex and thread_printing_error is used to be
 * thread safe when printing.
 */
#ifdef _POSIX_THREADS
static pthread_mutex_t ofi_error_printing_mutex = PTHREAD_MUTEX_INITIALIZER;
#else
static mutex_t ofi_error_printing_mutex = NULL;
#endif
#ifdef __MACH30__
static thread_port_t thread_printing_error = MACH_PORT_NULL;
#else
static port_t thread_printing_error = MACH_PORT_NULL;
#endif

/*
 * This is the list of valid ofi structs created.  The creation and destruction
 * uses the ofi_alloc_mutex to be thread safe.  The user is responsible for
 * being thread safe with each ofi created and not destroying one in a thread
 * while using the same one in another thread.
 */
#ifdef _POSIX_THREADS
static pthread_mutex_t ofi_alloc_mutex = PTHREAD_MUTEX_INITIALIZER;
#else
static mutex_t ofi_alloc_mutex = NULL;
#endif
#define N_OFI 10
struct ofi_list {
    struct ofi list[N_OFI];
    enum bool used[N_OFI];
    struct ofi_list *next;
};
static struct ofi_list *ofi_lists = NULL;

static struct ofi * ofi_alloc(
    void);
static enum bool ofi_free(
    struct ofi *ofi);
static enum bool ofi_valid(
    struct ofi *ofi);

static NSObjectFileImageReturnCode NSCreateImageFromFile(
    enum bool coreFile,
    const char *pathName,
    NSObjectFileImage *objectFileImage);

/*
 * NSCreateObjectFileImageFromFile() creates an NSObjectFileImage for the
 * specified file name if the file is a correct Mach-O file that can be loaded
 * with NSloadModule().  For return codes of NSObjectFileImageFailure and
 * NSObjectFileImageFormat an error message is printed to stderr.  All
 * other codes cause no printing. 
 */
NSObjectFileImageReturnCode
NSCreateObjectFileImageFromFile(
const char *pathName,
NSObjectFileImage *objectFileImage)
{
	return(NSCreateImageFromFile(FALSE, pathName, objectFileImage));
}

/*
 * NSCreateCoreFileImageFromFile() creates an NSObjectFileImage for the 
 * specified core file name if the file is a correct Mach-O core file.
 * For return codes of NSObjectFileImageFailure and NSObjectFileImageFormat
 * an error message is printed to stderr.  All other codes cause no printing. 
 */
NSObjectFileImageReturnCode
NSCreateCoreFileImageFromFile(
const char *pathName,
NSObjectFileImage *objectFileImage)
{
	return(NSCreateImageFromFile(TRUE, pathName, objectFileImage));
}

/*
 * NSCreateImageFromFile() is the internal shared routine to implemement
 * NSCreateObjectFileImageFromFile() and NSCreateCoreFileImageFromFile().
 */
static
NSObjectFileImageReturnCode
NSCreateImageFromFile(
enum bool coreFile,
const char *pathName,
NSObjectFileImage *objectFileImage)
{
    struct arch_flag host_arch_flag;
    NSObjectFileImageReturnCode o;
    struct ofi *ofi;
#ifndef __OPENSTEP__
    static char ***NXArgv_pointer = NULL;

	if(NXArgv_pointer == NULL)
	    NXArgv_pointer = _NSGetArgv();
	progname = (*NXArgv_pointer)[0];
#else /* defined(__OPENSTEP__) */
#ifndef __DYNAMIC__
    extern char **NXArgv;

	progname = NXArgv[0];
#else
    static char ***NXArgv_pointer = NULL;

	if(NXArgv_pointer == NULL)
	    _dyld_lookup_and_bind("_NXArgv",
		(unsigned long *)&NXArgv_pointer, NULL);
	progname = (*NXArgv_pointer)[0];
#endif
#endif /* __OPENSTEP__ */
	*objectFileImage = NULL;

	if(get_arch_from_host(&host_arch_flag, NULL) == 0){
	    error("can't determine the host architecture (fix "
		  "get_arch_from_host() )");
	    o = NSObjectFileImageFailure;
	    goto done;
	}

	ofi = ofi_alloc();
	if(ofi == NULL){
	    error("can't allocate memory for NSObjectFileImage");
	    o = NSObjectFileImageFailure;
	    goto done;
	}

	o = ofile_map(pathName, &host_arch_flag, NULL, &(ofi->ofile), FALSE);
	if(o != NSObjectFileImageSuccess){
	    /* error was reported by ofile_map() if required */
	    (void)ofi_free(ofi);
	    goto done;
	}

	/*
	 * Check to make sure this is a Mach-O file.
	 */
	if(ofi->ofile.file_type != OFILE_Mach_O &&
	   (ofi->ofile.file_type != OFILE_FAT ||
	    ofi->ofile.arch_type != OFILE_Mach_O)){
	    o = NSObjectFileImageInappropriateFile;
	    ofile_unmap(&(ofi->ofile));
	    (void)ofi_free(ofi);
	    goto done;
	}

	/*
	 * Check to see if this Mach-O filetype is appropriate for loading
	 * into a program with NSloadModule().
	 */
	if(ofi->ofile.mh->filetype == MH_FVMLIB ||
	   (ofi->ofile.mh->filetype == MH_CORE && coreFile == FALSE) ||
	   ofi->ofile.mh->filetype == MH_DYLIB ||
	   ofi->ofile.mh->filetype == MH_DYLINKER){
	    o = NSObjectFileImageInappropriateFile;
	    ofile_unmap(&(ofi->ofile));
	    (void)ofi_free(ofi);
	    goto done;
	}

	/*
	 * For Mach-O filetypes that are not MH_BUNDLE an extra step to make
	 * them into a MH_BUNDLE type is needed.
	 */
	if(coreFile == FALSE && ofi->ofile.mh->filetype != MH_BUNDLE){
	    if(ofi->ofile.mh->filetype == MH_OBJECT ||
	       ofi->ofile.mh->filetype == MH_EXECUTE ||
	       ofi->ofile.mh->filetype == MH_PRELOAD){
		/* TODO: do an "ld -bundle" on this object */
		o = NSObjectFileImageInappropriateFile;
		ofile_unmap(&(ofi->ofile));
		(void)ofi_free(ofi);
		goto done;
	    }
	    else{
		/* some new unknown Mach-O filetype */
		o = NSObjectFileImageInappropriateFile;
		ofile_unmap(&(ofi->ofile));
		(void)ofi_free(ofi);
		goto done;
	    }
	}

	*objectFileImage = (NSObjectFileImage)ofi;
done:
	if(thread_printing_error == mach_thread_self()){
	    thread_printing_error = MACH_PORT_NULL;
#ifdef _POSIX_THREADS
	    pthread_mutex_unlock(&ofi_error_printing_mutex);
#else
	    mutex_unlock(ofi_error_printing_mutex);
#endif
	}
	return(o);
}

NSObjectFileImageReturnCode
NSCreateObjectFileImageFromMemory(
void *address,
unsigned long size, 
NSObjectFileImage *objectFileImage)
{
#ifndef __OPENSTEP__
    static char ***NXArgv_pointer = NULL;

	if(NXArgv_pointer == NULL)
	    NXArgv_pointer = _NSGetArgv();
	progname = (*NXArgv_pointer)[0];
#else /* defined(__OPENSTEP__) */
#ifndef __DYNAMIC__
    extern char **NXArgv;

	progname = NXArgv[0];
#else
    static char ***NXArgv_pointer = NULL;

	if(NXArgv_pointer == NULL)
	    _dyld_lookup_and_bind("_NXArgv",
		(unsigned long *)&NXArgv_pointer, NULL);
	progname = (*NXArgv_pointer)[0];
#endif
#endif /* __OPENSTEP__ */

	return(NSObjectFileImageFailure);
}

/*
 * ofi_alloc() returns an ofi struct to use from the list of structs allocated
 * structs and allocates lists as needed.
 */
static
struct ofi *
ofi_alloc(void)
{
    struct ofi_list **p, *ofi_list;
    unsigned long i;

#ifdef _POSIX_THREADS
	pthread_mutex_lock(&ofi_alloc_mutex);
#else
	/* this could of course could leak a mutex when first created */
	if(ofi_alloc_mutex == NULL)
	    ofi_alloc_mutex = mutex_alloc();
	mutex_lock(ofi_alloc_mutex);
#endif
	for(p = &ofi_lists; ; p = &((*p)->next)){
	    if(*p == NULL){
		ofi_list = malloc(sizeof(struct ofi_list));
		if(ofi_list == NULL){
#ifdef _POSIX_THREADS
		    pthread_mutex_unlock(&ofi_alloc_mutex);
#else
		    mutex_unlock(ofi_alloc_mutex);
#endif
		    return(NULL);
		}
		*p = ofi_list;
		memset(ofi_list, '\0', sizeof(struct ofi_list));
		ofi_list->used[0] = TRUE;
#ifdef _POSIX_THREADS
		pthread_mutex_unlock(&ofi_alloc_mutex);
#else
		mutex_unlock(ofi_alloc_mutex);
#endif
		return(ofi_list->list);
	    }
	    ofi_list = *p;
	    for(i = 0; i < N_OFI; i++){
		if(ofi_list->used[i] == FALSE){
		    memset(ofi_list->list + i, '\0', sizeof(struct ofi));
		    ofi_list->used[i] = TRUE;
#ifdef _POSIX_THREADS
		    pthread_mutex_unlock(&ofi_alloc_mutex);
#else
		    mutex_unlock(ofi_alloc_mutex);
#endif
		    return(ofi_list->list + i);
		}
	    }
	}
#ifdef _POSIX_THREADS
	pthread_mutex_unlock(&ofi_alloc_mutex);
#else
	mutex_unlock(ofi_alloc_mutex);
#endif
	return(NULL);
}

static
enum bool
ofi_free(
struct ofi *ofi)
{
    struct ofi_list **p, *ofi_list, *prev;
    unsigned long i, used;
    enum bool return_value;

#ifdef _POSIX_THREADS
	pthread_mutex_lock(&ofi_alloc_mutex);
#else
	/* this could of course could leak a mutex when first created */
	if(ofi_alloc_mutex == NULL)
	    ofi_alloc_mutex = mutex_alloc();
	mutex_lock(ofi_alloc_mutex);
#endif
	return_value = FALSE;
	prev = NULL;
	for(p = &ofi_lists; *p != NULL; p = &((*p)->next)){
	    ofi_list = *p;
	    used = 0;
	    for(i = 0; i < N_OFI; i++){
		if(ofi_list->used[i] == TRUE){
		    if(ofi == ofi_list->list + i){
			ofi_list->used[i] = FALSE;
			memset(ofi_list->list + i, '\0', sizeof(struct ofi));
			return_value = TRUE;
		    }
		    else{
			used++;
		    }
		}
	    }
	    if(used == 0){
		if(prev == NULL){
		    ofi_lists = ofi_list->next;
		}
		else{
		    prev->next = ofi_list->next;
		}
		free(ofi_list);
		break;
	    }
	    prev = ofi_list;
	}

#ifdef _POSIX_THREADS
	pthread_mutex_unlock(&ofi_alloc_mutex);
#else
	mutex_unlock(ofi_alloc_mutex);
#endif
	return(return_value);
}

static
enum bool
ofi_valid(
struct ofi *ofi)
{
    struct ofi_list *ofi_list;
    unsigned long i;

	for(ofi_list = ofi_lists; ofi_list != NULL; ofi_list = ofi_list->next){
	    for(i = 0; i < N_OFI; i++){
		if(ofi_list->list + i == ofi)
		    return(TRUE);
	    }
	}
	return(FALSE);
}

/*
 * All printing of all ofi messages goes through this function.
 */
__private_extern__
void
vprint(
const char *format,
va_list ap)
{
#ifndef _POSIX_THREADS
	/* this could of course could leak a mutex when first created */
	if(ofi_error_printing_mutex == NULL)
	    ofi_error_printing_mutex = mutex_alloc();
#endif

	if(thread_printing_error != mach_thread_self()){
#ifdef _POSIX_THREADS
	    pthread_mutex_lock(&ofi_error_printing_mutex);
#else
	    mutex_lock(ofi_error_printing_mutex);
#endif
	    thread_printing_error = mach_thread_self();
	}

	vfprintf(stderr, format, ap);
}

/*
 * The print function that just calls the above vprint() function.
 */
__private_extern__
void
print(
const char *format,
...)
{
    va_list ap;

	va_start(ap, format);
	vprint(format, ap);
	va_end(ap);
}

/*
 * Print the error message and return to the caller after setting the error
 * indication.
 */
__private_extern__
void
error(
const char *format,
...)
{
    va_list ap;

	va_start(ap, format);
        print("%s: ", progname);
	vprint(format, ap);
        print("\n");
	va_end(ap);
	errors++;
}

/*
 * Print the error message along with the system error message and return to
 * the caller after setting the error indication.
 */
__private_extern__
void
system_error(
const char *format,
...)
{
    va_list ap;

	va_start(ap, format);
        print("%s: ", progname);
	vprint(format, ap);
	print(" (%s)\n", strerror(errno));
	va_end(ap);
	errors++;
}

/*
 * Print the error message along with the mach error string.
 */
__private_extern__
void
my_mach_error(
kern_return_t r,
char *format,
...)
{
    va_list ap;

	va_start(ap, format);
        print("%s: ", progname);
	vprint(format, ap);
	print(" (%s)\n", mach_error_string(r));
	va_end(ap);
	errors++;
}

/*
 * savestr() malloc's space for the string passed to it, copys the string into
 * the space and returns a pointer to that space.  This version returns NULL if
 * malloc() returns NULL. 
 */
__private_extern__
char *
savestr(
const char *s)
{
    long len;
    char *r;

	len = strlen(s) + 1;
	r = (char *)malloc(len);
	if(r == NULL)
	    return(NULL);
	strcpy(r, s);
	return(r);
}

enum bool
NSDestroyObjectFileImage(
NSObjectFileImage objectFileImage)
{
    struct ofi *ofi;

	ofi = (struct ofi *)objectFileImage;
	if(ofi_valid(ofi) == FALSE)
	    return(FALSE);
	/* TODO: deal with not removing the user's memory in the case this was
	   not created from a file. */
	ofile_unmap(&(ofi->ofile));
	if(ofi_free(ofi) == FALSE)
	    return(FALSE);
	return(TRUE);
}

unsigned long
NSSymbolDefinitionCountInObjectFileImage(
NSObjectFileImage objectFileImage)
{
	return(0);
}

const char *
NSSymbolDefinitionNameInObjectFileImage(
NSObjectFileImage objectFileImage,
unsigned long ordinal)
{
	return(NULL);
}

unsigned long
NSSymbolReferenceCountInObjectFileImage(
NSObjectFileImage objectFileImage)
{
	return(0);
}

const char *
NSSymbolReferenceNameInObjectFileImage(
NSObjectFileImage objectFileImage,
unsigned long ordinal,
enum bool *tentative_definition) /* can be NULL */
{
	return(NULL);
}

enum bool
NSIsSymbolDefinedInObjectFileImage(
NSObjectFileImage objectFileImage,
const char *symbolName)
{
	return(FALSE);
}

void *
NSGetSectionDataInObjectFileImage(
NSObjectFileImage objectFileImage,
const char *segmentName,
const char *sectionName)
{
	return(NULL);
}
