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
#import <libc.h>
#import <mach-o/loader.h>
#ifndef __OPENSTEP__
extern const struct section *getsectbynamefromheader(
	const struct mach_header *mhp,
	const char *segname,
	const char *sectname);
#endif
#import <stdio.h>
#import <string.h>
#import <mach/mach.h>
#import "stuff/openstep_mach.h"
#import <mach-o/ldsyms.h>
#import <errno.h>

#ifndef __MACH30__
#import "../profileServer/profileServer.h"
#endif /* __MACH30__ */

#import "inline_strcmp.h"
#import "dyld_init.h"
#import "images.h"
#import "symbols.h"
#import "errors.h"
#import "reloc.h"
#import "lock.h"
#import "debug.h"
#import "trace.h"

/*
 * This is set in _dyld_init() and used for the cputype and cpusubtype.
 */
struct host_basic_info host_basic_info = { 0 };
#if (defined(__GONZO_BUNSEN_BEAKER__) || defined(__HERA__)) && defined(__ppc__)
/* set if this is a g4 processor */
enum bool processor_has_vec = FALSE;
#endif

/*
 * If the executable has the MH_BINDATLOAD flag set then executable_bind_at_load
 * is set to TRUE and both non-lazy and lazy undefined references are placed on
 * the undefined list as all modules get bound.  This will also be set to TRUE
 * if the dyld environment variable DYLD_BIND_AT_LAUNCH is set.  If the
 * environment variable is set then dyld_bind_at_launch gets set to TRUE.
 */
enum bool executable_bind_at_load = FALSE;
static enum bool dyld_bind_at_launch = FALSE;

/*
 * If the executable has the MH_FORCE_FLAT flag set then force_flat_namespace i
 * set to TRUE and the program and all its images are bound with a flat
 * namespace.  This will also be set to TRUE if the dyld environment variable
 * DYLD_FORCE_FLAT_NAMESPACE is set.  If the environment variable is set then
 * dyld_force_flat_namespace gets set to TRUE.
 */
enum bool force_flat_namespace = FALSE;
static enum bool dyld_force_flat_namespace = FALSE;

/*
 * The string home points at the enviroment variable HOME.  It is use in setting
 * the default fallback search paths if needed.
 */
char *home = "/";

/*
 * The string dyld_framework_path points to the framework search path and comes
 * from the enviroment variable DYLD_FRAMEWORK_PATH.  It should look like
 * "path1:path2:path3".  The string dyld_fallback_framework_path points to the
 * fallback framework search path and comes from the enviroment variable
 * DYLD_FALLBACK_FRAMEWORK_PATH.  The default_fallback_framework_path will get
 * set in load_library_image() in images.c if it is needed.
 */
char *dyld_framework_path = NULL;
char *dyld_fallback_framework_path = NULL;
char *default_fallback_framework_path = NULL;

/*
 * The string dyld_library_path points to the dynamic shared library search
 * path and comes from the enviroment variable DYLD_LIBRARY_PATH.  It should
 * look like "path1:path2:path3". The string dyld_fallback_library_path points
 * to the dynamic shared library fallback search path and comes from the
 * enviroment variable DYLD_FALLBACK_LIBRARY_PATH.  The
 * default_fallback_library_path will get set in load_library_image() in
 * images.c if it is needed.
 */
char *dyld_library_path = NULL;
char *dyld_fallback_library_path = NULL;
char *default_fallback_library_path = NULL;

/*
 * The string dyld_image_suffix points to the dynamic shared library
 * suffix and comes from the enviroment variable DYLD_IMAGE_SUFFIX.
 */
char *dyld_image_suffix = NULL;

/*
 * The string dyld_insert_libraries points to a list of dynamic shared libraries
 * and comes from the enviroment variable DYLD_INSERT_LIBRARIES.  It should
 * look like "lib1:lib2:lib3".
 */
char *dyld_insert_libraries = NULL;

