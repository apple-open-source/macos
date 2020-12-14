/*
 * Copyright (c) 2011 Apple Inc. All rights reserved.
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

#ifndef _NETSMB_SMB_PACKETS_2_h
#define _NETSMB_SMB_PACKETS_2_h

#include <sys/syslog.h>

#define	SMB2_SIGNATURE			"\xFESMB"
#define	SMB2_SIGLEN				4
#define	SMB2_HDRLEN				64

typedef uint32_t DWORD;

typedef struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME;

struct smb2_header
{
    uint8_t     protocol_id[4];
    uint16_t    structure_size;
    uint16_t    credit_charge;
    uint32_t    status; /* nt_status */
    uint16_t    command;
    uint16_t    credit_reqrsp;
    uint32_t    flags;
    uint32_t    next_command;
    uint64_t    message_id;
    
    union {
        /* Async commands have an async ID. */
        struct {
            uint64_t    async_id;
        } async;
        
        /* Sync command have a tree and process ID. */
        struct {
            uint32_t    process_id;
            uint32_t    tree_id;
        } sync;
    };
    
    uint64_t    session_id;
    uint8_t     signature[16];
    
    //enum { minimum_size = 64, maximum_size = 64 };
    //enum { defined_size = 64 };
};

#endif
