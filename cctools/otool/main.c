/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
#include <limits.h>
#include <ar.h>
#include <mach-o/ranlib.h>
#include <libc.h>
#include "stuff/bool.h"
#include "stuff/ofile.h"
#include "stuff/errors.h"
#include "stuff/allocate.h"
#include "otool.h"
#include "ofile_print.h"
#include "m68k_disasm.h"
#include "i860_disasm.h"
#include "i386_disasm.h"
#include "m88k_disasm.h"
#include "ppc_disasm.h"
#include "hppa_disasm.h"
#include "sparc_disasm.h"

/* Name of this program for error messages (argv[0]) */
char *progname = NULL;

/*
 * The flags to indicate the actions to perform.
 */
enum bool fflag = FALSE; /* print the fat headers */
enum bool aflag = FALSE; /* print the archive header */
enum bool hflag = FALSE; /* print the exec or mach header */
enum bool lflag = FALSE; /* print the load commands */
enum bool Lflag = FALSE; /* print the shared library names */
enum bool Dflag = FALSE; /* print the shared library id name */
enum bool tflag = FALSE; /* print the text */
enum bool dflag = FALSE; /* print the data */
enum bool oflag = FALSE; /* print the objctive-C info */
enum bool Oflag = FALSE; /* print the objctive-C selector strings only */
enum bool rflag = FALSE; /* print the relocation entries */
enum bool Tflag = FALSE; /* print the dylib table of contents */
enum bool Mflag = FALSE; /* print the dylib module table */
enum bool Rflag = FALSE; /* print the dylib reference table */
enum bool Iflag = FALSE; /* print the indirect symbol table entries */
enum bool Hflag = FALSE; /* print the two-level hints table */
enum bool Sflag = FALSE; /* print the contents of the __.SYMDEF file */
enum bool vflag = FALSE; /* print verbosely (symbolicly) when possible */
enum bool Vflag = FALSE; /* print dissassembled operands verbosely */
enum bool cflag = FALSE; /* print the argument and environ strings of a core */
enum bool iflag = FALSE; /* print the shared library initialization table */
enum bool Wflag = FALSE; /* print the mod time of an archive as a number */
enum bool Xflag = FALSE; /* don't print leading address in disassembly */
enum bool Zflag = FALSE; /* don't use simplified ppc mnemonics in disassembly */
char *pflag = NULL; 	 /* procedure name to start disassembling from */
char *segname = NULL;	 /* name of the section to print the contents of */
char *sectname = NULL;

/* this is set when any of the flags that process object files is set */
enum bool object_processing = FALSE;

static void usage(
    void);

static void processor(
    struct ofile *ofile,
    char *arch_name,
    void *cookie);

static void get_symbol_table_info(
    struct mach_header *mh,
    struct load_command *load_commands,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    unsigned long object_size,
    struct nlist **symbols,
    unsigned long *nsymbols,
    char **strings,
    unsigned long *strings_size);

static void get_toc_info(
    struct mach_header *mh,
    struct load_command *load_commands,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    unsigned long object_size,
    struct dylib_table_of_contents **tocs,
    unsigned long *ntocs);

static void get_module_table_info(
    struct mach_header *mh,
    struct load_command *load_commands,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    unsigned long object_size,
    struct dylib_module **mods,
    unsigned long *nmods);

static void get_ref_info(
    struct mach_header *mh,
    struct load_command *load_commands,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    unsigned long object_size,
    struct dylib_reference **refs,
    unsigned long *nrefs);

static void get_indirect_symbol_table_info(
    struct mach_header *mh,
    struct load_command *load_commands,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    unsigned long object_size,
    unsigned long **indirect_symbols,
    unsigned long *nindirect_symbols);

static enum bool get_dyst(
    struct mach_header *mh,
    struct load_command *load_commands,
    enum byte_sex load_commands_byte_sex,
    struct dysymtab_command *dyst);

static void get_hints_table_info(
    struct mach_header *mh,
    struct load_command *load_commands,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    unsigned long object_size,
    struct twolevel_hint **hints,
    unsigned long *nhints);

static enum bool get_hints_cmd(
    struct mach_header *mh,
    struct load_command *load_commands,
    enum byte_sex load_commands_byte_sex,
    struct twolevel_hints_command *hints_cmd);

static int sym_compare(
    struct nlist *sym1,
    struct nlist *sym2);

static int rel_compare(
    struct relocation_info *rel1,
    struct relocation_info *rel2);

static enum bool get_sect_info(
    char *segname,
    char *sectname,
    struct mach_header *mh,
    struct load_command *load_commands,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    unsigned long object_size,
    char **sect_pointer,
    unsigned long *sect_size,
    unsigned long *sect_addr,
    struct relocation_info **sect_relocs,
    unsigned long *sect_nrelocs,
    unsigned long *sect_flags);

static void print_text(
    cpu_type_t cputype,
    enum byte_sex object_byte_sex,
    char *sect,
    unsigned long size,
    unsigned long addr,
    struct nlist *sorted_symbols,
    unsigned long nsorted_symbols,
    struct nlist *symbols,
    unsigned long nsymbols,
    char *strings,
    unsigned long strings_size,
    struct relocation_info *relocs,
    unsigned long nrelocs,
    unsigned long *indirect_symbols,
    unsigned long nindirect_symbols,
    struct mach_header *mh,
    struct load_command *load_commands,
    enum bool disassemble,
    enum bool verbose);

static void print_argstrings(
    struct mach_header *mh,
    struct load_command *load_commands,
    enum byte_sex load_commands_byte_sex,
    char *object_addr,
    unsigned long object_size);

int
main(
int argc,
char **argv,
char **envp)
{
    int i;
    unsigned long j, nfiles;
    struct arch_flag *arch_flags;
    unsigned long narch_flags;
    enum bool all_archs, use_member_syntax;
    char **files;

	progname = argv[0];
	arch_flags = NULL;
	narch_flags = 0;
	all_archs = FALSE;
	use_member_syntax = TRUE;

	if(argc <= 1)
	    usage();

	/*
	 * Parse the arguments.
	 */
	nfiles = 0;
        files = allocate(sizeof(char *) * argc);
	for(i = 1; i < argc; i++){
	    if(argv[i][0] == '-' && argv[i][1] == '\0'){
		for(i += 1 ; i < argc; i++)
		    files[nfiles++] = argv[i];
		break;
	    }
	    if(argv[i][0] != '-'){
		files[nfiles++] = argv[i];
		continue;
	    }
	    if(strcmp(argv[i], "-arch") == 0){
		if(i + 1 == argc){
		    error("missing argument(s) to %s option", argv[i]);
		    usage();
		}
		if(strcmp("all", argv[i+1]) == 0){
		    all_archs = TRUE;
		}
		else{
		    arch_flags = reallocate(arch_flags,
			    (narch_flags + 1) * sizeof(struct arch_flag));
		    if(get_arch_from_flag(argv[i+1],
					  arch_flags + narch_flags) == 0){
			error("unknown architecture specification flag: "
			      "%s %s", argv[i], argv[i+1]);
			arch_usage();
			usage();
		    }
		    narch_flags++;
		}
		i++;
		continue;
	    }
	    if(argv[i][1] == 'p'){
		if(argc <=  i + 1){
		    error("-p requires an argument (a text symbol name)");
		    usage();
		}
		if(pflag)
		    error("only one -p flag can be specified");
		pflag = argv[i + 1];
		i++;
		continue;
	    }
	    if(argv[i][1] == 's'){
		if(argc <=  i + 2){
		    error("-s requires two arguments (a segment name and a "
			  "section name)");
		    usage();
		}
		if(sectname != NULL){
		    error("only one -s flag can be specified");
		    usage();
		}
		segname  = argv[i + 1];
		sectname = argv[i + 2];
		i += 2;
		object_processing = TRUE;
		continue;
	    }
	    for(j = 1; argv[i][j] != '\0'; j++){
		switch(argv[i][j]){
		case 'V':
		    Vflag = TRUE;
		case 'v':
		    vflag = TRUE;
		    break;
		case 'f':
		    fflag = TRUE;
		    break;
		case 'a':
		    aflag = TRUE;
		    break;
		case 'h':
		    hflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'l':
		    lflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'L':
		    Lflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'D':
		    Dflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 't':
		    tflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'd':
		    dflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'o':
		    oflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'O':
		    Oflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'r':
		    rflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'T':
		    Tflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'M':
		    Mflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'R':
		    Rflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'I':
		    Iflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'H':
		    Hflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'S':
		    Sflag = TRUE;
		    break;
		case 'c':
		    cflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'i':
		    iflag = TRUE;
		    object_processing = TRUE;
		    break;
		case 'W':
		    Wflag = TRUE;
		    break;
		case 'X':
		    Xflag = TRUE;
		    break;
		case 'Z':
		    Zflag = TRUE;
		    break;
		case 'm':
		    use_member_syntax = FALSE;
		    break;
		default:
		    error("unknown char `%c' in flag %s\n", argv[i][j],argv[i]);
		    usage();
		}
	    }
	}

	/*
	 * Check for correctness of arguments.
	 */
	if(!fflag && !aflag && !hflag && !lflag && !Lflag && !tflag && !dflag &&
	   !oflag && !Oflag && !rflag && !Tflag && !Mflag && !Rflag && !Iflag &&
	   !Hflag && !Sflag && !cflag && !iflag && !Dflag && !segname){
	    error("one of -fahlLtdoOrTMRIHScis must be specified");
	    usage();
	}
	if(nfiles == 0){
	    error("at least one file must be specified");
	    usage();
	}
	if(segname != NULL && sectname != NULL){
	    /* treat "-s __TEXT __text" the same as -t */
	    if(strcmp(segname, SEG_TEXT) == 0 &&
	       strcmp(sectname, SECT_TEXT) == 0){
		tflag = TRUE;
		segname = NULL;
		sectname = NULL;
	    }
	    /* treat "-s __TEXT __fvmlib0" the same as -i */
	    else if(strcmp(segname, SEG_TEXT) == 0 &&
	       strcmp(sectname, SECT_FVMLIB_INIT0) == 0){
		iflag = TRUE;
		segname = NULL;
		sectname = NULL;
	    }
	}

	for(j = 0; j < nfiles; j++){
	    ofile_process(files[j], arch_flags, narch_flags, all_archs, TRUE,
			  TRUE, use_member_syntax, processor, NULL);
	}

	if(errors)
	    return(EXIT_FAILURE);
	else
	    return(EXIT_SUCCESS);
}

