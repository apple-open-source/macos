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
/*
 * The NeXT Computer, Inc. libtool(1) program that handles fat files, archives
 * and Mach-O objects files (no 4.3bsd a.out files).  This is also the ranlib(1)
 * program.
 */
#include <mach/mach.h>
#include "stuff/openstep_mach.h"
#include <libc.h>
#ifndef __OPENSTEP__
#include <utime.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ar.h>
#include <mach-o/ranlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "stuff/bool.h"
#include "stuff/ofile.h"
#include "stuff/round.h"
#include "stuff/errors.h"
#include "stuff/allocate.h"
#include "stuff/execute.h"
#include "stuff/version_number.h"

#include "make.h"
#include <mach/mach_init.h>
#if defined(__OPENSTEP__) || defined(__GONZO_BUNSEN_BEAKER__)
#include <servers/netname.h>
#else
#include <servers/bootstrap.h>
#endif

/* used by error routines as the name of the program */
char *progname = NULL;

/* the bytesex of the host this program is running on */
static enum byte_sex host_byte_sex = UNKNOWN_BYTE_SEX;

/*
 * The time the table of contents' are set to and the time to base the
 * modification time of the output file to be set to.
 */
static long toc_time = 0;

/*
 * The mode of the table of contents member (S_IFREG | (0666 & ~umask))
 */
static u_short toc_mode = 0;

/* flags set from the command line arguments */
struct cmd_flags {
    char **files;	/* array of file name arguments */
    unsigned long
	nfiles;		/* number of file name arguments */
    char **filelist;	/* filelist argument the file name argument came from */
    enum bool
	no_files_ok;	/* ok to see no files */
    enum bool ranlib;	/* set if this is run as ranlib not libtool */
    enum bool s;	/* sort the table of contents */
    enum bool a;	/* don't sort the table of contents (original form) */
    enum bool c;	/* include commmon symbols in the table of contents */
    enum bool t;	/* just "touch" the archives to get the date right */
    enum bool f;	/* warn if the output archive is fat,used by ar(1) -s */
    char *output;	/* the output file specified by -o */
    enum bool final_output_specified; /* if -final_output is specified */
    enum bool dynamic;	/* create a dynamic shared library, static by default */
    char *compatibility;/* compatibility version if specified, NULL otherwise */
    char *current;	/* current version if specified, NULL otherwise */
    char *install_name; /* install name if specified, NULL otherwise */
    char *seg1addr;	/* seg1addr if specified, NULL otherwise */
    char *segs_read_only_addr;	/* segs_read_only_addr if specified, or NULL */
    char *segs_read_write_addr;	/* segs_read_write_addr if specified, or NULL */
    char *seg_addr_table;	/* seg_addr_table if specified, or NULL */
    char *seg_addr_table_filename;
			/* seg_addr_table_filename if specified, or NULL */
    char **Ldirs;	/* array of -Ldir arguments */
    unsigned long
	nLdirs;		/* number of -Ldir arguments */
    char **ldflags;	/* other ld(1) flags to pass */
    unsigned long
	nldflags;	/* number of ld(1) flags for above */
    enum bool verbose;	/* print exec(2) commands run */
    struct arch_flag
	arch_only_flag;	/* the -arch_only flag if specified */
    enum bool		/* set if either -prebind or -noprebind is seen */
	prebinding_flag_specified;
    enum bool		/* set if -prebind is seen or the LD_PREBIND */
	prebinding;	/*  environment variable is set (and -noprebind isn't)*/
    enum bool		/* set if either -all_load or -noall_load is seen */
	all_load_flag_specified;
    enum bool		/* set if -all_load is seen (and -noall_load isn't) */
	all_load;
    enum bool		/* set with -L (the default) off with -T, for -static */
	use_long_names; /* use 4.4bsd extended format 1 for long names */
    enum bool L_or_T_specified;
    enum bool		/* set if the environ var RC_TRACE_ARCHIVES is set */
	rc_trace_archives;
};
static struct cmd_flags cmd_flags = { 0 };

/* The value of the environment variable NEXT_ROOT */
static char *next_root = NULL;

/* the standard directories to search for -lx names */
char *standard_dirs[] = {
    "/lib/",
    "/usr/lib/",
    "/usr/local/lib/",
    NULL
};

/*
 * The input files are broken down in to their object files and then placed in
 * these structures.  They are sorted by architecture type and then each object
 * has a member struct created for it in one of the arch structs.  All of these
 * structs hang off of 'archs'.
 */
static struct arch *archs = NULL;
static unsigned long narchs = 0;

struct arch {
    struct arch_flag arch_flag;	/* the identifing info of this architecture */
    unsigned long size;		/* current working size and final size */

    /* the table of contents (toc) stuff for this architecture in the library */
    unsigned long  toc_size;	/* total size of the toc including ar_hdr */
    struct ar_hdr  toc_ar_hdr;	/* the archive header for this member */
    enum bool toc_long_name;    /* use the long name in the output */
    char *toc_name;		/* name of toc member */
    unsigned long toc_name_size;/* size of name of toc member */
    struct ranlib *toc_ranlibs;	/* ranlib structs */
    unsigned long  toc_nranlibs;/* number of ranlib structs */
    char	  *toc_strings;	/* strings of symbol names for ranlib structs */
    unsigned long  toc_strsize;	/* number of bytes for the strings above */

    /* the members of this architecture in the library */
    struct member *members;	/* the members of the library for this arch */
    unsigned long nmembers;	/* the number of the above members */
};

struct member {
    unsigned long offset;	    /* current working offset and final offset*/
    struct ar_hdr ar_hdr;	    /* the archive header for this member */
    char null_byte;		    /* space to write '\0' for ar_hdr */
    char *object_addr;		    /* the address of the object file */
    unsigned long object_size;	    /* the size of the object file */
    enum byte_sex object_byte_sex;  /* the byte sex of the object file */
    struct mach_header *mh;	    /* the mach_header of the object file */
    struct load_command		    /* the start of the load commands */
	*load_commands;
    struct symtab_command *st;	    /* the symbol table command */
    struct section **sections;	    /* array of section structs */

    /* the name of the member in the output */
    char         *member_name;	    /* the member name */
    unsigned long member_name_size; /* the size of the member name */
    enum bool output_long_name;	    /* use the extended format #1 for the
				       member name in the output */

    /* info recorded from the input file this member came from */
    char	  *input_file_name;	/* the input file name */
    char	  *input_base_name;     /* the base name in the input file */
    unsigned long  input_base_name_size;/* the size of the base name */
    struct ar_hdr *input_ar_hdr;
};

static void usage(
    void);
static void process(
    void);
static char *file_name_from_l_flag(
    char *l_flag);
static char *search_for_file(
    char *base_name);
static void add_member(
    struct ofile *ofile);
static void free_archs(
    void);
static void create_library(
    char *output);
static void create_dynamic_shared_library(
    char *output);
static void create_dynamic_shared_library_cleanup(
    int sig);
static void make_table_of_contents(
    struct arch *arch,
    char *output);
static int ranlib_name_qsort(
    const struct ranlib *ran1,
    const struct ranlib *ran2);
static int ranlib_offset_qsort(
    const struct ranlib *ran1,
    const struct ranlib *ran2);
static enum bool toc_symbol(
    struct nlist *symbol,
    struct section **sections);
static enum bool check_sort_ranlibs(
    struct arch *arch,
    char *output);
static void warn_duplicate_member_names(
    void);
static int member_name_qsort(
    const struct member *member1,
    const struct member *member2);
static int member_offset_qsort(
    const struct member *member1,
    const struct member *member2);
static void warn_member(
    struct arch *arch,
    struct member *member,
    const char *format, ...) __attribute__ ((format (printf, 3, 4)));

