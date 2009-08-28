/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)xlate.m4	1.30	08/05/31 SMI"

#if !defined(__APPLE__)
#include <memory.h>
#include <libelf.h>
#include <link.h>
#include <sys/elf_SPARC.h>
#include <sys/elf_amd64.h>
#include <decl.h>
#include <msg.h>
#include <sgs.h>
#else /* is Apple Mac OS X */
#include <memory.h>
#include <libelf.h>
#include <link.h>
/* NOTHING */ /* In lieu of Solaris <sys/elf_SPARC.h> */
/* NOTHING */ /* In lieu of Solaris <sys/elf_amd64.h> */
#include <decl.h>
#include <msg.h>
/* NOTHING */ /* In lieu of Solaris <sys/sgs.h> */
#include <string.h>
#endif /* __APPLE__ */


/*
 * fmsize:  Array used to determine what size the the structures
 *	    are (for memory image & file image).
 *
 * x32:  Translation routines - to file & to memory.
 *
 * What must be done when adding a new type for conversion:
 *
 * The first question is whether you need a new ELF_T_* type
 * to be created.  If you've introduced a new structure - then
 * it will need to be described - this is done by:
 *
 * o adding a new type ELF_T_* to usr/src/head/libelf.h
 * o Create a new macro to define the bytes contained in the structure. Take a
 *   look at the 'Syminfo_1' macro defined below.  The declarations describe
 *   the structure based off of the field size of each element of the structure.
 * o Add a entry to the fmsize table for the new ELF_T_* type.
 * o Create a <newtype>_11_tof macro.  Take a look at 'syminfo_11_tof'.
 * o Create a <newtype>_11_tom macro.  Take a look at 'syminfo_11_tom'.
 * o The <newtype>_11_tof & <newtype>_11_tom results in conversion routines
 *   <newtype>_2L11_tof, <newtype>_2L11_tom, <newtype>_2M11_tof,
 *   <newtype>_2M11_tom being created in xlate.c.  These routines
 *   need to be added to the 'x32[]' array.
 * o Add entries to getdata.c::align32[] and getdata.c::align64[].  These
 *   tables define what the alignment requirements for a data type are.
 *
 * In order to tie a section header type (SHT_*) to a data
 * structure you need to update elf32_mtype() so that it can
 * make the association.  If you are introducing a new section built
 * on a basic datatype (SHT_INIT_ARRAY) then this is all the updating
 * that needs to be done.
 *
 *
 * ELF translation routines
 *	These routines make a subtle implicit assumption.
 *	The file representations of all structures are "packed,"
 *	meaning no implicit padding bytes occur.  This might not
 *	be the case for the memory representations.  Consequently,
 *	the memory representations ALWAYS contain at least as many
 *	bytes as the file representations.  Otherwise, the memory
 *	structures would lose information, meaning they're not
 *	implemented properly.
 *
 *	The words above apply to structures with the same members.
 *	If a future version changes the number of members, the
 *	relative structure sizes for different version must be
 *	tested with the compiler.
 */

#define	HI32	0x80000000UL
#define	LO31	0x7fffffffUL

/*
 *	These macros create indexes for accessing the bytes of
 *	words and halfwords for ELFCLASS32 data representations
 *	(currently ELFDATA2LSB and ELFDATA2MSB).  In all cases,
 *
 *	w = (((((X_3 << 8) + X_2) << 8) + X_1) << 8) + X_0
 *	h = (X_1 << 8) + X_0
 *
 *	These assume the file representations for Addr, Off,
 *	Sword, and Word use 4 bytes, but the memory def's for
 *	the types may differ.
 *
 *	Naming convention:
 *		..._L	ELFDATA2LSB
 *		..._M	ELFDATA2MSB
 *
 *	enuma_*(n)	define enum names for addr n
 *	enumb_*(n)	define enum names for byte n
 *	enumh_*(n)	define enum names for half n
 *	enumo_*(n)	define enum names for off n
 *	enumw_*(n)	define enum names for word n
 *	enuml_*(n)	define enum names for Lword n
 *	tofa(d,s,n)	xlate addr n from mem s to file d
 *	tofb(d,s,n)	xlate byte n from mem s to file d
 *	tofh(d,s,n)	xlate half n from mem s to file d
 *	tofo(d,s,n)	xlate off n from mem s to file d
 *	tofw(d,s,n)	xlate word n from mem s to file d
 *	tofl(d,s,n)	xlate Lword n from mem s to file d
 *	toma(s,n)	xlate addr n from file s to expression value
 *	tomb(s,n)	xlate byte n from file s to expression value
 *	tomh(s,n)	xlate half n from file s to expression value
 *	tomo(s,n)	xlate off n from file s to expression value
 *	tomw(s,n)	xlate word n from file s to expression value
 *	toml(s,n)	xlate Lword n from file s to expression value
 *
 *	tof*() macros must move a multi-byte value into a temporary
 *	because `in place' conversions are allowed.  If a temp is not
 *	used for multi-byte objects, storing an initial destination byte
 *	may clobber a source byte not yet examined.
 *
 *	tom*() macros compute an expression value from the source
 *	without touching the destination; so they're safe.
 */





/*
 * ELF data object indexes
 *	The enums are broken apart to get around deficiencies
 *	in some compilers.
 */




enum
{
	A_L0, A_L1, A_L2, A_L3
};

enum
{
	A_M3, A_M2, A_M1, A_M0,
	A_sizeof
};





enum
{
	H_L0, H_L1
};

enum
{
	H_M1, H_M0,
	H_sizeof
};




enum
{
	L_L0, L_L1, L_L2, L_L3, L_L4, L_L5, L_L6, L_L7
};

enum
{
	L_M7, L_M6, L_M5, L_M4, L_M3, L_M2, L_M1, L_M0,
	L_sizeof
};





enum
{
	M1_value_L0, M1_value_L1, M1_value_L2, M1_value_L3, M1_value_L4, M1_value_L5, M1_value_L6, M1_value_L7,
	M1_info_L0, M1_info_L1, M1_info_L2, M1_info_L3,
	M1_poffset_L0, M1_poffset_L1, M1_poffset_L2, M1_poffset_L3,
	M1_repeat_L0, M1_repeat_L1,
	M1_stride_L0, M1_stride_L1
};

enum
{
	M1_value_M7, M1_value_M6, M1_value_M5, M1_value_M4, M1_value_M3, M1_value_M2, M1_value_M1, M1_value_M0,
	M1_info_M3, M1_info_M2, M1_info_M1, M1_info_M0,
	M1_poffset_M3, M1_poffset_M2, M1_poffset_M1, M1_poffset_M0,
	M1_repeat_M1, M1_repeat_M0,
	M1_stride_M1, M1_stride_M0,
	M1_sizeof
};





enum
{
	MP1_value_L0, MP1_value_L1, MP1_value_L2, MP1_value_L3, MP1_value_L4, MP1_value_L5, MP1_value_L6, MP1_value_L7,
	MP1_info_L0, MP1_info_L1, MP1_info_L2, MP1_info_L3,
	MP1_poffset_L0, MP1_poffset_L1, MP1_poffset_L2, MP1_poffset_L3,
	MP1_repeat_L0, MP1_repeat_L1,
	MP1_stride_L0, MP1_stride_L1,
	MP1_padding_L0, MP1_padding_L1, MP1_padding_L2, MP1_padding_L3
};

enum
{
	MP1_value_M7, MP1_value_M6, MP1_value_M5, MP1_value_M4, MP1_value_M3, MP1_value_M2, MP1_value_M1, MP1_value_M0,
	MP1_info_M3, MP1_info_M2, MP1_info_M1, MP1_info_M0,
	MP1_poffset_M3, MP1_poffset_M2, MP1_poffset_M1, MP1_poffset_M0,
	MP1_repeat_M1, MP1_repeat_M0,
	MP1_stride_M1, MP1_stride_M0,
	MP1_padding_M3, MP1_padding_M2, MP1_padding_M1, MP1_padding_M0,
	MP1_sizeof
};





enum
{
	O_L0, O_L1, O_L2, O_L3
};

enum
{
	O_M3, O_M2, O_M1, O_M0,
	O_sizeof
};





enum
{
	W_L0, W_L1, W_L2, W_L3
};

enum
{
	W_M3, W_M2, W_M1, W_M0,
	W_sizeof
};





enum
{
	D1_tag_L0, D1_tag_L1, D1_tag_L2, D1_tag_L3,
	D1_val_L0, D1_val_L1, D1_val_L2, D1_val_L3
};

enum
{
	D1_tag_M3, D1_tag_M2, D1_tag_M1, D1_tag_M0,
	D1_val_M3, D1_val_M2, D1_val_M1, D1_val_M0,
	D1_sizeof
};


#define	E1_Nident	16




enum
{
	E1_ident, E1_ident_L_Z = E1_Nident - 1,
	E1_type_L0, E1_type_L1,
	E1_machine_L0, E1_machine_L1,
	E1_version_L0, E1_version_L1, E1_version_L2, E1_version_L3,
	E1_entry_L0, E1_entry_L1, E1_entry_L2, E1_entry_L3,
	E1_phoff_L0, E1_phoff_L1, E1_phoff_L2, E1_phoff_L3,
	E1_shoff_L0, E1_shoff_L1, E1_shoff_L2, E1_shoff_L3,
	E1_flags_L0, E1_flags_L1, E1_flags_L2, E1_flags_L3,
	E1_ehsize_L0, E1_ehsize_L1,
	E1_phentsize_L0, E1_phentsize_L1,
	E1_phnum_L0, E1_phnum_L1,
	E1_shentsize_L0, E1_shentsize_L1,
	E1_shnum_L0, E1_shnum_L1,
	E1_shstrndx_L0, E1_shstrndx_L1
};

enum
{
	E1_ident_M_Z = E1_Nident - 1,
	E1_type_M1, E1_type_M0,
	E1_machine_M1, E1_machine_M0,
	E1_version_M3, E1_version_M2, E1_version_M1, E1_version_M0,
	E1_entry_M3, E1_entry_M2, E1_entry_M1, E1_entry_M0,
	E1_phoff_M3, E1_phoff_M2, E1_phoff_M1, E1_phoff_M0,
	E1_shoff_M3, E1_shoff_M2, E1_shoff_M1, E1_shoff_M0,
	E1_flags_M3, E1_flags_M2, E1_flags_M1, E1_flags_M0,
	E1_ehsize_M1, E1_ehsize_M0,
	E1_phentsize_M1, E1_phentsize_M0,
	E1_phnum_M1, E1_phnum_M0,
	E1_shentsize_M1, E1_shentsize_M0,
	E1_shnum_M1, E1_shnum_M0,
	E1_shstrndx_M1, E1_shstrndx_M0,
	E1_sizeof
};




enum
{
	N1_namesz_L0, N1_namesz_L1, N1_namesz_L2, N1_namesz_L3,
	N1_descsz_L0, N1_descsz_L1, N1_descsz_L2, N1_descsz_L3,
	N1_type_L0, N1_type_L1, N1_type_L2, N1_type_L3
};

enum
{
	N1_namesz_M3, N1_namesz_M2, N1_namesz_M1, N1_namesz_M0,
	N1_descsz_M3, N1_descsz_M2, N1_descsz_M1, N1_descsz_M0,
	N1_type_M3, N1_type_M2, N1_type_M1, N1_type_M0,
	N1_sizeof
};




enum
{
	P1_type_L0, P1_type_L1, P1_type_L2, P1_type_L3,
	P1_offset_L0, P1_offset_L1, P1_offset_L2, P1_offset_L3,
	P1_vaddr_L0, P1_vaddr_L1, P1_vaddr_L2, P1_vaddr_L3,
	P1_paddr_L0, P1_paddr_L1, P1_paddr_L2, P1_paddr_L3,
	P1_filesz_L0, P1_filesz_L1, P1_filesz_L2, P1_filesz_L3,
	P1_memsz_L0, P1_memsz_L1, P1_memsz_L2, P1_memsz_L3,
	P1_flags_L0, P1_flags_L1, P1_flags_L2, P1_flags_L3,
	P1_align_L0, P1_align_L1, P1_align_L2, P1_align_L3
};

enum
{
	P1_type_M3, P1_type_M2, P1_type_M1, P1_type_M0,
	P1_offset_M3, P1_offset_M2, P1_offset_M1, P1_offset_M0,
	P1_vaddr_M3, P1_vaddr_M2, P1_vaddr_M1, P1_vaddr_M0,
	P1_paddr_M3, P1_paddr_M2, P1_paddr_M1, P1_paddr_M0,
	P1_filesz_M3, P1_filesz_M2, P1_filesz_M1, P1_filesz_M0,
	P1_memsz_M3, P1_memsz_M2, P1_memsz_M1, P1_memsz_M0,
	P1_flags_M3, P1_flags_M2, P1_flags_M1, P1_flags_M0,
	P1_align_M3, P1_align_M2, P1_align_M1, P1_align_M0,
	P1_sizeof
};





enum
{
	R1_offset_L0, R1_offset_L1, R1_offset_L2, R1_offset_L3,
	R1_info_L0, R1_info_L1, R1_info_L2, R1_info_L3
};

enum
{
	R1_offset_M3, R1_offset_M2, R1_offset_M1, R1_offset_M0,
	R1_info_M3, R1_info_M2, R1_info_M1, R1_info_M0,
	R1_sizeof
};





enum
{
	RA1_offset_L0, RA1_offset_L1, RA1_offset_L2, RA1_offset_L3,
	RA1_info_L0, RA1_info_L1, RA1_info_L2, RA1_info_L3,
	RA1_addend_L0, RA1_addend_L1, RA1_addend_L2, RA1_addend_L3
};

enum
{
	RA1_offset_M3, RA1_offset_M2, RA1_offset_M1, RA1_offset_M0,
	RA1_info_M3, RA1_info_M2, RA1_info_M1, RA1_info_M0,
	RA1_addend_M3, RA1_addend_M2, RA1_addend_M1, RA1_addend_M0,
	RA1_sizeof
};





enum
{
	SH1_name_L0, SH1_name_L1, SH1_name_L2, SH1_name_L3,
	SH1_type_L0, SH1_type_L1, SH1_type_L2, SH1_type_L3,
	SH1_flags_L0, SH1_flags_L1, SH1_flags_L2, SH1_flags_L3,
	SH1_addr_L0, SH1_addr_L1, SH1_addr_L2, SH1_addr_L3,
	SH1_offset_L0, SH1_offset_L1, SH1_offset_L2, SH1_offset_L3,
	SH1_size_L0, SH1_size_L1, SH1_size_L2, SH1_size_L3,
	SH1_link_L0, SH1_link_L1, SH1_link_L2, SH1_link_L3,
	SH1_info_L0, SH1_info_L1, SH1_info_L2, SH1_info_L3,
	SH1_addralign_L0, SH1_addralign_L1, SH1_addralign_L2, SH1_addralign_L3,
	SH1_entsize_L0, SH1_entsize_L1, SH1_entsize_L2, SH1_entsize_L3
};

enum
{
	SH1_name_M3, SH1_name_M2, SH1_name_M1, SH1_name_M0,
	SH1_type_M3, SH1_type_M2, SH1_type_M1, SH1_type_M0,
	SH1_flags_M3, SH1_flags_M2, SH1_flags_M1, SH1_flags_M0,
	SH1_addr_M3, SH1_addr_M2, SH1_addr_M1, SH1_addr_M0,
	SH1_offset_M3, SH1_offset_M2, SH1_offset_M1, SH1_offset_M0,
	SH1_size_M3, SH1_size_M2, SH1_size_M1, SH1_size_M0,
	SH1_link_M3, SH1_link_M2, SH1_link_M1, SH1_link_M0,
	SH1_info_M3, SH1_info_M2, SH1_info_M1, SH1_info_M0,
	SH1_addralign_M3, SH1_addralign_M2, SH1_addralign_M1, SH1_addralign_M0,
	SH1_entsize_M3, SH1_entsize_M2, SH1_entsize_M1, SH1_entsize_M0,
	SH1_sizeof
};





enum
{
	ST1_name_L0, ST1_name_L1, ST1_name_L2, ST1_name_L3,
	ST1_value_L0, ST1_value_L1, ST1_value_L2, ST1_value_L3,
	ST1_size_L0, ST1_size_L1, ST1_size_L2, ST1_size_L3,
	ST1_info_L,
	ST1_other_L,
	ST1_shndx_L0, ST1_shndx_L1
};

enum
{
	ST1_name_M3, ST1_name_M2, ST1_name_M1, ST1_name_M0,
	ST1_value_M3, ST1_value_M2, ST1_value_M1, ST1_value_M0,
	ST1_size_M3, ST1_size_M2, ST1_size_M1, ST1_size_M0,
	ST1_info_M,
	ST1_other_M,
	ST1_shndx_M1, ST1_shndx_M0,
	ST1_sizeof
};





enum
{
	SI1_boundto_L0, SI1_boundto_L1,
	SI1_flags_L0, SI1_flags_L1
};

enum
{
	SI1_boundto_M1, SI1_boundto_M0,
	SI1_flags_M1, SI1_flags_M0,
	SI1_sizeof
};





enum
{
	C1_tag_L0, C1_tag_L1, C1_tag_L2, C1_tag_L3,
	C1_val_L0, C1_val_L1, C1_val_L2, C1_val_L3
};

enum
{
	C1_tag_M3, C1_tag_M2, C1_tag_M1, C1_tag_M0,
	C1_val_M3, C1_val_M2, C1_val_M1, C1_val_M0,
	C1_sizeof
};





enum
{
	VD1_version_L0, VD1_version_L1,
	VD1_flags_L0, VD1_flags_L1,
	VD1_ndx_L0, VD1_ndx_L1,
	VD1_cnt_L0, VD1_cnt_L1,
	VD1_hash_L0, VD1_hash_L1, VD1_hash_L2, VD1_hash_L3,
	VD1_aux_L0, VD1_aux_L1, VD1_aux_L2, VD1_aux_L3,
	VD1_next_L0, VD1_next_L1, VD1_next_L2, VD1_next_L3
};

enum
{
	VD1_version_M1, VD1_version_M0,
	VD1_flags_M1, VD1_flags_M0,
	VD1_ndx_M1, VD1_ndx_M0,
	VD1_cnt_M1, VD1_cnt_M0,
	VD1_hash_M3, VD1_hash_M2, VD1_hash_M1, VD1_hash_M0,
	VD1_aux_M3, VD1_aux_M2, VD1_aux_M1, VD1_aux_M0,
	VD1_next_M3, VD1_next_M2, VD1_next_M1, VD1_next_M0,
	VD1_sizeof
};





enum
{
	VDA1_name_L0, VDA1_name_L1, VDA1_name_L2, VDA1_name_L3,
	VDA1_next_L0, VDA1_next_L1, VDA1_next_L2, VDA1_next_L3
};

enum
{
	VDA1_name_M3, VDA1_name_M2, VDA1_name_M1, VDA1_name_M0,
	VDA1_next_M3, VDA1_next_M2, VDA1_next_M1, VDA1_next_M0,
	VDA1_sizeof
};





enum
{
	VN1_version_L0, VN1_version_L1,
	VN1_cnt_L0, VN1_cnt_L1,
	VN1_file_L0, VN1_file_L1, VN1_file_L2, VN1_file_L3,
	VN1_aux_L0, VN1_aux_L1, VN1_aux_L2, VN1_aux_L3,
	VN1_next_L0, VN1_next_L1, VN1_next_L2, VN1_next_L3
};