/*
 * Print the current usage message.
 */
static
void
usage(
void)
{
	fprintf(stderr,
		"Usage: %s [-fahlLDtdorSTMRIHvVcXm] <object file> ...\n",
		progname);

	fprintf(stderr, "\t-f print the fat headers\n");
	fprintf(stderr, "\t-a print the archive header\n");
	fprintf(stderr, "\t-h print the mach header\n");
	fprintf(stderr, "\t-l print the load commands\n");
	fprintf(stderr, "\t-L print shared libraries used\n");
	fprintf(stderr, "\t-D print shared library id name\n");
	fprintf(stderr, "\t-t print the text section (disassemble with -v)\n");
	fprintf(stderr, "\t-p <routine name>  start dissassemble from routine "
		"name\n");
	fprintf(stderr, "\t-s <segname> <sectname> print contents of "
		"section\n");
	fprintf(stderr, "\t-d print the data section\n");
	fprintf(stderr, "\t-o print the Objective-C segment\n");
	fprintf(stderr, "\t-r print the relocation entries\n");
	fprintf(stderr, "\t-S print the table of contents of a library\n");
	fprintf(stderr, "\t-T print the table of contents of a dynamic "
		"shared library\n");
	fprintf(stderr, "\t-M print the module table of a dynamic shared "
		"library\n");
	fprintf(stderr, "\t-R print the reference table of a dynamic shared "
		"library\n");
	fprintf(stderr, "\t-I print the indirect symbol table\n");
	fprintf(stderr, "\t-H print the two-level hints table\n");
	fprintf(stderr, "\t-v print verbosely (symbolicly) when possible\n");
	fprintf(stderr, "\t-V print disassembled operands symbolicly\n");
	fprintf(stderr, "\t-c print argument strings of a core file\n");
	fprintf(stderr, "\t-X print no leading addresses or headers\n");
	fprintf(stderr, "\t-m don't use archive(member) syntax\n");
	exit(EXIT_FAILURE);
}

