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
/*
 * This is a hacked version of nm(1) that has only one purpose:
 * to support cross-building the Mach kernel project.
 *
 * It takes an object file and prints out the value of the data symbols
 * from the data segment.  It requires all the symbols to be unsigned
 * long values.
 *
 * The values can be emitted in decimal form (the default) or in
 * hexadecimal with the -x flag.  The -c flag produces a C-syntax
 * header file that will define each of the symbols in the object file.
 * The -n flag lets you set the name of the header file that is used
 * for the multiple-inclusion protection #define at the top of the file.
 * The -o flag lets you specify a file to which the output of the program
 * will be written.
 *
 * This whole exercise is necessary because the only way to find out the
 * offsets of structure elements is to actually send code through
 * the compiler for your target architecture.  This makes it inconvenient
 * to extract that information when your build machine is of a different
 * architecture.
 *
 * An example: you create "genassym.c", which defines the offsets for
 * various structure elements that you want to use in assembly files.
 *
 * #import <stddef.h>
 * ...
 *
 * unsigned long FOO_HEAD = offsetof(struct foo, head);
 * ...
 *
 * You then build genassym.o for the target architecture, and run this
 * tool on the resulting .o:
 *
 * host> kern_tool -c genassym.o
 *
 * #ifndef _GENASSYM.H_
 * #define _GENASSYM.H_
 * ...
 * #define FOO_HEAD	44
 * ...
 * #endif / * _GENASSYM.H_ * /
 *
 * Now you have your include file for the target architecture with the
 * correct offset values.
 *
 * Curtis Galloway 1/25/95
 *
 * Here are the comments from the front of nm.c:
 * ----------------------------------------------
 *
 * The NeXT Computer, Inc. nm(1) program that handles fat files, archives and
 * Mach-O objects files (no 4.3bsd a.out files).  A few lines of code were taken
 * and adapted from the 4.3bsd release which included the following identifing
 * line:
 * static	char sccsid[] = "@(#)nm.c 4.7 5/19/86";
 *
 *		CHANGES FROM THE 4.3bsd VERSION OF nm(1):
 *
 * When processing multiple files which are archives the 4.3bsd version of nm
 * would only print the archive member name (without the -o option) of the
 * object files before printing the symbols.  This version of nm will print the
 * archive name with the member name in ()'s in this case which makes it clear
 * which symbols belong to which arguments in the case that multiple arguments
 * are archives and have members of the same name.
 *
 * To allow the "-arch <arch_flag>" command line argument the processing of
 * command line arguments was changed to allow the options to be specified
 * in more than one group of arguments with each group preceded by a '-'.  This
 * change in behavior would only be noticed if a command line of the form
 * "nm -n -Q" was used where the 4.3bsd version would open a file of the name
 * "-Q" to process.  To do this with this version of nm the new command line
 * argument "-" would be used to treat all remaining arguments as file names.
 * So the equivalent command would be "nm -n - -Q".  This should not be a
 * problem as the 4.3bsd would treat the command "nm -Q" by saying "-Q" is an
 * invalid argument which was slightly inconsistant.
 */
#include <mach/mach.h> /* first so to get rid of a precomp warning */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/stab.h>
#include "stuff/bool.h"
#include "stuff/ofile.h"
#include "stuff/errors.h"
#include "stuff/allocate.h"

/* used by error routines as the name of the program */
char *progname = NULL;

/* output file stream */
FILE *output;

/* flags set from the command line arguments */
struct cmd_flags {
    unsigned long nfiles;
    enum bool c;	/* print C-syntax header file */
    enum bool x;	/* print the symbol table entry in hex */
    enum bool n;	/* name of header file to create */
    char *ofile_name;
    enum bool o;	/* output file path */
    char *ofile_path;
};
/* These need to be static because of the qsort compare function */
static struct cmd_flags cmd_flags = { 0 };
static char *strings = NULL;
static unsigned long strsize = 0;

/* flags set by processing a specific object file */
struct process_flags {
    unsigned long nsect;	/* The nsect, address and size for the */
    unsigned long sect_addr,	/*  section specified by the -s flag */
	     sect_size;
    enum bool sect_start_symbol;/* For processing the -l flag, set if a */
				/*  symbol with the start address of the */
				/*  section is found */
    unsigned long nsects;	/* For printing the symbol types, the number */
    struct section **sections;	/*  of sections and an array of section ptrs */
    unsigned char text_nsect,	/* For printing symbols types, T, D, and B */
		  data_nsect,	/*  for text, data and bss symbols */
		  bss_nsect;
};

static void usage(
    void);
