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
#if defined(__HERA__) || defined(__GONZO_BUNSEN_BEAKER__) || \
    defined(__OPENSTEP__)
#define NO_DYLD_TRACING
#endif

#ifndef NO_DYLD_TRACING
#import <sys/syscall.h>
#import <unistd.h>
#import <sys/kdebug.h>
#endif /* defined(NO_DYLD_TRACING) */

extern enum bool dyld_trace;

/* DYLD Trace Class */
#define DYLD_CLASS			31

extern void trace_with_string(int trace_type, char *name);

/* DYLD Trace Subclasses */
#define DYLD_INIT_SUBCLASS		0
#define DYLD_CALLOUT_SUBCLASS		1
#define DYLD_LIBFUNC_SUBCLASS		2
#define DYLD_SYMBOL_SUBCLASS		3
#define DYLD_IMAGE_SUBCLASS		4
#define DYLD_TRACE_STRING_SUBCLASS	5

#define SYSCALL_CODE(Class, SubClass, code) \
(((Class & 0xff) << 24) | ((SubClass & 0xff) << 16) | ((code & 0x3fff)  << 2))

#ifndef NO_DYLD_TRACING

/*  init trace entries  */
#define DYLD_TRACE_INIT_START(code) \
if(dyld_trace){ syscall(180, \
    (SYSCALL_CODE(DYLD_CLASS, DYLD_INIT_SUBCLASS, code) | DBG_FUNC_START), \
    0, 0, 0, 0, 0);}
#define DYLD_TRACE_INIT_END(code) \
if(dyld_trace){ syscall(180, \
    (SYSCALL_CODE(DYLD_CLASS, DYLD_INIT_SUBCLASS, code) | DBG_FUNC_END), \
    0, 0, 0, 0, 0);}

#else /* defined(NO_DYLD_TRACING) */

#define DYLD_TRACE_INIT_START(code)
#define DYLD_TRACE_INIT_END(code)

#endif /* defined(NO_DYLD_TRACING) */

/* init trace entries */
#define DYLD_TRACE_initialize	0

#ifndef NO_DYLD_TRACING

/* callout trace entries */
#define DYLD_TRACE_CALLOUT_START(code, func) \
if(dyld_trace){ syscall(180, \
    (SYSCALL_CODE(DYLD_CLASS, DYLD_CALLOUT_SUBCLASS, code) | DBG_FUNC_START), \
    func, 0, 0, 0, 0);}
#define DYLD_TRACE_CALLOUT_END(code, func) \
if(dyld_trace){ syscall(180, \
    (SYSCALL_CODE(DYLD_CLASS, DYLD_CALLOUT_SUBCLASS, code) | DBG_FUNC_END), \
    func, 0, 0, 0, 0);}

#else /* defined(NO_DYLD_TRACING) */

#define DYLD_TRACE_CALLOUT_START(code, func)
#define DYLD_TRACE_CALLOUT_END(code, func)

#endif /* defined(NO_DYLD_TRACING) */

/* callout trace codes */
#define DYLD_TRACE_image_init_routine		0
#define DYLD_TRACE_dependent_init_routine	1
#define DYLD_TRACE_lazy_init_routine		2 /* obsolete */
#define DYLD_TRACE_module_init_for_library	3
#define DYLD_TRACE_module_init_for_object	4
#define DYLD_TRACE_module_terminator_for_object	5
#define DYLD_TRACE_module_init_for_dylib	6
#define DYLD_TRACE_mod_term_func		7
#define DYLD_TRACE_object_func			8
#define DYLD_TRACE_library_func			9
#define DYLD_TRACE_add_image_func		10
#define DYLD_TRACE_remove_image_func		11
#define DYLD_TRACE_link_object_module_func	12
#define DYLD_TRACE_link_library_module_func	13
#define DYLD_TRACE_link_module_func		14


#ifndef NO_DYLD_TRACING

#define DYLD_TRACE_LIBFUNC_NAMED_START(code, name) \
if(dyld_trace){ trace_with_string( \
    (SYSCALL_CODE(DYLD_CLASS, DYLD_LIBFUNC_SUBCLASS, code) | DBG_FUNC_START), \
    name);}
#define DYLD_TRACE_LIBFUNC_START(code) \
if(dyld_trace){ syscall(180, \
    (SYSCALL_CODE(DYLD_CLASS, DYLD_LIBFUNC_SUBCLASS, code) | DBG_FUNC_START), \
    0, 0, 0, 0, 0);}
#define DYLD_TRACE_LIBFUNC_END(code) \
if(dyld_trace){ syscall(180, \
    (SYSCALL_CODE(DYLD_CLASS, DYLD_LIBFUNC_SUBCLASS, code) | DBG_FUNC_END), \
    0, 0, 0, 0, 0);}

#else /* defined(NO_DYLD_TRACING) */

#define DYLD_TRACE_LIBFUNC_NAMED_START(code, name)
#define DYLD_TRACE_LIBFUNC_START(code)
#define DYLD_TRACE_LIBFUNC_END(code)

