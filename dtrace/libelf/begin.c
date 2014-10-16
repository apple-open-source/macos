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

/*
 *	Copyright (c) 1988 AT&T
 *	All Rights Reserved
 */

#pragma ident	"@(#)begin.c	1.18	08/05/31 SMI"

#include <ar.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <libelf.h>
#include <sys/mman.h>
#include "decl.h"
#include "member.h"
#include "msg.h"

static const char	armag[] = ARMAG;

#include <crt_externs.h>
#include <mach/mach.h>
#include <mach-o/loader.h>
#include <mach-o/dyld.h>
#include <mach-o/fat.h>
#include <sys/sysctl.h>

void
__swap_mach_header(struct mach_header* header)
{
	SWAP32(header->magic);
	SWAP32(header->cputype);
	SWAP32(header->cpusubtype);
	SWAP32(header->filetype);
	SWAP32(header->ncmds);
	SWAP32(header->sizeofcmds);
	SWAP32(header->flags);
}

void
__swap_mach_header_64(struct mach_header_64* header)
{
	SWAP32(header->magic);
	SWAP32(header->cputype);
	SWAP32(header->cpusubtype);
	SWAP32(header->filetype);
	SWAP32(header->ncmds);
	SWAP32(header->sizeofcmds);
	SWAP32(header->flags);
}

void
__swap_segment_command(struct segment_command* segment)
{
	SWAP32(segment->cmd);
	SWAP32(segment->cmdsize);
	SWAP32(segment->vmaddr);
	SWAP32(segment->vmsize);
	SWAP32(segment->fileoff);
	SWAP32(segment->filesize);
	SWAP32(segment->maxprot);
	SWAP32(segment->initprot);
	SWAP32(segment->nsects);
	SWAP32(segment->flags);
}

void
__swap_segment_command_64(struct segment_command_64* segment)
{
	SWAP32(segment->cmd);
	SWAP32(segment->cmdsize);
	SWAP64(segment->vmaddr);
	SWAP64(segment->vmsize);
	SWAP64(segment->fileoff);
	SWAP64(segment->filesize);
	SWAP32(segment->maxprot);
	SWAP32(segment->initprot);
	SWAP32(segment->nsects);
	SWAP32(segment->flags);
}

void 
__swap_section(struct section* section_ptr)
{
	SWAP32(section_ptr->addr);
	SWAP32(section_ptr->size);
	SWAP32(section_ptr->offset);
	SWAP32(section_ptr->align);
	SWAP32(section_ptr->reloff);
	SWAP32(section_ptr->nreloc);
	SWAP32(section_ptr->flags);
	SWAP32(section_ptr->reserved1);
	SWAP32(section_ptr->reserved2);
}

void 
__swap_section_64(struct section_64* section_ptr)
{
	SWAP64(section_ptr->addr);
	SWAP64(section_ptr->size);
	SWAP32(section_ptr->offset);
	SWAP32(section_ptr->align);
	SWAP32(section_ptr->reloff);
	SWAP32(section_ptr->nreloc);
	SWAP32(section_ptr->flags);
	SWAP32(section_ptr->reserved1);
	SWAP32(section_ptr->reserved2);
}

void __swap_symtab_command(struct symtab_command *symtab)
{
	SWAP32(symtab->cmd);
	SWAP32(symtab->cmdsize);
	SWAP32(symtab->symoff);
	SWAP32(symtab->nsyms);
	SWAP32(symtab->stroff);
	SWAP32(symtab->strsize);
}

static cpu_type_t current_program_arch(void)
{
        cpu_type_t current_arch = (_NSGetMachExecuteHeader())->cputype;
        return current_arch;
}

