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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <mach-o/reloc.h>
#include <libc.h>
#include <mach/mach.h>
#include "stuff/arch.h"
#include "stuff/errors.h"
#include "stuff/allocate.h"
#include "stuff/hash_string.h"

#include "libgcc.h"
#include "mkshlib.h"
#include "branch.h"

char *progname;		/* name of the program for error messages (argv[0]) */

char *spec_filename;	/* file name of the specification input file */
char *host_filename;	/* file name of the host shared library output file */
char *target_filename;	/* file name of the target shared library output file */
char *shlib_symbol_name;/* shared library definision symbol name */
char *shlib_reference_name; /* shared library full reference symbol name */
char *target_base_name;	/* base name of target shared library */
long vflag = 0;		/* the verbose flag (-v) */
long dflag = 0;		/* the debug flag (-d) */
static long fflag = 0;	/* don't create the host library */

/*
 * The architecture of the output file as specified by -arch and the cputype
 * and cpusubtype of the object files being loaded which will be the output
 * cputype and cpusubtype.
 */
struct arch_flag arch_flag = { 0 };
/* the byte sex of the machine this program is running on */
enum byte_sex host_byte_sex = UNKNOWN_BYTE_SEX;
/* the byte sex of the output files */
enum byte_sex target_byte_sex = UNKNOWN_BYTE_SEX;

/*
 * Array to hold ld(1) flags.
 */
char **ldflags = NULL;
unsigned long nldflags = 0;

#define	MKSHLIBNAME	"mkshlib"	/* base name of this program */
#define	ASNAME		"as"		/* base name of the assembler */
#define	LDNAME		"ld"		/* base name of the link editor */

char *asname;		/* name of the assember being used */
char *ldname;		/* name of the link editior being used */

static void usage(void);
static void libdef_sym(void);

/*
 * runlist is used by the routine run() to execute a program and it contains
 * the command line arguments.  Strings are added to it by add_runlist().
 * The routine reset_runlist() resets it for new use.
 */
static struct {
    int size;
    int next;
    char **strings;
} runlist;