static
void
processor(
struct ofile *ofile,
char *arch_name,
void *cookie) /* cookie is not used */
{
    char *addr;
    unsigned long i, size, magic;
    struct mach_header mh;
    struct load_command *load_commands;
    unsigned long nsymbols, nsorted_symbols, strings_size, len;
    struct nlist *symbols, *allocated_symbols, *sorted_symbols;
    char *strings, *p;
    unsigned char n_type;
    char *sect;
    unsigned long sect_size, sect_addr, sect_nrelocs, sect_flags, nrelocs;
    struct relocation_info *sect_relocs, *relocs;
    unsigned long *indirect_symbols, *allocated_indirect_symbols,
		  nindirect_symbols;
    struct dylib_module *mods, *allocated_mods;
    struct dylib_table_of_contents *tocs, *allocated_tocs;
    struct dylib_reference *refs, *allocated_refs;
    unsigned long nmods, ntocs, nrefs;
    struct twolevel_hint *hints, *allocated_hints;
    unsigned long nhints;

	sorted_symbols = NULL;
	nsorted_symbols = 0;
	indirect_symbols = NULL;
	nindirect_symbols = 0;
	hints = NULL;
	nhints = 0;
	/*
	 * These may or may not be allocated.  If allocated they will not be
	 * NULL and then free'ed before returning.
	 */
	load_commands = NULL;
	allocated_symbols = NULL;
	sorted_symbols = NULL;
	allocated_indirect_symbols = NULL;
	allocated_tocs = NULL;
	allocated_mods = NULL;
	allocated_refs = NULL;
	allocated_hints = NULL;

	/*
	 * The fat headers are printed in ofile_map() in ofile.c #ifdef'ed
	 * OTOOL.
	 */

	/*
	 * Archive headers.
	 */
	if(aflag && ofile->member_ar_hdr != NULL)
	    print_ar_hdr(ofile->member_ar_hdr, ofile->member_name,
			 ofile->member_name_size, vflag);

	/*
	 * Archive table of contents.
	 */
	if(ofile->member_ar_hdr != NULL &&
	   strncmp(ofile->member_name, SYMDEF, sizeof(SYMDEF)-1) == 0){
	    if(Sflag == FALSE)
		return;
	    if(ofile->file_type == OFILE_FAT){
		addr = ofile->file_addr + ofile->fat_archs[ofile->narch].offset;
		size = ofile->fat_archs[ofile->narch].size;
	    }
	    else{
		addr = ofile->file_addr;
		size = ofile->file_size;
	    }
	    if(addr + size > ofile->file_addr + ofile->file_size)
		size = (ofile->file_addr + ofile->file_size) - addr;
	    print_library_toc(ofile->member_ar_hdr, /* toc_ar_hdr */
			      ofile->member_name, /* toc_name */
			      ofile->member_name_size, /* toc_name_size */
			      ofile->member_addr, /* toc_addr */
			      ofile->member_size, /* toc_size */
			      get_toc_byte_sex(addr, size),
			      ofile->file_name, /* library_name */
			      addr, /* library_addr */
			      size, /* library_size */
			      arch_name,
			      vflag);
	    return;
	}

	if(object_processing == FALSE)
	    return;

	/*
	 * Print header for the object name if in an archive or an architecture
	 * name is passed in.
	 */
	if(Xflag == FALSE){
	    printf("%s", ofile->file_name);
	    if(ofile->member_ar_hdr != NULL){
		printf("(%.*s)", (int)ofile->member_name_size,
			ofile->member_name);
	    }
	    if(arch_name != NULL)
		printf(" (architecture %s):", arch_name);
	    else
		printf(":");
	    /*
	     * If the mach_header pointer is NULL the file is not an object
	     * file.  Truncated object file (where the file size is less
	     * than sizeof(struct mach_header)) also does not have it's
	     * mach_header set.  So deal with both cases here and then
	     * return as the rest of this routine deals only with things
	     * in object files.
	     */
	    if(ofile->mh == NULL){
		if(ofile->file_type == OFILE_FAT){
		    /*
		     * This routine is not called on fat files where the
		     * offset is past end of file.  An error message is
		     * printed in ofile_specific_arch() in ofile.c.
		     */
		    if(ofile->arch_type == OFILE_ARCHIVE){
			addr = ofile->member_addr;
			size = ofile->member_size;
		    }
		    else{
			addr = ofile->file_addr +
			       ofile->fat_archs[ofile->narch].offset;
			size = ofile->fat_archs[ofile->narch].size;
		    }
		    if(addr + size > ofile->file_addr + ofile->file_size)
			size = (ofile->file_addr + ofile->file_size) - addr;
		}
		else if(ofile->file_type == OFILE_ARCHIVE){
		    addr = ofile->member_addr;
		    size = ofile->member_size;
		}
		else{ /* ofile->file_type == OFILE_UNKNOWN */
		    addr = ofile->file_addr;
		    size = ofile->file_size;
		}
		if(size > sizeof(long)){
		    memcpy(&magic, addr, sizeof(unsigned long));
		    if(magic == MH_MAGIC || magic == SWAP_LONG(MH_MAGIC)){
			printf(" is a truncated object file\n");
			memset(&mh, '\0', sizeof(struct mach_header));
			if(size > sizeof(struct mach_header))
			    size = sizeof(struct mach_header);
			memcpy(&mh, addr, size);
			if(magic == SWAP_LONG(MH_MAGIC))
			    swap_mach_header(&mh, get_host_byte_sex());
			if(hflag)
			    print_mach_header(&mh, vflag);
			return;
		    }
		}
		printf(" is not an object file\n");
		return;
	    }
	}
	if(ofile->mh != NULL){
	    if((int)(ofile->mh) % sizeof(unsigned long)){
		if(Xflag == FALSE)
		    printf("(object file offset is not a multiple of sizeof("
			   "unsigned long))");
		memcpy(&mh, ofile->mh, sizeof(struct mach_header));
		if(mh.magic == SWAP_LONG(MH_MAGIC))
		    swap_mach_header(&mh, get_host_byte_sex());
		ofile->mh = &mh;
	    }
	    else if(ofile->mh->magic == SWAP_LONG(MH_MAGIC)){
		mh = *(ofile->mh);
		swap_mach_header(&mh, get_host_byte_sex());
		ofile->mh = &mh;
	    }
	}
	if(Xflag == FALSE)
	    printf("\n");

	/*
	 * If this is not an object file then just return.
	 */
	if(ofile->mh == NULL)
	    return;

	/*
	 * Calculate the true number of bytes of the of the object file that
	 * is in memory (in case this file is truncated).
	 */
	addr = ofile->object_addr;
	size = ofile->object_size;
	if(addr + size > ofile->file_addr + ofile->file_size)
	    size = (ofile->file_addr + ofile->file_size) - addr;

	/*
	 * Mach header.
	 */
	if(hflag)
	    print_mach_header(ofile->mh, vflag);

	/*
	 * Load commands.
	 */
	if(ofile->mh->sizeofcmds + sizeof(struct mach_header) > size){
	    load_commands = allocate(ofile->mh->sizeofcmds);
	    memset(load_commands, '\0', ofile->mh->sizeofcmds);
	    memcpy(load_commands, ofile->load_commands, 
		   size - sizeof(struct mach_header));
	    ofile->load_commands = load_commands;
	}
	if(lflag)
	    print_loadcmds(ofile->mh, ofile->load_commands,
			   ofile->object_byte_sex, size, vflag, Vflag);

	if(Lflag || Dflag)
	    print_libraries(ofile->mh, ofile->load_commands,
			    ofile->object_byte_sex, (Dflag && !Lflag), vflag);

	/*
	 * If the indicated operation needs the symbol table get it.
	 */
	sect_flags = 0;
	if(segname != NULL && sectname != NULL){
	    (void)get_sect_info(segname, sectname, ofile->mh,
			ofile->load_commands, ofile->object_byte_sex,
			addr, size, &sect, &sect_size, &sect_addr,
			&sect_relocs, &sect_nrelocs, &sect_flags);
	    /*
	     * The MH_DYLIB_STUB format has all section sizes set to zero 
	     * except sections with indirect symbol table entries (so that the
	     * indirect symbol table table entries can be printed, which are
	     * based on the section size).  So if we are being asked to print
	     * the section contents of one of these sections in a MH_DYLIB_STUB
	     * we assume it has been stripped and set the section size to zero.
	     */
	    if(ofile->mh->filetype == MH_DYLIB_STUB &&
	       ((sect_flags & SECTION_TYPE) == S_NON_LAZY_SYMBOL_POINTERS ||
	        (sect_flags & SECTION_TYPE) == S_LAZY_SYMBOL_POINTERS ||
	        (sect_flags & SECTION_TYPE) == S_SYMBOL_STUBS))
		sect_size = 0;
	}
	if(Rflag || Mflag)
	    get_symbol_table_info(ofile->mh, ofile->load_commands,
		ofile->object_byte_sex, addr, size,
		&symbols, &nsymbols, &strings, &strings_size);
	if(vflag && (rflag || Tflag || Mflag || Rflag || Iflag || Hflag || tflag
	   || iflag || oflag ||
	   (sect_flags & SECTION_TYPE) == S_LITERAL_POINTERS ||
	   (sect_flags & S_ATTR_PURE_INSTRUCTIONS) ==
		S_ATTR_PURE_INSTRUCTIONS ||
	   (sect_flags & S_ATTR_SOME_INSTRUCTIONS) ==
		S_ATTR_SOME_INSTRUCTIONS)){
	    get_symbol_table_info(ofile->mh, ofile->load_commands,
		ofile->object_byte_sex, addr, size,
		&symbols, &nsymbols, &strings, &strings_size);

	    if((int)symbols % sizeof(unsigned long) ||
	       ofile->object_byte_sex != get_host_byte_sex()){
		allocated_symbols = allocate(nsymbols * sizeof(struct nlist));
		memcpy(allocated_symbols, symbols,
		       nsymbols * sizeof(struct nlist));
		symbols = allocated_symbols;
	    }
	    if(ofile->object_byte_sex != get_host_byte_sex())
		swap_nlist(symbols, nsymbols, get_host_byte_sex());

	    /*
	     * If the operation needs a sorted symbol table create it.
	     */
	    if(tflag || iflag || oflag || 
	       (sect_flags & S_ATTR_PURE_INSTRUCTIONS) ==
		    S_ATTR_PURE_INSTRUCTIONS ||
	       (sect_flags & S_ATTR_SOME_INSTRUCTIONS) ==
		    S_ATTR_SOME_INSTRUCTIONS){
		sorted_symbols = allocate(nsymbols * sizeof(struct nlist));
		nsorted_symbols = 0;
		for(i = 0; i < nsymbols; i++){
		    if(symbols[i].n_un.n_strx > 0 &&
			(unsigned long)symbols[i].n_un.n_strx < strings_size)
			p = strings + symbols[i].n_un.n_strx;
		    else
			p = "symbol with bad string index";
		    if(symbols[i].n_type & ~(N_TYPE|N_EXT|N_PEXT))
			continue;
		    n_type = symbols[i].n_type & N_TYPE;
		    if(n_type == N_ABS || n_type == N_SECT){
			len = strlen(p);
			if(len > sizeof(".o") - 1 &&
			   strcmp(p + (len - (sizeof(".o") - 1)), ".o") == 0)
			    continue;
			if(strcmp(p, "gcc_compiled.") == 0)
			    continue;
			if(n_type == N_ABS && symbols[i].n_value == 0 &&
			   *p == '.')
			    continue;
			sorted_symbols[nsorted_symbols] = symbols[i];
			sorted_symbols[nsorted_symbols].n_un.n_name = p;
			nsorted_symbols++;
		    }
		}
		qsort(sorted_symbols, nsorted_symbols, sizeof(struct nlist),
		      (int (*)(const void *, const void *))sym_compare);
	    }
	}

	if(Mflag || Tflag || Rflag){
	    get_module_table_info(ofile->mh, ofile->load_commands,
		ofile->object_byte_sex, addr, size, &mods, &nmods);
	    if((int)mods % sizeof(unsigned long) ||
	       ofile->object_byte_sex != get_host_byte_sex()){
		allocated_mods = allocate(nmods * sizeof(struct dylib_module));
		memcpy(allocated_mods, mods,
		       nmods * sizeof(struct dylib_module));
		mods = allocated_mods;
	    }
	    if(ofile->object_byte_sex != get_host_byte_sex())
		swap_dylib_module(mods, nmods, get_host_byte_sex());
	}

	if(Tflag){
	    get_toc_info(ofile->mh, ofile->load_commands,
		ofile->object_byte_sex, addr, size, &tocs, &ntocs);
	    if((int)tocs % sizeof(unsigned long) ||
	       ofile->object_byte_sex != get_host_byte_sex()){
		allocated_tocs = allocate(ntocs *
					sizeof(struct dylib_table_of_contents));
		memcpy(allocated_tocs, tocs,
		       ntocs * sizeof(struct dylib_table_of_contents));
		tocs = allocated_tocs;
	    }
	    if(ofile->object_byte_sex != get_host_byte_sex())
		swap_dylib_table_of_contents(tocs, ntocs, get_host_byte_sex());
	    print_toc(ofile->mh, ofile->load_commands,
		ofile->object_byte_sex, addr, size, tocs, ntocs, mods, nmods,
		symbols, nsymbols, strings, strings_size, vflag);
	}

	if(Mflag)
	    print_module_table(ofile->mh, ofile->load_commands,
		ofile->object_byte_sex, addr, size, mods, nmods,
		symbols, nsymbols, strings, strings_size, vflag);

	if(Rflag){
	    get_ref_info(ofile->mh, ofile->load_commands,
		ofile->object_byte_sex, addr, size, &refs, &nrefs);
	    if((int)refs % sizeof(unsigned long) ||
	       ofile->object_byte_sex != get_host_byte_sex()){
		allocated_refs = allocate(nrefs *
					sizeof(struct dylib_reference));
		memcpy(allocated_refs, refs,
		       nrefs * sizeof(struct dylib_reference));
		refs = allocated_refs;
	    }
	    if(ofile->object_byte_sex != get_host_byte_sex())
		swap_dylib_reference(refs, nrefs, get_host_byte_sex());
	    print_refs(ofile->mh, ofile->load_commands,
		ofile->object_byte_sex, addr, size, refs, nrefs, mods, nmods,
		symbols, nsymbols, strings, strings_size, vflag);
	}

	if(Iflag || (tflag && vflag)){
	    get_indirect_symbol_table_info(ofile->mh, ofile->load_commands,
		ofile->object_byte_sex, addr, size,
		&indirect_symbols, &nindirect_symbols);
	    if((int)indirect_symbols % sizeof(unsigned long) ||
	       ofile->object_byte_sex != get_host_byte_sex()){
		allocated_indirect_symbols = allocate(nindirect_symbols *
						     sizeof(unsigned long));
		memcpy(allocated_indirect_symbols, indirect_symbols,
		       nindirect_symbols * sizeof(unsigned long));
		indirect_symbols = allocated_indirect_symbols;
	    }
	    if(ofile->object_byte_sex != get_host_byte_sex())
		swap_indirect_symbols(indirect_symbols, nindirect_symbols,
				      get_host_byte_sex());
	}
	if(Hflag){
	    get_hints_table_info(ofile->mh, ofile->load_commands,
		ofile->object_byte_sex, addr, size,
		&hints, &nhints);
	    if((int)hints % sizeof(unsigned long) ||
	       ofile->object_byte_sex != get_host_byte_sex()){
		allocated_hints = allocate(nhints *
					   sizeof(struct twolevel_hint));
		memcpy(allocated_hints, hints,
		       nhints * sizeof(struct twolevel_hint));
		hints = allocated_hints;
	    }
	    if(ofile->object_byte_sex != get_host_byte_sex())
		swap_twolevel_hint(hints, nhints, get_host_byte_sex());
	    print_hints(ofile->mh, ofile->load_commands,
		ofile->object_byte_sex, addr, size, hints,
		nhints, symbols, nsymbols, strings, strings_size, vflag);
	}
	if(Iflag)
	    print_indirect_symbols(ofile->mh, ofile->load_commands,
		ofile->object_byte_sex, addr, size, indirect_symbols,
		nindirect_symbols, symbols, nsymbols, strings, strings_size,
		vflag);

	if(rflag)
	    print_reloc(ofile->mh, ofile->load_commands, ofile->object_byte_sex,
			addr, size, symbols, nsymbols, strings, strings_size,
			vflag);

	if(tflag ||
	   (sect_flags & S_ATTR_PURE_INSTRUCTIONS) ==
		S_ATTR_PURE_INSTRUCTIONS ||
	   (sect_flags & S_ATTR_SOME_INSTRUCTIONS) ==
		S_ATTR_SOME_INSTRUCTIONS){
	    if(tflag)
		(void)get_sect_info(SEG_TEXT, SECT_TEXT, ofile->mh,
		    ofile->load_commands, ofile->object_byte_sex,
		    addr, size, &sect, &sect_size, &sect_addr,
		    &sect_relocs, &sect_nrelocs, &sect_flags);

	    /* create aligned relocations entries as needed */
	    relocs = NULL;
	    nrelocs = 0;
	    if(Vflag){
		if((long)sect_relocs % sizeof(long) != 0 ||
		   ofile->object_byte_sex != get_host_byte_sex()){
		    nrelocs = sect_nrelocs;
		    relocs = allocate(nrelocs *
				      sizeof(struct relocation_info));
		    memcpy(relocs, sect_relocs, nrelocs *
			   sizeof(struct relocation_info));
		}
		else{
		    nrelocs = sect_nrelocs;
		    relocs = sect_relocs;
		}
		if(ofile->object_byte_sex != get_host_byte_sex())
		    swap_relocation_info(relocs, nrelocs,
					 get_host_byte_sex());
	    }
	    if(Xflag == FALSE){
		if(tflag)
		    printf("(%s,%s) section\n", SEG_TEXT, SECT_TEXT);
		else
		    printf("Contents of (%.16s,%.16s) section\n", segname,
			   sectname);
	    }
	    print_text(ofile->mh->cputype, ofile->object_byte_sex,
		       sect, sect_size, sect_addr, sorted_symbols,
		       nsorted_symbols, symbols, nsymbols, strings,
		       strings_size, relocs, nrelocs,
indirect_symbols, nindirect_symbols, ofile->mh, ofile->load_commands,
vflag, Vflag);

	    if(relocs != NULL && relocs != sect_relocs)
		free(relocs);
	}

	if(iflag){
	    if(get_sect_info(SEG_TEXT, SECT_FVMLIB_INIT0, ofile->mh,
		ofile->load_commands, ofile->object_byte_sex,
		addr, size, &sect, &sect_size, &sect_addr,
		&sect_relocs, &sect_nrelocs, &sect_flags) == TRUE){

		/* create aligned, sorted relocations entries */
		nrelocs = sect_nrelocs;
		relocs = allocate(nrelocs * sizeof(struct relocation_info));
		memcpy(relocs, sect_relocs, nrelocs *
		       sizeof(struct relocation_info));
		if(ofile->object_byte_sex != get_host_byte_sex())
		    swap_relocation_info(relocs, nrelocs, get_host_byte_sex());
		qsort(relocs, nrelocs, sizeof(struct relocation_info),
		      (int (*)(const void *, const void *))rel_compare);

		if(Xflag == FALSE)
		    printf("Shared library initialization (%s,%s) section\n",
			   SEG_TEXT, SECT_FVMLIB_INIT0);
		print_shlib_init(ofile->object_byte_sex, sect, sect_size,
			sect_addr, sorted_symbols, nsorted_symbols, symbols,
			nsymbols, strings, strings_size, relocs, nrelocs,vflag);
		free(relocs);
	    }
	}

	if(dflag){
	    if(get_sect_info(SEG_DATA, SECT_DATA, ofile->mh,
		ofile->load_commands, ofile->object_byte_sex,
		addr, size, &sect, &sect_size, &sect_addr,
		&sect_relocs, &sect_nrelocs, &sect_flags) == TRUE){

		if(Xflag == FALSE)
		    printf("(%s,%s) section\n", SEG_DATA, SECT_DATA);
		print_sect(ofile->mh->cputype, ofile->object_byte_sex,
			   sect, sect_size, sect_addr);
	    }
	}

	if(segname != NULL && sectname != NULL &&
	   (sect_flags & S_ATTR_PURE_INSTRUCTIONS) !=
		S_ATTR_PURE_INSTRUCTIONS &&
	   (sect_flags & S_ATTR_SOME_INSTRUCTIONS) !=
		S_ATTR_SOME_INSTRUCTIONS){
	    if(strcmp(segname, SEG_OBJC) == 0 &&
	       strcmp(sectname, "__protocol") == 0 && vflag == TRUE){
		print_objc_protocol_section(ofile->mh, ofile->load_commands,
		   ofile->object_byte_sex, ofile->object_addr,
		   ofile->object_size, vflag);
	    }
	    else if(strcmp(segname, SEG_OBJC) == 0 &&
	            (strcmp(sectname, "__string_object") == 0 ||
	             strcmp(sectname, "__cstring_object") == 0) &&
		    vflag == TRUE){
		print_objc_string_object_section(sectname, ofile->mh,
		   ofile->load_commands, ofile->object_byte_sex,
		   ofile->object_addr, ofile->object_size, vflag);
	    }
	    else if(strcmp(segname, SEG_OBJC) == 0 &&
	       strcmp(sectname, "__runtime_setup") == 0 && vflag == TRUE){
		print_objc_runtime_setup_section(ofile->mh,ofile->load_commands,
		   ofile->object_byte_sex, ofile->object_addr,
		   ofile->object_size, vflag);
	    }
	    else if(get_sect_info(segname, sectname, ofile->mh,
		ofile->load_commands, ofile->object_byte_sex,
		addr, size, &sect, &sect_size, &sect_addr,
		&sect_relocs, &sect_nrelocs, &sect_flags) == TRUE){

		if(Xflag == FALSE)
		    printf("Contents of (%.16s,%.16s) section\n", segname,
			   sectname);

		if(vflag){
		    switch((sect_flags & SECTION_TYPE)){
		    case 0:
			print_sect(ofile->mh->cputype, ofile->object_byte_sex,
				   sect, sect_size, sect_addr);
			break;
		    case S_ZEROFILL:
			printf("zerofill section and has no contents in the "
			       "file\n");
			break;
		    case S_CSTRING_LITERALS:
			print_cstring_section(sect, sect_size, sect_addr,
				Xflag == TRUE ? FALSE : TRUE);
			break;
		    case S_4BYTE_LITERALS:
			print_literal4_section(sect, sect_size, sect_addr,
					      ofile->object_byte_sex,
					      Xflag == TRUE ? FALSE : TRUE);
			break;
		    case S_8BYTE_LITERALS:
			print_literal8_section(sect, sect_size, sect_addr,
					      ofile->object_byte_sex,
					      Xflag == TRUE ? FALSE : TRUE);
			break;
		    case S_LITERAL_POINTERS:
			/* create aligned, sorted relocations entries */
			nrelocs = sect_nrelocs;
			relocs = allocate(nrelocs *
					  sizeof(struct relocation_info));
			memcpy(relocs, sect_relocs, nrelocs *
			       sizeof(struct relocation_info));
			if(ofile->object_byte_sex != get_host_byte_sex())
			    swap_relocation_info(relocs, nrelocs,
					         get_host_byte_sex());
			qsort(relocs, nrelocs, sizeof(struct relocation_info),
			      (int (*)(const void *, const void *))rel_compare);
			print_literal_pointer_section(ofile->mh,
				ofile->load_commands, ofile->object_byte_sex,
				addr, size, sect, sect_size, sect_addr,
			   	symbols, nsymbols, strings, strings_size,
				relocs, nrelocs, Xflag == TRUE ? FALSE : TRUE);
			free(relocs);

			break;
		    default:
			printf("Unknown section type (0x%x)\n",
			       (unsigned int)(sect_flags & SECTION_TYPE));
			print_sect(ofile->mh->cputype, ofile->object_byte_sex,
				   sect, sect_size, sect_addr);
			break;
		    }
		}
		else{
		    if((sect_flags & SECTION_TYPE) == S_ZEROFILL)
			printf("zerofill section and has no contents in the "
			       "file\n");
		    else
			print_sect(ofile->mh->cputype, ofile->object_byte_sex,
				   sect, sect_size, sect_addr);
		}
	    }
	}

	if(cflag)
	    print_argstrings(ofile->mh, ofile->load_commands,
			     ofile->object_byte_sex, ofile->object_addr,
			     ofile->object_size);

	if(oflag)
	    print_objc_segment(ofile->mh, ofile->load_commands,
			       ofile->object_byte_sex, ofile->object_addr,
			       ofile->object_size, sorted_symbols,
			       nsorted_symbols, vflag);

	if(load_commands != NULL)
	    free(load_commands);
	if(allocated_symbols != NULL)
	    free(allocated_symbols);
	if(sorted_symbols != NULL)
	    free(sorted_symbols);
	if(allocated_indirect_symbols != NULL)
	    free(allocated_indirect_symbols);
	if(allocated_hints != NULL)
	    free(allocated_hints);
	if(allocated_tocs != NULL)
	    free(allocated_tocs);
	if(allocated_mods != NULL)
	    free(allocated_mods);
	if(allocated_refs != NULL)
	    free(allocated_refs);
}