enum bool dyld_print_libraries = FALSE;
enum bool dyld_trace = FALSE;
enum bool dyld_mem_protect = FALSE;
enum bool dyld_ebadexec_only = FALSE;
enum bool dyld_dead_lock_hang = FALSE;
unsigned long dyld_prebind_debug = 0;
unsigned long dyld_hints_debug = 0;
unsigned long dyld_sample_debug = 0;
enum bool dyld_executable_path_debug = FALSE;
enum bool dyld_two_level_debug = FALSE;
enum bool dyld_abort_multiple_inits = FALSE;
enum bool dyld_new_local_shared_regions = FALSE;
enum bool dyld_no_fix_prebinding = FALSE;

/*
 * This indicates if the profile server for shared pcsample buffers exists.
 */
enum bool profile_server = FALSE;

/*
 * This indicates if the launching of this program is being done using the
 * prebound state.  This is assumed to be the case and is disabled if the
 * assumptions to do this are not true at launch time.  The prebound state of
 * images is only used when the program is first launched.  After the program
 * is launched the variable launched is set to TRUE and all prebound libraries
 * loaded then have the prebinding undone.
 */
enum bool prebinding = TRUE;
enum bool launched = FALSE;
enum bool executable_prebound = FALSE;

static struct segment_command *data_seg;
static void get_data_segment(
    void);

/*
 * The variable executables_path is an absolute path of the executable being
 * run.  This is used to support shared library install paths that are relative
 * to the executable.  These install paths by convention start with
 * "@executable_path" and that is replaced by the real absolute path of the
 * executable.  To get an absolute path of the executable the exec interface to
 * the entry point code was changed to pass the first argument to the exec()
 * system call (this can be but is not always the same as argv[0]).  The exec
 * path is picked up in pickup_environment_variables() as the pointer to it
 * passed by the kernel which follows the terminating 0 of the envp[] array.
 * The variable exec_path is set to this.  The current working directory is
 * may also be needed along with the exec_path to construct an absolute path of
 * the executable into the variable executables_path, see the routine
 * create_executables_path().
 */
char *executables_path = NULL;
unsigned long executables_pathlen = 0;
char *exec_path = NULL;

static void pickup_environment_variables(
    char *envp[]);

/*
 * _dyld_init() is the start off point for the dynamic link editor.  It is
 * called before any part of an executable program runs.  This is done either
 * in the executable runtime startoff or by the kernel as a result of an exec(2)
 * system call (which goes through __dyld_start to get here).
 *
 * This routine causes the dynamic shared libraries an executable uses to be
 * mapped, sets up the executable and the libraries to call the dynamic link
 * editor when a lazy reference to a symbol is first used, resolves all non-lazy
 * symbol references needed to start running the program and then returns to
 * the executable program to start up the program.
 */
