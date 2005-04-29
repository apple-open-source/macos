/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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


#include <sys/types.h>
#include <sys/stat.h> 
#include <sys/mman.h>
#include <unistd.h> 
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include <ar.h>

#include <algorithm>
#include <vector>
#include <set>

#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/reloc.h>
#include <mach-o/ppc/reloc.h>
#include <mach-o/ranlib.h>
#include <mach-o/stab.h>

#include "ObjectFile.h"
#include "Options.h"




// buiild Writer for -arch ppc64
#undef MACHO_32_SAME_ENDIAN
#undef MACHO_32_OPPOSITE_ENDIAN
#undef MACHO_64_SAME_ENDIAN
#undef MACHO_64_OPPOSITE_ENDIAN
#if __ppc__ || __ppc64__
	#define MACHO_64_SAME_ENDIAN
#elif __i386__
	#define MACHO_64_OPPOSITE_ENDIAN
#else
	#error unknown architecture
#endif
namespace ppc64 {
	#undef  ARCH_PPC
	#define ARCH_PPC64
	#undef  ARCH_I386
	#include "MachOAbstraction.h"
	#include "ObjectFileMachO.cpp"
	#include "ObjectFileDylibMachO.cpp"
	#include "ObjectFileArchiveMachO.cpp"
};

// buiild Writer for -arch ppc
#undef MACHO_32_SAME_ENDIAN
#undef MACHO_32_OPPOSITE_ENDIAN
#undef MACHO_64_SAME_ENDIAN
#undef MACHO_64_OPPOSITE_ENDIAN
#if __ppc__ || __ppc64__
	#define MACHO_32_SAME_ENDIAN
#elif __i386__
	#define MACHO_32_OPPOSITE_ENDIAN
#else
	#error unknown architecture
#endif
namespace ppc {
	#define ARCH_PPC
	#undef  ARCH_PPC64
	#undef  ARCH_I386
	#include "MachOAbstraction.h"
	#include "ObjectFileMachO.cpp"
	#include "ObjectFileDylibMachO.cpp"
	#include "ObjectFileArchiveMachO.cpp"
};


// buiild Writer for -arch i386
#undef MACHO_32_SAME_ENDIAN
#undef MACHO_32_OPPOSITE_ENDIAN
#undef MACHO_64_SAME_ENDIAN
#undef MACHO_64_OPPOSITE_ENDIAN
#if __ppc__ || __ppc64__
	#define MACHO_32_OPPOSITE_ENDIAN
#elif __i386__
	#define MACHO_32_SAME_ENDIAN
#else
	#error unknown architecture
#endif
#undef i386 // compiler sometimes #defines this
namespace i386 {
	#undef  ARCH_PPC
	#undef  ARCH_PPC64
	#define ARCH_I386
	#include "MachOAbstraction.h"
	#include "ObjectFileMachO.cpp"
	#include "ObjectFileDylibMachO.cpp"
	#include "ObjectFileArchiveMachO.cpp"
};


