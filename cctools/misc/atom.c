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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <errno.h>
#include <libc.h>
#include <gnu/a.out.h>
#include <gnu/symseg.h>
#include <mach-o/loader.h>
#include <mach/m68k/thread_status.h>
#include "stuff/allocate.h"
#include "stuff/errors.h"
#include "stuff/round.h"

#define OBJC_SYM "__OBJC_SYMBOLS"
#define OBJC_MOD "__OBJC_MODULES"
#define OBJC_SEL "__OBJC_STRINGS"

/*
 * These can't use the pagesize system call because these values are constants
 * in the link editor.  These constants are hold overs from the SUN when both
 * binaries could run on the same system.  This stuff should have been moved
 * into <a.out.h>.
 */
#define N_PAGSIZ(x) 0x2000
#define N_SEGSIZ(x) 0x20000
/*
 * These versions of the macros correct the fact that the text address of
 * OMAGIC files is always 0.  Only the first is wrong but the others use
 * the first.  It should be corrected in a.out.h.
 */
#define NN_TXTADDR(x) \
	(((x).a_magic==OMAGIC)? 0 : N_PAGSIZ(x))
#define NN_DATADDR(x) \
	(((x).a_magic==OMAGIC)? (NN_TXTADDR(x)+(x).a_text) \
	: (N_SEGSIZ(x)+((NN_TXTADDR(x)+(x).a_text-1) & ~(N_SEGSIZ(x)-1))))
#define NN_BSSADDR(x)  (NN_DATADDR(x)+(x).a_data)

char *progname;		/* name of the program for error messages (argv[0]) */

char *aoutfile, *machofile;
int afd, mfd;

/* The a.out header of the object to be converted */
struct exec exec;

/* The structures for mach executables (some used for relocatables too) */
struct mach_header		header;
struct segment_command		pagezero_segment;
struct segment_command		text_segment;	/* __TEXT */
struct section			text_section;	/* __TEXT, __text */
struct segment_command		data_segment;	/* __DATA */
struct section			data_section;	/* __DATA, __data */
struct section			bss_section;	/* __DATA, __bss */
struct section			sym_section;	/* __OBJC, __symbol_table */
struct section			mod_section;	/* __OBJC, __module_info */
struct section			sel_section;	/* __OBJC, __selector_strs */
struct symtab_command		symbol_table;
struct symseg_command		symbol_segment;
struct thread_command		thread;
unsigned long 			flavor;
unsigned long 			count;
struct m68k_thread_state_regs	cpu;
/* segment structure for a mach relocatable */
struct segment_command		reloc_segment;

/* The following are used for creating mach segments from files. */
struct seg_create {
	long sizeofsects;	/* the sum of section sizes */
	struct segment_command sg; /* the segment load command itself */
	struct section_list *slp;  /* list of section structures */
} *seg_create;
long n_seg_create;
long seg_create_size;

struct section_list {
	char *filename;		/* file name for the contents of the section */
	long filesize;		/* the file size as returned by stat(2) */
	struct section s;	/* the section structure */
	struct section_list *next;	/* next section_list */
};

/*
 * The -objc flag to create the sections in the __OBJC segment from parts
 * of the data section.  This has a KNOWN BUG that can only be fixed by using
 * the atom in the assembler.
 *
 * The known bug is that if something is refering to a symbol plus
 * an offset then the atom process can get the relocation wrong (the
 * r_symbolnum is set wrong) if the resuling symbol plus offset is not in
 * the same section as the symbol's value.  This can only be fixed in the
 * assembler by baseing the r_symbolnum on the value of the symbol without
 * the offset added.
 */
int objcflag = 0;

/*
 * The -gg flag passes through the symsegs created with the -gg option to the
 * compiler.  By default this is not done.
 */
int ggflag = 0;

/*
 * The following variables are used when the -objc flag is set.
 */

/*
 * The symbol and string table of the a.out file.  Used to lookup the symbols
 * that start the objective-C sections when the -objc flag is present.
 */
struct nlist *symtab;	/* pointer to the symbol table */
char *strtab;		/* the string table */
long strsize;		/* size of the string table */

/*
 * The data relocation entried which are looked through to divide up into the
 * objective-C section's relocation entries and the data section's.
 */
struct relocation_info *data_reloc;
long data_reloff, data_nreloc;

/*
 * Pointers to the contents of the text and data section which are needed to
 * update the relocation entries to see what address is being referenced by
 * each relocation entry.
 */
char *text, *data;

static struct nlist *lookup(
    char *sym_name);
static void usage(void);