/*
 * get_symbol_table_info() returns pointers to the symbol table and string
 * table as well as the number of symbols and size of the string table.
 * This routine handles the problems related to the file being truncated and
 * only returns valid pointers and sizes that can be used.  This routine will
 * return pointers that are misaligned and it is up to the caller to deal with
 * alignment issues.  It is also up to the caller to deal with byte sex of the
 * the symbol table.
 */
static
void
get_symbol_table_info(
struct mach_header *mh,			/* input */
struct load_command *load_commands,
enum byte_sex load_commands_byte_sex,
char *object_addr,
unsigned long object_size,
struct nlist **symbols,			/* output */
unsigned long *nsymbols,
char **strings,
unsigned long *strings_size)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    unsigned long i, left, size, st_cmd;
    struct load_command *lc, l;
    struct symtab_command st;

	*symbols = NULL;
	*nsymbols = 0;
	*strings = NULL;
	*strings_size = 0;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != load_commands_byte_sex;

	st_cmd = ULONG_MAX;
	lc = load_commands;
	for(i = 0 ; i < mh->ncmds; i++){
	    memcpy((char *)&l, (char *)lc, sizeof(struct load_command));
	    if(swapped)
		swap_load_command(&l, host_byte_sex);
	    if(l.cmdsize % sizeof(long) != 0)
		printf("load command %lu size not a multiple of "
		       "sizeof(long)\n", i);
	    if((char *)lc + l.cmdsize >
	       (char *)load_commands + mh->sizeofcmds)
		printf("load command %lu extends past end of load "
		       "commands\n", i);
	    left = mh->sizeofcmds - ((char *)lc - (char *)load_commands);

	    switch(l.cmd){
	    case LC_SYMTAB:
		if(st_cmd != ULONG_MAX){
		    printf("more than one LC_SYMTAB command (using command %lu)"
			   "\n", st_cmd);
		    break;
		}
		memset((char *)&st, '\0', sizeof(struct symtab_command));
		size = left < sizeof(struct symtab_command) ?
		       left : sizeof(struct symtab_command);
		memcpy((char *)&st, (char *)lc, size);
		if(swapped)
		    swap_symtab_command(&st, host_byte_sex);
		st_cmd = i;
	    }
	    if(l.cmdsize == 0){
		printf("load command %lu size zero (can't advance to other "
		       "load commands)\n", i);
		break;
	    }
	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	    if((char *)lc > (char *)load_commands + mh->sizeofcmds)
		break;
	}
	if((char *)load_commands + mh->sizeofcmds != (char *)lc)
	    printf("Inconsistent mh_sizeofcmds\n");

	if(st_cmd == ULONG_MAX){
	    return;
	}

	if(st.symoff >= object_size){
	    printf("symbol table offset is past end of file\n");
	}
	else{
	    *symbols = (struct nlist *)(object_addr + st.symoff);
	    if(st.symoff + st.nsyms * sizeof(struct nlist) > object_size){
		printf("symbol table extends past end of file\n");
		*nsymbols = (object_size - st.symoff) / sizeof(struct nlist);
	    }
	    else
		*nsymbols = st.nsyms;
	}

	if(st.stroff >= object_size){
	    printf("string table offset is past end of file\n");
	}
	else{
	    *strings = object_addr + st.stroff;
	    if(st.stroff + st.strsize > object_size){
		printf("string table extends past end of file\n");
		*strings_size = object_size - st.symoff;
	    }
	    else
		*strings_size = st.strsize;
	}
}