static void nm(
    struct ofile *ofile,
    char *arch_name,
    void *cookie);
static struct nlist *select_symbols(
    struct ofile *ofile,
    struct symtab_command *st,
    struct dysymtab_command *dyst,
    struct cmd_flags *cmd_flags,
    struct process_flags *process_flags,
    unsigned long *nsymbols);
static void print_symbols(
    struct ofile *ofile,
    struct nlist *symbols,
    unsigned long nsymbols,
    char *strings,
    unsigned long strsize,
    struct cmd_flags *cmd_flags,
    struct process_flags *process_flags,
    char *arch_name);
static int compare(
    struct nlist *p1,
    struct nlist *p2);

static inline char *upper_string(const char *str)
{
    char *newstr, *p;
    const char *src;
    
    p = newstr = malloc(strlen(str)+1);
    if ((src = strrchr(str, '/')) != NULL)
	src = src + 1;
    else
	src = str;
    while (*src && *src != '.')
	*p++ = toupper(*src++);
    *p = '\0';
    return newstr;
}

int
main(
int argc,
char **argv,
char **envp)
{
    int i;
    unsigned long j;
    struct arch_flag *arch_flags;
    unsigned long narch_flags;
    enum bool all_archs;
    char **files;

	progname = argv[0];

	arch_flags = NULL;
	narch_flags = 0;
	all_archs = FALSE;

	output = stdout;
	
	cmd_flags.nfiles = 0;
	cmd_flags.c = FALSE;
	cmd_flags.x = FALSE;
	cmd_flags.ofile_name = NULL;
	cmd_flags.ofile_path = NULL;

        files = allocate(sizeof(char *) * argc);
	for(i = 1; i < argc; i++){
	    if(argv[i][0] == '-'){
		if(argv[i][1] == '\0'){
		    for( ; i < argc; i++)
			files[cmd_flags.nfiles++] = argv[i];
		    break;
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
		}
		else{
		    for(j = 1; argv[i][j] != '\0'; j++){
			switch(argv[i][j]){
			case 'c':
			    cmd_flags.c = TRUE;
			    break;
			case 'x':
			    cmd_flags.x = TRUE;
			    break;
			case 'n':
			    cmd_flags.n = TRUE;
			    break;
			case 'o':
			    cmd_flags.o = TRUE;
			    break;
			default:
			    error("invalid argument -%c", argv[i][j]);
			    usage();
			}
		    }
		    if(cmd_flags.n == TRUE && cmd_flags.ofile_name == NULL){
			if(i + 1 == argc){
			    error("missing arguments to -n");
			    usage();
			}
			cmd_flags.ofile_name  = upper_string(argv[i+1]);
			i += 1;
		    }
		    if(cmd_flags.o == TRUE && cmd_flags.ofile_path == NULL){
			if(i + 1 == argc){
			    error("missing arguments to -o");
			    usage();
			}
			cmd_flags.ofile_path  = argv[i+1];
			i += 1;
		    }
		}
		continue;
	    }
	    files[cmd_flags.nfiles++] = argv[i];
	}
	if (cmd_flags.ofile_name == NULL) {
	    if (cmd_flags.nfiles == 0)
		cmd_flags.ofile_name = upper_string("a.out");
	    else
		cmd_flags.ofile_name = upper_string(files[0]);
	}
	if (cmd_flags.o == TRUE) {
	    output = fopen(cmd_flags.ofile_path, "w");
	    if (output == NULL) {
		error("couldn't open output file %s\n",cmd_flags.ofile_path);
		output = stdout;
	    }
	}

	for(j = 0; j < cmd_flags.nfiles; j++)
	    ofile_process(files[j], arch_flags, narch_flags, all_archs, FALSE,
			  FALSE, TRUE, nm, &cmd_flags);
	if(cmd_flags.nfiles == 0)
	    ofile_process("a.out",  arch_flags, narch_flags, all_archs, FALSE,
			  FALSE, TRUE, nm, &cmd_flags);

	if (cmd_flags.ofile_name != NULL)
	    free(cmd_flags.ofile_name);
	    
	if(errors == 0)
	    return(EXIT_SUCCESS);
	else
	    return(EXIT_FAILURE);
}

/*
 * usage() prints the current usage message and exits indicating failure.
 */