#endif /* defined(NO_DYLD_TRACING) */

/* DYLD Libfunc Trace Codes */
#define DYLD_TRACE_lookup_and_bind_with_hint			0
#define DYLD_TRACE_lookup_and_bind_fully			1
#define DYLD_TRACE_link_module					2
#define DYLD_TRACE_unlink_module				3
#define DYLD_TRACE_bind_objc_module				4
#define DYLD_TRACE_bind_fully_image_containing_address		5
#define DYLD_TRACE_make_delayed_module_initializer_calls	6
#define DYLD_TRACE_NSNameOfSymbol				7
#define DYLD_TRACE_NSAddressOfSymbol				8
#define DYLD_TRACE_NSModuleForSymbol				9
#define DYLD_TRACE_NSLookupAndBindSymbolWithHint		10
#define DYLD_TRACE_NSLookupSymbolInModule			11
#define DYLD_TRACE_NSLookupSymbolInImage			12
#define DYLD_TRACE_NSIsSymbolNameDefined			13
#define DYLD_TRACE_NSIsSymbolNameDefinedWithHint		14
#define DYLD_TRACE_NSIsSymbolNameDefinedInImage			15
#define DYLD_TRACE_NSNameOfModule				16
#define DYLD_TRACE_NSLibraryNameForModule			17
#define DYLD_TRACE_NSAddLibrary					18
#define DYLD_TRACE_NSAddLibraryWithSearching			19
#define DYLD_TRACE_NSAddImage					20
	
#ifndef NO_DYLD_TRACING

#define DYLD_TRACE_SYMBOLS_NAMED_START(code, name) \
if(dyld_trace){ trace_with_string( \
    (SYSCALL_CODE(DYLD_CLASS, DYLD_SYMBOL_SUBCLASS, code) | DBG_FUNC_START), \
    name);}
#define DYLD_TRACE_SYMBOLS_ADDRESSED_START(code, address) \
if(dyld_trace){ syscall(180, \
    (SYSCALL_CODE(DYLD_CLASS, DYLD_SYMBOL_SUBCLASS, code) | DBG_FUNC_START), \
    address, 0, 0, 0, 0);}
#define DYLD_TRACE_SYMBOLS_START(code) \
if(dyld_trace){ syscall(180, \
    (SYSCALL_CODE(DYLD_CLASS, DYLD_SYMBOL_SUBCLASS, code) | DBG_FUNC_START), \
     0, 0, 0, 0, 0);}
#define DYLD_TRACE_SYMBOLS_END(code) \
if(dyld_trace){ syscall(180, \
    (SYSCALL_CODE(DYLD_CLASS, DYLD_SYMBOL_SUBCLASS, code) | DBG_FUNC_END), \
    0, 0, 0, 0, 0);}

#else /* defined(NO_DYLD_TRACING) */

#define DYLD_TRACE_SYMBOLS_NAMED_START(code, name)
#define DYLD_TRACE_SYMBOLS_ADDRESSED_START(code, address)
#define DYLD_TRACE_SYMBOLS_START(code)
#define DYLD_TRACE_SYMBOLS_END(code)

#endif /* defined(NO_DYLD_TRACING) */

#define DYLD_TRACE_lookup_symbol		0
#define DYLD_TRACE_bind_lazy_symbol_reference	1
#define DYLD_TRACE_bind_symbol_by_name		2
#define DYLD_TRACE_link_in_need_modules		3

#ifndef NO_DYLD_TRACING

#define DYLD_TRACE_IMAGES_NAMED_START(code, name) \
if(dyld_trace){ trace_with_string( \
    (SYSCALL_CODE(DYLD_CLASS, DYLD_IMAGE_SUBCLASS, code) | DBG_FUNC_START), \
    name);} 
#define DYLD_TRACE_IMAGES_START(code) \
if(dyld_trace){ syscall(180, \
    (SYSCALL_CODE(DYLD_CLASS, DYLD_IMAGE_SUBCLASS, code) | DBG_FUNC_START), \
    0, 0, 0, 0, 0);}
#define DYLD_TRACE_IMAGES_END(code) \
if(dyld_trace){ syscall(180, \
    (SYSCALL_CODE(DYLD_CLASS, DYLD_IMAGE_SUBCLASS, code) | DBG_FUNC_END), \
    0, 0, 0, 0, 0);}

#else /* defined(NO_DYLD_TRACING) */

#define DYLD_TRACE_IMAGES_NAMED_START(code, name)
#define DYLD_TRACE_IMAGES_START(code)
#define DYLD_TRACE_IMAGES_END(code)

#endif /* defined(NO_DYLD_TRACING) */

#define DYLD_TRACE_map_image			0
#define DYLD_TRACE_load_executable_image	1
#define DYLD_TRACE_load_library_image		2
#define DYLD_TRACE_map_library_image		3
#define DYLD_TRACE_map_bundle_image		4
#define DYLD_TRACE_load_dependent_libraries	5
#define DYLD_TRACE_notify_prebinding_agent	6

#define DYLD_TRACE_string_type			0
