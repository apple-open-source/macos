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
/* Copyright (c) 1995 NeXT Computer, Inc.  All Rights Reserved. */
#import <libc.h>
#import <stdio.h>
#import <mach/mach.h>
#import "stuff/openstep_mach.h"
#import <mach/mach_error.h>
#import <mach/message.h>
#import <servers/bootstrap.h>

#import "profileServer.h"

static char *progname = "dylib_profile";

static const char *profile_request_string[] = {
    "Create profile buffer",	// NSCreateProfileBufferForLibrary,
    "Remove profile buffer",	// NSRemoveProfileBufferForLibrary,
    "Start profiling",		// NSStartProfilingForLibrary,
    "Look up buffer",		// NSBufferFileForLibrary,
    "Stop profiling",		// NSStopProfilingForLibrary,
    "Globally enable profiling",  // NSEnableProfiling,
    "Globally disable profiling", // NSDisableProfiling
};

static const char *profile_state_string[] = {
    "Buffer not created",	// NSBufferNotCreated = 0,
    "Profiling started",	// NSProfilingStarted,
    "Buffer removed",		// NSBufferRemoved,
    "Profiling stopped",	// NSProfilingStopped,
    "Profiling disabled",	// NSProfilingDisabled

};

static const char *profile_result_string[] = {
    "Error 0",			// NSSuccess = 0,
    "Unknown error",		// NSUnknownError,
    "Unknown request",		// NSUnknownRequest,
    "File or dylib not found",	// NSNotFound,
    "No memory available",	// NSNoMemory,
    "Permission denied",	// NSNoPermission,
    "Bootstrap service error",	// NSBootstrapError,
    "File error",		// NSFileError,
    "Object file format error",	// NSOfileFormatError
};

static
int
copy_file(
    const char *file,
    const char *copy)
{
    int fd1, fd2, retval;
    char *buf;
    struct stat info;
    kern_return_t ret;

    fd1 = open(file, O_RDONLY, 0666);
    if (fd1 < 0) {
        perror("open");
        return -1;
    }
    if (fstat(fd1, &info) < 0) {
        perror("fstat");
        return -1;
    }
    if (info.st_size < 0 || (info.st_mode & S_IFMT) != S_IFREG) {
        return -1;
    }
    if ((ret = map_fd(fd1, 0, (vm_offset_t *)&buf, TRUE, info.st_size)) !=
	KERN_SUCCESS) {
        mach_error("map_fd", ret);
        return -1;
    }
    fd2 = open(copy, O_WRONLY | O_CREAT, 0666);
    if (fd2 < 0) {
        perror("open");
        return -1;
    }
    retval = write(fd2, buf, info.st_size);
    if (retval < 0) {
        perror("write");
        return retval;
    }
    retval = fsync(fd2);
    if (retval < 0) {
        perror("fsync");
        return retval;
    }
    retval = close(fd2);
    if (retval < 0) {
        perror("close");
        return retval;
    }
    retval = close(fd1);
    if (retval < 0) {
        perror("close");
        return retval;
    }
    ret = vm_deallocate(mach_task_self(), (vm_offset_t)buf,
			(vm_size_t)info.st_size);
    if (ret != KERN_SUCCESS) {
        mach_error("vm_deallocate", ret);
        return ret;
    }
    return 0;
}


static void
usage(void)
{
    fprintf(stderr, "usage: %s [-e | -d | -s] | "
                    "[-c | -r | -b | -h | -p [-o <file>] <dylib>]\n",
		    progname);
    fprintf(stderr, "-s:  show profiling status\n");
    fprintf(stderr, "-e:  globally enable profiling\n");
    fprintf(stderr, "-d:  globally disable profiling\n");
    fprintf(stderr, "-c:  create profile buffer\n");
    fprintf(stderr, "-r:  remove profile buffer\n");
    fprintf(stderr, "-b:  begin profiling for dylib\n");
    fprintf(stderr, "-h:  halt profiling for dylib\n");
    fprintf(stderr, "-p:  dump gmon.out\n");
    fprintf(stderr, "-o:  dump to specified file name\n");
}	