enum
{
	VN1_version_M1, VN1_version_M0,
	VN1_cnt_M1, VN1_cnt_M0,
	VN1_file_M3, VN1_file_M2, VN1_file_M1, VN1_file_M0,
	VN1_aux_M3, VN1_aux_M2, VN1_aux_M1, VN1_aux_M0,
	VN1_next_M3, VN1_next_M2, VN1_next_M1, VN1_next_M0,
	VN1_sizeof
};





enum
{
	VNA1_hash_L0, VNA1_hash_L1, VNA1_hash_L2, VNA1_hash_L3,
	VNA1_flags_L0, VNA1_flags_L1,
	VNA1_other_L0, VNA1_other_L1,
	VNA1_name_L0, VNA1_name_L1, VNA1_name_L2, VNA1_name_L3,
	VNA1_next_L0, VNA1_next_L1, VNA1_next_L2, VNA1_next_L3
};

enum
{
	VNA1_hash_M3, VNA1_hash_M2, VNA1_hash_M1, VNA1_hash_M0,
	VNA1_flags_M1, VNA1_flags_M0,
	VNA1_other_M1, VNA1_other_M0,
	VNA1_name_M3, VNA1_name_M2, VNA1_name_M1, VNA1_name_M0,
	VNA1_next_M3, VNA1_next_M2, VNA1_next_M1, VNA1_next_M0,
	VNA1_sizeof
};


/*
 *	Translation function declarations.
 *
 *		<object>_<data><dver><sver>_tof
 *		<object>_<data><dver><sver>_tom
 *	where
 *		<data>	2L	ELFDATA2LSB
 *			2M	ELFDATA2MSB
 */

static void	addr_2L_tof(), addr_2L_tom(),
		addr_2M_tof(), addr_2M_tom(),
		byte_to(),
		dyn_2L11_tof(), dyn_2L11_tom(),
		dyn_2M11_tof(), dyn_2M11_tom(),
		ehdr_2L11_tof(), ehdr_2L11_tom(),
		ehdr_2M11_tof(), ehdr_2M11_tom(),
		half_2L_tof(), half_2L_tom(),
		half_2M_tof(), half_2M_tom(),
		move_2L11_tof(), move_2L11_tom(),
		move_2M11_tof(), move_2M11_tom(),
		movep_2L11_tof(), movep_2L11_tom(),
		movep_2M11_tof(), movep_2M11_tom(),
		off_2L_tof(), off_2L_tom(),
		off_2M_tof(), off_2M_tom(),
		note_2L11_tof(), note_2L11_tom(),
		note_2M11_tof(), note_2M11_tom(),
		phdr_2L11_tof(), phdr_2L11_tom(),
		phdr_2M11_tof(), phdr_2M11_tom(),
		rel_2L11_tof(), rel_2L11_tom(),
		rel_2M11_tof(), rel_2M11_tom(),
		rela_2L11_tof(), rela_2L11_tom(),
		rela_2M11_tof(), rela_2M11_tom(),
		shdr_2L11_tof(), shdr_2L11_tom(),
		shdr_2M11_tof(), shdr_2M11_tom(),
		sword_2L_tof(), sword_2L_tom(),
		sword_2M_tof(), sword_2M_tom(),
		sym_2L11_tof(), sym_2L11_tom(),
		sym_2M11_tof(), sym_2M11_tom(),
		syminfo_2L11_tof(), syminfo_2L11_tom(),
		syminfo_2M11_tof(), syminfo_2M11_tom(),
		word_2L_tof(), word_2L_tom(),
		word_2M_tof(), word_2M_tom(),
		verdef_2L11_tof(), verdef_2L11_tom(),
		verdef_2M11_tof(), verdef_2M11_tom(),
		verneed_2L11_tof(), verneed_2L11_tom(),
		verneed_2M11_tof(), verneed_2M11_tom(),
		cap_2L11_tof(), cap_2L11_tom(),
		cap_2M11_tof(), cap_2M11_tom();


/*	x32 [dst_version - 1] [src_version - 1] [encode - 1] [type]
 */

static struct {
	void	(*x_tof)(),
		(*x_tom)();
} x32 [EV_CURRENT] [EV_CURRENT] [ELFDATANUM - 1] [ELF_T_NUM] =
{
	{
		{
			{			/* [1-1][1-1][2LSB-1][.] */
/* BYTE */			{ byte_to, byte_to },
/* ADDR */			{ addr_2L_tof, addr_2L_tom },
/* DYN */			{ dyn_2L11_tof, dyn_2L11_tom },
/* EHDR */			{ ehdr_2L11_tof, ehdr_2L11_tom },
/* HALF */			{ half_2L_tof, half_2L_tom },
/* OFF */			{ off_2L_tof, off_2L_tom },
/* PHDR */			{ phdr_2L11_tof, phdr_2L11_tom },
/* RELA */			{ rela_2L11_tof, rela_2L11_tom },
/* REL */			{ rel_2L11_tof, rel_2L11_tom },
/* SHDR */			{ shdr_2L11_tof, shdr_2L11_tom },
/* SWORD */			{ sword_2L_tof, sword_2L_tom },
/* SYM */			{ sym_2L11_tof, sym_2L11_tom },
/* WORD */			{ word_2L_tof, word_2L_tom },
/* VERDEF */			{ verdef_2L11_tof, verdef_2L11_tom},
/* VERNEED */			{ verneed_2L11_tof, verneed_2L11_tom},
/* SXWORD */			{ 0, 0 },	/* illegal 32-bit op */
/* XWORD */			{ 0, 0 },	/* illegal 32-bit op */
/* SYMINFO */			{ syminfo_2L11_tof, syminfo_2L11_tom },
/* NOTE */			{ note_2L11_tof, note_2L11_tom },
/* MOVE */			{ move_2L11_tof, move_2L11_tom },
/* MOVEP */			{ movep_2L11_tof, movep_2L11_tom },
/* CAP */			{ cap_2L11_tof, cap_2L11_tom },
			},
			{			/* [1-1][1-1][2MSB-1][.] */
/* BYTE */			{ byte_to, byte_to },
/* ADDR */			{ addr_2M_tof, addr_2M_tom },
/* DYN */			{ dyn_2M11_tof, dyn_2M11_tom },
/* EHDR */			{ ehdr_2M11_tof, ehdr_2M11_tom },
/* HALF */			{ half_2M_tof, half_2M_tom },
/* OFF */			{ off_2M_tof, off_2M_tom },
/* PHDR */			{ phdr_2M11_tof, phdr_2M11_tom },
/* RELA */			{ rela_2M11_tof, rela_2M11_tom },
/* REL */			{ rel_2M11_tof, rel_2M11_tom },
/* SHDR */			{ shdr_2M11_tof, shdr_2M11_tom },
/* SWORD */			{ sword_2M_tof, sword_2M_tom },
/* SYM */			{ sym_2M11_tof, sym_2M11_tom },
/* WORD */			{ word_2M_tof, word_2M_tom },
/* VERDEF */			{ verdef_2M11_tof, verdef_2M11_tom},
/* VERNEED */			{ verneed_2M11_tof, verneed_2M11_tom},
/* SXWORD */			{ 0, 0 },	/* illegal 32-bit op */
/* XWORD */			{ 0, 0 },	/* illegal 32-bit op */
/* SYMINFO */			{ syminfo_2M11_tof, syminfo_2M11_tom },
/* NOTE */			{ note_2M11_tof, note_2M11_tom },
/* MOVE */			{ move_2M11_tof, move_2M11_tom },
/* MOVEP */			{ movep_2M11_tof, movep_2M11_tom },
/* CAP */			{ cap_2M11_tof, cap_2M11_tom },
			},
		},
	},
};


/*
 *	size [version - 1] [type]
 */

static const struct {
	size_t	s_filesz,
		s_memsz;
} fmsize [EV_CURRENT] [ELF_T_NUM] =
{
	{					/* [1-1][.] */
/* BYTE */	{ 1, 1 },
/* ADDR */	{ A_sizeof, sizeof (Elf32_Addr) },
/* DYN */	{ D1_sizeof, sizeof (Elf32_Dyn) },
/* EHDR */	{ E1_sizeof, sizeof (Elf32_Ehdr) },
/* HALF */	{ H_sizeof, sizeof (Elf32_Half) },
/* OFF */	{ O_sizeof, sizeof (Elf32_Off) },
/* PHDR */	{ P1_sizeof, sizeof (Elf32_Phdr) },
/* RELA */	{ RA1_sizeof, sizeof (Elf32_Rela) },
/* REL */	{ R1_sizeof, sizeof (Elf32_Rel) },
/* SHDR */	{ SH1_sizeof, sizeof (Elf32_Shdr) },
/* SWORD */	{ W_sizeof, sizeof (Elf32_Sword) },
/* SYM */	{ ST1_sizeof, sizeof (Elf32_Sym) },
/* WORD */	{ W_sizeof, sizeof (Elf32_Word) },
/* VERDEF */	{ 1, 1},	/* because bot VERDEF & VERNEED have varying */
/* VERNEED */	{ 1, 1},	/* sized structures we set their sizes */
				/* to 1 byte */
/* SXWORD */			{ 0, 0 },	/* illegal 32-bit op */
/* XWORD */			{ 0, 0 },	/* illegal 32-bit op */
/* SYMINFO */	{ SI1_sizeof, sizeof (Elf32_Syminfo) },
/* NOTE */	{ 1, 1},	/* NOTE has varying sized data we can't */
				/*  use the usual table magic. */
/* MOVE */	{ M1_sizeof, sizeof (Elf32_Move) },
/* MOVEP */	{ MP1_sizeof, sizeof (Elf32_Move) },
/* CAP */	{ C1_sizeof, sizeof (Elf32_Cap) },
	},
};


/*
 *	memory type [version - 1] [section type]
 */

static const Elf_Type	mtype[EV_CURRENT][SHT_NUM] =
{ 
	{			/* [1-1][.] */
/* NULL */		ELF_T_BYTE,
/* PROGBITS */		ELF_T_BYTE,
/* SYMTAB */		ELF_T_SYM,
/* STRTAB */		ELF_T_BYTE,
/* RELA */		ELF_T_RELA,
/* HASH */		ELF_T_WORD,
/* DYNAMIC */		ELF_T_DYN,
/* NOTE */		ELF_T_NOTE,
/* NOBITS */		ELF_T_BYTE,
/* REL */		ELF_T_REL,
/* SHLIB */		ELF_T_BYTE,
/* DYNSYM */		ELF_T_SYM,
/* UNKNOWN12 */		ELF_T_BYTE,
/* UNKNOWN13 */		ELF_T_BYTE,
/* INIT_ARRAY */	ELF_T_ADDR,
/* FINI_ARRAY */	ELF_T_ADDR,
/* PREINIT_ARRAY */	ELF_T_ADDR,
/* GROUP */		ELF_T_WORD,
/* SYMTAB_SHNDX */	ELF_T_WORD
	},
};


size_t
elf32_fsize(Elf_Type type, size_t count, unsigned ver)
{
	if (--ver >= EV_CURRENT) {
		_elf_seterr(EREQ_VER, 0);
		return (0);
	}
	if ((unsigned)type >= ELF_T_NUM) {
		_elf_seterr(EREQ_TYPE, 0);
		return (0);
	}
	return (fmsize[ver][type].s_filesz * count);
}


size_t
_elf32_msize(Elf_Type type, unsigned ver)
{
	return (fmsize[ver - 1][type].s_memsz);
}


Elf_Type
_elf32_mtype(Elf * elf, Elf32_Word shtype, unsigned ver)
{
	Elf32_Ehdr *	ehdr = (Elf32_Ehdr *)elf->ed_ehdr;

	if (shtype < SHT_NUM)
		return (mtype[ver - 1][shtype]);

	switch (shtype) {
	case SHT_SUNW_symsort:
	case SHT_SUNW_tlssort:
		return (ELF_T_WORD);
	case SHT_SUNW_LDYNSYM:
		return (ELF_T_SYM);
	case SHT_SUNW_dof:
		return (ELF_T_BYTE);
	case SHT_SUNW_cap:
		return (ELF_T_CAP);
	case SHT_SUNW_SIGNATURE:
		return (ELF_T_BYTE);
	case SHT_SUNW_ANNOTATE:
		return (ELF_T_BYTE);
	case SHT_SUNW_DEBUGSTR:
		return (ELF_T_BYTE);
	case SHT_SUNW_DEBUG:
		return (ELF_T_BYTE);
	case SHT_SUNW_move:
		/*
		 * 32bit sparc binaries have a padded
		 * MOVE structure.  So - return the
		 * appropriate type.
		 */
		if ((ehdr->e_machine == EM_SPARC) ||
		    (ehdr->e_machine == EM_SPARC32PLUS)) {
			return (ELF_T_MOVEP);
		}

		return (ELF_T_MOVE);
	case SHT_SUNW_COMDAT:
		return (ELF_T_BYTE);
	case SHT_SUNW_syminfo:
		return (ELF_T_SYMINFO);
	case SHT_SUNW_verdef:
		return (ELF_T_VDEF);
	case SHT_SUNW_verneed:
		return (ELF_T_VNEED);
	case SHT_SUNW_versym:
		return (ELF_T_HALF);
	};

	/*
	 * Check for the sparc specific section types
	 * below.
	 */
	if (((ehdr->e_machine == EM_SPARC) ||
	    (ehdr->e_machine == EM_SPARC32PLUS) ||
	    (ehdr->e_machine == EM_SPARCV9)) &&
	    (shtype == SHT_SPARC_GOTDATA))
		return (ELF_T_BYTE);

	/*
	 * Check for the amd64 specific section types
	 * below.
	 */
	if ((ehdr->e_machine == EM_AMD64) &&
	    (shtype == SHT_AMD64_UNWIND))
		return (ELF_T_BYTE);

	/*
	 * And the default is ELF_T_BYTE - but we should
	 * certainly have caught any sections we know about
	 * above.  This is for unknown sections to libelf.
	 */
	return (ELF_T_BYTE);
}


size_t
_elf32_entsz(Elf *elf, Elf32_Word shtype, unsigned ver)
{
	Elf_Type	ttype;

	ttype = _elf32_mtype(elf, shtype, ver);
	return ((ttype == ELF_T_BYTE) ? 0 : fmsize[ver - 1][ttype].s_filesz); 
}


/*
 * Determine the data encoding used by the current system.
 */
uint_t
_elf_sys_encoding(void)
{
	union {
		Elf32_Word	w;
		unsigned char	c[W_sizeof];
	} u;

	u.w = 0x10203;
	/*CONSTANTCONDITION*/
	if (~(Elf32_Word)0 == -(Elf32_Sword)1 && (((((((Elf32_Word)(u.c)[W_L3]<<8)
		+(u.c)[W_L2])<<8)
		+(u.c)[W_L1])<<8)
		+(u.c)[W_L0]) == 0x10203)
		return (ELFDATA2LSB);

	/*CONSTANTCONDITION*/
	if (~(Elf32_Word)0 == -(Elf32_Sword)1 && (((((((Elf32_Word)(u.c)[W_M3]<<8)
		+(u.c)[W_M2])<<8)
		+(u.c)[W_M1])<<8)
		+(u.c)[W_M0]) == 0x10203)
		return (ELFDATA2MSB);

	/* Not expected to occur */
	return (ELFDATANONE);
}


/*
 * XX64	This routine is also used to 'version' interactions with Elf64
 *	applications, but there's no way to figure out if the caller is
 *	asking Elf32 or Elf64 questions, even though it has Elf32
 *	dependencies.  Ick.
 */
unsigned
elf_version(unsigned ver)
{
	register unsigned	j;

	if (ver == EV_NONE)
		return EV_CURRENT;
	if (ver > EV_CURRENT)
	{
		_elf_seterr(EREQ_VER, 0);
		return EV_NONE;
	}
	(void) mutex_lock(&_elf_globals_mutex);
	if (_elf_work != EV_NONE)
	{
		j = _elf_work;
		_elf_work = ver;
		(void) mutex_unlock(&_elf_globals_mutex);
		return j;
	}
	_elf_work = ver;

	_elf_encode = _elf_sys_encoding();

	(void) mutex_unlock(&_elf_globals_mutex);

	return ver;
}


static Elf_Data *
xlate(Elf_Data *dst, const Elf_Data *src, unsigned encode, int tof)
						/* tof !0 -> xlatetof */
{
	size_t		cnt, dsz, ssz;
	unsigned	type;
	unsigned	dver, sver;
	void		(*f)();
	unsigned	_encode;

	if (dst == 0 || src == 0)
		return (0);
	if (--encode >= (ELFDATANUM - 1)) {
		_elf_seterr(EREQ_ENCODE, 0);
		return (0);
	}
	if ((dver = dst->d_version - 1) >= EV_CURRENT ||
	    (sver = src->d_version - 1) >= EV_CURRENT) {
		_elf_seterr(EREQ_VER, 0);
		return (0);
	}
	if ((type = src->d_type) >= ELF_T_NUM) {
		_elf_seterr(EREQ_TYPE, 0);
		return (0);
	}

	if (tof) {
		dsz = fmsize[dver][type].s_filesz;
		ssz = fmsize[sver][type].s_memsz;
		f = x32[dver][sver][encode][type].x_tof;
	} else {
		dsz = fmsize[dver][type].s_memsz;
		ssz = fmsize[sver][type].s_filesz;
		f = x32[dver][sver][encode][type].x_tom;
	}
	cnt = src->d_size / ssz;
	if (dst->d_size < dsz * cnt) {
		_elf_seterr(EREQ_DSZ, 0);
		return (0);
	}

	ELFACCESSDATA(_encode, _elf_encode)
	if ((_encode == (encode + 1)) && (dsz == ssz)) {
		/*
		 *	ld(1) frequently produces empty sections (eg. .dynsym,
		 *	.dynstr, .symtab, .strtab, etc) so that the initial
		 *	output image can be created of the correct size.  Later
		 *	these sections are filled in with the associated data.
		 *	So that we don't have to pre-allocate buffers for
		 *	these segments, allow for the src destination to be 0.
		 */
		if (src->d_buf && src->d_buf != dst->d_buf)
			(void) memcpy(dst->d_buf, src->d_buf, src->d_size);
		dst->d_type = src->d_type;
		dst->d_size = src->d_size;
		return (dst);
	}
	if (cnt)
		(*f)(dst->d_buf, src->d_buf, cnt);
	dst->d_size = dsz * cnt;
	dst->d_type = src->d_type;
	return (dst);
}


Elf_Data *
elf32_xlatetof(Elf_Data *dst, const Elf_Data *src, unsigned encode)
{
	return (xlate(dst, src, encode, 1));
}


Elf_Data *
elf32_xlatetom(Elf_Data *dst, const Elf_Data *src, unsigned encode)
{
	return (xlate(dst, src, encode, 0));
}


/*
 * xlate to file format
 *
 *	..._tof(name, data) -- macros
 *
 *	Recall that the file format must be no larger than the
 *	memory format (equal versions).  Use "forward" copy.
 *	All these routines require non-null, non-zero arguments.
 */




static void
addr_2L_tof(unsigned char *dst, Elf32_Addr *src, size_t cnt)
{
	Elf32_Addr	*end = src + cnt;

	do {
		{ register Elf32_Addr _t_ = *src;
		(dst)[A_L0] = (unsigned char)_t_,
		(dst)[A_L1] = (unsigned char)(_t_>>8),
		(dst)[A_L2] = (unsigned char)(_t_>>16),
		(dst)[A_L3] = (unsigned char)(_t_>>24); };
		dst += A_sizeof;
	} while (++src < end);
}

static void
addr_2M_tof(unsigned char *dst, Elf32_Addr *src, size_t cnt)
{
	Elf32_Addr	*end = src + cnt;

	do {
		{ register Elf32_Addr _t_ = *src;
		(dst)[A_M0] = (unsigned char)_t_,
		(dst)[A_M1] = (unsigned char)(_t_>>8),
		(dst)[A_M2] = (unsigned char)(_t_>>16),
		(dst)[A_M3] = (unsigned char)(_t_>>24); };
		dst += A_sizeof;
	} while (++src < end);
}


