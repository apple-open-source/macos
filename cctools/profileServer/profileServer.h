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
/* profileServer.h created by mwatson on Wed 24-May-1995 */

#import <libc.h>

#define PROFILE_DIR		"/var/tmp/profile"
#define PROFILE_SERVER_NAME	"NSProfileServer"
#define PROFILE_CONTROL_NAME	"NSProfileControl"
#define PROFILE_REQUEST_ID 0xfadedace

enum request_type {
    NSCreateProfileBufferForLibrary = 0,
    NSRemoveProfileBufferForLibrary,
    NSStartProfilingForLibrary,
    NSBufferFileForLibrary,
    NSStopProfilingForLibrary,
    NSEnableProfiling,
    NSDisableProfiling,
    NSBufferStatus
};

enum profile_state {
    NSBufferNotCreated = 0,
    NSProfilingStarted,
    NSBufferRemoved,
    NSProfilingStopped,
    NSProfilingDisabled
};

enum result_code {
    NSSuccess = 0,
    NSUnknownError,
    NSUnknownRequest,
    NSNotFound,
    NSNoMemory,
    NSNoPermission,
    NSBootstrapError,
    NSFileError,
    NSOfileFormatError
};
    
#ifdef __MACH30__
#error "profileServer not ported yet to Mach 3.0, Not built if RC_OS=macos"
#endif

struct request_msg {
    msg_header_t	hdr;
    msg_type_t		request_type;
    enum request_type  	request;
    msg_type_t		dylib_type;
    char		dylib_file[MAXPATHLEN];
};

struct reply_msg {
    msg_header_t	hdr;
    msg_type_t		profile_state_type;
    enum profile_state	profile_state;
    msg_type_t		result_code_type;
    enum result_code	result_code;
    msg_type_t		type;
    char		gmon_file[MAXPATHLEN];
};

boolean_t profile_server_exists(void);
boolean_t buffer_for_dylib(const char *dylib_name, char *gmon_file);
boolean_t start_profiling_for_dylib(const char *dylib_name);
boolean_t stop_profiling_for_dylib(const char *dylib_name);