static
void
usage(
void)
{
	fprintf(stderr, "Usage: %s [-cx] "
		"[-n <name>] [-o <ofile] "
		"[[-arch <arch_flag>] ...] "
		"[file ...]\n", progname);
	exit(EXIT_FAILURE);
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
    enum bool found;
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

	lc = load_commands;
	for(i = 0 ; found == FALSE && i < mh->ncmds; i++){
	    memcpy((char *)&l, (char *)lc, sizeof(struct load_command));
	    if(l.cmdsize % sizeof(long) != 0)
		error("load command %lu size not a multiple of "
		       "sizeof(long)\n", i);
	    if((char *)lc + l.cmdsize >
	       (char *)load_commands + mh->sizeofcmds)
		error("load command %lu extends past end of load "
		       "commands\n", i);
	    left = mh->sizeofcmds - ((char *)lc - (char *)load_commands);

	    switch(l.cmd){
	    case LC_SEGMENT:
		memset((char *)&sg, '\0', sizeof(struct segment_command));
		size = left < sizeof(struct segment_command) ?
		       left : sizeof(struct segment_command);
		memcpy((char *)&sg, (char *)lc, size);

		if((mh->filetype == MH_OBJECT && sg.segname[0] == '\0') ||
		   strncmp(sg.segname, segname, sizeof(sg.segname)) == 0){

		    p = (char *)lc + sizeof(struct segment_command);
		    for(j = 0 ; found == FALSE && j < sg.nsects ; j++){
			if(p + sizeof(struct section) >
			   (char *)load_commands + mh->sizeofcmds){
			    error("section structure command extends past "
				   "end of load commands\n");
			}
			left = mh->sizeofcmds - (p - (char *)load_commands);
			memset((char *)&s, '\0', sizeof(struct section));
			size = left < sizeof(struct section) ?
			       left : sizeof(struct section);
			memcpy((char *)&s, p, size);

			if(strncmp(s.sectname, sectname,
				   sizeof(s.sectname)) == 0){
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
		error("load command %lu size zero (can't advance to other "
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
		error("section offset for section (%.16s,%.16s) is past end "
		       "of file\n", s.segname, s.sectname);
	    }
	    else{
		*sect_pointer = object_addr + s.offset;
		if(s.offset + s.size > object_size){
		    error("section (%.16s,%.16s) extends past end of file\n",
			   s.segname, s.sectname);
		    *sect_size = object_size - s.offset;
		}
		else
		    *sect_size = s.size;
	    }
	}
	if(s.reloff >= object_size){
	    error("relocation entries offset for (%.16s,%.16s): is past end "
		   "of file\n", s.segname, s.sectname);
	}
	else{
	    *sect_relocs = (struct relocation_info *)(object_addr + s.reloff);
	    if(s.reloff + s.nreloc * sizeof(struct relocation_info) >
								object_size){
		error("relocation entries for section (%.16s,%.16s) extends "
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

/*
 * nm() is the routine that gets called by ofile_process() to process single
 * object files.
 */
static
void
nm(
struct ofile *ofile,
char *arch_name,
void *cookie)
{
    struct cmd_flags *cmd_flags;
    struct process_flags process_flags;
    unsigned long i, j, k;
    struct load_command *lc;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct segment_command *sg;
    struct section *s;

    struct nlist *symbols;
    unsigned long nsymbols;

	cmd_flags = (struct cmd_flags *)cookie;

	process_flags.nsect = -1;
	process_flags.sect_addr = 0;
	process_flags.sect_size = 0;
	process_flags.sect_start_symbol = FALSE;
	process_flags.nsects = 0;
	process_flags.sections = NULL;
	process_flags.text_nsect = NO_SECT;
	process_flags.data_nsect = NO_SECT;
	process_flags.bss_nsect = NO_SECT;

	st = NULL;
	dyst = NULL;
	lc = ofile->load_commands;
	for(i = 0; i < ofile->mh->ncmds; i++){
	    if(st == NULL && lc->cmd == LC_SYMTAB){
		st = (struct symtab_command *)lc;
	    }
	    else if(dyst == NULL && lc->cmd == LC_DYSYMTAB){
		dyst = (struct dysymtab_command *)lc;
	    }
	    else if(lc->cmd == LC_SEGMENT){
		sg = (struct segment_command *)lc;
		process_flags.nsects += sg->nsects;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	if(st == NULL || st->nsyms == 0){
	    error("no name list");
	    return;
	}
	if(process_flags.nsects > 0){
	    process_flags.sections = (struct section **)
		       malloc(sizeof(struct section *) * process_flags.nsects);
	    k = 0;
	    lc = ofile->load_commands;
	    for (i = 0; i < ofile->mh->ncmds; i++){
		if(lc->cmd == LC_SEGMENT){
		    sg = (struct segment_command *)lc;
		    s = (struct section *)
			  ((char *)sg + sizeof(struct segment_command));
		    for(j = 0; j < sg->nsects; j++){
			if(strcmp((s + j)->sectname, SECT_TEXT) == 0 &&
			   strcmp((s + j)->segname, SEG_TEXT) == 0)
			    process_flags.text_nsect = k + 1;
			else if(strcmp((s + j)->sectname, SECT_DATA) == 0 &&
				strcmp((s + j)->segname, SEG_DATA) == 0)
			    process_flags.data_nsect = k + 1;
			else if(strcmp((s + j)->sectname, SECT_BSS) == 0 &&
				strcmp((s + j)->segname, SEG_DATA) == 0)
			    process_flags.bss_nsect = k + 1;
			process_flags.sections[k++] = s + j;
		    }
		}
		lc = (struct load_command *)
		      ((char *)lc + lc->cmdsize);
	    }
	}

	/* select symbols to print */
	symbols = select_symbols(ofile, st, dyst, cmd_flags, &process_flags,
				 &nsymbols);

	/* set names in the symbols to be printed */
	strings = ofile->object_addr + st->stroff;
	strsize = st->strsize;

	    for(i = 0; i < nsymbols; i++){
		if(symbols[i].n_un.n_strx == 0)
		    symbols[i].n_un.n_name = "";
		else if(symbols[i].n_un.n_strx < 0 ||
			(unsigned long)symbols[i].n_un.n_strx > st->strsize)
		    symbols[i].n_un.n_name = "bad string index";
		else
		    symbols[i].n_un.n_name = symbols[i].n_un.n_strx + strings;

		if((symbols[i].n_type & N_TYPE) == N_INDR){
		    if(symbols[i].n_value == 0)
			symbols[i].n_value = (long)"";
		    else if(symbols[i].n_value > st->strsize)
			symbols[i].n_value = (long)"bad string index";
		    else
			symbols[i].n_value =
				    (long)(symbols[i].n_value + strings);
		}
	    }

	/* sort the symbols if needed */
	qsort(symbols, nsymbols, sizeof(struct nlist),
		(int (*)(const void *, const void *))compare);

	if (cmd_flags->c == TRUE) {
	    /* print header */
	    fprintf(output, "#ifndef _%s.H_\n", cmd_flags->ofile_name);
	    fprintf(output, "#define _%s.H_\n", cmd_flags->ofile_name);
	}
	
	/* now print the symbols as specified by the flags */
	print_symbols(ofile, symbols, nsymbols, strings, st->strsize,
			cmd_flags, &process_flags, arch_name);

	free(symbols);
	if(process_flags.sections != NULL)
	    free(process_flags.sections);
	    
	if (cmd_flags->c == TRUE) {
	    /* print footer */
	    fprintf(output, "#endif /* _%s.H_ */\n", cmd_flags->ofile_name);
	}
}

/*
 * select_symbols returns an allocated array of nlist structs as the symbols
 * that are to be printed based on the flags.  The number of symbols in the
 * array returned in returned indirectly through nsymbols.
 */
static
struct nlist *
select_symbols(
struct ofile *ofile,
struct symtab_command *st,
struct dysymtab_command *dyst,
struct cmd_flags *cmd_flags,
struct process_flags *process_flags,
unsigned long *nsymbols)
{
    unsigned long i, flags;
    struct nlist *all_symbols, *selected_symbols, undefined;
    struct dylib_module m;
    struct dylib_reference *refs;

	all_symbols = (struct nlist *)(ofile->object_addr + st->symoff);
	selected_symbols = allocate(sizeof(struct nlist) * st->nsyms);
	*nsymbols = 0;

	if(ofile->object_byte_sex != get_host_byte_sex())
	    swap_nlist(all_symbols, st->nsyms, get_host_byte_sex());

	if(ofile->dylib_module != NULL){
	    m = *ofile->dylib_module;
	    refs = (struct dylib_reference *)(ofile->object_addr +
					      dyst->extrefsymoff);
	    if(ofile->object_byte_sex != get_host_byte_sex()){
		swap_dylib_module(&m, 1, get_host_byte_sex());
		swap_dylib_reference(refs + m.irefsym, m.nrefsym,
				     get_host_byte_sex());
	    }
	    for(i = 0; i < m.nrefsym; i++){
		flags = refs[i + m.irefsym].flags;
		if(flags == REFERENCE_FLAG_UNDEFINED_NON_LAZY ||
		   flags == REFERENCE_FLAG_UNDEFINED_LAZY ||
		   flags == REFERENCE_FLAG_PRIVATE_UNDEFINED_NON_LAZY ||
		   flags == REFERENCE_FLAG_PRIVATE_UNDEFINED_LAZY){
		    undefined = all_symbols[refs[i + m.irefsym].isym];
		    if(flags == REFERENCE_FLAG_UNDEFINED_NON_LAZY ||
		       flags == REFERENCE_FLAG_UNDEFINED_LAZY)
			undefined.n_type = N_UNDF | N_EXT;
		    else
			undefined.n_type = N_UNDF;
		    undefined.n_desc = (undefined.n_desc &~ REFERENCE_TYPE) |
				       flags;
		    undefined.n_value = 0;
		    
		    selected_symbols[(*nsymbols)++] = undefined;
		}
	    }
	    for(i = 0; i < m.nextdefsym; i++){
		selected_symbols[(*nsymbols)++] =
			all_symbols[m.iextdefsym + i];
	    }
	    for(i = 0; i < m.nlocalsym; i++){
		selected_symbols[(*nsymbols)++] =
			all_symbols[m.ilocalsym + i];
	    }
	}
	else{
	    for(i = 0; i < st->nsyms; i++){
		selected_symbols[(*nsymbols)++] = all_symbols[i];
	    }
	}
	if(ofile->object_byte_sex != get_host_byte_sex())
	    swap_nlist(all_symbols, st->nsyms, ofile->object_byte_sex);
	/*
	 * Could reallocate selected symbols to the exact size but it is more
	 * of a time waste than a memory savings.
	 */
	return(selected_symbols);
}


/*
 * print_symbols() prints out the value of the data symbols
 * in the specified format.
 */
static
void
print_symbols(
struct ofile *ofile,
struct nlist *symbols,
unsigned long nsymbols,
char *strings,
unsigned long strsize,
struct cmd_flags *cmd_flags,
struct process_flags *process_flags,
char *arch_name)
{
    unsigned long i;
    unsigned char c;
    unsigned long value;

    char *sect, *addr;
    unsigned long sect_size, sect_addr, sect_nrelocs, sect_flags, size;
    struct relocation_info *sect_relocs;
    enum bool swapped;
    

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

	swapped = ofile->object_byte_sex != get_host_byte_sex();

	if(get_sect_info(SEG_DATA, SECT_DATA, ofile->mh,
	    ofile->load_commands, ofile->object_byte_sex,
	    addr, size, &sect, &sect_size, &sect_addr,
	    &sect_relocs, &sect_nrelocs, &sect_flags) != TRUE) {
	    error("No data section information\n");
	    return;
	}

	for(i = 0; i < nsymbols; i++){
	    c = symbols[i].n_type;
	    if(c & N_STAB){
		continue;
	    }
	    switch(c & N_TYPE){
	    case N_UNDF:
		c = 'u';
		if(symbols[i].n_value != 0)
		    c = 'c';
		break;
	    case N_ABS:
		c = 'a';
		break;
	    case N_SECT:
		if(symbols[i].n_sect == process_flags->text_nsect)
		    c = 't';
		else if(symbols[i].n_sect == process_flags->data_nsect)
		    c = 'd';
		else if(symbols[i].n_sect == process_flags->bss_nsect)
		    c = 'b';
		else
		    c = 's';
		break;
	    case N_INDR:
		c = 'i';
		break;
	    default:
		c = '?';
		break;
	    }
	    if((symbols[i].n_type & N_EXT) && c != '?')
		c = toupper(c);

		if(c == 'u' || c == 'U' || c == 'i' || c == 'I')
		    fprintf(output, "        ");
		else {
		    if (c == 'D') {
			unsigned long offset;

			if (cmd_flags->c == TRUE)
			    fprintf(output, "#define ");
			/* skip leading underscore of names */
			fprintf(output, "%s\t", (symbols[i].n_un.n_name + 1));
			/* Subtract section offset */
			offset = sect_addr + symbols[i].n_value;
			/* Check range */
			if (offset > (sect_size - sizeof(unsigned long))) {
			    error("value for symbol '%s' located "
			          "outside data section\n",
				  (symbols[i].n_un.n_name + 1));
			    continue;
			}
			bcopy(sect + offset, &value, sizeof(value));
			if (swapped)
			    value = SWAP_LONG(value);
			/* SWAP_LONG */
			if (cmd_flags->x == TRUE)
			    fprintf(output, "0x%08lx\n", value);
			else
			    fprintf(output, "%ld\n", value);
		    }
		}
	}
}

/*
 * compare is the function used by qsort if any sorting of symbols is to be
 * done.
 */
static
int
compare(
struct nlist *p1,
struct nlist *p2)
{
    return strcmp(p1->n_un.n_name, p2->n_un.n_name);
}