int
main(
int argc,
char *argv[],
char *envp[])
{
    long i;

	progname = argv[0];
	signal(SIGINT, cleanup);
	host_byte_sex = get_host_byte_sex();

	asname = ASNAME;
	ldname = LDNAME;

	for(i = 1; i < argc; i++){
	    if(argv[i][0] != '-'){
		error("unknown flag %s", argv[i]);
		usage();
	    }
	    switch(argv[i][1]){
		case 'a':
		    if(strcmp(argv[i], "-arch") == 0){
			if(++i >= argc)
			    fatal("-arch: argument missing");
			if(arch_flag.name != NULL)
			    fatal("-arch: multiply specified");
			if(get_arch_from_flag(argv[i], &arch_flag) == 0){
			    error("unknown architecture specification flag: "
				  "-arch %s", argv[i]);
			    arch_usage();
			    usage();
			}
		    }
		    else
			goto unknown_flag;
		    break;

		case 's':
		    if(strcmp(argv[i], "-sectcreate") == 0 ||
		       strcmp(argv[i], "-segcreate") == 0 ||
		       strcmp(argv[i], "-sectorder") == 0 ||
		       strcmp(argv[i], "-sectalign") == 0 ||
		       strcmp(argv[i], "-segprot") == 0){
			if(i + 3 >= argc){
			    error("not enough arguments follow %s", argv[i]);
			    usage();
			}
			ldflags = reallocate(ldflags, sizeof(char * ) *
						  (nldflags + 4));
			ldflags[nldflags++] = argv[i];
			ldflags[nldflags++] = argv[i + 1];
			ldflags[nldflags++] = argv[i + 2];
			ldflags[nldflags++] = argv[i + 3];
			i += 3;
			break;
		    }
		    if(strcmp(argv[i], "-segaddr") == 0){
			if(i + 2 >= argc){
			    error("not enough arguments follow %s", argv[i]);
			    usage();
			}
			ldflags = reallocate(ldflags, sizeof(char * ) *
						  (nldflags + 3));
			ldflags[nldflags++] = argv[i];
			ldflags[nldflags++] = argv[i + 1];
			ldflags[nldflags++] = argv[i + 2];
			i += 2;
			break;
		    }
		    if(strcmp(argv[i], "-segalign") == 0){
			if(i + 1 >= argc){
			    error("not enough arguments follow %s", argv[i]);
			    usage();
			}
			ldflags = reallocate(ldflags, sizeof(char * ) *
						  (nldflags + 2));
			ldflags[nldflags++] = argv[i];
			ldflags[nldflags++] = argv[i + 1];
			i += 1;
			break;
		    }
		    if(strcmp(argv[i], "-seglinkedit") == 0 ||
		       strcmp(argv[i], "-sectorder_detail") == 0){
			ldflags = reallocate(ldflags, sizeof(char * ) *
						  (nldflags + 1));
			ldflags[nldflags++] = argv[i];
			break;
		    }
		    if(i + 1 == argc){
			error("missing file name for %s", argv[i]);
			usage();
		    }
		    i++;
		    spec_filename = argv[i];
		    break;
		case 'h':
		    if(i + 1 == argc){
			error("missing file name for %s", argv[i]);
			usage();
		    }
		    i++;
		    host_filename = argv[i];
		    break;
		case 't':
		    if(i + 1 == argc){
			error("missing file name for %s", argv[i]);
			usage();
		    }
		    i++;
		    target_filename = argv[i];
		    break;
		case 'f':
		    fflag++;
		    break;
		case 'u':
		    fatal("-u flag obsolete, use #undefined in spec file to "
			  "list expected undefined symbol names");
		case 'm':
		    if(strcmp(argv[i], "-minor_version") == 0){
			if(i + 1 == argc){
			    error("argument for %s missing", argv[i]);
			    usage();
			}
			i++;
	    		if(minor_version != 0)
			    fatal("minor_version already specified");
			minor_version = atol(argv[i]);
			if(minor_version <= 0)
			    fatal("minor_version value: %ld must be "
				  "greater than 0", minor_version);
			break;
		    }
		    goto unknown_flag;
		case 'i':
		    if(strcmp(argv[i], "-image_version") == 0){
			if(i + 1 == argc){
			    error("argument for %s missing", argv[i]);
			    usage();
			}
			i++;
	    		if(image_version != 0)
			    fatal("image_version already specified");
			image_version = atol(argv[i]);
			if(image_version <= 0)
			    fatal("image_version value: %ld must be "
				  "greater than 0", image_version);
			break;
		    }
		    goto unknown_flag;
		case 'v':
		    vflag++;
		    break;
		case 'd':
		    dflag++;
		    break;
		default:
		    if(strcmp(argv[i], "-u") == 0 ||
		       strcmp(argv[i], "-U") == 0){
			if(i + 1 >= argc){
			    error("not enough arguments follow %s", argv[i]);
			    usage();
			}
			ldflags = reallocate(ldflags, sizeof(char * ) *
						  (nldflags + 2));
			ldflags[nldflags++] = argv[i];
			ldflags[nldflags++] = argv[i + 1];
			i += 1;
			break;
		    }
		    if(strncmp(argv[i], "-y", 2) == 0 ||
		       strcmp(argv[i], "-M") == 0 ||
		       strcmp(argv[i], "-noseglinkedit") == 0){
			ldflags = reallocate(ldflags, sizeof(char * ) *
						  (nldflags + 1));
			ldflags[nldflags++] = argv[i];
			break;
		    }
unknown_flag:
		    error("unknown flag %s", argv[i]);
		    usage();
	    }
	}
	if(spec_filename == (char *)0){
	    error("no library specification file specified (use -s filename)");
	    usage();
	}
	if(!fflag && host_filename == (char *)0){
	    error("no host shared library file name specified (use -h "
		  "filename)");
	    usage();
	}
	if(target_filename == (char *)0){
	    error("no target shared library file name specified (use -t "
		  "filename)");
	    usage();
	}

	/*
	 * Parse the spec file to know what to do.
	 */
	parse_spec();

	/*
	 * Make the shared library definition symbol.
	 */
	libdef_sym();

	/*
	 * Build the target shared library.
	 */
	target();

	/*
	 * Build the host shared library.
	 */
	if(!fflag)
	    host();

	/*
	 * Treat all errors as fatal and remove the created files.
	 */
	if(errors)
	    cleanup(0);

	return(EXIT_SUCCESS);
}

/*
 * Print the current usage message and exit.
 */
static
void
usage(void)
{
	fprintf(stderr, "Usage: %s -s specfile -h hostfile -t targetfile "
		"[options] ...\n", progname);
	exit(EXIT_FAILURE);
}

/*
 * Build a library definition symbol from the name target shared library
 * will be executed as.
 */
