/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * The redo_prebinding(1) program.  This redoes the prebinding of an executable
 * or dynamic library.
 *
 * redo_prebinding [-c|-p|-d] [-i] [-r rootdir] [-o output_file] input_file 
 *	-c check only and return status
 *	-p check only for prebound files and return status
 *	-d check only for dylibs and return status
 *	-i ignore non-prebound files
 *	-r prepend the next argument to dependent libraries
 * With no -c, -p or -d it exits 0 if sucessful and 2 means it could not be
 * done for reasons like a dependent library is missing.  An exit of 3 is for
 * the specific case when the dependent libraries are out of date with respect
 * to each other.
 * 
 * If -c, check only, is specified a 0 exit means the file's prebinding is
 * uptodate, 1 means it needs to be redone and 2 means it could not be checked
 * for reasons like a dependent library is missing.
 *
 * If -p, check only for prebound files, is specified 1 exit means the file is
 * a Mach-O that could be prebound and is not otherwise the exit is 0.
 *
 * If -d, check only for dylib files, is specified a 0 exit means the file is a
 * dylib, 1 means the file is not a dylib and 2 means there is some mix in
 * the architectures.
 *
 * Other possible options to consider implementing:
 *	-seg1addr (for dylibs only) slide library to new seg1addr
 */
#import <stdio.h>
#import <stdlib.h>
#import <string.h>
#import <limits.h>
#import <libc.h>
#import <sys/types.h>
#import <sys/stat.h>
#import <mach-o/loader.h>
#import <mach-o/reloc.h>
#import <mach-o/hppa/reloc.h>
#import <mach-o/sparc/reloc.h>
#import <mach-o/ppc/reloc.h>
#import <stuff/breakout.h>
#import <stuff/best_arch.h>
#import <stuff/allocate.h>
#import <stuff/errors.h>
#import <stuff/round.h>
#import <stuff/hppa.h>
#import <stuff/execute.h>

#define U_ABS(l) (((long)(l))<0 ? (unsigned long)(-(l)) : (l))

/* name of the program for error messages (argv[0]) */
char *progname;

/* -c option, only check and return status */
enum bool check_only = FALSE;

/* -i option, ignore non-prebound files */
enum bool ignore_non_prebound = FALSE;

/* -p option, check for non-prebound files */
enum bool check_for_non_prebound = FALSE;

/* -d option, check for dynamic library files */
enum bool check_for_dylibs = FALSE;
enum bool seen_a_dylib = FALSE;
enum bool seen_a_non_dylib = FALSE;

/* -r option's argument, root directory to prepend to dependent libraries */
char *root_dir = NULL;

/* -debug turn on debugging printf()'s */
enum bool debug = FALSE;

/*
 * If some architecture was processed then the output file needs to be built
 * otherwise no output file is written.
 */
enum bool arch_processed = FALSE;

/* the link state of each module */
enum link_state {
    UNLINKED,
    LINKED
};

/*
 * These are set to the current arch's symbolic info.
 */
static struct arch *arch = NULL;
static struct arch_flag arch_flag = { 0 };
static enum bool arch_swapped;
static char *arch_name = NULL;
static struct nlist *arch_symbols = NULL;
static unsigned long arch_nsyms = 0;
static char *arch_strings = NULL;
static unsigned long arch_strsize = 0;
static struct dylib_table_of_contents *arch_tocs = NULL;
static unsigned long arch_ntoc = 0;
static struct dylib_module *arch_mods = NULL;
static unsigned long arch_nmodtab = 0;
static struct dylib_reference *arch_refs = NULL;
static unsigned long arch_nextrefsyms = 0;
static enum link_state arch_state = LINKED;

static unsigned long arch_seg1addr;
static unsigned long arch_segs_read_write_addr;
static enum bool arch_split_segs;
static struct relocation_info *arch_extrelocs = NULL;
static unsigned long arch_nextrel = 0;
static unsigned long *arch_indirect_symtab = NULL;
static unsigned long arch_nindirectsyms = 0;

/*
 * These hold the dependent libraries for the arch currently being processed.
 * Their link edit information is used to update the arch currently being
 * processed.
 */
struct lib {
    char *dylib_name;
    char *file_name;
    struct ofile *ofile;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct routines_command *rc;
    struct nlist *symbols;
    unsigned long nsyms;
    char *strings;
    unsigned long strsize;
    struct dylib_table_of_contents *tocs;
    unsigned long ntoc;
    struct dylib_module *mods;
    unsigned long nmodtab;
    struct dylib_reference *refs;
    unsigned long nextrefsyms;
    enum link_state *module_states;
    enum bool LC_PREBOUND_DYLIB_found;
    unsigned long LC_PREBOUND_DYLIB_size;
};
struct lib *libs = NULL;
unsigned long nlibs = 0;

/*
 * This is used by check_for_overlapping_segments() to create a list of segment
 * for overlap checking.
 */
struct segment {
    char *file_name;
    struct segment_command *sg;
};

static void usage(
    void);

static void process_archs(
    struct arch *archs,
    unsigned long narchs);

static void process_arch(void);

static void load_archs_libraries(void);

static void load_library(
    char *file_name,
    struct dylib_command *dl_load,
    enum bool time_stamps_must_match);

static void load_dependent_libraries(void);

static void check_for_overlapping_segments(void);

static void check_overlap(
    struct segment *s1,
    struct segment *s2);

static void setup_symbolic_info(void);

static void swap_arch_for_output(void);

static void check_symbolic_info_tables(
    char *file_name,
    struct symtab_command *st,
    struct dysymtab_command *dyst,
    struct nlist *symbols,
    unsigned long nsyms,
    char *strings,
    unsigned long strsize,
    struct dylib_table_of_contents *tocs,
    unsigned long ntoc,
    struct dylib_module *mods,
    unsigned long nmodtab,
    struct dylib_reference *refs,
    unsigned long nextrefsyms);

static void check_for_dylib_override_symbols(void);

static void check_dylibs_for_definition(
    char *file_name,
    char *symbol_name);

static enum bool check_dylibs_for_reference(
    char *symbol_name);

/* these two variables are used by the bsearch routines */
static char *bsearch_strings = NULL;
static struct nlist *bsearch_symbols = NULL;

static int dylib_bsearch(
    const char *symbol_name,
    const struct dylib_table_of_contents *toc);

static int nlist_bsearch(
    const char *symbol_name,
    const struct nlist *symbol);

static void setup_initial_undefined_list(void);

static void link_in_need_modules(void);

/*
 * The structure of an element in a symbol list.
 */
struct symbol_list {
    char *name;			/* name of the symbol */
    struct symbol_list *prev;	/* previous in the chain */
    struct symbol_list *next;	/* next in the chain */
};
/*
 * The head of the undefined list.  This is a circular list so it can be
 * searched from start to end and so new items can be put on the end.  This
 * structure never has its name filled in but they only serve as head and tail
 * of the list.
 */
static struct symbol_list undefined_list = {
    NULL, &undefined_list, &undefined_list
};

static void add_to_undefined_list(
    char *name);

static void link_library_module(
    enum link_state *module_state,
    struct lib *lib);

struct indr_loop_list {
    struct nlist *symbol;
    struct indr_loop_list *next;
};
#define NO_INDR_LOOP ((struct indr_loop_list *)1)

static void lookup_symbol(
    char *name,
    struct nlist **symbol,
    enum link_state **module_state,
    struct lib **lib,
    struct indr_loop_list *indr_loop);

static void build_new_symbol_table(
    void);

static void update_external_relocs(
    void);

static void update_generic_external_relocs(
    void);

static void update_hppa_external_relocs(
    void);

static void update_sparc_external_relocs(
    void);

static void update_ppc_external_relocs(
    void);

static char *contents_pointer_for_vmaddr(
    unsigned long vmaddr,
    unsigned long size);

static void update_symbol_pointers(
    void);

static void update_load_commands(
    void);

/*
 * These routines are used to get/set values that might not be aligned correctly
 * which are being relocated.
 */
static
inline
long
get_arch_long(
void *addr)
{
    long l;

	memcpy(&l, addr, sizeof(long));
	if(arch_swapped == TRUE)
	    return(SWAP_LONG(l));
	else
	    return(l);
}

static
inline
short
get_arch_short(
void *addr)
{
    short s;

	memcpy(&s, addr, sizeof(short));
	if(arch_swapped == TRUE)
	    return(SWAP_SHORT(s));
	else
	    return(s);
}

static
inline
char
get_arch_byte(
char *addr)
{
	return(*addr);
}

static
inline
void
set_arch_long(
void *addr,
long value)
{
	if(arch_swapped == TRUE)
	    value = SWAP_LONG(value);
	memcpy(addr, &value, sizeof(long));
}

static
inline
void
set_arch_short(
void *addr,
short value)
{
    if(arch_swapped == TRUE)
	value = SWAP_SHORT(value);
    memcpy(addr, &value, sizeof(short));
}

static
inline
void
set_arch_byte(
char *addr,
char value)
{
	*addr = value;
}

/*
 * main() see top of file for program's description and options.
 */
int
main(
int argc,
char *argv[],
char *envp[])
{
    unsigned long i;
    char *input_file, *output_file;
    struct arch *archs;
    unsigned long narchs;
    struct stat stat_buf;
    enum bool verbose;
    unsigned short mode;
    uid_t uid;
    gid_t gid;


    	input_file = NULL;
	output_file = NULL;
	archs = NULL;
	narchs = 0;
	errors = 0;
	verbose = FALSE;

	progname = argv[0];

	for(i = 1; i < argc; i++){
	    if(argv[i][0] == '-'){
		if(strcmp(argv[i], "-o") == 0){
		    if(i + 1 >= argc)
			fatal("-o requires an argument");
		    if(output_file != NULL)
			fatal("only one -o option allowed");
		    output_file = argv[i + 1];
		    i++;
		}
		else if(strcmp(argv[i], "-r") == 0){
		    if(i + 1 >= argc)
			fatal("-r requires an argument");
		    if(root_dir != NULL)
			fatal("only one -r option allowed");
		    root_dir = argv[i + 1];
		    i++;
		}
		else if(strcmp(argv[i], "-c") == 0){
		    check_only = TRUE;
		}
		else if(strcmp(argv[i], "-i") == 0){
		    ignore_non_prebound = TRUE;
		}
		else if(strcmp(argv[i], "-p") == 0){
		    check_for_non_prebound = TRUE;
		}
		else if(strcmp(argv[i], "-d") == 0){
		    check_for_dylibs = TRUE;
		}
		else if(strcmp(argv[i], "-debug") == 0){
		    debug = TRUE;
		}
		else if(strcmp(argv[i], "-v") == 0){
		    verbose = TRUE;
		}
		else{
		    fprintf(stderr, "%s: unknown option: %s\n", progname,
			    argv[i]);
		    usage();
		}
	    }
	    else{
		if(input_file != NULL)
		    fatal("only one input file allowed");
		input_file = argv[i];
	    }
	}
	if(input_file == NULL){
	    fprintf(stderr, "%s no input file specified\n", progname);
	    usage();
	}
	if(check_only + check_for_non_prebound + check_for_dylibs > 1){
	    fprintf(stderr, "%s only one of -c, -p or -d can be specified\n",
		    progname);
	    usage();
	}

	/* breakout the file for processing */
	breakout(input_file, &archs, &narchs);
	if(errors)
	    exit(2);

	/* checkout the file for processing */
	checkout(archs, narchs);

	/* process the input file */
	process_archs(archs, narchs);
	if(errors)
	    exit(2);

	/*
	 * If we are checking for dylibs and get back from process_archs() we
	 * either have all dylibs or all non-dylibs.  So exit with 0 for dylibs
	 * an 1 for non-dylibs.
	 */
	if(check_for_dylibs == TRUE){
	    if(seen_a_dylib == TRUE)
		exit(0);
	    exit(1);
	}

	/*
	 * If we are checking for non-prebound files and get back from
	 * process_archs() we don't have any Mach-O's that were not prebound
	 * so indicate this with an exit status of 0.
	 */
	if(check_for_non_prebound == TRUE)
	    exit(0);

	/*
	 * Create an output file if we processed any of the archs and we are
	 * not doing checking only.
	 */
	if(arch_processed == TRUE){
	    if(check_only == TRUE)
		exit(1);
	    if(stat(input_file, &stat_buf) == -1)
		system_error("can't stat input file: %s", input_file);
	    mode = stat_buf.st_mode & 06777;
	    uid = stat_buf.st_uid;
            gid = stat_buf.st_gid;

	    if(output_file != NULL){
		writeout(archs, narchs, output_file, mode, TRUE, FALSE, FALSE);
	    }
	    else{
		output_file = makestr(input_file, ".redo_prebinding", NULL);
		writeout(archs, narchs, output_file, mode, TRUE, FALSE, FALSE);
		if(rename(output_file, input_file) == 1)
		    system_error("can't move temporary file: %s to input "
				 "file: %s\n", output_file, input_file);
		free(output_file);
		output_file = NULL;
	    }
	    /*
	     * Run /usr/bin/objcunique on the output.
	     */
	    if(stat("/usr/bin/objcunique", &stat_buf) != -1){
		reset_execute_list();
		add_execute_list("/usr/bin/objcunique");
		if(output_file != NULL)
		    add_execute_list(output_file);
		else
		    add_execute_list(input_file);
		add_execute_list("-prebind");
		if(ignore_non_prebound == TRUE)
		    add_execute_list("-i");
		if(root_dir != NULL){
		    add_execute_list("-r");
		    add_execute_list(root_dir);
		}
		if(execute_list(verbose) == 0)
		    fatal("internal /usr/bin/objcunique command failed");
	    }
	    /*
	     * Call chmod(2) to insure set-uid, set-gid and sticky bits get set.
	     * Then call chown to insure the file has the same owner and group
	     * as the original file.
	     */
	    if(output_file != NULL){
		if(chmod(output_file, mode) == -1)
		    system_error("can't set permissions on file: %s",
			output_file);
		if(chown(output_file, uid, gid) == -1)
		    system_error("can't set owner and group on file: %s",
			output_file);
	    }
	    else{
		if(chmod(input_file, mode) == -1)
		    system_error("can't set permissions on file: %s",
			input_file);
		if(chown(input_file, uid, gid) == -1)
		    system_error("can't set owner and group on file: %s",
			input_file);
	    }
	}
	else{
	    if(check_only == TRUE)
		exit(0);
	}

	/* clean-up data structures */
	free_archs(archs, narchs);

	if(errors)
	    return(2);
	else
	    return(0);
}

