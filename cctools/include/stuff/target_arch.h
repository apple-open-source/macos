#ifndef _STUFF_TARGET_ARCH_H_
#define _STUFF_TARGET_ARCH_H_
#include <stdint.h>

#ifdef ARCH64	/* 64-bit architecutres */

typedef struct mach_header_64 mach_header_t;
#define MH_MAGIC_VALUE MH_MAGIC_64
#define MH_MAGIC_NAME "MH_MAGIC_64"
#define swap_mach_header_t swap_mach_header_64
#define CMDSIZE_ROUND 8
typedef struct segment_command_64 segment_command_t;
#define	LC_SEGMENT_VALUE LC_SEGMENT_64
#define LC_SEGMENT_NAME "LC_SEGMENT_64"
#define swap_segment_command_t swap_segment_command_64
typedef struct section_64 section_t;
#define swap_section_t swap_section_64
typedef struct routines_command_64 routines_command_t;
#define	LC_ROUTINES_VALUE LC_ROUTINES_64
#define	LC_ROUTINES_NAME "LC_ROUTINES_64"
#define swap_routines_command_t swap_routines_command_64
typedef struct dylib_module_64 dylib_module_t;
#define STRUCT_DYLIB_MODULE_NAME "struct dylib_module_64"
#define swap_dylib_module_t swap_dylib_module_64
typedef struct nlist_64 nlist_t;
#define STRUCT_NLIST_NAME "struct nlist_64"
#define swap_nlist_t swap_nlist_64

typedef uint64_t target_addr_t;
typedef int64_t signed_target_addr_t;
#define TA_DFMT "%llu"
#define TA_XFMT "%016llx"

#else		/* 32-bit architecutres */

typedef struct mach_header mach_header_t;
#define MH_MAGIC_VALUE MH_MAGIC
#define MH_MAGIC_NAME "MH_MAGIC"
#define swap_mach_header_t swap_mach_header
#define CMDSIZE_ROUND 4
typedef struct segment_command segment_command_t;
#define	LC_SEGMENT_VALUE LC_SEGMENT
#define LC_SEGMENT_NAME "LC_SEGMENT"
#define swap_segment_command_t swap_segment_command
typedef struct section section_t;
#define swap_section_t swap_section
typedef struct routines_command routines_command_t;
#define	LC_ROUTINES_VALUE LC_ROUTINES
#define	LC_ROUTINES_NAME "LC_ROUTINES"
#define swap_routines_command_t swap_routines_command
typedef struct dylib_module dylib_module_t;
#define STRUCT_DYLIB_MODULE_NAME "struct dylib_module"
#define swap_dylib_module_t swap_dylib_module
typedef struct nlist nlist_t;
#define STRUCT_NLIST_NAME "struct nlist"
#define swap_nlist_t swap_nlist

typedef uint32_t target_addr_t;
typedef int32_t signed_target_addr_t;
#define TA_DFMT "%u"
#define TA_XFMT "%08x"

#endif 

#endif /* _STUFF_TARGET_ARCH_H_ */