msg_return_t
send_request(
    port_t		port,
    enum request_type 	request,
    const char *  	dylib,
    enum profile_state	*profile_state,
    enum result_code	*result_code,
    char *		gmon_file
)
{
    union {
	struct request_msg	request;
	struct reply_msg	reply;
    } msg;
    msg_return_t	msg_ret;

    /*
     * Cons up the header and type structs
     */
    msg.request.hdr.msg_simple = TRUE;
    msg.request.hdr.msg_size = sizeof(struct request_msg);
    msg.request.hdr.msg_type = MSG_TYPE_NORMAL;
    msg.request.hdr.msg_local_port = thread_reply();
    msg.request.hdr.msg_remote_port = port;
    msg.request.hdr.msg_id = PROFILE_REQUEST_ID;
    msg.request.request_type.msg_type_name = MSG_TYPE_INTEGER_32;
    msg.request.request_type.msg_type_size = sizeof(enum request_type) * 8;
    msg.request.request_type.msg_type_number = 1;
    msg.request.request_type.msg_type_inline = TRUE;
    msg.request.request_type.msg_type_longform = FALSE;
    msg.request.request_type.msg_type_deallocate = FALSE;
    msg.request.dylib_type.msg_type_name = MSG_TYPE_CHAR;
    msg.request.dylib_type.msg_type_size = sizeof(char) * 8;
    msg.request.dylib_type.msg_type_number = strlen(dylib) + 1;
    msg.request.dylib_type.msg_type_inline = TRUE;
    msg.request.dylib_type.msg_type_longform = FALSE;
    msg.request.dylib_type.msg_type_deallocate = FALSE;
    
    strcpy(msg.request.dylib_file, dylib);
    msg.request.request = request;

    /*
     * Send it off.
     */
    msg_ret = msg_rpc(&msg.request.hdr, MSG_OPTION_NONE,
	   sizeof(msg), (msg_timeout_t)0, (msg_timeout_t)0);
    if (msg_ret != RPC_SUCCESS) {
	mach_error("msg_rpc:", msg_ret);
	return msg_ret;
    }

    *profile_state = msg.reply.profile_state;
    *result_code = msg.reply.result_code;
    strcpy(gmon_file, msg.reply.gmon_file);
    return SEND_SUCCESS;
}

static void
print_status(
    port_t	port
)
{
    int i;
    char buf[128];
    char file[MAXPATHLEN];
    kern_return_t ret;
    enum profile_state state;
    enum result_code status;
    
    for (i=0;; i++) {
	sprintf(buf, "%d", i);
	ret = send_request(port, NSBufferStatus, buf, &state, &status, file);
	if (ret != KERN_SUCCESS) {
	    fprintf(stderr, "%s: msg_send failed: %s\n",
		progname, mach_error_string(ret));
	    exit(3);
	}
	if (status == NSNotFound)
	    break;
	printf("%s: %s\n",file, profile_state_string[state]);
    }
    if (i == 0) {
	printf("No libraries are being profiled.\n");
    }
}

static void
profile_error(
    enum request_type	request,
    const char		*dylib,
    enum result_code	result
)
{
    if (dylib) 
	fprintf(stderr, "%s: %s: %s: %s\n",progname,
	    dylib,
	    profile_request_string[request],
	    profile_result_string[result]);
    else
	fprintf(stderr, "%s: %s: %s\n", progname,
	    profile_request_string[request],
	    profile_result_string[result]);
}