/*
 * get_toc_info() returns a pointer and the size of the table of contents.
 * This routine handles the problems related to the file being truncated and
 * only returns valid pointers and sizes that can be used.  This routine will
 * return pointers that are misaligned and it is up to the caller to deal with
 * alignment issues.  It is also up to the caller to deal with byte sex of the
 * table.
 */
static
void
get_toc_info(
struct mach_header *mh,			/* input */
struct load_command *load_commands,
enum byte_sex load_commands_byte_sex,
char *object_addr,
unsigned long object_size,
struct dylib_table_of_contents **tocs,	/* output */
unsigned long *ntocs)
{
    struct dysymtab_command dyst;

	*tocs = NULL;
	*ntocs = 0;

	if(get_dyst(mh, load_commands, load_commands_byte_sex, &dyst) == FALSE)
	    return;

	if(dyst.tocoff >= object_size){
	    printf("table of contents offset is past end of file\n");
	}
	else{
	    *tocs = (struct dylib_table_of_contents *)(object_addr +
						       dyst.tocoff);
	    if(dyst.tocoff + dyst.ntoc *
	       sizeof(struct dylib_table_of_contents) > object_size){
		printf("table of contents extends past end of file\n");
		*ntocs = (object_size - dyst.tocoff) /
				     sizeof(struct dylib_table_of_contents);
	    }
	    else
		*ntocs = dyst.ntoc;
	}
}

/*
 * get_module_table_info() returns a pointer and the size of the
 * module table.  This routine handles the problems related to the file being
 * truncated and only returns valid pointers and sizes that can be used.  This
 * routine will return pointers that are misaligned and it is up to the caller
 * to deal with alignment issues.  It is also up to the caller to deal with
 * byte sex of the table.
 */