int
main(
int argc,
char **argv,
char **envp)
{
    char *p, *endp, *filelist, *dirname, *addr;
    int fd;
    struct stat stat_buf;
    unsigned long i, j, temp, nfiles, maxfiles;
    int oumask, numask;
    kern_return_t r;
    enum bool lflags_seen, bad_flag_seen;

	lflags_seen = FALSE;
	progname = argv[0];

	host_byte_sex = get_host_byte_sex();

	toc_time = time(0);

	numask = 0;
	oumask = umask(numask);
	toc_mode = S_IFREG | (0666 & ~oumask);
	(void)umask(oumask);

	/* see if this is being run as ranlib */
	p = strrchr(argv[0], '/');
	if(p != NULL)
	    p++;
	else
	    p = argv[0];
	if(strncmp(p, "ranlib", sizeof("ranlib") - 1) == 0)
	    cmd_flags.ranlib = TRUE;

	/* The default is to used long names */
	cmd_flags.use_long_names = TRUE;

	/* process the command line arguments and collect the files */
	maxfiles = argc;
        cmd_flags.files = allocate(sizeof(char *) * maxfiles);
        cmd_flags.filelist = allocate(sizeof(char *) * maxfiles);
        memset(cmd_flags.filelist, '\0', sizeof(char *) * maxfiles);
	for(i = 1; i < argc; i++){
	    if(argv[i][0] == '-'){
		if(argv[i][1] == '\0'){
		    for(i += 1 ; i < argc; i++)
			cmd_flags.files[cmd_flags.nfiles++] = argv[i];
		    break;
		}
		if(strcmp(argv[i], "-o") == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    if(i + 1 == argc){
			error("missing argument to: %s option", argv[i]);
			usage();
		    }
		    if(cmd_flags.output != NULL){
			error("more than one: %s option specified", argv[i]);
			usage();
		    }
		    cmd_flags.output = argv[i+1];
		    i++;
		}
		else if(strcmp(argv[i], "-arch_only") == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    if(i + 1 == argc){
			error("missing argument to %s option", argv[i]);
			usage();
		    }
		    if(cmd_flags.arch_only_flag.name != NULL){
			error("more than one: %s option specified", argv[i]);
			usage();
		    }
		    else{
			if(get_arch_from_flag(argv[i+1],
					      &cmd_flags.arch_only_flag) == 0){
			    error("unknown architecture specification flag: "
				  "%s %s", argv[i], argv[i+1]);
			    arch_usage();
			    usage();
			}
		    }
		    i++;
		}
		else if(strcmp(argv[i], "-dynamic") == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    cmd_flags.dynamic = TRUE;
		}
		else if(strcmp(argv[i], "-static") == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    cmd_flags.dynamic = FALSE;
		}
		else if(strcmp(argv[i], "-filelist") == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    if(i + 1 == argc){
			error("missing argument to: %s option", argv[i]);
			usage();
		    }
		    filelist = argv[i + 1];
		    dirname = strrchr(filelist, ',');
		    if(dirname != NULL){
			*dirname = '\0';
			dirname++;
		    }
		    else
			dirname = "";
		    if((fd = open(filelist, O_RDONLY, 0)) == -1)
			system_fatal("can't open file list file: %s", filelist);
		    if(fstat(fd, &stat_buf) == -1)
			system_fatal("can't stat file list file: %s", filelist);
		    /*
		     * For some reason mapping files with zero size fails
		     * so it has to be handled specially.
		     */
		    if(stat_buf.st_size != 0){
			if((r = map_fd((int)fd, (vm_offset_t)0,
			    (vm_offset_t *)&(addr), (boolean_t)TRUE,
			    (vm_size_t)stat_buf.st_size)) != KERN_SUCCESS)
			    mach_fatal(r, "can't map file list file: %s",
				filelist);
		    }
		    else{
			fatal("file list file: %s is empty", filelist);
		    }
		    if(*dirname != '\0')
			dirname[-1] = ',';
		    close(fd);
		    nfiles = 0;
		    for(j = 0; j < stat_buf.st_size; j++){
			if(addr[j] == '\n')
			    nfiles++;
		    }
		    if(addr[stat_buf.st_size - 1] != '\n')
			nfiles++;
		    p = allocate((strlen(dirname) + 1) * nfiles +
				 stat_buf.st_size);
		    cmd_flags.files = reallocate(cmd_flags.files, 
					sizeof(char *) * (maxfiles + nfiles));
        	    cmd_flags.filelist = reallocate(cmd_flags.filelist,
					sizeof(char *) * (maxfiles + nfiles));
        	    memset(cmd_flags.filelist + maxfiles, '\0',
			   sizeof(char *) * nfiles);
		    maxfiles += nfiles;

		    cmd_flags.files[cmd_flags.nfiles] = p;
		    cmd_flags.filelist[cmd_flags.nfiles] = filelist;
		    cmd_flags.nfiles++;
		    if(*dirname != '\0'){
			strcpy(p, dirname);
			p += strlen(dirname);
			*p++ = '/';
		    }
		    for(j = 0; j < stat_buf.st_size; j++){
			if(addr[j] != '\n')
			    *p++ = addr[j];
			else{
			    *p++ = '\0';
			    if(j != stat_buf.st_size - 1){
				cmd_flags.files[cmd_flags.nfiles] = p;
				cmd_flags.filelist[cmd_flags.nfiles] =argv[i+1];
				cmd_flags.nfiles++;
				if(*dirname != '\0'){
				    strcpy(p, dirname);
				    p += strlen(dirname);
				    *p++ = '/';
				}
			    }
			}
		    }
		    if(addr[stat_buf.st_size - 1] != '\n')
			*p = '\0';
		    i++;
		}
		else if(strcmp(argv[i], "-compatibility_version") == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    if(i + 1 == argc){
			error("missing argument to: %s option", argv[i]);
			usage();
		    }
		    if(cmd_flags.compatibility != NULL){
			error("more than one: %s option specified", argv[i]);
			usage();
		    }
		    if(get_version_number(argv[i], argv[i+1], &temp) == FALSE){
			usage();
		    }
		    cmd_flags.compatibility = argv[i+1];
		    i++;
		}
		else if(strcmp(argv[i], "-current_version") == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    if(i + 1 == argc){
			error("missing argument to: %s option", argv[i]);
			usage();
		    }
		    if(cmd_flags.current != NULL){
			error("more than one: %s option specified", argv[i]);
			usage();
		    }
		    if(get_version_number(argv[i], argv[i+1], &temp) == FALSE){
			usage();
		    }
		    cmd_flags.current = argv[i+1];
		    i++;
		}
		else if(strcmp(argv[i], "-install_name") == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    if(i + 1 == argc){
			error("missing argument to: %s option", argv[i]);
			usage();
		    }
		    if(cmd_flags.install_name != NULL){
			error("more than one: %s option specified", argv[i]);
			usage();
		    }
		    cmd_flags.install_name = argv[i+1];
		    i++;
		}
		else if(strcmp(argv[i], "-seg1addr") == 0 ||
			strcmp(argv[i], "-image_base") == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    if(i + 1 == argc){
			error("missing argument to: %s option", argv[i]);
			usage();
		    }
		    if(cmd_flags.seg1addr != NULL){
			error("more than one: %s option specified", argv[i]);
			usage();
		    }
		    temp = strtoul(argv[i + 1], &endp, 16);
		    if(*endp != '\0'){
			error("address for -seg1addr %s not a proper "
			      "hexadecimal number", argv[i+1]);
			usage();
		    }
		    cmd_flags.seg1addr = argv[i+1];
		    i++;
		}
		else if(strcmp(argv[i], "-segs_read_only_addr") == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    if(i + 1 == argc){
			error("missing argument to: %s option", argv[i]);
			usage();
		    }
		    if(cmd_flags.segs_read_only_addr != NULL){
			error("more than one: %s option specified", argv[i]);
			usage();
		    }
		    temp = strtoul(argv[i + 1], &endp, 16);
		    if(*endp != '\0'){
			error("address for -segs_read_only_addr %s not a "
			      "proper hexadecimal number", argv[i+1]);
			usage();
		    }
		    cmd_flags.segs_read_only_addr = argv[i+1];
		    i++;
		}
		else if(strcmp(argv[i], "-segs_read_write_addr") == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    if(i + 1 == argc){
			error("missing argument to: %s option", argv[i]);
			usage();
		    }
		    if(cmd_flags.segs_read_write_addr != NULL){
			error("more than one: %s option specified", argv[i]);
			usage();
		    }
		    temp = strtoul(argv[i + 1], &endp, 16);
		    if(*endp != '\0'){
			error("address for -segs_read_write_addr %s not a "
			      "proper hexadecimal number", argv[i+1]);
			usage();
		    }
		    cmd_flags.segs_read_write_addr = argv[i+1];
		    i++;
		}
		else if(strcmp(argv[i], "-seg_addr_table") == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    if(i + 1 == argc){
			error("missing argument to: %s option", argv[i]);
			usage();
		    }
		    if(cmd_flags.seg_addr_table != NULL){
			error("more than one: %s option specified", argv[i]);
			usage();
		    }
		    cmd_flags.seg_addr_table = argv[i+1];
		    i++;
		}
		else if(strcmp(argv[i], "-seg_addr_table_filename") == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    if(i + 1 == argc){
			error("missing argument to: %s option", argv[i]);
			usage();
		    }
		    if(cmd_flags.seg_addr_table_filename != NULL){
			error("more than one: %s option specified", argv[i]);
			usage();
		    }
		    cmd_flags.seg_addr_table_filename = argv[i+1];
		    i++;
		}
		else if(strcmp(argv[i], "-sectcreate") == 0 ||
		        strcmp(argv[i], "-segcreate") == 0 ||
		        strcmp(argv[i], "-sectorder") == 0 ||
		        strcmp(argv[i], "-sectalign") == 0 ||
		        strcmp(argv[i], "-segprot") == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    if(i + 3 >= argc){
			error("not enough arguments follow %s", argv[i]);
			usage();
		    }
		    cmd_flags.ldflags = reallocate(cmd_flags.ldflags,
				sizeof(char *) * (cmd_flags.nldflags + 4));
		    cmd_flags.ldflags[cmd_flags.nldflags++] = argv[i];
		    cmd_flags.ldflags[cmd_flags.nldflags++] = argv[i+1];
		    cmd_flags.ldflags[cmd_flags.nldflags++] = argv[i+2];
		    cmd_flags.ldflags[cmd_flags.nldflags++] = argv[i+3];
		    if(strcmp(argv[i], "-sectcreate") == 0 ||
		       strcmp(argv[i], "-segcreate") == 0)
			cmd_flags.no_files_ok = TRUE;
		    i += 3;
		}
		else if(strcmp(argv[i], "-segalign") == 0 ||
		        strcmp(argv[i], "-undefined") == 0 ||
		        strcmp(argv[i], "-multiply_defined") == 0 ||
		        strcmp(argv[i], "-multiply_defined_unused") == 0 ||
		        strcmp(argv[i], "-umbrella") == 0 ||
			strcmp(argv[i], "-sub_umbrella") == 0 ||
			strcmp(argv[i], "-sub_library") == 0 ||
			strcmp(argv[i], "-allowable_client") == 0 ||
		        strcmp(argv[i], "-read_only_relocs") == 0 ||
		        strcmp(argv[i], "-init") == 0 ||
		        strcmp(argv[i], "-U") == 0 ||
		        strcmp(argv[i], "-Y") == 0 ||
		        strcmp(argv[i], "-dylib_file") == 0 ||
		        strcmp(argv[i], "-final_output") == 0 ||
		        strcmp(argv[i], "-headerpad") == 0 ||
		        strcmp(argv[i], "-weak_reference_mismatches") == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    if(i + 1 >= argc){
			error("not enough arguments follow %s", argv[i]);
			usage();
		    }
		    cmd_flags.ldflags = reallocate(cmd_flags.ldflags,
				sizeof(char *) * (cmd_flags.nldflags + 2));
		    cmd_flags.ldflags[cmd_flags.nldflags++] = argv[i];
		    cmd_flags.ldflags[cmd_flags.nldflags++] = argv[i+1];
		    if(strcmp(argv[i], "-final_output") == 0)
			cmd_flags.final_output_specified = TRUE;
		    i += 1;
		}
		else if(strcmp(argv[i], "-sectorder_detail") == 0 ||
		        strcmp(argv[i], "-Sn") == 0 ||
		        strcmp(argv[i], "-Si") == 0 ||
		        strcmp(argv[i], "-S") == 0 ||
		        strcmp(argv[i], "-X") == 0 ||
		        strcmp(argv[i], "-whatsloaded") == 0 ||
			strcmp(argv[i], "-whyload") == 0 ||
			strcmp(argv[i], "-arch_errors_fatal") == 0 ||
			strcmp(argv[i], "-run_init_lazily") == 0 ||
			strcmp(argv[i], "-twolevel_namespace") == 0 ||
			strcmp(argv[i], "-twolevel_namespace_hints") == 0 ||
			strcmp(argv[i], "-flat_namespace") == 0 ||
			strcmp(argv[i], "-nomultidefs") == 0 ||
			strcmp(argv[i], "-headerpad_max_install_names") == 0 ||
			strcmp(argv[i], "-prebind_all_twolevel_modules") == 0 ||
			strcmp(argv[i], "-ObjC") == 0 ||
			strcmp(argv[i], "-M") == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    cmd_flags.ldflags = reallocate(cmd_flags.ldflags,
				sizeof(char *) * (cmd_flags.nldflags + 1));
		    cmd_flags.ldflags[cmd_flags.nldflags++] = argv[i];
		}
		else if(strcmp(argv[i], "-prebind") == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    if(cmd_flags.prebinding_flag_specified == TRUE &&
		       cmd_flags.prebinding == FALSE){
			error("both -prebind and -noprebind can't be "
			      "specified");
			usage();
		    }
		    cmd_flags.prebinding_flag_specified = TRUE;
		    cmd_flags.prebinding = TRUE;
		    cmd_flags.ldflags = reallocate(cmd_flags.ldflags,
				sizeof(char *) * (cmd_flags.nldflags + 1));
		    cmd_flags.ldflags[cmd_flags.nldflags++] = argv[i];
		}
		else if(strcmp(argv[i], "-noprebind") == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    if(cmd_flags.prebinding_flag_specified == TRUE &&
		       cmd_flags.prebinding == TRUE){
			error("both -prebind and -noprebind can't be "
			      "specified");
			usage();
		    }
		    cmd_flags.prebinding_flag_specified = TRUE;
		    cmd_flags.prebinding = FALSE;
		    cmd_flags.ldflags = reallocate(cmd_flags.ldflags,
				sizeof(char *) * (cmd_flags.nldflags + 1));
		    cmd_flags.ldflags[cmd_flags.nldflags++] = argv[i];
		}
		else if(strcmp(argv[i], "-all_load") == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    if(cmd_flags.all_load_flag_specified == TRUE &&
		       cmd_flags.all_load == FALSE){
			error("both -all_load and -noall_load can't be "
			      "specified");
			usage();
		    }
		    cmd_flags.all_load_flag_specified = TRUE;
		    cmd_flags.all_load = TRUE;
		}
		else if(strcmp(argv[i], "-noall_load") == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    if(cmd_flags.all_load_flag_specified == TRUE &&
		       cmd_flags.all_load == TRUE){
			error("both -all_load and -noall_load can't be "
			      "specified");
			usage();
		    }
		    cmd_flags.all_load_flag_specified = TRUE;
		    cmd_flags.all_load = FALSE;
		}
		else if(strncmp(argv[i], "-y", 2) == 0 ||
		        strncmp(argv[i], "-i", 2) == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    if(strncmp(argv[i], "-i", 2) == 0)
			cmd_flags.no_files_ok = TRUE;
		    cmd_flags.ldflags = reallocate(cmd_flags.ldflags,
				sizeof(char *) * (cmd_flags.nldflags + 1));
		    cmd_flags.ldflags[cmd_flags.nldflags++] = argv[i];
		}
		else if(argv[i][1] == 'l'){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    if(argv[i][2] == '\0'){
			error("-l: name missing");
			usage();
		    }
		    cmd_flags.files[cmd_flags.nfiles++] = argv[i];
		    lflags_seen = TRUE;
		}
		else if(strcmp(argv[i], "-framework") == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    if(i + 1 >= argc){
			error("not enough arguments follow %s", argv[i]);
			usage();
		    }
		    cmd_flags.files[cmd_flags.nfiles++] = argv[i];
		    cmd_flags.files[cmd_flags.nfiles++] = argv[i+1];
		    lflags_seen = TRUE;
		    i += 1;
		}
		else if(strcmp(argv[i], "-T") == 0){
		    if(cmd_flags.L_or_T_specified == TRUE){
			error("both -T and -L can't be specified");
			usage();
		    }
		    cmd_flags.L_or_T_specified = TRUE;
		    cmd_flags.use_long_names = FALSE;
		}
		else if(argv[i][1] == 'L' || argv[i][1] == 'F'){
		    if(argv[i][1] == 'L' && argv[i][2] == '\0'){
			if(cmd_flags.L_or_T_specified == TRUE){
			    error("both -T and -L can't be specified");
			    usage();
			}
			cmd_flags.L_or_T_specified = TRUE;
			cmd_flags.use_long_names = TRUE;
		    }
		    else{
			if(cmd_flags.ranlib == TRUE){
			    error("unknown option: %s", argv[i]);
			    usage();
			}
			cmd_flags.Ldirs = realloc(cmd_flags.Ldirs,
				    sizeof(char *) * (cmd_flags.nLdirs + 1));
			cmd_flags.Ldirs[cmd_flags.nLdirs++] = argv[i];
		    }
		}
		else if(argv[i][1] == 'g'){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    /* We need to ignore -g[gdb,codeview,stab][number] flags */
			;
		}
		else if(strcmp(argv[i], "-pg") == 0){
		    if(cmd_flags.ranlib == TRUE){
			error("unknown option: %s", argv[i]);
			usage();
		    }
		    /* We need to ignore -pg */
			;
		}
		else{
		    for(j = 1; argv[i][j] != '\0'; j++){
			switch(argv[i][j]){
			case 's':
			    cmd_flags.s = TRUE;
			    break;
			case 'a':
			    cmd_flags.a = TRUE;
			    break;
			case 'c':
			    cmd_flags.c = TRUE;
			    break;
			case 'v':
			    if(cmd_flags.ranlib == TRUE){
				error("unknown option character `%c' in: %s",
				      argv[i][j], argv[i]);
				usage();
			    }
			    cmd_flags.verbose= TRUE;
			    break;
			case 't':
			    if(cmd_flags.ranlib == TRUE){
				warning("touch option (`%c' in: %s) ignored "
					"(table of contents rebuilt anyway)",
					argv[i][j], argv[i]);
				cmd_flags.t = TRUE;
				break;
			    }
			    else {
				error("unknown option character `%c' in: %s",
				      argv[i][j], argv[i]);
				usage();
			    }
			case 'f':
			    if(cmd_flags.ranlib == TRUE){
				cmd_flags.f = TRUE;
				break;
			    }
			    else {
				error("unknown option character `%c' in: %s",
				      argv[i][j], argv[i]);
				usage();
			    }
			default:
			    error("unknown option character `%c' in: %s",
				  argv[i][j], argv[i]);
			    usage();
			}
		    }
		}
	    }
	    else
		cmd_flags.files[cmd_flags.nfiles++] = argv[i];
	}
	/*
         * Test to see if the environment variable RC_TRACE_ARCHIVES is set.
         */
        if(getenv("RC_TRACE_ARCHIVES") != NULL)
	    cmd_flags.rc_trace_archives = TRUE;

	/*
	 * If the environment variable NEXT_ROOT is set prepend it to the
	 * standard paths for library searches.  This was added to ease
	 * cross build environments.
	 */
	next_root = getenv("NEXT_ROOT");
	if(next_root != NULL){
	    for(i = 0; standard_dirs[i] != NULL; i++){
		p = allocate(strlen(next_root) +
			     strlen(standard_dirs[i]) + 1);
		strcpy(p, next_root);
		strcat(p, standard_dirs[i]);
		standard_dirs[i] = p;
	    }
	}

	/* check the command line arguments for correctness */
	if(cmd_flags.ranlib == FALSE && cmd_flags.dynamic == TRUE){
	    if(cmd_flags.s == TRUE){
		warning("-static not specified, -s invalid");
	    }
	    if(cmd_flags.a == TRUE){
		warning("-static not specified, -a invalid");
	    }
	    if(cmd_flags.c == TRUE){
		warning("-static not specified, -c invalid");
	    }
	    if(cmd_flags.L_or_T_specified == TRUE){
		if(cmd_flags.use_long_names == TRUE)
		    warning("-static not specified, -L invalid");
		else
		    warning("-static not specified, -T invalid");
	    }
	}
	if(cmd_flags.s == TRUE && cmd_flags.a == TRUE){
	    error("only one of -s or -a can be specified");
	    usage();
	}
	if(cmd_flags.ranlib == FALSE && cmd_flags.output == NULL){
	    error("no output file specified (specify with -o output)");
	    usage();
	}
	if(cmd_flags.dynamic == FALSE){
	    if(cmd_flags.compatibility != NULL){
		warning("-dynamic not specified, -compatibility_version %s "
		      "invalid", cmd_flags.compatibility);
	    }
	    if(cmd_flags.current != NULL){
		warning("-dynamic not specified, -current_version %s invalid",
		      cmd_flags.current);
	    }
	    if(cmd_flags.install_name != NULL){
		warning("-dynamic not specified, -install_name %s invalid",
		      cmd_flags.install_name);
	    }
	    if(cmd_flags.seg1addr != NULL){
		warning("-dynamic not specified, -seg1addr %s invalid",
		      cmd_flags.seg1addr);
	    }
	    if(cmd_flags.segs_read_only_addr != NULL){
		warning("-dynamic not specified, -segs_read_only_addr %s "
			"invalid", cmd_flags.segs_read_only_addr);
	    }
	    if(cmd_flags.segs_read_write_addr != NULL){
		warning("-dynamic not specified, -segs_read_write_addr %s "
			"invalid", cmd_flags.segs_read_write_addr);
	    }
	    if(cmd_flags.seg_addr_table != NULL){
		warning("-dynamic not specified, -seg_addr_table %s "
			"invalid", cmd_flags.seg_addr_table);
	    }
	    if(cmd_flags.seg_addr_table_filename != NULL){
		warning("-dynamic not specified, -seg_addr_table_filename %s "
			"invalid", cmd_flags.seg_addr_table_filename);
	    }
	    if(cmd_flags.all_load_flag_specified == TRUE){
		if(cmd_flags.all_load == TRUE)
		    warning("-dynamic not specified, -all_load invalid");
		else
		    warning("-dynamic not specified, -noall_load invalid");
	    }
	    if(cmd_flags.nldflags != 0){
		fprintf(stderr, "%s: -dynamic not specified the following "
			"flags are invalid: ", progname);
		for(i = 0; i < cmd_flags.nldflags; i++)
		    fprintf(stderr, "%s ", cmd_flags.ldflags[i]);
		fprintf(stderr, "\n");
	    }
	    if(cmd_flags.nLdirs != 0){
		/* Note: both -L and -F flags are in cmd_flags.Ldirs to keep the
		   search order right. */
		bad_flag_seen = FALSE;
		for(i = 0; i < cmd_flags.nLdirs; i++){
		    if(strncmp(cmd_flags.Ldirs[i], "-L", 2) == 0)
			continue;
		    if(bad_flag_seen == FALSE){
			fprintf(stderr, "%s: -dynamic not specified the "
				"following flags are invalid: ", progname);
			bad_flag_seen = TRUE;
		    }
		    fprintf(stderr, "%s ", cmd_flags.Ldirs[i]);
		}
		if(bad_flag_seen == TRUE)
		    fprintf(stderr, "\n");
	    }
	}
	else{
	    /*
	     * The -prebind flag can also be specified with the LD_PREBIND
	     * environment variable.
	     */
	    if(getenv("LD_PREBIND") != NULL){
		if(cmd_flags.prebinding_flag_specified == TRUE &&
		   cmd_flags.prebinding == FALSE){
		    warning("LD_PREBIND environment variable ignored because "
			    "-noprebind specified");
		}
		else{
		    cmd_flags.prebinding_flag_specified = TRUE;
		    cmd_flags.prebinding = TRUE;
		}
	    }
	}
	if(cmd_flags.nfiles == 0){
	    if(cmd_flags.ranlib == TRUE){
		error("no archives specified");
		usage();
	    }
	    else{
		if(cmd_flags.dynamic == TRUE && cmd_flags.no_files_ok == TRUE)
		    warning("warning no files specified");
		else{
		    error("no files specified");
		    usage();
		}
	    }
	}

	/* set the defaults if not specified */
	if(cmd_flags.a == FALSE)
	    cmd_flags.s = TRUE; /* sort table of contents by default */

	process();

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
	if(cmd_flags.ranlib)
	    fprintf(stderr, "Usage: %s [-sactfLT] [-] archive [...]\n",
		    progname);
	else{
	    fprintf(stderr, "Usage: %s -static [-] file [...] "
		    "[-filelist listfile[,dirname]] [-arch_only arch] "
		    "[-sacLT]\n", progname);
	    fprintf(stderr, "Usage: %s -dynamic [-] file [...] "
		    "[-filelist listfile[,dirname]] [-arch_only arch] "
		    "[-o output] [-install_name name] "
		    "[-compatibility_version #] [-current_version #] "
		    "[-seg1addr 0x#] [-segs_read_only_addr 0x#] "
		    "[-segs_read_write_addr 0x#] [-seg_addr_table <filename>] "
		    "[-seg_addr_table_filename <file_system_path>] "
		    "[-all_load] [-noall_load]\n",
		    progname);
	}
	exit(EXIT_FAILURE);
}

