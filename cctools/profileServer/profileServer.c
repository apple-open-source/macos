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
/* Copyright (c) 1995 NeXT Computer, Inc.  All Rights Reserved. */

#import <libc.h>
#import <stdio.h>
#import <stdlib.h>
#import <syslog.h>
#import <sys/file.h>
#import <sys/types.h>
#import <sys/stat.h>
#import <sys/errno.h>
#import <mach/mach.h>
#import "stuff/openstep_mach.h"
#import <mach/mach_error.h>
#ifdef __OPENSTEP__
#import <mach-o/gmon.h>
#else
#import <sys/gmon.h>
#import <sys/sysctl.h>
#endif
#import <servers/bootstrap.h>

#import "stuff/ofile.h"
#import "stuff/errors.h"
#import "stuff/round.h"

#import "profileServer.h"

static enum result_code create_file_for_dylib(const char *dylib, char *gmon_out);
static enum result_code create(const char *dylib, const char *gmon_out);
static enum result_code new_buffer_for_library(const char *dylib);
static enum result_code remove_buffer_for_library(const char *dylib);
static enum result_code map_dylib(const char *dylib, const char *gmon_out);
static enum result_code unmap_dylib(const char *dylib);
static struct dylib_map *lookup_dylib(const char *dylib);
static enum result_code set_profiling_for_dylib(const char *dylib, boolean_t enabled);
static enum result_code set_profiling_enabled(boolean_t enabled);
static boolean_t init_server(void);
static boolean_t run_server(void);
#ifndef __OPENSTEP__
static int getprofhz(void);
#endif

struct dylib_map {
    char	*dylib_name;
    char	*gmon_name;
    boolean_t	enabled;
    struct dylib_map *previous;
    struct dylib_map *next;
};

char *progname = NULL;
static struct dylib_map *mapHead = (struct dylib_map *)NULL;
static struct dylib_map *mapTail = (struct dylib_map *)NULL;
static port_t profile_port = PORT_NULL;
static port_t control_port = PORT_NULL;
static port_set_name_t	port_set = PORT_NULL;
static boolean_t profiling_enabled = FALSE;

/*
 * profileServer is invoked as follows:
 *
 *	% profileServer [ dylib1 [dylib2 ...]]
 *
 * where dylib1 is the file name of a dynamic library
 */

int
main(
int argc,
char *argv[])
{
	int i;

        if ((progname = strrchr(argv[0], '/')) == NULL)
	    progname = argv[0];
	else
	    progname++;

	openlog(progname, LOG_PID, LOG_DAEMON);
	if (init_server() == FALSE)
	    exit(EXIT_FAILURE);
	
	if (argc > 1)
	    set_profiling_enabled(TRUE);
        for (i = 1; i < argc; i++) {
            new_buffer_for_library(argv[i]);
	    set_profiling_for_dylib(argv[i], TRUE);
        }

        if (run_server() == TRUE)
	    return(EXIT_SUCCESS);
	else
	    return(EXIT_FAILURE);
}

static enum result_code
create_file_for_dylib(
const char *dylib,
char *new_gmon_file)
{
	extern int errno;
	enum result_code result;
	struct stat s;
	int cc;
	
	cc = stat(PROFILE_DIR, &s);
	if (cc < 0) {
	    if (errno == ENOENT) {
		cc = mkdir(PROFILE_DIR, 01777);
		if (cc < 0) {
		    syslog(LOG_ERR, "Couldn't create profile directory %s: %m",
			PROFILE_DIR);
		    return NSFileError;
		}
	    } else {
		syslog(LOG_ERR, "Couldn't find profile directory %s: %m", PROFILE_DIR);
		return NSNotFound;
	    }
	}
        sprintf(new_gmon_file, "%s/profile.XXXXXX", PROFILE_DIR);
        if (mktemp(new_gmon_file) == NULL) {
		return NSFileError;
	}
        if ((result = create(dylib, new_gmon_file)) != NSSuccess) {
            return result;
        }
        syslog(LOG_INFO, "Created %s for %s", new_gmon_file, dylib);
	return NSSuccess;
}

static enum result_code
new_buffer_for_library(
const char *dylib)
{
    enum result_code result;
    char tempfile[MAXPATHLEN];

    if (lookup_dylib(dylib) == NULL) {
	if((result = create_file_for_dylib(dylib, tempfile)) != NSSuccess)
	    return result;
        if((result = map_dylib(dylib, tempfile)) != NSSuccess)
            return result;
        if((result = set_profiling_for_dylib(dylib, FALSE)) != NSSuccess)
            return result;
    }
    return NSSuccess;
}