static void
byte_to(unsigned char *dst, unsigned char *src, size_t cnt)
{
	if (dst != src)
		(void) memcpy(dst, src, cnt);
}





static void
dyn_2L11_tof(unsigned char *dst, Elf32_Dyn *src, size_t cnt)
{
	Elf32_Dyn	*end = src + cnt;

	do {
		{ register Elf32_Word _t_ = src->d_tag;
		(dst)[D1_tag_L0] = (unsigned char)_t_,
		(dst)[D1_tag_L1] = (unsigned char)(_t_>>8),
		(dst)[D1_tag_L2] = (unsigned char)(_t_>>16),
		(dst)[D1_tag_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Off _t_ = src->d_un.d_val;
		(dst)[D1_val_L0] = (unsigned char)_t_,
		(dst)[D1_val_L1] = (unsigned char)(_t_>>8),
		(dst)[D1_val_L2] = (unsigned char)(_t_>>16),
		(dst)[D1_val_L3] = (unsigned char)(_t_>>24); };
		dst += D1_sizeof;
	} while (++src < end);
}

static void
dyn_2M11_tof(unsigned char *dst, Elf32_Dyn *src, size_t cnt)
{
	Elf32_Dyn	*end = src + cnt;

	do {
		{ register Elf32_Word _t_ = src->d_tag;
		(dst)[D1_tag_M0] = (unsigned char)_t_,
		(dst)[D1_tag_M1] = (unsigned char)(_t_>>8),
		(dst)[D1_tag_M2] = (unsigned char)(_t_>>16),
		(dst)[D1_tag_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Off _t_ = src->d_un.d_val;
		(dst)[D1_val_M0] = (unsigned char)_t_,
		(dst)[D1_val_M1] = (unsigned char)(_t_>>8),
		(dst)[D1_val_M2] = (unsigned char)(_t_>>16),
		(dst)[D1_val_M3] = (unsigned char)(_t_>>24); };
		dst += D1_sizeof;
	} while (++src < end);
}





static void
ehdr_2L11_tof(unsigned char *dst, Elf32_Ehdr *src, size_t cnt)
{
	Elf32_Ehdr	*end = src + cnt;

	do {
		if (&dst[E1_ident] != src->e_ident)
			(void) memcpy(&dst[E1_ident], src->e_ident, E1_Nident);
		{ register Elf32_Half _t_ = src->e_type;
		(dst)[E1_type_L0] = (unsigned char)_t_,
		(dst)[E1_type_L1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->e_machine;
		(dst)[E1_machine_L0] = (unsigned char)_t_,
		(dst)[E1_machine_L1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Word _t_ = src->e_version;
		(dst)[E1_version_L0] = (unsigned char)_t_,
		(dst)[E1_version_L1] = (unsigned char)(_t_>>8),
		(dst)[E1_version_L2] = (unsigned char)(_t_>>16),
		(dst)[E1_version_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Addr _t_ = src->e_entry;
		(dst)[E1_entry_L0] = (unsigned char)_t_,
		(dst)[E1_entry_L1] = (unsigned char)(_t_>>8),
		(dst)[E1_entry_L2] = (unsigned char)(_t_>>16),
		(dst)[E1_entry_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Off _t_ = src->e_phoff;
		(dst)[E1_phoff_L0] = (unsigned char)_t_,
		(dst)[E1_phoff_L1] = (unsigned char)(_t_>>8),
		(dst)[E1_phoff_L2] = (unsigned char)(_t_>>16),
		(dst)[E1_phoff_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Off _t_ = src->e_shoff;
		(dst)[E1_shoff_L0] = (unsigned char)_t_,
		(dst)[E1_shoff_L1] = (unsigned char)(_t_>>8),
		(dst)[E1_shoff_L2] = (unsigned char)(_t_>>16),
		(dst)[E1_shoff_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->e_flags;
		(dst)[E1_flags_L0] = (unsigned char)_t_,
		(dst)[E1_flags_L1] = (unsigned char)(_t_>>8),
		(dst)[E1_flags_L2] = (unsigned char)(_t_>>16),
		(dst)[E1_flags_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Half _t_ = src->e_ehsize;
		(dst)[E1_ehsize_L0] = (unsigned char)_t_,
		(dst)[E1_ehsize_L1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->e_phentsize;
		(dst)[E1_phentsize_L0] = (unsigned char)_t_,
		(dst)[E1_phentsize_L1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->e_phnum;
		(dst)[E1_phnum_L0] = (unsigned char)_t_,
		(dst)[E1_phnum_L1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->e_shentsize;
		(dst)[E1_shentsize_L0] = (unsigned char)_t_,
		(dst)[E1_shentsize_L1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->e_shnum;
		(dst)[E1_shnum_L0] = (unsigned char)_t_,
		(dst)[E1_shnum_L1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->e_shstrndx;
		(dst)[E1_shstrndx_L0] = (unsigned char)_t_,
		(dst)[E1_shstrndx_L1] = (unsigned char)(_t_>>8); };
		dst += E1_sizeof;
	} while (++src < end);
}

static void
ehdr_2M11_tof(unsigned char *dst, Elf32_Ehdr *src, size_t cnt)
{
	Elf32_Ehdr	*end = src + cnt;

	do {
		if (&dst[E1_ident] != src->e_ident)
			(void) memcpy(&dst[E1_ident], src->e_ident, E1_Nident);
		{ register Elf32_Half _t_ = src->e_type;
		(dst)[E1_type_M0] = (unsigned char)_t_,
		(dst)[E1_type_M1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->e_machine;
		(dst)[E1_machine_M0] = (unsigned char)_t_,
		(dst)[E1_machine_M1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Word _t_ = src->e_version;
		(dst)[E1_version_M0] = (unsigned char)_t_,
		(dst)[E1_version_M1] = (unsigned char)(_t_>>8),
		(dst)[E1_version_M2] = (unsigned char)(_t_>>16),
		(dst)[E1_version_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Addr _t_ = src->e_entry;
		(dst)[E1_entry_M0] = (unsigned char)_t_,
		(dst)[E1_entry_M1] = (unsigned char)(_t_>>8),
		(dst)[E1_entry_M2] = (unsigned char)(_t_>>16),
		(dst)[E1_entry_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Off _t_ = src->e_phoff;
		(dst)[E1_phoff_M0] = (unsigned char)_t_,
		(dst)[E1_phoff_M1] = (unsigned char)(_t_>>8),
		(dst)[E1_phoff_M2] = (unsigned char)(_t_>>16),
		(dst)[E1_phoff_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Off _t_ = src->e_shoff;
		(dst)[E1_shoff_M0] = (unsigned char)_t_,
		(dst)[E1_shoff_M1] = (unsigned char)(_t_>>8),
		(dst)[E1_shoff_M2] = (unsigned char)(_t_>>16),
		(dst)[E1_shoff_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->e_flags;
		(dst)[E1_flags_M0] = (unsigned char)_t_,
		(dst)[E1_flags_M1] = (unsigned char)(_t_>>8),
		(dst)[E1_flags_M2] = (unsigned char)(_t_>>16),
		(dst)[E1_flags_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Half _t_ = src->e_ehsize;
		(dst)[E1_ehsize_M0] = (unsigned char)_t_,
		(dst)[E1_ehsize_M1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->e_phentsize;
		(dst)[E1_phentsize_M0] = (unsigned char)_t_,
		(dst)[E1_phentsize_M1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->e_phnum;
		(dst)[E1_phnum_M0] = (unsigned char)_t_,
		(dst)[E1_phnum_M1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->e_shentsize;
		(dst)[E1_shentsize_M0] = (unsigned char)_t_,
		(dst)[E1_shentsize_M1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->e_shnum;
		(dst)[E1_shnum_M0] = (unsigned char)_t_,
		(dst)[E1_shnum_M1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->e_shstrndx;
		(dst)[E1_shstrndx_M0] = (unsigned char)_t_,
		(dst)[E1_shstrndx_M1] = (unsigned char)(_t_>>8); };
		dst += E1_sizeof;
	} while (++src < end);
}





static void
half_2L_tof(unsigned char *dst, Elf32_Half *src, size_t cnt)
{
	Elf32_Half	*end = src + cnt;

	do {
		{ register Elf32_Half _t_ = *src;
		(dst)[H_L0] = (unsigned char)_t_,
		(dst)[H_L1] = (unsigned char)(_t_>>8); };
		dst += H_sizeof;
	} while (++src < end);
}

static void
half_2M_tof(unsigned char *dst, Elf32_Half *src, size_t cnt)
{
	Elf32_Half	*end = src + cnt;

	do {
		{ register Elf32_Half _t_ = *src;
		(dst)[H_M0] = (unsigned char)_t_,
		(dst)[H_M1] = (unsigned char)(_t_>>8); };
		dst += H_sizeof;
	} while (++src < end);
}





static void
move_2L11_tof(unsigned char *dst, Elf32_Move *src, size_t cnt)
{
	Elf32_Move	*end = src + cnt;

	do {
		{ Elf32_Lword _t_ = src->m_value;
		(dst)[M1_value_L0] = (Byte)_t_,
		(dst)[M1_value_L1] = (Byte)(_t_>>8),
		(dst)[M1_value_L2] = (Byte)(_t_>>16),
		(dst)[M1_value_L3] = (Byte)(_t_>>24),
		(dst)[M1_value_L4] = (Byte)(_t_>>32),
		(dst)[M1_value_L5] = (Byte)(_t_>>40),
		(dst)[M1_value_L6] = (Byte)(_t_>>48),
		(dst)[M1_value_L7] = (Byte)(_t_>>56); };
		{ register Elf32_Word _t_ = src->m_info;
		(dst)[M1_info_L0] = (unsigned char)_t_,
		(dst)[M1_info_L1] = (unsigned char)(_t_>>8),
		(dst)[M1_info_L2] = (unsigned char)(_t_>>16),
		(dst)[M1_info_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->m_poffset;
		(dst)[M1_poffset_L0] = (unsigned char)_t_,
		(dst)[M1_poffset_L1] = (unsigned char)(_t_>>8),
		(dst)[M1_poffset_L2] = (unsigned char)(_t_>>16),
		(dst)[M1_poffset_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Half _t_ = src->m_repeat;
		(dst)[M1_repeat_L0] = (unsigned char)_t_,
		(dst)[M1_repeat_L1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->m_stride;
		(dst)[M1_stride_L0] = (unsigned char)_t_,
		(dst)[M1_stride_L1] = (unsigned char)(_t_>>8); };
		dst += M1_sizeof;
	} while (++src < end);
}

static void
move_2M11_tof(unsigned char *dst, Elf32_Move *src, size_t cnt)
{
	Elf32_Move	*end = src + cnt;

	do {
		{ Elf32_Lword _t_ = src->m_value;
		(dst)[M1_value_M0] = (Byte)_t_,
		(dst)[M1_value_M1] = (Byte)(_t_>>8),
		(dst)[M1_value_M2] = (Byte)(_t_>>16),
		(dst)[M1_value_M3] = (Byte)(_t_>>24),
		(dst)[M1_value_M4] = (Byte)(_t_>>32),
		(dst)[M1_value_M5] = (Byte)(_t_>>40),
		(dst)[M1_value_M6] = (Byte)(_t_>>48),
		(dst)[M1_value_M7] = (Byte)(_t_>>56); };
		{ register Elf32_Word _t_ = src->m_info;
		(dst)[M1_info_M0] = (unsigned char)_t_,
		(dst)[M1_info_M1] = (unsigned char)(_t_>>8),
		(dst)[M1_info_M2] = (unsigned char)(_t_>>16),
		(dst)[M1_info_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->m_poffset;
		(dst)[M1_poffset_M0] = (unsigned char)_t_,
		(dst)[M1_poffset_M1] = (unsigned char)(_t_>>8),
		(dst)[M1_poffset_M2] = (unsigned char)(_t_>>16),
		(dst)[M1_poffset_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Half _t_ = src->m_repeat;
		(dst)[M1_repeat_M0] = (unsigned char)_t_,
		(dst)[M1_repeat_M1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->m_stride;
		(dst)[M1_stride_M0] = (unsigned char)_t_,
		(dst)[M1_stride_M1] = (unsigned char)(_t_>>8); };
		dst += M1_sizeof;
	} while (++src < end);
}





static void
movep_2L11_tof(unsigned char *dst, Elf32_Move *src, size_t cnt)
{
	Elf32_Move	*end = src + cnt;

	do {
		{ Elf32_Lword _t_ = src->m_value;
		(dst)[MP1_value_L0] = (Byte)_t_,
		(dst)[MP1_value_L1] = (Byte)(_t_>>8),
		(dst)[MP1_value_L2] = (Byte)(_t_>>16),
		(dst)[MP1_value_L3] = (Byte)(_t_>>24),
		(dst)[MP1_value_L4] = (Byte)(_t_>>32),
		(dst)[MP1_value_L5] = (Byte)(_t_>>40),
		(dst)[MP1_value_L6] = (Byte)(_t_>>48),
		(dst)[MP1_value_L7] = (Byte)(_t_>>56); };
		{ register Elf32_Word _t_ = src->m_info;
		(dst)[MP1_info_L0] = (unsigned char)_t_,
		(dst)[MP1_info_L1] = (unsigned char)(_t_>>8),
		(dst)[MP1_info_L2] = (unsigned char)(_t_>>16),
		(dst)[MP1_info_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->m_poffset;
		(dst)[MP1_poffset_L0] = (unsigned char)_t_,
		(dst)[MP1_poffset_L1] = (unsigned char)(_t_>>8),
		(dst)[MP1_poffset_L2] = (unsigned char)(_t_>>16),
		(dst)[MP1_poffset_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Half _t_ = src->m_repeat;
		(dst)[MP1_repeat_L0] = (unsigned char)_t_,
		(dst)[MP1_repeat_L1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->m_stride;
		(dst)[MP1_stride_L0] = (unsigned char)_t_,
		(dst)[MP1_stride_L1] = (unsigned char)(_t_>>8); };
		dst += MP1_sizeof;
	} while (++src < end);
}

static void
movep_2M11_tof(unsigned char *dst, Elf32_Move *src, size_t cnt)
{
	Elf32_Move	*end = src + cnt;

	do {
		{ Elf32_Lword _t_ = src->m_value;
		(dst)[MP1_value_M0] = (Byte)_t_,
		(dst)[MP1_value_M1] = (Byte)(_t_>>8),
		(dst)[MP1_value_M2] = (Byte)(_t_>>16),
		(dst)[MP1_value_M3] = (Byte)(_t_>>24),
		(dst)[MP1_value_M4] = (Byte)(_t_>>32),
		(dst)[MP1_value_M5] = (Byte)(_t_>>40),
		(dst)[MP1_value_M6] = (Byte)(_t_>>48),
		(dst)[MP1_value_M7] = (Byte)(_t_>>56); };
		{ register Elf32_Word _t_ = src->m_info;
		(dst)[MP1_info_M0] = (unsigned char)_t_,
		(dst)[MP1_info_M1] = (unsigned char)(_t_>>8),
		(dst)[MP1_info_M2] = (unsigned char)(_t_>>16),
		(dst)[MP1_info_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->m_poffset;
		(dst)[MP1_poffset_M0] = (unsigned char)_t_,
		(dst)[MP1_poffset_M1] = (unsigned char)(_t_>>8),
		(dst)[MP1_poffset_M2] = (unsigned char)(_t_>>16),
		(dst)[MP1_poffset_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Half _t_ = src->m_repeat;
		(dst)[MP1_repeat_M0] = (unsigned char)_t_,
		(dst)[MP1_repeat_M1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->m_stride;
		(dst)[MP1_stride_M0] = (unsigned char)_t_,
		(dst)[MP1_stride_M1] = (unsigned char)(_t_>>8); };
		dst += MP1_sizeof;
	} while (++src < end);
}





static void
off_2L_tof(unsigned char *dst, Elf32_Off *src, size_t cnt)
{
	Elf32_Off	*end = src + cnt;

	do {
		{ register Elf32_Off _t_ = *src;
		(dst)[O_L0] = (unsigned char)_t_,
		(dst)[O_L1] = (unsigned char)(_t_>>8),
		(dst)[O_L2] = (unsigned char)(_t_>>16),
		(dst)[O_L3] = (unsigned char)(_t_>>24); };
		dst += O_sizeof;
	} while (++src < end);
}

static void
off_2M_tof(unsigned char *dst, Elf32_Off *src, size_t cnt)
{
	Elf32_Off	*end = src + cnt;

	do {
		{ register Elf32_Off _t_ = *src;
		(dst)[O_M0] = (unsigned char)_t_,
		(dst)[O_M1] = (unsigned char)(_t_>>8),
		(dst)[O_M2] = (unsigned char)(_t_>>16),
		(dst)[O_M3] = (unsigned char)(_t_>>24); };
		dst += O_sizeof;
	} while (++src < end);
}





static void
note_2L11_tof(unsigned char *dst, Elf32_Nhdr *src, size_t cnt)
{
	/* LINTED */
	Elf32_Nhdr *	end = (Elf32_Nhdr *)((char *)src + cnt);

	do {
		Elf32_Word	descsz, namesz;

		/*
		 * cache size of desc & name fields - while rounding
		 * up their size.
		 */
		namesz = S_ROUND(src->n_namesz, sizeof (Elf32_Word));
		descsz = src->n_descsz;

		/*
		 * Copy contents of Elf32_Nhdr
		 */
		{ register Elf32_Word _t_ = src->n_namesz;
		(dst)[N1_namesz_L0] = (unsigned char)_t_,
		(dst)[N1_namesz_L1] = (unsigned char)(_t_>>8),
		(dst)[N1_namesz_L2] = (unsigned char)(_t_>>16),
		(dst)[N1_namesz_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->n_descsz;
		(dst)[N1_descsz_L0] = (unsigned char)_t_,
		(dst)[N1_descsz_L1] = (unsigned char)(_t_>>8),
		(dst)[N1_descsz_L2] = (unsigned char)(_t_>>16),
		(dst)[N1_descsz_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->n_type;
		(dst)[N1_type_L0] = (unsigned char)_t_,
		(dst)[N1_type_L1] = (unsigned char)(_t_>>8),
		(dst)[N1_type_L2] = (unsigned char)(_t_>>16),
		(dst)[N1_type_L3] = (unsigned char)(_t_>>24); };

		/*
		 * Copy contents of Name field
		 */
		dst += N1_sizeof;
		src++;
		(void)memcpy(dst, src, namesz);

		/*
		 * Copy contents of desc field
		 */
		dst += namesz;
		src = (Elf32_Nhdr *)((uintptr_t)src + namesz);
		(void)memcpy(dst, src, descsz);
		descsz = S_ROUND(descsz, sizeof (Elf32_Word));
		dst += descsz;
		src = (Elf32_Nhdr *)((uintptr_t)src + descsz);
	} while (src < end);
}

static void
note_2M11_tof(unsigned char *dst, Elf32_Nhdr *src, size_t cnt)
{
	/* LINTED */
	Elf32_Nhdr *	end = (Elf32_Nhdr *)((char *)src + cnt);

	do {
		Elf32_Word	descsz, namesz;

		/*
		 * cache size of desc & name fields - while rounding
		 * up their size.
		 */
		namesz = S_ROUND(src->n_namesz, sizeof (Elf32_Word));
		descsz = src->n_descsz;

		/*
		 * Copy contents of Elf32_Nhdr
		 */
		{ register Elf32_Word _t_ = src->n_namesz;
		(dst)[N1_namesz_M0] = (unsigned char)_t_,
		(dst)[N1_namesz_M1] = (unsigned char)(_t_>>8),
		(dst)[N1_namesz_M2] = (unsigned char)(_t_>>16),
		(dst)[N1_namesz_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->n_descsz;
		(dst)[N1_descsz_M0] = (unsigned char)_t_,
		(dst)[N1_descsz_M1] = (unsigned char)(_t_>>8),
		(dst)[N1_descsz_M2] = (unsigned char)(_t_>>16),
		(dst)[N1_descsz_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->n_type;
		(dst)[N1_type_M0] = (unsigned char)_t_,
		(dst)[N1_type_M1] = (unsigned char)(_t_>>8),
		(dst)[N1_type_M2] = (unsigned char)(_t_>>16),
		(dst)[N1_type_M3] = (unsigned char)(_t_>>24); };

		/*
		 * Copy contents of Name field
		 */
		dst += N1_sizeof;
		src++;
		(void)memcpy(dst, src, namesz);

		/*
		 * Copy contents of desc field
		 */
		dst += namesz;
		src = (Elf32_Nhdr *)((uintptr_t)src + namesz);
		(void)memcpy(dst, src, descsz);
		descsz = S_ROUND(descsz, sizeof (Elf32_Word));
		dst += descsz;
		src = (Elf32_Nhdr *)((uintptr_t)src + descsz);
	} while (src < end);
}





static void
phdr_2L11_tof(unsigned char *dst, Elf32_Phdr *src, size_t cnt)
{
	Elf32_Phdr	*end = src + cnt;

	do {
		{ register Elf32_Word _t_ = src->p_type;
		(dst)[P1_type_L0] = (unsigned char)_t_,
		(dst)[P1_type_L1] = (unsigned char)(_t_>>8),
		(dst)[P1_type_L2] = (unsigned char)(_t_>>16),
		(dst)[P1_type_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Off _t_ = src->p_offset;
		(dst)[P1_offset_L0] = (unsigned char)_t_,
		(dst)[P1_offset_L1] = (unsigned char)(_t_>>8),
		(dst)[P1_offset_L2] = (unsigned char)(_t_>>16),
		(dst)[P1_offset_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Addr _t_ = src->p_vaddr;
		(dst)[P1_vaddr_L0] = (unsigned char)_t_,
		(dst)[P1_vaddr_L1] = (unsigned char)(_t_>>8),
		(dst)[P1_vaddr_L2] = (unsigned char)(_t_>>16),
		(dst)[P1_vaddr_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Addr _t_ = src->p_paddr;
		(dst)[P1_paddr_L0] = (unsigned char)_t_,
		(dst)[P1_paddr_L1] = (unsigned char)(_t_>>8),
		(dst)[P1_paddr_L2] = (unsigned char)(_t_>>16),
		(dst)[P1_paddr_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->p_filesz;
		(dst)[P1_filesz_L0] = (unsigned char)_t_,
		(dst)[P1_filesz_L1] = (unsigned char)(_t_>>8),
		(dst)[P1_filesz_L2] = (unsigned char)(_t_>>16),
		(dst)[P1_filesz_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->p_memsz;
		(dst)[P1_memsz_L0] = (unsigned char)_t_,
		(dst)[P1_memsz_L1] = (unsigned char)(_t_>>8),
		(dst)[P1_memsz_L2] = (unsigned char)(_t_>>16),
		(dst)[P1_memsz_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->p_flags;
		(dst)[P1_flags_L0] = (unsigned char)_t_,
		(dst)[P1_flags_L1] = (unsigned char)(_t_>>8),
		(dst)[P1_flags_L2] = (unsigned char)(_t_>>16),
		(dst)[P1_flags_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->p_align;
		(dst)[P1_align_L0] = (unsigned char)_t_,
		(dst)[P1_align_L1] = (unsigned char)(_t_>>8),
		(dst)[P1_align_L2] = (unsigned char)(_t_>>16),
		(dst)[P1_align_L3] = (unsigned char)(_t_>>24); };
		dst += P1_sizeof;
	} while (++src < end);
}

static void
phdr_2M11_tof(unsigned char *dst, Elf32_Phdr *src, size_t cnt)
{
	Elf32_Phdr	*end = src + cnt;

	do {
		{ register Elf32_Word _t_ = src->p_type;
		(dst)[P1_type_M0] = (unsigned char)_t_,
		(dst)[P1_type_M1] = (unsigned char)(_t_>>8),
		(dst)[P1_type_M2] = (unsigned char)(_t_>>16),
		(dst)[P1_type_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Off _t_ = src->p_offset;
		(dst)[P1_offset_M0] = (unsigned char)_t_,
		(dst)[P1_offset_M1] = (unsigned char)(_t_>>8),
		(dst)[P1_offset_M2] = (unsigned char)(_t_>>16),
		(dst)[P1_offset_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Addr _t_ = src->p_vaddr;
		(dst)[P1_vaddr_M0] = (unsigned char)_t_,
		(dst)[P1_vaddr_M1] = (unsigned char)(_t_>>8),
		(dst)[P1_vaddr_M2] = (unsigned char)(_t_>>16),
		(dst)[P1_vaddr_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Addr _t_ = src->p_paddr;
		(dst)[P1_paddr_M0] = (unsigned char)_t_,
		(dst)[P1_paddr_M1] = (unsigned char)(_t_>>8),
		(dst)[P1_paddr_M2] = (unsigned char)(_t_>>16),
		(dst)[P1_paddr_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->p_filesz;
		(dst)[P1_filesz_M0] = (unsigned char)_t_,
		(dst)[P1_filesz_M1] = (unsigned char)(_t_>>8),
		(dst)[P1_filesz_M2] = (unsigned char)(_t_>>16),
		(dst)[P1_filesz_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->p_memsz;
		(dst)[P1_memsz_M0] = (unsigned char)_t_,
		(dst)[P1_memsz_M1] = (unsigned char)(_t_>>8),
		(dst)[P1_memsz_M2] = (unsigned char)(_t_>>16),
		(dst)[P1_memsz_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->p_flags;
		(dst)[P1_flags_M0] = (unsigned char)_t_,
		(dst)[P1_flags_M1] = (unsigned char)(_t_>>8),
		(dst)[P1_flags_M2] = (unsigned char)(_t_>>16),
		(dst)[P1_flags_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->p_align;
		(dst)[P1_align_M0] = (unsigned char)_t_,
		(dst)[P1_align_M1] = (unsigned char)(_t_>>8),
		(dst)[P1_align_M2] = (unsigned char)(_t_>>16),
		(dst)[P1_align_M3] = (unsigned char)(_t_>>24); };
		dst += P1_sizeof;
	} while (++src < end);
}





static void
rel_2L11_tof(unsigned char *dst, Elf32_Rel *src, size_t cnt)
{
	Elf32_Rel	*end = src + cnt;

	do {
		{ register Elf32_Addr _t_ = src->r_offset;
		(dst)[R1_offset_L0] = (unsigned char)_t_,
		(dst)[R1_offset_L1] = (unsigned char)(_t_>>8),
		(dst)[R1_offset_L2] = (unsigned char)(_t_>>16),
		(dst)[R1_offset_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->r_info;
		(dst)[R1_info_L0] = (unsigned char)_t_,
		(dst)[R1_info_L1] = (unsigned char)(_t_>>8),
		(dst)[R1_info_L2] = (unsigned char)(_t_>>16),
		(dst)[R1_info_L3] = (unsigned char)(_t_>>24); };
		dst += R1_sizeof;
	} while (++src < end);
}

static void
rel_2M11_tof(unsigned char *dst, Elf32_Rel *src, size_t cnt)
{
	Elf32_Rel	*end = src + cnt;

	do {
		{ register Elf32_Addr _t_ = src->r_offset;
		(dst)[R1_offset_M0] = (unsigned char)_t_,
		(dst)[R1_offset_M1] = (unsigned char)(_t_>>8),
		(dst)[R1_offset_M2] = (unsigned char)(_t_>>16),
		(dst)[R1_offset_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->r_info;
		(dst)[R1_info_M0] = (unsigned char)_t_,
		(dst)[R1_info_M1] = (unsigned char)(_t_>>8),
		(dst)[R1_info_M2] = (unsigned char)(_t_>>16),
		(dst)[R1_info_M3] = (unsigned char)(_t_>>24); };
		dst += R1_sizeof;
	} while (++src < end);
}





static void
rela_2L11_tof(unsigned char *dst, Elf32_Rela *src, size_t cnt)
{
	Elf32_Rela	*end = src + cnt;

	do {
		{ register Elf32_Addr _t_ = src->r_offset;
		(dst)[RA1_offset_L0] = (unsigned char)_t_,
		(dst)[RA1_offset_L1] = (unsigned char)(_t_>>8),
		(dst)[RA1_offset_L2] = (unsigned char)(_t_>>16),
		(dst)[RA1_offset_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->r_info;
		(dst)[RA1_info_L0] = (unsigned char)_t_,
		(dst)[RA1_info_L1] = (unsigned char)(_t_>>8),
		(dst)[RA1_info_L2] = (unsigned char)(_t_>>16),
		(dst)[RA1_info_L3] = (unsigned char)(_t_>>24); };
		/*CONSTANTCONDITION*/
		if (~(Elf32_Word)0 == -(Elf32_Sword)1) {	/* 2s comp */
			{ register Elf32_Word _t_ = src->r_addend;
		(dst)[RA1_addend_L0] = (unsigned char)_t_,
		(dst)[RA1_addend_L1] = (unsigned char)(_t_>>8),
		(dst)[RA1_addend_L2] = (unsigned char)(_t_>>16),
		(dst)[RA1_addend_L3] = (unsigned char)(_t_>>24); };
		} else {
			Elf32_Word	w;

			if (src->r_addend < 0) {
				w = - src->r_addend;
				w = ~w + 1;
			} else
				w = src->r_addend;
			{ register Elf32_Word _t_ = w;
		(dst)[RA1_addend_L0] = (unsigned char)_t_,
		(dst)[RA1_addend_L1] = (unsigned char)(_t_>>8),
		(dst)[RA1_addend_L2] = (unsigned char)(_t_>>16),
		(dst)[RA1_addend_L3] = (unsigned char)(_t_>>24); };
		}
		dst += RA1_sizeof;
	} while (++src < end);
}

static void
rela_2M11_tof(unsigned char *dst, Elf32_Rela *src, size_t cnt)
{
	Elf32_Rela	*end = src + cnt;

	do {
		{ register Elf32_Addr _t_ = src->r_offset;
		(dst)[RA1_offset_M0] = (unsigned char)_t_,
		(dst)[RA1_offset_M1] = (unsigned char)(_t_>>8),
		(dst)[RA1_offset_M2] = (unsigned char)(_t_>>16),
		(dst)[RA1_offset_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->r_info;
		(dst)[RA1_info_M0] = (unsigned char)_t_,
		(dst)[RA1_info_M1] = (unsigned char)(_t_>>8),
		(dst)[RA1_info_M2] = (unsigned char)(_t_>>16),
		(dst)[RA1_info_M3] = (unsigned char)(_t_>>24); };
		/*CONSTANTCONDITION*/
		if (~(Elf32_Word)0 == -(Elf32_Sword)1) {	/* 2s comp */
			{ register Elf32_Word _t_ = src->r_addend;
		(dst)[RA1_addend_M0] = (unsigned char)_t_,
		(dst)[RA1_addend_M1] = (unsigned char)(_t_>>8),
		(dst)[RA1_addend_M2] = (unsigned char)(_t_>>16),
		(dst)[RA1_addend_M3] = (unsigned char)(_t_>>24); };
		} else {
			Elf32_Word	w;

			if (src->r_addend < 0) {
				w = - src->r_addend;
				w = ~w + 1;
			} else
				w = src->r_addend;
			{ register Elf32_Word _t_ = w;
		(dst)[RA1_addend_M0] = (unsigned char)_t_,
		(dst)[RA1_addend_M1] = (unsigned char)(_t_>>8),
		(dst)[RA1_addend_M2] = (unsigned char)(_t_>>16),
		(dst)[RA1_addend_M3] = (unsigned char)(_t_>>24); };
		}
		dst += RA1_sizeof;
	} while (++src < end);
}





static void
shdr_2L11_tof(unsigned char *dst, Elf32_Shdr *src, size_t cnt)
{
	Elf32_Shdr	*end = src + cnt;

	do {
		{ register Elf32_Word _t_ = src->sh_name;
		(dst)[SH1_name_L0] = (unsigned char)_t_,
		(dst)[SH1_name_L1] = (unsigned char)(_t_>>8),
		(dst)[SH1_name_L2] = (unsigned char)(_t_>>16),
		(dst)[SH1_name_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->sh_type;
		(dst)[SH1_type_L0] = (unsigned char)_t_,
		(dst)[SH1_type_L1] = (unsigned char)(_t_>>8),
		(dst)[SH1_type_L2] = (unsigned char)(_t_>>16),
		(dst)[SH1_type_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->sh_flags;
		(dst)[SH1_flags_L0] = (unsigned char)_t_,
		(dst)[SH1_flags_L1] = (unsigned char)(_t_>>8),
		(dst)[SH1_flags_L2] = (unsigned char)(_t_>>16),
		(dst)[SH1_flags_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Addr _t_ = src->sh_addr;
		(dst)[SH1_addr_L0] = (unsigned char)_t_,
		(dst)[SH1_addr_L1] = (unsigned char)(_t_>>8),
		(dst)[SH1_addr_L2] = (unsigned char)(_t_>>16),
		(dst)[SH1_addr_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Off _t_ = src->sh_offset;
		(dst)[SH1_offset_L0] = (unsigned char)_t_,
		(dst)[SH1_offset_L1] = (unsigned char)(_t_>>8),
		(dst)[SH1_offset_L2] = (unsigned char)(_t_>>16),
		(dst)[SH1_offset_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->sh_size;
		(dst)[SH1_size_L0] = (unsigned char)_t_,
		(dst)[SH1_size_L1] = (unsigned char)(_t_>>8),
		(dst)[SH1_size_L2] = (unsigned char)(_t_>>16),
		(dst)[SH1_size_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->sh_link;
		(dst)[SH1_link_L0] = (unsigned char)_t_,
		(dst)[SH1_link_L1] = (unsigned char)(_t_>>8),
		(dst)[SH1_link_L2] = (unsigned char)(_t_>>16),
		(dst)[SH1_link_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->sh_info;
		(dst)[SH1_info_L0] = (unsigned char)_t_,
		(dst)[SH1_info_L1] = (unsigned char)(_t_>>8),
		(dst)[SH1_info_L2] = (unsigned char)(_t_>>16),
		(dst)[SH1_info_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->sh_addralign;
		(dst)[SH1_addralign_L0] = (unsigned char)_t_,
		(dst)[SH1_addralign_L1] = (unsigned char)(_t_>>8),
		(dst)[SH1_addralign_L2] = (unsigned char)(_t_>>16),
		(dst)[SH1_addralign_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->sh_entsize;
		(dst)[SH1_entsize_L0] = (unsigned char)_t_,
		(dst)[SH1_entsize_L1] = (unsigned char)(_t_>>8),
		(dst)[SH1_entsize_L2] = (unsigned char)(_t_>>16),
		(dst)[SH1_entsize_L3] = (unsigned char)(_t_>>24); };
		dst += SH1_sizeof;
	} while (++src < end);
}

static void
shdr_2M11_tof(unsigned char *dst, Elf32_Shdr *src, size_t cnt)
{
	Elf32_Shdr	*end = src + cnt;

	do {
		{ register Elf32_Word _t_ = src->sh_name;
		(dst)[SH1_name_M0] = (unsigned char)_t_,
		(dst)[SH1_name_M1] = (unsigned char)(_t_>>8),
		(dst)[SH1_name_M2] = (unsigned char)(_t_>>16),
		(dst)[SH1_name_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->sh_type;
		(dst)[SH1_type_M0] = (unsigned char)_t_,
		(dst)[SH1_type_M1] = (unsigned char)(_t_>>8),
		(dst)[SH1_type_M2] = (unsigned char)(_t_>>16),
		(dst)[SH1_type_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->sh_flags;
		(dst)[SH1_flags_M0] = (unsigned char)_t_,
		(dst)[SH1_flags_M1] = (unsigned char)(_t_>>8),
		(dst)[SH1_flags_M2] = (unsigned char)(_t_>>16),
		(dst)[SH1_flags_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Addr _t_ = src->sh_addr;
		(dst)[SH1_addr_M0] = (unsigned char)_t_,
		(dst)[SH1_addr_M1] = (unsigned char)(_t_>>8),
		(dst)[SH1_addr_M2] = (unsigned char)(_t_>>16),
		(dst)[SH1_addr_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Off _t_ = src->sh_offset;
		(dst)[SH1_offset_M0] = (unsigned char)_t_,
		(dst)[SH1_offset_M1] = (unsigned char)(_t_>>8),
		(dst)[SH1_offset_M2] = (unsigned char)(_t_>>16),
		(dst)[SH1_offset_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->sh_size;
		(dst)[SH1_size_M0] = (unsigned char)_t_,
		(dst)[SH1_size_M1] = (unsigned char)(_t_>>8),
		(dst)[SH1_size_M2] = (unsigned char)(_t_>>16),
		(dst)[SH1_size_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->sh_link;
		(dst)[SH1_link_M0] = (unsigned char)_t_,
		(dst)[SH1_link_M1] = (unsigned char)(_t_>>8),
		(dst)[SH1_link_M2] = (unsigned char)(_t_>>16),
		(dst)[SH1_link_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->sh_info;
		(dst)[SH1_info_M0] = (unsigned char)_t_,
		(dst)[SH1_info_M1] = (unsigned char)(_t_>>8),
		(dst)[SH1_info_M2] = (unsigned char)(_t_>>16),
		(dst)[SH1_info_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->sh_addralign;
		(dst)[SH1_addralign_M0] = (unsigned char)_t_,
		(dst)[SH1_addralign_M1] = (unsigned char)(_t_>>8),
		(dst)[SH1_addralign_M2] = (unsigned char)(_t_>>16),
		(dst)[SH1_addralign_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->sh_entsize;
		(dst)[SH1_entsize_M0] = (unsigned char)_t_,
		(dst)[SH1_entsize_M1] = (unsigned char)(_t_>>8),
		(dst)[SH1_entsize_M2] = (unsigned char)(_t_>>16),
		(dst)[SH1_entsize_M3] = (unsigned char)(_t_>>24); };
		dst += SH1_sizeof;
	} while (++src < end);
}





static void
sword_2L_tof(unsigned char *dst, Elf32_Sword *src, size_t cnt)
{
	Elf32_Sword	*end = src + cnt;

	do {
		/*CONSTANTCONDITION*/
		if (~(Elf32_Word)0 == -(Elf32_Sword)1) {	/* 2s comp */
			{ register Elf32_Word _t_ = *src;
		(dst)[W_L0] = (unsigned char)_t_,
		(dst)[W_L1] = (unsigned char)(_t_>>8),
		(dst)[W_L2] = (unsigned char)(_t_>>16),
		(dst)[W_L3] = (unsigned char)(_t_>>24); };
		} else {
			Elf32_Word	w;

			if (*src < 0) {
				w = - *src;
				w = ~w + 1;
			} else
				w = *src;
			{ register Elf32_Word _t_ = w;
		(dst)[W_L0] = (unsigned char)_t_,
		(dst)[W_L1] = (unsigned char)(_t_>>8),
		(dst)[W_L2] = (unsigned char)(_t_>>16),
		(dst)[W_L3] = (unsigned char)(_t_>>24); };
		}
		dst += W_sizeof;
	} while (++src < end);
}

static void
sword_2M_tof(unsigned char *dst, Elf32_Sword *src, size_t cnt)
{
	Elf32_Sword	*end = src + cnt;

	do {
		/*CONSTANTCONDITION*/
		if (~(Elf32_Word)0 == -(Elf32_Sword)1) {	/* 2s comp */
			{ register Elf32_Word _t_ = *src;
		(dst)[W_M0] = (unsigned char)_t_,
		(dst)[W_M1] = (unsigned char)(_t_>>8),
		(dst)[W_M2] = (unsigned char)(_t_>>16),
		(dst)[W_M3] = (unsigned char)(_t_>>24); };
		} else {
			Elf32_Word	w;

			if (*src < 0) {
				w = - *src;
				w = ~w + 1;
			} else
				w = *src;
			{ register Elf32_Word _t_ = w;
		(dst)[W_M0] = (unsigned char)_t_,
		(dst)[W_M1] = (unsigned char)(_t_>>8),
		(dst)[W_M2] = (unsigned char)(_t_>>16),
		(dst)[W_M3] = (unsigned char)(_t_>>24); };
		}
		dst += W_sizeof;
	} while (++src < end);
}





static void
cap_2L11_tof(unsigned char *dst, Elf32_Cap *src, size_t cnt)
{
	Elf32_Cap	*end = src + cnt;

	do {
		{ register Elf32_Word _t_ = src->c_tag;
		(dst)[C1_tag_L0] = (unsigned char)_t_,
		(dst)[C1_tag_L1] = (unsigned char)(_t_>>8),
		(dst)[C1_tag_L2] = (unsigned char)(_t_>>16),
		(dst)[C1_tag_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->c_un.c_val;
		(dst)[C1_val_L0] = (unsigned char)_t_,
		(dst)[C1_val_L1] = (unsigned char)(_t_>>8),
		(dst)[C1_val_L2] = (unsigned char)(_t_>>16),
		(dst)[C1_val_L3] = (unsigned char)(_t_>>24); };
		dst += C1_sizeof;
	} while (++src < end);
}

static void
cap_2M11_tof(unsigned char *dst, Elf32_Cap *src, size_t cnt)
{
	Elf32_Cap	*end = src + cnt;

	do {
		{ register Elf32_Word _t_ = src->c_tag;
		(dst)[C1_tag_M0] = (unsigned char)_t_,
		(dst)[C1_tag_M1] = (unsigned char)(_t_>>8),
		(dst)[C1_tag_M2] = (unsigned char)(_t_>>16),
		(dst)[C1_tag_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->c_un.c_val;
		(dst)[C1_val_M0] = (unsigned char)_t_,
		(dst)[C1_val_M1] = (unsigned char)(_t_>>8),
		(dst)[C1_val_M2] = (unsigned char)(_t_>>16),
		(dst)[C1_val_M3] = (unsigned char)(_t_>>24); };
		dst += C1_sizeof;
	} while (++src < end);
}





static void
syminfo_2L11_tof(unsigned char *dst, Elf32_Syminfo *src, size_t cnt)
{
	Elf32_Syminfo	*end = src + cnt;

	do {
		{ register Elf32_Half _t_ = src->si_boundto;
		(dst)[SI1_boundto_L0] = (unsigned char)_t_,
		(dst)[SI1_boundto_L1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->si_flags;
		(dst)[SI1_flags_L0] = (unsigned char)_t_,
		(dst)[SI1_flags_L1] = (unsigned char)(_t_>>8); };
		dst += SI1_sizeof;
	} while (++src < end);
}

static void
syminfo_2M11_tof(unsigned char *dst, Elf32_Syminfo *src, size_t cnt)
{
	Elf32_Syminfo	*end = src + cnt;

	do {
		{ register Elf32_Half _t_ = src->si_boundto;
		(dst)[SI1_boundto_M0] = (unsigned char)_t_,
		(dst)[SI1_boundto_M1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->si_flags;
		(dst)[SI1_flags_M0] = (unsigned char)_t_,
		(dst)[SI1_flags_M1] = (unsigned char)(_t_>>8); };
		dst += SI1_sizeof;
	} while (++src < end);
}





static void
sym_2L11_tof(unsigned char *dst, Elf32_Sym *src, size_t cnt)
{
	Elf32_Sym	*end = src + cnt;

	do {
		{ register Elf32_Word _t_ = src->st_name;
		(dst)[ST1_name_L0] = (unsigned char)_t_,
		(dst)[ST1_name_L1] = (unsigned char)(_t_>>8),
		(dst)[ST1_name_L2] = (unsigned char)(_t_>>16),
		(dst)[ST1_name_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Addr _t_ = src->st_value;
		(dst)[ST1_value_L0] = (unsigned char)_t_,
		(dst)[ST1_value_L1] = (unsigned char)(_t_>>8),
		(dst)[ST1_value_L2] = (unsigned char)(_t_>>16),
		(dst)[ST1_value_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->st_size;
		(dst)[ST1_size_L0] = (unsigned char)_t_,
		(dst)[ST1_size_L1] = (unsigned char)(_t_>>8),
		(dst)[ST1_size_L2] = (unsigned char)(_t_>>16),
		(dst)[ST1_size_L3] = (unsigned char)(_t_>>24); };
		(dst)[ST1_info_L] = (unsigned char)(src->st_info);
		(dst)[ST1_other_L] = (unsigned char)(src->st_other);
		{ register Elf32_Half _t_ = src->st_shndx;
		(dst)[ST1_shndx_L0] = (unsigned char)_t_,
		(dst)[ST1_shndx_L1] = (unsigned char)(_t_>>8); };
		dst += ST1_sizeof;
	} while (++src < end);
}

static void
sym_2M11_tof(unsigned char *dst, Elf32_Sym *src, size_t cnt)
{
	Elf32_Sym	*end = src + cnt;

	do {
		{ register Elf32_Word _t_ = src->st_name;
		(dst)[ST1_name_M0] = (unsigned char)_t_,
		(dst)[ST1_name_M1] = (unsigned char)(_t_>>8),
		(dst)[ST1_name_M2] = (unsigned char)(_t_>>16),
		(dst)[ST1_name_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Addr _t_ = src->st_value;
		(dst)[ST1_value_M0] = (unsigned char)_t_,
		(dst)[ST1_value_M1] = (unsigned char)(_t_>>8),
		(dst)[ST1_value_M2] = (unsigned char)(_t_>>16),
		(dst)[ST1_value_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->st_size;
		(dst)[ST1_size_M0] = (unsigned char)_t_,
		(dst)[ST1_size_M1] = (unsigned char)(_t_>>8),
		(dst)[ST1_size_M2] = (unsigned char)(_t_>>16),
		(dst)[ST1_size_M3] = (unsigned char)(_t_>>24); };
		(dst)[ST1_info_M] = (unsigned char)(src->st_info);
		(dst)[ST1_other_M] = (unsigned char)(src->st_other);
		{ register Elf32_Half _t_ = src->st_shndx;
		(dst)[ST1_shndx_M0] = (unsigned char)_t_,
		(dst)[ST1_shndx_M1] = (unsigned char)(_t_>>8); };
		dst += ST1_sizeof;
	} while (++src < end);
}





static void
word_2L_tof(unsigned char *dst, Elf32_Word *src, size_t cnt)
{
	Elf32_Word	*end = src + cnt;

	do {
		{ register Elf32_Word _t_ = *src;
		(dst)[W_L0] = (unsigned char)_t_,
		(dst)[W_L1] = (unsigned char)(_t_>>8),
		(dst)[W_L2] = (unsigned char)(_t_>>16),
		(dst)[W_L3] = (unsigned char)(_t_>>24); };
		dst += W_sizeof;
	} while (++src < end);
}

static void
word_2M_tof(unsigned char *dst, Elf32_Word *src, size_t cnt)
{
	Elf32_Word	*end = src + cnt;

	do {
		{ register Elf32_Word _t_ = *src;
		(dst)[W_M0] = (unsigned char)_t_,
		(dst)[W_M1] = (unsigned char)(_t_>>8),
		(dst)[W_M2] = (unsigned char)(_t_>>16),
		(dst)[W_M3] = (unsigned char)(_t_>>24); };
		dst += W_sizeof;
	} while (++src < end);
}





static void
verdef_2L11_tof(unsigned char *dst, Elf32_Verdef *src, size_t cnt)
{
	/* LINTED */
	Elf32_Verdef	*end = (Elf32_Verdef *)((char *)src + cnt);

	do {
		Elf32_Verdef	*next_verdef;
		Elf32_Verdaux	*vaux;
		Elf32_Half	i;
		unsigned char	*vaux_dst;
		unsigned char	*dst_next;

		/* LINTED */
		next_verdef = (Elf32_Verdef *)(src->vd_next ?
		    (char *)src + src->vd_next : (char *)end);
		dst_next = dst + src->vd_next;

		/* LINTED */
		vaux = (Elf32_Verdaux *)((char *)src + src->vd_aux);
		vaux_dst = dst + src->vd_aux;

		/*
		 * Convert auxilary structures
		 */
		for (i = 0; i < src->vd_cnt; i++) {
			Elf32_Verdaux	*vaux_next;
			unsigned char	*vaux_dst_next;

			/*
			 * because our source and destination can be
			 * the same place we need to figure out the next
			 * location now.
			 */
			/* LINTED */
			vaux_next = (Elf32_Verdaux *)((char *)vaux +
			    vaux->vda_next);
			vaux_dst_next = vaux_dst + vaux->vda_next;

			{ register Elf32_Addr _t_ = vaux->vda_name;
		(vaux_dst)[VDA1_name_L0] = (unsigned char)_t_,
		(vaux_dst)[VDA1_name_L1] = (unsigned char)(_t_>>8),
		(vaux_dst)[VDA1_name_L2] = (unsigned char)(_t_>>16),
		(vaux_dst)[VDA1_name_L3] = (unsigned char)(_t_>>24); };
			{ register Elf32_Word _t_ = vaux->vda_next;
		(vaux_dst)[VDA1_next_L0] = (unsigned char)_t_,
		(vaux_dst)[VDA1_next_L1] = (unsigned char)(_t_>>8),
		(vaux_dst)[VDA1_next_L2] = (unsigned char)(_t_>>16),
		(vaux_dst)[VDA1_next_L3] = (unsigned char)(_t_>>24); };
			vaux_dst = vaux_dst_next;
			vaux = vaux_next;
		}

		/*
		 * Convert Elf32_Verdef structure.
		 */
		{ register Elf32_Half _t_ = src->vd_version;
		(dst)[VD1_version_L0] = (unsigned char)_t_,
		(dst)[VD1_version_L1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->vd_flags;
		(dst)[VD1_flags_L0] = (unsigned char)_t_,
		(dst)[VD1_flags_L1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->vd_ndx;
		(dst)[VD1_ndx_L0] = (unsigned char)_t_,
		(dst)[VD1_ndx_L1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->vd_cnt;
		(dst)[VD1_cnt_L0] = (unsigned char)_t_,
		(dst)[VD1_cnt_L1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Word _t_ = src->vd_hash;
		(dst)[VD1_hash_L0] = (unsigned char)_t_,
		(dst)[VD1_hash_L1] = (unsigned char)(_t_>>8),
		(dst)[VD1_hash_L2] = (unsigned char)(_t_>>16),
		(dst)[VD1_hash_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->vd_aux;
		(dst)[VD1_aux_L0] = (unsigned char)_t_,
		(dst)[VD1_aux_L1] = (unsigned char)(_t_>>8),
		(dst)[VD1_aux_L2] = (unsigned char)(_t_>>16),
		(dst)[VD1_aux_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->vd_next;
		(dst)[VD1_next_L0] = (unsigned char)_t_,
		(dst)[VD1_next_L1] = (unsigned char)(_t_>>8),
		(dst)[VD1_next_L2] = (unsigned char)(_t_>>16),
		(dst)[VD1_next_L3] = (unsigned char)(_t_>>24); };
		src = next_verdef;
		dst = dst_next;
	} while (src < end);
}

static void
verdef_2M11_tof(unsigned char *dst, Elf32_Verdef *src, size_t cnt)
{
	/* LINTED */
	Elf32_Verdef	*end = (Elf32_Verdef *)((char *)src + cnt);

	do {
		Elf32_Verdef	*next_verdef;
		Elf32_Verdaux	*vaux;
		Elf32_Half	i;
		unsigned char	*vaux_dst;
		unsigned char	*dst_next;

		/* LINTED */
		next_verdef = (Elf32_Verdef *)(src->vd_next ?
		    (char *)src + src->vd_next : (char *)end);
		dst_next = dst + src->vd_next;

		/* LINTED */
		vaux = (Elf32_Verdaux *)((char *)src + src->vd_aux);
		vaux_dst = dst + src->vd_aux;

		/*
		 * Convert auxilary structures
		 */
		for (i = 0; i < src->vd_cnt; i++) {
			Elf32_Verdaux	*vaux_next;
			unsigned char	*vaux_dst_next;

			/*
			 * because our source and destination can be
			 * the same place we need to figure out the next
			 * location now.
			 */
			/* LINTED */
			vaux_next = (Elf32_Verdaux *)((char *)vaux +
			    vaux->vda_next);
			vaux_dst_next = vaux_dst + vaux->vda_next;

			{ register Elf32_Addr _t_ = vaux->vda_name;
		(vaux_dst)[VDA1_name_M0] = (unsigned char)_t_,
		(vaux_dst)[VDA1_name_M1] = (unsigned char)(_t_>>8),
		(vaux_dst)[VDA1_name_M2] = (unsigned char)(_t_>>16),
		(vaux_dst)[VDA1_name_M3] = (unsigned char)(_t_>>24); };
			{ register Elf32_Word _t_ = vaux->vda_next;
		(vaux_dst)[VDA1_next_M0] = (unsigned char)_t_,
		(vaux_dst)[VDA1_next_M1] = (unsigned char)(_t_>>8),
		(vaux_dst)[VDA1_next_M2] = (unsigned char)(_t_>>16),
		(vaux_dst)[VDA1_next_M3] = (unsigned char)(_t_>>24); };
			vaux_dst = vaux_dst_next;
			vaux = vaux_next;
		}

		/*
		 * Convert Elf32_Verdef structure.
		 */
		{ register Elf32_Half _t_ = src->vd_version;
		(dst)[VD1_version_M0] = (unsigned char)_t_,
		(dst)[VD1_version_M1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->vd_flags;
		(dst)[VD1_flags_M0] = (unsigned char)_t_,
		(dst)[VD1_flags_M1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->vd_ndx;
		(dst)[VD1_ndx_M0] = (unsigned char)_t_,
		(dst)[VD1_ndx_M1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->vd_cnt;
		(dst)[VD1_cnt_M0] = (unsigned char)_t_,
		(dst)[VD1_cnt_M1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Word _t_ = src->vd_hash;
		(dst)[VD1_hash_M0] = (unsigned char)_t_,
		(dst)[VD1_hash_M1] = (unsigned char)(_t_>>8),
		(dst)[VD1_hash_M2] = (unsigned char)(_t_>>16),
		(dst)[VD1_hash_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->vd_aux;
		(dst)[VD1_aux_M0] = (unsigned char)_t_,
		(dst)[VD1_aux_M1] = (unsigned char)(_t_>>8),
		(dst)[VD1_aux_M2] = (unsigned char)(_t_>>16),
		(dst)[VD1_aux_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->vd_next;
		(dst)[VD1_next_M0] = (unsigned char)_t_,
		(dst)[VD1_next_M1] = (unsigned char)(_t_>>8),
		(dst)[VD1_next_M2] = (unsigned char)(_t_>>16),
		(dst)[VD1_next_M3] = (unsigned char)(_t_>>24); };
		src = next_verdef;
		dst = dst_next;
	} while (src < end);
}




static void
verneed_2L11_tof(unsigned char *dst, Elf32_Verneed *src, size_t cnt)
{
	/* LINTED */
	Elf32_Verneed	*end = (Elf32_Verneed *)((char *)src + cnt);

	do {
		Elf32_Verneed	*next_verneed;
		Elf32_Vernaux	*vaux;
		Elf32_Half	i;
		unsigned char	*vaux_dst;
		unsigned char	*dst_next;

		/* LINTED */
		next_verneed = (Elf32_Verneed *)(src->vn_next ?
		    (char *)src + src->vn_next : (char *)end);
		dst_next = dst + src->vn_next;

		/* LINTED */
		vaux = (Elf32_Vernaux *)((char *)src + src->vn_aux);
		vaux_dst = dst + src->vn_aux;

		/*
		 * Convert auxilary structures first
		 */
		for (i = 0; i < src->vn_cnt; i++) {
			Elf32_Vernaux *	vaux_next;
			unsigned char *	vaux_dst_next;

			/*
			 * because our source and destination can be
			 * the same place we need to figure out the
			 * next location now.
			 */
			/* LINTED */
			vaux_next = (Elf32_Vernaux *)((char *)vaux +
			    vaux->vna_next);
			vaux_dst_next = vaux_dst + vaux->vna_next;

			{ register Elf32_Word _t_ = vaux->vna_hash;
		(vaux_dst)[VNA1_hash_L0] = (unsigned char)_t_,
		(vaux_dst)[VNA1_hash_L1] = (unsigned char)(_t_>>8),
		(vaux_dst)[VNA1_hash_L2] = (unsigned char)(_t_>>16),
		(vaux_dst)[VNA1_hash_L3] = (unsigned char)(_t_>>24); };
			{ register Elf32_Half _t_ = vaux->vna_flags;
		(vaux_dst)[VNA1_flags_L0] = (unsigned char)_t_,
		(vaux_dst)[VNA1_flags_L1] = (unsigned char)(_t_>>8); };
			{ register Elf32_Half _t_ = vaux->vna_other;
		(vaux_dst)[VNA1_other_L0] = (unsigned char)_t_,
		(vaux_dst)[VNA1_other_L1] = (unsigned char)(_t_>>8); };
			{ register Elf32_Addr _t_ = vaux->vna_name;
		(vaux_dst)[VNA1_name_L0] = (unsigned char)_t_,
		(vaux_dst)[VNA1_name_L1] = (unsigned char)(_t_>>8),
		(vaux_dst)[VNA1_name_L2] = (unsigned char)(_t_>>16),
		(vaux_dst)[VNA1_name_L3] = (unsigned char)(_t_>>24); };
			{ register Elf32_Word _t_ = vaux->vna_next;
		(vaux_dst)[VNA1_next_L0] = (unsigned char)_t_,
		(vaux_dst)[VNA1_next_L1] = (unsigned char)(_t_>>8),
		(vaux_dst)[VNA1_next_L2] = (unsigned char)(_t_>>16),
		(vaux_dst)[VNA1_next_L3] = (unsigned char)(_t_>>24); };
			vaux_dst = vaux_dst_next;
			vaux = vaux_next;
		}
		/*
		 * Convert Elf32_Verneed structure.
		 */
		{ register Elf32_Half _t_ = src->vn_version;
		(dst)[VN1_version_L0] = (unsigned char)_t_,
		(dst)[VN1_version_L1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->vn_cnt;
		(dst)[VN1_cnt_L0] = (unsigned char)_t_,
		(dst)[VN1_cnt_L1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Addr _t_ = src->vn_file;
		(dst)[VN1_file_L0] = (unsigned char)_t_,
		(dst)[VN1_file_L1] = (unsigned char)(_t_>>8),
		(dst)[VN1_file_L2] = (unsigned char)(_t_>>16),
		(dst)[VN1_file_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->vn_aux;
		(dst)[VN1_aux_L0] = (unsigned char)_t_,
		(dst)[VN1_aux_L1] = (unsigned char)(_t_>>8),
		(dst)[VN1_aux_L2] = (unsigned char)(_t_>>16),
		(dst)[VN1_aux_L3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->vn_next;
		(dst)[VN1_next_L0] = (unsigned char)_t_,
		(dst)[VN1_next_L1] = (unsigned char)(_t_>>8),
		(dst)[VN1_next_L2] = (unsigned char)(_t_>>16),
		(dst)[VN1_next_L3] = (unsigned char)(_t_>>24); };
		src = next_verneed;
		dst = dst_next;
	} while (src < end);
}

static void
verneed_2M11_tof(unsigned char *dst, Elf32_Verneed *src, size_t cnt)
{
	/* LINTED */
	Elf32_Verneed	*end = (Elf32_Verneed *)((char *)src + cnt);

	do {
		Elf32_Verneed	*next_verneed;
		Elf32_Vernaux	*vaux;
		Elf32_Half	i;
		unsigned char	*vaux_dst;
		unsigned char	*dst_next;

		/* LINTED */
		next_verneed = (Elf32_Verneed *)(src->vn_next ?
		    (char *)src + src->vn_next : (char *)end);
		dst_next = dst + src->vn_next;

		/* LINTED */
		vaux = (Elf32_Vernaux *)((char *)src + src->vn_aux);
		vaux_dst = dst + src->vn_aux;

		/*
		 * Convert auxilary structures first
		 */
		for (i = 0; i < src->vn_cnt; i++) {
			Elf32_Vernaux *	vaux_next;
			unsigned char *	vaux_dst_next;

			/*
			 * because our source and destination can be
			 * the same place we need to figure out the
			 * next location now.
			 */
			/* LINTED */
			vaux_next = (Elf32_Vernaux *)((char *)vaux +
			    vaux->vna_next);
			vaux_dst_next = vaux_dst + vaux->vna_next;

			{ register Elf32_Word _t_ = vaux->vna_hash;
		(vaux_dst)[VNA1_hash_M0] = (unsigned char)_t_,
		(vaux_dst)[VNA1_hash_M1] = (unsigned char)(_t_>>8),
		(vaux_dst)[VNA1_hash_M2] = (unsigned char)(_t_>>16),
		(vaux_dst)[VNA1_hash_M3] = (unsigned char)(_t_>>24); };
			{ register Elf32_Half _t_ = vaux->vna_flags;
		(vaux_dst)[VNA1_flags_M0] = (unsigned char)_t_,
		(vaux_dst)[VNA1_flags_M1] = (unsigned char)(_t_>>8); };
			{ register Elf32_Half _t_ = vaux->vna_other;
		(vaux_dst)[VNA1_other_M0] = (unsigned char)_t_,
		(vaux_dst)[VNA1_other_M1] = (unsigned char)(_t_>>8); };
			{ register Elf32_Addr _t_ = vaux->vna_name;
		(vaux_dst)[VNA1_name_M0] = (unsigned char)_t_,
		(vaux_dst)[VNA1_name_M1] = (unsigned char)(_t_>>8),
		(vaux_dst)[VNA1_name_M2] = (unsigned char)(_t_>>16),
		(vaux_dst)[VNA1_name_M3] = (unsigned char)(_t_>>24); };
			{ register Elf32_Word _t_ = vaux->vna_next;
		(vaux_dst)[VNA1_next_M0] = (unsigned char)_t_,
		(vaux_dst)[VNA1_next_M1] = (unsigned char)(_t_>>8),
		(vaux_dst)[VNA1_next_M2] = (unsigned char)(_t_>>16),
		(vaux_dst)[VNA1_next_M3] = (unsigned char)(_t_>>24); };
			vaux_dst = vaux_dst_next;
			vaux = vaux_next;
		}
		/*
		 * Convert Elf32_Verneed structure.
		 */
		{ register Elf32_Half _t_ = src->vn_version;
		(dst)[VN1_version_M0] = (unsigned char)_t_,
		(dst)[VN1_version_M1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Half _t_ = src->vn_cnt;
		(dst)[VN1_cnt_M0] = (unsigned char)_t_,
		(dst)[VN1_cnt_M1] = (unsigned char)(_t_>>8); };
		{ register Elf32_Addr _t_ = src->vn_file;
		(dst)[VN1_file_M0] = (unsigned char)_t_,
		(dst)[VN1_file_M1] = (unsigned char)(_t_>>8),
		(dst)[VN1_file_M2] = (unsigned char)(_t_>>16),
		(dst)[VN1_file_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->vn_aux;
		(dst)[VN1_aux_M0] = (unsigned char)_t_,
		(dst)[VN1_aux_M1] = (unsigned char)(_t_>>8),
		(dst)[VN1_aux_M2] = (unsigned char)(_t_>>16),
		(dst)[VN1_aux_M3] = (unsigned char)(_t_>>24); };
		{ register Elf32_Word _t_ = src->vn_next;
		(dst)[VN1_next_M0] = (unsigned char)_t_,
		(dst)[VN1_next_M1] = (unsigned char)(_t_>>8),
		(dst)[VN1_next_M2] = (unsigned char)(_t_>>16),
		(dst)[VN1_next_M3] = (unsigned char)(_t_>>24); };
		src = next_verneed;
		dst = dst_next;
	} while (src < end);
}


/* xlate to memory format
 *
 *	..._tom(name, data) -- macros
 *
 *	Recall that the memory format may be larger than the
 *	file format (equal versions).  Use "backward" copy.
 *	All these routines require non-null, non-zero arguments.
 */





static void
addr_2L_tom(Elf32_Addr *dst, unsigned char *src, size_t cnt)
{
	Elf32_Addr	*end = dst;

	dst += cnt;
	src += cnt * A_sizeof;
	while (dst-- > end) {
		src -= A_sizeof;
		*dst = (((((((Elf32_Addr)(src)[A_L3]<<8)
		+(src)[A_L2])<<8)
		+(src)[A_L1])<<8)
		+(src)[A_L0]);
	}
}

static void
addr_2M_tom(Elf32_Addr *dst, unsigned char *src, size_t cnt)
{
	Elf32_Addr	*end = dst;

	dst += cnt;
	src += cnt * A_sizeof;
	while (dst-- > end) {
		src -= A_sizeof;
		*dst = (((((((Elf32_Addr)(src)[A_M3]<<8)
		+(src)[A_M2])<<8)
		+(src)[A_M1])<<8)
		+(src)[A_M0]);
	}
}





static void
dyn_2L11_tom(Elf32_Dyn *dst, unsigned char *src, size_t cnt)
{
	Elf32_Dyn	*end = dst + cnt;

	do {
		dst->d_tag = (((((((Elf32_Word)(src)[D1_tag_L3]<<8)
		+(src)[D1_tag_L2])<<8)
		+(src)[D1_tag_L1])<<8)
		+(src)[D1_tag_L0]);
		dst->d_un.d_val = (((((((Elf32_Word)(src)[D1_val_L3]<<8)
		+(src)[D1_val_L2])<<8)
		+(src)[D1_val_L1])<<8)
		+(src)[D1_val_L0]);
		src += D1_sizeof;
	} while (++dst < end);
}

static void
dyn_2M11_tom(Elf32_Dyn *dst, unsigned char *src, size_t cnt)
{
	Elf32_Dyn	*end = dst + cnt;

	do {
		dst->d_tag = (((((((Elf32_Word)(src)[D1_tag_M3]<<8)
		+(src)[D1_tag_M2])<<8)
		+(src)[D1_tag_M1])<<8)
		+(src)[D1_tag_M0]);
		dst->d_un.d_val = (((((((Elf32_Word)(src)[D1_val_M3]<<8)
		+(src)[D1_val_M2])<<8)
		+(src)[D1_val_M1])<<8)
		+(src)[D1_val_M0]);
		src += D1_sizeof;
	} while (++dst < end);
}





static void
ehdr_2L11_tom(Elf32_Ehdr *dst, unsigned char *src, size_t cnt)
{
	Elf32_Ehdr	*end = dst;

	dst += cnt;
	src += cnt * E1_sizeof;
	while (dst-- > end) {
		src -= E1_sizeof;
		dst->e_shstrndx = (((Elf32_Half)(src)[E1_shstrndx_L1]<<8)+(src)[E1_shstrndx_L0]);
		dst->e_shnum = (((Elf32_Half)(src)[E1_shnum_L1]<<8)+(src)[E1_shnum_L0]);
		dst->e_shentsize = (((Elf32_Half)(src)[E1_shentsize_L1]<<8)+(src)[E1_shentsize_L0]);
		dst->e_phnum = (((Elf32_Half)(src)[E1_phnum_L1]<<8)+(src)[E1_phnum_L0]);
		dst->e_phentsize = (((Elf32_Half)(src)[E1_phentsize_L1]<<8)+(src)[E1_phentsize_L0]);
		dst->e_ehsize = (((Elf32_Half)(src)[E1_ehsize_L1]<<8)+(src)[E1_ehsize_L0]);
		dst->e_flags = (((((((Elf32_Word)(src)[E1_flags_L3]<<8)
		+(src)[E1_flags_L2])<<8)
		+(src)[E1_flags_L1])<<8)
		+(src)[E1_flags_L0]);
		dst->e_shoff = (((((((Elf32_Off)(src)[E1_shoff_L3]<<8)
		+(src)[E1_shoff_L2])<<8)
		+(src)[E1_shoff_L1])<<8)
		+(src)[E1_shoff_L0]);
		dst->e_phoff = (((((((Elf32_Off)(src)[E1_phoff_L3]<<8)
		+(src)[E1_phoff_L2])<<8)
		+(src)[E1_phoff_L1])<<8)
		+(src)[E1_phoff_L0]);
		dst->e_entry = (((((((Elf32_Addr)(src)[E1_entry_L3]<<8)
		+(src)[E1_entry_L2])<<8)
		+(src)[E1_entry_L1])<<8)
		+(src)[E1_entry_L0]);
		dst->e_version = (((((((Elf32_Word)(src)[E1_version_L3]<<8)
		+(src)[E1_version_L2])<<8)
		+(src)[E1_version_L1])<<8)
		+(src)[E1_version_L0]);
		dst->e_machine = (((Elf32_Half)(src)[E1_machine_L1]<<8)+(src)[E1_machine_L0]);
		dst->e_type = (((Elf32_Half)(src)[E1_type_L1]<<8)+(src)[E1_type_L0]);
		if (dst->e_ident != &src[E1_ident])
			(void) memcpy(dst->e_ident, &src[E1_ident], E1_Nident);
	}
}

static void
ehdr_2M11_tom(Elf32_Ehdr *dst, unsigned char *src, size_t cnt)
{
	Elf32_Ehdr	*end = dst;

	dst += cnt;
	src += cnt * E1_sizeof;
	while (dst-- > end) {
		src -= E1_sizeof;
		dst->e_shstrndx = (((Elf32_Half)(src)[E1_shstrndx_M1]<<8)+(src)[E1_shstrndx_M0]);
		dst->e_shnum = (((Elf32_Half)(src)[E1_shnum_M1]<<8)+(src)[E1_shnum_M0]);
		dst->e_shentsize = (((Elf32_Half)(src)[E1_shentsize_M1]<<8)+(src)[E1_shentsize_M0]);
		dst->e_phnum = (((Elf32_Half)(src)[E1_phnum_M1]<<8)+(src)[E1_phnum_M0]);
		dst->e_phentsize = (((Elf32_Half)(src)[E1_phentsize_M1]<<8)+(src)[E1_phentsize_M0]);
		dst->e_ehsize = (((Elf32_Half)(src)[E1_ehsize_M1]<<8)+(src)[E1_ehsize_M0]);
		dst->e_flags = (((((((Elf32_Word)(src)[E1_flags_M3]<<8)
		+(src)[E1_flags_M2])<<8)
		+(src)[E1_flags_M1])<<8)
		+(src)[E1_flags_M0]);
		dst->e_shoff = (((((((Elf32_Off)(src)[E1_shoff_M3]<<8)
		+(src)[E1_shoff_M2])<<8)
		+(src)[E1_shoff_M1])<<8)
		+(src)[E1_shoff_M0]);
		dst->e_phoff = (((((((Elf32_Off)(src)[E1_phoff_M3]<<8)
		+(src)[E1_phoff_M2])<<8)
		+(src)[E1_phoff_M1])<<8)
		+(src)[E1_phoff_M0]);
		dst->e_entry = (((((((Elf32_Addr)(src)[E1_entry_M3]<<8)
		+(src)[E1_entry_M2])<<8)
		+(src)[E1_entry_M1])<<8)
		+(src)[E1_entry_M0]);
		dst->e_version = (((((((Elf32_Word)(src)[E1_version_M3]<<8)
		+(src)[E1_version_M2])<<8)
		+(src)[E1_version_M1])<<8)
		+(src)[E1_version_M0]);
		dst->e_machine = (((Elf32_Half)(src)[E1_machine_M1]<<8)+(src)[E1_machine_M0]);
		dst->e_type = (((Elf32_Half)(src)[E1_type_M1]<<8)+(src)[E1_type_M0]);
		if (dst->e_ident != &src[E1_ident])
			(void) memcpy(dst->e_ident, &src[E1_ident], E1_Nident);
	}
}





static void
half_2L_tom(Elf32_Half *dst, unsigned char *src, size_t cnt)
{
	Elf32_Half	*end = dst;

	dst += cnt;
	src += cnt * H_sizeof;
	while (dst-- > end) {
		src -= H_sizeof;
		*dst = (((Elf32_Half)(src)[H_L1]<<8)+(src)[H_L0]);
	}
}

static void
half_2M_tom(Elf32_Half *dst, unsigned char *src, size_t cnt)
{
	Elf32_Half	*end = dst;

	dst += cnt;
	src += cnt * H_sizeof;
	while (dst-- > end) {
		src -= H_sizeof;
		*dst = (((Elf32_Half)(src)[H_M1]<<8)+(src)[H_M0]);
	}
}





static void
move_2L11_tom(Elf32_Move *dst, unsigned char *src, size_t cnt)
{
	Elf32_Move	*end = dst + cnt;

	do {
		dst->m_value = (((((((((((Elf32_Lword)(src)[M1_value_L7]<<8)
		+(src)[M1_value_L6]<<8)
		+(src)[M1_value_L5]<<8)
		+(src)[M1_value_L4]<<8)
		+(src)[M1_value_L3]<<8)
		+(src)[M1_value_L2])<<8)
		+(src)[M1_value_L1])<<8)
		+(src)[M1_value_L0]);
		dst->m_info = (((((((Elf32_Word)(src)[M1_info_L3]<<8)
		+(src)[M1_info_L2])<<8)
		+(src)[M1_info_L1])<<8)
		+(src)[M1_info_L0]);
		dst->m_poffset = (((((((Elf32_Word)(src)[M1_poffset_L3]<<8)
		+(src)[M1_poffset_L2])<<8)
		+(src)[M1_poffset_L1])<<8)
		+(src)[M1_poffset_L0]);
		dst->m_repeat = (((Elf32_Half)(src)[M1_repeat_L1]<<8)+(src)[M1_repeat_L0]);
		dst->m_stride = (((Elf32_Half)(src)[M1_stride_L1]<<8)+(src)[M1_stride_L0]);
		src += M1_sizeof;
	} while (++dst < end);
}

static void
move_2M11_tom(Elf32_Move *dst, unsigned char *src, size_t cnt)
{
	Elf32_Move	*end = dst + cnt;

	do {
		dst->m_value = (((((((((((Elf32_Lword)(src)[M1_value_M7]<<8)
		+(src)[M1_value_M6]<<8)
		+(src)[M1_value_M5]<<8)
		+(src)[M1_value_M4]<<8)
		+(src)[M1_value_M3]<<8)
		+(src)[M1_value_M2])<<8)
		+(src)[M1_value_M1])<<8)
		+(src)[M1_value_M0]);
		dst->m_info = (((((((Elf32_Word)(src)[M1_info_M3]<<8)
		+(src)[M1_info_M2])<<8)
		+(src)[M1_info_M1])<<8)
		+(src)[M1_info_M0]);
		dst->m_poffset = (((((((Elf32_Word)(src)[M1_poffset_M3]<<8)
		+(src)[M1_poffset_M2])<<8)
		+(src)[M1_poffset_M1])<<8)
		+(src)[M1_poffset_M0]);
		dst->m_repeat = (((Elf32_Half)(src)[M1_repeat_M1]<<8)+(src)[M1_repeat_M0]);
		dst->m_stride = (((Elf32_Half)(src)[M1_stride_M1]<<8)+(src)[M1_stride_M0]);
		src += M1_sizeof;
	} while (++dst < end);
}





static void
movep_2L11_tom(Elf32_Move *dst, unsigned char *src, size_t cnt)
{
	Elf32_Move		*end = dst + cnt;

	do
	{
		dst->m_value = (((((((((((Elf32_Lword)(src)[MP1_value_L7]<<8)
		+(src)[MP1_value_L6]<<8)
		+(src)[MP1_value_L5]<<8)
		+(src)[MP1_value_L4]<<8)
		+(src)[MP1_value_L3]<<8)
		+(src)[MP1_value_L2])<<8)
		+(src)[MP1_value_L1])<<8)
		+(src)[MP1_value_L0]);
		dst->m_info = (((((((Elf32_Word)(src)[MP1_info_L3]<<8)
		+(src)[MP1_info_L2])<<8)
		+(src)[MP1_info_L1])<<8)
		+(src)[MP1_info_L0]);
		dst->m_poffset = (((((((Elf32_Word)(src)[MP1_poffset_L3]<<8)
		+(src)[MP1_poffset_L2])<<8)
		+(src)[MP1_poffset_L1])<<8)
		+(src)[MP1_poffset_L0]);
		dst->m_repeat = (((Elf32_Half)(src)[MP1_repeat_L1]<<8)+(src)[MP1_repeat_L0]);
		dst->m_stride = (((Elf32_Half)(src)[MP1_stride_L1]<<8)+(src)[MP1_stride_L0]);
		src += MP1_sizeof;
	} while (++dst < end);
}

static void
movep_2M11_tom(Elf32_Move *dst, unsigned char *src, size_t cnt)
{
	Elf32_Move		*end = dst + cnt;

	do
	{
		dst->m_value = (((((((((((Elf32_Lword)(src)[MP1_value_M7]<<8)
		+(src)[MP1_value_M6]<<8)
		+(src)[MP1_value_M5]<<8)
		+(src)[MP1_value_M4]<<8)
		+(src)[MP1_value_M3]<<8)
		+(src)[MP1_value_M2])<<8)
		+(src)[MP1_value_M1])<<8)
		+(src)[MP1_value_M0]);
		dst->m_info = (((((((Elf32_Word)(src)[MP1_info_M3]<<8)
		+(src)[MP1_info_M2])<<8)
		+(src)[MP1_info_M1])<<8)
		+(src)[MP1_info_M0]);
		dst->m_poffset = (((((((Elf32_Word)(src)[MP1_poffset_M3]<<8)
		+(src)[MP1_poffset_M2])<<8)
		+(src)[MP1_poffset_M1])<<8)
		+(src)[MP1_poffset_M0]);
		dst->m_repeat = (((Elf32_Half)(src)[MP1_repeat_M1]<<8)+(src)[MP1_repeat_M0]);
		dst->m_stride = (((Elf32_Half)(src)[MP1_stride_M1]<<8)+(src)[MP1_stride_M0]);
		src += MP1_sizeof;
	} while (++dst < end);
}





static void
note_2L11_tom(Elf32_Nhdr *dst, unsigned char *src, size_t cnt)
{
	/* LINTED */
	Elf32_Nhdr	*end = (Elf32_Nhdr *)((char *)dst + cnt);

	while (dst < end) {
		Elf32_Nhdr *	nhdr;
		unsigned char *	namestr;
		void *		desc;
		Elf32_Word	field_sz;

		dst->n_namesz = (((((((Elf32_Word)(src)[N1_namesz_L3]<<8)
		+(src)[N1_namesz_L2])<<8)
		+(src)[N1_namesz_L1])<<8)
		+(src)[N1_namesz_L0]);
		dst->n_descsz = (((((((Elf32_Word)(src)[N1_descsz_L3]<<8)
		+(src)[N1_descsz_L2])<<8)
		+(src)[N1_descsz_L1])<<8)
		+(src)[N1_descsz_L0]);
		dst->n_type = (((((((Elf32_Word)(src)[N1_type_L3]<<8)
		+(src)[N1_type_L2])<<8)
		+(src)[N1_type_L1])<<8)
		+(src)[N1_type_L0]);
		nhdr = dst;
		/* LINTED */
		dst = (Elf32_Nhdr *)((char *)dst + sizeof (Elf32_Nhdr));
		namestr = src + N1_sizeof;
		field_sz = S_ROUND(nhdr->n_namesz, sizeof (Elf32_Word));
		(void)memcpy((void *)dst, namestr, field_sz);
		desc = namestr + field_sz;
		/* LINTED */
		dst = (Elf32_Nhdr *)((char *)dst + field_sz);
		field_sz = nhdr->n_descsz;
		(void)memcpy(dst, desc, field_sz);
		field_sz = S_ROUND(field_sz, sizeof (Elf32_Word));
		/* LINTED */
		dst = (Elf32_Nhdr *)((char *)dst + field_sz);
		src = (unsigned char *)desc + field_sz;
	}
}

static void
note_2M11_tom(Elf32_Nhdr *dst, unsigned char *src, size_t cnt)
{
	/* LINTED */
	Elf32_Nhdr	*end = (Elf32_Nhdr *)((char *)dst + cnt);

	while (dst < end) {
		Elf32_Nhdr *	nhdr;
		unsigned char *	namestr;
		void *		desc;
		Elf32_Word	field_sz;

		dst->n_namesz = (((((((Elf32_Word)(src)[N1_namesz_M3]<<8)
		+(src)[N1_namesz_M2])<<8)
		+(src)[N1_namesz_M1])<<8)
		+(src)[N1_namesz_M0]);
		dst->n_descsz = (((((((Elf32_Word)(src)[N1_descsz_M3]<<8)
		+(src)[N1_descsz_M2])<<8)
		+(src)[N1_descsz_M1])<<8)
		+(src)[N1_descsz_M0]);
		dst->n_type = (((((((Elf32_Word)(src)[N1_type_M3]<<8)
		+(src)[N1_type_M2])<<8)
		+(src)[N1_type_M1])<<8)
		+(src)[N1_type_M0]);
		nhdr = dst;
		/* LINTED */
		dst = (Elf32_Nhdr *)((char *)dst + sizeof (Elf32_Nhdr));
		namestr = src + N1_sizeof;
		field_sz = S_ROUND(nhdr->n_namesz, sizeof (Elf32_Word));
		(void)memcpy((void *)dst, namestr, field_sz);
		desc = namestr + field_sz;
		/* LINTED */
		dst = (Elf32_Nhdr *)((char *)dst + field_sz);
		field_sz = nhdr->n_descsz;
		(void)memcpy(dst, desc, field_sz);
		field_sz = S_ROUND(field_sz, sizeof (Elf32_Word));
		/* LINTED */
		dst = (Elf32_Nhdr *)((char *)dst + field_sz);
		src = (unsigned char *)desc + field_sz;
	}
}





static void
off_2L_tom(Elf32_Off *dst, unsigned char *src, size_t cnt)
{
	Elf32_Off	*end = dst;

	dst += cnt;
	src += cnt * O_sizeof;
	while (dst-- > end) {
		src -= O_sizeof;
		*dst = (((((((Elf32_Off)(src)[O_L3]<<8)
		+(src)[O_L2])<<8)
		+(src)[O_L1])<<8)
		+(src)[O_L0]);
	}
}

static void
off_2M_tom(Elf32_Off *dst, unsigned char *src, size_t cnt)
{
	Elf32_Off	*end = dst;

	dst += cnt;
	src += cnt * O_sizeof;
	while (dst-- > end) {
		src -= O_sizeof;
		*dst = (((((((Elf32_Off)(src)[O_M3]<<8)
		+(src)[O_M2])<<8)
		+(src)[O_M1])<<8)
		+(src)[O_M0]);
	}
}





static void
phdr_2L11_tom(Elf32_Phdr *dst, unsigned char *src, size_t cnt)
{
	Elf32_Phdr	*end = dst;

	dst += cnt;
	src += cnt * P1_sizeof;
	while (dst-- > end) {
		src -= P1_sizeof;
		dst->p_align = (((((((Elf32_Word)(src)[P1_align_L3]<<8)
		+(src)[P1_align_L2])<<8)
		+(src)[P1_align_L1])<<8)
		+(src)[P1_align_L0]);
		dst->p_flags = (((((((Elf32_Word)(src)[P1_flags_L3]<<8)
		+(src)[P1_flags_L2])<<8)
		+(src)[P1_flags_L1])<<8)
		+(src)[P1_flags_L0]);
		dst->p_memsz = (((((((Elf32_Word)(src)[P1_memsz_L3]<<8)
		+(src)[P1_memsz_L2])<<8)
		+(src)[P1_memsz_L1])<<8)
		+(src)[P1_memsz_L0]);
		dst->p_filesz = (((((((Elf32_Word)(src)[P1_filesz_L3]<<8)
		+(src)[P1_filesz_L2])<<8)
		+(src)[P1_filesz_L1])<<8)
		+(src)[P1_filesz_L0]);
		dst->p_paddr = (((((((Elf32_Addr)(src)[P1_paddr_L3]<<8)
		+(src)[P1_paddr_L2])<<8)
		+(src)[P1_paddr_L1])<<8)
		+(src)[P1_paddr_L0]);
		dst->p_vaddr = (((((((Elf32_Addr)(src)[P1_vaddr_L3]<<8)
		+(src)[P1_vaddr_L2])<<8)
		+(src)[P1_vaddr_L1])<<8)
		+(src)[P1_vaddr_L0]);
		dst->p_offset = (((((((Elf32_Off)(src)[P1_offset_L3]<<8)
		+(src)[P1_offset_L2])<<8)
		+(src)[P1_offset_L1])<<8)
		+(src)[P1_offset_L0]);
		dst->p_type = (((((((Elf32_Word)(src)[P1_type_L3]<<8)
		+(src)[P1_type_L2])<<8)
		+(src)[P1_type_L1])<<8)
		+(src)[P1_type_L0]);
	}
}

static void
phdr_2M11_tom(Elf32_Phdr *dst, unsigned char *src, size_t cnt)
{
	Elf32_Phdr	*end = dst;

	dst += cnt;
	src += cnt * P1_sizeof;
	while (dst-- > end) {
		src -= P1_sizeof;
		dst->p_align = (((((((Elf32_Word)(src)[P1_align_M3]<<8)
		+(src)[P1_align_M2])<<8)
		+(src)[P1_align_M1])<<8)
		+(src)[P1_align_M0]);
		dst->p_flags = (((((((Elf32_Word)(src)[P1_flags_M3]<<8)
		+(src)[P1_flags_M2])<<8)
		+(src)[P1_flags_M1])<<8)
		+(src)[P1_flags_M0]);
		dst->p_memsz = (((((((Elf32_Word)(src)[P1_memsz_M3]<<8)
		+(src)[P1_memsz_M2])<<8)
		+(src)[P1_memsz_M1])<<8)
		+(src)[P1_memsz_M0]);
		dst->p_filesz = (((((((Elf32_Word)(src)[P1_filesz_M3]<<8)
		+(src)[P1_filesz_M2])<<8)
		+(src)[P1_filesz_M1])<<8)
		+(src)[P1_filesz_M0]);
		dst->p_paddr = (((((((Elf32_Addr)(src)[P1_paddr_M3]<<8)
		+(src)[P1_paddr_M2])<<8)
		+(src)[P1_paddr_M1])<<8)
		+(src)[P1_paddr_M0]);
		dst->p_vaddr = (((((((Elf32_Addr)(src)[P1_vaddr_M3]<<8)
		+(src)[P1_vaddr_M2])<<8)
		+(src)[P1_vaddr_M1])<<8)
		+(src)[P1_vaddr_M0]);
		dst->p_offset = (((((((Elf32_Off)(src)[P1_offset_M3]<<8)
		+(src)[P1_offset_M2])<<8)
		+(src)[P1_offset_M1])<<8)
		+(src)[P1_offset_M0]);
		dst->p_type = (((((((Elf32_Word)(src)[P1_type_M3]<<8)
		+(src)[P1_type_M2])<<8)
		+(src)[P1_type_M1])<<8)
		+(src)[P1_type_M0]);
	}
}





static void
rel_2L11_tom(Elf32_Rel *dst, unsigned char *src, size_t cnt)
{
	Elf32_Rel	*end = dst;

	dst += cnt;
	src += cnt * R1_sizeof;
	while (dst-- > end) {
		src -= R1_sizeof;
		dst->r_info = (((((((Elf32_Word)(src)[R1_info_L3]<<8)
		+(src)[R1_info_L2])<<8)
		+(src)[R1_info_L1])<<8)
		+(src)[R1_info_L0]);
		dst->r_offset = (((((((Elf32_Addr)(src)[R1_offset_L3]<<8)
		+(src)[R1_offset_L2])<<8)
		+(src)[R1_offset_L1])<<8)
		+(src)[R1_offset_L0]);
	}
}

static void
rel_2M11_tom(Elf32_Rel *dst, unsigned char *src, size_t cnt)
{
	Elf32_Rel	*end = dst;

	dst += cnt;
	src += cnt * R1_sizeof;
	while (dst-- > end) {
		src -= R1_sizeof;
		dst->r_info = (((((((Elf32_Word)(src)[R1_info_M3]<<8)
		+(src)[R1_info_M2])<<8)
		+(src)[R1_info_M1])<<8)
		+(src)[R1_info_M0]);
		dst->r_offset = (((((((Elf32_Addr)(src)[R1_offset_M3]<<8)
		+(src)[R1_offset_M2])<<8)
		+(src)[R1_offset_M1])<<8)
		+(src)[R1_offset_M0]);
	}
}





static void
rela_2L11_tom(Elf32_Rela *dst, unsigned char *src, size_t cnt)
{
	Elf32_Rela	*end = dst;

	dst += cnt;
	src += cnt * RA1_sizeof;
	while (dst-- > end) {
		src -= RA1_sizeof;
		/*CONSTANTCONDITION*/
		if (~(Elf32_Word)0 == -(Elf32_Sword)1 &&	/* 2s comp */
		    ~(~(Elf32_Word)0 >> 1) == HI32) {
			dst->r_addend = (((((((Elf32_Word)(src)[RA1_addend_L3]<<8)
		+(src)[RA1_addend_L2])<<8)
		+(src)[RA1_addend_L1])<<8)
		+(src)[RA1_addend_L0]);
		} else {
			union {
				Elf32_Word w;
				Elf32_Sword sw;
			} u;

			if ((u.w = (((((((Elf32_Word)(src)[RA1_addend_L3]<<8)
		+(src)[RA1_addend_L2])<<8)
		+(src)[RA1_addend_L1])<<8)
		+(src)[RA1_addend_L0])) & HI32) {
				u.w |= ~(Elf32_Word)LO31;
				u.w = ~u.w + 1;
				u.sw = -u.w;
			}
			dst->r_addend = u.sw;
		}
		dst->r_info = (((((((Elf32_Word)(src)[RA1_info_L3]<<8)
		+(src)[RA1_info_L2])<<8)
		+(src)[RA1_info_L1])<<8)
		+(src)[RA1_info_L0]);
		dst->r_offset = (((((((Elf32_Addr)(src)[RA1_offset_L3]<<8)
		+(src)[RA1_offset_L2])<<8)
		+(src)[RA1_offset_L1])<<8)
		+(src)[RA1_offset_L0]);
	}
}

static void
rela_2M11_tom(Elf32_Rela *dst, unsigned char *src, size_t cnt)
{
	Elf32_Rela	*end = dst;

	dst += cnt;
	src += cnt * RA1_sizeof;
	while (dst-- > end) {
		src -= RA1_sizeof;
		/*CONSTANTCONDITION*/
		if (~(Elf32_Word)0 == -(Elf32_Sword)1 &&	/* 2s comp */
		    ~(~(Elf32_Word)0 >> 1) == HI32) {
			dst->r_addend = (((((((Elf32_Word)(src)[RA1_addend_M3]<<8)
		+(src)[RA1_addend_M2])<<8)
		+(src)[RA1_addend_M1])<<8)
		+(src)[RA1_addend_M0]);
		} else {
			union {
				Elf32_Word w;
				Elf32_Sword sw;
			} u;

			if ((u.w = (((((((Elf32_Word)(src)[RA1_addend_M3]<<8)
		+(src)[RA1_addend_M2])<<8)
		+(src)[RA1_addend_M1])<<8)
		+(src)[RA1_addend_M0])) & HI32) {
				u.w |= ~(Elf32_Word)LO31;
				u.w = ~u.w + 1;
				u.sw = -u.w;
			}
			dst->r_addend = u.sw;
		}
		dst->r_info = (((((((Elf32_Word)(src)[RA1_info_M3]<<8)
		+(src)[RA1_info_M2])<<8)
		+(src)[RA1_info_M1])<<8)
		+(src)[RA1_info_M0]);
		dst->r_offset = (((((((Elf32_Addr)(src)[RA1_offset_M3]<<8)
		+(src)[RA1_offset_M2])<<8)
		+(src)[RA1_offset_M1])<<8)
		+(src)[RA1_offset_M0]);
	}
}





static void
shdr_2L11_tom(Elf32_Shdr *dst, unsigned char *src, size_t cnt)
{
	Elf32_Shdr	*end = dst;

	dst += cnt;
	src += cnt * SH1_sizeof;
	while (dst-- > end) {
		src -= SH1_sizeof;
		dst->sh_entsize = (((((((Elf32_Word)(src)[SH1_entsize_L3]<<8)
		+(src)[SH1_entsize_L2])<<8)
		+(src)[SH1_entsize_L1])<<8)
		+(src)[SH1_entsize_L0]);
		dst->sh_addralign = (((((((Elf32_Word)(src)[SH1_addralign_L3]<<8)
		+(src)[SH1_addralign_L2])<<8)
		+(src)[SH1_addralign_L1])<<8)
		+(src)[SH1_addralign_L0]);
		dst->sh_info = (((((((Elf32_Word)(src)[SH1_info_L3]<<8)
		+(src)[SH1_info_L2])<<8)
		+(src)[SH1_info_L1])<<8)
		+(src)[SH1_info_L0]);
		dst->sh_link = (((((((Elf32_Word)(src)[SH1_link_L3]<<8)
		+(src)[SH1_link_L2])<<8)
		+(src)[SH1_link_L1])<<8)
		+(src)[SH1_link_L0]);
		dst->sh_size = (((((((Elf32_Word)(src)[SH1_size_L3]<<8)
		+(src)[SH1_size_L2])<<8)
		+(src)[SH1_size_L1])<<8)
		+(src)[SH1_size_L0]);
		dst->sh_offset = (((((((Elf32_Off)(src)[SH1_offset_L3]<<8)
		+(src)[SH1_offset_L2])<<8)
		+(src)[SH1_offset_L1])<<8)
		+(src)[SH1_offset_L0]);
		dst->sh_addr = (((((((Elf32_Addr)(src)[SH1_addr_L3]<<8)
		+(src)[SH1_addr_L2])<<8)
		+(src)[SH1_addr_L1])<<8)
		+(src)[SH1_addr_L0]);
		dst->sh_flags = (((((((Elf32_Word)(src)[SH1_flags_L3]<<8)
		+(src)[SH1_flags_L2])<<8)
		+(src)[SH1_flags_L1])<<8)
		+(src)[SH1_flags_L0]);
		dst->sh_type = (((((((Elf32_Word)(src)[SH1_type_L3]<<8)
		+(src)[SH1_type_L2])<<8)
		+(src)[SH1_type_L1])<<8)
		+(src)[SH1_type_L0]);
		dst->sh_name = (((((((Elf32_Word)(src)[SH1_name_L3]<<8)
		+(src)[SH1_name_L2])<<8)
		+(src)[SH1_name_L1])<<8)
		+(src)[SH1_name_L0]);
	}
}

static void
shdr_2M11_tom(Elf32_Shdr *dst, unsigned char *src, size_t cnt)
{
	Elf32_Shdr	*end = dst;

	dst += cnt;
	src += cnt * SH1_sizeof;
	while (dst-- > end) {
		src -= SH1_sizeof;
		dst->sh_entsize = (((((((Elf32_Word)(src)[SH1_entsize_M3]<<8)
		+(src)[SH1_entsize_M2])<<8)
		+(src)[SH1_entsize_M1])<<8)
		+(src)[SH1_entsize_M0]);
		dst->sh_addralign = (((((((Elf32_Word)(src)[SH1_addralign_M3]<<8)
		+(src)[SH1_addralign_M2])<<8)
		+(src)[SH1_addralign_M1])<<8)
		+(src)[SH1_addralign_M0]);
		dst->sh_info = (((((((Elf32_Word)(src)[SH1_info_M3]<<8)
		+(src)[SH1_info_M2])<<8)
		+(src)[SH1_info_M1])<<8)
		+(src)[SH1_info_M0]);
		dst->sh_link = (((((((Elf32_Word)(src)[SH1_link_M3]<<8)
		+(src)[SH1_link_M2])<<8)
		+(src)[SH1_link_M1])<<8)
		+(src)[SH1_link_M0]);
		dst->sh_size = (((((((Elf32_Word)(src)[SH1_size_M3]<<8)
		+(src)[SH1_size_M2])<<8)
		+(src)[SH1_size_M1])<<8)
		+(src)[SH1_size_M0]);
		dst->sh_offset = (((((((Elf32_Off)(src)[SH1_offset_M3]<<8)
		+(src)[SH1_offset_M2])<<8)
		+(src)[SH1_offset_M1])<<8)
		+(src)[SH1_offset_M0]);
		dst->sh_addr = (((((((Elf32_Addr)(src)[SH1_addr_M3]<<8)
		+(src)[SH1_addr_M2])<<8)
		+(src)[SH1_addr_M1])<<8)
		+(src)[SH1_addr_M0]);
		dst->sh_flags = (((((((Elf32_Word)(src)[SH1_flags_M3]<<8)
		+(src)[SH1_flags_M2])<<8)
		+(src)[SH1_flags_M1])<<8)
		+(src)[SH1_flags_M0]);
		dst->sh_type = (((((((Elf32_Word)(src)[SH1_type_M3]<<8)
		+(src)[SH1_type_M2])<<8)
		+(src)[SH1_type_M1])<<8)
		+(src)[SH1_type_M0]);
		dst->sh_name = (((((((Elf32_Word)(src)[SH1_name_M3]<<8)
		+(src)[SH1_name_M2])<<8)
		+(src)[SH1_name_M1])<<8)
		+(src)[SH1_name_M0]);
	}
}






static void
sword_2L_tom(Elf32_Sword *dst, unsigned char *src, size_t cnt)
{
	Elf32_Sword	*end = dst;

	dst += cnt;
	src += cnt * W_sizeof;
	while (dst-- > end) {
		src -= W_sizeof;
		/*CONSTANTCONDITION*/
		if (~(Elf32_Word)0 == -(Elf32_Sword)1 &&	/* 2s comp */
		    ~(~(Elf32_Word)0 >> 1) == HI32) {
			*dst = (((((((Elf32_Word)(src)[W_L3]<<8)
		+(src)[W_L2])<<8)
		+(src)[W_L1])<<8)
		+(src)[W_L0]);
		} else {
			union {
				Elf32_Word w;
				Elf32_Sword sw;
			} u;

			if ((u.w = (((((((Elf32_Word)(src)[W_L3]<<8)
		+(src)[W_L2])<<8)
		+(src)[W_L1])<<8)
		+(src)[W_L0])) & HI32) {
				u.w |= ~(Elf32_Word)LO31;
				u.w = ~u.w + 1;
				u.sw = -u.w;
			}
			*dst = u.sw;
		}
	}
}

static void
sword_2M_tom(Elf32_Sword *dst, unsigned char *src, size_t cnt)
{
	Elf32_Sword	*end = dst;

	dst += cnt;
	src += cnt * W_sizeof;
	while (dst-- > end) {
		src -= W_sizeof;
		/*CONSTANTCONDITION*/
		if (~(Elf32_Word)0 == -(Elf32_Sword)1 &&	/* 2s comp */
		    ~(~(Elf32_Word)0 >> 1) == HI32) {
			*dst = (((((((Elf32_Word)(src)[W_M3]<<8)
		+(src)[W_M2])<<8)
		+(src)[W_M1])<<8)
		+(src)[W_M0]);
		} else {
			union {
				Elf32_Word w;
				Elf32_Sword sw;
			} u;

			if ((u.w = (((((((Elf32_Word)(src)[W_M3]<<8)
		+(src)[W_M2])<<8)
		+(src)[W_M1])<<8)
		+(src)[W_M0])) & HI32) {
				u.w |= ~(Elf32_Word)LO31;
				u.w = ~u.w + 1;
				u.sw = -u.w;
			}
			*dst = u.sw;
		}
	}
}





static void
cap_2L11_tom(Elf32_Cap *dst, unsigned char *src, size_t cnt)
{
	Elf32_Cap	*end = dst + cnt;

	do {
		dst->c_tag = (((((((Elf32_Word)(src)[C1_tag_L3]<<8)
		+(src)[C1_tag_L2])<<8)
		+(src)[C1_tag_L1])<<8)
		+(src)[C1_tag_L0]);
		dst->c_un.c_val = (((((((Elf32_Word)(src)[C1_val_L3]<<8)
		+(src)[C1_val_L2])<<8)
		+(src)[C1_val_L1])<<8)
		+(src)[C1_val_L0]);
		src += C1_sizeof;
	} while (++dst < end);
}

static void
cap_2M11_tom(Elf32_Cap *dst, unsigned char *src, size_t cnt)
{
	Elf32_Cap	*end = dst + cnt;

	do {
		dst->c_tag = (((((((Elf32_Word)(src)[C1_tag_M3]<<8)
		+(src)[C1_tag_M2])<<8)
		+(src)[C1_tag_M1])<<8)
		+(src)[C1_tag_M0]);
		dst->c_un.c_val = (((((((Elf32_Word)(src)[C1_val_M3]<<8)
		+(src)[C1_val_M2])<<8)
		+(src)[C1_val_M1])<<8)
		+(src)[C1_val_M0]);
		src += C1_sizeof;
	} while (++dst < end);
}





static void
syminfo_2L11_tom(Elf32_Syminfo *dst, unsigned char *src, size_t cnt)
{
	Elf32_Syminfo	*end = dst;

	dst += cnt;
	src += cnt * SI1_sizeof;
	while (dst-- > end) {
		src -= SI1_sizeof;
		dst->si_boundto = (((Elf32_Half)(src)[SI1_boundto_L1]<<8)+(src)[SI1_boundto_L0]);
		dst->si_flags = (((Elf32_Half)(src)[SI1_flags_L1]<<8)+(src)[SI1_flags_L0]);
	}
}

static void
syminfo_2M11_tom(Elf32_Syminfo *dst, unsigned char *src, size_t cnt)
{
	Elf32_Syminfo	*end = dst;

	dst += cnt;
	src += cnt * SI1_sizeof;
	while (dst-- > end) {
		src -= SI1_sizeof;
		dst->si_boundto = (((Elf32_Half)(src)[SI1_boundto_M1]<<8)+(src)[SI1_boundto_M0]);
		dst->si_flags = (((Elf32_Half)(src)[SI1_flags_M1]<<8)+(src)[SI1_flags_M0]);
	}
}





static void
sym_2L11_tom(Elf32_Sym *dst, unsigned char *src, size_t cnt)
{
	Elf32_Sym	*end = dst;

	dst += cnt;
	src += cnt * ST1_sizeof;
	while (dst-- > end) {
		src -= ST1_sizeof;
		dst->st_shndx = (((Elf32_Half)(src)[ST1_shndx_L1]<<8)+(src)[ST1_shndx_L0]);
		dst->st_other = ((unsigned char)(src)[ST1_other_L]);
		dst->st_info = ((unsigned char)(src)[ST1_info_L]);
		dst->st_size = (((((((Elf32_Word)(src)[ST1_size_L3]<<8)
		+(src)[ST1_size_L2])<<8)
		+(src)[ST1_size_L1])<<8)
		+(src)[ST1_size_L0]);
		dst->st_value = (((((((Elf32_Addr)(src)[ST1_value_L3]<<8)
		+(src)[ST1_value_L2])<<8)
		+(src)[ST1_value_L1])<<8)
		+(src)[ST1_value_L0]);
		dst->st_name = (((((((Elf32_Word)(src)[ST1_name_L3]<<8)
		+(src)[ST1_name_L2])<<8)
		+(src)[ST1_name_L1])<<8)
		+(src)[ST1_name_L0]);
	}
}

static void
sym_2M11_tom(Elf32_Sym *dst, unsigned char *src, size_t cnt)
{
	Elf32_Sym	*end = dst;

	dst += cnt;
	src += cnt * ST1_sizeof;
	while (dst-- > end) {
		src -= ST1_sizeof;
		dst->st_shndx = (((Elf32_Half)(src)[ST1_shndx_M1]<<8)+(src)[ST1_shndx_M0]);
		dst->st_other = ((unsigned char)(src)[ST1_other_M]);
		dst->st_info = ((unsigned char)(src)[ST1_info_M]);
		dst->st_size = (((((((Elf32_Word)(src)[ST1_size_M3]<<8)
		+(src)[ST1_size_M2])<<8)
		+(src)[ST1_size_M1])<<8)
		+(src)[ST1_size_M0]);
		dst->st_value = (((((((Elf32_Addr)(src)[ST1_value_M3]<<8)
		+(src)[ST1_value_M2])<<8)
		+(src)[ST1_value_M1])<<8)
		+(src)[ST1_value_M0]);
		dst->st_name = (((((((Elf32_Word)(src)[ST1_name_M3]<<8)
		+(src)[ST1_name_M2])<<8)
		+(src)[ST1_name_M1])<<8)
		+(src)[ST1_name_M0]);
	}
}





static void
word_2L_tom(Elf32_Word *dst, unsigned char *src, size_t cnt)
{
	Elf32_Word	*end = dst;

	dst += cnt;
	src += cnt * W_sizeof;
	while (dst-- > end) {
		src -= W_sizeof;
		*dst = (((((((Elf32_Word)(src)[W_L3]<<8)
		+(src)[W_L2])<<8)
		+(src)[W_L1])<<8)
		+(src)[W_L0]);
	}
}

static void
word_2M_tom(Elf32_Word *dst, unsigned char *src, size_t cnt)
{
	Elf32_Word	*end = dst;

	dst += cnt;
	src += cnt * W_sizeof;
	while (dst-- > end) {
		src -= W_sizeof;
		*dst = (((((((Elf32_Word)(src)[W_M3]<<8)
		+(src)[W_M2])<<8)
		+(src)[W_M1])<<8)
		+(src)[W_M0]);
	}
}





static void
verdef_2L11_tom(Elf32_Verdef *dst, unsigned char *src, size_t cnt)
{
	/* LINTED */
	Elf32_Verdef	*end = (Elf32_Verdef *)((char *)dst + cnt);

	while (dst < end) {
		Elf32_Verdaux	*vaux;
		unsigned char	*src_vaux;
		Elf32_Half	i;

		dst->vd_version = (((Elf32_Half)(src)[VD1_version_L1]<<8)+(src)[VD1_version_L0]);
		dst->vd_flags = (((Elf32_Half)(src)[VD1_flags_L1]<<8)+(src)[VD1_flags_L0]);
		dst->vd_ndx = (((Elf32_Half)(src)[VD1_ndx_L1]<<8)+(src)[VD1_ndx_L0]);
		dst->vd_cnt = (((Elf32_Half)(src)[VD1_cnt_L1]<<8)+(src)[VD1_cnt_L0]);
		dst->vd_hash = (((((((Elf32_Word)(src)[VD1_hash_L3]<<8)
		+(src)[VD1_hash_L2])<<8)
		+(src)[VD1_hash_L1])<<8)
		+(src)[VD1_hash_L0]);
		dst->vd_aux = (((((((Elf32_Word)(src)[VD1_aux_L3]<<8)
		+(src)[VD1_aux_L2])<<8)
		+(src)[VD1_aux_L1])<<8)
		+(src)[VD1_aux_L0]);
		dst->vd_next = (((((((Elf32_Word)(src)[VD1_next_L3]<<8)
		+(src)[VD1_next_L2])<<8)
		+(src)[VD1_next_L1])<<8)
		+(src)[VD1_next_L0]);

		src_vaux = src + dst->vd_aux;
		/* LINTED */
		vaux = (Elf32_Verdaux*)((char *)dst + dst->vd_aux);
		for (i = 0; i < dst->vd_cnt; i++) {
			vaux->vda_name = (((((((Elf32_Addr)(src_vaux)[VDA1_name_L3]<<8)
		+(src_vaux)[VDA1_name_L2])<<8)
		+(src_vaux)[VDA1_name_L1])<<8)
		+(src_vaux)[VDA1_name_L0]);
			vaux->vda_next = (((((((Elf32_Addr)(src_vaux)[VDA1_next_L3]<<8)
		+(src_vaux)[VDA1_next_L2])<<8)
		+(src_vaux)[VDA1_next_L1])<<8)
		+(src_vaux)[VDA1_next_L0]);
			src_vaux += vaux->vda_next;
			/* LINTED */
			vaux = (Elf32_Verdaux *)((char *)vaux +
			    vaux->vda_next);
		}
		src += dst->vd_next;
		/* LINTED */
		dst = (Elf32_Verdef *)(dst->vd_next ?
		    (char *)dst + dst->vd_next : (char *)end);
	}
}

static void
verdef_2M11_tom(Elf32_Verdef *dst, unsigned char *src, size_t cnt)
{
	/* LINTED */
	Elf32_Verdef	*end = (Elf32_Verdef *)((char *)dst + cnt);

	while (dst < end) {
		Elf32_Verdaux	*vaux;
		unsigned char	*src_vaux;
		Elf32_Half	i;

		dst->vd_version = (((Elf32_Half)(src)[VD1_version_M1]<<8)+(src)[VD1_version_M0]);
		dst->vd_flags = (((Elf32_Half)(src)[VD1_flags_M1]<<8)+(src)[VD1_flags_M0]);
		dst->vd_ndx = (((Elf32_Half)(src)[VD1_ndx_M1]<<8)+(src)[VD1_ndx_M0]);
		dst->vd_cnt = (((Elf32_Half)(src)[VD1_cnt_M1]<<8)+(src)[VD1_cnt_M0]);
		dst->vd_hash = (((((((Elf32_Word)(src)[VD1_hash_M3]<<8)
		+(src)[VD1_hash_M2])<<8)
		+(src)[VD1_hash_M1])<<8)
		+(src)[VD1_hash_M0]);
		dst->vd_aux = (((((((Elf32_Word)(src)[VD1_aux_M3]<<8)
		+(src)[VD1_aux_M2])<<8)
		+(src)[VD1_aux_M1])<<8)
		+(src)[VD1_aux_M0]);
		dst->vd_next = (((((((Elf32_Word)(src)[VD1_next_M3]<<8)
		+(src)[VD1_next_M2])<<8)
		+(src)[VD1_next_M1])<<8)
		+(src)[VD1_next_M0]);

		src_vaux = src + dst->vd_aux;
		/* LINTED */
		vaux = (Elf32_Verdaux*)((char *)dst + dst->vd_aux);
		for (i = 0; i < dst->vd_cnt; i++) {
			vaux->vda_name = (((((((Elf32_Addr)(src_vaux)[VDA1_name_M3]<<8)
		+(src_vaux)[VDA1_name_M2])<<8)
		+(src_vaux)[VDA1_name_M1])<<8)
		+(src_vaux)[VDA1_name_M0]);
			vaux->vda_next = (((((((Elf32_Addr)(src_vaux)[VDA1_next_M3]<<8)
		+(src_vaux)[VDA1_next_M2])<<8)
		+(src_vaux)[VDA1_next_M1])<<8)
		+(src_vaux)[VDA1_next_M0]);
			src_vaux += vaux->vda_next;
			/* LINTED */
			vaux = (Elf32_Verdaux *)((char *)vaux +
			    vaux->vda_next);
		}
		src += dst->vd_next;
		/* LINTED */
		dst = (Elf32_Verdef *)(dst->vd_next ?
		    (char *)dst + dst->vd_next : (char *)end);
	}
}





static void
verneed_2L11_tom(Elf32_Verneed *dst, unsigned char *src, size_t cnt)
{
	/* LINTED */
	Elf32_Verneed	*end = (Elf32_Verneed *)((char *)dst + cnt);

	while (dst < end) {
		Elf32_Vernaux *	vaux;
		unsigned char *	src_vaux;
		Elf32_Half	i;
		dst->vn_version = (((Elf32_Half)(src)[VN1_version_L1]<<8)+(src)[VN1_version_L0]);
		dst->vn_cnt = (((Elf32_Half)(src)[VN1_cnt_L1]<<8)+(src)[VN1_cnt_L0]);
		dst->vn_file = (((((((Elf32_Addr)(src)[VN1_file_L3]<<8)
		+(src)[VN1_file_L2])<<8)
		+(src)[VN1_file_L1])<<8)
		+(src)[VN1_file_L0]);
		dst->vn_aux = (((((((Elf32_Word)(src)[VN1_aux_L3]<<8)
		+(src)[VN1_aux_L2])<<8)
		+(src)[VN1_aux_L1])<<8)
		+(src)[VN1_aux_L0]);
		dst->vn_next = (((((((Elf32_Word)(src)[VN1_next_L3]<<8)
		+(src)[VN1_next_L2])<<8)
		+(src)[VN1_next_L1])<<8)
		+(src)[VN1_next_L0]);

		src_vaux = src + dst->vn_aux;
		/* LINTED */
		vaux = (Elf32_Vernaux *)((char *)dst + dst->vn_aux);
		for (i = 0; i < dst->vn_cnt; i++) {
			vaux->vna_hash = (((((((Elf32_Word)(src_vaux)[VNA1_hash_L3]<<8)
		+(src_vaux)[VNA1_hash_L2])<<8)
		+(src_vaux)[VNA1_hash_L1])<<8)
		+(src_vaux)[VNA1_hash_L0]);
			vaux->vna_flags = (((Elf32_Half)(src_vaux)[VNA1_flags_L1]<<8)+(src_vaux)[VNA1_flags_L0]);
			vaux->vna_other = (((Elf32_Half)(src_vaux)[VNA1_other_L1]<<8)+(src_vaux)[VNA1_other_L0]);
			vaux->vna_name = (((((((Elf32_Addr)(src_vaux)[VNA1_name_L3]<<8)
		+(src_vaux)[VNA1_name_L2])<<8)
		+(src_vaux)[VNA1_name_L1])<<8)
		+(src_vaux)[VNA1_name_L0]);
			vaux->vna_next = (((((((Elf32_Word)(src_vaux)[VNA1_next_L3]<<8)
		+(src_vaux)[VNA1_next_L2])<<8)
		+(src_vaux)[VNA1_next_L1])<<8)
		+(src_vaux)[VNA1_next_L0]);
			src_vaux += vaux->vna_next;
			/* LINTED */
			vaux = (Elf32_Vernaux *)((char *)vaux +
			    vaux->vna_next);
		}
		src += dst->vn_next;
		/* LINTED */
		dst = (Elf32_Verneed *)(dst->vn_next ?
		    (char *)dst + dst->vn_next : (char *)end);
	}
}

static void
verneed_2M11_tom(Elf32_Verneed *dst, unsigned char *src, size_t cnt)
{
	/* LINTED */
	Elf32_Verneed	*end = (Elf32_Verneed *)((char *)dst + cnt);

	while (dst < end) {
		Elf32_Vernaux *	vaux;
		unsigned char *	src_vaux;
		Elf32_Half	i;
		dst->vn_version = (((Elf32_Half)(src)[VN1_version_M1]<<8)+(src)[VN1_version_M0]);
		dst->vn_cnt = (((Elf32_Half)(src)[VN1_cnt_M1]<<8)+(src)[VN1_cnt_M0]);
		dst->vn_file = (((((((Elf32_Addr)(src)[VN1_file_M3]<<8)
		+(src)[VN1_file_M2])<<8)
		+(src)[VN1_file_M1])<<8)
		+(src)[VN1_file_M0]);
		dst->vn_aux = (((((((Elf32_Word)(src)[VN1_aux_M3]<<8)
		+(src)[VN1_aux_M2])<<8)
		+(src)[VN1_aux_M1])<<8)
		+(src)[VN1_aux_M0]);
		dst->vn_next = (((((((Elf32_Word)(src)[VN1_next_M3]<<8)
		+(src)[VN1_next_M2])<<8)
		+(src)[VN1_next_M1])<<8)
		+(src)[VN1_next_M0]);

		src_vaux = src + dst->vn_aux;
		/* LINTED */
		vaux = (Elf32_Vernaux *)((char *)dst + dst->vn_aux);
		for (i = 0; i < dst->vn_cnt; i++) {
			vaux->vna_hash = (((((((Elf32_Word)(src_vaux)[VNA1_hash_M3]<<8)
		+(src_vaux)[VNA1_hash_M2])<<8)
		+(src_vaux)[VNA1_hash_M1])<<8)
		+(src_vaux)[VNA1_hash_M0]);
			vaux->vna_flags = (((Elf32_Half)(src_vaux)[VNA1_flags_M1]<<8)+(src_vaux)[VNA1_flags_M0]);
			vaux->vna_other = (((Elf32_Half)(src_vaux)[VNA1_other_M1]<<8)+(src_vaux)[VNA1_other_M0]);
			vaux->vna_name = (((((((Elf32_Addr)(src_vaux)[VNA1_name_M3]<<8)
		+(src_vaux)[VNA1_name_M2])<<8)
		+(src_vaux)[VNA1_name_M1])<<8)
		+(src_vaux)[VNA1_name_M0]);
			vaux->vna_next = (((((((Elf32_Word)(src_vaux)[VNA1_next_M3]<<8)
		+(src_vaux)[VNA1_next_M2])<<8)
		+(src_vaux)[VNA1_next_M1])<<8)
		+(src_vaux)[VNA1_next_M0]);
			src_vaux += vaux->vna_next;
			/* LINTED */
			vaux = (Elf32_Vernaux *)((char *)vaux +
			    vaux->vna_next);
		}
		src += dst->vn_next;
		/* LINTED */
		dst = (Elf32_Verneed *)(dst->vn_next ?
		    (char *)dst + dst->vn_next : (char *)end);
	}
}