/*
 * process() the input files into libraries based on the command flags.
 */
static
void
process(
void)
{
    unsigned long i, j, k, previous_errors;
    struct ofile *ofiles;
    char *file_name;
    enum bool flag, rc_trace_archive_printed;

	/*
	 * For libtool processing put all input files in the specified output
	 * file.  For ranlib processing all input files should be archives or
	 * fat files with archives in them and each is processed by itself and
	 * not combined with anything else.  The format of fat object files in
	 * a thin archive is supported here also.
	 */
	ofiles = allocate(sizeof(struct ofile) * cmd_flags.nfiles);
	for(i = 0; i < cmd_flags.nfiles; i++){
	    if(strncmp(cmd_flags.files[i], "-l", 2) == 0){
		file_name = file_name_from_l_flag(cmd_flags.files[i]);
		if(file_name != NULL)
		    if(ofile_map(file_name, NULL, NULL, ofiles + i, TRUE) ==
		       FALSE)
			continue;
	    }
	    else if(strcmp(cmd_flags.files[i], "-framework") == 0){
		i++;
		continue;
	    }
	    else{
		if(ofile_map(cmd_flags.files[i], NULL, NULL, ofiles + i,
			     TRUE) == FALSE)
		    continue;
	    }

	    previous_errors = errors;
	    errors = 0;
	    rc_trace_archive_printed = FALSE;

	    if(ofiles[i].file_type == OFILE_FAT){
		(void)ofile_first_arch(ofiles + i);
		do{
		    if(ofiles[i].arch_type == OFILE_ARCHIVE){
			if(cmd_flags.rc_trace_archives == TRUE &&
			   cmd_flags.dynamic == FALSE &&
			   rc_trace_archive_printed == FALSE){
			    printf("[Logging for Build & Integration] Used "
				   "static archive: %s\n", ofiles[i].file_name);
			    rc_trace_archive_printed = TRUE;
			}
			/* loop through archive */
			if((flag = ofile_first_member(ofiles + i)) == TRUE){
			    if(ofiles[i].member_ar_hdr != NULL &&
			       strncmp(ofiles[i].member_name, SYMDEF,
				       sizeof(SYMDEF) - 1) == 0)
				flag = ofile_next_member(ofiles + i);
			    while(flag == TRUE){
				/* No fat members in a fat file */
				if(ofiles[i].mh != NULL ||
				   cmd_flags.ranlib == TRUE)
				    add_member(ofiles + i);
				else{
				    error("for architecture: %s file: %s(%.*s) "
					  "is not an object file (not allowed "
					  "in a library)",
					  ofiles[i].arch_flag.name,
					  cmd_flags.files[i],
					  (int)ofiles[i].member_name_size,
					  ofiles[i].member_name);
				}
				flag = ofile_next_member(ofiles + i);
			    }
			}
		    }
		    else if(ofiles[i].arch_type == OFILE_Mach_O){
			if(cmd_flags.ranlib == TRUE){
			    error("for architecture: %s file: %s is not an "
				  "archive (no processing done on this file)",
				  ofiles[i].arch_flag.name, cmd_flags.files[i]);
			    goto ranlib_fat_error;
			}
			else
			    add_member(ofiles + i);
		    }
		    else if(ofiles[i].arch_type == OFILE_UNKNOWN){
			if(cmd_flags.ranlib == TRUE){
			    error("for architecture: %s file: %s is not an "
				  "archive (no processing done on this file)",
				  ofiles[i].arch_flag.name, cmd_flags.files[i]);
			    goto ranlib_fat_error;
			}
			else{
			    error("for architecture: %s file: %s is not an "
				  "object file (not allowed in a library)",
				  ofiles[i].arch_flag.name, cmd_flags.files[i]);
			}
		    }
		}while(ofile_next_arch(ofiles + i) == TRUE);
	    }
	    else if(ofiles[i].file_type == OFILE_ARCHIVE){
		if(cmd_flags.rc_trace_archives == TRUE &&
		   cmd_flags.dynamic == FALSE &&
		   rc_trace_archive_printed == FALSE){
		    printf("[Logging for Build & Integration] Used static "
			   "archive: %s\n", ofiles[i].file_name);
		    rc_trace_archive_printed = TRUE;
		}
		/* loop through archive */
		if((flag = ofile_first_member(ofiles + i)) == TRUE){
		    if(ofiles[i].member_ar_hdr != NULL &&
		       strncmp(ofiles[i].member_name, SYMDEF,
			       sizeof(SYMDEF) - 1) == 0)
			flag = ofile_next_member(ofiles + i);
		    while(flag == TRUE){
			/* incorrect form: archive with fat object members */
			if(ofiles[i].member_type == OFILE_FAT){
			    (void)ofile_first_arch(ofiles + i);
			    do{
				if(ofiles[i].mh != NULL ||
				   cmd_flags.ranlib == TRUE){
				    add_member(ofiles + i);
				}
				else{
				    /*
				     * Can't really get here because ofile_*()
				     * routines will refuse to process this
				     * type of file (but I'll leave it here).
				     */
				    error("file: %s(%.*s) for architecture: %s "
					"is not an object file (not allowed in "
					"a library)", cmd_flags.files[i],
					(int)ofiles[i].member_name_size,
					ofiles[i].member_name,
					ofiles[i].arch_flag.name);
				}

			    }while(ofile_next_arch(ofiles + i) == TRUE);
			}
			else if(ofiles[i].mh != NULL ||
				cmd_flags.ranlib == TRUE){
			    add_member(ofiles + i);
			}
			else{
			    error("file: %s(%.*s) is not an object file (not "
				  "allowed in a library)", cmd_flags.files[i],
				  (int)ofiles[i].member_name_size,
				  ofiles[i].member_name);
			}
			flag = ofile_next_member(ofiles + i);
		    }
		}
	    }
	    else if(ofiles[i].file_type == OFILE_Mach_O){
		if(cmd_flags.ranlib == TRUE){
		    error("file: %s is not an archive", cmd_flags.files[i]);
		    continue;
		}
		add_member(ofiles + i);
	    }
	    else{ /* ofiles[i].file_type == OFILE_UNKNOWN */
		if(cmd_flags.ranlib == TRUE){
		    error("file: %s is not an archive", cmd_flags.files[i]);
		    continue;
		}
		else{
		    error("file: %s is not an object file (not allowed in a "
			  "library)", cmd_flags.files[i]);
		}
	    }

	    if(cmd_flags.ranlib == TRUE){
		/*
		 * In the case where ranlib is being used on an archive that
		 * contains fat object files with multiple members and non-
		 * object members this has to be treated as an error because
		 * it is not known which architecture(s) the non-object file
		 * belong to.
		 */
		if(narchs > 1){
		    for(j = 0; j < narchs; j++){
			for(k = 0; k < archs[j].nmembers; k++){
			    if(archs[j].members[k].mh == NULL){
				error("library member: %s(%.*s) is not an "
				      "object file (not allowed in a library "
				      "with multiple architectures)",
				      cmd_flags.files[i],
				      (int)archs[j].members[k].
					input_base_name_size,
				      archs[j].members[k].input_base_name);
			    }
			}
		    }
		}
		if(errors == 0)
		    create_library(cmd_flags.files[i]);
		if(cmd_flags.nfiles > 1){
ranlib_fat_error:
		    free_archs();
		    ofile_unmap(ofiles + i);
		}
	    }
	    errors += previous_errors;
	}
	if(cmd_flags.ranlib == FALSE && errors == 0)
	    create_library(cmd_flags.output);

	/*
	 * Clean-up of ofiles[] and archs could be done here but since this
	 * program is now done it is faster to just exit.
	 */
}