static
void
libdef_sym(void)
{
    char *p;

	target_base_name = strrchr(target_name, '/');
	if(target_base_name != (char *)0)
	    target_base_name++;
	else
	    target_base_name = target_name;
	shlib_symbol_name = makestr(SHLIB_SYMBOL_NAME, target_base_name,
				    (char *)0);
	/*
	 * The shared library full reference symbol is the target base name
	 * up to (but not including) the first '.' .  This is so the name for
	 * "/usr/shlib/libsys_s.A.shlib" will be "libsys_s" etc.
	 */
	shlib_reference_name = makestr(target_base_name, (char *)0);
	p = strchr(shlib_reference_name, '.');
	if(p != NULL)
	    *p = '\0';
}


/*
 * This routine cleans up the files that were created temporary files.
 */
void
cleanup(
int sig)
{
	if(!dflag){
	    unlink(branch_source_filename);
	    unlink(branch_object_filename);
	    unlink(lib_id_object_filename);
	    unlink(target_filename);
	    unlink(host_filename);
	}
	exit(EXIT_FAILURE);
}

/*
 * This routine returns the symbol name used for the branch table slot number
 * passed to it.  It is used to create the branch table and later to look it
 * up in target.  The name created is out of the name space of high level
 * languages but not out of the assembler's name space.
 */
char *
branch_slot_symbol(
long i)
{
    static char buf[40];

	sprintf(buf, "%s%ld", BRANCH_SLOT_NAME, i);
	return(buf);
}

/*
 * This routine returns the non-zero if the symbol name is used for the branch
 * table slot.
 */
long
is_branch_slot_symbol(
char *p)
{
	return(strncmp(p, BRANCH_SLOT_NAME, sizeof(BRANCH_SLOT_NAME) - 1) == 0);
}

/*
 * This routine returns the non-zero if the symbol name is used for the library
 * definition symbol.
 */
long
is_libdef_symbol(
char *p)
{
	return(strncmp(p, SHLIB_SYMBOL_NAME, sizeof(SHLIB_SYMBOL_NAME)-1) == 0);
}

/*
 * run() does an exec using the runlist that has been created.
 * A zero return value indicates success.
 */
long
run(void)
{
    char *cmd, **ex, **p;
    int forkpid, waitpid, termsig;
#ifndef __OPENSTEP__
    int waitstatus;
#else
    union wait waitstatus;
#endif

	ex = runlist.strings;
	cmd = runlist.strings[0];

	if(vflag){
	    fprintf(stderr, "+ %s ", cmd);
	    p = &(ex[1]);
	    while(*p != (char *)0)
		    fprintf(stderr, "%s ", *p++);
	    fprintf(stderr, "\n");
	}

	forkpid = fork();
	if(forkpid == -1)
	    system_fatal("can't fork a new process to run: %s", cmd);

	if(forkpid == 0){
	    if(execvp(cmd, ex) == -1)
		system_fatal("can't find or exec: %s", cmd);
	}
	else{
	    waitpid = wait(&waitstatus);
	    if(waitpid == -1)
		system_fatal("wait on forked process %d failed", forkpid);
#ifndef __OPENSTEP__
	    termsig = WTERMSIG(waitstatus);
#else
	    termsig = waitstatus.w_termsig;
#endif
	    if(termsig != 0 && termsig != SIGINT)
		fatal("fatal error in %s", cmd);
	    return(
#ifndef __OPENSTEP__
		WEXITSTATUS(waitstatus) != 0 ||
#else
		waitstatus.w_retcode != 0 ||
#endif
		termsig != 0);
	}
	return(0);
}

/*
 * This routine is passed a string to be added to the list of strings as the
 * command line arguments of the next command to be run.
 */
void
add_runlist(
char *str)
{
	if(runlist.strings == (char **)0){
	    runlist.next = 0;
	    runlist.size = 128;
	    runlist.strings = allocate(runlist.size * sizeof(char **));
	}
	if(runlist.next + 1 >= runlist.size){
	    runlist.strings = reallocate(runlist.strings,
				(runlist.size * 2) * sizeof(char **));
	    runlist.size *= 2;
	}
	runlist.strings[runlist.next++] = str;
	runlist.strings[runlist.next] = (char *)0;
}

/*
 * This routine reset the list of strings of command line arguments so that
 * an new command line argument list can be built.
 */
void
reset_runlist(void)
{
	runlist.next = 0;
}