unsigned long
_dyld_init(
struct mach_header *mh,
unsigned long argc,
char **argv,
char **envp)
{
    unsigned int count;
    kern_return_t r;
    unsigned long entry_point;
    mach_port_t my_mach_host_self;
#ifndef __MACH30__
    struct section *s;
#endif
#ifdef MALLOC_DEBUG
    extern void cthread_init(void);

	cthread_init();
#endif
	
	/* set lock for dyld data structures */
	set_lock();

	/*
	 * Get the cputype and cpusubtype of the machine we're running on.
	 */
	count = HOST_BASIC_INFO_COUNT;
	my_mach_host_self = mach_host_self();
	if((r = host_info(my_mach_host_self, HOST_BASIC_INFO, (host_info_t)
			  (&host_basic_info), &count)) != KERN_SUCCESS){
	    mach_port_deallocate(mach_task_self(), my_mach_host_self);
	    mach_error(r, "can't get host basic info");
	}
	mach_port_deallocate(mach_task_self(), my_mach_host_self);
#if defined(__GONZO_BUNSEN_BEAKER__) && defined(__ppc__)
	if(host_basic_info.cpu_type == CPU_TYPE_POWERPC &&
	   (host_basic_info.cpu_subtype == CPU_SUBTYPE_POWERPC_7400 ||
	    host_basic_info.cpu_subtype == CPU_SUBTYPE_POWERPC_7450))
	    processor_has_vec = TRUE;
#endif

	/*
	 * Pickup the environment variables for the dynamic link editor.
	 */
	pickup_environment_variables(envp);
	
	/*
	 * Make initial trace entry if requested.
	 */
	DYLD_TRACE_INIT_START(0);

	/*
	 * Create the executable's path from the exec_path and the current
	 * working directory (if needed).  If we did not pick up the exec_path
	 * (we are running with an old kernel) use argv[0] if has a slash in it
	 * as it is a path relative to the current working directory.  Of course
	 * argv[0] may not have anything to do with the filename being executed
	 * in all cases but it is likely to be right.
	 */
	if(exec_path != NULL)
	    create_executables_path(exec_path);
	else if(strchr(argv[0], '/') != NULL)
	    create_executables_path(argv[0]);
	if(dyld_executable_path_debug == TRUE)
	    printf("executables_path = %s\n",
		   executables_path == NULL ? "NULL" : executables_path);

#ifdef DYLD_PROFILING
	s = (struct section *) getsectbynamefromheader(
	    &_mh_dylinker_header, SEG_TEXT, SECT_TEXT);
	monstartup((char *)(s->addr + dyld_image_vmaddr_slide),
		   (char *)(s->addr + dyld_image_vmaddr_slide + s->size));
#endif

#ifndef __MACH30__
	/*
	 * See if the profile server for shared pcsample buffers exists.
	 * Then if so try to setup a pcsample buffer for dyld itself.
	 */
	profile_server = profile_server_exists();
	if(profile_server == TRUE){
	    s = (struct section *) getsectbynamefromheader(
		&_mh_dylinker_header, SEG_TEXT, SECT_TEXT);
	    shared_pcsample_buffer("/usr/lib/dyld", s, dyld_image_vmaddr_slide);
	}
#endif /* __MACH30__ */

	/*
	 * Start off by loading the executable image as the first object image
	 * that make up the program.  This in turn will load the dynamic shared
	 * libraries the executable uses and the libraries those libraries use
	 * to the list of library images that make up the program.
	 */
	if((mh->flags & MH_FORCE_FLAT) != 0 ||
	   dyld_force_flat_namespace == TRUE)
	    force_flat_namespace = TRUE;
	if((mh->flags & MH_NOFIXPREBINDING) == MH_NOFIXPREBINDING)
	    dyld_no_fix_prebinding = TRUE;
	executable_prebound = (mh->flags & MH_PREBOUND) == MH_PREBOUND;
	load_executable_image(argv[0], mh, &entry_point);

	/*
	 * If the prebinding set is still set then try to setup this program to
	 * use the prebound state in it's images.  If any of these fail then
	 * undo any prebinding and bind as usual.
	 */
	if((mh->flags & MH_PREBOUND) != MH_PREBOUND){
	    /*
	     * The executable is not prebound but if the libraries are setup
	     * for prebinding and the executable when built had no undefined
	     * symbols then try to use the prebound libraries.  This is for
	     * the flat namespace case (and only some sub cases, see the
	     * comments in try_to_use_prebound_libraries()).  If this fails
	     * then the two-level namespace cases are handled by the routine
	     * find_twolevel_prebound_lib_subtrees() which is called below.
	     */
	    if(prebinding == TRUE){
		if((mh->flags & MH_NOUNDEFS) == MH_NOUNDEFS){
		    try_to_use_prebound_libraries();
		}
		else{
		    if(dyld_prebind_debug != 0)
			print("dyld: %s: prebinding disabled because "
			       "executable not marked with MH_NOUNDEFS\n",
				argv[0]);
		    prebinding = FALSE;
		}
	    }
	}
	else if(prebinding == TRUE){
	    set_images_to_prebound();
	}
	if(prebinding == FALSE){
	    /*
	     * The program was not fully prebound but it we are not forcing
	     * flat namespace semantics we can still use any sub trees of
	     * libraries that are all two-level namespace and prebound.
	     */
	    if(force_flat_namespace == FALSE)
		find_twolevel_prebound_lib_subtrees();

	    /*
	     * First undo any images that were prebound.
	     */
	    undo_prebound_images();

	    /*
	     * Build the initial list of non-lazy symbol references based on the
	     * executable.
	     */
	    if((mh->flags & MH_BINDATLOAD) != 0 || dyld_bind_at_launch == TRUE)
		executable_bind_at_load = TRUE;
	    setup_initial_undefined_list(FALSE);

	    /*
	     * With the undefined list set up link in the needed modules.
	     */
	    link_in_need_modules(FALSE, FALSE);
	}
	else{
	    if(dyld_prebind_debug != 0){
		if((mh->flags & MH_PREBOUND) != MH_PREBOUND)
		    print("dyld: %s: prebinding enabled using only prebound "
			   "libraries\n", argv[0]);
		else
		    print("dyld: %s: prebinding enabled\n", argv[0]);
	    }
	}
	launched = TRUE;

	/*
	 * If DYLD_EBADEXEC_ONLY is set then print a message as the program
	 * will launch.
	 */
	if(dyld_ebadexec_only == TRUE){
	    error("executable: %s will be launched (DYLD_EBADEXEC_ONLY set, "
		"program not started)", argv[0]);
	    link_edit_error(DYLD_FILE_ACCESS, EBADEXEC, argv[0]);
	}

	/* release lock for dyld data structures */
	release_lock();
	
	DYLD_TRACE_INIT_END(0);

	/*
	 * Return the address of the executable's entry point which is used if
	 * this routine was called from __dyld_start.  Otherwise this was called
	 * from the runtime startoff of the executable and this return value is
	 * ignored.
	 */
	return(entry_point);
}