/*
 * file_name_from_l_flag() is passed a "-lx" flag and returns a name of a file
 * for this flag.  The flag "-lx" is the same flag as used in the link editor
 * to refer to file names.  If it can't find a file name for the flag it prints
 * an error and returns NULL.
 */
static
char *
file_name_from_l_flag(
char *l_flag)
{
    char *file_name, *p;

	p = &l_flag[2];
	p = strrchr(p, '.');
	if(p != NULL && strcmp(p, ".o") == 0){
	    p = &l_flag[2];
	    file_name = search_for_file(p);
	}
	else{
	    file_name = NULL;
	    if(cmd_flags.dynamic == TRUE){
		p = makestr("lib", &l_flag[2], ".dylib", NULL);
		file_name = search_for_file(p);
		free(p);
	    }
	    if(file_name == NULL){
		p = makestr("lib", &l_flag[2], ".a", NULL);
		file_name = search_for_file(p);
		free(p);
	    }
	}
	if(file_name == NULL)
	    error("can't locate file for: %s", l_flag);
	return(file_name);
}

/*
 * search_for_file() takes base_name and trys to find a file with that base name
 * is the -L search directories and in the standard directories.  If it is
 * sucessful it returns a pointer to the file name else it returns NULL.
 */
static
char *
search_for_file(
char *base_name)
{
    unsigned long i;
    char *file_name;

	for(i = 0; i < cmd_flags.nLdirs ; i++){
	    if(cmd_flags.Ldirs[i][1] != 'L')
		continue;
	    file_name = makestr(cmd_flags.Ldirs[i] + 2, "/", base_name, NULL);
	    if(access(file_name, R_OK) != -1)
		return(file_name);
	    free(file_name);
	}
	for(i = 0; standard_dirs[i] != NULL ; i++){
	    file_name = makestr(standard_dirs[i], base_name, NULL);
	    if(access(file_name, R_OK) != -1)
		return(file_name);
	    free(file_name);
	}
	return(NULL);
}

/*
 * add_member() add the specified ofile as a member to the library.  The
 * specified ofile must be either an object file (libtool or ranlib) or an
 * archive member with an unknown file type (ranlib only).
 */
static
void
add_member(
struct ofile *ofile)
{
    unsigned long i, j, size, ar_name_size;
    struct arch *arch;
    struct member *member;
    struct stat stat_buf;
    char *p, c, ar_name_buf[sizeof(ofile->member_ar_hdr->ar_name) + 1];
    char ar_size_buf[sizeof(ofile->member_ar_hdr->ar_size) + 1];
    const struct arch_flag *family_arch_flag;
    long k;
    enum bool has_space_in_name;

	/*
	 * If this did not come from an archive get the stat info which is
	 * needed to fill in the archive header for this member.
	 */
	if(ofile->member_ar_hdr == NULL){
	    if(stat(ofile->file_name, &stat_buf) == -1){
		system_error("can't stat file: %s", ofile->file_name);
		return;
	    }
	}

	/*
	 * Determine the size this member will have in the library which
	 * includes the padding as a result of rounding the size of the
	 * member.  To get all members on a 4 byte boundary (so that mapping
	 * in object files can be used directly) the size of the member is
	 * CHANGED to reflect this padding.  In the UNIX definition of archives
	 * the size of the member is never changed but the offset to the next
	 * member is defined to be the offset of the previous member plus
	 * the size of the previous member rounded to 2.  So to get 4 byte
	 * boundaries without breaking the UNIX definition of archives the
	 * size is changed here.  As with the UNIX ar(1) program the padded
	 * bytes are set to the character '\n'.
	 */
	if(ofile->mh != NULL)
	    size = round(ofile->object_size, sizeof(long));
	else{
	    size = round(ofile->member_size, sizeof(long));
	}

	/* select or create an arch type to put this in */
	i = 0;
	if(ofile->mh != NULL){
	    if(ofile->mh->cputype == 0){
		if(ofile->member_ar_hdr != NULL){
		    error("file: %s(%.*s) cputype is zero (a reserved value)",
			  ofile->file_name, (int)ofile->member_name_size,
			  ofile->member_name);
		}
		else
		    error("file: %s cputype is zero (a reserved value)",
			  ofile->file_name);
		return;
	    }
	    /*
	     * If we are building a dynamic library then don't add dynamic
	     * shared libraries to the archs.  This is so that a dependent
	     * dynamic shared library this happens to be fat will not cause the
	     * library to be created fat unless there are object going into
	     * the library that are fat.
	     */
	    if(ofile->mh->filetype == MH_DYLIB){
		/*
		 * If we are building a static library we should not put a
		 * dynamic library Mach-O file into the static library.  This
		 * can happen if a libx.a file is really a dynamic library and
		 * someone is using -lx when creating a static library.
		 */
		if(cmd_flags.dynamic != TRUE){
		    if(ofile->member_ar_hdr != NULL){
			warning("file: %s(%.*s) is a dynamic library, not "
				"added to the static library", 
			        ofile->file_name, (int)ofile->member_name_size,
			        ofile->member_name);
		    }
		    else
			warning("file: %s is a dynamic library, not added to "
				"the static library", ofile->file_name);
		}
		return;
	    }
	    /*
	     * If -arch_only is specified then only add this file if it matches
	     * the architecture specified.
	     */
	    if(cmd_flags.arch_only_flag.name != NULL &&
	       cmd_flags.arch_only_flag.cputype != ofile->mh->cputype)
		return;

	    for( ; i < narchs; i++){
		if(archs[i].arch_flag.cputype == ofile->mh->cputype)
		    break;
	    }
	}
	if(narchs == 1 && archs[0].arch_flag.cputype == 0){
	    i = 0;
	}
	else if(i == narchs){
	    archs = reallocate(archs, sizeof(struct arch) * (narchs+1));
	    memset(archs + narchs, '\0', sizeof(struct arch));
	    if(ofile->mh != NULL){
		family_arch_flag =
			get_arch_family_from_cputype(ofile->mh->cputype);
		if(family_arch_flag != NULL)
		    archs[narchs].arch_flag = *family_arch_flag;
	    }
	    else
		archs[narchs].arch_flag.name = "unknown";
	    narchs++;
	}
	arch = archs + i;