static cpu_type_t current_kernel_arch(void)
{
        struct host_basic_info  hi;
        unsigned int            size;
        kern_return_t           kret;
        cpu_type_t                                current_arch;
        int                                                ret, mib[4];
        size_t                                        len;
        struct kinfo_proc                kp;
        
        size = sizeof(hi)/sizeof(int);
        kret = host_info(mach_host_self(), HOST_BASIC_INFO, (host_info_t)&hi, &size);
        if (kret != KERN_SUCCESS) {
                return 0;
        }
        current_arch = hi.cpu_type;
        /* Now determine if the kernel is running in 64-bit mode */
        mib[0] = CTL_KERN;
        mib[1] = KERN_PROC;
        mib[2] = KERN_PROC_PID;
        mib[3] = 0; /* kernproc, pid 0 */
        len = sizeof(kp);
        ret = sysctl(mib, sizeof(mib)/sizeof(mib[0]), &kp, &len, NULL, 0);
        if (ret == -1) {
                return 0;
        }
        if (kp.kp_proc.p_flag & P_LP64) {
                current_arch |= CPU_ARCH_ABI64;
        }
        return current_arch;
}

/*
 * Initialize archive member
 */
Elf *
_elf_member(int fd, Elf * ref, unsigned flags)
{
	register Elf	*elf;
	Member		*mh;
	size_t		base;

	if (ref->ed_nextoff >= ref->ed_fsz)
		return (0);
	if (ref->ed_fd == -1)		/* disabled */
		fd = -1;
	if (flags & EDF_WRITE) {
		_elf_seterr(EREQ_ARRDWR, 0);
		return (0);
	}
	if (ref->ed_fd != fd) {
		_elf_seterr(EREQ_ARMEMFD, 0);
		return (0);
	}
	if ((_elf_vm(ref, ref->ed_nextoff, sizeof (struct ar_hdr)) !=
	    OK_YES) || ((mh = _elf_armem(ref,
	    ref->ed_ident + ref->ed_nextoff, ref->ed_fsz)) == 0))
		return (0);

	base = ref->ed_nextoff + sizeof (struct ar_hdr);
	if (ref->ed_fsz - base < mh->m_hdr.ar_size) {
		_elf_seterr(EFMT_ARMEMSZ, 0);
		return (0);
	}
	if ((elf = (Elf *)calloc(1, sizeof (Elf))) == 0) {
		_elf_seterr(EMEM_ELF, errno);
		return (0);
	}
	++ref->ed_activ;
	elf->ed_parent = ref;
	elf->ed_fd = fd;
	elf->ed_myflags |= flags;
	elf->ed_armem = mh;
	elf->ed_fsz = mh->m_hdr.ar_size;
	elf->ed_baseoff = ref->ed_baseoff + base;
	elf->ed_memoff = base - mh->m_slide;
	elf->ed_siboff = base + elf->ed_fsz + (elf->ed_fsz & 1);
	ref->ed_nextoff = elf->ed_siboff;
	elf->ed_image = ref->ed_image;
	elf->ed_imagesz = ref->ed_imagesz;
	elf->ed_vm = ref->ed_vm;
	elf->ed_vmsz = ref->ed_vmsz;
	elf->ed_ident = ref->ed_ident + base - mh->m_slide;

	/*
	 * If this member is the archive string table,
	 * we've already altered the bytes.
	 */

	if (ref->ed_arstroff == ref->ed_nextoff)
		elf->ed_status = ES_COOKED;
	return (elf);
}


Elf *
_elf_regular(int fd, unsigned flags)		/* initialize regular file */
{
	Elf		*elf;

	if ((elf = (Elf *)calloc(1, sizeof (Elf))) == 0) {
		_elf_seterr(EMEM_ELF, errno);
		return (0);
	}

	NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(*elf))
	elf->ed_fd = fd;
	elf->ed_myflags |= flags;
	if (_elf_inmap(elf) != OK_YES) {
		free(elf);
		return (0);
	}
	NOTE(NOW_VISIBLE_TO_OTHER_THREADS(*elf))
	return (elf);
}