/*
 * usage() prints the current usage message.
 */
static
void
usage(
void)
{
	fprintf(stderr, "Usage: %s [-c|-p|-d] [-i] [-r rootdir] "
		"[-o output_file] input_file\n", progname);
	exit(EXIT_FAILURE);
}

/*
 * process_archs() is passed the broken out arch's and processes each of them
 * checking to make sure they are prebound and either executables or dylibs.
 */
static
void
process_archs(
struct arch *archs,
unsigned long narchs)
{
    unsigned long i;

	for(i = 0; i < narchs; i++){
	    arch = archs + i;
	    if(arch->type != OFILE_Mach_O){
		if(check_for_dylibs == TRUE){
		    if(seen_a_dylib == TRUE)
			exit(2);
		    seen_a_non_dylib = TRUE;
		    continue;
		}
		else if(check_only == TRUE ||
		        ignore_non_prebound == TRUE ||
		        check_for_non_prebound == TRUE)
		    continue;
		else
		    fatal_arch(arch, NULL, "file is not a Mach-O file: ");
	    }
	    if(arch->object->mh->filetype != MH_EXECUTE &&
	       arch->object->mh->filetype != MH_DYLIB){
		if(check_for_dylibs == TRUE){
		    if(seen_a_dylib == TRUE)
			exit(2);
		    seen_a_non_dylib = TRUE;
		}
		else if(check_only == TRUE ||
		        ignore_non_prebound == TRUE ||
		        check_for_non_prebound == TRUE)
		    continue;
		else
		    fatal_arch(arch, NULL, "file is not a Mach-O "
			"executable or dynamic shared library file: ");
	    }
	    if(check_for_dylibs == TRUE){
		if(arch->object->mh->filetype == MH_DYLIB){
		    if(seen_a_non_dylib == TRUE)
			exit(2);
		    seen_a_dylib = TRUE;
		}
		else{
		    if(seen_a_dylib == TRUE)
			exit(2);
		    seen_a_non_dylib = TRUE;
		}
		continue;
	    }
	    if((arch->object->mh->flags & MH_PREBOUND) != MH_PREBOUND){
		if(check_for_non_prebound == TRUE){
		    if((arch->object->mh->flags & MH_DYLDLINK) == MH_DYLDLINK)
			exit(1);
		    continue;
		}
		else if(check_only == TRUE || ignore_non_prebound == TRUE)
		    continue;
		else
		    fatal_arch(arch, NULL, "file is not prebound: ");
	    }
	    if(check_for_non_prebound == TRUE)
		continue;

	    /* Now redo the prebinding for this arch[i] */
	    process_arch();
	}
}

/*
 * process_arch() takes one arch which is a prebound executable or dylib and
 * redoes the prebinding.
 */
static
void
process_arch(
void)
{
	/*
	 * Clear out any libraries loaded for the previous arch that was
	 * processed.
	 */
	if(libs != NULL)
	    free(libs);
	libs = NULL;
	nlibs = 0;

	/* set up an arch_flag for this arch's object */
	arch_flag.cputype = arch->object->mh->cputype;
	arch_flag.cpusubtype = arch->object->mh->cpusubtype;
	set_arch_flag_name(&arch_flag);
	arch_name = arch_flag.name;

	if(debug == TRUE)
	    printf("%s: processing file: %s (for architecture %s)\n",
		   progname, arch->file_name, arch_name);

	/*
	 * First load the dynamic libraries this arch directly depends on
	 * allowing the time stamps not to match since we are redoing the
	 * prebinding for this arch.
	 */
	load_archs_libraries();

	/*
	 * Now load the dependent libraries who's time stamps much match.
	 */
        load_dependent_libraries();

	/*
	 * To deal with libsys, in that it has no dependent libs and it's
	 * prebinding can't be redone as indr(1) has been run on it and it has
	 * undefineds for __NXArgc, __NXArgv, and __environ from crt code,
	 * we return if the arch has no dependent libraries.
	 */
	if(nlibs == 0)
	    return;

	/*
	 * Before we use the symbolic information we may need to swap everything
	 * into the host byte sex and check to see if it is all valid.
	 */
	setup_symbolic_info();

	/*
	 * Check for overlaping segments in case a library now overlaps this
	 * arch.  We assume that the checks for the dependent libraries made
	 * by the link editor when the where prebound is still vaild.
	 */
	check_for_overlapping_segments();

	/*
         * Check to make sure symbols are not overridden in dependent dylibs
         * when so prebinding can be redone.
	 */
	check_for_dylib_override_symbols();

	/*
	 * Setup the initial list of undefined symbols from the arch being
	 * processed.
	 */
	setup_initial_undefined_list();
 
	/*
	 * Link in the needed modules from the dependent libraries based on the
	 * undefined symbols setup above.  This will check for multiply defined
	 * symbols.  Then allow undefined symbols to be checked for.
	 */
	link_in_need_modules();
	
	/*
	 * If check_only is set then load_library() checked the time stamps and
	 * did an exit(1) if they did not match.  So if we get here this arch
	 * has been checked so just return so the other archs can be checked.
	 */
	if(check_only == TRUE)
	    return;

	/*
	 * Now that is possible to redo the prebinding as all the above checks
	 * have been done.  So this arch will be processed so set arch_processed
	 * to indicate an output file needs to be created.
	 */
	arch_processed = TRUE;

	/*
	 * Now that is possible to redo the prebinding build a new symbol table
	 * using the new values for prebound undefined symbols.
	 */
	build_new_symbol_table();

	/*
	 * Using the new and old symbol table update the external relocation
	 * entries.
	 */
	update_external_relocs();

	/*
	 * Using the new and old symbol table update the symbol pointers.
	 */
	update_symbol_pointers();

	/*
	 * Update the time stamps in the LC_LOAD_DYLIB commands and update
	 * the LC_PREBOUND_DYLIB is this is an excutable.
	 */
	update_load_commands();

	/*
	 * If the arch is swapped swap it back for output.
	 */
	if(arch_swapped == TRUE)
	    swap_arch_for_output();
}

/*
 * load_archs_libraries() loads the libraries referenced by the image arch.
 */