/*
 * pickup_environment_variables() goes through the pointer to the environment
 * strings and sets the global variables for dyld based on the environment
 * strings.
 */
static
void
pickup_environment_variables(
char *envp[])
{
    char **p;

	for(p = envp; *p != NULL; p++){
	    if(**p == 'D' && strncmp(*p, "DYLD_", sizeof("DYLD_") - 1) == 0){
		if(strncmp(*p, "DYLD_FRAMEWORK_PATH=",
		           sizeof("DYLD_FRAMEWORK_PATH=") - 1) == 0){
		    /*
		     * Framework path searching is not done for setuid programs
		     * which are not run by the real user.  Futher the
		     * evironment varaible for the framework path is cleared so
		     * that if this program executes a non-set uid program this
		     * part of the evironment will not be passed along so that
		     * that program also will not have it's frameworks searched
		     * for.
		     */
		    if(getuid() != 0 &&
		       (getuid() != geteuid() || getgid() != getegid()))
			(*p)[sizeof("DYLD_FRAMEWORK_PATH=") - 1] = '\0';
		    else if(*(*p + sizeof("DYLD_FRAMEWORK_PATH=") - 1) != '\0'){
			dyld_framework_path =
				*p + sizeof("DYLD_FRAMEWORK_PATH=") - 1;
			dyld_no_fix_prebinding = TRUE;
		    }
		}
		else if(strncmp(*p, "DYLD_FALLBACK_FRAMEWORK_PATH=",
		           sizeof("DYLD_FALLBACK_FRAMEWORK_PATH=") - 1) == 0){
		    if(getuid() != 0 &&
		       (getuid() != geteuid() || getgid() != getegid()))
			(*p)[sizeof("DYLD_FALLBACK_FRAMEWORK_PATH=")- 1] = '\0';
		    else if(*(*p + sizeof(
				"DYLD_FALLBACK_FRAMEWORK_PATH=") - 1) != '\0'){
			dyld_fallback_framework_path =
			    *p + sizeof("DYLD_FALLBACK_FRAMEWORK_PATH=") - 1;
			dyld_no_fix_prebinding = TRUE;
		    }
		}
		else if(strncmp(*p, "DYLD_LIBRARY_PATH=",
		                sizeof("DYLD_LIBRARY_PATH=") - 1) == 0){
		    /*
		     * Library path searching is not done for setuid programs
		     * which are not run by the real user.  Futher the
		     * evironment varaible for the library path is cleared so
		     * that if this program executes a non-set uid program this
		     * part of the evironment will not be passed along so that
		     * that program also will not have it's libraries searched
		     * for.
		     */
		    if(getuid() != 0 &&
		       (getuid() != geteuid() || getgid() != getegid()))
			(*p)[sizeof("DYLD_LIBRARY_PATH=") - 1] = '\0';
		    else if(*(*p + sizeof("DYLD_LIBRARY_PATH=") - 1) != '\0'){
			dyld_library_path =
				*p + sizeof("DYLD_LIBRARY_PATH=") - 1;
			dyld_no_fix_prebinding = TRUE;
		    }
		}
		else if(strncmp(*p, "DYLD_FALLBACK_LIBRARY_PATH=",
			    sizeof("DYLD_FALLBACK_LIBRARY_PATH=") - 1) == 0){
		    if(getuid() != 0 &&
		       (getuid() != geteuid() || getgid() != getegid()))
			(*p)[sizeof("DYLD_FALLBACK_LIBRARY_PATH=") - 1] = '\0';
		    else if(*(*p + sizeof(
				"DYLD_FALLBACK_LIBRARY_PATH=") - 1) != '\0'){
			dyld_fallback_library_path =
				*p + sizeof("DYLD_FALLBACK_LIBRARY_PATH=") - 1;
			dyld_no_fix_prebinding = TRUE;
		    }
		}
		else if(strncmp(*p, "DYLD_IMAGE_SUFFIX=",
		                sizeof("DYLD_IMAGE_SUFFIX=") - 1) == 0){
		    /*
		     * Image suffix searching is not done for setuid programs
		     * which are not run by the real user.  Futher the
		     * evironment varaible for the library path is cleared so
		     * that if this program executes a non-set uid program this
		     * part of the evironment will not be passed along so that
		     * that program also will not have it's libraries searched
		     * for.
		     */
		    if(getuid() != 0 &&
		       (getuid() != geteuid() || getgid() != getegid()))
			(*p)[sizeof("DYLD_IMAGE_SUFFIX=") - 1] = '\0';
		    else if(*(*p + sizeof("DYLD_IMAGE_SUFFIX=") - 1) != '\0'){
			dyld_image_suffix =
				*p + sizeof("DYLD_IMAGE_SUFFIX=") - 1;
			dyld_no_fix_prebinding = TRUE;
		    }
		}
		else if(strncmp(*p, "DYLD_INSERT_LIBRARIES=",
		                sizeof("DYLD_INSERT_LIBRARIES=") - 1) == 0){
		    /*
		     * Library insertion is also not done for setuid programs
		     * which are not run by the real user.  Futher the
		     * evironment varaible for the library insertion is cleared
		     * so that if this program executes a non-set uid program
		     * this part of the evironment will not be passed along so
		     * that that program also will not have libraries inserted.
		     */
		    if(getuid() != 0 &&
		       (getuid() != geteuid() || getgid() != getegid()))
			(*p)[sizeof("DYLD_INSERT_LIBRARIES=") - 1] = '\0';
		    else if(*(*p + sizeof("DYLD_INSERT_LIBRARIES=")- 1) !='\0'){
			dyld_insert_libraries = malloc(strlen(
				*p + sizeof("DYLD_INSERT_LIBRARIES=") - 1) + 1);
			strcpy(dyld_insert_libraries,
			       *p + sizeof("DYLD_INSERT_LIBRARIES=") - 1);
			dyld_no_fix_prebinding = TRUE;
		    }
		}
		else if(strncmp(*p, "DYLD_DEBUG_TRACE=",
		                sizeof("DYLD_DEBUG_TRACE=") - 1) == 0){
		    dyld_debug_trace = TRUE;
		}
		else if(strncmp(*p, "DYLD_ERROR_PRINT=",
		                sizeof("DYLD_ERROR_PRINT=") - 1) == 0){
		    dyld_error_print = TRUE;
		}
		else if(strncmp(*p, "DYLD_PRINT_LIBRARIES=",
		                sizeof("DYLD_PRINT_LIBRARIES=") - 1) == 0){
		    dyld_print_libraries = TRUE;
		}
		else if(strncmp(*p, "DYLD_TRACE=",
		                sizeof("DYLD_TRACE=") - 1) == 0){
		    dyld_trace = TRUE;
		}
		else if(strncmp(*p, "DYLD_MEM_PROTECT=",
		                sizeof("DYLD_MEM_PROTECT=") - 1) == 0){
		    dyld_mem_protect = TRUE;
		    get_data_segment();
		    mem_prot_lock = *global_lock;
		    global_lock = &mem_prot_lock;
		    mem_prot_debug_lock = *debug_thread_lock;
		    debug_thread_lock = &mem_prot_debug_lock;
		}
		else if(strncmp(*p, "DYLD_EBADEXEC_ONLY=",
		                sizeof("DYLD_EBADEXEC_ONLY=") - 1) == 0){
		    dyld_ebadexec_only = TRUE;
		}
		else if(strncmp(*p, "DYLD_BIND_AT_LAUNCH=",
		                sizeof("DYLD_BIND_AT_LAUNCH=") - 1) == 0){
		    dyld_bind_at_launch = TRUE;
		}
		else if(strncmp(*p, "DYLD_FORCE_FLAT_NAMESPACE=",
		                sizeof("DYLD_FORCE_FLAT_NAMESPACE=") - 1) == 0){
		    dyld_force_flat_namespace = TRUE;
		}
		else if(strncmp(*p, "DYLD_DEAD_LOCK_HANG=",
		                sizeof("DYLD_DEAD_LOCK_HANG=") - 1) == 0){
		    dyld_dead_lock_hang = TRUE;
		}
		else if(strncmp(*p, "DYLD_ABORT_MULTIPLE_INITS=",
		                sizeof("DYLD_ABORT_MULTIPLE_INITS=") - 1) == 0){
		    dyld_abort_multiple_inits = TRUE;
		}
		else if(strncmp(*p, "DYLD_NEW_LOCAL_SHARED_REGIONS=",
			    sizeof("DYLD_NEW_LOCAL_SHARED_REGIONS=") - 1) == 0){
		    dyld_new_local_shared_regions = TRUE;
		    dyld_no_fix_prebinding = TRUE;
		}
		else if(strncmp(*p, "DYLD_NO_FIX_PREBINDING=",
			    sizeof("DYLD_NO_FIX_PREBINDING=") - 1) == 0){
		    dyld_no_fix_prebinding = TRUE;
		}
		else if(strncmp(*p, "DYLD_PREBIND_DEBUG=",
		                sizeof("DYLD_PREBIND_DEBUG") - 1) == 0){
		    if(strcmp("3", *p + sizeof("DYLD_PREBIND_DEBUG=") - 1) == 0)
			dyld_prebind_debug = 3;
		    else if(strcmp("2",
			    *p + sizeof("DYLD_PREBIND_DEBUG=") - 1) == 0)
			dyld_prebind_debug = 2;
		    else
			dyld_prebind_debug = 1;
		}
		else if(strncmp(*p, "DYLD_HINTS_DEBUG=",
		                sizeof("DYLD_HINTS_DEBUG") - 1) == 0){
		    if(strcmp("2", *p + sizeof("DYLD_HINTS_DEBUG=") - 1) == 0)
			dyld_hints_debug = 2;
		    else
			dyld_hints_debug = 1;
		}
		else if(strncmp(*p, "DYLD_SAMPLE_DEBUG=",
		                sizeof("DYLD_SAMPLE_DEBUG") - 1) == 0){
		    if(strcmp("2", *p + sizeof("DYLD_SAMPLE_DEBUG=") - 1) == 0)
			dyld_sample_debug = 2;
		    else
			dyld_sample_debug = 1;
		}
		else if(strncmp(*p, "DYLD_EXECUTABLE_PATH_DEBUG=",
			    sizeof("DYLD_EXECUTABLE_PATH_DEBUG=") - 1) == 0){
		    dyld_executable_path_debug = TRUE;
		}
		else if(strncmp(*p, "DYLD_TWO_LEVEL_DEBUG=",
			    sizeof("DYLD_TWO_LEVEL_DEBUG=") - 1) == 0){
		    dyld_two_level_debug = TRUE;
		}
	    }
	    else if(**p == 'H' && strncmp(*p, "HOME=", sizeof("HOME=")-1) == 0){
		home = save_string(*p + sizeof("HOME=") - 1);
	    }
	}

#if defined(__MACH30__) && (defined(__ppc__) || defined(__i386__))
	/*
	 * Pickup the pointer to the exec path.  This is placed by the kernel
	 * just after the trailing 0 of the envp[] array.  If exec_path is left
	 * as NULL other code will use argv[0] as a guess.
	 */
	exec_path = p[1];
	if(dyld_executable_path_debug == TRUE)
	    printf("exec_path = %s\n", exec_path == NULL ? "NULL" : exec_path);
#endif
}