static
void
get_module_table_info(
struct mach_header *mh,			/* input */
struct load_command *load_commands,
enum byte_sex load_commands_byte_sex,
char *object_addr,
unsigned long object_size,
struct dylib_module **mods,		/* output */
unsigned long *nmods)
{
    struct dysymtab_command dyst;

	*mods = NULL;
	*nmods = 0;

	if(get_dyst(mh, load_commands, load_commands_byte_sex, &dyst) == FALSE)
	    return;

	if(dyst.modtaboff >= object_size){
	    printf("module table offset is past end of file\n");
	}
	else{
	    *mods = (struct dylib_module *)(object_addr + dyst.modtaboff);
	    if(dyst.modtaboff +
	       dyst.nmodtab * sizeof(struct dylib_module) > object_size){
		printf("module table extends past end of file\n");
		*nmods = (object_size - dyst.modtaboff) /
				     sizeof(struct dylib_module);
	    }
	    else
		*nmods = dyst.nmodtab;
	}
}

/*
 * get_ref_info() returns a pointer and the size of the reference table.
 * This routine handles the problems related to the file being truncated and
 * only returns valid pointers and sizes that can be used.  This routine will
 * return pointers that are misaligned and it is up to the caller to deal with
 * alignment issues.  It is also up to the caller to deal with byte sex of the
 * table.
 */
static
void
get_ref_info(
struct mach_header *mh,			/* input */
struct load_command *load_commands,
enum byte_sex load_commands_byte_sex,
char *object_addr,
unsigned long object_size,
struct dylib_reference **refs,		/* output */
unsigned long *nrefs)
{
    struct dysymtab_command dyst;

	*refs = NULL;
	*nrefs = 0;

	if(get_dyst(mh, load_commands, load_commands_byte_sex, &dyst) == FALSE)
	    return;

	if(dyst.extrefsymoff >= object_size){
	    printf("reference table offset is past end of file\n");
	}
	else{
	    *refs = (struct dylib_reference *)(object_addr + dyst.extrefsymoff);
	    if(dyst.extrefsymoff + dyst.nextrefsyms *
	       sizeof(struct dylib_reference) > object_size){
		printf("reference table extends past end of file\n");
		*nrefs = (object_size - dyst.extrefsymoff) /
				     sizeof(struct dylib_reference);
	    }
	    else
		*nrefs = dyst.nextrefsyms;
	}
}

/*
 * get_indirect_symbol_table_info() returns a pointer and the size of the
 * indirect symbol table.  This routine handles the problems related to the
 * file being truncated and only returns valid pointers and sizes that can be
 * used.  This routine will return pointers that are misaligned and it is up to
 * the caller to deal with alignment issues.  It is also up to the caller to
 * deal with byte sex of the table.
 */
static
void
get_indirect_symbol_table_info(
struct mach_header *mh,			/* input */
struct load_command *load_commands,
enum byte_sex load_commands_byte_sex,
char *object_addr,
unsigned long object_size,
unsigned long **indirect_symbols,	/* output */
unsigned long *nindirect_symbols)
{
    struct dysymtab_command dyst;

	*indirect_symbols = NULL;
	*nindirect_symbols = 0;

	if(get_dyst(mh, load_commands, load_commands_byte_sex, &dyst) == FALSE)
	    return;

	if(dyst.indirectsymoff >= object_size){
	    printf("indirect symbol table offset is past end of file\n");
	}
	else{
	    *indirect_symbols = (unsigned long *)(object_addr +
						  dyst.indirectsymoff);
	    if(dyst.indirectsymoff +
	       dyst.nindirectsyms * sizeof(unsigned long) > object_size){
		printf("indirect symbol table extends past end of file\n");
		*nindirect_symbols = (object_size - dyst.indirectsymoff) /
				     sizeof(unsigned long);
	    }
	    else
		*nindirect_symbols = dyst.nindirectsyms;
	}
}

/*
 * get_dyst() gets the dysymtab_command from the mach header and load commands
 * passed to it and copys it into dyst.  It if doesn't find one it returns FALSE
 * else it returns TRUE.
 */
static
enum bool
get_dyst(
struct mach_header *mh,
struct load_command *load_commands,
enum byte_sex load_commands_byte_sex,
struct dysymtab_command *dyst)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    unsigned long i, left, size, dyst_cmd;
    struct load_command *lc, l;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != load_commands_byte_sex;

	dyst_cmd = ULONG_MAX;
	lc = load_commands;
	for(i = 0 ; i < mh->ncmds; i++){
	    memcpy((char *)&l, (char *)lc, sizeof(struct load_command));
	    if(swapped)
		swap_load_command(&l, host_byte_sex);
	    if(l.cmdsize % sizeof(long) != 0)
		printf("load command %lu size not a multiple of "
		       "sizeof(long)\n", i);
	    if((char *)lc + l.cmdsize >
	       (char *)load_commands + mh->sizeofcmds)
		printf("load command %lu extends past end of load "
		       "commands\n", i);
	    left = mh->sizeofcmds - ((char *)lc - (char *)load_commands);

	    switch(l.cmd){
	    case LC_DYSYMTAB:
		if(dyst_cmd != ULONG_MAX){
		    printf("more than one LC_DYSYMTAB command (using command "
			   "%lu)\n", dyst_cmd);
		    break;
		}
		memset((char *)dyst, '\0', sizeof(struct dysymtab_command));
		size = left < sizeof(struct dysymtab_command) ?
		       left : sizeof(struct dysymtab_command);
		memcpy((char *)dyst, (char *)lc, size);
		if(swapped)
		    swap_dysymtab_command(dyst, host_byte_sex);
		dyst_cmd = i;
	    }
	    if(l.cmdsize == 0){
		printf("load command %lu size zero (can't advance to other "
		       "load commands)\n", i);
		break;
	    }
	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	    if((char *)lc > (char *)load_commands + mh->sizeofcmds)
		break;
	}
	if((char *)load_commands + mh->sizeofcmds != (char *)lc)
	    printf("Inconsistent mh_sizeofcmds\n");

	if(dyst_cmd == ULONG_MAX){
	    return(FALSE);
	}
	return(TRUE);
}

/*
 * get_hints_table_info() returns a pointer and the size of the two-level hints
 * table.  This routine handles the problems related to the file being truncated
 * and only returns valid pointers and sizes that can be used.  This routine
 * will return pointers that are misaligned and it is up to the caller to deal
 * with alignment issues.  It is also up to the caller to deal with byte sex of
 * the table.
 */
static
void
get_hints_table_info(
struct mach_header *mh,			/* input */
struct load_command *load_commands,
enum byte_sex load_commands_byte_sex,
char *object_addr,
unsigned long object_size,
struct twolevel_hint **hints,	/* output */
unsigned long *nhints)
{
    struct twolevel_hints_command hints_cmd;

	*hints = NULL;
	*nhints = 0;

	if(get_hints_cmd(mh, load_commands, load_commands_byte_sex,
		         &hints_cmd) == FALSE)
	    return;

	if(hints_cmd.offset >= object_size){
	    printf("two-level hints offset is past end of file\n");
	}
	else{
	    *hints = (struct twolevel_hint *)(object_addr + hints_cmd.offset);
	    if(hints_cmd.offset +
	       hints_cmd.nhints * sizeof(struct twolevel_hint) > object_size){
		printf("two-level hints table extends past end of file\n");
		*nhints = (object_size - hints_cmd.offset) /
			  sizeof(struct twolevel_hint);
	    }
	    else
		*nhints = hints_cmd.nhints;
	}
}

/*
 * get_hints_cmd() gets the twolevel_hints_command from the mach header and
 * load commands passed to it and copys it into hints_cmd.  It if doesn't find
 * one it returns FALSE else it returns TRUE.
 */
static
enum bool
get_hints_cmd(
struct mach_header *mh,
struct load_command *load_commands,
enum byte_sex load_commands_byte_sex,
struct twolevel_hints_command *hints_cmd)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    unsigned long i, left, size, cmd;
    struct load_command *lc, l;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != load_commands_byte_sex;

	cmd = ULONG_MAX;
	lc = load_commands;
	for(i = 0 ; i < mh->ncmds; i++){
	    memcpy((char *)&l, (char *)lc, sizeof(struct load_command));
	    if(swapped)
		swap_load_command(&l, host_byte_sex);
	    if(l.cmdsize % sizeof(long) != 0)
		printf("load command %lu size not a multiple of "
		       "sizeof(long)\n", i);
	    if((char *)lc + l.cmdsize >
	       (char *)load_commands + mh->sizeofcmds)
		printf("load command %lu extends past end of load "
		       "commands\n", i);
	    left = mh->sizeofcmds - ((char *)lc - (char *)load_commands);

	    switch(l.cmd){
	    case LC_TWOLEVEL_HINTS:
		if(cmd != ULONG_MAX){
		    printf("more than one LC_TWOLEVEL_HINTS command (using "
			   "command %lu)\n", cmd);
		    break;
		}
		memset((char *)hints_cmd, '\0',
		       sizeof(struct twolevel_hints_command));
		size = left < sizeof(struct twolevel_hints_command) ?
		       left : sizeof(struct twolevel_hints_command);
		memcpy((char *)hints_cmd, (char *)lc, size);
		if(swapped)
		    swap_twolevel_hints_command(hints_cmd, host_byte_sex);
		cmd = i;
	    }
	    if(l.cmdsize == 0){
		printf("load command %lu size zero (can't advance to other "
		       "load commands)\n", i);
		break;
	    }
	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	    if((char *)lc > (char *)load_commands + mh->sizeofcmds)
		break;
	}
	if((char *)load_commands + mh->sizeofcmds != (char *)lc)
	    printf("Inconsistent mh_sizeofcmds\n");

	if(cmd == ULONG_MAX){
	    return(FALSE);
	}
	return(TRUE);
}