static enum result_code
remove_buffer_for_library(
const char *dylib)
{
    struct dylib_map *map = lookup_dylib(dylib);
    if (map == NULL) {
        return NSNotFound;
    }
    if (unlink(map->gmon_name) == -1) {
        perror("unlink");
        return NSFileError;
    }
    return unmap_dylib(dylib);
}

/*
 * create() takes the file name of a dynamic library (dylib) and creates a
 * gmon.out file (gmon_out).  It checks to see the dylib file is correct and
 * creates the gmon.out file proportional to the size of the (__TEXT,__text)
 * section of the dynamic library.
 */
static enum result_code
create(
const char *dylib,
const char *gmon_out)
{
    struct arch_flag host_arch_flag;
    struct ofile ofile;
    unsigned long i, j, size;
    struct load_command *lc;
    struct segment_command *sg;
    struct section *s, *text_section;
    kern_return_t r;
#ifdef __OPENSTEP__
    struct phdr *header;
#else
    struct gmonhdr *header;
#endif
    char *pcsample_buffer;
    int fd;
    unsigned short mask;
    enum result_code result = NSUnknownError;

    size = 0;
    /*
       * Open and map in the dylib file and check it for correctness.
       */
    if (get_arch_from_host(&host_arch_flag, NULL) == 0) {
        warning("can't determine the host architecture");
        return NSUnknownError;
    }
    if (ofile_map(dylib, &host_arch_flag, NULL, &ofile, FALSE) == FALSE) {
        return NSOfileFormatError;
    }
    if (ofile.mh == NULL ||
       (ofile.mh->filetype != MH_DYLIB &&
        ofile.mh->filetype != MH_DYLINKER)) {
	result = NSOfileFormatError;
        goto bail;
    }
    /*
       * Get the text section for dynamic library.
       */
    text_section = NULL;
    lc = ofile.load_commands;
    for (i = 0; i < ofile.mh->ncmds && text_section == NULL; i++){
        if (lc->cmd == LC_SEGMENT){
            sg = (struct segment_command *)lc;
            s = (struct section *)
                ((char *)sg + sizeof(struct segment_command));
            for (j = 0; j < sg->nsects; j++){
                if (strcmp(s->sectname, SECT_TEXT) == 0 &&
                   strcmp(s->segname, SEG_TEXT) == 0){
                    text_section = s;
                    break;
                }
                s++;
            }
        }
        lc = (struct load_command *)((char *)lc + lc->cmdsize);
    }
    if (text_section == NULL) {
        warning("file: %s does not have a (" SEG_TEXT "," SECT_TEXT
                ") section", dylib);
	result = NSOfileFormatError;
        goto bail;
    }

    /*
       * Create a pcsample buffer for the text section.
       */
    size = round(text_section->size / 1, sizeof(unsigned short));
#ifdef __OPENSTEP__
    size += sizeof(struct phdr);
#else
    size += sizeof(struct gmonhdr);
#endif
    r = vm_allocate(mach_task_self(), (vm_address_t *)&pcsample_buffer,
                    (vm_size_t)size, TRUE);
    if (r != KERN_SUCCESS) {
        warning("can't vm_allocate pcsample buffer of size: %lu", size);
	result = NSNoMemory;
        goto bail;
    }

    /*
       * Create and write the pcsample file. See comments in gmon.h for the
       * values of the profile header.
       */
#ifdef __OPENSTEP__
    header = (struct phdr *)pcsample_buffer;
    header->lpc = (char *)(text_section->addr);
    header->hpc = (char *)(text_section->addr + text_section->size);
#else
    header = (struct gmonhdr *)pcsample_buffer;
    /* use memset to zero out the spares[] in the header */
    memset(header, '\0', sizeof(header));
    header->lpc = text_section->addr;
    header->hpc = text_section->addr + text_section->size;
    header->version = GMONVERSION;
    header->profrate = getprofhz();
#endif
    header->ncnt = (int)size;
    mask = umask(0);
    if ((fd = open(gmon_out, O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1) {
	(void)umask(mask);
        warning("can't create gmon.out file: %s", gmon_out);
	result = NSFileError;
        goto bail;
    }
    (void)umask(mask);
    if (write(fd, pcsample_buffer, size) != size) {
        warning("can't write gmon.out file: %s", gmon_out);
        close(fd);
	result = NSFileError;
        goto bail;
    }
    r = vm_deallocate(mach_task_self(), (vm_address_t)pcsample_buffer,
		      (vm_size_t)size);
    if (r != KERN_SUCCESS) {
        warning("can't vm_deallocate pcsample buffer");
        close(fd);
        ofile_unmap(&ofile);
        return NSNoMemory;
    }
    ofile_unmap(&ofile);
    if (close(fd) == -1) {
        warning("can't close gmon.out file: %s", gmon_out);
        return NSFileError;
    }
    return NSSuccess;
    
bail:
    if(size != 0)
	vm_deallocate(mach_task_self(), (vm_address_t)pcsample_buffer,
		      (vm_size_t)size);
    ofile_unmap(&ofile);
    return result;	
}

#ifndef __OPENSTEP__
/*
 * Get the profiling rate.
 */
static
int
getprofhz(void)
{
    int mib[2];
    size_t size;
    struct clockinfo clockrate;

        mib[0] = CTL_KERN;
        mib[1] = KERN_CLOCKRATE;
        clockrate.profhz = 1;
        size = sizeof(clockrate);
        if(sysctl(mib, 2, &clockrate, &size, NULL, 0) < 0)
		;
        return(clockrate.profhz);
}
#endif

/*
 * map_dylib() takes the file name of a dynamic library (dylib) and adds it to
 * the internal table of known libraries.  A linear list, but it should
 * be small.
 */
static enum result_code
map_dylib(
const char *dylib,
const char *gmon_out)
{
    if (lookup_dylib(dylib) == NULL) {
        struct dylib_map *map;

        map = malloc(sizeof(struct dylib_map));
	if(map == NULL)
	    return(NSNoMemory);
        map->dylib_name = malloc(strlen(dylib) + 1);
	if(map->dylib_name == NULL)
	    return(NSNoMemory);
        map->gmon_name = malloc(strlen(gmon_out) + 1);
	if(map->gmon_name == NULL)
	    return(NSNoMemory);
        map->previous = (struct dylib_map *)NULL;
        map->next = (struct dylib_map *)NULL;
        strcpy(map->dylib_name, dylib);
        strcpy(map->gmon_name, gmon_out);
        map->enabled = FALSE;
        if (mapHead == (struct dylib_map *)NULL) {
            mapHead = mapTail = map;
        } else {
            mapTail->next = map;
            map->previous = mapTail;
            mapTail = map;
        }
    }
    return(NSSuccess);
}

/*
 * lookup_dylib() takes the file name of a dynamic library (dylib) and returns the
 * dylib file structure if there is one, or NULL if one has not been recorded.
 */
static
struct dylib_map *
lookup_dylib(
const char *dylib)
{
    struct dylib_map *map;
    for (map = mapHead; map != (struct dylib_map *)NULL; map = map->next) {
        if (strcmp(map->dylib_name, dylib) == 0) {
            return (map);
   	}
    }
    return NULL;
}

/*
 * unmap_dylib() takes the file name of a dynamic library (dylib) and removes
 * it from the map table.
 */
static enum result_code
unmap_dylib(
const char *dylib)
{
    struct dylib_map *map = lookup_dylib(dylib);
    if (map == NULL) {
        return NSNotFound;
    }
    if (mapHead == mapTail) {
        mapHead = mapTail = (struct dylib_map *)NULL;
    } else {
        if (map == mapTail) {
            mapTail = map->previous;
            mapTail->next = NULL;
        } else {
            map->next->previous = map->previous;
        }
        if (map == mapHead) {
            mapHead = map->next;
            mapHead->previous = NULL;
        } else {
            map->previous->next = map->next;
        }
    }
    free(map->dylib_name);
    free(map->gmon_name);
    free(map);
    return NSSuccess;
}

static enum result_code
set_profiling_for_dylib(const char *dylib, boolean_t enabled)
{
    struct dylib_map *map = lookup_dylib(dylib);
    if (map == NULL) {
        return NSNotFound;
    }
    map->enabled = enabled;
    return NSSuccess;
}

static enum result_code
set_profiling_enabled(boolean_t enabled)
{
    kern_return_t	ret;
    
    if (enabled) {
	if (profiling_enabled)
	    return NSSuccess;
	ret = port_allocate(mach_task_self(), &profile_port);
	if (ret != KERN_SUCCESS) {
	    syslog(LOG_ERR, "Couldn't allocate profile port: %s", mach_error_string(ret));
	    return NSBootstrapError;
	}
	ret = bootstrap_register(bootstrap_port, PROFILE_SERVER_NAME, profile_port);
	if (ret != BOOTSTRAP_SUCCESS) {
	    syslog(LOG_ERR, "Couldn't check in profile server port: %s", mach_error_string(ret));
	    return NSBootstrapError;
	}
	ret = port_set_add(mach_task_self(), port_set, profile_port);
	if (ret != BOOTSTRAP_SUCCESS) {
	    syslog(LOG_ERR, "Couldn't add profile server to port set: %s", mach_error_string(ret));
	    return NSBootstrapError;
	}
	profiling_enabled = TRUE;
    } else {
	if (!profiling_enabled)
	    return TRUE;
	ret = port_set_remove(mach_task_self(), profile_port);
	if (ret != KERN_SUCCESS) {
	    /* Non-fatal; should go ahead and destroy the port below. */
	    syslog(LOG_WARNING, "Warning: couldn't remove profile server from port set: %s", mach_error_string(ret));
	}
	ret = port_deallocate(mach_task_self(), profile_port);
	if (ret != KERN_SUCCESS) {
	    syslog(LOG_ERR, "Couldn't destroy profile server port: %s", mach_error_string(ret));
	    return NSBootstrapError;
	}
	profile_port = PORT_NULL;
	profiling_enabled = FALSE;
    }
    return NSSuccess;
}

static enum result_code
get_buffer_status(
    char *dylib_file,
    char **fileName,
    enum profile_state *state
)
{
    struct dylib_map *map;
    int index;
    
    index = atoi(dylib_file);
    for (map = mapHead; map && index; index--)
	map = map->next;
    if (map) {
	*fileName = map->dylib_name;
	*state = map->enabled ? NSProfilingStarted : NSProfilingStopped;
	return NSSuccess;
    }
    return NSNotFound;
}

static boolean_t
init_server(void)
{
    kern_return_t ret;
    
    /* Create services if they doesn't already exist. */
    ret = bootstrap_create_service(bootstrap_port, PROFILE_SERVER_NAME, &profile_port);
    if (ret != BOOTSTRAP_SUCCESS && ret != BOOTSTRAP_NAME_IN_USE) {
        syslog(LOG_ERR, "Couldn't create profile service: %s", mach_error_string(ret));
        return FALSE;
    }
    ret = bootstrap_create_service(bootstrap_port, PROFILE_CONTROL_NAME, &control_port);
    if (ret != BOOTSTRAP_SUCCESS && ret != BOOTSTRAP_NAME_IN_USE) {
	syslog(LOG_ERR, "Couldn't create profile control service: %s", mach_error_string(ret));
	return FALSE;
    }

    /* Initially, the service is disabled.
     * Create a port set including the profile service
     * and the profile control service.
     */
    ret = port_set_allocate(mach_task_self(), &port_set);
    if (ret != KERN_SUCCESS) {
	syslog(LOG_ERR, "Couldn't allocate port set: %s", mach_error_string(ret));
	return FALSE;
    }

    ret = port_allocate(mach_task_self(), &control_port);
    if (ret != KERN_SUCCESS) {
        syslog(LOG_ERR, "Couldn't allocate control port: %s", mach_error_string(ret));
        return FALSE;
    }
    /* Enable the control service. */
    ret = bootstrap_register(bootstrap_port, PROFILE_CONTROL_NAME, control_port);
    if (ret != BOOTSTRAP_SUCCESS) {
	syslog(LOG_ERR, "Couldn't check in profile control port: %s", mach_error_string(ret));
	return FALSE;
    }
    
    ret = port_set_add(mach_task_self(), port_set, control_port);
    if (ret != KERN_SUCCESS) {
	syslog(LOG_ERR, "Couldn't add control port to port set: %s", mach_error_string(ret));
	return FALSE;
    }
    return TRUE;
}

static boolean_t
run_server(void)
{
    struct request_msg request_msg;
    struct reply_msg reply_msg;
    msg_return_t msg_ret;
    struct dylib_map *map;
    char *fileName = "";
    enum result_code result;
    enum profile_state state;
    
    for (;;) {
        /*
         * Wait for an incoming request
         */
        request_msg.hdr.msg_local_port = port_set;
        request_msg.hdr.msg_size = sizeof(struct request_msg);

        msg_ret = msg_receive(&request_msg.hdr, MSG_OPTION_NONE, (msg_timeout_t)0);
        if (msg_ret != RCV_SUCCESS) {
            syslog(LOG_ERR, "msg_receive: %s", mach_error_string(msg_ret));
	    break;
        }
	result = NSUnknownError;
        if (request_msg.hdr.msg_id != PROFILE_REQUEST_ID) {
            syslog(LOG_ERR, "Unknown message type: %d",
                request_msg.hdr.msg_id);
            continue;
        }

	if (request_msg.hdr.msg_local_port == profile_port) {
	    map = lookup_dylib(request_msg.dylib_file);
	    if (map) {
		fileName = map->gmon_name;
		state = map->enabled ? NSProfilingStarted : NSProfilingStopped;
		result = NSSuccess;
	    } else {
		state = NSBufferNotCreated;
		result = NSNotFound;
	    }
	    switch (request_msg.request) {
		case NSCreateProfileBufferForLibrary:
		    result = new_buffer_for_library(request_msg.dylib_file);
		    state = NSProfilingStopped;
		    break;
		case NSRemoveProfileBufferForLibrary:
		    result = remove_buffer_for_library(request_msg.dylib_file);
		    state = NSBufferRemoved;
		    break;
		case NSBufferFileForLibrary:
#if defined(NEW_BUFFER_PER_REQUEST)
		    if (state == NSProfilingStarted) {
			char new_gmon_file[MAXPATHLEN];
			result = create_file_for_dylib(request_msg.dylib_file, new_gmon_file);
		    	fileName = new_gmon_file;
		    }
#endif
		    break;
		case NSStartProfilingForLibrary:
		    result = set_profiling_for_dylib(request_msg.dylib_file, TRUE);
		    state = NSProfilingStarted;
		    break;
		case NSStopProfilingForLibrary:
		    result = set_profiling_for_dylib(request_msg.dylib_file, FALSE);
		    state = NSProfilingStopped;
		    break;
		case NSBufferStatus:
		    result = get_buffer_status(request_msg.dylib_file, &fileName, &state);
		    break;
		default:
		    syslog(LOG_ERR, "Unknown request type: %d",  request_msg.request);
		    result = NSUnknownRequest;
		    continue;
	    }
	} else if (request_msg.hdr.msg_local_port == control_port) {
	    switch (request_msg.request) {
		case NSEnableProfiling:
		    result = set_profiling_enabled(TRUE);
		    break;
		case NSDisableProfiling:
		    result = set_profiling_enabled(FALSE);
		    break;
		default:
		    syslog(LOG_ERR, "unknown control request type: %d", request_msg.request);
		    result = NSUnknownRequest;
		    continue;
	    }
	}

        /*
         * Cons up the header and type structs
         */
        reply_msg.hdr.msg_simple = TRUE;
        reply_msg.hdr.msg_size = sizeof(struct reply_msg);
        reply_msg.hdr.msg_type = MSG_TYPE_NORMAL;
        reply_msg.hdr.msg_local_port = PORT_NULL;
        reply_msg.hdr.msg_remote_port = request_msg.hdr.msg_remote_port;
        reply_msg.hdr.msg_id = request_msg.hdr.msg_id;
	strcpy(reply_msg.gmon_file, fileName);
        reply_msg.type.msg_type_name = MSG_TYPE_CHAR;
        reply_msg.type.msg_type_size = sizeof(char) * 8;
        reply_msg.type.msg_type_number = strlen(fileName) + 1;
        reply_msg.type.msg_type_inline = TRUE;
        reply_msg.type.msg_type_longform = FALSE;
        reply_msg.type.msg_type_deallocate = FALSE;
	reply_msg.profile_state = state;
        reply_msg.profile_state_type.msg_type_name = MSG_TYPE_INTEGER_32;
        reply_msg.profile_state_type.msg_type_size = sizeof(enum profile_state) * 8;
        reply_msg.profile_state_type.msg_type_number = 1;
        reply_msg.profile_state_type.msg_type_inline = TRUE;
        reply_msg.profile_state_type.msg_type_longform = FALSE;
        reply_msg.profile_state_type.msg_type_deallocate = FALSE;
        reply_msg.result_code_type.msg_type_name = MSG_TYPE_INTEGER_32;
        reply_msg.result_code_type.msg_type_size = sizeof(enum result_code) * 8;
        reply_msg.result_code_type.msg_type_number = 1;
        reply_msg.result_code_type.msg_type_inline = TRUE;
        reply_msg.result_code_type.msg_type_longform = FALSE;
        reply_msg.result_code_type.msg_type_deallocate = FALSE;
	reply_msg.result_code = result;

        /*
         * Send it off.
         */
        msg_ret = msg_send(&reply_msg.hdr, MSG_OPTION_NONE, (msg_timeout_t)0);
        if (msg_ret != SEND_SUCCESS) {
            syslog(LOG_ERR, "msg_send: %s", mach_error_string(msg_ret));
        }
    }
    return FALSE;
}