	/*
	 * If the cputype of this arch is not yet known then see if this new
	 * member can determine it.
	 */
	if(arch->arch_flag.cputype == 0 && ofile->mh != NULL){
	    family_arch_flag = get_arch_family_from_cputype(ofile->mh->cputype);
	    arch->arch_flag = *family_arch_flag;
	}

	/* create a member in this arch type for this member */
	arch->members = reallocate(arch->members, sizeof(struct member) *
				   (arch->nmembers + 1));
	member = arch->members + arch->nmembers;
	memset(member, '\0', sizeof(struct member));
	arch->nmembers++;

	/* fill in the member for this ofile */
	member->input_file_name = ofile->file_name;

	if(ofile->member_ar_hdr == NULL){
	    /*
	     * We are creating an archive member in the output file from a
	     * file (that is not archive member in an input file).  First get
	     * the base name the file_name for the member name.
	     */
	    p = strrchr(ofile->file_name, '/');
	    if(p != NULL)
		p++;
	    else
		p = ofile->file_name;
	    member->input_base_name = p;
	    member->input_base_name_size = strlen(p);
	    member->member_name = member->input_base_name;
	    /*
	     * If we can use long names then if the name does not fit in the
	     * archive header or contains a space character then use the
	     * extened format #1.  The size of the name is rounded up so the
	     * object file after the name will be on a sizeof(unsigned long)
	     * boundary.  The name will be padded with '\0's when it is
	     * written out.
	     */
	    if(cmd_flags.use_long_names == TRUE &&
	       (member->input_base_name_size >
					    sizeof(member->ar_hdr.ar_name) ||	
	        strchr(member->input_base_name, ' ') != NULL)){
		member->output_long_name = TRUE;
		member->member_name_size = member->input_base_name_size;
		ar_name_size = 
		    round(member->input_base_name_size, sizeof(unsigned long));
		sprintf(ar_name_buf, "%s%-*lu", AR_EFMT1, 
			(int)(sizeof(member->ar_hdr.ar_name) -
			      (sizeof(AR_EFMT1) - 1)), ar_name_size);
		memcpy(member->ar_hdr.ar_name, ar_name_buf,
		      sizeof(member->ar_hdr.ar_name));
	    }
	    else{
		ar_name_size = 0;
		member->output_long_name = FALSE;
		/*
		 * Truncate the file_name if needed and place in archive header.
		 */
		c = '\0';
		if(strlen(p) > sizeof(member->ar_hdr.ar_name)){
		    c = p[sizeof(member->ar_hdr.ar_name)];
		    p[sizeof(member->ar_hdr.ar_name)] = '\0';
		}
		sprintf((char *)(&member->ar_hdr), "%-*s",
			(int)sizeof(member->ar_hdr.ar_name), p);
		if(c != '\0')
		    p[sizeof(member->ar_hdr.ar_name)] = c;
		member->member_name_size = size_ar_name(&member->ar_hdr);
	    }
	    /*
	     * Create the rest of the archive header after the name.
	     */
	    sprintf((char *)(&member->ar_hdr) + sizeof(member->ar_hdr.ar_name),
	       "%-*ld%-*u%-*u%-*o%-*ld%-*s",
	       (int)sizeof(member->ar_hdr.ar_date),
		   (long int)stat_buf.st_mtime,
	       (int)sizeof(member->ar_hdr.ar_uid),
		   (unsigned short)stat_buf.st_uid,
	       (int)sizeof(member->ar_hdr.ar_gid),
		   (unsigned short)stat_buf.st_gid,
	       (int)sizeof(member->ar_hdr.ar_mode),
		   (unsigned int)stat_buf.st_mode,
	       (int)sizeof(member->ar_hdr.ar_size),
		   (long)size + ar_name_size,
	       (int)sizeof(member->ar_hdr.ar_fmag),
		   ARFMAG);
	}
	else{
	    /*
	     * We are creating an archive member in the output file from an
	     * archive member in an input file.  There can be some changes to
	     * the contents.  First the size might be changed and the contents
	     * padded with '\n's to round it to a multiple of sizeof(unsigned
	     * long).  Second we may take a member using extended format #1
	     * for it's name and truncate it then place the name in the archive
	     * header.  Or we may round the name size to a multiple of
	     * sizeof(unsigned long).
	     */
	    member->input_ar_hdr = ofile->member_ar_hdr;
	    member->input_base_name = ofile->member_name;
	    member->input_base_name_size = ofile->member_name_size;

	    member->ar_hdr = *(ofile->member_ar_hdr);
	    member->member_name = ofile->member_name;

	    if(cmd_flags.use_long_names == TRUE){
		/*
		 * We can use long names.  So if the input ofile is using the
		 * extended format #1 we need make sure the size of the name is
		 * rounded to sizeof(unsigned long) and write that size into the
		 * ar_name with the AR_EFMT1 string.
		 */
		if(ofile->member_name != ofile->member_ar_hdr->ar_name){
		    member->output_long_name = TRUE;
		    member->member_name_size = ofile->member_name_size;
		    ar_name_size = round(ofile->member_name_size,
					 sizeof(unsigned long));
		    sprintf(ar_name_buf, "%s%-*lu", AR_EFMT1, 
			    (int)(sizeof(member->ar_hdr.ar_name) -
				  (sizeof(AR_EFMT1) - 1)), ar_name_size);
		    memcpy(member->ar_hdr.ar_name, ar_name_buf,
			  sizeof(member->ar_hdr.ar_name));
		}
		else{
		    /*
		     * Check for names with spaces in ar_name, if it has spaces
		     * in the name convert it to extended format #1.
		     */
		    for(k = sizeof(member->ar_hdr.ar_name) - 1 ; k > 0 ; k--)
			if(ofile->member_name[k] != ' ')
			    break;
		    has_space_in_name = FALSE;
		    for( ; k > 0 ; k--){
			if(ofile->member_name[k] == ' '){
			    has_space_in_name = TRUE;
			    break;
			}
		    }
		    member->member_name_size = size_ar_name(&member->ar_hdr);
		    if(has_space_in_name == TRUE){
			ar_name_size = round(member->member_name_size,
					     sizeof(unsigned long));
			member->output_long_name = TRUE;
			sprintf(ar_name_buf, "%s%-*lu", AR_EFMT1, 
				(int)(sizeof(member->ar_hdr.ar_name) -
				      (sizeof(AR_EFMT1) - 1)), ar_name_size);
			memcpy(member->ar_hdr.ar_name, ar_name_buf,
			      sizeof(member->ar_hdr.ar_name));
		    }
		    else{
			ar_name_size = 0;
			member->output_long_name = FALSE;
		    }
		}
	    }
	    else{
		/*
		 * We can't use long names.  So if the input ofile is using the
		 * extended format #1 we need to truncate the name and write it
		 * into the ar_name field.  Note the extended format is also
		 * used it the name has a space in it so it may be shorter than
		 * sizeof(ar_hdr.ar_name) .
		 */
		ar_name_size = 0;
		member->output_long_name = FALSE;
		if(ofile->member_name != ofile->member_ar_hdr->ar_name){
		    for(j = 0; j < sizeof(member->ar_hdr.ar_name) &&
			       j < ofile->member_name_size &&
			       ofile->member_name[j] != '\0'; j++)
			member->ar_hdr.ar_name[j] = ofile->member_name[j];
		    for( ; j < sizeof(member->ar_hdr.ar_name); j++)
			member->ar_hdr.ar_name[j] = ' ';
		}
		member->member_name_size = size_ar_name(&member->ar_hdr);
	    }
	    /*
	     * Since sprintf() writes a '\0' at the end of the string the
	     * memcpy is needed to preserve the ARFMAG string that follows.
	     */
	    sprintf(ar_size_buf, "%-*ld",
		    (int)sizeof(member->ar_hdr.ar_size),
		    (long)size + ar_name_size);
	    memcpy(member->ar_hdr.ar_size, ar_size_buf,
		   sizeof(member->ar_hdr.ar_size));
	}

	member->offset = arch->size;
	arch->size += sizeof(struct ar_hdr) + size + ar_name_size;

	if(ofile->mh != NULL){
	    member->object_addr = ofile->object_addr;
	    member->object_size = ofile->object_size;
	    member->object_byte_sex = ofile->object_byte_sex;
	    member->mh = ofile->mh;
	    member->load_commands = ofile->load_commands;
	}
	else{
	    member->object_addr = ofile->member_addr;
	    member->object_size = ofile->member_size;
	}
}

/*
 * free_archs() frees the memory allocated that is pointed to by archs.
 */
static
void
free_archs(
void)
{
    unsigned long i;

	for(i = 0 ; i < narchs; i++){
	    /* 
	     * Just leak memory on the arch_flag.name in some cases
	     * (unknown archiectures only where the space is malloced and
	     * a sprintf() is done into the memory)
	     */
	    if(archs[i].toc_ranlibs != NULL)
		free(archs[i].toc_ranlibs);
	    if(archs[i].toc_strings != NULL)
		free(archs[i].toc_strings);
	    if(archs[i].members != NULL)
		free(archs[i].members);
	}
	if(archs != NULL)
	    free(archs);
	archs = NULL;
	narchs = 0;
}

/*
 * create_library() creates a library from the data structure pointed to by
 * archs into the specified output file.  Only when more than one architecture
 * is in archs will a fat file be created.
 */