/*
 * Function for qsort for comparing symbols.
 */
static
int
sym_compare(
struct nlist *sym1,
struct nlist *sym2)
{
	if(sym1->n_value == sym2->n_value)
	    return(0);
	if(sym1->n_value < sym2->n_value)
	    return(-1);
	else
	    return(1);
}

/*
 * Function for qsort for comparing relocation entries.
 */
static
int
rel_compare(
struct relocation_info *rel1,
struct relocation_info *rel2)
{
    struct scattered_relocation_info *srel;
    unsigned long r_address1, r_address2;

	if((rel1->r_address & R_SCATTERED) != 0){
	    srel = (struct scattered_relocation_info *)rel1;
	    r_address1 = srel->r_address;
	}
	else
	    r_address1 = rel1->r_address;
	if((rel2->r_address & R_SCATTERED) != 0){
	    srel = (struct scattered_relocation_info *)rel2;
	    r_address2 = srel->r_address;
	}
	else
	    r_address2 = rel2->r_address;

	if(r_address1 == r_address2)
	    return(0);
	if(r_address1 < r_address2)
	    return(-1);
	else
	    return(1);
}

static
enum bool
get_sect_info(
char *segname,				/* input */
char *sectname,
struct mach_header *mh,
struct load_command *load_commands,
enum byte_sex load_commands_byte_sex,
char *object_addr,
unsigned long object_size,
char **sect_pointer,			/* output */
unsigned long *sect_size,
unsigned long *sect_addr,
struct relocation_info **sect_relocs,
unsigned long *sect_nrelocs,
unsigned long *sect_flags)
{
    enum byte_sex host_byte_sex;
    enum bool found, swapped;
    unsigned long i, j, left, size;
    struct load_command *lc, l;
    struct segment_command sg;
    struct section s;
    char *p;

	*sect_pointer = NULL;
	*sect_size = 0;
	*sect_addr = 0;
	*sect_relocs = NULL;
	*sect_nrelocs = 0;
	*sect_flags = 0;

	found = FALSE;
	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != load_commands_byte_sex;

	lc = load_commands;
	for(i = 0 ; found == FALSE && i < mh->ncmds; i++){
	    memcpy((char *)&l, (char *)lc, sizeof(struct load_command));
	    if(swapped)
		swap_load_command(&l, host_byte_sex);
	    if(l.cmdsize % sizeof(long) != 0)
		printf("load command %lu size not a multiple of "
		       "sizeof(long)\n", i);
	    if((char *)lc + l.cmdsize >
	       (char *)load_commands + mh->sizeofcmds)
		printf("load command %lu extends past end of load "
		       "commands\n", i);
	    left = mh->sizeofcmds - ((char *)lc - (char *)load_commands);

	    switch(l.cmd){
	    case LC_SEGMENT:
		memset((char *)&sg, '\0', sizeof(struct segment_command));
		size = left < sizeof(struct segment_command) ?
		       left : sizeof(struct segment_command);
		memcpy((char *)&sg, (char *)lc, size);
		if(swapped)
		    swap_segment_command(&sg, host_byte_sex);

		if((mh->filetype == MH_OBJECT && sg.segname[0] == '\0') ||
		   strncmp(sg.segname, segname, sizeof(sg.segname)) == 0){

		    p = (char *)lc + sizeof(struct segment_command);
		    for(j = 0 ; found == FALSE && j < sg.nsects ; j++){
			if(p + sizeof(struct section) >
			   (char *)load_commands + mh->sizeofcmds){
			    printf("section structure command extends past "
				   "end of load commands\n");
			}
			left = mh->sizeofcmds - (p - (char *)load_commands);
			memset((char *)&s, '\0', sizeof(struct section));
			size = left < sizeof(struct section) ?
			       left : sizeof(struct section);
			memcpy((char *)&s, p, size);
			if(swapped)
			    swap_section(&s, 1, host_byte_sex);

			if(strncmp(s.sectname, sectname,
				   sizeof(s.sectname)) == 0 &&
			   strncmp(s.segname, segname,
				   sizeof(s.segname)) == 0){
			    found = TRUE;
			    break;
			}

			if(p + sizeof(struct section) >
			   (char *)load_commands + mh->sizeofcmds)
			    return(FALSE);
			p += size;
		    }
		}
		break;
	    }
	    if(l.cmdsize == 0){
		printf("load command %lu size zero (can't advance to other "
		       "load commands)\n", i);
		break;
	    }
	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	    if((char *)lc > (char *)load_commands + mh->sizeofcmds)
		break;
	}
	if(found == FALSE)
	    return(FALSE);

	if((s.flags & SECTION_TYPE) == S_ZEROFILL){
	    *sect_pointer = NULL;
	    *sect_size = s.size;
	}
	else{
	    if(s.offset >= object_size){
		printf("section offset for section (%.16s,%.16s) is past end "
		       "of file\n", s.segname, s.sectname);
	    }
	    else{
		*sect_pointer = object_addr + s.offset;
		if(s.offset + s.size > object_size){
		    printf("section (%.16s,%.16s) extends past end of file\n",
			   s.segname, s.sectname);
		    *sect_size = object_size - s.offset;
		}
		else
		    *sect_size = s.size;
	    }
	}
	if(s.reloff >= object_size){
	    printf("relocation entries offset for (%.16s,%.16s): is past end "
		   "of file\n", s.segname, s.sectname);
	}
	else{
	    *sect_relocs = (struct relocation_info *)(object_addr + s.reloff);
	    if(s.reloff + s.nreloc * sizeof(struct relocation_info) >
								object_size){
		printf("relocation entries for section (%.16s,%.16s) extends "
		       "past end of file\n", s.segname, s.sectname);
		*sect_nrelocs = (object_size - s.reloff) /
				sizeof(struct relocation_info);
	    }
	    else
		*sect_nrelocs = s.nreloc;
	}
	*sect_addr = s.addr;
	*sect_flags = s.flags;
	return(TRUE);
}

