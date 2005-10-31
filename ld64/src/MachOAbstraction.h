/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*- 
 *
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
#include <stdint.h>
#include <string.h>
#include <mach-o/nlist.h>
#include <mach-o/loader.h>
#include <libkern/OSByteOrder.h>


#undef ENDIAN_READ16
#undef ENDIAN_WRITE16
#undef ENDIAN_READ32
#undef ENDIAN_WRITE32
#undef ENDIAN_SWAP64
#undef ENDIAN_SWAP_POINTER

#if defined(MACHO_32_SAME_ENDIAN) || defined(MACHO_64_SAME_ENDIAN)
	#define ENDIAN_READ16(x)			(x)
	#define ENDIAN_WRITE16(into, value) into = (value);
	
	#define ENDIAN_READ32(x)			(x)
	#define ENDIAN_WRITE32(into, value) into = (value);

	#define ENDIAN_SWAP64(x)			(x)
	
#elif defined(MACHO_32_OPPOSITE_ENDIAN) || defined(MACHO_64_OPPOSITE_ENDIAN)
	#define ENDIAN_READ16(x)			OSReadSwapInt16((uint16_t*)&(x), 0)
	#define ENDIAN_WRITE16(into, value)	OSWriteSwapInt16(&(into), 0, value);

	#define ENDIAN_READ32(x)			OSReadSwapInt32((uint32_t*)&(x), 0)
	#define ENDIAN_WRITE32(into, value) OSWriteSwapInt32(&(into), 0, value);
	
	#define ENDIAN_SWAP64(x)			OSSwapInt64(x)
	
#else
	#error file format undefined
#endif


#if defined(MACHO_64_SAME_ENDIAN) || defined(MACHO_64_OPPOSITE_ENDIAN)
	typedef uint64_t	macho_uintptr_t;
	typedef int64_t		macho_intptr_t;
#else
	typedef uint32_t	macho_uintptr_t;
	typedef int32_t		macho_intptr_t;
#endif

#if defined(MACHO_32_SAME_ENDIAN) 
	#define ENDIAN_SWAP_POINTER(x)				(x)
#elif defined(MACHO_64_SAME_ENDIAN)
	#define ENDIAN_SWAP_POINTER(x)				(x)
#elif defined(MACHO_32_OPPOSITE_ENDIAN) 
	#define ENDIAN_SWAP_POINTER(x)				OSSwapInt32(x)
#elif defined(MACHO_64_OPPOSITE_ENDIAN)
	#define ENDIAN_SWAP_POINTER(x)				OSSwapInt64(x)
#else
	#error file format undefined
#endif


#undef mach_header 
#undef mach_header_64 
class macho_header {
public:
	uint32_t	magic() const;
	void		set_magic(uint32_t);

	cpu_type_t	cputype() const;
	void		set_cputype(cpu_type_t);

	cpu_subtype_t	cpusubtype() const;
	void		set_cpusubtype(cpu_subtype_t);

	uint32_t	filetype() const;
	void		set_filetype(uint32_t);

	uint32_t	ncmds() const;
	void		set_ncmds(uint32_t);

	uint32_t	sizeofcmds() const;
	void		set_sizeofcmds(uint32_t);

	uint32_t	flags() const;
	void		set_flags(uint32_t);

	void		set_reserved();
	 
#if defined(MACHO_64_SAME_ENDIAN) 
	enum { size = sizeof(mach_header_64) };
	enum { magic_value = MH_MAGIC_64 };
#elif defined(MACHO_64_OPPOSITE_ENDIAN)
	enum { size = sizeof(mach_header_64) };
	enum { magic_value = MH_MAGIC_64 };
#elif defined(MACHO_32_SAME_ENDIAN)
	enum { size = sizeof(mach_header) };
	enum { magic_value = MH_MAGIC };
#elif defined(MACHO_32_OPPOSITE_ENDIAN)
	enum { size = sizeof(mach_header) };
	enum { magic_value = MH_MAGIC };
#endif

private:
#if defined(MACHO_64_SAME_ENDIAN) || defined(MACHO_64_OPPOSITE_ENDIAN)
	struct mach_header_64 content;
#else
	struct mach_header content;
#endif
};
#define mach_header __my_bad
#define mach_header_64 __my_bad

inline  __attribute__((always_inline))
uint32_t macho_header::magic() const {
	return ENDIAN_READ32(content.magic);
}

inline  __attribute__((always_inline))
void macho_header::set_magic(uint32_t _value) {
	ENDIAN_WRITE32(content.magic, _value);
}

inline  __attribute__((always_inline))
cpu_type_t macho_header::cputype() const {
	return ENDIAN_READ32(content.cputype);
}

inline  __attribute__((always_inline))
void macho_header::set_cputype(cpu_type_t _value) {
	ENDIAN_WRITE32(content.cputype, _value);
}

inline  __attribute__((always_inline))
cpu_subtype_t macho_header::cpusubtype() const {
	return ENDIAN_READ32(content.cpusubtype);
}