static
void
create_library(
char *output)
{
    unsigned long i, j, k, l, library_size, offset, pad, *time_offsets;
    enum byte_sex target_byte_sex;
    char *library, *p;
    kern_return_t r;
    struct arch *arch;
    struct fat_header *fat_header;
    struct fat_arch *fat_arch;
    int fd;
#ifndef __OPENSTEP__
    struct utimbuf timep;
#else
    time_t timep[2];
#endif
    struct stat stat_buf;
    struct ar_hdr toc_ar_hdr;

	if(narchs == 0){
	    if(cmd_flags.ranlib == TRUE){
		warning("empty library: %s (no table of contents added)",
			output);
		return;
	    }
	    else{
		if(cmd_flags.dynamic == FALSE ||
		   cmd_flags.no_files_ok == FALSE){
		    error("no library created (no object files in input "
			  "files)");
		    return;
		}
	    }
	}

	if(cmd_flags.dynamic == TRUE){
	    create_dynamic_shared_library(output);
	    return;
	}

	/* if this is libtool warn about duplicate member names */
	if(cmd_flags.ranlib == FALSE)
	    warn_duplicate_member_names();

	/*
	 * Calculate the total size of the library and the final size of each
	 * architecture.
	 */
	if(narchs > 1){
	    library_size = sizeof(struct fat_header) +
			   sizeof(struct fat_arch) * narchs;
	    if(cmd_flags.f == TRUE)
		warning("archive library: %s will be fat and ar(1) will not "
			"be able to operate on it", output);
	}
	else
	    library_size = 0;
	for(i = 0; i < narchs; i++){
	    make_table_of_contents(archs + i, output);
	    archs[i].size += SARMAG + archs[i].toc_size;
	    library_size += archs[i].size;
	}

	/*
	 * This buffer is vm_allocate'ed to make sure all holes are filled with
	 * zero bytes.
	 */
	if((r = vm_allocate(mach_task_self(), (vm_address_t *)&library,
			    library_size, TRUE)) != KERN_SUCCESS)
	    mach_fatal(r, "can't vm_allocate() buffer for output file: %s of "
		       "size %lu", output, library_size);

	/*
	 * If there is more than one architecture then fill in the fat file
	 * header and the fat_arch structures in the buffer.
	 */
	if(narchs > 1){
	    fat_header = (struct fat_header *)library;
	    fat_header->magic = FAT_MAGIC;
	    fat_header->nfat_arch = narchs;
	    offset = sizeof(struct fat_header) +
			    sizeof(struct fat_arch) * narchs;
	    fat_arch = (struct fat_arch *)(library + sizeof(struct fat_header));
	    for(i = 0; i < narchs; i++){
		fat_arch[i].cputype = archs[i].arch_flag.cputype;
		fat_arch[i].cpusubtype = archs[i].arch_flag.cpusubtype;
		fat_arch[i].offset = offset;
		fat_arch[i].size = archs[i].size;
		fat_arch[i].align = 2;
		offset += archs[i].size;
	    }
#ifdef __LITTLE_ENDIAN__
	    swap_fat_header(fat_header, BIG_ENDIAN_BYTE_SEX);
	    swap_fat_arch(fat_arch, narchs, BIG_ENDIAN_BYTE_SEX);
#endif /* __LITTLE_ENDIAN__ */
	    offset = sizeof(struct fat_header) +
			    sizeof(struct fat_arch) * narchs;
	}
	else
	    offset = 0;

	/*
	 * The time_offsets array records the offsets to the table of conternts
	 * archive header's ar_date fields.
	 */
	time_offsets = allocate(narchs * sizeof(unsigned long));

	/*
	 * Now put each arch in the buffer.
	 */
	for(i = 0; i < narchs; i++){
	    p = library + offset;
	    arch = archs + i;

	    /*
	     * If the input files only contains non-object files then the
	     * byte sex of the output can't be determined which is needed for
	     * the two binary long's of the table of contents.  But since these
	     * will be zero (the same in both byte sexes) because there are no
	     * symbols in the table of contents if there are no object files.
	     */

	    /* put in the archive magic string */
	    memcpy(p, ARMAG, SARMAG);
	    p += SARMAG;

	    /*
	     * Warn for what really is a bad library that has an empty table of
	     * contents but this is allowed in the original ranlib.
	     */
	    if(arch->toc_nranlibs == 0){
		if(narchs > 1)
		    warning("warning for library: %s for architecture: %s the "
			    "table of contents is empty (no object file members"
			    " in the library define global symbols)", output,
			    arch->arch_flag.name);
		else
		    warning("warning for library: %s the table of contents is "
			    "empty (no object file members in the library "
			    "define global symbols)", output);
	    }

	    /*
	     * Pick the byte sex to write the table of contents in.
	     */
	    target_byte_sex = UNKNOWN_BYTE_SEX;
	    for(j = 0;
		j < arch->nmembers && target_byte_sex == UNKNOWN_BYTE_SEX;
		j++){
		target_byte_sex = arch->members[j].object_byte_sex;
	    }
	    if(target_byte_sex == UNKNOWN_BYTE_SEX)
		target_byte_sex = host_byte_sex;

	    /*
	     * Put in the table of contents member:
	     *	the archive header
	     *  the archive member name (if using a long name)
	     *	a long for the number of bytes of the ranlib structs
	     *	the ranlib structs
	     *	a long for the number of bytes of the strings for the ranlibs
	     *	the strings for the ranlib structs
	     */
	    time_offsets[i] =
			 (p - library) +
			 ((char *)&toc_ar_hdr.ar_date - (char *)&toc_ar_hdr);
	    memcpy(p, (char *)&arch->toc_ar_hdr, sizeof(struct ar_hdr));
	    p += sizeof(struct ar_hdr);

	    if(arch->toc_long_name == TRUE){
		memcpy(p, arch->toc_name, arch->toc_name_size);
		p += arch->toc_name_size;
	    }

	    l = arch->toc_nranlibs * sizeof(struct ranlib);
	    if(target_byte_sex != host_byte_sex)
		l = SWAP_LONG(l);
	    memcpy(p, (char *)&l, sizeof(long));
	    p += sizeof(long);

	    if(target_byte_sex != host_byte_sex)
		swap_ranlib(arch->toc_ranlibs, arch->toc_nranlibs,
			    target_byte_sex);
	    memcpy(p, (char *)arch->toc_ranlibs,
		   arch->toc_nranlibs * sizeof(struct ranlib));
	    p += arch->toc_nranlibs * sizeof(struct ranlib);

	    l = arch->toc_strsize;
	    if(target_byte_sex != host_byte_sex)
		l = SWAP_LONG(l);
	    memcpy(p, (char *)&l, sizeof(long));
	    p += sizeof(long);

	    memcpy(p, (char *)arch->toc_strings, arch->toc_strsize);
	    p += arch->toc_strsize;

	    /*
	     * Put in the archive header and member contents for each member.
	     */
	    for(j = 0; j < arch->nmembers; j++){
		memcpy(p, (char *)&(arch->members[j].ar_hdr),
		       sizeof(struct ar_hdr));
		p += sizeof(struct ar_hdr);

		/*
		 * If we are using extended format #1 for long names write out
		 * the name.  Note the name is padded with '\0' and the
		 * member_name_size is the unrounded size.
		 */
		if(arch->members[j].output_long_name == TRUE){
		    strncpy(p, arch->members[j].member_name,
			    arch->members[j].member_name_size);
		    p += round(arch->members[j].member_name_size,
			       sizeof(unsigned long));
		}

		/*
		 * ofile_map swaps the headers to the host_byte_sex if the
		 * object's byte sex is not the same as the host byte sex so
		 * if this is the case swap them back before writing them out.
		 */
		if(arch->members[j].mh != NULL &&
		   arch->members[j].object_byte_sex != host_byte_sex){
		    if(swap_object_headers(arch->members[j].mh,
		       arch->members[j].load_commands) == FALSE)
			fatal("internal error: swap_object_headers() failed");
		}
		memcpy(p, arch->members[j].object_addr, 
		       arch->members[j].object_size);
		p += arch->members[j].object_size;
		pad = round(arch->members[j].object_size, sizeof(long)) -
		      arch->members[j].object_size;
		/* as with the UNIX ar(1) program pad with '\n' characters */
		for(k = 0; k < pad; k++)
		    *p++ = '\n';
	    }
	    offset += arch->size;
	}

	/*
	 * Create the output file.  The unlink() is done to handle the problem
	 * when the outputfile is not writable but the directory allows the
	 * file to be removed (since the file may not be there the return code
	 * of the unlink() is ignored).
	 */
	(void)unlink(output);
	if((fd = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1){
	    system_error("can't create output file: %s", output);
	    return;
	}
	if(write(fd, library, library_size) != library_size){
	    system_error("can't write output file: %s", output);
	    return;
	}
	if(close(fd) == -1){
	    system_fatal("can't close output file: %s", output);
	    return;
	}

	/*
	 * Now that the library is created on the file system it is written
	 * to get the time for the file on that file system.
	 */
	if(stat(output, &stat_buf) == -1){
	    system_fatal("can't stat file output file: %s", output);
	    return;
	}
	if((fd = open(output, O_WRONLY, 0)) == -1){
	    system_error("can't open output file: %s", output);
	    return;
	}
	/*
         * With the time from the file system the library is on set the ar_date
	 * using the modification time returned by stat.  Then write this into
	 * all the ar_date's in the file.
	 */
	sprintf((char *)(&toc_ar_hdr), "%-*s%-*ld",
	   (int)sizeof(toc_ar_hdr.ar_name), 
	       SYMDEF,
	   (int)sizeof(toc_ar_hdr.ar_date),
	       (long int)stat_buf.st_mtime + 5);
	for(i = 0; i < narchs; i++){
	    if(lseek(fd, time_offsets[i], L_SET) == -1){
		system_error("can't lseek in output file: %s", output);
		return;
	    }
	    if(write(fd, &toc_ar_hdr.ar_date, sizeof(toc_ar_hdr.ar_date)) !=
		     sizeof(toc_ar_hdr.ar_date)){
		system_error("can't write to output file: %s", output);
		return;
	    }
	}
	if(close(fd) == -1){
	    system_fatal("can't close output file: %s", output);
	    return;
	}
	/*
	 * Now set the modtime of the created library back to it's stat time
	 * when we first closed it.
	 */
#ifndef __OPENSTEP__
	timep.actime = stat_buf.st_mtime;
	timep.modtime = stat_buf.st_mtime;
	if(utime(output, &timep) == -1)
#else
	timep[0] = stat_buf.st_mtime;
	timep[1] = stat_buf.st_mtime;
	if(utime(output, timep) == -1)
#endif
	{
	    system_fatal("can't set the modifiy times in output file: %s",
			 output);
	    return;
	}
	if((r = vm_deallocate(mach_task_self(), (vm_address_t)library,
			      library_size)) != KERN_SUCCESS){
	    my_mach_error(r, "can't vm_deallocate() buffer for output file");
	    return;
	}
}

/*
 * tellProjectBuilder() is called to cause the doing messages to be sent to
 * ProjectBuilder.  The string pointed to by message and arch_name together
 * must not be more that 1024 characters.
 */
static
void
tellProjectBuilder(
char *message,
char *arch_name,
char *fileName)
{
    char message_buf[1024];
    mach_port_t ProjectBuilder_port;
    char *portName;
#if defined(__OPENSTEP__) || defined(__GONZO_BUNSEN_BEAKER__)
    char *hostName;
    
	hostName = getenv("MAKEHOST");
	if(hostName == NULL)
	    hostName = "";
#endif
	portName = getenv("MAKEPORT");
	if(portName == NULL)
	    return;
#if defined(__OPENSTEP__) || defined(__GONZO_BUNSEN_BEAKER__)
	if(netname_look_up(name_server_port, hostName, portName,
	   (int *)&ProjectBuilder_port) != KERN_SUCCESS)
	    return;
#else
	if(bootstrap_look_up(bootstrap_port, portName,
	   (int *)&ProjectBuilder_port) != KERN_SUCCESS)
	    return;
#endif
	if(ProjectBuilder_port == MACH_PORT_NULL)
	    return;
    
	strcpy(message_buf, message);
	strcat(message_buf, arch_name);

	make_alert(ProjectBuilder_port,
	    -1,
	    NULL, 0, /* functionName, not used by ProjectBuilder */
	    fileName, strlen(fileName)+1 > 1024 ? 1024 : strlen(fileName)+1,
	    NULL, 0,
	    0,
	    message_buf, strlen(message_buf) + 1);
}

/*
 * create_dynamic_shared_library() creates a dynamic shared library from the
 * data structure pointed to by archs into the specified output file.  Only
 * when more than one architecture is in archs will a fat file be created.
 */
static
void
create_dynamic_shared_library(
char *output)
{
    unsigned long i, j;
    char *p, *filelist;
    struct stat stat_buf;
    enum bool use_force_cpusubtype_ALL;
    const struct arch_flag *family_arch_flag;

	/*
	 * If there is more than one architecture setup a signal handler to
	 * clean up the temporary files in case we get a signal.
	 */
	if(narchs > 1)
	    signal(SIGINT, create_dynamic_shared_library_cleanup);

	/*
	 * If -arch_only is specified with a specific cpusubtype other than the
	 * family cpusubtype do not use -force_cpusubtype_ALL as the user wants
	 * the output to be tagged with that cpusubtype.
	 */
	use_force_cpusubtype_ALL = TRUE;
	if(cmd_flags.arch_only_flag.name != NULL){
	    family_arch_flag = get_arch_family_from_cputype(
				    cmd_flags.arch_only_flag.cputype);
	    if(family_arch_flag != NULL){
		if(family_arch_flag->cpusubtype !=
		   cmd_flags.arch_only_flag.cpusubtype)
		    use_force_cpusubtype_ALL = FALSE;
	    }
	}

	/*
	 * For each architecture run ld(1) -dylib to create the dynamic shared
	 * library.
	 */
	for(i = 0; i < narchs || (i == 0 && narchs == 0); i++){
	    reset_execute_list();
	    add_execute_list("ld");
	    if(narchs != 0 && cmd_flags.arch_only_flag.name == NULL)
		add_execute_list("-arch_multiple");
	    if(archs != NULL){
		add_execute_list("-arch");
		if(use_force_cpusubtype_ALL == TRUE)
		    add_execute_list(archs[i].arch_flag.name);
		else
		    add_execute_list(cmd_flags.arch_only_flag.name);
	    }
	    add_execute_list("-dylib");
	    add_execute_list("-dynamic");
	    if(cmd_flags.all_load_flag_specified == FALSE ||
	       cmd_flags.all_load == TRUE)
		add_execute_list("-all_load");
	    if(use_force_cpusubtype_ALL == TRUE)
		add_execute_list("-force_cpusubtype_ALL");
	    add_execute_list("-no_arch_warnings");
	    if(cmd_flags.seg1addr != NULL){
		add_execute_list("-seg1addr");
		add_execute_list(cmd_flags.seg1addr);
	    }
	    if(cmd_flags.segs_read_only_addr != NULL){
		add_execute_list("-segs_read_only_addr");
		add_execute_list(cmd_flags.segs_read_only_addr);
	    }
	    if(cmd_flags.segs_read_write_addr != NULL){
		add_execute_list("-segs_read_write_addr");
		add_execute_list(cmd_flags.segs_read_write_addr);
	    }
	    if(cmd_flags.seg_addr_table != NULL){
		add_execute_list("-seg_addr_table");
		add_execute_list(cmd_flags.seg_addr_table);
	    }
	    if(cmd_flags.seg_addr_table_filename != NULL){
		add_execute_list("-seg_addr_table_filename");
		add_execute_list(cmd_flags.seg_addr_table_filename);
	    }
	    if(cmd_flags.compatibility != NULL){
		add_execute_list("-dylib_compatibility_version");
		add_execute_list(cmd_flags.compatibility);
	    }
	    if(cmd_flags.current != NULL){
		add_execute_list("-dylib_current_version");
		add_execute_list(cmd_flags.current);
	    }
	    if(cmd_flags.install_name != NULL){
		add_execute_list("-dylib_install_name");
		add_execute_list(cmd_flags.install_name);
	    }
	    else{
		if(narchs > 1){
		    add_execute_list("-dylib_install_name");
		    add_execute_list(cmd_flags.output);
		}
	    }
	    for(j = 0; j < cmd_flags.nldflags; j++)
		add_execute_list(cmd_flags.ldflags[j]);
	    for(j = 0; j < cmd_flags.nLdirs; j++)
		add_execute_list(cmd_flags.Ldirs[j]);
	    add_execute_list("-ldylib1.o");
	    filelist = NULL;
	    for(j = 0; j < cmd_flags.nfiles; j++){
		if(cmd_flags.filelist[j] == NULL){
		    add_execute_list(cmd_flags.files[j]);
		}
		else{
		    if(cmd_flags.filelist[j] != filelist){
			add_execute_list("-filelist");
			add_execute_list(cmd_flags.filelist[j]);
			filelist = cmd_flags.filelist[j];
		    }
		}
	    }
	    if(narchs <= 1){
		add_execute_list("-o");
		add_execute_list(cmd_flags.output);
		tellProjectBuilder("Linking %s", "", cmd_flags.output);
	    }
	    else{
		add_execute_list("-o");
		add_execute_list(makestr(cmd_flags.output, ".libtool.",
					 archs[i].arch_flag.name, NULL));
		if(cmd_flags.final_output_specified == FALSE){
		    add_execute_list("-final_output");
		    add_execute_list(cmd_flags.output);
		}
		tellProjectBuilder("Linking %s for ", archs[i].arch_flag.name,
				   cmd_flags.output);
	    }
	    if(execute_list(cmd_flags.verbose) == 0)
		fatal("internal link edit command failed");
	}
	/*
	 * If there is more than one architecture then run lipo to put them
	 * in a fat file.
	 */
	if(narchs > 1){
	    tellProjectBuilder("Combining into %s", "", cmd_flags.output);
	    reset_execute_list();
	    add_execute_list("lipo");
	    add_execute_list("-create");
	    add_execute_list("-output");
	    add_execute_list(cmd_flags.output);
	    for(i = 0; i < narchs; i++){
		add_execute_list(makestr(cmd_flags.output, ".libtool.",
					 archs[i].arch_flag.name, NULL));
	    }
	    if(execute_list(cmd_flags.verbose) == 0)
		fatal("internal lipo command failed");
	    for(i = 0; i < narchs; i++){
		p = makestr(cmd_flags.output, ".libtool.",
			    archs[i].arch_flag.name, NULL);
		if(unlink(p) == -1){
		    error("can't remove temporary file: %s", p);
		}
	    }
	}
	/*
	 * If we are doing prebinding then run /usr/bin/objcunique on the
	 * output.
	 */ 
	if(cmd_flags.prebinding == TRUE){
	    if(stat("/usr/bin/objcunique", &stat_buf) != -1){
		reset_execute_list();
		add_execute_list("/usr/bin/objcunique");
		add_execute_list(cmd_flags.output);
		add_execute_list("-prebind");
		for(j = 0; j < cmd_flags.nLdirs; j++)
		    add_execute_list(cmd_flags.Ldirs[j]);
		if(execute_list(cmd_flags.verbose) == 0)
		    fatal("internal /usr/bin/objcunique command failed");
	    }
	}
}

/*
 * create_dynamic_shared_library_cleanup() is the signal handler to remove the
 * temporary files if more than one arch is being used.
 */
static
void
create_dynamic_shared_library_cleanup(
int sig)
{
    unsigned long i;

	for(i = 0; i < narchs; i++){
	    (void)unlink(makestr(cmd_flags.output, ".libtool.",
				 archs[i].arch_flag.name, NULL));
	}
	exit(EXIT_FAILURE);
}

/*
 * make_table_of_contents() make the table of contents for the specified arch
 * and fills in the toc_* fields in the arch.  Output is the name of the output
 * file for error messages.
 */
static
void
make_table_of_contents(
struct arch *arch,
char *output)
{
    unsigned long i, j, k, r, s, nsects;
    struct member *member;
    struct load_command *lc;
    struct segment_command *sg;
    struct nlist *symbols;
    char *strings;
    enum bool sorted;
    char *ar_name;
    struct section *section;

	/*
	 * First pass over the members to count how many ranlib structs are
	 * needed and the size of the strings in the toc that are needed.
	 */
	for(i = 0; i < arch->nmembers; i++){
	    member = arch->members + i;
	    if(member->mh != NULL){
		nsects = 0;
		lc = member->load_commands;
		for(j = 0; j < member->mh->ncmds; j++){
		    if(lc->cmd == LC_SYMTAB){
			if(member->st == NULL)
			    member->st = (struct symtab_command *)lc;
		    }
		    else if(lc->cmd == LC_SEGMENT){
			sg = (struct segment_command *)lc;
			nsects += sg->nsects;
		    }
		    lc = (struct load_command *)((char *)lc + lc->cmdsize);
		}
		member->sections = allocate(nsects * sizeof(struct section *));
		nsects = 0;
		lc = member->load_commands;
		for(j = 0; j < member->mh->ncmds; j++){
		    if(lc->cmd == LC_SEGMENT){
			sg = (struct segment_command *)lc;
			section = (struct section *)
				((char *)sg + sizeof(struct segment_command));
			for(k = 0; k < sg->nsects; k++){
			    member->sections[nsects++] = section++;
			}
		    }
		    lc = (struct load_command *)((char *)lc + lc->cmdsize);
		}
		if(member->st != NULL && member->st->nsyms != 0){
		    symbols = (struct nlist *)(member->object_addr +
					       member->st->symoff);
		    if(member->object_byte_sex != get_host_byte_sex())
			swap_nlist(symbols, member->st->nsyms,
				   get_host_byte_sex());
		    strings = member->object_addr + member->st->stroff;
		    for(j = 0; j < member->st->nsyms; j++){
			if(symbols[j].n_un.n_strx > member->st->strsize){
			    warn_member(arch, member, "malformed object (symbol"
					" %lu n_strx field extends past the "
					"end of the string table)", j);
			    continue;
			}
			if(toc_symbol(symbols + j, member->sections) == TRUE){
			    arch->toc_nranlibs++;
			    arch->toc_strsize += strlen(strings +
						    symbols[j].n_un.n_strx) + 1;
			}
		    }
		}
		else
		    warn_member(arch, member, "has no symbols");
	    }
	    else{
		if(cmd_flags.ranlib == FALSE){
		    warn_member(arch, member, "is not an object file");
		    errors++;
		}
	    }
	}

	/*
	 * Allocate the space for the ranlib structs and strings for the
	 * table of contents.
	 */
	arch->toc_ranlibs = allocate(sizeof(struct ranlib) *arch->toc_nranlibs);
	arch->toc_strsize = round(arch->toc_strsize, sizeof(long));
	arch->toc_strings = allocate(arch->toc_strsize);

	/*
	 * Second pass over the members to fill in the ranlib structs and
	 * the strings for the table of contents.  The ran_name field is
	 * filled in with a pointer to a string contained in arch->toc_strings
	 * for easy sorting and conversion to an index.  The ran_off field is
	 * filled in with the member index plus one to allow marking with it's
	 * negative value by check_sort_ranlibs() and easy conversion to the
	 * real offset.
	 */
	r = 0;
	s = 0;
	for(i = 0; i < arch->nmembers; i++){
	    member = arch->members + i;
	    if(member->mh != NULL){
		if(member->st != NULL && member->st->nsyms != 0){
		    symbols = (struct nlist *)(member->object_addr +
					       member->st->symoff);
		    strings = member->object_addr + member->st->stroff;
		    for(j = 0; j < member->st->nsyms; j++){
			if(symbols[j].n_un.n_strx > member->st->strsize)
			    continue;
			if(toc_symbol(symbols + j, member->sections) == TRUE){
			    strcpy(arch->toc_strings + s, 
				   strings + symbols[j].n_un.n_strx);
			    arch->toc_ranlibs[r].ran_un.ran_name =
							arch->toc_strings + s;
			    arch->toc_ranlibs[r].ran_off = i + 1;
			    r++;
			    s += strlen(strings + symbols[j].n_un.n_strx) + 1;
			}
		    }
		    if(member->object_byte_sex != get_host_byte_sex())
			swap_nlist(symbols, member->st->nsyms,
				   get_host_byte_sex());
		}
	    }
	}

	/*
	 * If the table of contents is to be sorted by symbol name then try to
	 * sort it and leave it sorted if no duplicates.
	 */
	if(cmd_flags.s == TRUE){
	    qsort(arch->toc_ranlibs, arch->toc_nranlibs, sizeof(struct ranlib),
		  (int (*)(const void *, const void *))ranlib_name_qsort);
	    sorted = check_sort_ranlibs(arch, output);
	    if(sorted == FALSE){
		qsort(arch->toc_ranlibs, arch->toc_nranlibs,
		      sizeof(struct ranlib),
		      (int (*)(const void *, const void *))ranlib_offset_qsort);
		arch->toc_name = SYMDEF;
		arch->toc_name_size = sizeof(SYMDEF) - 1;
		arch->toc_long_name = FALSE;
		ar_name = arch->toc_name;
	    }
	    else{
		/*
		 * Since the SYMDEF_SORTED is "__.SYMDEF SORTED" which contains
		 * a space, it should use extended format #1 if we can use long
		 * names.  This code assumes SYMDEF_SORTED contains a space and
		 * the size (16) is a multiple of sizeof(unsigned long).
		 */
		arch->toc_name = SYMDEF_SORTED;
		arch->toc_name_size = sizeof(SYMDEF_SORTED) - 1;
		if(cmd_flags.use_long_names == TRUE){
		    arch->toc_long_name = TRUE;
		    ar_name = AR_EFMT1 "16";
		}
		else{
		    arch->toc_long_name = FALSE;
		    ar_name = arch->toc_name;
		}
	    }
	}
	else{
	    sorted = FALSE;
	    arch->toc_name = SYMDEF;
	    arch->toc_name_size = sizeof(SYMDEF) - 1;
	    arch->toc_long_name = FALSE;
	    ar_name = arch->toc_name;
	}

	/*
	 * Now set the ran_off and ran_un.ran_strx fields of the ranlib structs.
	 * To do this the size of the toc member must be know because it comes
	 * first in the library.  The size of the toc member is made up of the
	 * sizeof an archive header struct (the size of the name if a long name
	 * is used) then the toc which is (as defined in ranlib.h):
	 *	a long for the number of bytes of the ranlib structs
	 *	the ranlib structures
	 *	a long for the number of bytes of the strings
	 *	the strings
	 */
	arch->toc_size = sizeof(struct ar_hdr) +
			 sizeof(long) +
			 arch->toc_nranlibs * sizeof(struct ranlib) +
			 sizeof(long) +
			 arch->toc_strsize;
	/* add the size of the name is a long name is used */
	if(arch->toc_long_name == TRUE)
	    arch->toc_size += arch->toc_name_size;
	for(i = 0; i < arch->nmembers; i++)
	    arch->members[i].offset += SARMAG + arch->toc_size;
	for(i = 0; i < arch->toc_nranlibs; i++){
	    arch->toc_ranlibs[i].ran_un.ran_strx = 
		arch->toc_ranlibs[i].ran_un.ran_name - arch->toc_strings;
	    arch->toc_ranlibs[i].ran_off = 
		arch->members[arch->toc_ranlibs[i].ran_off - 1].offset;
	}

	sprintf((char *)(&arch->toc_ar_hdr), "%-*s%-*ld%-*u%-*u%-*o%-*ld",
	   (int)sizeof(arch->toc_ar_hdr.ar_name),
	       ar_name,
	   (int)sizeof(arch->toc_ar_hdr.ar_date),
	       toc_time,
	   (int)sizeof(arch->toc_ar_hdr.ar_uid),
	       (unsigned short)getuid(),
	   (int)sizeof(arch->toc_ar_hdr.ar_gid),
	       (unsigned short)getgid(),
	   (int)sizeof(arch->toc_ar_hdr.ar_mode),
	       (unsigned int)toc_mode,
	   (int)sizeof(arch->toc_ar_hdr.ar_size),
	       (long)(arch->toc_size - sizeof(struct ar_hdr)));
	/*
	 * This has to be done by hand because sprintf puts a null
	 * at the end of the buffer.
	 */
	memcpy(arch->toc_ar_hdr.ar_fmag, ARFMAG,
	       (int)sizeof(arch->toc_ar_hdr.ar_fmag));
}

/*
 * Function for qsort() for comparing ranlib structures by name.
 */
static
int
ranlib_name_qsort(
const struct ranlib *ran1,
const struct ranlib *ran2)
{
	return(strcmp(ran1->ran_un.ran_name, ran2->ran_un.ran_name));
}

/*
 * Function for qsort() for comparing ranlib structures by offset.
 */
static
int
ranlib_offset_qsort(
const struct ranlib *ran1,
const struct ranlib *ran2)
{
	if(ran1->ran_off < ran2->ran_off)
	    return(-1);
	if(ran1->ran_off > ran2->ran_off)
	    return(1);
	/* ran1->ran_off == ran2->ran_off */
	    return(0);
}

/*
 * toc_symbol() returns TRUE if the symbol is to be included in the table of
 * contents otherwise it returns FALSE.
 */
static
enum bool
toc_symbol(
struct nlist *symbol,
struct section **sections)
{
	/* if the name is NULL then it won't be in the table of contents */
	if(symbol->n_un.n_strx == 0)
	    return(FALSE);
	/* if symbol is not external then it won't be in the toc */
	if((symbol->n_type & N_EXT) == 0)
	    return(FALSE);
	/* if symbol is undefined then it won't be in the toc */
	if((symbol->n_type & N_TYPE) == N_UNDF && symbol->n_value == 0)
	    return(FALSE);
	/* if symbol is common and the -c flag is not specified then ... */
	if((symbol->n_type & N_TYPE) == N_UNDF && symbol->n_value != 0 &&
	   cmd_flags.c == FALSE)
	    return(FALSE);
	/* if the symbols is in a section marked NO_TOC then ... */
	if((symbol->n_type & N_TYPE) == N_SECT &&
	   (sections[symbol->n_sect - 1]->flags & S_ATTR_NO_TOC) != 0)
	    return(FALSE);

	return(TRUE);
}

/*
 * check_sort_ranlibs() checks the table of contents for the specified arch
 * which is sorted by name for more then one object defining the same symbol.
 * It this is the case it prints each symbol that is defined in more than one
 * object along with the object it is defined in.  It returns TRUE if there are
 * no multiple definitions and FALSE otherwise.
 */
static
enum bool
check_sort_ranlibs(
struct arch *arch,
char *output)
{
    long i;
    enum bool multiple_defs;
    struct member *member;

	/*
	 * Since the symbol table is sorted by name look to any two adjcent
	 * entries with the same name.  If such entries are found print them
	 * only once (marked by changing the sign of their ran_off).
	 */
	multiple_defs = FALSE;
	for(i = 0; i < (long)arch->toc_nranlibs - 1; i++){
	    if(strcmp(arch->toc_ranlibs[i].ran_un.ran_name,
		      arch->toc_ranlibs[i+1].ran_un.ran_name) == 0){
		if(multiple_defs == FALSE){
		    fprintf(stderr, "%s: same symbol defined in more than one "
			    "member ", progname);
		    if(narchs > 1)
			fprintf(stderr, "for architecture: %s ",
				arch->arch_flag.name);
		    fprintf(stderr, "in: %s (table of contents will not be "
			    "sorted)\n", output);
		    multiple_defs = TRUE;
		}
		if((int)(arch->toc_ranlibs[i].ran_off) > 0){
		    member = arch->members + arch->toc_ranlibs[i].ran_off - 1;
		    warn_member(arch, member, "defines symbol: %s",
				arch->toc_ranlibs[i].ran_un.ran_name);
		    arch->toc_ranlibs[i].ran_off =
				-(arch->toc_ranlibs[i].ran_off);
		}
		if((int)(arch->toc_ranlibs[i+1].ran_off) > 0){
		    member = arch->members + arch->toc_ranlibs[i+1].ran_off - 1;
		    warn_member(arch, member, "defines symbol: %s",
				arch->toc_ranlibs[i+1].ran_un.ran_name);
		    arch->toc_ranlibs[i+1].ran_off =
				-(arch->toc_ranlibs[i+1].ran_off);
		}
	    }
	}

	if(multiple_defs == FALSE)
	    return(TRUE);
	else{
	    for(i = 0; i < arch->toc_nranlibs; i++)
		if(((int)arch->toc_ranlibs[i].ran_off) < 0)
		    arch->toc_ranlibs[i].ran_off =
			-(arch->toc_ranlibs[i].ran_off);
	    return(FALSE);
	}
}

/*
 * warn_duplicate_member_names() generates a warning if two members end up with
 * the same ar_name.  This is only a warning because ld(1) and this program
 * has no problems with it.  Only if ar(1) were used to extract the files
 * would this be a problem (even the 4.4bsd ar(1) using long names can
 * get hosed by base names,  the 4.3bsd ar(1) can't handle full 16 character
 * ar_names).
 */
static
void
warn_duplicate_member_names(
void)
{
    unsigned long i, j, len, len1, len2;

	for(i = 0; i < narchs; i++){
	    /* sort in order of ar_names */
	    qsort(archs[i].members, archs[i].nmembers, sizeof(struct member),
		  (int (*)(const void *, const void *))member_name_qsort);

	    /* check for duplicate names */
	    for(j = 0; j < archs[i].nmembers - 1; j++){
		len1 = archs[i].members[j].member_name_size;
		len2 = archs[i].members[j+1].member_name_size;
		len = len1 > len2 ? len1 : len2;
		if(strncmp(archs[i].members[j].member_name,
			   archs[i].members[j+1].member_name,
			   len) == 0){
		    fprintf(stderr, "%s: warning ", progname);
		    if(narchs > 1)
			fprintf(stderr, "for architecture: %s ",
				archs[i].arch_flag.name);
		    fprintf(stderr, "same member name (%.*s) in output file "
			    "used for input files: ", (int)len1,
			    archs[i].members[j].member_name);

		    if(archs[i].members[j].input_ar_hdr != NULL){
			len = archs[i].members[j].input_base_name_size;
			fprintf(stderr, "%s(%.*s) and: ",
				archs[i].members[j].input_file_name, (int)len,
				archs[i].members[j].input_base_name);
		    }
		    else
			fprintf(stderr, "%s and: ",
				archs[i].members[j].input_file_name);

		    if(archs[i].members[j+1].input_ar_hdr != NULL){
			len = archs[i].members[j+1].input_base_name_size;
			fprintf(stderr, "%s(%.*s) due to use of basename, "
				"truncation and blank padding\n",
				archs[i].members[j+1].input_file_name, (int)len,
				archs[i].members[j+1].input_base_name);
		    }
		    else
			fprintf(stderr, "%s (due to use of basename, truncation"
				", blank padding or duplicate input files)\n",
				archs[i].members[j+1].input_file_name);
		}
	    }

	    /* sort back in order of offset */
	    qsort(archs[i].members, archs[i].nmembers, sizeof(struct member),
		  (int (*)(const void *, const void *))member_offset_qsort);
	}
}

/*
 * Function for qsort() for comparing member structures by ar_hdr.ar_name.
 */
static
int
member_name_qsort(
const struct member *member1,
const struct member *member2)
{
    unsigned long len, len1, len2;

	len1 = member1->member_name_size;
	len2 = member2->member_name_size;
	len = len1 > len2 ? len1 : len2;
	return(strncmp(member1->member_name, member2->member_name, len));
}

/*
 * Function for qsort() for comparing member structures by offset.
 */
static
int
member_offset_qsort(
const struct member *member1,
const struct member *member2)
{
	if(member1->offset < member2->offset)
	    return(-1);
	if(member1->offset > member2->offset)
	    return(1);
	/* member1->offset == member2->offset */
	    return(0);
}

/*
 * warn_member() is like the error routines it prints the program name the
 * member name specified and message specified.
 */
static
void
warn_member(
struct arch *arch,
struct member *member,
const char *format, ...)
{
    va_list ap;

	fprintf(stderr, "%s: ", progname);
	if(narchs > 1)
	    fprintf(stderr, "for architecture: %s ", arch->arch_flag.name);

	if(member->input_ar_hdr != NULL){
	    fprintf(stderr, "file: %s(%.*s) ", member->input_file_name,
		    (int)member->input_base_name_size, member->input_base_name);
	}
	else
	    fprintf(stderr, "file: %s ", member->input_file_name);

	va_start(ap, format);
	vfprintf(stderr, format, ap);
        fprintf(stderr, "\n");
	va_end(ap);
}