int
main(int argc, char **argv)
{
    extern int 		optind;
    extern char		*optarg;
#ifdef __OPENSTEP__
    extern int 		getopt(int argc, char **argv, char *optstring);
#endif
    int 		c;
    int 		bflag = 0, hflag = 0, pflag = 0, eflag = 0, dflag = 0;
    int			sflag = 0;
    int 		errflag = 0;
    char 		*ofile = "gmon.out";
    char 		*dylib;
    port_t		server_port;
    port_t		control_port;
    enum profile_state	state;
    enum request_type	request = -1, control_request = -1;
    char		gmon_file[MAXPATHLEN];
    boolean_t		profile_active, control_active;
    enum result_code	result;
    kern_return_t	ret;
    
    progname = argv[0];
    
    if (argc == 1) {
	 usage();
	 exit(2);
    }
    while ((c = getopt(argc, argv, "crbhpo:eds")) != EOF)
        switch (c) {
          case 'c':
            request = NSCreateProfileBufferForLibrary;
            break;
          case 'r':
            request = NSRemoveProfileBufferForLibrary;
            break;
          case 'b':
            bflag = 1;
            request = NSStartProfilingForLibrary;
            break;
          case 'h':
            hflag = 1;
            request = NSStopProfilingForLibrary;
            break;
          case 'p':
	    request = NSBufferFileForLibrary,
            pflag = 1;
            break;
          case 'o':
	    ofile = optarg;
            break;
	  case 'e':
	    eflag = 1;
	    control_request = NSEnableProfiling;
	    break;
	  case 'd':
	    dflag = 1;
	    control_request = NSDisableProfiling;
	    break;
	  case 's':
	    sflag = 1;
	    break;
          default:
            errflag = 1;
            break;
        }
	
    if (dflag && eflag) {
	fprintf(stderr, "You must specify only one of -d and -e.\n");
	usage();
	exit(2);
    }
    if (bflag && hflag) {
	fprintf(stderr, "You must specify only one of -b and -h.\n");
	usage();
	exit(2);
    }
    
    if (errflag || (!(dflag || eflag || sflag) && optind >= argc)) {
	usage();
	exit(2);
    }
    
    dylib = argv[optind];
    
    if (bootstrap_look_up(bootstrap_port, PROFILE_SERVER_NAME, &server_port)
	!= BOOTSTRAP_SUCCESS ||
	bootstrap_status(bootstrap_port, PROFILE_SERVER_NAME, &profile_active)
	!= BOOTSTRAP_SUCCESS) {
	fprintf(stderr, "%s: couldn't locate profile server port\n",progname);
	exit(3);
    }
    
    if (bootstrap_look_up(bootstrap_port, PROFILE_CONTROL_NAME, &control_port)
	!= BOOTSTRAP_SUCCESS ||
	bootstrap_status(bootstrap_port, PROFILE_CONTROL_NAME, &control_active)
	!= BOOTSTRAP_SUCCESS) {
	fprintf(stderr, "%s: couldn't locate profile control port\n",progname);
	exit(3);
    }
    
    if (sflag) {
	if (!control_active) {
	    printf("Profiling service not found\n");
	    exit(0);
	}
	if (!profile_active) {
	    printf("Profiling service not enabled\n");
	    exit(0);
	}
	print_status(server_port);
	exit(0);
    }
    
    *gmon_file = '\0';
    if (dflag || eflag) {
	if (!control_active) {
	    fprintf(stderr, "%s: profile server isn't running\n", progname);
	    exit(3);
	}
	if ((ret = send_request(control_port, control_request, "", &state, &result, gmon_file)) != KERN_SUCCESS) {
	    fprintf(stderr, "%s: msg_send failed: %s", progname, mach_error_string(ret));
	    exit(3);
	}
	if (result != NSSuccess) {
	    profile_error(control_request, dylib, result);
	    exit(3);
	}
	exit(0);
    }

    if (request != -1) {
	if (!profile_active) {
	    fprintf(stderr, "%s: profile server isn't active.\n", progname);
	    fprintf(stderr, "Use '%s -e' to enable it.\n", progname);
	    exit(3);
	}
	if ((ret = send_request(server_port, request, dylib, &state, &result, gmon_file)) != KERN_SUCCESS) {
	    fprintf(stderr, "%s: msg_send failed: %s\n", progname, mach_error_string(ret));
	    exit(3);
	}
	if (result != NSSuccess) {
	    profile_error(request, dylib, result);
	    exit(3);
	}
    }
    
    if (pflag) {	
	if (*gmon_file == '\0' || state == NSBufferNotCreated) {
	    fprintf(stderr, "%s: dylib %s not found\n",progname,dylib);
	    exit(1);
	}
	
	errflag = copy_file(gmon_file, ofile);
	if (errflag) {
	    fprintf(stderr,"%s: couldn't write to %s\n", progname, ofile);
	    exit(errflag);
	}
    }
    
    exit(0);
}