Elf *
_elf_config(Elf * elf)
{
	char *		base;
	unsigned	encode;

	ELFRWLOCKINIT(&elf->ed_rwlock);

	/*
	 * Determine if this is a ELF file.
	 */
	base = elf->ed_ident;
	if ((elf->ed_fsz >= EI_NIDENT) &&
	    (_elf_vm(elf, (size_t)0, (size_t)EI_NIDENT) == OK_YES) &&
	    (base[EI_MAG0] == ELFMAG0) &&
	    (base[EI_MAG1] == ELFMAG1) &&
	    (base[EI_MAG2] == ELFMAG2) &&
	    (base[EI_MAG3] == ELFMAG3)) {
		elf->ed_kind = ELF_K_ELF;
		elf->ed_class = base[EI_CLASS];
		elf->ed_encode = base[EI_DATA];
		if ((elf->ed_version = base[EI_VERSION]) == 0)
			elf->ed_version = 1;
		elf->ed_identsz = EI_NIDENT;

		/*
		 * Allow writing only if originally specified read only.
		 * This is only necessary if the file must be translating
		 * from one encoding to another.
		 */
		ELFACCESSDATA(encode, _elf_encode)
		if ((elf->ed_vm == 0) && ((elf->ed_myflags & EDF_WRITE) == 0) &&
		    (elf->ed_encode != encode)) {
			if (mprotect((char *)elf->ed_image, elf->ed_imagesz,
			    PROT_READ|PROT_WRITE) == -1) {
				_elf_seterr(EIO_VM, errno);
				return (0);
			}
		}
		return (elf);
	}

	/*
	 * Determine if this is a Mach-o file.
	 */
	if ((elf->ed_fsz >= sizeof(struct fat_header)) &&
	    (_elf_vm(elf, (size_t)0, (size_t)sizeof(struct fat_header)) == OK_YES) &&
	    (FAT_MAGIC == *(unsigned int *)(elf->ed_ident) || 
		 FAT_CIGAM == *(unsigned int *)(elf->ed_ident))) 
	{
		struct fat_header *fat_header = (struct fat_header *)(elf->ed_ident);
		int nfat_arch = OSSwapBigToHostInt32(fat_header->nfat_arch);
		int end_of_archs = sizeof(struct fat_header) + nfat_arch * sizeof(struct fat_arch);
		struct fat_arch *arch = (struct fat_arch *)(elf->ed_ident + sizeof(struct fat_header));
		
		cpu_type_t cputype = (elf->ed_myflags & EDF_RDKERNTYPE) ? current_kernel_arch() :current_program_arch();
			
		if (end_of_archs > elf->ed_fsz) {
			_elf_seterr(EIO_VM, errno);
			return 0;
		}
			
		for (; nfat_arch-- > 0; arch++) {
			if(((cpu_type_t)OSSwapBigToHostInt32(arch->cputype)) == cputype) {
				elf->ed_ident += OSSwapBigToHostInt32(arch->offset);
				elf->ed_image += OSSwapBigToHostInt32(arch->offset);
				elf->ed_fsz -= OSSwapBigToHostInt32(arch->offset);
				elf->ed_imagesz -= OSSwapBigToHostInt32(arch->offset);
				break;
			}
		}
		/* Fall through positioned at mach_header for "thin" architecture matching host endian-ness */
	}
	
	if ((elf->ed_fsz >= sizeof(struct mach_header)) &&
	    (_elf_vm(elf, (size_t)0, (size_t)sizeof(struct mach_header)) == OK_YES) &&
	    (MH_MAGIC == *(unsigned int *)(elf->ed_image) || 
		 MH_CIGAM == *(unsigned int *)(elf->ed_image))) {
		 
		struct mach_header hdr, *mh = (struct mach_header *)elf->ed_image;
		struct load_command *thisLC = (struct load_command *)(&(mh[1]));
		int i, n = 0;
		int needSwap = (MH_CIGAM == mh->magic);
		
		if (needSwap) {
			hdr = *mh;
			mh = &hdr;
			__swap_mach_header(mh);
		}
			
		for (i = 0; i < mh->ncmds; i++) {
			int cmd = thisLC->cmd, cmdsize = thisLC->cmdsize;
			
			if (needSwap) {
				SWAP32(cmd);
				SWAP32(cmdsize);
			}
				
			switch(cmd) {
				case LC_SEGMENT:
				{
					struct segment_command seg, *thisSG = (struct segment_command *)thisLC;
					
					if (needSwap) {
						seg = *thisSG;
						thisSG = &seg;
						__swap_segment_command(thisSG);
					}
					
					n += thisSG->nsects;
					break;
				}
					
				case LC_SYMTAB:
					n += 2;
					break;
					
				default:
					break;
			}
			thisLC = (struct load_command *) ((caddr_t) thisLC + cmdsize);
		}
		
		if (0 == (elf->ed_ident = malloc(sizeof(Elf32_Ehdr)))) {
			_elf_seterr(EMEM_ELF, errno);
			return (0);
		}
		
		((Elf32_Ehdr *)(elf->ed_ident))->e_ident[EI_MAG0] = 'M';
		((Elf32_Ehdr *)(elf->ed_ident))->e_ident[EI_MAG1] = 'a';
		((Elf32_Ehdr *)(elf->ed_ident))->e_ident[EI_MAG2] = 'c';
		((Elf32_Ehdr *)(elf->ed_ident))->e_ident[EI_MAG3] = 'h';
		((Elf32_Ehdr *)(elf->ed_ident))->e_ident[EI_CLASS] = ELFCLASS32;
#if defined(__BIG_ENDIAN__)
		((Elf32_Ehdr *)(elf->ed_ident))->e_ident[EI_DATA] = (needSwap ? ELFDATA2LSB : ELFDATA2MSB);
#else
		((Elf32_Ehdr *)(elf->ed_ident))->e_ident[EI_DATA] = (needSwap ? ELFDATA2MSB : ELFDATA2LSB);
#endif
		((Elf32_Ehdr *)(elf->ed_ident))->e_ident[EI_VERSION] = EV_CURRENT;
		((Elf32_Ehdr *)(elf->ed_ident))->e_ident[EI_OSABI] = ELFOSABI_NONE;
		((Elf32_Ehdr *)(elf->ed_ident))->e_ident[EI_ABIVERSION] = 0;
		((Elf32_Ehdr *)(elf->ed_ident))->e_type = ET_NONE;
		((Elf32_Ehdr *)(elf->ed_ident))->e_machine = EM_NONE;
		((Elf32_Ehdr *)(elf->ed_ident))->e_version = EV_CURRENT;
		((Elf32_Ehdr *)(elf->ed_ident))->e_phoff = 0;
		((Elf32_Ehdr *)(elf->ed_ident))->e_shoff = sizeof(struct mach_header);
		((Elf32_Ehdr *)(elf->ed_ident))->e_ehsize = sizeof(Elf32_Ehdr);
		((Elf32_Ehdr *)(elf->ed_ident))->e_phentsize = sizeof(Elf32_Phdr);
		((Elf32_Ehdr *)(elf->ed_ident))->e_phnum = 0;
		((Elf32_Ehdr *)(elf->ed_ident))->e_shentsize = sizeof(Elf32_Shdr);
		((Elf32_Ehdr *)(elf->ed_ident))->e_shnum = n + 1;
		((Elf32_Ehdr *)(elf->ed_ident))->e_shstrndx = SHN_MACHO;

		elf->ed_kind = ELF_K_MACHO;
		elf->ed_class = ((Elf32_Ehdr *)(elf->ed_ident))->e_ident[EI_CLASS];
#if defined(__BIG_ENDIAN__)
		elf->ed_encode = ELFDATA2MSB;
#else
		elf->ed_encode = ELFDATA2LSB;
#endif
		elf->ed_version = ((Elf32_Ehdr *)(elf->ed_ident))->e_ident[EI_VERSION];
		elf->ed_identsz = EI_NIDENT;

		/*
		 * Allow writing only if originally specified read only.
		 * This is only necessary if the file must be translating
		 * from one encoding to another.
		 */
 		ELFACCESSDATA(encode, _elf_encode)
		if ((elf->ed_vm == 0) && ((elf->ed_myflags & EDF_WRITE) == 0) &&
		    (elf->ed_encode != encode)) {
			if (mprotect((char *)elf->ed_image, elf->ed_imagesz,
			    PROT_READ|PROT_WRITE) == -1) {
				_elf_seterr(EIO_VM, errno);
				return (0);
			}
		}
		return (elf);
	}
	
	if ((elf->ed_fsz >= sizeof(struct mach_header_64)) &&
	    (_elf_vm(elf, (size_t)0, (size_t)sizeof(struct mach_header_64)) == OK_YES) &&
	    (MH_MAGIC_64 == *(unsigned int *)(elf->ed_image) || 
		 MH_CIGAM_64 == *(unsigned int *)(elf->ed_image))) {
		 
		struct mach_header_64 hdr, *mh64 = (struct mach_header_64 *)elf->ed_image;
		struct load_command *thisLC = (struct load_command *)(&(mh64[1]));
		int i, n = 0;
		int needSwap = (MH_CIGAM_64 == mh64->magic);
		
		if (needSwap) {
			hdr = *mh64;
			mh64 = &hdr;
			__swap_mach_header_64(mh64);
		}
			
		for (i = 0; i < mh64->ncmds; i++) {
			int cmd = thisLC->cmd, cmdsize = thisLC->cmdsize;
			
			if (needSwap) {
				SWAP32(cmd);
				SWAP32(cmdsize);
			}
				
			switch(cmd) {
				case LC_SEGMENT_64:
				{
					struct segment_command_64 seg, *thisSG64 = (struct segment_command_64 *)thisLC;

					if (needSwap) {
						seg = *thisSG64;
						thisSG64 = &seg;
						__swap_segment_command_64(thisSG64);
					}

					n += thisSG64->nsects;
					break;
				}
					
				case LC_SYMTAB:
					n += 2;
					break;
					
				default:
					break;
			}
			thisLC = (struct load_command *) ((caddr_t) thisLC + cmdsize);
		}
		
		if (0 == (elf->ed_ident = malloc(sizeof(Elf64_Ehdr)))) {
			_elf_seterr(EMEM_ELF, errno);
			return (0);
		}
		
		((Elf64_Ehdr *)(elf->ed_ident))->e_ident[EI_MAG0] = 'M';
		((Elf64_Ehdr *)(elf->ed_ident))->e_ident[EI_MAG1] = 'a';
		((Elf64_Ehdr *)(elf->ed_ident))->e_ident[EI_MAG2] = 'c';
		((Elf64_Ehdr *)(elf->ed_ident))->e_ident[EI_MAG3] = 'h';
		((Elf64_Ehdr *)(elf->ed_ident))->e_ident[EI_CLASS] = ELFCLASS64;
#if defined(__BIG_ENDIAN__)
		((Elf64_Ehdr *)(elf->ed_ident))->e_ident[EI_DATA] = (needSwap ? ELFDATA2LSB : ELFDATA2MSB);
#else
		((Elf64_Ehdr *)(elf->ed_ident))->e_ident[EI_DATA] = (needSwap ? ELFDATA2MSB : ELFDATA2LSB);
#endif
		((Elf64_Ehdr *)(elf->ed_ident))->e_ident[EI_VERSION] = EV_CURRENT;
		((Elf64_Ehdr *)(elf->ed_ident))->e_ident[EI_OSABI] = ELFOSABI_NONE;
		((Elf64_Ehdr *)(elf->ed_ident))->e_ident[EI_ABIVERSION] = 0;
		((Elf64_Ehdr *)(elf->ed_ident))->e_type = ET_NONE;
		((Elf64_Ehdr *)(elf->ed_ident))->e_machine = EM_NONE;
		((Elf64_Ehdr *)(elf->ed_ident))->e_version = EV_CURRENT;
		((Elf64_Ehdr *)(elf->ed_ident))->e_phoff = 0;
		((Elf64_Ehdr *)(elf->ed_ident))->e_shoff = sizeof(struct mach_header_64);
		((Elf64_Ehdr *)(elf->ed_ident))->e_ehsize = sizeof(Elf64_Ehdr);
		((Elf64_Ehdr *)(elf->ed_ident))->e_phentsize = sizeof(Elf64_Phdr);
		((Elf64_Ehdr *)(elf->ed_ident))->e_phnum = 0;
		((Elf64_Ehdr *)(elf->ed_ident))->e_shentsize = sizeof(Elf64_Shdr);
		((Elf64_Ehdr *)(elf->ed_ident))->e_shnum = n + 1;
		((Elf64_Ehdr *)(elf->ed_ident))->e_shstrndx = SHN_MACHO_64;

		elf->ed_kind = ELF_K_MACHO;
		elf->ed_class = ((Elf64_Ehdr *)(elf->ed_ident))->e_ident[EI_CLASS];
#if defined(__BIG_ENDIAN__)
		elf->ed_encode = ELFDATA2MSB;
#else
		elf->ed_encode = ELFDATA2LSB;
#endif
		elf->ed_version = ((Elf64_Ehdr *)(elf->ed_ident))->e_ident[EI_VERSION];
		elf->ed_identsz = EI_NIDENT;

		/*
		 * Allow writing only if originally specified read only.
		 * This is only necessary if the file must be translating
		 * from one encoding to another.
		 */
		ELFACCESSDATA(encode, _elf_encode)
		if ((elf->ed_vm == 0) && ((elf->ed_myflags & EDF_WRITE) == 0) &&
		    (elf->ed_encode != encode)) {
			if (mprotect((char *)elf->ed_image, elf->ed_imagesz,
			    PROT_READ|PROT_WRITE) == -1) {
				_elf_seterr(EIO_VM, errno);
				return (0);
			}
		}
		return (elf);
	}

	/*
	 * Determine if this is an Archive
	 */
	if ((elf->ed_fsz >= SARMAG) &&
	    (_elf_vm(elf, (size_t)0, (size_t)SARMAG) == OK_YES) &&
	    (memcmp(base, armag, SARMAG) == 0)) {
		_elf_arinit(elf);
		elf->ed_kind = ELF_K_AR;
		elf->ed_identsz = SARMAG;
		return (elf);
	}

	/*
	 *	Return a few ident bytes, but not so many that
	 *	getident() must read a large file.  512 is arbitrary.
	 */

	elf->ed_kind = ELF_K_NONE;
	if ((elf->ed_identsz = elf->ed_fsz) > 512)
		elf->ed_identsz = 512;

	return (elf);
}

