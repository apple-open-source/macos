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
#if defined(__MWERKS__) && !defined(__private_extern__)
#define __private_extern__ __declspec(private_extern)
#endif

/*
 * Global types, variables and routines declared in the file ld.c.
 *
 * The following include file need to be included before this file:
 * #include <sys/loader.h>
 * #include <mach.h>
 * #include <stdarg.h>  (included in <stdio.h>)
 */

/* Type for the possible levels of stripping, in increasing order */
enum strip_levels {
    STRIP_NONE,
    STRIP_DUP_INCLS,
    STRIP_L_SYMBOLS,
    STRIP_DEBUG,
    STRIP_NONGLOBALS,
    STRIP_ALL
};

/* The error level check for undefined symbols */
enum undefined_check_level {
    UNDEFINED_ERROR,
    UNDEFINED_WARNING,
    UNDEFINED_SUPPRESS
};

/* The error level check for read only relocs */
enum read_only_reloc_check_level {
    READ_ONLY_RELOC_ERROR,
    READ_ONLY_RELOC_WARNING,
    READ_ONLY_RELOC_SUPPRESS
};

/* name of this program as executed (argv[0]) */
__private_extern__ char *progname;
/* indication of an error set in error(), for processing a number of errors
   and then exiting */
__private_extern__ unsigned long errors;
/* the pagesize of the machine this program is running on, getpagesize() value*/
__private_extern__ unsigned long host_pagesize;
/* the byte sex of the machine this program is running on */
__private_extern__ enum byte_sex host_byte_sex;

/* name of output file */
__private_extern__ char *outputfile;
/* type of output file */
__private_extern__ unsigned long filetype;

/*
 * The architecture of the output file as specified by -arch and the cputype
 * and cpusubtype of the object files being loaded which will be the output
 * cputype and cpusubtype.  specific_arch_flag is true if an -arch flag is
 * specified and the flag for a specific implementation of an architecture.
 */
__private_extern__ struct arch_flag arch_flag;
__private_extern__ enum bool specific_arch_flag;

/*
 * The -force_cpusubtype_ALL flag.
 */
__private_extern__ enum bool force_cpusubtype_ALL;

/* the byte sex of the output file */
__private_extern__ enum byte_sex target_byte_sex;

__private_extern__
enum bool trace;		/* print stages of link-editing */
__private_extern__
enum bool save_reloc;		/* save relocation information */
__private_extern__
enum bool output_for_dyld;	/* produce output for use with dyld */
__private_extern__
enum bool bind_at_load;		/* mark the output for dyld to be bound
				   when loaded */
__private_extern__
enum bool lazy_init;		/* mark the shared library to have its
				   init routine to be run lazily via
				   catching memory faults to its
				   writeable segments */
__private_extern__
enum bool load_map;		/* print a load map */
__private_extern__
enum bool define_comldsyms;	/* define common and link-editor defined
					   symbol reguardless of file type */
__private_extern__
enum bool seglinkedit;		/* create the link edit segment */
__private_extern__
enum bool whyload;		/* print why archive members are 
					   loaded */
__private_extern__
enum bool flush;		/* Use the output_flush routine to flush
				   output file by pages */
__private_extern__
enum bool sectorder_detail;	/* print sectorder warnings in detail */
__private_extern__
enum bool nowarnings;		/* suppress warnings */
__private_extern__
enum bool no_arch_warnings;	/* suppress wrong arch warnings */
__private_extern__
enum bool arch_errors_fatal;	/* cause wrong arch errors to be fatal */
__private_extern__
enum bool archive_ObjC;		/* objective-C archive semantics */
__private_extern__
enum bool archive_all;		/* always load everything in archives */
__private_extern__
enum bool keep_private_externs;	/* don't turn private externs into
					   non-external symbols */
/* TRUE if -dynamic is specified, FALSE if -static is specified */
__private_extern__ enum bool dynamic;

/* The level of symbol table stripping */
__private_extern__ enum strip_levels strip_level;
/* Strip the base file symbols (the -A argument's symbols) */
__private_extern__ enum bool strip_base_symbols;

/* The list of symbols to be traced */
__private_extern__ char **trace_syms;
__private_extern__ unsigned long ntrace_syms;

/* The number of references of undefined symbols to print */
__private_extern__ unsigned long Yflag;

/* The list of allowed undefined symbols */
__private_extern__ char **undef_syms;
__private_extern__ unsigned long nundef_syms;

/* The list of -dylib_file arguments */
__private_extern__ char **dylib_files;
__private_extern__ unsigned long ndylib_files;

/* The checking for undefined symbols */
__private_extern__ enum undefined_check_level undefined_flag;

/* The checking for read only relocs */
__private_extern__ enum read_only_reloc_check_level read_only_reloc_flag;

/* The prebinding optimization */
__private_extern__ enum bool prebinding;