static
void
print_text(
cpu_type_t cputype,
enum byte_sex object_byte_sex,
char *sect,
unsigned long size,
unsigned long addr,
struct nlist *sorted_symbols,
unsigned long nsorted_symbols,
struct nlist *symbols,
unsigned long nsymbols,
char *strings,
unsigned long strings_size,
struct relocation_info *relocs,
unsigned long nrelocs,
unsigned long *indirect_symbols,
unsigned long nindirect_symbols,
struct mach_header *mh,
struct load_command *load_commands,
enum bool disassemble,
enum bool verbose)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    unsigned long i, j, offset, cur_addr, long_word;
    unsigned short short_word;
    unsigned char byte_word;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != object_byte_sex;

	if(disassemble == TRUE){
	    if(pflag){
		for(i = 0; i < nsorted_symbols; i++){
		    if(strcmp(sorted_symbols[i].n_un.n_name, pflag) == 0)
			break;
		}
		if(i == nsorted_symbols){
		    printf("Can't find -p symbol: %s\n", pflag);
		    return;
		}
		if(sorted_symbols[i].n_value < addr ||
		   sorted_symbols[i].n_value >= addr + size){
		    printf("-p symbol: %s not in text section\n", pflag);
		    return;
		}
		offset = sorted_symbols[i].n_value - addr;
		sect += offset;
		cur_addr = sorted_symbols[i].n_value;
	    }
	    else{
		offset = 0;
		cur_addr = addr;
	    }
	    for(i = offset ; i < size ; ){
		print_label(cur_addr, TRUE, sorted_symbols, nsorted_symbols);
		if(Xflag)
		    printf("\t");
		else
		    printf("%08x\t", (unsigned int)cur_addr);
	 	if(cputype == CPU_TYPE_MC680x0)
		    j = m68k_disassemble(sect, size - i, cur_addr, addr,
				object_byte_sex, relocs, nrelocs, symbols,
				nsymbols, sorted_symbols, nsorted_symbols,
				strings, strings_size, indirect_symbols,
				nindirect_symbols, mh, load_commands, verbose);
		else if(cputype == CPU_TYPE_I860)
		    j = i860_disassemble(sect, size - i, cur_addr, addr,
				object_byte_sex, relocs, nrelocs, symbols,
				nsymbols, sorted_symbols, nsorted_symbols,
				strings, strings_size, verbose);
		else if(cputype == CPU_TYPE_I386)
		    j = i386_disassemble(sect, size - i, cur_addr, addr,
				object_byte_sex, relocs, nrelocs, symbols,
				nsymbols, sorted_symbols, nsorted_symbols,
				strings, strings_size, indirect_symbols,
				nindirect_symbols, mh, load_commands, verbose);
		else if(cputype == CPU_TYPE_MC88000)
		    j = m88k_disassemble(sect, size - i, cur_addr, addr,
				object_byte_sex, relocs, nrelocs, symbols,
				nsymbols, sorted_symbols, nsorted_symbols,
				strings, strings_size, verbose);
		else if(cputype == CPU_TYPE_POWERPC ||
			cputype == CPU_TYPE_VEO)
		    j = ppc_disassemble(sect, size - i, cur_addr, addr,
				object_byte_sex, relocs, nrelocs, symbols,
				nsymbols, sorted_symbols, nsorted_symbols,
				strings, strings_size, indirect_symbols,
				nindirect_symbols, mh, load_commands, verbose);
		else if(cputype == CPU_TYPE_HPPA)
		    j = hppa_disassemble(sect, size - i, cur_addr, addr,
				object_byte_sex, relocs, nrelocs, symbols,
				nsymbols, sorted_symbols, nsorted_symbols,
				strings, strings_size, verbose);
		else if(cputype == CPU_TYPE_SPARC)
		    j = sparc_disassemble(sect, size - i, cur_addr, addr,
				object_byte_sex, relocs, nrelocs, symbols,
				nsymbols, sorted_symbols, nsorted_symbols,
				strings, strings_size, indirect_symbols,
				nindirect_symbols, mh, load_commands, verbose);
		else{
		    printf("Can't disassemble unknown cputype %d\n", cputype);
		    return;
		}
		sect += j;
		cur_addr += j;
		i += j;
	    }
	}
	else{
	    if(cputype == CPU_TYPE_I386){
		for(i = 0 ; i < size ; i += j , addr += j){
		    printf("%08x ", (unsigned int)addr);
		    for(j = 0;
			j < 16 * sizeof(char) && i + j < size;
			j += sizeof(char)){
			byte_word = *(sect + i + j);
			printf("%02x ", (unsigned int)byte_word);
		    }
		    printf("\n");
		}
	    }
	    else if(cputype == CPU_TYPE_MC680x0){
		for(i = 0 ; i < size ; i += j , addr += j){
		    printf("%08x ", (unsigned int)addr);
		    for(j = 0;
			j < 8 * sizeof(short) && i + j < size;
			j += sizeof(short)){
			memcpy(&short_word, sect + i + j, sizeof(short));
			if(swapped)
			    short_word = SWAP_SHORT(short_word);
			printf("%04x ", (unsigned int)short_word);
		    }
		    printf("\n");
		}
	    }
	    else{
		for(i = 0 ; i < size ; i += j , addr += j){
		    printf("%08x ", (unsigned int)addr);
		    for(j = 0;
			j < 4 * sizeof(long) && i + j < size;
			j += sizeof(long)){
			memcpy(&long_word, sect + i + j, sizeof(long));
			if(swapped)
			    long_word = SWAP_LONG(long_word);
			printf("%08x ", (unsigned int)long_word);
		    }
		    printf("\n");
		}
	    }
	}
}

static
void
print_argstrings(
struct mach_header *mh,
struct load_command *load_commands,
enum byte_sex load_commands_byte_sex,
char *object_addr,
unsigned long object_size)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    unsigned long i, len, left, size, usrstack, arg;
    struct load_command *lc, l;
    struct segment_command sg;
    char *stack, *stack_top, *p, *q, *argv;
    struct arch_flag arch_flags;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != load_commands_byte_sex;

	arch_flags.cputype = mh->cputype;
	arch_flags.cpusubtype = mh->cpusubtype;
	usrstack = get_stack_addr_from_flag(&arch_flags);
	if(usrstack == 0){
	    printf("Don't know the value of USRSTACK for unknown cputype "
		   "(%d)\n", mh->cputype);
	    return;
	}
	printf("Argument strings on the stack at: 0x%x\n",
	       (unsigned int)usrstack);
	lc = load_commands;
	for(i = 0 ; i < mh->ncmds; i++){
	    memcpy((char *)&l, (char *)lc, sizeof(struct load_command));
	    if(swapped)
		swap_load_command(&l, host_byte_sex);
	    if(l.cmdsize % sizeof(long) != 0)
		printf("load command %lu size not a multiple of "
		       "sizeof(long)\n", i);
	    if((char *)lc + l.cmdsize >
	       (char *)load_commands + mh->sizeofcmds)
		printf("load command %lu extends past end of load "
		       "commands\n", i);
	    left = mh->sizeofcmds - ((char *)lc - (char *)load_commands);

	    switch(l.cmd){
	    case LC_SEGMENT:
		memset((char *)&sg, '\0', sizeof(struct segment_command));
		size = left < sizeof(struct segment_command) ?
		       left : sizeof(struct segment_command);
		memcpy((char *)&sg, (char *)lc, size);
		if(swapped)
		    swap_segment_command(&sg, host_byte_sex);

		if(mh->cputype == CPU_TYPE_HPPA){
		    if(sg.vmaddr == usrstack &&
		       sg.fileoff + sg.filesize <= object_size){
			stack = object_addr + sg.fileoff;
			stack_top = stack + sg.filesize;
			/*
			 * There appears to be some pointer then argc at the
			 * bottom of the stack first before argv[].
			 */
			argv = stack + 2 * sizeof(unsigned long);
			memcpy(&arg, argv, sizeof(unsigned long));
			if(swapped)
			    arg = SWAP_LONG(arg);
			while(argv < stack_top &&
			      arg != 0 &&
			      arg >= usrstack && arg < usrstack + sg.filesize){
			    p = stack + (arg - usrstack);
			    printf("\t");
			    while(p < stack_top && *p != '\0'){
				printf("%c", *p);
				p++;
			    }
			    printf("\n");
			    argv += sizeof(unsigned long);
			    memcpy(&arg, argv, sizeof(unsigned long));
			    if(swapped)
				arg = SWAP_LONG(arg);
			}
			/* after argv[] then there is envp[] */
			argv += sizeof(unsigned long);
			memcpy(&arg, argv, sizeof(unsigned long));
			if(swapped)
			    arg = SWAP_LONG(arg);
			while(argv < stack_top &&
			      arg != 0 &&
			      arg >= usrstack && arg < usrstack + sg.filesize){
			    p = stack + (arg - usrstack);
			    printf("\t");
			    while(p < stack_top && *p != '\0'){
				printf("%c", *p);
				p++;
			    }
			    printf("\n");
			    argv += sizeof(unsigned long);
			    memcpy(&arg, argv, sizeof(unsigned long));
			    if(swapped)
				arg = SWAP_LONG(arg);
			}
		    }
		}
		else{
		    if(sg.vmaddr + sg.vmsize == usrstack &&
		       sg.fileoff + sg.filesize <= object_size){
			stack = object_addr + sg.fileoff;
			stack_top = stack + sg.filesize;

			/* the first thing on the stack is a long 0 */
			stack_top -= 4;
			p = (char *)stack_top;

			/* find the first non-null character before the long 0*/
			while(p > stack && *p == '\0')
			    p--;
			if(p != (char *)stack_top)
			    p++;

			q = p;
			/* Stop when we find another long 0 */
			while(p > stack && (*p != '\0' || *(p-1) != '\0' ||
			      *(p-2) != '\0' || *(p-3) != '\0')){
			    p--;
			    /* step back over the string to its start */
			    while(p > stack && *p != '\0')
				p--;
			}

			p++; /* step forward to the start of the first string */
			while(p < q){
			    printf("\t");
			    len = 0;
			    while(p + len < q && p[len] != '\0'){
				printf("%c", p[len]);
				len++;
			    }
			    printf("\n");
			    p += len + 1;
			}
			return;
		    }
		}

		break;
	    }
	    if(l.cmdsize == 0){
		printf("load command %lu size zero (can't advance to other "
		       "load commands)\n", i);
		break;
	    }
	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	    if((char *)lc > (char *)load_commands + mh->sizeofcmds)
		break;
	}
}

/*
 * To avoid linking in libm.  These variables are defined as they are used in
 * pthread_init() to put in place a fast sqrt().
 */
size_t hw_sqrt_len = 0;

double
sqrt(double x)
{
	return(0.0);
}
double
hw_sqrt(double x)
{
	return(0.0);
}
