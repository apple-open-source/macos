/*
 * Copyright (c) 2006-2008 Apple Computer, Inc.  All Rights Reserved.
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

#ifdef __APPLE__

#include <mach/machine.h>

#define I386_STRING		"i386"
#define X86_64_STRING		"x86_64"
#define ANY_CPU_STRING		"any"

static inline char*
string_for_arch(cpu_type_t arch)
{
	switch(arch) {
		case CPU_TYPE_I386:
			return I386_STRING;
		case CPU_TYPE_X86_64:
			return X86_64_STRING;
		case CPU_TYPE_ANY:
			return ANY_CPU_STRING;
		default:
			return NULL;
	}
}

static inline cpu_type_t
arch_for_string(const char* string)
{
	if(!strcmp(string, I386_STRING))
		return CPU_TYPE_I386;
	else if(!strcmp(string, X86_64_STRING))
		return CPU_TYPE_X86_64;
	else if(!strcmp(string, ANY_CPU_STRING))
		return CPU_TYPE_ANY;
	else
		return (cpu_type_t)0;
}

static inline int needs_swapping(cpu_type_t a, cpu_type_t b)
{
	switch(a) {
	case CPU_TYPE_I386:
	case CPU_TYPE_X86_64:
		if(b == CPU_TYPE_POWERPC || b == CPU_TYPE_POWERPC64)
			return 1;
		else
			return 0;
	case CPU_TYPE_POWERPC:
	case CPU_TYPE_POWERPC64:
		if(b == CPU_TYPE_I386 || b == CPU_TYPE_X86_64)
			return 1;
		else
			return 0;
	}
	
	return 0;
}

#if defined(__i386__)
#define host_arch CPU_TYPE_I386
#elif defined(__x86_64__)
#define host_arch CPU_TYPE_X86_64
#else
#error Unsupported architecture
#endif

#endif