/*
 * getenv for the libc code I use.  The normal getenv() calls
 * _dyld_lookup_and_bind() which dyld can't call.  This is needed for isspace()
 * in strtod needed by vsnprintf() for locale stuff.
 */
char *
getenv(const char *name)
{
	return(NULL);
}

/*
 * _NSGetEnviron() is needed by the malloc() in libc in MacOS X DP4 beginning
 * with Gonzo1H6.
 */
char ***
_NSGetEnviron(
void)
{
    static char *p = NULL;
    static char **pp = &p;
	return(&pp);
}

#ifdef DYLD_PROFILING
/*
 * profiling_exit is used as the symbol "__exit" in the routines
 * relocate_symbol_pointers_in_{object_image,library_image} to get the gmon.out
 * file written out.
 */
void
profiling_exit(
int status)
{
	dyld_monoutput();
	_exit(status);
}

/*
 * dyld_monoutput() causes dyld's gmon.out file to be written an profiling to
 * be turned off.
 */
void
dyld_monoutput(
void)
{
    struct stat stat_buf;
    char *p, gmon_out[1024];
    static enum bool done = FALSE;

	if(done == TRUE)
	    return;
	done = TRUE;
	if(stat("/dyld.gmon.out", &stat_buf) == 0){
	    p = strrchr(executables_name, '/');
	    if(p == NULL)
		p = executables_name;
	    else
		p++;
	    sprintf(gmon_out, "/dyld.gmon.out/%s.%d", p, getpid());
	    monoutput(gmon_out);
	}
}
#endif /* DYLD_PROFILING */