Elf *
elf_memory(char * image, size_t sz)
{
	Elf		*elf;
	unsigned	work;

	/*
	 * version() no called yet?
	 */
	ELFACCESSDATA(work, _elf_work)
	if (work == EV_NONE) {
		_elf_seterr(ESEQ_VER, 0);
		return (0);
	}

	if ((elf = (Elf *)calloc(1, sizeof (Elf))) == 0) {
		_elf_seterr(EMEM_ELF, errno);
		return (0);
	}
	NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(*elf))
	elf->ed_fd = -1;
	elf->ed_myflags |= EDF_READ | EDF_MEMORY;
	elf->ed_image = elf->ed_ident = image;
	elf->ed_imagesz = elf->ed_fsz = elf->ed_identsz = sz;
	elf->ed_kind = ELF_K_ELF;
	elf->ed_class = image[EI_CLASS];
	elf->ed_encode = image[EI_DATA];
	if ((elf->ed_version = image[EI_VERSION]) == 0)
		elf->ed_version = 1;
	elf->ed_identsz = EI_NIDENT;
	elf->ed_activ = 1;
	elf = _elf_config(elf);
	NOTE(NOW_VISIBLE_TO_OTHER_THREADS(*elf))
	return (elf);
}

/*
 * The following is a private interface between the linkers (ld & ld.so.1)
 * and libelf.
 *
 * elf_begin(0, ELF_C_IMAGE, ref)
 *	Return a new elf_descriptor which uses the memory image from
 *	ref as the base image of the elf file.  Before this elf_begin()
 *	is called an elf_update(ref, ELF_C_WRIMAGE) must have been
 *	done to the ref elf descriptor.
 *	The ELF_C_IMAGE is unique in that modificatino of the Elf structure
 *	is illegal (no elf_new*()) but you can modify the actual
 *	data image of the file in question.
 *
 *	When you are done processing this file you can then perform a
 *	elf_end() on it.
 *
 *	NOTE: if an elf_update(ref, ELF_C_WRITE) is done on the ref Elf
 *		descriptor then the memory image that the ELF_C_IMAGE
 *		is using has been discarded.  The proper calling convention
 *		for this is as follows:
 *
 *	elf1 = elf_begin(fd, ELF_C_WRITE, 0);
 *	...
 *	elf_update(elf1, ELF_C_WRIMAGE);	 build memory image
 *	elf2 = elf_begin(0, ELF_C_IMAGE, elf1);
 *	...
 *	elf_end(elf2);
 *	elf_updage(elf1, ELF_C_WRITE);		flush memory image to disk
 *	elf_end(elf1);
 *
 *
 * elf_begin(0, ELF_C_IMAGE, 0);
 *	returns a pointer to an elf descriptor as if it were opened
 *	with ELF_C_WRITE except that it has no file descriptor and it
 *	will not create a file.  It's to be used with the command:
 *
 *		elf_update(elf, ELF_C_WRIMAGE)
 *
 *	which will build a memory image instead of a file image.
 *	The memory image is allocated via dynamic memory (malloc) and
 *	can be free with a subsequent call to
 *
 *		elf_update(elf, ELF_C_WRITE)
 *
 *	NOTE: that if elf_end(elf) is called it will not free the
 *		memory image if it is still allocated.  It is then
 *		the callers responsiblity to free it via a call
 *		to free().
 *
 *	Here is a potential calling sequence for this interface:
 *
 *	elf1 = elf_begin(0, ELF_C_IMAGE, 0);
 *	...
 *	elf_update(elf1, ELF_C_WRIMAGE);	build memory image
 *	elf2 = elf_begin(0, ELF_C_IMAGE, elf1);
 *	...
 *	image_ptr = elf32_getehdr(elf2);	get pointer to image
 *	elf_end(elf2);
 *	elf_end(elf1);
 *	...
 *	use image
 *	...
 *	free(image_ptr);
 */