inline  __attribute__((always_inline))
void macho_header::set_cpusubtype(cpu_subtype_t _value) {
	ENDIAN_WRITE32(content.cpusubtype, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_header::filetype() const {
	return ENDIAN_READ32(content.filetype);
}

inline  __attribute__((always_inline))
void macho_header::set_filetype(uint32_t _value) {
	ENDIAN_WRITE32(content.filetype, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_header::ncmds() const {
	return ENDIAN_READ32(content.ncmds);
}

inline  __attribute__((always_inline))
void macho_header::set_ncmds(uint32_t _value) {
	ENDIAN_WRITE32(content.ncmds, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_header::sizeofcmds() const {
	return ENDIAN_READ32(content.sizeofcmds);
}

inline  __attribute__((always_inline))
void macho_header::set_sizeofcmds(uint32_t _value) {
	ENDIAN_WRITE32(content.sizeofcmds, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_header::flags() const {
	return ENDIAN_READ32(content.flags);
}

inline  __attribute__((always_inline))
void macho_header::set_flags(uint32_t _value) {
	ENDIAN_WRITE32(content.flags, _value);
}

inline  __attribute__((always_inline))
void macho_header::set_reserved() {
#if defined(MACHO_64_SAME_ENDIAN) || defined(MACHO_64_OPPOSITE_ENDIAN)
	content.reserved = 0;
#endif
}


#undef load_command
class macho_load_command {
public:
	uint32_t	cmd() const;
	void		set_cmd(uint32_t);

	uint32_t	cmdsize() const;
	void		set_cmdsize(uint32_t);
	
private:
	struct load_command content;
};

inline  __attribute__((always_inline))
uint32_t macho_load_command::cmd() const {
	return ENDIAN_READ32(content.cmd);
}

inline  __attribute__((always_inline))
void macho_load_command::set_cmd(uint32_t _value) {
	ENDIAN_WRITE32(content.cmd, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_load_command::cmdsize() const {
	return ENDIAN_READ32(content.cmdsize);
}

inline  __attribute__((always_inline))
void macho_load_command::set_cmdsize(uint32_t _value) {
	ENDIAN_WRITE32(content.cmdsize, _value);
}
#define load_command __my_bad


#undef segment_command
#undef segment_command_64
class macho_segment_command {
public:
	uint32_t	cmd() const;
	void		set_cmd(uint32_t);

	uint32_t	cmdsize() const;
	void		set_cmdsize(uint32_t);

	const char*	segname() const;
	void		set_segname(const char*);

	uint64_t	vmaddr() const;
	void		set_vmaddr(uint64_t);

	uint64_t	vmsize() const;
	void		set_vmsize(uint64_t);

	uint64_t	fileoff() const;
	void		set_fileoff(uint64_t);

	uint64_t	filesize() const;
	void		set_filesize(uint64_t);

	vm_prot_t	maxprot() const;
	void		set_maxprot(vm_prot_t);

	vm_prot_t	initprot() const;
	void		set_initprot(vm_prot_t);

	uint32_t	nsects() const;
	void		set_nsects(uint32_t);

	uint32_t	flags() const;
	void		set_flags(uint32_t);

	enum { size = 
#if defined(MACHO_64_SAME_ENDIAN) || defined(MACHO_64_OPPOSITE_ENDIAN)
	sizeof(segment_command_64) };
#else
	sizeof(segment_command) };
#endif

	enum { command = 
#if defined(MACHO_64_SAME_ENDIAN) || defined(MACHO_64_OPPOSITE_ENDIAN)
		LC_SEGMENT_64
#else
		LC_SEGMENT
#endif
	};

private:
#if defined(MACHO_64_SAME_ENDIAN) || defined(MACHO_64_OPPOSITE_ENDIAN)
	struct segment_command_64 content;
#else
	struct segment_command content;
#endif
};
#define segment_command __my_bad
#define segment_command_64 __my_bad

inline  __attribute__((always_inline))
uint32_t macho_segment_command::cmd() const {
	return ENDIAN_READ32(content.cmd);
}

inline  __attribute__((always_inline))
void macho_segment_command::set_cmd(uint32_t _value) {
	ENDIAN_WRITE32(content.cmd, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_segment_command::cmdsize() const {
	return ENDIAN_READ32(content.cmdsize);
}

inline  __attribute__((always_inline))
void macho_segment_command::set_cmdsize(uint32_t _value) {
	ENDIAN_WRITE32(content.cmdsize, _value);
}

inline  __attribute__((always_inline))
const char* macho_segment_command::segname() const {
	return content.segname;
}

inline  __attribute__((always_inline))
void macho_segment_command::set_segname(const char* _value) {
	strncpy(content.segname, _value, 16);
}

inline  __attribute__((always_inline))
uint64_t macho_segment_command::vmaddr() const {
#if defined(ARCH_PPC64)
	return ENDIAN_SWAP64(content.vmaddr);
#elif defined(ARCH_PPC) || defined(ARCH_I386)
	return ENDIAN_READ32(content.vmaddr);
#else
	#error unknown architecture
#endif
}

inline  __attribute__((always_inline))
void macho_segment_command::set_vmaddr(uint64_t _value) {
#if defined(ARCH_PPC64)
	content.vmaddr = ENDIAN_SWAP64(_value);
#elif defined(ARCH_PPC) || defined(ARCH_I386)
	ENDIAN_WRITE32(content.vmaddr, _value);
#else
	#error unknown architecture
#endif
}

inline  __attribute__((always_inline))
uint64_t macho_segment_command::vmsize() const {
#if defined(ARCH_PPC64)
	return ENDIAN_SWAP64(content.vmsize);
#elif defined(ARCH_PPC) || defined(ARCH_I386)
	return ENDIAN_READ32(content.vmsize);
#else
	#error unknown architecture
#endif
}

inline  __attribute__((always_inline))
void macho_segment_command::set_vmsize(uint64_t _value) {
#if defined(ARCH_PPC64)
	content.vmsize = ENDIAN_SWAP64(_value);
#elif defined(ARCH_PPC) || defined(ARCH_I386)
	ENDIAN_WRITE32(content.vmsize, _value);
#else
	#error unknown architecture
#endif
}

inline  __attribute__((always_inline))
uint64_t macho_segment_command::fileoff() const {
#if defined(ARCH_PPC64)
	return ENDIAN_SWAP64(content.fileoff);
#elif defined(ARCH_PPC) || defined(ARCH_I386)
	return ENDIAN_READ32(content.fileoff);
#else
	#error unknown architecture
#endif
}

inline  __attribute__((always_inline))
void macho_segment_command::set_fileoff(uint64_t _value) {
#if defined(ARCH_PPC64)
	content.fileoff = ENDIAN_SWAP64(_value);
#elif defined(ARCH_PPC) || defined(ARCH_I386)
	ENDIAN_WRITE32(content.fileoff, _value);
#else
	#error unknown architecture
#endif
}

inline  __attribute__((always_inline))
uint64_t macho_segment_command::filesize() const {
#if defined(ARCH_PPC64)
	return ENDIAN_SWAP64(content.filesize);
#elif defined(ARCH_PPC) || defined(ARCH_I386)
	return ENDIAN_READ32(content.filesize);
#else
	#error unknown architecture
#endif
}

inline  __attribute__((always_inline))
void macho_segment_command::set_filesize(uint64_t _value) {
#if defined(ARCH_PPC64)
	content.filesize = ENDIAN_SWAP64(_value);
#elif defined(ARCH_PPC) || defined(ARCH_I386)
	ENDIAN_WRITE32(content.filesize, _value);
#else
	#error unknown architecture
#endif
}

inline  __attribute__((always_inline))
vm_prot_t macho_segment_command::maxprot() const {
	return ENDIAN_READ32(content.maxprot);
}

inline  __attribute__((always_inline))
void macho_segment_command::set_maxprot(vm_prot_t _value) {
	ENDIAN_WRITE32(content.maxprot, _value);
}

inline  __attribute__((always_inline))
vm_prot_t macho_segment_command::initprot() const {
	return ENDIAN_READ32(content.initprot);
}

inline  __attribute__((always_inline))
void macho_segment_command::set_initprot(vm_prot_t _value) {
	ENDIAN_WRITE32(content.initprot, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_segment_command::nsects() const {
	return ENDIAN_READ32(content.nsects);
}

inline  __attribute__((always_inline))
void macho_segment_command::set_nsects(uint32_t _value) {
	ENDIAN_WRITE32(content.nsects, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_segment_command::flags() const {
	return ENDIAN_READ32(content.flags);
}

inline  __attribute__((always_inline))
void macho_segment_command::set_flags(uint32_t _value) {
	ENDIAN_WRITE32(content.flags, _value);
}

#undef section
#undef section_64
class macho_section {
public:
	const char*	sectname() const;
	void		set_sectname(const char*);

	const char*	segname() const;
	void		set_segname(const char*);

	uint64_t	addr() const;
	void		set_addr(uint64_t);

	uint64_t	size() const;
	void		set_size(uint64_t);

	uint32_t	offset() const;
	void		set_offset(uint32_t);

	uint32_t	align() const;
	void		set_align(uint32_t);

	uint32_t	reloff() const;
	void		set_reloff(uint32_t);

	uint32_t	nreloc() const;
	void		set_nreloc(uint32_t);

	uint32_t	flags() const;
	void		set_flags(uint32_t);

	uint32_t	reserved1() const;
	void		set_reserved1(uint32_t);

	uint32_t	reserved2() const;
	void		set_reserved2(uint32_t);

	enum { content_size = 
#if defined(MACHO_64_SAME_ENDIAN) || defined(MACHO_64_OPPOSITE_ENDIAN)
	sizeof(section_64) };
#else
	sizeof(section) };
#endif

private:
#if defined(MACHO_64_SAME_ENDIAN) || defined(MACHO_64_OPPOSITE_ENDIAN)
	struct section_64 content;
#else
	struct section content;
#endif
};
#define section __my_bad
#define section_64 __my_bad

inline  __attribute__((always_inline))
const char* macho_section::sectname() const {
	return content.sectname;
}

inline  __attribute__((always_inline))
void macho_section::set_sectname(const char* _value) {
	strncpy(content.sectname, _value, 16);
}

inline  __attribute__((always_inline))
const char* macho_section::segname() const {
	return content.segname;
}

inline  __attribute__((always_inline))
void macho_section::set_segname(const char* _value) {
	strncpy(content.segname, _value, 16);
}

inline  __attribute__((always_inline))
uint64_t macho_section::addr() const {
#if defined(ARCH_PPC64)
	return ENDIAN_SWAP64(content.addr);
#elif defined(ARCH_PPC) || defined(ARCH_I386)
	return ENDIAN_READ32(content.addr);
#else
	#error unknown architecture
#endif
}

inline  __attribute__((always_inline))
void macho_section::set_addr(uint64_t _value) {
#if defined(ARCH_PPC64)
	content.addr = ENDIAN_SWAP64(_value);
#elif defined(ARCH_PPC) || defined(ARCH_I386)
	ENDIAN_WRITE32(content.addr, _value);
#else
	#error unknown architecture
#endif
}

inline  __attribute__((always_inline))
uint64_t macho_section::size() const {
#if defined(ARCH_PPC64)
	return ENDIAN_SWAP64(content.size);
#elif defined(ARCH_PPC) || defined(ARCH_I386)
	return ENDIAN_READ32(content.size);
#else
	#error unknown architecture
#endif
}

inline  __attribute__((always_inline))
void macho_section::set_size(uint64_t _value) {
#if defined(ARCH_PPC64)
	content.size = ENDIAN_SWAP64(_value);
#elif defined(ARCH_PPC) || defined(ARCH_I386)
	ENDIAN_WRITE32(content.size, _value);
#else
	#error unknown architecture
#endif
}

inline  __attribute__((always_inline))
uint32_t macho_section::offset() const {
	return ENDIAN_READ32(content.offset);
}

inline  __attribute__((always_inline))
void macho_section::set_offset(uint32_t _value) {
	ENDIAN_WRITE32(content.offset, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_section::align() const {
	return ENDIAN_READ32(content.align);
}

inline  __attribute__((always_inline))
void macho_section::set_align(uint32_t _value) {
	ENDIAN_WRITE32(content.align, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_section::reloff() const {
	return ENDIAN_READ32(content.reloff);
}

inline  __attribute__((always_inline))
void macho_section::set_reloff(uint32_t _value) {
	ENDIAN_WRITE32(content.reloff, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_section::nreloc() const {
	return ENDIAN_READ32(content.nreloc);
}

inline  __attribute__((always_inline))
void macho_section::set_nreloc(uint32_t _value) {
	ENDIAN_WRITE32(content.nreloc, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_section::flags() const {
	return ENDIAN_READ32(content.flags);
}

inline  __attribute__((always_inline))
void macho_section::set_flags(uint32_t _value) {
	ENDIAN_WRITE32(content.flags, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_section::reserved1() const {
	return ENDIAN_READ32(content.reserved1);
}

inline  __attribute__((always_inline))
void macho_section::set_reserved1(uint32_t _value) {
	ENDIAN_WRITE32(content.reserved1, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_section::reserved2() const {
	return ENDIAN_READ32(content.reserved2);
}

inline  __attribute__((always_inline))
void macho_section::set_reserved2(uint32_t _value) {
	ENDIAN_WRITE32(content.reserved2, _value);
}

#undef dylib_command
class macho_dylib_command {
public:
	uint32_t	cmd() const;
	void		set_cmd(uint32_t);

	uint32_t	cmdsize() const;
	void		set_cmdsize(uint32_t);

	const char*	name() const;
	void		set_name_offset();

	uint32_t	timestamp() const;
	void		set_timestamp(uint32_t);

	uint32_t	current_version() const;
	void		set_current_version(uint32_t);

	uint32_t	compatibility_version() const;
	void		set_compatibility_version(uint32_t);

	enum { name_offset = sizeof(struct dylib_command) };

private:
	struct dylib_command content;
};
#define dylib_command __my_bad

inline  __attribute__((always_inline))
uint32_t macho_dylib_command::cmd() const {
	return ENDIAN_READ32(content.cmd);
}

inline  __attribute__((always_inline))
void macho_dylib_command::set_cmd(uint32_t _value) {
	ENDIAN_WRITE32(content.cmd, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dylib_command::cmdsize() const {
	return ENDIAN_READ32(content.cmdsize);
}

inline  __attribute__((always_inline))
void macho_dylib_command::set_cmdsize(uint32_t _value) {
	ENDIAN_WRITE32(content.cmdsize, _value);
}

inline  __attribute__((always_inline))
const char* macho_dylib_command::name() const {
	return (char*)(&content) + ENDIAN_READ32(content.dylib.name.offset);
}

inline  __attribute__((always_inline))
void macho_dylib_command::set_name_offset() {
	ENDIAN_WRITE32(content.dylib.name.offset, name_offset);
}

inline  __attribute__((always_inline))
uint32_t macho_dylib_command::timestamp() const {
	return ENDIAN_READ32(content.dylib.timestamp);
}

inline  __attribute__((always_inline))
void macho_dylib_command::set_timestamp(uint32_t _value) {
	ENDIAN_WRITE32(content.dylib.timestamp, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dylib_command::current_version() const {
	return ENDIAN_READ32(content.dylib.current_version);
}

inline  __attribute__((always_inline))
void macho_dylib_command::set_current_version(uint32_t _value) {
	ENDIAN_WRITE32(content.dylib.current_version, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dylib_command::compatibility_version() const {
	return ENDIAN_READ32(content.dylib.compatibility_version);
}

inline  __attribute__((always_inline))
void macho_dylib_command::set_compatibility_version(uint32_t _value) {
	ENDIAN_WRITE32(content.dylib.compatibility_version, _value);
}



#undef dylinker_command
class macho_dylinker_command {
public:
	uint32_t	cmd() const;
	void		set_cmd(uint32_t);

	uint32_t	cmdsize() const;
	void		set_cmdsize(uint32_t);

	void		set_name_offset();

	enum { name_offset = sizeof(struct dylinker_command) };

private:
	struct dylinker_command content;
};
#define dylinker_command __my_bad

inline  __attribute__((always_inline))
uint32_t macho_dylinker_command::cmd() const {
	return ENDIAN_READ32(content.cmd);
}

inline  __attribute__((always_inline))
void macho_dylinker_command::set_cmd(uint32_t _value) {
	ENDIAN_WRITE32(content.cmd, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dylinker_command::cmdsize() const {
	return ENDIAN_READ32(content.cmdsize);
}

inline  __attribute__((always_inline))
void macho_dylinker_command::set_cmdsize(uint32_t _value) {
	ENDIAN_WRITE32(content.cmdsize, _value);
}

inline  __attribute__((always_inline))
void macho_dylinker_command::set_name_offset() {
	ENDIAN_WRITE32(content.name.offset, name_offset);
}



#undef sub_framework_command 
class macho_sub_framework_command {
public:
	uint32_t	cmd() const;
	void		set_cmd(uint32_t);

	uint32_t	cmdsize() const;
	void		set_cmdsize(uint32_t);

	const char*	name() const;
	void		set_name_offset();

	enum { name_offset = sizeof(struct sub_framework_command) };

private:
	struct sub_framework_command content;
};
#define sub_framework_command __my_bad

inline  __attribute__((always_inline))
uint32_t macho_sub_framework_command::cmd() const {
	return ENDIAN_READ32(content.cmd);
}

inline  __attribute__((always_inline))
void macho_sub_framework_command::set_cmd(uint32_t _value) {
	ENDIAN_WRITE32(content.cmd, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_sub_framework_command::cmdsize() const {
	return ENDIAN_READ32(content.cmdsize);
}

inline  __attribute__((always_inline))
void macho_sub_framework_command::set_cmdsize(uint32_t _value) {
	ENDIAN_WRITE32(content.cmdsize, _value);
}

inline  __attribute__((always_inline))
const char* macho_sub_framework_command::name() const {
	return (char*)(&content) + ENDIAN_READ32(content.umbrella.offset);
}

inline  __attribute__((always_inline))
void macho_sub_framework_command::set_name_offset() {
	ENDIAN_WRITE32(content.umbrella.offset, name_offset);
}

#undef sub_client_command 
class macho_sub_client_command {
public:
	uint32_t	cmd() const;
	void		set_cmd(uint32_t);

	uint32_t	cmdsize() const;
	void		set_cmdsize(uint32_t);

	const char*	name() const;
	void		set_name_offset();

	enum { name_offset = sizeof(struct sub_client_command) };
private:
	struct sub_client_command content;
};
#define sub_client_command __my_bad

inline  __attribute__((always_inline))
uint32_t macho_sub_client_command::cmd() const {
	return ENDIAN_READ32(content.cmd);
}

inline  __attribute__((always_inline))
void macho_sub_client_command::set_cmd(uint32_t _value) {
	ENDIAN_WRITE32(content.cmd, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_sub_client_command::cmdsize() const {
	return ENDIAN_READ32(content.cmdsize);
}

inline  __attribute__((always_inline))
void macho_sub_client_command::set_cmdsize(uint32_t _value) {
	ENDIAN_WRITE32(content.cmdsize, _value);
}

inline  __attribute__((always_inline))
const char* macho_sub_client_command::name() const {
	return (char*)(&content) + ENDIAN_READ32(content.client.offset);
}

inline  __attribute__((always_inline))
void macho_sub_client_command::set_name_offset() {
	ENDIAN_WRITE32(content.client.offset, name_offset);
}



#undef sub_umbrella_command 
class macho_sub_umbrella_command {
public:
	uint32_t	cmd() const;
	void		set_cmd(uint32_t);

	uint32_t	cmdsize() const;
	void		set_cmdsize(uint32_t);

	const char*	name() const;
	void		set_name_offset();

	enum { name_offset = sizeof(struct sub_umbrella_command) };
private:
	struct sub_umbrella_command content;
};
#define sub_umbrella_command __my_bad

inline  __attribute__((always_inline))
uint32_t macho_sub_umbrella_command::cmd() const {
	return ENDIAN_READ32(content.cmd);
}

inline  __attribute__((always_inline))
void macho_sub_umbrella_command::set_cmd(uint32_t _value) {
	ENDIAN_WRITE32(content.cmd, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_sub_umbrella_command::cmdsize() const {
	return ENDIAN_READ32(content.cmdsize);
}

inline  __attribute__((always_inline))
void macho_sub_umbrella_command::set_cmdsize(uint32_t _value) {
	ENDIAN_WRITE32(content.cmdsize, _value);
}

inline  __attribute__((always_inline))
const char* macho_sub_umbrella_command::name() const {
	return (char*)(&content) + ENDIAN_READ32(content.sub_umbrella.offset);
}

inline  __attribute__((always_inline))
void macho_sub_umbrella_command::set_name_offset() {
	ENDIAN_WRITE32(content.sub_umbrella.offset, name_offset);
}




#undef sub_library_command 
class macho_sub_library_command {
public:
	uint32_t	cmd() const;
	void		set_cmd(uint32_t);

	uint32_t	cmdsize() const;
	void		set_cmdsize(uint32_t);

	const char*	name() const;
	void		set_name_offset();

	enum { name_offset = sizeof(struct sub_library_command) };
private:
	struct sub_library_command content;
};
#define sub_library_command __my_bad

inline  __attribute__((always_inline))
uint32_t macho_sub_library_command::cmd() const {
	return ENDIAN_READ32(content.cmd);
}

inline  __attribute__((always_inline))
void macho_sub_library_command::set_cmd(uint32_t _value) {
	ENDIAN_WRITE32(content.cmd, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_sub_library_command::cmdsize() const {
	return ENDIAN_READ32(content.cmdsize);
}

inline  __attribute__((always_inline))
void macho_sub_library_command::set_cmdsize(uint32_t _value) {
	ENDIAN_WRITE32(content.cmdsize, _value);
}

inline  __attribute__((always_inline))
const char* macho_sub_library_command::name() const {
	return (char*)(&content) + ENDIAN_READ32(content.sub_library.offset);
}

inline  __attribute__((always_inline))
void macho_sub_library_command::set_name_offset() {
	ENDIAN_WRITE32(content.sub_library.offset, name_offset);
}



#undef routines_command 
#undef routines_command_64 
class macho_routines_command {
public:
	uint32_t	cmd() const;
	void		set_cmd(uint32_t);

	uint32_t	cmdsize() const;
	void		set_cmdsize(uint32_t);

	uint64_t	init_address() const;
	void		set_init_address(uint64_t);

	uint64_t	init_module() const;
	void		set_init_module(uint64_t);

#if defined(MACHO_64_SAME_ENDIAN) || defined(MACHO_64_OPPOSITE_ENDIAN)
	enum { size = sizeof(struct routines_command_64) };
	enum { command = LC_ROUTINES_64 };
#else
	enum { size = sizeof(struct routines_command) };
	enum { command = LC_ROUTINES };
#endif

private:
#if defined(MACHO_64_SAME_ENDIAN) || defined(MACHO_64_OPPOSITE_ENDIAN)
	struct routines_command_64 content;
#else
	struct routines_command content;
#endif
};
#define routines_command __my_bad
#define routines_command_64 __my_bad

inline  __attribute__((always_inline))
uint32_t macho_routines_command::cmd() const {
	return ENDIAN_READ32(content.cmd);
}

inline  __attribute__((always_inline))
void macho_routines_command::set_cmd(uint32_t _value) {
	ENDIAN_WRITE32(content.cmd, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_routines_command::cmdsize() const {
	return ENDIAN_READ32(content.cmdsize);
}

inline  __attribute__((always_inline))
void macho_routines_command::set_cmdsize(uint32_t _value) {
	ENDIAN_WRITE32(content.cmdsize, _value);
}

inline  __attribute__((always_inline))
uint64_t macho_routines_command::init_address() const {
#if defined(ARCH_PPC64)
	return ENDIAN_SWAP64(content.init_address);
#elif defined(ARCH_PPC) || defined(ARCH_I386)
	return ENDIAN_READ32(content.init_address);
#else
	#error unknown architecture
#endif
}

inline  __attribute__((always_inline))
void macho_routines_command::set_init_address(uint64_t _value) {
#if defined(ARCH_PPC64)
	content.init_address = ENDIAN_SWAP64(_value);
#elif defined(ARCH_PPC) || defined(ARCH_I386)
	ENDIAN_WRITE32(content.init_address, _value);
#else
	#error unknown architecture
#endif
}

inline  __attribute__((always_inline))
uint64_t macho_routines_command::init_module() const {
#if defined(ARCH_PPC64)
	return ENDIAN_SWAP64(content.init_module);
#elif defined(ARCH_PPC) || defined(ARCH_I386)
	return ENDIAN_READ32(content.init_module);
#else
	#error unknown architecture
#endif
}

inline  __attribute__((always_inline))
void macho_routines_command::set_init_module(uint64_t _value) {
#if defined(ARCH_PPC64)
	content.init_module = ENDIAN_SWAP64(_value);
#elif defined(ARCH_PPC) || defined(ARCH_I386)
	ENDIAN_WRITE32(content.init_module, _value);
#else
	#error unknown architecture
#endif
}



#undef symtab_command 
class macho_symtab_command {
public:
	uint32_t	cmd() const;
	void		set_cmd(uint32_t);

	uint32_t	cmdsize() const;
	void		set_cmdsize(uint32_t);

	uint32_t	symoff() const;
	void		set_symoff(uint32_t);

	uint32_t	nsyms() const;
	void		set_nsyms(uint32_t);

	uint32_t	stroff() const;
	void		set_stroff(uint32_t);

	uint32_t	strsize() const;
	void		set_strsize(uint32_t);
	
	enum { size = sizeof(struct symtab_command ) };
	
private:
	struct symtab_command content;
};
#define symtab_command __my_bad


inline  __attribute__((always_inline))
uint32_t macho_symtab_command::cmd() const {
	return ENDIAN_READ32(content.cmd);
}

inline  __attribute__((always_inline))
void macho_symtab_command::set_cmd(uint32_t _value) {
	ENDIAN_WRITE32(content.cmd, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_symtab_command::cmdsize() const {
	return ENDIAN_READ32(content.cmdsize);
}

inline  __attribute__((always_inline))
void macho_symtab_command::set_cmdsize(uint32_t _value) {
	ENDIAN_WRITE32(content.cmdsize, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_symtab_command::symoff() const {
	return ENDIAN_READ32(content.symoff);
}

inline  __attribute__((always_inline))
void macho_symtab_command::set_symoff(uint32_t _value) {
	ENDIAN_WRITE32(content.symoff, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_symtab_command::nsyms() const {
	return ENDIAN_READ32(content.nsyms);
}

inline  __attribute__((always_inline))
void macho_symtab_command::set_nsyms(uint32_t _value) {
	ENDIAN_WRITE32(content.nsyms, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_symtab_command::stroff() const {
	return ENDIAN_READ32(content.stroff);
}

inline  __attribute__((always_inline))
void macho_symtab_command::set_stroff(uint32_t _value) {
	ENDIAN_WRITE32(content.stroff, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_symtab_command::strsize() const {
	return ENDIAN_READ32(content.strsize);
}

inline  __attribute__((always_inline))
void macho_symtab_command::set_strsize(uint32_t _value) {
	ENDIAN_WRITE32(content.strsize, _value);
}


#undef dysymtab_command 
class macho_dysymtab_command {
public:
	uint32_t	cmd() const;
	void		set_cmd(uint32_t);

	uint32_t	cmdsize() const;
	void		set_cmdsize(uint32_t);

	uint32_t	ilocalsym() const;
	void		set_ilocalsym(uint32_t);

	uint32_t	nlocalsym() const;
	void		set_nlocalsym(uint32_t);

	uint32_t	iextdefsym() const;
	void		set_iextdefsym(uint32_t);

	uint32_t	nextdefsym() const;
	void		set_nextdefsym(uint32_t);

	uint32_t	iundefsym() const;
	void		set_iundefsym(uint32_t);

	uint32_t	nundefsym() const;
	void		set_nundefsym(uint32_t);

	uint32_t	tocoff() const;
	void		set_tocoff(uint32_t);

	uint32_t	ntoc() const;
	void		set_ntoc(uint32_t);

	uint32_t	modtaboff() const;
	void		set_modtaboff(uint32_t);

	uint32_t	nmodtab() const;
	void		set_nmodtab(uint32_t);

	uint32_t	extrefsymoff() const;
	void		set_extrefsymoff(uint32_t);

	uint32_t	nextrefsyms() const;
	void		set_nextrefsyms(uint32_t);

	uint32_t	indirectsymoff() const;
	void		set_indirectsymoff(uint32_t);

	uint32_t	nindirectsyms() const;
	void		set_nindirectsyms(uint32_t);

	uint32_t	extreloff() const;
	void		set_extreloff(uint32_t);

	uint32_t	nextrel() const;
	void		set_nextrel(uint32_t);

	uint32_t	locreloff() const;
	void		set_locreloff(uint32_t);

	uint32_t	nlocrel() const;
	void		set_nlocrel(uint32_t);

	enum { size = sizeof(struct dysymtab_command ) };
private:
	struct dysymtab_command content;
};
#define dysymtab_command __my_bad

inline  __attribute__((always_inline))
uint32_t macho_dysymtab_command::cmd() const {
	return ENDIAN_READ32(content.cmd);
}

inline  __attribute__((always_inline))
void macho_dysymtab_command::set_cmd(uint32_t _value) {
	ENDIAN_WRITE32(content.cmd, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dysymtab_command::cmdsize() const {
	return ENDIAN_READ32(content.cmdsize);
}

inline  __attribute__((always_inline))
void macho_dysymtab_command::set_cmdsize(uint32_t _value) {
	ENDIAN_WRITE32(content.cmdsize, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dysymtab_command::ilocalsym() const {
	return ENDIAN_READ32(content.ilocalsym);
}

inline  __attribute__((always_inline))
void macho_dysymtab_command::set_ilocalsym(uint32_t _value) {
	ENDIAN_WRITE32(content.ilocalsym, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dysymtab_command::nlocalsym() const {
	return ENDIAN_READ32(content.nlocalsym);
}

inline  __attribute__((always_inline))
void macho_dysymtab_command::set_nlocalsym(uint32_t _value) {
	ENDIAN_WRITE32(content.nlocalsym, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dysymtab_command::iextdefsym() const {
	return ENDIAN_READ32(content.iextdefsym);
}

inline  __attribute__((always_inline))
void macho_dysymtab_command::set_iextdefsym(uint32_t _value) {
	ENDIAN_WRITE32(content.iextdefsym, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dysymtab_command::nextdefsym() const {
	return ENDIAN_READ32(content.nextdefsym);
}

inline  __attribute__((always_inline))
void macho_dysymtab_command::set_nextdefsym(uint32_t _value) {
	ENDIAN_WRITE32(content.nextdefsym, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dysymtab_command::iundefsym() const {
	return ENDIAN_READ32(content.iundefsym);
}

inline  __attribute__((always_inline))
void macho_dysymtab_command::set_iundefsym(uint32_t _value) {
	ENDIAN_WRITE32(content.iundefsym, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dysymtab_command::nundefsym() const {
	return ENDIAN_READ32(content.nundefsym);
}

inline  __attribute__((always_inline))
void macho_dysymtab_command::set_nundefsym(uint32_t _value) {
	ENDIAN_WRITE32(content.nundefsym, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dysymtab_command::tocoff() const {
	return ENDIAN_READ32(content.tocoff);
}

inline  __attribute__((always_inline))
void macho_dysymtab_command::set_tocoff(uint32_t _value) {
	ENDIAN_WRITE32(content.tocoff, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dysymtab_command::ntoc() const {
	return ENDIAN_READ32(content.ntoc);
}

inline  __attribute__((always_inline))
void macho_dysymtab_command::set_ntoc(uint32_t _value) {
	ENDIAN_WRITE32(content.ntoc, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dysymtab_command::modtaboff() const {
	return ENDIAN_READ32(content.modtaboff);
}

inline  __attribute__((always_inline))
void macho_dysymtab_command::set_modtaboff(uint32_t _value) {
	ENDIAN_WRITE32(content.modtaboff, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dysymtab_command::nmodtab() const {
	return ENDIAN_READ32(content.nmodtab);
}

inline  __attribute__((always_inline))
void macho_dysymtab_command::set_nmodtab(uint32_t _value) {
	ENDIAN_WRITE32(content.nmodtab, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dysymtab_command::extrefsymoff() const {
	return ENDIAN_READ32(content.extrefsymoff);
}

inline  __attribute__((always_inline))
void macho_dysymtab_command::set_extrefsymoff(uint32_t _value) {
	ENDIAN_WRITE32(content.extrefsymoff, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dysymtab_command::nextrefsyms() const {
	return ENDIAN_READ32(content.nextrefsyms);
}

inline  __attribute__((always_inline))
void macho_dysymtab_command::set_nextrefsyms(uint32_t _value) {
	ENDIAN_WRITE32(content.nextrefsyms, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dysymtab_command::indirectsymoff() const {
	return ENDIAN_READ32(content.indirectsymoff);
}

inline  __attribute__((always_inline))
void macho_dysymtab_command::set_indirectsymoff(uint32_t _value) {
	ENDIAN_WRITE32(content.indirectsymoff, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dysymtab_command::nindirectsyms() const {
	return ENDIAN_READ32(content.nindirectsyms);
}

inline  __attribute__((always_inline))
void macho_dysymtab_command::set_nindirectsyms(uint32_t _value) {
	ENDIAN_WRITE32(content.nindirectsyms, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dysymtab_command::extreloff() const {
	return ENDIAN_READ32(content.extreloff);
}

inline  __attribute__((always_inline))
void macho_dysymtab_command::set_extreloff(uint32_t _value) {
	ENDIAN_WRITE32(content.extreloff, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dysymtab_command::nextrel() const {
	return ENDIAN_READ32(content.nextrel);
}

inline  __attribute__((always_inline))
void macho_dysymtab_command::set_nextrel(uint32_t _value) {
	ENDIAN_WRITE32(content.nextrel, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dysymtab_command::locreloff() const {
	return ENDIAN_READ32(content.locreloff);
}

inline  __attribute__((always_inline))
void macho_dysymtab_command::set_locreloff(uint32_t _value) {
	ENDIAN_WRITE32(content.locreloff, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_dysymtab_command::nlocrel() const {
	return ENDIAN_READ32(content.nlocrel);
}

inline  __attribute__((always_inline))
void macho_dysymtab_command::set_nlocrel(uint32_t _value) {
	ENDIAN_WRITE32(content.nlocrel, _value);
}



#undef twolevel_hints_command 
class macho_twolevel_hints_command {
public:
	uint32_t	cmd() const;
	void		set_cmd(uint32_t);

	uint32_t	cmdsize() const;
	void		set_cmdsize(uint32_t);

	uint32_t	offset() const;
	void		set_offset(uint32_t);

	uint32_t	nhints() const;
	void		set_nhints(uint32_t);

private:
	struct twolevel_hints_command content;
};
#define twolevel_hints_command __my_bad

inline  __attribute__((always_inline))
uint32_t macho_twolevel_hints_command::cmd() const {
	return ENDIAN_READ32(content.cmd);
}

inline  __attribute__((always_inline))
void macho_twolevel_hints_command::set_cmd(uint32_t _value) {
	ENDIAN_WRITE32(content.cmd, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_twolevel_hints_command::cmdsize() const {
	return ENDIAN_READ32(content.cmdsize);
}

inline  __attribute__((always_inline))
void macho_twolevel_hints_command::set_cmdsize(uint32_t _value) {
	ENDIAN_WRITE32(content.cmdsize, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_twolevel_hints_command::offset() const {
	return ENDIAN_READ32(content.offset);
}

inline  __attribute__((always_inline))
void macho_twolevel_hints_command::set_offset(uint32_t _value) {
	ENDIAN_WRITE32(content.offset, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_twolevel_hints_command::nhints() const {
	return ENDIAN_READ32(content.nhints);
}

inline  __attribute__((always_inline))
void macho_twolevel_hints_command::set_nhints(uint32_t _value) {
	ENDIAN_WRITE32(content.nhints, _value);
}


#undef thread_command
class macho_thread_command {
public:
	uint32_t	cmd() const;
	void		set_cmd(uint32_t);

	uint32_t	cmdsize() const;
	void		set_cmdsize(uint32_t);

	uint32_t	flavor() const;
	void		set_flavor(uint32_t);

	uint32_t	count() const;
	void		set_count(uint32_t);

	uint32_t	threadState32(uint32_t index) const;
	void		set_threadState32(uint32_t index, uint32_t value);

	uint64_t	threadState64(uint32_t offset) const;
	void		set_threadState64(uint32_t index, uint64_t value);

	enum { size = sizeof(struct thread_command) + 8 };

private:
	struct thread_command	content;
	uint32_t				content_flavor;
	uint32_t				content_count;
	uint32_t				threadState[1];
};
#define thread_command __my_bad

inline  __attribute__((always_inline))
uint32_t macho_thread_command::cmd() const {
	return ENDIAN_READ32(content.cmd);
}

inline  __attribute__((always_inline))
void macho_thread_command::set_cmd(uint32_t _value) {
	ENDIAN_WRITE32(content.cmd, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_thread_command::cmdsize() const {
	return ENDIAN_READ32(content.cmdsize);
}

inline  __attribute__((always_inline))
void macho_thread_command::set_cmdsize(uint32_t _value) {
	ENDIAN_WRITE32(content.cmdsize, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_thread_command::flavor() const {
	return ENDIAN_READ32(content_flavor);
}

inline  __attribute__((always_inline))
void macho_thread_command::set_flavor(uint32_t _value) {
	ENDIAN_WRITE32(content_flavor, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_thread_command::count() const {
	return ENDIAN_READ32(content_count);
}

inline  __attribute__((always_inline))
void macho_thread_command::set_count(uint32_t _value) {
	ENDIAN_WRITE32(content_count, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_thread_command::threadState32(uint32_t index) const
{
	return ENDIAN_READ32(threadState[index]);
}

inline  __attribute__((always_inline))
void macho_thread_command::set_threadState32(uint32_t index, uint32_t _value)
{
	ENDIAN_WRITE32(threadState[index], _value);
}

inline  __attribute__((always_inline))
uint64_t macho_thread_command::threadState64(uint32_t index) const
{
	uint64_t temp = *((uint64_t*)(&threadState[index]));
	return ENDIAN_SWAP64(temp);
}

inline  __attribute__((always_inline))
void macho_thread_command::set_threadState64(uint32_t index, uint64_t _value)
{
	*((uint64_t*)(&threadState[index])) = ENDIAN_SWAP64(_value);
}



#undef nlist 
#undef nlist_64
class macho_nlist {
public:
	uint32_t	n_strx() const;
	void		set_n_strx(uint32_t);

	uint8_t		n_type() const;
	void		set_n_type(uint8_t);

	uint8_t		n_sect() const;
	void		set_n_sect(uint8_t);

	uint16_t	n_desc() const;
	void		set_n_desc(uint16_t);

	uint64_t	n_value() const;
	void		set_n_value(uint64_t);

	
#if defined(MACHO_64_SAME_ENDIAN) || defined(MACHO_64_OPPOSITE_ENDIAN)
	enum { size = sizeof(struct nlist_64) };
#else
	enum { size = sizeof(struct nlist) };
#endif

private:
#if defined(MACHO_64_SAME_ENDIAN) || defined(MACHO_64_OPPOSITE_ENDIAN)
	struct nlist_64 content;
#else
	struct nlist content;
#endif
};
#define nlist __my_bad
#define nlist_64 __my_bad

inline  __attribute__((always_inline))
uint32_t macho_nlist::n_strx() const {
	return ENDIAN_READ32(content.n_un.n_strx);
}

inline  __attribute__((always_inline))
void macho_nlist::set_n_strx(uint32_t _value) {
	ENDIAN_WRITE32(content.n_un.n_strx, _value);
}

inline  __attribute__((always_inline))
uint8_t macho_nlist::n_type() const {
	return content.n_type;
}

inline  __attribute__((always_inline))
void macho_nlist::set_n_type(uint8_t _value) {
	content.n_type = _value;
}

inline  __attribute__((always_inline))
uint8_t macho_nlist::n_sect() const {
	return content.n_sect;
}

inline  __attribute__((always_inline))
void macho_nlist::set_n_sect(uint8_t _value) {
	content.n_sect = _value;
}

inline  __attribute__((always_inline))
uint16_t macho_nlist::n_desc() const {
	return ENDIAN_READ16(content.n_desc);
}

inline  __attribute__((always_inline))
void macho_nlist::set_n_desc(uint16_t _value) {
	ENDIAN_WRITE16(content.n_desc, _value);
}

inline  __attribute__((always_inline))
uint64_t macho_nlist::n_value() const {
#if defined(ARCH_PPC64)
	return ENDIAN_SWAP64(content.n_value);
#elif defined(ARCH_PPC) || defined(ARCH_I386)
	return ENDIAN_READ32(content.n_value);
#else
	#error unknown architecture
#endif
}

inline  __attribute__((always_inline))
void macho_nlist::set_n_value(uint64_t _value) {
#if defined(ARCH_PPC64)
	content.n_value = ENDIAN_SWAP64(_value);
#elif defined(ARCH_PPC) || defined(ARCH_I386)
	ENDIAN_WRITE32(content.n_value, _value);
#else
	#error unknown architecture
#endif
}



#undef relocation_info 
class macho_relocation_info {
public:
	int32_t		r_address() const;
	void		set_r_address(int32_t);

	uint32_t	r_symbolnum() const;
	void		set_r_symbolnum(uint32_t);

	bool		r_pcrel() const;
	void		set_r_pcrel(bool);

	uint8_t		r_length() const;
	void		set_r_length(uint8_t);

	bool		r_extern() const;
	void		set_r_extern(bool);

	uint8_t		r_type() const;
	void		set_r_type(uint8_t);

	enum { size = sizeof(struct relocation_info) };
#if defined(MACHO_64_SAME_ENDIAN) || defined(MACHO_64_OPPOSITE_ENDIAN)
	enum { pointer_length = 3 };
#else
	enum { pointer_length = 2 };
#endif
	
private:
	struct relocation_info content;
};
#define relocation_info __my_bad


inline  __attribute__((always_inline))
int32_t macho_relocation_info::r_address() const {
	return ENDIAN_READ32(content.r_address);
}

inline  __attribute__((always_inline))
void macho_relocation_info::set_r_address(int32_t _value) {
	ENDIAN_WRITE32(content.r_address, _value);
}

inline  __attribute__((always_inline))
uint32_t macho_relocation_info::r_symbolnum() const {
	uint32_t temp = ENDIAN_READ32(((const uint32_t*)&content)[1]);
#if defined(ARCH_PPC) || defined(ARCH_PPC64)
	return (temp >> 8);
#elif defined(ARCH_I386)
	return temp & 0x00FFFFFF;
#else
	#error unknown architecture
#endif
}

inline  __attribute__((always_inline))
void macho_relocation_info::set_r_symbolnum(uint32_t _value) {
	uint32_t temp = ENDIAN_READ32(((const uint32_t*)&content)[1]);
#if defined(ARCH_PPC) || defined(ARCH_PPC64)
	temp &= 0x000000FF;
	temp |= ((_value & 0x00FFFFFF) << 8);
#elif defined(ARCH_I386)
	temp &= 0xFF000000;
	temp |= (_value & 0x00FFFFFF);
#else
	#error unknown architecture
#endif
	ENDIAN_WRITE32(((uint32_t*)&content)[1], temp);
}

inline  __attribute__((always_inline))
bool macho_relocation_info::r_pcrel() const {
	uint32_t temp = ENDIAN_READ32(((const uint32_t*)&content)[1]);
#if defined(ARCH_PPC) || defined(ARCH_PPC64)
	return ((temp & 0x00000080) != 0);
#elif defined(ARCH_I386)
	return ((temp & 0x01000000) != 0);
#else
	#error unknown architecture
#endif
}

inline  __attribute__((always_inline))
void macho_relocation_info::set_r_pcrel(bool _value) {
	uint32_t temp = ENDIAN_READ32(((const uint32_t*)&content)[1]);
#if defined(ARCH_PPC) || defined(ARCH_PPC64)
	temp &= 0xFFFFFF7F;
	if ( _value )
		temp |= 0x00000080;
#elif defined(ARCH_I386)
	temp &= 0xFEFFFFFF;
	if ( _value )
		temp |= 0x01000000;
#else
	#error unknown architecture
#endif
	ENDIAN_WRITE32(((uint32_t*)&content)[1], temp);
}

inline  __attribute__((always_inline))
uint8_t macho_relocation_info::r_length() const {
	uint32_t temp = ENDIAN_READ32(((const uint32_t*)&content)[1]);
#if defined(ARCH_PPC) || defined(ARCH_PPC64)
	return ((temp & 0x00000060) >> 5);
#elif defined(ARCH_I386)
	return ((temp & 0x06000000) >> 25);
#else
	#error unknown architecture
#endif
}

inline  __attribute__((always_inline))
void macho_relocation_info::set_r_length(uint8_t _value) {
	uint32_t temp = ENDIAN_READ32(((const uint32_t*)&content)[1]);
#if defined(ARCH_PPC) || defined(ARCH_PPC64)
	temp &= 0xFFFFFF9F;
	temp |= ((_value & 0x03) << 5);
#elif defined(ARCH_I386)
	temp &= 0xF9FFFFFF;
	temp |= ((_value & 0x03) << 25);
#else
	#error unknown architecture
#endif
	ENDIAN_WRITE32(((uint32_t*)&content)[1], temp);
}

inline  __attribute__((always_inline))
bool macho_relocation_info::r_extern() const {
	uint32_t temp = ENDIAN_READ32(((const uint32_t*)&content)[1]);
#if defined(ARCH_PPC) || defined(ARCH_PPC64)
	return ((temp & 0x00000010) != 0);
#elif defined(ARCH_I386)
	return ((temp & 0x08000000) != 0);
#else
	#error unknown architecture
#endif
}

inline  __attribute__((always_inline))
void macho_relocation_info::set_r_extern(bool _value) {
	uint32_t temp = ENDIAN_READ32(((const uint32_t*)&content)[1]);
#if defined(ARCH_PPC) || defined(ARCH_PPC64)
	temp &= 0xFFFFFFEF;
	if ( _value )
		temp |= 0x00000010;
#elif defined(ARCH_I386)
	temp &= 0xF7FFFFFF;
	if ( _value )
		temp |= 0x08000000;
#else
	#error unknown architecture
#endif
	ENDIAN_WRITE32(((uint32_t*)&content)[1], temp);
}

inline  __attribute__((always_inline))
uint8_t macho_relocation_info::r_type() const {
	uint32_t temp = ENDIAN_READ32(((const uint32_t*)&content)[1]);
#if defined(ARCH_PPC) || defined(ARCH_PPC64)
	return (temp & 0x0000000F);
#elif defined(ARCH_I386)
	return ((temp & 0xF0000000) >> 28);
#else
	#error unknown architecture
#endif
}

inline  __attribute__((always_inline))
void macho_relocation_info::set_r_type(uint8_t _value) {
	uint32_t temp = ENDIAN_READ32(((const uint32_t*)&content)[1]);
#if defined(ARCH_PPC) || defined(ARCH_PPC64)
	temp &= 0xFFFFFFF0;
	temp |= (_value & 0x0F);
#elif defined(ARCH_I386)
	temp &= 0x0FFFFFFF;
	temp |= ((_value & 0x0F) << 28);
#else
	#error unknown architecture
#endif
	ENDIAN_WRITE32(((uint32_t*)&content)[1], temp);
}



#undef scattered_relocation_info 
class macho_scattered_relocation_info {
public:
	bool		r_scattered() const;
	void		set_r_scattered(bool);

	bool		r_pcrel() const;
	void		set_r_pcrel(bool);

	uint8_t		r_length() const;
	void		set_r_length(uint8_t);

	uint8_t		r_type() const;
	void		set_r_type(uint8_t);

	uint32_t	r_address() const;
	void		set_r_address(uint32_t);

	int32_t		r_value() const;
	void		set_r_value(int32_t);

private:
	struct scattered_relocation_info content;
};
#define scattered_relocation_info __my_bad

inline  __attribute__((always_inline))
bool macho_scattered_relocation_info::r_scattered() const {
	uint32_t temp = *((const uint32_t*)&content);
	temp = ENDIAN_READ32(temp);
	return ((temp & 0x80000000) != 0);
}

inline  __attribute__((always_inline))
void macho_scattered_relocation_info::set_r_scattered(bool _value) {
	uint32_t temp = ENDIAN_READ32(*((const uint32_t*)&content));
	if ( _value )
		temp |= 0x80000000;
	else
		temp &= ~0x80000000;
	ENDIAN_WRITE32(*((uint32_t*)&content), temp);
}

inline  __attribute__((always_inline))
bool macho_scattered_relocation_info::r_pcrel() const {
	uint32_t temp = *((const uint32_t*)&content);
	temp = ENDIAN_READ32(temp);
	return ((temp & 0x40000000) != 0);
}

inline  __attribute__((always_inline))
void macho_scattered_relocation_info::set_r_pcrel(bool _value) {
	uint32_t temp = ENDIAN_READ32(*((const uint32_t*)&content));
	if ( _value )
		temp |= 0x40000000;
	else
		temp &= ~0x40000000;
	ENDIAN_WRITE32(*((uint32_t*)&content), temp);
}

inline  __attribute__((always_inline))
uint8_t macho_scattered_relocation_info::r_length() const {
	uint32_t temp = ENDIAN_READ32(*((const uint32_t*)&content));
	return (temp >> 28) & 0x03;
}

inline  __attribute__((always_inline))
void macho_scattered_relocation_info::set_r_length(uint8_t _value) {
	uint32_t temp = ENDIAN_READ32(*((const uint32_t*)&content));
	temp &= 0xCFFFFFFF;
	temp |= ((_value & 0x03) << 28);
	ENDIAN_WRITE32(*((uint32_t*)&content), temp);
}

inline  __attribute__((always_inline))
uint8_t macho_scattered_relocation_info::r_type() const {
	uint32_t temp = ENDIAN_READ32(*((const uint32_t*)&content));
	return (temp >> 24) & 0x0F;
}

inline  __attribute__((always_inline))
void macho_scattered_relocation_info::set_r_type(uint8_t _value) {
	uint32_t temp = ENDIAN_READ32(*((const uint32_t*)&content));
	temp &= 0xF0FFFFFF;
	temp |= ((_value &0x0F) << 24);
	ENDIAN_WRITE32(*((uint32_t*)&content), temp);
}

inline  __attribute__((always_inline))
uint32_t macho_scattered_relocation_info::r_address() const {
	uint32_t temp = ENDIAN_READ32(*((const uint32_t*)&content));
	return temp & 0x00FFFFFF;
}

inline  __attribute__((always_inline))
void macho_scattered_relocation_info::set_r_address(uint32_t _value) {
	uint32_t temp = ENDIAN_READ32(*((const uint32_t*)&content));
	_value &= 0x00FFFFFF;
	temp &= 0xFF000000;
	temp |= _value;
	ENDIAN_WRITE32(*((uint32_t*)&content), temp);
}

inline  __attribute__((always_inline))
int32_t macho_scattered_relocation_info::r_value() const {
	return ENDIAN_READ32(content.r_value);
}

inline  __attribute__((always_inline))
void macho_scattered_relocation_info::set_r_value(int32_t _value) {
	ENDIAN_WRITE32(content.r_value, _value);
}