void
main(
int argc,
char **argv,
char **envp)
{
    struct stat statbuf;
    struct segment_command *sgp;
    struct section_list *slp;
    long i, j, k, len, header_size, vmaddr, fileoff, zs_addr, s_offset,
	 page_size;
    char *p;
    int fd;
    struct relocation_info *preloc;
    struct nlist *pnlist;
    struct symbol_root *psymbol_root;
    struct mach_root *pmach_root, mach_root;

    sgp = NULL;
    progname = argv[0];
    page_size = 8192;

    aoutfile = NULL;
    machofile = NULL;

	for(i = 1; i < argc; i++){
	    /*
	     * The -segcreate <segment name> <section name> <file name>
	     * option.  To create a segment from a file.
	     */
	    if(strcmp("-segcreate", argv[i]) == 0){
		if(i + 3 >= argc)
		    fatal("missing arguments to -segcreate <segment name> "
			  "<section name> <file name>");
		if(stat(argv[i+3], &statbuf) == -1)
		    system_fatal("Can't stat file: %s (to create a segment "
				 "with)", argv[i+3]);

	        if(n_seg_create == 0){
		    seg_create = (struct seg_create *)
				  allocate(sizeof (struct seg_create));
		    seg_create[0].slp = (struct section_list *)
					 allocate(sizeof (struct section_list));
		}
		else{
		    /*
		     * First see if this segment already exists.
		     */
		    for(j = 0; j < n_seg_create; j++){
			sgp = &(seg_create[j].sg);
			if(strncmp(sgp->segname, argv[i+1],
				   sizeof(sgp->segname)) == 0){
			    slp = seg_create[j].slp;
			    for(k = 0; k < sgp->nsects; k++){
				if(strncmp(slp->s.sectname, argv[i+2],
					   sizeof(slp->s.sectname)) == 0)
				    fatal("more that one -segcreate option "
					  "with the same segment (%s) and "
					  "section (%s) name", argv[i+1],
					   argv[i+2]);
				slp = slp->next;
			    }
			    break;
			}
		    }
		    /*
		     * If this is just another section in an already existing
		     * segment then just create another section list structure
		     * for it.
		     */
		    if(j != n_seg_create){
			slp = seg_create[j].slp;
			for (k = 0; k < sgp->nsects-1 ; k++)
			    slp = slp->next;
			slp->next = (struct section_list *)
				    allocate(sizeof(struct section_list));
			slp = slp->next;
			bzero((char *)slp, sizeof(struct section_list));
			slp->filename = argv[i+3];
			slp->filesize = statbuf.st_size;
			len = strlen(argv[i+2]);
			if(len > sizeof(slp->s.sectname)){
			    strncpy(slp->s.sectname, argv[i+2],
				    sizeof(slp->s.sectname));
			    error("section name: %s too long trunctated to %s",
				  argv[i+2], slp->s.sectname);
			}
			else
			    strcpy(slp->s.sectname, argv[i+2]);
			strncpy(slp->s.segname, sgp->segname,
				sizeof(slp->s.segname));
			slp->s.addr = 0; /* filled in later */
			slp->s.size = statbuf.st_size;
			round(slp->s.size, sizeof(long));
			slp->s.offset = 0; /* filled in later */
			slp->s.align = 2;
			/* all other fields zero */

			seg_create[j].sizeofsects += slp->s.size;
			sgp->cmdsize += sizeof(struct section);
			sgp->nsects++;

			i += 3;
			continue;
		    }
		    /*
		     * This is a new segment, create the structures to hold the
		     * info for it.
		     */
		    seg_create = (struct seg_create *)reallocate(seg_create,
			(n_seg_create + 1) * sizeof (struct seg_create));
		    seg_create[n_seg_create].slp = (struct section_list *)
				    allocate(sizeof (struct section_list));
		}
		slp = seg_create[n_seg_create].slp;
		bzero((char *)slp, sizeof(struct section_list));
		slp->filename = argv[i+3];
		slp->filesize = statbuf.st_size;
		len = strlen(argv[i+2]);
		if(len > sizeof(slp->s.sectname)){
		    strncpy(slp->s.sectname, argv[i+2],
			    sizeof(slp->s.sectname));
		    error("section name: %s too long trunctated to %s",
			  argv[i+2], slp->s.sectname);
		}
		else
		    strcpy(slp->s.sectname, argv[i+2]);
		len = strlen(argv[i+1]);
		if(len > sizeof(slp->s.segname)){
		    strncpy(slp->s.segname, argv[i+1], sizeof(slp->s.segname));
		    error("segment name: %s too long trunctated to %s",
			  argv[i+1], slp->s.segname);
		    len = sizeof(slp->s.segname);
		}
		else
		    strcpy(slp->s.segname, argv[i+1]);
		slp->s.addr = 0; /* filled in later */
		slp->s.size = statbuf.st_size;
		round(slp->s.size, sizeof(long));
		slp->s.align = 2; /* long aligned (2^2) */
		slp->s.offset = 0; /* filled in later */
		/* all other fields zero */

		seg_create[n_seg_create].sizeofsects = slp->s.size;

		sgp = &(seg_create[n_seg_create].sg);
		bzero((char *)sgp, sizeof(struct segment_command));
		sgp->cmd = LC_SEGMENT;
		sgp->cmdsize = sizeof(struct segment_command) +
			       sizeof(struct section);
		strncpy(sgp->segname, slp->s.segname, sizeof(sgp->segname));
		sgp->vmaddr = 0; /* filled in later */
		sgp->vmsize = 0; /* filled in later */;
		sgp->fileoff = 0; /* filled in later */
		sgp->filesize = 0; /* set to vmsize later */
		sgp->maxprot = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
		sgp->initprot =VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
		sgp->nsects = 1;
		sgp->flags = 0;

		n_seg_create++;
		i += 3;
		continue;
	    }
	    if(strcmp("-objc", argv[i]) == 0){
		objcflag = 1;
		continue;
	    }
	    if(strcmp("-gg", argv[i]) == 0){
		ggflag = 1;
		continue;
	    }
	    if(aoutfile == NULL){
		aoutfile = argv[i];
		continue;
	    }
	    if(machofile == NULL){
		machofile = argv[i];
		continue;
	    }
	    error("unrecognized argument: %s\n", argv[i]);
	    usage();
	}
	if(aoutfile == NULL){
	    error("a.out file to convert not specified");
	    usage();
	}
	if(machofile == NULL){
	    error("output Mach-O file not specified");
	    usage();
	}

	/*
	 * Now for each segment created from files round the vmsize to the 
	 * pagesize and set the filesize.  Also sum up the total size into
	 * seg_create_size.
	 */
	for(i = 0; i < n_seg_create; i++){
	    seg_create[i].sg.vmsize =
		(seg_create[i].sizeofsects + page_size - 1) & (- page_size);
	    seg_create[i].sg.filesize = seg_create[i].sg.vmsize;
	    seg_create_size += seg_create[i].sg.vmsize;
	}

	/*
	 * Open the a.out file and see if it is in a format that can be
	 * converted to a mach object file.
	 */
	if((afd = open(aoutfile, O_RDONLY)) == -1)
	    system_fatal("Can't open a.out file: %s", aoutfile);
	if(read(afd, &exec, sizeof(struct exec)) != sizeof(struct exec))
	    system_fatal("Can't read exec header of a.out file: %s", aoutfile);
	if(exec.a_magic != ZMAGIC && exec.a_magic != OMAGIC)
	    fatal("Can't convert non-ZMAGIC or non-OMAGIC a.out file: %s",
		  aoutfile);
	if(n_seg_create > 0 && exec.a_magic == OMAGIC){
	    error("-segcreate option(s) ignored with OMAGIC a.out file: %s",
		  aoutfile);
	    n_seg_create = 0;
	    seg_create_size = 0;
	}
	if(objcflag && exec.a_magic == ZMAGIC){
	    error("-objc option ignored with ZMAGIC a.out file: %s",
		  aoutfile);
	    objcflag = 0;
	}

	/*
	 * Create the mach object file.
	 */
	if((mfd = open(machofile, O_WRONLY|O_CREAT|O_TRUNC, 0)) == -1)
	    system_fatal("Can't create Mach-O file: %s", machofile);

	/*
	 * For converted a.out files the resulting Mach-O object file will
	 * look like:
	 *
	 *	mach header
	 *	mach load commands
	 *	<pad to page offset in the file (for ZMAGIC input)>
	 *	the text segment (from the original a.out including it's header
	 * 			  for ZMAGIC input)
	 *	the data segment
	 *	the objective-C segment (for OMAGIC input only)
	 *	any segments created from files (for ZMAGIC input only)
	 *	the relocation entries
	 *	the 4.3bsd symbol table
	 *	the 4.3bsd string table
	 *	the gdb symbol segments
	 *
	 * The pad for executables is required because the text is not relocated
	 * to follow directly after the mach header and load commands and
	 * therefore must remain on a page offset in the file.
	 */

	header.magic = MH_MAGIC;
	header.cputype = CPU_TYPE_MC680x0;
	header.cpusubtype = CPU_SUBTYPE_MC680x0_ALL;
	if(exec.a_magic == ZMAGIC){
	    header.filetype = MH_EXECUTE;
	    header.ncmds = 5 + n_seg_create;
	    if(ggflag)
		header.ncmds++;
	    header.sizeofcmds =
		    3 * sizeof(struct segment_command) +
		    3 * sizeof(struct section) +
		    sizeof(struct symtab_command) +
		    sizeof(struct thread_command) +
		    sizeof(unsigned long) +
		    sizeof(unsigned long) +
		    sizeof(struct m68k_thread_state_regs);
	    if(ggflag)
		header.sizeofcmds += sizeof(struct symseg_command);
	    for(i = 0; i < n_seg_create ; i++)
		header.sizeofcmds += seg_create[i].sg.cmdsize;
	    header.flags = MH_NOUNDEFS;
	    /* header_size includes the pad to the next page offset */
	    header_size =
	      (header.sizeofcmds + sizeof(struct mach_header) + page_size - 1) &
	      (- page_size);
	}
	else{
	    header.filetype = MH_OBJECT;
	    header.ncmds = 2;
	    if(ggflag)
		header.ncmds++;
	    if(objcflag){
		header.sizeofcmds =
			sizeof(struct segment_command) +
			6 * sizeof(struct section) +
			sizeof(struct symtab_command);
		if(ggflag)
		    header.sizeofcmds += sizeof(struct symseg_command);
	    }
	    else{
		header.sizeofcmds =
			sizeof(struct segment_command) +
			3 * sizeof(struct section) +
			sizeof(struct symtab_command);
		if(ggflag)
		    header.sizeofcmds += sizeof(struct symseg_command);
	    }
	    header.flags = 0;
	    header_size = header.sizeofcmds + sizeof(struct mach_header);
	}

	pagezero_segment.cmd = LC_SEGMENT;
	pagezero_segment.cmdsize = sizeof(struct segment_command);
	strcpy(pagezero_segment.segname, SEG_PAGEZERO);
	pagezero_segment.vmaddr = 0;
	pagezero_segment.vmsize = page_size;
	pagezero_segment.fileoff = 0;
	pagezero_segment.filesize = 0;
	pagezero_segment.maxprot = VM_PROT_NONE;
	pagezero_segment.initprot = VM_PROT_NONE;
	pagezero_segment.nsects = 0;
	pagezero_segment.flags = 0;

	reloc_segment.cmd = LC_SEGMENT;
	if(objcflag)
	    reloc_segment.cmdsize = sizeof(struct segment_command) +
				    6 * sizeof(struct section);
	else
	    reloc_segment.cmdsize = sizeof(struct segment_command) +
				    3 * sizeof(struct section);
	/* leave reloc_segment.segname full of zeros */
	reloc_segment.vmaddr = NN_TXTADDR(exec);
	reloc_segment.vmsize = exec.a_text + exec.a_data + exec.a_bss;
	reloc_segment.fileoff = header_size;
	reloc_segment.filesize = exec.a_text + exec.a_data;
	reloc_segment.maxprot = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
	reloc_segment.initprot= VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
	if(objcflag)
	    reloc_segment.nsects = 6;
	else
	    reloc_segment.nsects = 3;
	reloc_segment.flags = 0;

	text_segment.cmd = LC_SEGMENT;
	text_segment.cmdsize = sizeof(struct segment_command) +
				 sizeof(struct section);
	strcpy(text_segment.segname, SEG_TEXT);
	text_segment.vmaddr = NN_TXTADDR(exec);
	text_segment.vmsize = round(exec.a_text, N_SEGSIZ(exec)) -
			      NN_TXTADDR(exec);
	text_segment.fileoff = header_size;
	text_segment.filesize = exec.a_text - NN_TXTADDR(exec);
	text_segment.maxprot = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
	text_segment.initprot = VM_PROT_READ | VM_PROT_EXECUTE;
	text_segment.nsects = 1;
	text_segment.flags = 0;

	strcpy(text_section.sectname, SECT_TEXT);
	strcpy(text_section.segname, SEG_TEXT);
	text_section.addr = NN_TXTADDR(exec);
	text_section.size = exec.a_text;
	text_section.offset = header_size;
	text_section.align = 2;
	if(exec.a_trsize != 0) {
	    text_section.reloff = header_size + exec.a_text + exec.a_data +
				    seg_create_size;
	    text_section.nreloc = exec.a_trsize /
				    sizeof (struct relocation_info);
	}
	else{
	    text_section.reloff = 0;
	    text_section.nreloc = 0;
	}
	text_section.flags = 0;
	text_section.reserved1 = 0;
	text_section.reserved2 = 0;

	data_segment.cmd = LC_SEGMENT;
	data_segment.cmdsize = sizeof(struct segment_command) +
				 2 * sizeof(struct section);
	strcpy(data_segment.segname, SEG_DATA);
	data_segment.vmaddr = NN_DATADDR(exec);
	data_segment.vmsize = exec.a_data + exec.a_bss;
	data_segment.fileoff = header_size + exec.a_text;
	data_segment.filesize = exec.a_data;
	data_segment.maxprot = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
	data_segment.initprot =VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
	data_segment.nsects = 2;
	data_segment.flags = 0;

	strcpy(data_section.sectname, SECT_DATA);
	strcpy(data_section.segname, SEG_DATA);
	data_section.addr = NN_DATADDR(exec);
	data_section.offset = header_size + exec.a_text;
	data_section.align = 2;
	data_section.flags = 0;
	data_section.reserved1 = 0;
	data_section.reserved2 = 0;
	if(objcflag){
	    strcpy(sym_section.sectname, SECT_OBJC_SYMBOLS);
	    strcpy(sym_section.segname, SEG_OBJC);
	    sym_section.align = 2;
	    strcpy(mod_section.sectname, SECT_OBJC_MODULES);
	    strcpy(mod_section.segname, SEG_OBJC);
	    mod_section.align = 2;
	    strcpy(sel_section.sectname, SECT_OBJC_STRINGS);
	    strcpy(sel_section.segname, SEG_OBJC);
	    sel_section.align = 2;

	    /*
	     * Now read the symbol and string table to find the symbols that
	     * start each of the objective-C sections.  Then use the addresses
	     * of those symbols to divide up the data section.  The assumptions
	     * are as follows:
	     *		the data for these are at the end of the data section
	     *		they are in the following order (if present)
	     *			symbol table section
	     *			module information section
	     *			selector strings section
	     */
	    symtab = (struct nlist *)allocate(exec.a_syms);
	    lseek(afd, N_SYMOFF(exec), L_SET);
	    if(read(afd, symtab, exec.a_syms) != exec.a_syms)
		system_fatal("Can't read symbol table from a.out file: %s",
			     aoutfile);
	    lseek(afd, N_STROFF(exec), L_SET);
	    if(read(afd, &strsize, sizeof(long)) != sizeof(long))
		system_fatal("Can't read string table size from a.out file: "
			     "%s", aoutfile);
	    strtab = (char *)allocate(strsize);
	    lseek(afd, N_STROFF(exec), L_SET);
	    if(read(afd, strtab, strsize) != strsize)
		system_fatal("Can't read string table from a.out file: %s",
			     aoutfile);

	    pnlist = lookup(OBJC_SEL);
	    if(pnlist != NULL){
		if(pnlist->n_value < data_section.addr ||
		   (pnlist->n_value >= data_section.addr + exec.a_data))
		    fatal("Objective-C selector string table symbol's value "
			  "not in the data section");
		if(pnlist->n_value % sizeof(long) != 0)
		    error("Objective-C selector string table symbol's value "
			  "not a multiple of sizeof(long)");
		sel_section.addr = pnlist->n_value;
		sel_section.size = (data_section.addr + exec.a_data) -
				   pnlist->n_value;
	    }
	    else{
		sel_section.addr = data_section.addr + exec.a_data;
		sel_section.size = 0;
	    }

	    pnlist = lookup(OBJC_MOD);
	    if(pnlist != NULL){
		if(pnlist->n_value < data_section.addr ||
		   (pnlist->n_value >= data_section.addr + exec.a_data))
		    fatal("Objective-C module info symbol's value not in the "
			  "data section");
		if(pnlist->n_value > sel_section.addr)
		    fatal("Objective-C module info symbol's value greater than "
		          "symbol table symbol's value");
		if(pnlist->n_value % sizeof(long) != 0)
		    error("Objective-C module info symbol's value not a "
			  "multiple of sizeof(long)");
		mod_section.addr = pnlist->n_value;
		mod_section.size = sel_section.addr - pnlist->n_value;
	    }
	    else{
		mod_section.addr = sel_section.addr;
		mod_section.size = 0;
	    }

	    pnlist = lookup(OBJC_SYM);
	    if(pnlist != NULL){
		if(pnlist->n_value < data_section.addr ||
		   (pnlist->n_value >= data_section.addr + exec.a_data))
		    fatal("Objective-C symbol table symbol's value not in the "
			  " data section");
		if(pnlist->n_value > mod_section.addr)
		    fatal("Objective-C symbol table symbol's value greater "
			  "than module info symbol's value");
		if(pnlist->n_value % sizeof(long) != 0)
		    error("Objective-C symbol table symbol's value not a "
			  "multiple of sizeof(long)");
		sym_section.addr = pnlist->n_value;
		sym_section.size = mod_section.addr - pnlist->n_value;
	    }
	    else{
		sym_section.addr = mod_section.addr;
		sym_section.size = 0;
	    }

	    /*
	     * Now knowing the sizes of all the objective-C sections the size
	     * of the data section and the file offsets can be calculated.
	     */
	    data_section.size = exec.a_data -
		(sel_section.size + sym_section.size + mod_section.size);

	    sym_section.offset = data_section.offset + data_section.size;
	    mod_section.offset =  sym_section.offset + sym_section.size;
	    sel_section.offset =  mod_section.offset + mod_section.size;

	    /*
	     * Now the data relocation entries have to be divided up.  This
	     * is relying on the fact that the assembler produces relocation
	     * entries in decreasing order based on the r_address field.  Note
	     * the r_address field is really an offset into the section.
	     */
	    if(exec.a_drsize != 0) {
		data_reloc = (struct relocation_info *)allocate(exec.a_drsize);
		lseek(afd, N_TXTOFF(exec) + exec.a_text + exec.a_data +
			   exec.a_trsize, L_SET);
		if(read(afd, data_reloc, exec.a_drsize) != exec.a_drsize)
		    system_fatal("Can't read data relocation entries from "
				 "a.out file: %s", aoutfile);

		data_reloff = header_size + exec.a_text + exec.a_data +
			      seg_create_size + exec.a_trsize;
		data_nreloc = exec.a_drsize / sizeof (struct relocation_info);

		data_section.reloff = data_reloff;
		data_section.nreloc = 0;
		sym_section.reloff  = data_reloff;
		sym_section.nreloc = 0;
		mod_section.reloff  = data_reloff;
		mod_section.nreloc = 0;
		sel_section.reloff  = data_reloff;
		sel_section.nreloc = 0;

		for(preloc = (struct relocation_info *)data_reloc;
		    preloc < data_reloc + data_nreloc;
		    preloc++){
		    if(preloc->r_address >=
		       (data_section.addr + data_section.size) - exec.a_text)
			data_section.reloff += sizeof(struct relocation_info);
		    else if(preloc->r_address >=
			    data_section.addr - exec.a_text)
			data_section.nreloc++;

		    if(preloc->r_address >=
		       (sym_section.addr + sym_section.size) - exec.a_text)
			sym_section.reloff += sizeof(struct relocation_info);
		    else if(preloc->r_address >=
		            sym_section.addr - exec.a_text)
			sym_section.nreloc++;

		    if(preloc->r_address >=
		       (mod_section.addr + mod_section.size) - exec.a_text)
			mod_section.reloff += sizeof(struct relocation_info);
		    else if(preloc->r_address >=
		            mod_section.addr - exec.a_text)
			mod_section.nreloc++;

		    if(preloc->r_address >=
		       (sel_section.addr + sel_section.size) - exec.a_text)
			sel_section.reloff += sizeof(struct relocation_info);
		    else if(preloc->r_address >=
			    sel_section.addr - exec.a_text)
			sel_section.nreloc++;
		}
	    }
	    else{
		data_section.reloff = 0;
		data_section.nreloc = 0;
		sym_section.reloff = 0;
		sym_section.nreloc = 0;
		mod_section.reloff = 0;
		mod_section.nreloc = 0;
		sel_section.reloff = 0;
		sel_section.nreloc = 0;
	    }
	    sym_section.flags = 0;
	    sym_section.reserved1 = 0;
	    sym_section.reserved2 = 0;
	    mod_section.flags = 0;
	    mod_section.reserved1 = 0;
	    mod_section.reserved2 = 0;
	    sel_section.flags = S_CSTRING_LITERALS;
	    sel_section.reserved1 = 0;
	    sel_section.reserved2 = 0;
	}
	else{
	    data_section.size = exec.a_data;
	    if(exec.a_drsize != 0) {
		data_section.reloff = header_size + exec.a_text + exec.a_data +
					seg_create_size + exec.a_trsize;
		data_section.nreloc = exec.a_drsize /
					sizeof (struct relocation_info);
	    }
	    else{
		data_section.reloff = 0;
		data_section.nreloc = 0;
	    }
	}

	strcpy(bss_section.sectname, SECT_BSS);
	strcpy(bss_section.segname, SEG_DATA);
	bss_section.addr = NN_DATADDR(exec) + exec.a_data;
	bss_section.size = exec.a_bss;
	bss_section.offset = 0;
	bss_section.align = 2;
	bss_section.reloff = 0;
	bss_section.nreloc = 0;
	bss_section.flags = S_ZEROFILL;
	bss_section.reserved1 = 0;
	bss_section.reserved2 = 0;

	thread.cmd = LC_UNIXTHREAD;
	thread.cmdsize = sizeof(struct thread_command) +
			   sizeof(unsigned long) + sizeof(unsigned long) +
			   sizeof(struct m68k_thread_state_regs);
	flavor = M68K_THREAD_STATE_REGS;
	count = M68K_THREAD_STATE_REGS_COUNT;
	cpu.pc = exec.a_entry;

	/*
	 * Assign the addresses and offsets to the segments and their sections
	 * created from files.
	 */
	vmaddr = NN_DATADDR(exec) + exec.a_data + exec.a_bss;
	vmaddr = (vmaddr + page_size - 1) & (- page_size);
	fileoff = header_size + exec.a_text + exec.a_data;
	for(i = 0; i < n_seg_create; i++){
	    seg_create[i].sg.fileoff = fileoff;
	    seg_create[i].sg.vmaddr = vmaddr;
	    zs_addr = vmaddr;
	    s_offset = fileoff;
	    slp = seg_create[i].slp;
	    for(j = 0; j < seg_create[i].sg.nsects; j++){
		slp->s.addr = zs_addr;
		slp->s.offset = s_offset;
		zs_addr += slp->s.size;
		s_offset += slp->s.size;
		slp = slp->next;
	    }
	    vmaddr += seg_create[i].sg.vmsize;
	    fileoff += seg_create[i].sg.filesize;
	}

	if(fstat(afd, &statbuf) == -1)
	    system_fatal("Can't stat a.out file: %s ", aoutfile);
	symbol_table.cmd = LC_SYMTAB;
	symbol_table.cmdsize = sizeof(struct symtab_command);
	symbol_table.symoff = 0;
	symbol_table.nsyms = 0;
	symbol_table.stroff = 0;
	symbol_table.strsize = 0;

	symbol_segment.cmd = LC_SYMSEG;
	symbol_segment.cmdsize = sizeof(struct symseg_command);
	symbol_segment.offset = 0;
	symbol_segment.size = 0;

	if(exec.a_syms != 0){
	    fileoff = header_size + exec.a_text + exec.a_data +
		      seg_create_size + exec.a_trsize + exec.a_drsize;
	    symbol_table.symoff = fileoff;
	    symbol_table.nsyms = exec.a_syms / sizeof(struct nlist);
	    fileoff += exec.a_syms;
	    symbol_table.stroff = fileoff;
	    if(strsize == 0){
		lseek(afd, N_STROFF(exec), L_SET);
		if(read(afd, &strsize, sizeof(long)) != sizeof(long))
		    system_fatal("Can't read string table size from a.out "
				 "file : %s", aoutfile);
	    }
	    symbol_table.strsize = strsize;
	    fileoff += strsize;

	    symbol_segment.cmd = LC_SYMSEG;
	    symbol_segment.cmdsize = sizeof(struct symseg_command);
	    symbol_segment.offset = fileoff;
	    symbol_segment.size = statbuf.st_size - (N_STROFF(exec) + strsize);
	}

	/*
	 * Now write out the mach object file.
	 */
	/* write the mach header and the normal set of load commands */
	lseek(mfd, 0L, L_SET);
	if(write(mfd, &header, sizeof(struct mach_header)) !=
		sizeof(struct mach_header))
	    system_fatal("Can't write mach header to Mach-O file: %s",
			 machofile);
	if(exec.a_magic == ZMAGIC){
	    if(write(mfd, &pagezero_segment, sizeof(struct segment_command)) !=
		    sizeof(struct segment_command))
		system_fatal("Can't write segment_command for: %s segment to "
			     "Mach-O file: %s", SEG_PAGEZERO, machofile);
	    if(write(mfd, &text_segment, sizeof(struct segment_command)) !=
		    sizeof(struct segment_command))
		system_fatal("Can't write segment_command for: %s segment to "
			     "Mach-O file: %s", SEG_TEXT, machofile);
	    if(write(mfd, &text_section, sizeof(struct section)) !=
		    sizeof(struct section))
		system_fatal("Can't write section header for: %s section to "
			     "Mach-O file: %s", SECT_TEXT, machofile);
	    if(write(mfd, &data_segment, sizeof(struct segment_command)) !=
		    sizeof(struct segment_command))
		system_fatal("Can't write segment_command for: %s segment to "
			     "Mach-O file: %s", SEG_DATA, machofile);
	    if(write(mfd, &data_section, sizeof(struct section)) !=
		    sizeof(struct section))
		system_fatal("Can't write section header for: %s section to "
			     "Mach-O file: %s", SECT_DATA, machofile);
	    if(write(mfd, &bss_section, sizeof(struct section)) !=
		    sizeof(struct section))
		system_fatal("Can't write section header for: %s section to "
			     "Mach-O file: %s", SECT_BSS, machofile);
	    if(write(mfd, &symbol_table, sizeof(struct symtab_command)) !=
		    sizeof(struct symtab_command))
		system_fatal("Can't write symtab_command to "
			     "Mach-O file: %s", machofile);
	    if(ggflag)
		if(write(mfd, &symbol_segment, sizeof(struct symseg_command)) !=
			sizeof(struct symseg_command))
		    system_fatal("Can't write symseg_command to "
				 "Mach-O file: %s", machofile);
	    if(write(mfd, &thread, sizeof(struct thread_command)) !=
		    sizeof(struct thread_command))
		system_fatal("Can't write thread_command to "
			     "Mach-O file: %s", machofile);
	    if(write(mfd, &flavor, sizeof(unsigned long)) !=
		    sizeof(unsigned long))
		system_fatal("Can't write thread flavor to "
			     "Mach-O file: %s", machofile);
	    if(write(mfd, &count, sizeof(unsigned long)) !=
		    sizeof(unsigned long))
		system_fatal("Can't write thread count to "
			     "Mach-O file: %s", machofile);
	    if(write(mfd, &cpu, sizeof(struct m68k_thread_state_regs)) !=
		    sizeof(struct m68k_thread_state_regs))
		system_fatal("Can't write thread state to "
			     "Mach-O file: %s", machofile);
	}
	else{
	    if(write(mfd, &reloc_segment, sizeof(struct segment_command)) !=
		    sizeof(struct segment_command))
		system_fatal("Can't write segment_command to "
			     "Mach-O file: %s", machofile);
	    if(write(mfd, &text_section, sizeof(struct section)) !=
		    sizeof(struct section))
		system_fatal("Can't write section header for: %s section to "
			     "Mach-O file: %s", SECT_TEXT, machofile);
	    if(write(mfd, &data_section, sizeof(struct section)) !=
		    sizeof(struct section))
		system_fatal("Can't write section header for: %s section to "
			     "Mach-O file: %s", SECT_DATA, machofile);
	    if(write(mfd, &bss_section, sizeof(struct section)) !=
		    sizeof(struct section))
		system_fatal("Can't write section header for: %s section to "
			     "Mach-O file: %s", SECT_BSS, machofile);
	    if(objcflag){
		if(write(mfd, &sym_section, sizeof(struct section)) !=
			sizeof(struct section))
		    system_fatal("Can't write section header for: %s section "
				 "to Mach-O file: %s", SECT_OBJC_SYMBOLS,
				 machofile);
		if(write(mfd, &mod_section, sizeof(struct section)) !=
			sizeof(struct section))
		    system_fatal("Can't write section header for: %s section "
				 "to Mach-O file: %s", SECT_OBJC_MODULES,
				 machofile);
		if(write(mfd, &sel_section, sizeof(struct section)) !=
			sizeof(struct section))
		    system_fatal("Can't write section header for: %s section "
				 "to Mach-O file: %s", SECT_OBJC_STRINGS,
				 machofile);
	    }
	    if(write(mfd, &symbol_table, sizeof(struct symtab_command)) !=
		    sizeof(struct symtab_command))
		system_fatal("Can't write symtab_command to "
			     "Mach-O file: %s", machofile);
	    if(ggflag)
		if(write(mfd, &symbol_segment, sizeof(struct symseg_command)) !=
			sizeof(struct symseg_command))
		    system_fatal("Can't write symseg_command to "
				 "Mach-O file: %s", machofile);
	}
	/* write the load commands for segments created from files */
	for(i = 0; i < n_seg_create; i++){
	    if(write(mfd, &(seg_create[i].sg), sizeof(struct segment_command))
	       != sizeof(struct segment_command))
		system_fatal("Can't write load commands for segments created "
			     "from files to Mach-O file: %s", machofile);
	    slp = seg_create[i].slp;
	    for(j = 0; j < seg_create[i].sg.nsects; j++){
		if(write(mfd, &(slp->s), sizeof(struct section))
		   != sizeof(struct section))
		    system_fatal("Can't write section structures of the load "
			         "commands for segments created from files to "
				 "Mach-O file: %s", machofile);
		slp = slp->next;
	    }
	}
	/* leave the pad after the headers */
	lseek(mfd, header_size, L_SET);

	/* read and write the text */
	text = (char *)allocate(exec.a_text);
	lseek(afd, N_TXTOFF(exec), L_SET);
	if(read(afd, text, exec.a_text) != exec.a_text)
	    system_fatal("Can't read text from a.out file: %s", aoutfile);
	if(write(mfd, text, exec.a_text) != exec.a_text)
	    system_fatal("Can't write text to Mach-O file: %s", machofile);

	/* read and write the data */
	data = (char *)allocate(exec.a_data);
	if(read(afd, data, exec.a_data) != exec.a_data)
	    system_fatal("Can't read data from a.out file: %s", aoutfile);
	if(write(mfd, data, exec.a_data) != exec.a_data)
	    system_fatal("Can't write data to Mach-O file: %s", machofile);

	/* read and write the segments created from files */
	for(i = 0; i < n_seg_create; i++){
	    slp = seg_create[i].slp;
	    for(j = 0 ; j < seg_create[i].sg.nsects ; j++){
		p = (char *)allocate(slp->filesize);
		if((fd = open(slp->filename, O_RDONLY)) == -1)
		    system_fatal("Can't open file: %s to create section %s in "
			         "segment: %s", slp->filename, slp->s.sectname,
				 slp->s.segname);
		if(read(fd, p, slp->filesize) != slp->filesize)
		    system_fatal("Can't read file: %s to create section %s in "
			         "segment: %s", slp->filename, slp->s.sectname,
				 slp->s.segname);
		lseek(mfd, slp->s.offset, L_SET);
		if(write(mfd, p, slp->filesize) != slp->filesize)
		    system_fatal("Can't write contents of: %s to Mach-O file: "
				 "%s", slp->filename, machofile);
		free(p);
		close(fd);
		slp = slp->next;
	    }
	}

	/* read and write the text relocation entries */
	if(exec.a_trsize != 0){
	    p = (char *)allocate(exec.a_trsize);
	    if(read(afd, p, exec.a_trsize) != exec.a_trsize)
		system_fatal("Can't read text relocation entries from a.out "
			     "file: %s", aoutfile);
	    /* convert local relocation entries to Mach-O style */
	    for(preloc = (struct relocation_info *)p;
		preloc < (struct relocation_info *)(p + exec.a_trsize);
		preloc++){
		if(preloc->r_extern == 0){
		    switch(preloc->r_symbolnum){
		    case N_TEXT:
			preloc->r_symbolnum = 1;
			break;
		    case N_DATA:
			if(objcflag){
			    long addr;

			    addr = 0;
			    /*
			     * First figure out what section the item being
			     * relocated is refering to and set the r_symbolnum
			     * to the correct section ordinal.
			     */
			    switch(preloc->r_length){
			    case 0:
				addr = *(char *)(text + preloc->r_address);
				break;
			    case 1:
				addr = *(short *)(text + preloc->r_address);
				break;
			    case 2:
				addr = *(long *)(text + preloc->r_address);
				break;
			    default:
				fatal("Bad r_length field (0x%x) for a local "
				      "text relocation entry (%d)",
				      (unsigned int)preloc->r_symbolnum,  (int)
				      ((struct relocation_info *)p - preloc));
			    }
			    if(preloc->r_pcrel)
				addr -= preloc->r_address + exec.a_text;
		  	    if(addr >= sel_section.addr + sel_section.size)
				preloc->r_symbolnum = 2;
		  	    else if(addr >= sel_section.addr)
				preloc->r_symbolnum = 6;
		  	    else if(addr >= mod_section.addr)
				preloc->r_symbolnum = 5;
		  	    else if(addr >= sym_section.addr)
				preloc->r_symbolnum = 4;
		  	    else
				preloc->r_symbolnum = 2;
			}
			else{
			    preloc->r_symbolnum = 2;
			}
			break;
		    case N_BSS:
			preloc->r_symbolnum = 3;
			break;
		    case N_ABS:
			preloc->r_symbolnum = R_ABS;
			break;
		    default:
			fatal("Bad r_symbolnum field (0x%x) for a local "
			      "relocation entry (%d)",
			      (unsigned int)preloc->r_symbolnum,  
			      (int)((struct relocation_info *)p - preloc));
		    }
		}
	    }
	    lseek(mfd, text_section.reloff, L_SET);
	    if(write(mfd, p, exec.a_trsize) != exec.a_trsize)
		system_fatal("Can't write text relocation entries to Mach-O "
			     "file: %s", machofile);
	    free(p);
	}

	/* read and write the data relocation entries */
	if(exec.a_drsize != 0){
	    if(data_reloc != NULL){
		p = (char *)data_reloc;
	    }
	    else{
		p = (char *)allocate(exec.a_drsize);
		if(read(afd, p, exec.a_drsize) != exec.a_drsize)
		    system_fatal("Can't read data relocation entries from "
				 "a.out file: %s", aoutfile);
	    }
	    /* convert local relocation entries to Mach-O style */
	    for(preloc = (struct relocation_info *)p;
		preloc < (struct relocation_info *)(p + exec.a_drsize);
		preloc++){
		if(preloc->r_extern == 0){
		    switch(preloc->r_symbolnum){
		    case N_TEXT:
			preloc->r_symbolnum = 1;
			break;
		    case N_DATA:
			if(objcflag){
			    long addr;

			    addr = 0;
			    /*
			     * First figure out what section the item being
			     * relocated is refering to and set the r_symbolnum
			     * to the correct section ordinal.
			     */
			    switch(preloc->r_length){
			    case 0:
				addr = *(char *)(data + preloc->r_address);
				break;
			    case 1:
				addr = *(short *)(data + preloc->r_address);
				break;
			    case 2:
				addr = *(long *)(data + preloc->r_address);
				break;
			    default:
				fatal("Bad r_length field (0x%x) for a local "
				      "data relocation entry (%d)",
				      (unsigned int)preloc->r_symbolnum, (int)
				      ((struct relocation_info *)p - preloc));
			    }
			    if(preloc->r_pcrel)
				addr -= preloc->r_address + exec.a_text +
					exec.a_data;
		  	    if(addr >= sel_section.addr + sel_section.size)
				preloc->r_symbolnum = 2;
		  	    else if(addr >= sel_section.addr)
				preloc->r_symbolnum = 6;
		  	    else if(addr >= mod_section.addr)
				preloc->r_symbolnum = 5;
		  	    else if(addr >= sym_section.addr)
				preloc->r_symbolnum = 4;
		  	    else
				preloc->r_symbolnum = 2;
			}
			else{
			    preloc->r_symbolnum = 2;
			}
			break;
		    case N_BSS:
			preloc->r_symbolnum = 3;
			break;
		    case N_ABS:
			preloc->r_symbolnum = R_ABS;
			break;
		    default:
			fatal("Bad r_symbolnum field (0x%x) for a local "
			      "relocation entry (%d)",
			      (unsigned int)preloc->r_symbolnum,  
			      (int)((struct relocation_info *)p - preloc));
		    }
		}
		if(objcflag){
		    /*
		     * Now adjust the r_address field (which is an
		     * offset) to be an offset into the section that
		     * the address it is refering to is in.
		     */
		    if(preloc->r_address >= sel_section.addr - exec.a_text)
			preloc->r_address -=
				(sel_section.addr - data_section.addr);
		    else if(preloc->r_address >= mod_section.addr - exec.a_text)
			preloc->r_address -=
				(mod_section.addr - data_section.addr);
		    else if(preloc->r_address >= sym_section.addr - exec.a_text)
			preloc->r_address -=
				(sym_section.addr - data_section.addr);
		    /* else its in the data section an it's fine as is*/
		}
	    }
	    if(objcflag)
		lseek(mfd, data_reloff, L_SET);
	    else
		lseek(mfd, data_section.reloff, L_SET);
	    if(write(mfd, p, exec.a_drsize) != exec.a_drsize)
		system_fatal("Can't write data relocation entries to Mach-O "
			     "file: %s", machofile);
	    free(p);
	}

	/* read and write the symbol table */
	if(exec.a_syms != 0){
	    if(symtab != NULL){
		p = (char *)symtab;
	    }
	    else{
		p = (char *)allocate(exec.a_syms);
		lseek(afd, N_SYMOFF(exec), L_SET);
		if(read(afd, p, exec.a_syms) != exec.a_syms)
		    system_fatal("Can't read symbol table from a.out file: %s",
				 aoutfile);
	    }
	    /* convert symbol table entries to Mach-O style */
	    for(pnlist = (struct nlist *)p;
		pnlist < (struct nlist *)(p + exec.a_syms);
		pnlist++){
		switch(pnlist->n_type & N_TYPE){
		case N_TEXT:
		    pnlist->n_sect = 1;
		    if((pnlist->n_type & N_STAB) == 0)
			pnlist->n_type = N_SECT | (pnlist->n_type & N_EXT);
		    break;
		case N_DATA:
		    if(objcflag){
			if(pnlist->n_value >=
			   sel_section.addr + sel_section.size)
			    pnlist->n_sect = 2;
			else if(pnlist->n_value >= sel_section.addr)
			    pnlist->n_sect = 6;
			else if(pnlist->n_value >= mod_section.addr)
			    pnlist->n_sect = 5;
			else if(pnlist->n_value >= sym_section.addr)
			    pnlist->n_sect = 4;
			else
			    pnlist->n_sect = 2;
		    }
		    else{
			pnlist->n_sect = 2;
		    }
		    if((pnlist->n_type & N_STAB) == 0)
			pnlist->n_type = N_SECT | (pnlist->n_type & N_EXT);
		    break;
		case N_BSS:
		    pnlist->n_sect = 3;
		    if((pnlist->n_type & N_STAB) == 0)
			pnlist->n_type = N_SECT | (pnlist->n_type & N_EXT);
		    break;
		case N_SECT:
		    break;
		default:
		    pnlist->n_sect = NO_SECT;
		}
	    }
	    lseek(mfd, symbol_table.symoff, L_SET);
	    if(write(mfd, p, exec.a_syms) != exec.a_syms)
		system_fatal("Can't write symbol table from a.out file: %s",
			     aoutfile);
	    free(p);
	}

	/* read and write the string table */
	if(strsize != 0){
	    if(strtab != NULL){
		p = strtab;
	    }
	    else{
		p = (char *)allocate(strsize);
		lseek(afd, N_STROFF(exec), L_SET);
		if(read(afd, p, strsize) != strsize)
		    system_fatal("Can't read string table from a.out file: %s",
				 aoutfile);
	    }
	    lseek(mfd, symbol_table.stroff, L_SET);
	    if(write(mfd, p, strsize) != strsize)
		system_fatal("Can't write string table from a.out file: %s",
			     aoutfile);
	    free(p);
	}

	/* read and write the symbol segment */
	if(ggflag && symbol_segment.size != 0){
	    p = (char *)allocate(symbol_segment.size);
	    if(read(afd, p, symbol_segment.size) != symbol_segment.size)
		system_fatal("Can't read symbol segment from a.out file: %s",
			     aoutfile);
	    /*
	     * Convert only the OMAGIC symbol_root to a mach_symbol_root.
	     * Since ZMAGIC files are no longer supported in gdb or produced 
	     * by default this is not done.  Also it's not easy.
	     */
	    if(exec.a_magic == OMAGIC){
		/*
		 * This conversion is quite ugly.  It relies on the fact that
		 * sizeof(struct symbol_root) - sizeof(struct mach_symbol_root)
		 * 	>= sizeof(struct loadmap)
		 * It replaces the symbol_root with a mach_symbol_root copying
		 * the like fields.  Then it sets the loadmap pointer (offset
		 * since it's in a file) to the end of the mach_symbol_root.
		 * The load map is really just a struct loadmap field struct
		 * with the nmaps field zero for a relocatable object.  It gets
		 * the zero by bzero'ing the sizeof the symbol_root.  This also
		 * assumes that in an OMAGIC file there is just one symbol_root.
		 * I did say this was ugly.
		 */
		if(symbol_segment.size < sizeof(struct symbol_root))
		    fatal("Invalid size of gdb symbol segment (smaller than a "
			  "symbol root)");
		psymbol_root = (struct symbol_root *)p;
		mach_root.format = MACH_ROOT_FORMAT;
		mach_root.length = psymbol_root->length;
		mach_root.ldsymoff = psymbol_root->ldsymoff;
		mach_root.filename = psymbol_root->filename;
		mach_root.filedir = psymbol_root->filedir;
		mach_root.blockvector = psymbol_root->blockvector;
		mach_root.typevector = psymbol_root->typevector;
		mach_root.language = psymbol_root->language;
		mach_root.version = psymbol_root->version;
		mach_root.compilation = psymbol_root->compilation;
		mach_root.sourcevector = psymbol_root->sourcevector;

		mach_root.loadmap =
			(struct loadmap *)(sizeof(struct mach_root));
		bzero(p, sizeof(struct symbol_root));
		pmach_root = (struct mach_root *)p;
		*pmach_root = mach_root;
	    }
	    lseek(mfd, symbol_segment.offset, L_SET);
	    if(write(mfd, p, symbol_segment.size) != symbol_segment.size)
		system_fatal("Can't write symbol segment to Mach-O file: %s",
			     machofile);
	    free(p);
	}

	/*
	 * Change the mode of the Mach-O file to preserve the a.out's mode
	 * in the converted Mach-O file.
	 */
	if(fchmod(mfd, statbuf.st_mode & 0777) == -1)
	    system_fatal("Can't change mode of Mach-O file: %s", machofile);

	close(afd);
	close(mfd);

	exit(0);
}

/*
 * A very poor lookup routine for looking up the objective-C static data symbols
 */
static
struct nlist *
lookup(
char *sym_name)
{
    struct nlist *p;

	for(p = symtab; p < symtab + (exec.a_syms / sizeof(struct nlist)); p++){
	    if((p->n_type & N_TYPE) != N_DATA ||
	       (p->n_type & (N_STAB | N_EXT)) != 0)
		continue;
	    if(p->n_un.n_strx > 0 && p->n_un.n_strx){
		if(strcmp(sym_name, strtab + p->n_un.n_strx) == 0)
		    return(p);
	    }
	}
	return(NULL);
}

static
void
usage(void)
{
	fatal("usage: %s a.out Mach-O [-segcreate <segment name> <section name>"
	      " <file name>]", progname);
}