Elf *
elf_begin(int fd, Elf_Cmd cmd, Elf *ref)
{
	register Elf	*elf;
	unsigned	work;
	unsigned	flags = 0;

	ELFACCESSDATA(work, _elf_work)
	if (work == EV_NONE)	/* version() not called yet */
	{
		_elf_seterr(ESEQ_VER, 0);
		return (0);
	}
	switch (cmd) {
	default:
		_elf_seterr(EREQ_BEGIN, 0);
		return (0);

	case ELF_C_NULL:
		return (0);

	case ELF_C_IMAGE:
		if (ref) {
			char *	image;
			size_t	imagesz;
			ELFRLOCK(ref);
			if ((image = ref->ed_wrimage) == 0) {
				_elf_seterr(EREQ_NOWRIMAGE, 0);
				ELFUNLOCK(ref);
				return (0);
			}
			imagesz = ref->ed_wrimagesz;
			ELFUNLOCK(ref);
			return (elf_memory(image, imagesz));
		}
		/* FALLTHROUGH */
	case ELF_C_WRITE:
		if ((elf = (Elf *)calloc(1, sizeof (Elf))) == 0) {
			_elf_seterr(EMEM_ELF, errno);
			return (0);
		}
		NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(*elf))
		ELFRWLOCKINIT(&elf->ed_rwlock);
		elf->ed_fd = fd;
		elf->ed_activ = 1;
		elf->ed_myflags |= EDF_WRITE;
		if (cmd == ELF_C_IMAGE)
			elf->ed_myflags |= EDF_WRALLOC;
		NOTE(NOW_VISIBLE_TO_OTHER_THREADS(*elf))
		return (elf);
	case ELF_C_RDWR:
		flags = EDF_WRITE | EDF_READ;
		break;

	case ELF_C_READ:
		flags = EDF_READ;
		break;
		
	case ELF_C_RDKERNTYPE:
		flags = EDF_READ | EDF_RDKERNTYPE;
		break;
	}

	/*
	 *	A null ref asks for a new file
	 *	Non-null ref bumps the activation count
	 *		or gets next archive member
	 */

	if (ref == 0) {
		if ((elf = _elf_regular(fd, flags)) == 0)
			return (0);
	} else {
		ELFWLOCK(ref);
		if ((ref->ed_myflags & flags) != flags) {
			_elf_seterr(EREQ_RDWR, 0);
			ELFUNLOCK(ref);
			return (0);
		}
		/*
		 * new activation ?
		 */
		if (ref->ed_kind != ELF_K_AR) {
			++ref->ed_activ;
			ELFUNLOCK(ref);
			return (ref);
		}
		if ((elf = _elf_member(fd, ref, flags)) == 0) {
			ELFUNLOCK(ref);
			return (0);
		}
		ELFUNLOCK(ref);
	}

	NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(*elf))
	elf->ed_activ = 1;
	elf = _elf_config(elf);
	NOTE(NOW_VISIBLE_TO_OTHER_THREADS(*elf))

	return (elf);
}