static
void
load_archs_libraries(
void)
{
    unsigned long i;
    struct load_command *lc;
    struct dylib_command *dl_load;

	lc = arch->object->load_commands;
	for(i = 0; i < arch->object->mh->ncmds; i++){
	    if(lc->cmd == LC_LOAD_DYLIB){
		dl_load = (struct dylib_command *)lc;
		load_library(arch->file_name, dl_load, FALSE);
	    }
	    if(lc->cmd == LC_ID_DYLIB && check_only == TRUE){
		dl_load = (struct dylib_command *)lc;
		load_library(arch->file_name, dl_load, TRUE);
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
}

/*
 * load_dependent_libraries() now that the libraries of the arch being are
 * loaded now load the dependent libraries who's time stamps much match.
 */
static
void
load_dependent_libraries(
void)
{
    unsigned long i, j;
    struct load_command *lc;
    struct dylib_command *dl_load;

	for(i = 0; i < nlibs; i++){
	    if(debug == TRUE)
		printf("%s: loading libraries for library %s\n",
		       progname, libs[i].file_name);
	    lc = libs[i].ofile->load_commands;
	    for(j = 0; j < libs[i].ofile->mh->ncmds; j++){
		if(lc->cmd == LC_LOAD_DYLIB){
		    dl_load = (struct dylib_command *)lc;
		    load_library(libs[i].ofile->file_name, dl_load, TRUE);
		}
		lc = (struct load_command *)((char *)lc + lc->cmdsize);
	    }
	}
}

/*
 * load_library() loads the library for the dl_load command for the current
 * architecture being processed in indicated by arch_flag.   This library is
 * being loaded because file_name depends in it.  If time_stamps_must_match is
 * TRUE then this library is not a direct dependent of what we are redoing the
 * prebinding for it must be correct.  Since we are now processing a valid 
 * file any errors in loading the library are fatal.
 */
static
void
load_library(
char *file_name,
struct dylib_command *dl_load,
enum bool time_stamps_must_match)
{
    unsigned long i;
    char *dylib_name;
    struct ofile *ofile;
    struct fat_arch *best_fat_arch;
    struct load_command *lc;
    struct dylib_command *dl_id;
    enum bool already_loaded;

	/* get the name of the library from the load command */
	dylib_name = (char *)dl_load + dl_load->dylib.name.offset;

	/* if this library is already loaded just return */
	already_loaded = FALSE;
	ofile = NULL;
	for(i = 0; i < nlibs; i++){
	    if(strcmp(libs[i].dylib_name, dylib_name) == 0){
		if(time_stamps_must_match == FALSE)
		    return;
		already_loaded = TRUE;
		ofile = libs[i].ofile;
		dylib_name = libs[i].file_name;
		break;
	    }
	}
	if(already_loaded == FALSE){
	    if(debug == TRUE)
		printf("%s: loading library: %s\n", progname, dylib_name);

	    /*
	     * If a root_dir option is used prepend the directory for rooted
	     * names.
	     */
	    if(root_dir != NULL && *dylib_name == '/')
		dylib_name = makestr(root_dir, dylib_name, NULL);

	    ofile = allocate(sizeof(struct ofile));

	    /* now map in the library for this architecture */
	    if(ofile_map(dylib_name, NULL, NULL, ofile, FALSE) == FALSE)
		exit(2);
	}

	/*
	 * Check to make sure the ofile is a dynamic library and for the
	 * the correct architecture.
	 */
	if(ofile->file_type == OFILE_FAT){
	    best_fat_arch = cpusubtype_findbestarch(
		arch_flag.cputype,
		arch_flag.cpusubtype,
		ofile->fat_archs,
		ofile->fat_header->nfat_arch);
	    if(best_fat_arch == NULL){
		error("dynamic shared library file: %s does not contain an "
		      "architecture that can be used with %s (architecture %s)",
		      dylib_name, file_name, arch_name);
		exit(2);
	    }

	    (void)ofile_first_arch(ofile);
	    do{
		if(best_fat_arch != ofile->fat_archs + ofile->narch)
		    continue;
		if(ofile->arch_type == OFILE_ARCHIVE){
		    error("file: %s (for architecture %s) is an archive (not "
			  "a Mach-O dynamic shared library)", dylib_name,
			  ofile->arch_flag.name);
		    exit(2);
		}
		else if(ofile->arch_type == OFILE_Mach_O){
		    if(ofile->mh->filetype != MH_DYLIB){
			error("file: %s (for architecture %s) is not a Mach-O "
			      "dynamic shared library", dylib_name,
			      arch_name);
			exit(2);
		    }
		    goto good;
		}
		else if(ofile->arch_type == OFILE_UNKNOWN){
		    error("file: %s (for architecture %s) is not a Mach-O "
			  "dynamic shared library", dylib_name, arch_name);
		    exit(2);
		}
	    }while(ofile_next_arch(ofile) == TRUE);
	}
	else if(ofile->file_type == OFILE_ARCHIVE){
	    error("file: %s is an archive (not a Mach-O dynamic shared "
		  "library)", dylib_name);
	    exit(2);
	}
	else if(ofile->file_type == OFILE_Mach_O){
	    if(arch_flag.cputype != ofile->mh->cputype){
		error("dynamic shared library: %s has the wrong CPU type for: "
		      "%s (architecture %s)", dylib_name, file_name,
		      arch_name);
		exit(2);
	    }
	    if(cpusubtype_combine(arch_flag.cputype,
		arch_flag.cpusubtype, ofile->mh->cpusubtype) == -1){
		error("dynamic shared library: %s has the wrong CPU subtype "
		      "for: %s (architecture %s)", dylib_name, file_name,
		      arch_name);
		exit(2);
	    }
	}
	else{ /* ofile->file_type == OFILE_UNKNOWN */
	    error("file: %s is not a Mach-O dynamic shared library",
		  dylib_name);
	    exit(2);
	}

good:
	/*
	 * At this point ofile is infact a dynamic library and the right part
	 * of the ofile selected for the arch passed into here.
	 */

	/*
	 * If the time stamps must match check for matching time stamps.
	 */
	if(time_stamps_must_match == TRUE){
	    lc = ofile->load_commands;
	    for(i = 0; i < ofile->mh->ncmds; i++){
		if(lc->cmd == LC_ID_DYLIB){
		    dl_id = (struct dylib_command *)lc;
		    if(dl_load->dylib.timestamp != dl_id->dylib.timestamp){
			if(dl_load->cmd == LC_ID_DYLIB){
			    error("library: %s (architecture %s) prebinding "
				  "not uptodate with installed dynamic shared "
				  "library: %s", file_name, arch_name,
				  dylib_name);
			    exit(1);
			}
			else{
			    error("library: %s (architecture %s) prebinding "
				  "not uptodate with dependent dynamic shared "
				  "library: %s", file_name, arch_name,
				  dylib_name);
			    exit(3);
			}
		    }
		}
		lc = (struct load_command *)((char *)lc + lc->cmdsize);
	    }
	}
	/*
	 * When the time stamps don't need to match we are processing the
	 * arch we are trying to redo the prebinding for.  So if we are just
	 * checking then see if the time stamps are out of date and if so
	 * exit(1) to indicate this needs to have it's prepinding redone.
	 */
	else if(check_only == TRUE){
	    lc = ofile->load_commands;
	    for(i = 0; i < ofile->mh->ncmds; i++){
		if(lc->cmd == LC_ID_DYLIB){
		    dl_id = (struct dylib_command *)lc;
		    if(dl_load->dylib.timestamp != dl_id->dylib.timestamp){
			exit(1);
		    }
		}
		lc = (struct load_command *)((char *)lc + lc->cmdsize);
	    }
	}

	/*
	 * To allow the check_only to check the installed library load_library()
	 * can be called with an LC_ID_DYLIB for the install library to check
	 * it time stamp.  This is not put into the list however as it would
	 * overlap.
	 */
	if(already_loaded == FALSE && dl_load->cmd == LC_LOAD_DYLIB){
	    /*
	     * Add this library's ofile to the list of libraries the current
	     * arch depends on.
	     */
	    libs = reallocate(libs, (nlibs + 1) * sizeof(struct lib));
	    memset(libs + nlibs, '\0', sizeof(struct lib));
	    libs[nlibs].dylib_name = (char *)dl_load +
				     dl_load->dylib.name.offset;
	    libs[nlibs].file_name = dylib_name;
	    libs[nlibs].ofile = ofile;
	    nlibs++;
	}
}

/*
 * check_for_overlapping_segments() checks to make sure the segments in the
 * arch and all the dependent libraries do not overlap.  If they do the
 * prebinding can't be redone.
 */
static
void
check_for_overlapping_segments(
void)
{
    unsigned long i, j;
    struct segment *segments;
    unsigned long nsegments;
    struct load_command *lc;
    struct segment_command *sg;

	segments = NULL;
	nsegments = 0;

	/* put each segment of the arch in the segment list */
	lc = arch->object->load_commands;
	for(i = 0; i < arch->object->mh->ncmds; i++){
	    if(lc->cmd == LC_SEGMENT){
		sg = (struct segment_command *)lc;
		segments = reallocate(segments, 
				      (nsegments + 1) * sizeof(struct segment));
		segments[nsegments].file_name = arch->file_name;
		segments[nsegments].sg = sg;
		nsegments++;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}

	/* put each segment of each library in the segment list */
	for(i = 0; i < nlibs; i++){
	    lc = libs[i].ofile->load_commands;
	    for(j = 0; j < libs[i].ofile->mh->ncmds; j++){
		if(lc->cmd == LC_SEGMENT){
		    sg = (struct segment_command *)lc;
		    segments = reallocate(segments, 
				      (nsegments + 1) * sizeof(struct segment));
		    segments[nsegments].file_name = libs[i].file_name;
		    segments[nsegments].sg = sg;
		    nsegments++;
		}
		lc = (struct load_command *)((char *)lc + lc->cmdsize);
	    }
	}

	/* check each segment against all others */
	for(i = 0; i < nsegments; i++){
	    for(j = i + 1; j < nsegments; j++){
		check_overlap(segments + i, segments + j);
	    }
	}
}

/*
 * check_overlap() checks if the two segments passed to it overlap and if so
 * prints an error message and exit with a value of 2 indicating the prebinding
 * can't be redone.
 */
static
void
check_overlap(
struct segment *s1,
struct segment *s2)
{
	if(s1->sg->vmsize == 0 || s2->sg->vmsize == 0)
	    return;

	if(s1->sg->vmaddr > s2->sg->vmaddr){
	    if(s2->sg->vmaddr + s2->sg->vmsize <= s1->sg->vmaddr)
		return;
	}
	else{
	    if(s1->sg->vmaddr + s1->sg->vmsize <= s2->sg->vmaddr)
		return;
	}
	error("prebinding can't be redone because %.16s segment (address = 0x%x"
	      " size = 0x%x) of %s overlaps with %.16s segment (address = 0x%x "
	      "size = 0x%x) of %s (for architecture %s)",
	      s1->sg->segname, (unsigned int)(s1->sg->vmaddr),
	      (unsigned int)(s1->sg->vmsize), s1->file_name,
	      s2->sg->segname, (unsigned int)(s2->sg->vmaddr),
	      (unsigned int)(s2->sg->vmsize), s2->file_name,
	      arch_name);
	exit(2);
}

/*
 * setup_symbolic_info() sets up all the symbolic info in the arch and loaded
 * libraries by swapping it into the host bytesex if needed and checking it to
 * be valid.
 */
static
void
setup_symbolic_info(
void)
{
    unsigned long i, j;
    enum byte_sex host_byte_sex;
    struct load_command *lc;

	host_byte_sex = get_host_byte_sex();
	arch_swapped = arch->object->object_byte_sex != host_byte_sex;

	if(arch->object->st == NULL){
	    error("malformed file: %s (no LC_SYMTAB load command) (for"
		  " architecture %s)", arch->file_name, arch_name);
	    exit(2);
	}
	if(arch->object->dyst == NULL){
	    error("malformed file: %s (no LC_DYSYMTAB load command) (for"
		  " architecture %s)", arch->file_name, arch_name);
	    exit(2);
	}

	arch_symbols = (struct nlist *)(arch->object->object_addr +
				   arch->object->st->symoff);
	arch_nsyms = arch->object->st->nsyms;
	if(arch_swapped == TRUE)
	    swap_nlist(arch_symbols, arch_nsyms, host_byte_sex);
	arch_strings = arch->object->object_addr + arch->object->st->stroff;
	arch_strsize = arch->object->st->strsize;

	arch_extrelocs = (struct relocation_info *)
		(arch->object->object_addr +
		 arch->object->dyst->extreloff);
	arch_nextrel = arch->object->dyst->nextrel;
	if(arch_swapped == TRUE)
	    swap_relocation_info(arch_extrelocs, arch_nextrel, host_byte_sex);

	arch_indirect_symtab = (unsigned long *)
		(arch->object->object_addr +
		 arch->object->dyst->indirectsymoff);
	arch_nindirectsyms = arch->object->dyst->nindirectsyms;
	if(arch_swapped == TRUE)
	    swap_indirect_symbols(arch_indirect_symtab, arch_nindirectsyms,
		host_byte_sex);
	
	if(arch->object->mh->filetype == MH_DYLIB){
	    arch_tocs = (struct dylib_table_of_contents *)
		    (arch->object->object_addr +
		     arch->object->dyst->tocoff);
	    arch_ntoc = arch->object->dyst->ntoc;
	    arch_mods = (struct dylib_module *)
		    (arch->object->object_addr +
		     arch->object->dyst->modtaboff);
	    arch_nmodtab = arch->object->dyst->nmodtab;
	    arch_refs = (struct dylib_reference *)
		    (arch->object->object_addr +
		     arch->object->dyst->extrefsymoff);
	    arch_nextrefsyms = arch->object->dyst->nextrefsyms;
	    if(arch_swapped == TRUE){
		swap_dylib_table_of_contents(
		    arch_tocs, arch_ntoc, host_byte_sex);
		swap_dylib_module(
		    arch_mods, arch_nmodtab, host_byte_sex);
		swap_dylib_reference(
		    arch_refs, arch_nextrefsyms, host_byte_sex);
	    }
	}
	else{
	    arch_tocs = NULL;
	    arch_ntoc = 0;;
	    arch_mods = NULL;
	    arch_nmodtab = 0;;
	    arch_refs = NULL;
	    arch_nextrefsyms = 0;;
	}
	check_symbolic_info_tables(
	    arch->file_name,
	    arch->object->st,
	    arch->object->dyst,
	    arch_symbols,
	    arch_nsyms,
	    arch_strings,
	    arch_strsize,
	    arch_tocs,
	    arch_ntoc,
	    arch_mods,
	    arch_nmodtab,
	    arch_refs,
	    arch_nextrefsyms);

	/*
	 * Get all the symbolic info for the libraries the arch uses in the
	 * correct byte sex.
	 */
	for(i = 0; i < nlibs; i++){
	    libs[i].st = NULL;
	    libs[i].dyst = NULL;
	    lc = libs[i].ofile->load_commands;
	    for(j = 0; j < libs[i].ofile->mh->ncmds; j++){
		if(lc->cmd == LC_SYMTAB){
		    if(libs[i].st != NULL){
			error("malformed library: %s (more than one LC_SYMTAB "
			      "load command) (for architecture %s)",
			      libs[i].file_name, arch_name);
			exit(2);
		    }
		    libs[i].st = (struct symtab_command *)lc;
		}
		else if(lc->cmd == LC_DYSYMTAB){
		    if(libs[i].dyst != NULL){
			error("malformed library: %s (more than one LC_DYSYMTAB"
			      " load command) (for architecture %s)",
			      libs[i].file_name, arch_name);
			exit(2);
		    }
		    libs[i].dyst = (struct dysymtab_command *)lc;
		}
		else if(lc->cmd == LC_ROUTINES){
		    if(libs[i].rc != NULL){
			error("malformed library: %s (more than one LC_ROUTINES"
			      " load command) (for architecture %s)",
			      libs[i].file_name, arch_name);
			exit(2);
		    }
		    libs[i].rc = (struct routines_command *)lc;
		}
		lc = (struct load_command *)((char *)lc + lc->cmdsize);
	    }
	    if(libs[i].st == NULL){
		error("malformed file: %s (no LC_SYMTAB load command) (for"
		      " architecture %s)", libs[i].file_name, arch_name);
		exit(2);
	    }
	    if(libs[i].dyst == NULL){
		error("malformed file: %s (no LC_DYSYMTAB load command) (for"
		      " architecture %s)", libs[i].file_name, arch_name);
		exit(2);
	    }

	    libs[i].symbols = (struct nlist *)(libs[i].ofile->object_addr +
				       libs[i].st->symoff);
	    libs[i].nsyms = libs[i].st->nsyms;
	    libs[i].strings = libs[i].ofile->object_addr + libs[i].st->stroff;
	    libs[i].strsize = libs[i].st->strsize;
	    libs[i].tocs = (struct dylib_table_of_contents *)
		    (libs[i].ofile->object_addr + libs[i].dyst->tocoff);
	    libs[i].ntoc = libs[i].dyst->ntoc;
	    libs[i].mods = (struct dylib_module *)
		    (libs[i].ofile->object_addr +
		     libs[i].dyst->modtaboff);
	    libs[i].nmodtab = libs[i].dyst->nmodtab;
	    libs[i].refs = (struct dylib_reference *)
		    (libs[i].ofile->object_addr +
		     libs[i].dyst->extrefsymoff);
	    libs[i].nextrefsyms = libs[i].dyst->nextrefsyms;
	    if(arch_swapped == TRUE){
		swap_nlist(
		   libs[i].symbols, libs[i].nsyms, host_byte_sex);
		swap_dylib_table_of_contents(
		    libs[i].tocs, libs[i].ntoc, host_byte_sex);
		swap_dylib_module(
		    libs[i].mods, libs[i].nmodtab, host_byte_sex);
		swap_dylib_reference(
		    libs[i].refs, libs[i].nextrefsyms, host_byte_sex);
	    }
	    check_symbolic_info_tables(
		libs[i].file_name,
		libs[i].st,
		libs[i].dyst,
		libs[i].symbols,
		libs[i].nsyms,
		libs[i].strings,
		libs[i].strsize,
		libs[i].tocs,
		libs[i].ntoc,
		libs[i].mods,
		libs[i].nmodtab,
		libs[i].refs,
		libs[i].nextrefsyms);
	    libs[i].module_states = allocate(libs[i].nmodtab *
					     sizeof(enum link_state));
	    memset(libs[i].module_states, '\0',
		   libs[i].nmodtab * sizeof(enum link_state));
	}
}

static
void
swap_arch_for_output(
void)
{
	swap_nlist(arch_symbols, arch_nsyms,
	    arch->object->object_byte_sex);
	swap_relocation_info(arch_extrelocs, arch_nextrel,
	    arch->object->object_byte_sex);
	swap_indirect_symbols(arch_indirect_symtab, arch_nindirectsyms,
	    arch->object->object_byte_sex);
	swap_dylib_table_of_contents(arch_tocs, arch_ntoc,
	    arch->object->object_byte_sex);
	swap_dylib_module(arch_mods, arch_nmodtab,
	    arch->object->object_byte_sex);
	swap_dylib_reference(arch_refs, arch_nextrefsyms,
	    arch->object->object_byte_sex);
}

/*
 * check_symbolic_info_tables() checks to see that the parts of the symbolic
 * info used to redo the prebinding is valid.
 */
static
void
check_symbolic_info_tables(
char *file_name,
struct symtab_command *st,
struct dysymtab_command *dyst,
struct nlist *symbols,
unsigned long nsyms,
char *strings,
unsigned long strsize,
struct dylib_table_of_contents *tocs,
unsigned long ntoc,
struct dylib_module *mods,
unsigned long nmodtab,
struct dylib_reference *refs,
unsigned long nextrefsyms)
{
    unsigned long i;

	/* check the symbol table's offsets into the string table */
	for(i = 0; i < nsyms; i++){
	    if(symbols[i].n_un.n_strx > strsize){
		error("mallformed file: %s (bad string table index (%ld) for "
		      "symbol %lu) (for architecture %s)", file_name,
		      symbols[i].n_un.n_strx, i, arch_name);
		exit(2);
	    }
	    if((symbols[i].n_type & N_TYPE) == N_INDR &&
		symbols[i].n_value > strsize){
		error("mallformed file: %s (bad string table index (%ld) for "
		      "N_INDR symbol %lu) (for architecture %s)", file_name,
		      symbols[i].n_value, i, arch_name);
		exit(2);
	    }
	}

	/* check toc's symbol and module indexes */
	for(i = 0; i < ntoc; i++){
	    if(tocs[i].symbol_index > nsyms){
		error("mallformed file: %s (bad symbol table index (%ld) for "
		      "table of contents entry %lu) (for architecture %s)",
		      file_name, tocs[i].symbol_index, i, arch_name);
		exit(2);
	    }
	    if(tocs[i].module_index > nmodtab){
		error("mallformed file: %s (bad module table index (%ld) for "
		      "table of contents entry %lu) (for architecture %s)",
		      file_name, tocs[i].module_index, i, arch_name);
		exit(2);
	    }
	}

	/* check module table's string index for module names */
	for(i = 0; i < nmodtab; i++){
	    if(mods[i].module_name > strsize){
		error("mallformed file: %s (bad string table index (%ld) for "
		      "module_name in module table entry %lu ) (for "
		      "architecture %s)", file_name, mods[i].module_name, i,
		      arch_name);
		exit(2);
	    }
	    if(mods[i].nextdefsym != 0 &&
	       (mods[i].iextdefsym < dyst->iextdefsym ||
	        mods[i].iextdefsym >= dyst->iextdefsym + dyst->nextdefsym)){
		error("mallformed file: %s (bad external symbol table index for"
		      " for module table entry %lu) (for architecture %s)",
		      file_name, i, arch_name);
		exit(2);
	    }
	    if(mods[i].nextdefsym != 0 &&
	       mods[i].iextdefsym + mods[i].nextdefsym >
	       dyst->iextdefsym + dyst->nextdefsym){
		error("mallformed file: %s (bad number of external symbol table"
		      " entries for module table entry %lu) (for architecture "
		      "%s)", file_name, i, arch_name);
		exit(2);
	    }
	}

	/* check refernce table's symbol indexes */
	for(i = 0; i < nextrefsyms; i++){
	    if(refs[i].isym > nsyms){
		error("mallformed file: %s (bad external symbol table index "
		      "reference table entry %lu) (for architecture %s)",
		      file_name, i, arch_name);
		exit(2);
	    }
	}
}

/*
 * check_for_dylib_override_symbols() checks to make sure symbols in this arch
 * are not overriding symbols in dependent dylibs which the dependent library
 * also uses.  This is to verify prebinding can be redone.
 */
static
void
check_for_dylib_override_symbols(
void)
{
    unsigned long i;

	for(i = arch->object->dyst->iextdefsym;
	    i < arch->object->dyst->iextdefsym + arch->object->dyst->nextdefsym;
	    i++){
	    check_dylibs_for_definition(
		arch->file_name, arch_strings + arch_symbols[i].n_un.n_strx);
	}
}

/*
 * check_dylibs_for_definition() checks to see if the symbol name is defined
 * in any of the dependent dynamic shared libraries.  If it is a an error
 * message is printed and exit(2) is done to indicate the prebinding can't be
 * redone.
 */
static
void
check_dylibs_for_definition(
char *file_name,
char *symbol_name)
{
    unsigned long i;
    struct dylib_table_of_contents *toc;

	for(i = 0; i < nlibs; i++){
	    bsearch_strings = libs[i].strings;
	    bsearch_symbols = libs[i].symbols;
	    toc = bsearch(symbol_name, libs[i].tocs, libs[i].ntoc,
			  sizeof(struct dylib_table_of_contents),
			  (int (*)(const void *, const void *))dylib_bsearch);
	    if(toc != NULL){
		/*
		 * There is a module that defineds this symbol.  If this
		 * symbol is also referenced by the libraries then we
		 * can't redo the prebindng.
		 */
		if(check_dylibs_for_reference(symbol_name) == TRUE){
		    error("prebinding can't be redone because of symbols "
		       "overridded in dependent dynamic shared libraries (%s "
		       "defined in: %s and in %s(%s)) (for architecture %s)",
		       symbol_name, file_name, libs[i].file_name,
		       libs[i].strings +
			    libs[i].mods[toc->module_index].module_name,
		       arch_name);
		    exit(2);
		}
	    }
	}
}

/*
 * check_dylibs_for_reference() checks the dependent dynamic shared libraries
 * to see if the specified merged symbol is referenced.  If it is TRUE is
 * returned else FALSE is returned.
 */
static
enum bool
check_dylibs_for_reference(
char *symbol_name)
{
    unsigned long i, j, symbol_index;
    struct dylib_table_of_contents *toc;
    struct nlist *symbol;

	for(i = 0; i < nlibs; i++){
	    /*
	     * See if this symbol appears at all (defined or undefined)
	     * in this library.
	     */
	    bsearch_strings = libs[i].strings;
	    bsearch_symbols = libs[i].symbols;
	    toc = bsearch(symbol_name, libs[i].tocs, libs[i].ntoc,
			  sizeof(struct dylib_table_of_contents),
			  (int (*)(const void *, const void *))dylib_bsearch);
	    if(toc != NULL){
		symbol_index = toc->symbol_index;
	    }
	    else{
		symbol = bsearch(symbol_name,
			 libs[i].symbols + libs[i].dyst->iundefsym,
			 libs[i].dyst->nundefsym,
			 sizeof(struct nlist),
			 (int (*)(const void *,const void *))nlist_bsearch);
		if(symbol == NULL)
		    continue;
		symbol_index = symbol - libs[i].symbols;
	    }
	    /*
	     * The symbol appears in this library.  Now see if it is
	     * referenced by a module in the library.
	     */
	    for(j = 0; j < libs[i].nextrefsyms; j++){
		if(libs[i].refs[j].isym == symbol_index &&
		   (libs[i].refs[j].flags ==
			REFERENCE_FLAG_UNDEFINED_NON_LAZY ||
		    libs[i].refs[j].flags ==
			REFERENCE_FLAG_UNDEFINED_LAZY))
		return(TRUE);
	    }
	}
	return(FALSE);
}

/*
 * Function for bsearch() for finding a symbol name in a dylib table of
 * contents.
 */
static
int
dylib_bsearch(
const char *symbol_name,
const struct dylib_table_of_contents *toc)
{
        return(strcmp(symbol_name,
                      bsearch_strings +
		      bsearch_symbols[toc->symbol_index].n_un.n_strx));
}

/*
 * Function for bsearch() for finding a symbol name in the sorted list of
 * undefined symbols.
 */
static
int
nlist_bsearch(
const char *symbol_name,
const struct nlist *symbol)
{
	return(strcmp(symbol_name, bsearch_strings + symbol->n_un.n_strx));
}

/*
 * setup_initial_undefined_list() builds the initial list of undefined symbol
 * references based on the arch's undefined symbols.
 */
static
void
setup_initial_undefined_list(
void)
{
    unsigned long i;

	for(i = arch->object->dyst->iundefsym;
	    i < arch->object->dyst->iundefsym + arch->object->dyst->nundefsym;
	    i++){
		add_to_undefined_list(
		    arch_strings + arch_symbols[i].n_un.n_strx);
	}
}

/*
 * link_in_need_modules() causes any needed modules to be linked into the
 * program.
 */
static
void
link_in_need_modules(
void)
{
    struct symbol_list *undefined, *next_undefined;
    struct nlist *symbol;
    enum link_state *module_state;
    struct lib *lib;

	for(undefined = undefined_list.next;
	    undefined != &undefined_list;
	    /* no increment expression */){

	    /*
	     * Look up the symbol, if is not found we can't redo the prebinding.
	     * So leave it on the undefined list and if there are undefined
	     * symbols on the list after all the undefined symbols have been
	     * searched for a message will be printed and exit(2) will be done
	     * to indicate this.
	     */
	    lookup_symbol(undefined->name, &symbol, &module_state, &lib,
			  NO_INDR_LOOP);
	    if(symbol != NULL){
		/*
		 * The symbol was found so remove it from the undefined_list.
		 * Then if the module that defined this symbol is unlinked
		 * then link it in checking for multiply defined symbols.
		 */
		/* take this off the undefined list */
		next_undefined = undefined->next;
		undefined->prev->next = undefined->next;
		undefined->next->prev = undefined->prev;
		undefined = next_undefined;

		if(*module_state == UNLINKED)
		    link_library_module(module_state, lib);

		if(undefined == &undefined_list &&
		   undefined->next != &undefined_list)
		    undefined = undefined->next;
	    }
	    else{
		undefined = undefined->next;
	    }
	}

	if(undefined_list.next != &undefined_list){
	    printf("%s: prebinding can't be redone for: %s (for architecture "
		   "%s) because of undefined symbols:\n", progname,
		   arch->file_name, arch_name);
	    for(undefined = undefined_list.next;
		undefined != &undefined_list;
		undefined = undefined->next){
		    printf("%s\n", undefined->name);
		}
	    exit(2);
	}
}

/*
 * link_library_module() links in the specified library module. It checks the
 * module for symbols that are already defined and reports multiply defined
 * errors.  Then it adds it's undefined symbols to the undefined list.
 */
static
void
link_library_module(
enum link_state *module_state,
struct lib *lib)
{
    unsigned long i, j, module_index;
    struct dylib_module *dylib_module;
    char *name;
    struct nlist *prev_symbol;
    enum link_state *prev_module_state;
    struct lib *prev_lib;
    struct nlist *ref_symbol;
    enum link_state *ref_module_state;
    struct lib *ref_lib;

	module_index = module_state - lib->module_states;
	dylib_module = lib->mods + module_index;

	/*
	 * For each defined symbol check to see if it is not defined in a module
	 * that is already linked (or being linked).
	 */
	for(i = dylib_module->iextdefsym;
	    i < dylib_module->iextdefsym + dylib_module->nextdefsym;
	    i++){

	    name = lib->strings + lib->symbols[i].n_un.n_strx;
	    lookup_symbol(name, &prev_symbol, &prev_module_state, &prev_lib,
			  NO_INDR_LOOP);
	    if(prev_symbol != NULL &&
	       module_state != prev_module_state &&
	       *prev_module_state != UNLINKED){
		printf("%s: prebinding can't be redone for: %s (for "
		       "architecture %s) because of multiply defined "
		       "symbol: %s\n", progname, arch->file_name, arch_name,
			name);
		if(prev_module_state == &arch_state)
		    printf("%s definition of %s\n", arch->file_name, name);
		else
		    printf("%s(%s) definition of %s\n", prev_lib->file_name,
			   prev_lib->strings +
			   prev_lib->mods[
				prev_module_state - prev_lib->module_states].
				module_name, name);
		if(module_state == &arch_state)
		    printf("%s definition of %s\n", arch->file_name, name);
		else
		    printf("%s(%s) definition of %s\n", lib->file_name,
			   lib->strings + dylib_module->module_name, name);
		exit(2);
	    }
	}

	/*
	 * For each reference to an undefined symbol look it up to see if it is
	 * defined in an already linked module.  If it is not then add it to
	 * the undefined list.
	 */
	for(i = dylib_module->irefsym;
	    i < dylib_module->irefsym + dylib_module->nrefsym;
	    i++){

	    if(lib->refs[i].flags == REFERENCE_FLAG_UNDEFINED_NON_LAZY ||
	       lib->refs[i].flags == REFERENCE_FLAG_UNDEFINED_LAZY){
		name = lib->strings +
		       lib->symbols[lib->refs[i].isym].n_un.n_strx;
		lookup_symbol(name, &ref_symbol, &ref_module_state, &ref_lib,
			      NO_INDR_LOOP);
		if(ref_symbol != NULL){
		    if(*ref_module_state == UNLINKED)
			add_to_undefined_list(name);

		}
		else{
		    add_to_undefined_list(name);
		}
	    }
	    else{
		/*
		 * If this is a reference to a private extern make sure the
		 * module that defineds it is linked and if not set cause it
		 * to be linked.  References to private externs in a library
		 * only are resolved to symbols in the same library and modules
		 * in a library that have private externs can't have any global
		 * symbols (this is done by the static link editor).  The reason
		 * this is done at all is so that module is marked as linked.
		 */
		if(lib->refs[i].flags ==
				   REFERENCE_FLAG_PRIVATE_UNDEFINED_NON_LAZY ||
		   lib->refs[i].flags ==
				   REFERENCE_FLAG_PRIVATE_UNDEFINED_LAZY){
		    for(j = 0; j < lib->nmodtab; j++){
			if(lib->refs[i].isym >= lib->mods[j].ilocalsym &&
			   lib->refs[i].isym <
			       lib->mods[j].ilocalsym + lib->mods[j].nlocalsym)
			    break;
		    }
		    if(j < lib->nmodtab){
			if(lib->module_states[j] == UNLINKED)
			    lib->module_states[j] = LINKED;
		    }
		}
	    }
	}

	*module_state = LINKED;

	/*
	 * If this library has a shared library initialization routine then
	 * make sure this module is linked in.  If not link it in.
	 */
	if(lib->rc != NULL &&
	   lib->module_states[lib->rc->init_module] == UNLINKED){
	   link_library_module(lib->module_states + lib->rc->init_module, lib);
	}
}

/*
 * add_to_undefined_list() adds an item to the list of undefined symbols.
 */
static
void
add_to_undefined_list(
char *name)
{
    struct symbol_list *undefined, *new;

	for(undefined = undefined_list.next;
	    undefined != &undefined_list;
	    undefined = undefined->next){
	    if(undefined->name == name)
		return;
	}

	/* get a new symbol list entry */
	new = allocate(sizeof(struct symbol_list));

	/* fill in the pointers for the undefined symbol */
	new->name = name;

	/* put this at the end of the undefined list */
	new->prev = undefined_list.prev;
	new->next = &undefined_list;
	undefined_list.prev->next = new;
	undefined_list.prev = new;
}

/*
 * lookup_symbol() is passed a name of a symbol.  The name is looked up in the
 * current arch and the libs.  If found symbol, module_state and lib is set
 * to indicate where the symbol is defined.
 */
static
void
lookup_symbol(
char *name,
struct nlist **symbol,
enum link_state **module_state,
struct lib **lib,
struct indr_loop_list *indr_loop)
{
    unsigned long i;
    struct dylib_table_of_contents *toc;
    struct nlist *s;
    struct indr_loop_list new_indr_loop, *loop;

	/*
	 * First search the current arch for the named symbol as a defined
	 * external symbol.  If the current arch is a dylib look in the table
	 * of contents else look in the sorted external symbols.
	 */
	if(arch->object->mh->filetype == MH_DYLIB){
	    bsearch_strings = arch_strings;
	    bsearch_symbols = arch_symbols;
	    toc = bsearch(name, arch_tocs, arch_ntoc,
			  sizeof(struct dylib_table_of_contents),
			  (int (*)(const void *, const void *))dylib_bsearch);
	    if(toc != NULL){
		*symbol = arch_symbols + toc->symbol_index;
		if(((*symbol)->n_type & N_TYPE) == N_INDR){
		    name = (*symbol)->n_value + arch_strings;
		    goto indr;
		}
		*module_state = &arch_state;
		*lib = NULL;
		return;
	    }
	}
	else{
	    bsearch_strings = arch_strings;
	    s = bsearch(name,
			arch_symbols + arch->object->dyst->iextdefsym,
			arch->object->dyst->nextdefsym,
			sizeof(struct nlist),
			(int (*)(const void *,const void *))nlist_bsearch);
	    if(s != NULL){
		*symbol = s;
		if(((*symbol)->n_type & N_TYPE) == N_INDR){
		    name = (*symbol)->n_value + arch_strings;
		    goto indr;
		}
		*module_state = &arch_state;
		*lib = NULL;
		return;
	    }
	}

	/*
	 * The symbol was not found in the current arch so look through the
	 * dependent libs for the a definition of the named symbol.
	 */
	for(i = 0; i < nlibs; i++){
	    bsearch_strings = libs[i].strings;
	    bsearch_symbols = libs[i].symbols;
	    toc = bsearch(name, libs[i].tocs, libs[i].ntoc,
			  sizeof(struct dylib_table_of_contents),
			  (int (*)(const void *, const void *))dylib_bsearch);
	    if(toc != NULL){
		*symbol = libs[i].symbols + toc->symbol_index;
		if(((*symbol)->n_type & N_TYPE) == N_INDR){
		    name = (*symbol)->n_value + libs[i].strings;
		    goto indr;
		}
		*module_state = libs[i].module_states + toc->module_index;
		*lib = libs + i;
		return;
	    }
	}

	*symbol = NULL;
	*module_state = NULL;
	*lib = NULL;
	return;

indr:
	if(indr_loop != NO_INDR_LOOP){
	    for(loop = indr_loop; loop != NULL; loop = loop->next){
		if(loop->symbol == *symbol){
		    /* this is an indirect loop */
		    *symbol = NULL;
		    *module_state = NULL;
		    *lib = NULL;
		    return;
		}
	    }
	}
	new_indr_loop.symbol = *symbol;
	new_indr_loop.next = indr_loop;
	lookup_symbol(name, symbol, module_state, lib, &new_indr_loop);
	return;
}

/*
 * build_new_symbol_table() builds a new symbol table for the current arch
 * using the new values for prebound undefined symbols from the dependent
 * libraries.
 */
static
void
build_new_symbol_table(
void)
{
    unsigned long i, sym_info_size;
    char *symbol_name;
    struct nlist *new_symbols;
    struct nlist *symbol;
    enum link_state *module_state;
    struct lib *lib;

	/* the size of the symbol table will not change just the contents */
	sym_info_size =
	    arch_nextrel * sizeof(struct relocation_info) +
	    arch->object->dyst->nlocrel * sizeof(struct relocation_info) +
	    arch_nindirectsyms * sizeof(unsigned long *) +
	    arch_ntoc * sizeof(struct dylib_table_of_contents) +
	    arch_nmodtab * sizeof(struct dylib_module) +
	    arch_nextrefsyms * sizeof(struct dylib_reference) +
	    arch_nsyms * sizeof(struct nlist) +
	    arch_strsize;

	arch->object->input_sym_info_size = sym_info_size;
	arch->object->output_sym_info_size = sym_info_size;

	arch->object->output_nsymbols = arch_nsyms;
	arch->object->output_strings_size = arch_strsize;

	arch->object->output_ilocalsym = arch->object->dyst->ilocalsym;
	arch->object->output_nlocalsym = arch->object->dyst->nlocalsym;
	arch->object->output_iextdefsym = arch->object->dyst->iextdefsym;
	arch->object->output_nextdefsym = arch->object->dyst->nextdefsym;
	arch->object->output_iundefsym = arch->object->dyst->iundefsym;
	arch->object->output_nundefsym = arch->object->dyst->nundefsym;

	arch->object->output_loc_relocs = (struct relocation_info *)
		(arch->object->object_addr +
		 arch->object->dyst->locreloff);
	arch->object->output_ext_relocs = arch_extrelocs;
	arch->object->output_indirect_symtab = arch_indirect_symtab;

	arch->object->output_tocs = arch_tocs;
	arch->object->output_ntoc = arch_ntoc;
	arch->object->output_mods = arch_mods;
	arch->object->output_nmodtab = arch_nmodtab;
	arch->object->output_refs = arch_refs;
	arch->object->output_nextrefsyms = arch_nextrefsyms;

	/*
	 * The new symbol table is just a copy of the old symbol table with
	 * the n_value's of the prebound undefined symbols updated.
	 */
	new_symbols = allocate(arch_nsyms * sizeof(struct nlist));
	memcpy(new_symbols, arch_symbols, arch_nsyms * sizeof(struct nlist));
	for(i = arch->object->dyst->iundefsym;
	    i < arch->object->dyst->iundefsym + arch->object->dyst->nundefsym;
	    i++){

	    symbol_name = arch_strings + arch_symbols[i].n_un.n_strx;
	    lookup_symbol(symbol_name, &symbol, &module_state, &lib,
			  NO_INDR_LOOP);
	    new_symbols[i].n_value = symbol->n_value;
	}
	arch->object->output_symbols = new_symbols;

	/* the strings don't change so just use the existing string table */
	arch->object->output_strings = arch_strings;
}

/*
 * update_external_relocs() drives the updating of the items with external
 * relocation entries for the current arch.
 */
static
void
update_external_relocs(
void)
{
    unsigned long i;
    struct load_command *lc;
    struct segment_command *sg;

	/* figure out what this arch's seg1addr or segs_read_write_addr is */
	arch_seg1addr = ULONG_MAX;
	arch_segs_read_write_addr = ULONG_MAX;
	if((arch->object->mh->flags & MH_SPLIT_SEGS) == MH_SPLIT_SEGS)
	    arch_split_segs = TRUE;
	else
	    arch_split_segs = FALSE;
	lc = arch->object->load_commands;
	for(i = 0; i < arch->object->mh->ncmds; i++){
	    if(lc->cmd == LC_SEGMENT){
		sg = (struct segment_command *)lc;
		if(sg->vmaddr < arch_seg1addr)
		    arch_seg1addr = sg->vmaddr;
		/*
		 * Pickup the address of the first read-write segment for
		 * MH_SPLIT_SEGS images.
		 */
		if((sg->initprot & VM_PROT_WRITE) == VM_PROT_WRITE &&
		   sg->vmaddr < arch_segs_read_write_addr)
		    arch_segs_read_write_addr = sg->vmaddr;

	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}

	switch(arch->object->mh->cputype){
	case CPU_TYPE_MC680x0:
	    update_generic_external_relocs();
	    break;
	case CPU_TYPE_I386:
	    update_generic_external_relocs();
	    break;
	case CPU_TYPE_HPPA:
	    update_hppa_external_relocs();
	    break;
	case CPU_TYPE_SPARC:
	    update_sparc_external_relocs();
	    break;
	case CPU_TYPE_POWERPC:
	    update_ppc_external_relocs();
	    break;
	default:
	    error("can't redo prebinding for: %s (for architecture %s) because "
		  "of unknown cputype", arch->file_name, arch_name);
	}
}

/*
 * update_generic_external_relocs() updates of the items with external
 * relocation entries for the architectures that use generic relocation entries
 * (the i386 and m68k architectures).  It only deals with external relocation
 * entries that are using prebound undefined symbols.
 */
static
void
update_generic_external_relocs(
void)
{
    unsigned long i, value;
    char *name, *p;
    struct nlist *defined_symbol, *arch_symbol;
    enum link_state *module_state;
    struct lib *lib;

	for(i = 0; i < arch_nextrel; i++){
	    /* check the r_symbolnum field */
	    if(arch_extrelocs[i].r_symbolnum > arch_nsyms){
		error("mallformed file: %s (bad symbol table index for "
		      "external relocation entry %lu) (for architecture %s)",
		      arch->file_name, i, arch_name);
		exit(2);
	    }

	    /*
	     * If the symbol this relocation entry is refering to is not a
	     * prebound undefined symbol then skip it.
	     */ 
	    arch_symbol = arch_symbols + arch_extrelocs[i].r_symbolnum;
	    if((arch_symbol->n_type & N_TYPE) != N_PBUD)
		continue;

	    /*
	     * Look up the symbol being referenced by this relocation entry to
	     * get the defined symbol's value to be used.
	     */
	    name = arch_strings + arch_symbol->n_un.n_strx;
	    lookup_symbol(name, &defined_symbol, &module_state, &lib,
			  NO_INDR_LOOP);

	    p = contents_pointer_for_vmaddr(arch_extrelocs[i].r_address +
				(arch_split_segs == TRUE ?
				 arch_segs_read_write_addr : arch_seg1addr),
					    1 << arch_extrelocs[i].r_length);
	    if(p == NULL){
		error("mallformed file: %s (for architecture %s) (bad r_address"
		      " field for external relocation entry %lu)",
		      arch->file_name, arch_name, i);
		exit(2);
	    }

	    switch(arch_extrelocs[i].r_length){
	    case 0: /* byte */
		value = get_arch_byte(p);
		value = (value - arch_symbol->n_value) +
			defined_symbol->n_value;
		if( (value & 0xffffff00) &&
		   ((value & 0xffffff80) != 0xffffff80)){
		    error("prebinding can't be redone for: %s (for architecture"
			" %s) because of relocation overflow (external "
			"relocation for symbol %s does not fit in 1 byte)",
			arch->file_name, arch_name, name);
		    exit(2);
		}
		set_arch_byte(p, value);
		break;
	    case 1: /* word (2 byte) */
		value = get_arch_short(p);
		value = (value - arch_symbol->n_value) +
			defined_symbol->n_value;
		if( (value & 0xffff0000) &&
		   ((value & 0xffff8000) != 0xffff8000)){
		    error("prebinding can't be redone for: %s (for architecture"
			" %s) because of relocation overflow (external "
			"relocation for symbol %s does not fit in 2 bytes)",
			arch->file_name, arch_name, name);
		    exit(2);
		}
		set_arch_short(p, value);
		break;
	    case 2: /* long (4 byte) */
		value = get_arch_long(p);
		value = (value - arch_symbol->n_value) +
			defined_symbol->n_value;
		set_arch_long(p, value);
		break;
	    default:
		error("mallformed file: %s (external relocation entry "
		      "%lu has bad r_length) (for architecture %s)",
		      arch->file_name, i, arch_name);
		exit(2);
	    }
	}
}

/*
 * update_hppa_external_relocs() updates of the items with external relocation
 * entries for the hppa architecture.  It only deals with external relocation
 * entries that are using prebound undefined symbols.
 */
static
void
update_hppa_external_relocs(
void)
{
    unsigned long i, value;
    char *name, *p;
    struct nlist *defined_symbol, *arch_symbol;
    enum link_state *module_state;
    struct lib *lib;
    unsigned long instruction, immediate;
    unsigned long other_half;
    unsigned long hi21, lo14;
    unsigned long w, w1, w2;

	for(i = 0; i < arch_nextrel; i++){
	    /* check the r_symbolnum field */
	    if(arch_extrelocs[i].r_symbolnum > arch_nsyms){
		error("mallformed file: %s (bad symbol table index for "
		      "external relocation entry %lu) (for architecture %s)",
		      arch->file_name, i, arch_name);
		exit(2);
	    }
	    /* check to see if it needs a pair and has a correct one */
	    if(arch_extrelocs[i].r_type == HPPA_RELOC_HI21 ||
	       arch_extrelocs[i].r_type == HPPA_RELOC_LO14 ||
	       arch_extrelocs[i].r_type == HPPA_RELOC_BR17){
		if(i + 1 == arch_nextrel){
		    error("mallformed file: %s (missing pair external "
			  "relocation entry for entry %lu) (for architecture "
			  "%s)", arch->file_name, i, arch_name);
		    exit(2);
		}
		if(arch_extrelocs[i + 1].r_type != HPPA_RELOC_PAIR){
		    error("mallformed file: %s (pair external relocation entry "
			  "for entry %lu is not of r_type HPPA_RELOC_PAIR) (for"
			  " architecture %s)", arch->file_name, i, arch_name);
		    exit(2);
		}
	    }

	    /*
	     * If the symbol this relocation entry is refering to is not a
	     * prebound undefined symbol then skip it.
	     */ 
	    arch_symbol = arch_symbols + arch_extrelocs[i].r_symbolnum;
	    if((arch_symbol->n_type & N_TYPE) != N_PBUD)
		goto next;

	    /*
	     * Look up the symbol being referenced by this relocation entry to
	     * get the defined symbol's value to be used.
	     */
	    name = arch_strings + arch_symbol->n_un.n_strx;
	    lookup_symbol(name, &defined_symbol, &module_state, &lib,
			  NO_INDR_LOOP);

	    p = contents_pointer_for_vmaddr(arch_extrelocs[i].r_address +
				(arch_split_segs == TRUE ?
				 arch_segs_read_write_addr : arch_seg1addr),
					    1 << arch_extrelocs[i].r_length);
	    if(p == NULL){
		error("mallformed file: %s (for architecture %s) (bad r_address"
		      " field for external relocation entry %lu)",
		      arch->file_name, arch_name, i);
		exit(2);
	    }

	    if(arch_extrelocs[i].r_type == HPPA_RELOC_VANILLA){
		switch(arch_extrelocs[i].r_length){
		case 0: /* byte */
		    value = get_arch_byte(p);
		    value = (value - arch_symbol->n_value) +
			    defined_symbol->n_value;
		    if( (value & 0xffffff00) &&
		       ((value & 0xffffff80) != 0xffffff80)){
			error("prebinding can't be redone for: %s (for "
			    "architecture %s) because of relocation overflow "
			    "(external relocation for symbol %s does not fit "
			    "in 1 byte)", arch->file_name, arch_name, name);
			exit(2);
		    }
		    set_arch_byte(p, value);
		    break;
		case 1: /* word (2 byte) */
		    value = get_arch_short(p);
		    value = (value - arch_symbol->n_value) +
			    defined_symbol->n_value;
		    if( (value & 0xffff0000) &&
		       ((value & 0xffff8000) != 0xffff8000)){
			error("prebinding can't be redone for: %s (for "
			    "architecture %s) because of relocation overflow "
			    "(external relocation for symbol %s does not fit "
			    "in 2 bytes)", arch->file_name, arch_name, name);
			exit(2);
		    }
		    set_arch_short(p, value);
		    break;
		case 2: /* long (4 byte) */
		    value = get_arch_long(p);
		    value = (value - arch_symbol->n_value) +
			    defined_symbol->n_value;
		    set_arch_long(p, value);
		    break;
		default:
		    error("mallformed file: %s (external relocation entry "
			  "%lu has bad r_length) (for architecture %s)",
			  arch->file_name, i, arch_name);
		    exit(2);
		}
	    }
	    /*
	     * Do hppa specific relocation based on the r_type.
	     */
	    else{
		instruction = get_arch_long(p);
		switch(arch_extrelocs[i].r_type){
		case HPPA_RELOC_HI21:
		    other_half  = arch_extrelocs[i + 1].r_address;
		    immediate = sign_ext(other_half, 14) + 
		               (assemble_21(instruction & 0x1fffff) << 11);
		    calc_hppa_HILO(-arch_symbol->n_value +
				   defined_symbol->n_value +
				   immediate, 0, &hi21, &lo14);
		    instruction = (instruction & 0xffe00000) |
				  dis_assemble_21(hi21 >> 11);
		    arch_extrelocs[i + 1].r_address = lo14 & 0x3fff;
		    break;
		case HPPA_RELOC_LO14:
		    other_half  = arch_extrelocs[i + 1].r_address;
		    immediate = low_sign_ext(instruction & 0x3fff, 14) +
		    	        (other_half << 11);
		    calc_hppa_HILO(-arch_symbol->n_value +
				   defined_symbol->n_value +
				   immediate, 0, &hi21, &lo14);
		    lo14 = low_sign_unext(lo14, 14);
		    instruction = (instruction & 0xffffc000) |
				  (lo14 & 0x3fff);
		    arch_extrelocs[i + 1].r_address = hi21 >> 11;
		    break;
		case HPPA_RELOC_BR17:
		    other_half  = arch_extrelocs[i + 1].r_address;
		    immediate = assemble_17((instruction & 0x1f0000) >> 16,
			                    (instruction & 0x1ffc) >> 2,
				             instruction & 1);
		    immediate = (sign_ext(immediate, 17) << 2) +
				(other_half << 11);
		    calc_hppa_HILO(-arch_symbol->n_value +
				   defined_symbol->n_value +
				   immediate, 0, &hi21, &lo14);
		    lo14 >>= 2;
		    dis_assemble_17(lo14, &w1, &w2, &w);
		    instruction = (instruction & 0xffe0e002) |
				  (w1 << 16) | (w2 << 2) | w;
		    arch_extrelocs[i + 1].r_address = hi21 >> 11;
		    break;
		case HPPA_RELOC_BL17:
		    immediate = assemble_17((instruction & 0x1f0000) >> 16,
			                    (instruction & 0x1ffc) >> 2,
				             instruction & 1);
		    if((immediate & 0x10000) != 0)
			immediate |= 0xfffe0000;
		    immediate <<= 2;
		    immediate -= arch_symbol->n_value;
		    immediate += defined_symbol->n_value;
		    if(U_ABS(immediate) > 0x3ffff){
			error("prebinding can't be redone for: %s (for "
			    "architecture %s) because of relocation overflow "
			    "(external relocation for symbol %s displacement "
			    "too large to fit)", arch->file_name, arch_name,
			    name);
			exit(2);
		    }
		    immediate >>= 2;
		    dis_assemble_17(immediate, &w1, &w2, &w);
		    instruction = (instruction & 0xffe0e002) |
				  (w1 << 16) | (w2 << 2) | w;
		    break;
		default:
		    error("mallformed file: %s (external relocation entry "
			  "%lu has unknown r_type) (for architecture %s)",
			  arch->file_name, i, arch_name);
		    exit(2);
		}
		set_arch_long(p, instruction);
	    }
next:
	    /*
	     * If the relocation entry had a pair step over it.
	     */
	    if(arch_extrelocs[i].r_type == HPPA_RELOC_HI21 ||
	       arch_extrelocs[i].r_type == HPPA_RELOC_LO14 ||
	       arch_extrelocs[i].r_type == HPPA_RELOC_BR17)
		i++;
	}
}

/*
 * update_sparc_external_relocs() updates of the items with external relocation
 * entries for the sparc architecture.  It only deals with external relocation
 * entries that are using prebound undefined symbols.
 */
static
void
update_sparc_external_relocs(
void)
{
    unsigned long i, value;
    char *name, *p;
    struct nlist *defined_symbol, *arch_symbol;
    enum link_state *module_state;
    struct lib *lib;
    unsigned long instruction, immediate;
    unsigned long other_half;

	for(i = 0; i < arch_nextrel; i++){
	    /* check the r_symbolnum field */
	    if(arch_extrelocs[i].r_symbolnum > arch_nsyms){
		error("mallformed file: %s (bad symbol table index for "
		      "external relocation entry %lu) (for architecture %s)",
		      arch->file_name, i, arch_name);
		exit(2);
	    }
	    /* check to see if it needs a pair and has a correct one */
	    if(arch_extrelocs[i].r_type == SPARC_RELOC_LO10 ||
	       arch_extrelocs[i].r_type == SPARC_RELOC_HI22){
		if(i + 1 == arch_nextrel){
		    error("mallformed file: %s (missing pair external "
			  "relocation entry for entry %lu) (for architecture "
			  "%s)", arch->file_name, i, arch_name);
		    exit(2);
		}
		if(arch_extrelocs[i + 1].r_type != SPARC_RELOC_PAIR){
		    error("mallformed file: %s (pair external relocation entry "
			  "for entry %lu is not of r_type SPARC_RELOC_PAIR) "
			  "(for architecture %s)", arch->file_name, i,
			  arch_name);
		    exit(2);
		}
	    }

	    /*
	     * If the symbol this relocation entry is refering to is not a
	     * prebound undefined symbol then skip it.
	     */ 
	    arch_symbol = arch_symbols + arch_extrelocs[i].r_symbolnum;
	    if((arch_symbol->n_type & N_TYPE) != N_PBUD)
		goto next;

	    /*
	     * Look up the symbol being referenced by this relocation entry to
	     * get the defined symbol's value to be used.
	     */
	    name = arch_strings + arch_symbol->n_un.n_strx;
	    lookup_symbol(name, &defined_symbol, &module_state, &lib,
			  NO_INDR_LOOP);

	    p = contents_pointer_for_vmaddr(arch_extrelocs[i].r_address +
				(arch_split_segs == TRUE ?
				 arch_segs_read_write_addr : arch_seg1addr),
					    1 << arch_extrelocs[i].r_length);
	    if(p == NULL){
		error("mallformed file: %s (for architecture %s) (bad r_address"
		      " field for external relocation entry %lu)",
		      arch->file_name, arch_name, i);
		exit(2);
	    }

	    if(arch_extrelocs[i].r_type == SPARC_RELOC_VANILLA){
		switch(arch_extrelocs[i].r_length){
		case 0: /* byte */
		    value = get_arch_byte(p);
		    value = (value - arch_symbol->n_value) +
			    defined_symbol->n_value;
		    if( (value & 0xffffff00) &&
		       ((value & 0xffffff80) != 0xffffff80)){
			error("prebinding can't be redone for: %s (for "
			    "architecture %s) because of relocation overflow "
			    "(external relocation for symbol %s does not fit "
			    "in 1 byte)", arch->file_name, arch_name, name);
			exit(2);
		    }
		    set_arch_byte(p, value);
		    break;
		case 1: /* word (2 byte) */
		    value = get_arch_short(p);
		    value = (value - arch_symbol->n_value) +
			    defined_symbol->n_value;
		    if( (value & 0xffff0000) &&
		       ((value & 0xffff8000) != 0xffff8000)){
			error("prebinding can't be redone for: %s (for "
			    "architecture %s) because of relocation overflow "
			    "(external relocation for symbol %s does not fit "
			    "in 2 bytes)", arch->file_name, arch_name, name);
			exit(2);
		    }
		    set_arch_short(p, value);
		    break;
		case 2: /* long (4 byte) */
		    value = get_arch_long(p);
		    value = (value - arch_symbol->n_value) +
			    defined_symbol->n_value;
		    set_arch_long(p, value);
		    break;
		default:
		    error("mallformed file: %s (external relocation entry "
			  "%lu has bad r_length) (for architecture %s)",
			  arch->file_name, i, arch_name);
		    exit(2);
		}
	    }
	    /*
	     * Do SPARC specific relocation based on the r_type.
	     */
	    else{
		instruction = get_arch_long(p);
		switch(arch_extrelocs[i].r_type){
		case SPARC_RELOC_HI22:
		    other_half = (arch_extrelocs[i + 1].r_address) & 0x3ff;
		    immediate = ((instruction & 0x3fffff) << 10) | other_half;
		    immediate -= arch_symbol->n_value;
		    immediate += defined_symbol->n_value;
		    instruction = (instruction & 0xffc00000) |
				  ((immediate >> 10) & 0x3fffff);
		    arch_extrelocs[i + 1].r_address = immediate & 0x3ff;
		    break;
		case SPARC_RELOC_LO10:
		    other_half = ((arch_extrelocs[i + 1].r_address) >> 10) &
				 0x3fffff;
		    immediate = (instruction & 0x3ff) | (other_half << 10);
		    immediate -= arch_symbol->n_value;
		    immediate += defined_symbol->n_value;
		    instruction = (instruction & 0xfffffc00) |
				  (immediate & 0x3ff);
		    arch_extrelocs[i + 1].r_address =
				  (immediate >> 10) & 0x3fffff;
		    break;

		case SPARC_RELOC_WDISP22:
		    immediate = (instruction & 0x3fffff);
		    if ((immediate & 0x200000) != 0)
			    immediate |= 0xffc00000;
		    immediate <<= 2;
		    immediate -= arch_symbol->n_value;
		    immediate += defined_symbol->n_value;
		    if ((immediate & 0xff800000) != 0xff800000 &&
				    (immediate & 0xff800000) != 0x00) {
			error("prebinding can't be redone for: %s (for "
			    "architecture %s) because of relocation overflow "
			    "(external relocation for symbol %s displacement "
			    "too large to fit)", arch->file_name, arch_name,
			    name);
			exit(2);
		    }
		    immediate >>= 2;
		    instruction = (instruction & 0xffc00000) | 
				    (immediate & 0x3fffff);
		    break;
		case SPARC_RELOC_WDISP30:
		    immediate = (instruction & 0x3fffffff);
		    immediate <<= 2;
		    immediate -= arch_symbol->n_value;
		    immediate += defined_symbol->n_value;
		    immediate >>= 2;
		    instruction = (instruction & 0xc0000000) |
				    (immediate & 0x3fffffff);
		    break;
		default:
		    error("mallformed file: %s (external relocation entry "
			  "%lu has unknown r_type) (for architecture %s)",
			  arch->file_name, i, arch_name);
		    exit(2);
		}
		set_arch_long(p, instruction);
	    }
next:
	    /*
	     * If the relocation entry had a pair step over it.
	     */
	    if(arch_extrelocs[i].r_type == SPARC_RELOC_LO10 ||
	       arch_extrelocs[i].r_type == SPARC_RELOC_HI22)
		i++;
	}
}

/*
 * update_ppc_external_relocs() updates of the items with external relocation
 * entries for the ppc architecture.  It only deals with external relocation
 * entries that are using prebound undefined symbols.
 */
static
void
update_ppc_external_relocs(
void)
{
    unsigned long i, value;
    char *name, *p;
    struct nlist *defined_symbol, *arch_symbol;
    enum link_state *module_state;
    struct lib *lib;
    unsigned long instruction, immediate;
    unsigned long other_half, br14_disp_sign;

	for(i = 0; i < arch_nextrel; i++){
	    /* check the r_symbolnum field */
	    if(arch_extrelocs[i].r_symbolnum > arch_nsyms){
		error("mallformed file: %s (bad symbol table index for "
		      "external relocation entry %lu) (for architecture %s)",
		      arch->file_name, i, arch_name);
		exit(2);
	    }
	    /* check to see if it needs a pair and has a correct one */
	    if(arch_extrelocs[i].r_type == PPC_RELOC_HI16 ||
	       arch_extrelocs[i].r_type == PPC_RELOC_LO16 ||
	       arch_extrelocs[i].r_type == PPC_RELOC_HA16 ||
	       arch_extrelocs[i].r_type == PPC_RELOC_LO14){
		if(i + 1 == arch_nextrel){
		    error("mallformed file: %s (missing pair external "
			  "relocation entry for entry %lu) (for architecture "
			  "%s)", arch->file_name, i, arch_name);
		    exit(2);
		}
		if(arch_extrelocs[i + 1].r_type != PPC_RELOC_PAIR){
		    error("mallformed file: %s (pair external relocation entry "
			  "for entry %lu is not of r_type PPC_RELOC_PAIR) "
			  "(for architecture %s)", arch->file_name, i,
			  arch_name);
		    exit(2);
		}
	    }

	    /*
	     * If the symbol this relocation entry is refering to is not a
	     * prebound undefined symbol then skip it.
	     */ 
	    arch_symbol = arch_symbols + arch_extrelocs[i].r_symbolnum;
	    if((arch_symbol->n_type & N_TYPE) != N_PBUD)
		goto next;

	    /*
	     * Look up the symbol being referenced by this relocation entry to
	     * get the defined symbol's value to be used.
	     */
	    name = arch_strings + arch_symbol->n_un.n_strx;
	    lookup_symbol(name, &defined_symbol, &module_state, &lib,
			  NO_INDR_LOOP);

	    p = contents_pointer_for_vmaddr(arch_extrelocs[i].r_address +
				(arch_split_segs == TRUE ?
				 arch_segs_read_write_addr : arch_seg1addr),
					    1 << arch_extrelocs[i].r_length);
	    if(p == NULL){
		error("mallformed file: %s (for architecture %s) (bad r_address"
		      " field for external relocation entry %lu)",
		      arch->file_name, arch_name, i);
		exit(2);
	    }

	    if(arch_extrelocs[i].r_type == PPC_RELOC_VANILLA){
		switch(arch_extrelocs[i].r_length){
		case 0: /* byte */
		    value = get_arch_byte(p);
		    value = (value - arch_symbol->n_value) +
			    defined_symbol->n_value;
		    if( (value & 0xffffff00) &&
		       ((value & 0xffffff80) != 0xffffff80)){
			error("prebinding can't be redone for: %s (for "
			    "architecture %s) because of relocation overflow "
			    "(external relocation for symbol %s does not fit "
			    "in 1 byte)", arch->file_name, arch_name, name);
			exit(2);
		    }
		    set_arch_byte(p, value);
		    break;
		case 1: /* word (2 byte) */
		    value = get_arch_short(p);
		    value = (value - arch_symbol->n_value) +
			    defined_symbol->n_value;
		    if( (value & 0xffff0000) &&
		       ((value & 0xffff8000) != 0xffff8000)){
			error("prebinding can't be redone for: %s (for "
			    "architecture %s) because of relocation overflow "
			    "(external relocation for symbol %s does not fit "
			    "in 2 bytes)", arch->file_name, arch_name, name);
			exit(2);
		    }
		    set_arch_short(p, value);
		    break;
		case 2: /* long (4 byte) */
		    value = get_arch_long(p);
		    value = (value - arch_symbol->n_value) +
			    defined_symbol->n_value;
		    set_arch_long(p, value);
		    break;
		default:
		    error("mallformed file: %s (external relocation entry "
			  "%lu has bad r_length) (for architecture %s)",
			  arch->file_name, i, arch_name);
		    exit(2);
		}
	    }
	    /*
	     * Do PPC specific relocation based on the r_type.
	     */
	    else{
		instruction = get_arch_long(p);
		switch(arch_extrelocs[i].r_type){
		case PPC_RELOC_HI16:
		    other_half = (arch_extrelocs[i + 1].r_address) & 0xffff;
		    immediate = ((instruction & 0xffff) << 16) | other_half;
		    immediate -= arch_symbol->n_value;
		    immediate += defined_symbol->n_value;
		    instruction = (instruction & 0xffff0000) |
				  ((immediate >> 16) & 0xffff);
		    arch_extrelocs[i + 1].r_address = immediate & 0xffff;
		    break;
		case PPC_RELOC_LO16:
		    other_half = (arch_extrelocs[i + 1].r_address) & 0xffff;
		    immediate = (other_half << 16) | (instruction & 0xffff);
		    immediate -= arch_symbol->n_value;
		    immediate += defined_symbol->n_value;
		    instruction = (instruction & 0xffff0000) |
				  (immediate & 0xffff);
		    arch_extrelocs[i + 1].r_address =
				  (immediate >> 16) & 0xffff;
		    break;
		case PPC_RELOC_HA16:
		    other_half = (arch_extrelocs[i + 1].r_address) & 0xffff;
		    immediate = ((instruction & 0xffff) << 16) | other_half;
		    immediate -= arch_symbol->n_value;
		    immediate += defined_symbol->n_value;
		    if((immediate & 0x00008000) != 0)
			instruction = (instruction & 0xffff0000) |
				  (((immediate + 0x00008000) >> 16) & 0xffff);
		    else
			instruction = (instruction & 0xffff0000) |
				      ((immediate >> 16) & 0xffff);
		    arch_extrelocs[i + 1].r_address = immediate & 0xffff;
		    break;
		case PPC_RELOC_LO14:
		    other_half = (arch_extrelocs[i + 1].r_address) & 0xffff;
		    immediate = (other_half << 16) | (instruction & 0xfffc);
		    immediate -= arch_symbol->n_value;
		    immediate += defined_symbol->n_value;
		    if((immediate & 0x3) != 0){
			error("prebinding can't be redone for: %s (for "
			    "architecture %s) because of relocated value "
			    "not a multiple of 4 bytes", arch->file_name,
			    arch_name);
			exit(2);
		    }
		    instruction = (instruction & 0xffff0003) |
				  (immediate & 0xfffc);
		    arch_extrelocs[i + 1].r_address =
				  (immediate >> 16) & 0xffff;
		    break;
		case PPC_RELOC_BR14:
		    br14_disp_sign = (instruction & 0x8000);
		    immediate = instruction & 0xfffc;
		    if((immediate & 0x8000) != 0)
			immediate |= 0xffff0000;
		    immediate -= arch_symbol->n_value;
		    immediate += defined_symbol->n_value;
		    if((immediate & 0x3) != 0){
			error("prebinding can't be redone for: %s (for "
			    "architecture %s) because of relocated value "
			    "not a multiple of 4 bytes", arch->file_name,
			    arch_name);
			exit(2);
		    }
		    if((immediate & 0xfffe0000) != 0xfffe0000 &&
		       (immediate & 0xfffe0000) != 0x00000000){
			error("prebinding can't be redone for: %s (for "
			    "architecture %s) because of relocation overflow "
			    "(external relocation for symbol %s displacement "
			    "too large to fit)", arch->file_name, arch_name,
			    name);
			exit(2);
		    }
		    instruction = (instruction & 0xffff0003) |
				  (immediate & 0xfffc);
		    /*
		     * If this is a branch conditional B-form where
		     * the branch condition is not branch always and
		     * the sign of the displacement is different
		     * after relocation then flip the Y-bit to
		     * preserve the sense of the branch prediction. 
		     */
		    if((instruction & 0xfc000000) == 0x40000000 &&
		       (instruction & 0x03e00000) != 0x02800000 &&
		       (instruction & 0x00008000) != br14_disp_sign)
			instruction ^= (1 << 21);
		    break;
		case PPC_RELOC_BR24:
		    immediate = instruction & 0x03fffffc;
		    if((immediate & 0x02000000) != 0)
			immediate |= 0xfc000000;
		    immediate -= arch_symbol->n_value;
		    immediate += defined_symbol->n_value;
		    if((immediate & 0x3) != 0){
			error("prebinding can't be redone for: %s (for "
			    "architecture %s) because of relocated value "
			    "not a multiple of 4 bytes", arch->file_name,
			    arch_name);
			exit(2);
		    }
		    if((immediate & 0xfe000000) != 0xfe000000 &&
		       (immediate & 0xfe000000) != 0x00000000){
			error("prebinding can't be redone for: %s (for "
			    "architecture %s) because of relocation overflow "
			    "(external relocation for symbol %s displacement "
			    "too large to fit)", arch->file_name, arch_name,
			    name);
			exit(2);
			exit(2);
		    }
		    instruction = (instruction & 0xfc000003) |
		    		  (immediate & 0x03fffffc);
		    break;
		default:
		    error("mallformed file: %s (external relocation entry "
			  "%lu has unknown r_type) (for architecture %s)",
			  arch->file_name, i, arch_name);
		    exit(2);
		}
		set_arch_long(p, instruction);
	    }
next:
	    /*
	     * If the relocation entry had a pair step over it.
	     */
	    if(arch_extrelocs[i].r_type == PPC_RELOC_HI16 ||
	       arch_extrelocs[i].r_type == PPC_RELOC_LO16 ||
	       arch_extrelocs[i].r_type == PPC_RELOC_HA16 ||
	       arch_extrelocs[i].r_type == PPC_RELOC_LO14)
		i++;
	}
}

/*
 * contents_pointer_for_vmaddr() returns a pointer in memory for the vmaddr
 * of the current arch.  If the vmaddr is out of range return NULL.
 */
static
char *
contents_pointer_for_vmaddr(
unsigned long vmaddr,
unsigned long size)
{
    unsigned long i, offset;
    struct load_command *lc;
    struct segment_command *sg;

	lc = arch->object->load_commands;
	for(i = 0; i < arch->object->mh->ncmds; i++){
	    if(lc->cmd == LC_SEGMENT){
		sg = (struct segment_command *)lc;
		if(vmaddr >= sg->vmaddr &&
		   vmaddr + size < sg->vmaddr + sg->vmsize){
		    offset = vmaddr - sg->vmaddr;
		    if(offset + size <= sg->filesize)
			return(arch->object->object_addr +
			       sg->fileoff + offset);
		    return(NULL);
		}
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	return(NULL);
}

/*
 * update_symbol_pointers() updates the symbol pointers using the new and old
 * symbol table.
 */
static
void
update_symbol_pointers(
void)
{
    unsigned long i, j, k, section_type;
    struct load_command *lc;
    struct segment_command *sg;
    struct section *s;
    struct nlist *arch_symbol, *defined_symbol;
    char *name, *p;
    enum link_state *module_state;
    struct lib *lib;

	/*
	 * For each symbol pointer section update the symbol pointers using
	 * prebound undefined symbols to their new values.
	 */
	lc = arch->object->load_commands;
	for(i = 0; i < arch->object->mh->ncmds; i++){
	    if(lc->cmd == LC_SEGMENT){
		sg = (struct segment_command *)lc;
		s = (struct section *)
		    ((char *)sg + sizeof(struct segment_command));
		for(j = 0 ; j < sg->nsects ; j++){
		    section_type = s->flags & SECTION_TYPE;
		    if(section_type == S_NON_LAZY_SYMBOL_POINTERS ||
		       section_type == S_LAZY_SYMBOL_POINTERS){
			if(s->reserved1 + s->size / sizeof(unsigned long) >
			   arch_nindirectsyms){
			    error("mallformed file: %s (for architecture %s) "
				"(indirect symbol table entries for section "
				"(%.16s,%.16s) extends past the end of the "
				"indirect symbol table)", arch->file_name,
				arch_name, s->segname, s->sectname);
			    exit(2);
			}
			for(k = 0; k < s->size / sizeof(unsigned long); k++){

			    /*
			     * If this indirect symbol table entry is for a
 			     * non-lazy symbol pointer section for a defined
			     * symbol which strip(1) has removed skip it.
			     */
			    if(section_type == S_NON_LAZY_SYMBOL_POINTERS &&
			       (arch_indirect_symtab[s->reserved1 + k] & 
			        INDIRECT_SYMBOL_LOCAL) == INDIRECT_SYMBOL_LOCAL)
				continue;

			    /* check symbol index of indirect symbol table */
			    if(arch_indirect_symtab[s->reserved1 + k] >
			       arch_nsyms){
				error("mallformed file: %s (for architecture "
				    "%s) (bad indirect symbol table entry %lu)",
				    arch->file_name, arch_name, i);
				exit(2);
			    }
	
			    /*
			     * If the symbol this indirect symbol table entry is
			     * refering to is not a prebound undefined symbol
			     * then skip it.
			     */ 
			    arch_symbol = arch_symbols +
				     arch_indirect_symtab[s->reserved1 + k];
			    if((arch_symbol->n_type & N_TYPE) != N_PBUD)
				continue;

			    /*
			     * Look up the symbol being referenced by this
			     * indirect symbol table entry to get the defined
			     * symbol's value to be used.
			     */
			    name = arch_strings + arch_symbol->n_un.n_strx;
			    lookup_symbol(name, &defined_symbol, &module_state,
					  &lib, NO_INDR_LOOP);

			    p = contents_pointer_for_vmaddr(
				s->addr + (k * sizeof(long)), sizeof(long));
			    if(p == NULL){
				error("mallformed file: %s (for architecture "
				    "%s) (bad indirect section (%.16s,%.16s))",
				    arch->file_name, arch_name, s->segname,
				    s->sectname);
				exit(2);
			    }
			    set_arch_long(p, defined_symbol->n_value);
			}
		    }
		    s++;
		}
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
}

/*
 * update_load_commands() updates the time stamps in the LC_LOAD_DYLIB commands
 * and updates (and adds) the LC_PREBOUND_DYLIB commands if this is an
 * excutable.
 */
static
void
update_load_commands(
void)
{
    unsigned long i, j, k, nmodules, size, sizeofcmds, ncmds, low_fileoff;
    struct load_command *lc1, *lc2, *new_load_commands;
    struct dylib_command *dl_load, *dl_id;
    struct prebound_dylib_command *pbdylib1, *pbdylib2;
    struct segment_command *sg;
    struct section *s;
    char *dylib_name, *linked_modules;

	/*
	 * First copy the time stamps for the dependent libraries from the
	 * library's ID commands to the arch's load_command.  Also size the
	 * non LC_PREBOUND_DYLIB commands.
	 */
	ncmds = 0;
	sizeofcmds = 0;
	lc1 = arch->object->load_commands;
	for(i = 0; i < arch->object->mh->ncmds; i++){
	    if(lc1->cmd == LC_LOAD_DYLIB){
		dl_load = (struct dylib_command *)lc1;
		dylib_name = (char *)dl_load + dl_load->dylib.name.offset;
		for(j = 0; j < nlibs; j++){
		    if(strcmp(libs[j].dylib_name, dylib_name) == 0){
			lc2 = libs[j].ofile->load_commands;
			for(k = 0; k < libs[j].ofile->mh->ncmds; k++){
			    if(lc2->cmd == LC_ID_DYLIB){
				dl_id = (struct dylib_command *)lc2;
				dl_load->dylib.timestamp =
				    dl_id->dylib.timestamp;
				break;
			    }
			    lc2 = (struct load_command *)
				((char *)lc2 + lc2->cmdsize);
			}
			break;
		    }
		}
	    }
	    if(lc1->cmd != LC_PREBOUND_DYLIB){
		ncmds += 1;
		sizeofcmds += lc1->cmdsize;
	    }
	    lc1 = (struct load_command *)((char *)lc1 + lc1->cmdsize);
	}

	/*
	 * Only executables have LC_PREBOUND_DYLIB commands so if this is not
	 * an executable (a library) then we are done here.
	 */
	if(arch->object->mh->filetype != MH_EXECUTE)
	    return;

	/*
	 * For each library the executable uses determine the size we need for
	 * the LC_PREBOUND_DYLIB load command for it.  If their is an exising
	 * LC_PREBOUND_DYLIB command use it if there is enough space in the
	 * command for the current number of modules.  If not calculate the
	 * size ld(1) would use for it.
	 */
	for(i = 0; i < nlibs; i++){
	    lc1 = arch->object->load_commands;
	    for(j = 0; j < arch->object->mh->ncmds; j++){
		if(lc1->cmd == LC_PREBOUND_DYLIB){
		    pbdylib1 = (struct prebound_dylib_command *)lc1;
		    dylib_name = (char *)pbdylib1 + pbdylib1->name.offset;
		    if(strcmp(libs[i].dylib_name, dylib_name) == 0){
			libs[i].LC_PREBOUND_DYLIB_found = TRUE;
			if(libs[i].nmodtab <= pbdylib1->nmodules){
			    libs[i].LC_PREBOUND_DYLIB_size = pbdylib1->cmdsize;
			}
			else{
			   /*
			    * Figure out the size left in the command for the
			    * the linked_modules bit vector.  When this is first
			    * created by ld(1) extra space is left so this
			    * this program can grow the vector if the library
			    * changes.
			    */
			   size = pbdylib1->cmdsize -
				  (sizeof(struct prebound_dylib_command) +
				   round(strlen(dylib_name) + 1, sizeof(long)));
			   /*
			    * Now see if the size left has enought space to fit
			    * the linked_modules bit vector for the number of
			    * modules this library currently has.
			    */
			   if((libs[i].nmodtab + 7)/8 <= size){
			        libs[i].LC_PREBOUND_DYLIB_size =
				    pbdylib1->cmdsize;
			   }
			   else{
				/*
				 * The existing space in not enough so calculate
				 * the new size as ld(1) would.  125% of the
				 * modules with a minimum size of 64 modules.
				 */
				nmodules = libs[i].nmodtab +
                                   (libs[i].nmodtab >> 2);
				if(nmodules < 64)
				    nmodules = 64;
				size = sizeof(struct prebound_dylib_command) +
				   round(strlen(dylib_name)+1, sizeof(long))+
				   round(nmodules / 8, sizeof(long));
				libs[i].LC_PREBOUND_DYLIB_size = size;
			   }
			}
			ncmds += 1;
			sizeofcmds += libs[i].LC_PREBOUND_DYLIB_size;
			break;
		    }
		}
		lc1 = (struct load_command *)((char *)lc1 + lc1->cmdsize);
	    }
	}
	/*
	 * Make a pass through the libraries and pick up any of them that did
	 * not appear in the load commands and then size their LC_PREBOUND_DYLIB
	 * command.
	 */
	for(i = 0; i < nlibs; i++){
	    if(libs[i].LC_PREBOUND_DYLIB_found == FALSE){
		/*
		 * Calculate the size as ld(1) would.  125% of the
		 * modules with a minimum size of 64 modules.
		 */
		nmodules = libs[i].nmodtab + (libs[i].nmodtab >> 2);
		if(nmodules < 64)
		    nmodules = 64;
		size = sizeof(struct prebound_dylib_command) +
		   round(strlen(libs[i].dylib_name) + 1, sizeof(long))+
		   round(nmodules / 8, sizeof(long));
		libs[i].LC_PREBOUND_DYLIB_size = size;
		sizeofcmds += libs[i].LC_PREBOUND_DYLIB_size;
		ncmds++;
	    }
	}

	/*
	 * If the size of the load commands that includes the updated
	 * LC_PREBOUND_DYLIB commands is larger than the existing load commands
	 * then see if they can be fitted in before the contents of the first
	 * section (or segment in the case of a LINKEDIT segment only file).
	 */
	if(sizeofcmds > arch->object->mh->sizeofcmds){
	    low_fileoff = ULONG_MAX;
	    lc1 = arch->object->load_commands;
	    for(i = 0; i < arch->object->mh->ncmds; i++){
		if(lc1->cmd == LC_SEGMENT){
		    sg = (struct segment_command *)lc1;
		    s = (struct section *)
			((char *)sg + sizeof(struct segment_command));
		    if(sg->nsects != 0){
			for(j = 0; j < sg->nsects; j++){
			    if(s->size != 0 && s->offset < low_fileoff)
				low_fileoff = s->offset;
			}
		    }
		    else{
			if(sg->filesize != 0 && sg->fileoff < low_fileoff)
			    low_fileoff = sg->fileoff;
		    }
		}
		lc1 = (struct load_command *)((char *)lc1 + lc1->cmdsize);
	    }
	    if(sizeofcmds + sizeof(struct mach_header) > low_fileoff){
		error("prebinding can't be redone for: %s (for architecture"
		      " %s) because larger updated load commands do not fit "
		      "(the program must be relinked)", arch->file_name,
		      arch_name);
		exit(2);
	    }
	}

	/*
	 * Allocate space for the new load commands as zero it out so any holes
	 * will be zero bytes.
	 */
	new_load_commands = allocate(sizeofcmds);
	memset(new_load_commands, '\0', sizeofcmds);

	/*
	 * Fill in the new load commands by copying in the non-LC_PREBOUND_DYLIB
	 * commands and updating the LC_PREBOUND_DYLIB commands.
	 */
	lc1 = arch->object->load_commands;
	lc2 = new_load_commands;
	for(i = 0; i < arch->object->mh->ncmds; i++){
	    if(lc1->cmd == LC_PREBOUND_DYLIB){
		pbdylib1 = (struct prebound_dylib_command *)lc1;
		pbdylib2 = (struct prebound_dylib_command *)lc2;
		dylib_name = (char *)pbdylib1 + pbdylib1->name.offset;
		for(j = 0; j < nlibs; j++){
		    if(strcmp(libs[j].dylib_name, dylib_name) == 0){
			pbdylib2->cmd = LC_PREBOUND_DYLIB;
			pbdylib2->cmdsize = libs[j].LC_PREBOUND_DYLIB_size;
			pbdylib2->name.offset =
				sizeof(struct prebound_dylib_command);
			strcpy(((char *)pbdylib2) +
				sizeof(struct prebound_dylib_command),
				dylib_name);
			pbdylib2->nmodules = libs[j].nmodtab;
			pbdylib2->linked_modules.offset =
				sizeof(struct prebound_dylib_command) +
				round(strlen(dylib_name) + 1, sizeof(long));
			linked_modules = ((char *)pbdylib2) +
                                sizeof(struct prebound_dylib_command) +
                                round(strlen(dylib_name) + 1, sizeof(long));
			for(k = 0; k < libs[j].nmodtab; k++){
			    if(libs[j].module_states[k] == LINKED)
				linked_modules[k / 8] |= 1 << k % 8;
			}
			lc2 = (struct load_command *)
				((char *)lc2 + lc2->cmdsize);
			break;
		    }
		}
	    }
	    else{
		memcpy(lc2, lc1, lc1->cmdsize);
		lc2 = (struct load_command *)((char *)lc2 + lc2->cmdsize);
	    }
	    lc1 = (struct load_command *)((char *)lc1 + lc1->cmdsize);
	}

	/*
	 * Add any new LC_PREBOUND_DYLIB load commands.
	 */
	for(i = 0; i < nlibs; i++){
	    if(libs[i].LC_PREBOUND_DYLIB_found == FALSE){
		pbdylib2 = (struct prebound_dylib_command *)lc2;
		pbdylib2->cmd = LC_PREBOUND_DYLIB;
		pbdylib2->cmdsize = libs[i].LC_PREBOUND_DYLIB_size;
		pbdylib2->name.offset =
			sizeof(struct prebound_dylib_command);
		strcpy(((char *)pbdylib2) +
			sizeof(struct prebound_dylib_command),
			libs[i].dylib_name);
		pbdylib2->nmodules = libs[i].nmodtab;
		pbdylib2->linked_modules.offset =
			sizeof(struct prebound_dylib_command) +
			round(strlen(libs[i].dylib_name) + 1, sizeof(long));
		linked_modules = ((char *)pbdylib2) +
			sizeof(struct prebound_dylib_command) +
			round(strlen(libs[i].dylib_name) + 1, sizeof(long));
		for(j = 0; j < libs[i].nmodtab; j++){
		    if(libs[i].module_states[j] == LINKED)
			linked_modules[j / 8] |= 1 << j % 8;
		}
		lc2 = (struct load_command *)
			((char *)lc2 + lc2->cmdsize);
	    }
	}

	/*
	 * Finally copy the updated load commands over the existing load
	 * commands.
	 */
	memcpy(arch->object->load_commands, new_load_commands, sizeofcmds);
	arch->object->mh->sizeofcmds = sizeofcmds;
	arch->object->mh->ncmds = ncmds;
	free(new_load_commands);

	/* reset the pointers into the load commands */
	lc1 = arch->object->load_commands;
	for(i = 0; i < arch->object->mh->ncmds; i++){
	    if(lc1->cmd == LC_SYMTAB){
		arch->object->st = (struct symtab_command *)lc1;
	    }
	    else if(lc1->cmd == LC_DYSYMTAB){
		arch->object->dyst = (struct dysymtab_command *)lc1;
	    }
	    lc1 = (struct load_command *)((char *)lc1 + lc1->cmdsize);
	}
}

/*
 * Print the warning message and the input file.
 */
__private_extern__
void
warning_arch(
struct arch *arch,
struct member *member,
char *format,
...)
{
    va_list ap;

	va_start(ap, format);
        fprintf(stderr, "%s: ", progname);
	vfprintf(stderr, format, ap);
	va_end(ap);
	if(member != NULL){
	    fprintf(stderr, "%s(%.*s)", arch->file_name,
		    (int)member->member_name_size, member->member_name);
	}
	else
	    fprintf(stderr, "%s", arch->file_name);
	if(arch->fat_arch_name != NULL)
	    fprintf(stderr, " (for architecture %s)\n", arch->fat_arch_name);
	else
	    fprintf(stderr, "\n");
	va_end(ap);
}

/*
 * Print the error message the input file and increment the error count
 */
__private_extern__
void
error_arch(
struct arch *arch,
struct member *member,
char *format,
...)
{
    va_list ap;

	va_start(ap, format);
        fprintf(stderr, "%s: ", progname);
	vfprintf(stderr, format, ap);
	va_end(ap);
	if(member != NULL){
	    fprintf(stderr, "%s(%.*s)", arch->file_name,
		    (int)member->member_name_size, member->member_name);
	}
	else
	    fprintf(stderr, "%s", arch->file_name);
	if(arch->fat_arch_name != NULL)
	    fprintf(stderr, " (for architecture %s)\n", arch->fat_arch_name);
	else
	    fprintf(stderr, "\n");
	va_end(ap);
	errors++;
}

/*
 * Print the fatal error message the input file and exit non-zero.
 */
__private_extern__
void
fatal_arch(
struct arch *arch,
struct member *member,
char *format,
...)
{
    va_list ap;

	va_start(ap, format);
        fprintf(stderr, "%s: ", progname);
	vfprintf(stderr, format, ap);
	va_end(ap);
	if(member != NULL){
	    fprintf(stderr, "%s(%.*s)", arch->file_name,
		    (int)member->member_name_size, member->member_name);
	}
	else
	    fprintf(stderr, "%s", arch->file_name);
	if(arch->fat_arch_name != NULL)
	    fprintf(stderr, " (for architecture %s)\n", arch->fat_arch_name);
	else
	    fprintf(stderr, "\n");
	va_end(ap);
	if(check_for_non_prebound == TRUE)
	    exit(0);
	exit(2);
}