/*
 * get_data_segment() sets data_seg to point at the data segment command for the
 * dynamic linker.
 */
static
void
get_data_segment(
void)
{
    long i;
        
	data_seg = (struct segment_command *)
	      ((char *)&_mh_dylinker_header + sizeof(struct mach_header));
	for(i = 0; i < _mh_dylinker_header.ncmds; i++){
	    if(data_seg->cmd == LC_SEGMENT){
		if(strncmp(data_seg->segname, SEG_DATA,
			   sizeof(data_seg->segname)) == 0){
		    return;
		}
	    }
	    data_seg = (struct segment_command *)
		((char *)data_seg + data_seg->cmdsize);
	}
	data_seg = NULL;
}

void
protect_data_segment(
void)
{
    kern_return_t r;

	if((r = vm_protect(mach_task_self(), data_seg->vmaddr +
		dyld_image_vmaddr_slide, (vm_size_t)data_seg->vmsize,
		FALSE, VM_PROT_READ)) != KERN_SUCCESS){
	    mach_error(r, "can't vm_protect data segment of dyld");
	    link_edit_error(DYLD_MACH_RESOURCE, r, "dyld");
	}
}

void
unprotect_data_segment(
void)
{
    kern_return_t r;

	if((r = vm_protect(mach_task_self(), data_seg->vmaddr +
		dyld_image_vmaddr_slide, (vm_size_t)data_seg->vmsize,
		FALSE, data_seg->initprot)) != KERN_SUCCESS){
	    mach_error(r, "can't vm_(un)protect data segment of dyld");
	    link_edit_error(DYLD_MACH_RESOURCE, r, "dyld");
	}
}