/* True if -m is specified to allow multiply symbols, as a warning */
__private_extern__ enum bool allow_multiply_defined_symbols;

/* The segment alignment */
__private_extern__ unsigned long segalign;
#ifndef RLD
__private_extern__ enum bool segalign_specified;
#endif !defined(RLD)
/* The size of pagezero from the -pagezero_size flag */
__private_extern__ unsigned long pagezero_size;
/* The maximum segment alignment allowed to be specified, in hex */
#define MAXSEGALIGN		0x8000
/* The default section alignment */
__private_extern__ unsigned long defaultsectalign;
/* The maximum section alignment allowed to be specified, as a power of two */
#define MAXSECTALIGN		15 /* 2**15 or 0x8000 */
/* The default section alignment if not specified, as a power of two */
#define DEFAULTSECTALIGN	4  /* 2**4 or 16 */

/* The first segment address */
__private_extern__ unsigned long seg1addr;
__private_extern__ enum bool seg1addr_specified;

/* read-only and read-write segment addresses */
__private_extern__ unsigned long segs_read_only_addr;
__private_extern__ enum bool segs_read_only_addr_specified;
__private_extern__ unsigned long segs_read_write_addr;
__private_extern__ enum bool segs_read_write_addr_specified;

/* The stack address and size */
__private_extern__ unsigned long stack_addr;
__private_extern__ enum bool stack_addr_specified;
__private_extern__ unsigned long stack_size;
__private_extern__ enum bool stack_size_specified;

/* The header pad */
__private_extern__ unsigned long headerpad;

/* The name of the specified entry point */
__private_extern__ char *entry_point_name;

/* The name of the specified library initialization routine */
__private_extern__ char *init_name;

/* The dylib information */
__private_extern__ char *dylib_install_name;
__private_extern__ unsigned long dylib_current_version;
__private_extern__ unsigned long dylib_compatibility_version;

/* the umbrella/sub framework information */
__private_extern__ enum bool sub_framework;
__private_extern__ enum bool umbrella_framework;
__private_extern__ char *sub_framework_name;
__private_extern__ char *umbrella_framework_name;
__private_extern__ char *client_name;
__private_extern__ char **allowable_clients;
__private_extern__ unsigned long nallowable_clients;

/* The list of sub_umbrella frameworks */
__private_extern__ char **sub_umbrellas;
__private_extern__ unsigned long nsub_umbrellas;

/* The dylinker information */
__private_extern__ char *dylinker_install_name;

/* The value of the environment variable NEXT_ROOT */
__private_extern__ char *next_root;

/* TRUE if the environment variable RC_TRACE_ARCHIVES is set */
__private_extern__ enum bool rc_trace_archives;

/* TRUE if the environment variable RC_TRACE_DYLIBS is set */
__private_extern__ enum bool rc_trace_dylibs;

/* TRUE if the environment variable RC_TRACE_PREBINDING_DISABLED is set */
__private_extern__ enum bool rc_trace_prebinding_disabled;

/* the argument to -final_output if any */
__private_extern__ char *final_output;

/* The variables to support namespace options */
__private_extern__ enum bool namespace_specified;
__private_extern__ enum bool twolevel_namespace;
__private_extern__ enum bool force_flat_namespace;

__private_extern__ void *allocate(
    unsigned long size);
__private_extern__ void *reallocate(
    void *,
    unsigned long size);
__private_extern__ unsigned long round(
    unsigned long v,
    unsigned long r);
__private_extern__ void tell_ProjectBuilder(
    char *message);
__private_extern__ void print(
    const char *format, ...) __attribute__ ((format (printf, 1, 2)));
__private_extern__ void vprint(
    const char *format, va_list ap);
__private_extern__ void warning(
    const char *format, ...) __attribute__ ((format (printf, 1, 2)));
__private_extern__ void error(
    const char *format, ...) __attribute__ ((format (printf, 1, 2)));
__private_extern__ void fatal(
    const char *format, ...) __attribute__ ((format (printf, 1, 2)));
__private_extern__ void warning_with_cur_obj(
    const char *format, ...) __attribute__ ((format (printf, 1, 2)));
__private_extern__ void error_with_cur_obj(
    const char *format, ...) __attribute__ ((format (printf, 1, 2)));
__private_extern__ void system_warning(
    const char *format, ...) __attribute__ ((format (printf, 1, 2)));
__private_extern__ void system_error(
    const char *format, ...) __attribute__ ((format (printf, 1, 2)));
__private_extern__ void system_fatal(
    const char *format, ...) __attribute__ ((format (printf, 1, 2)));
__private_extern__ void mach_fatal(
    kern_return_t r,
    char *format, ...) __attribute__ ((format (printf, 2, 3)));

#ifdef DEBUG
__private_extern__ unsigned long debug;		/* link-editor debugging */
#endif DEBUG
