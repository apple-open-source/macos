/*
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 2000-2007 Apple Inc.  All Rights Reserved.
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

#include <mach/mach.h>
#include <mach-o/loader.h>
#include <mach/mach_port.h>

/*******************************************************************************
* copyKextUUID()
*
* Return value: -1 error
*                0 kmod not loaded/no UUID
*                1 success
*******************************************************************************/
int copyKextUUID(
    mach_port_t host_port,
    const char * kernel_filename,
    const char * bundle_id,
    char ** uuid,
    unsigned int * uuid_size);

/*******************************************************************************
* copyMachoUUIDFromMemory()
*
* Return value: -1 error
*                0 no UUID
*                1 success
*******************************************************************************/
int copyMachoUUIDFromMemory(
    struct mach_header * kernel_file,
    void * kernel_file_end,
    char ** uuid,
    unsigned int * uuid_size);

/*******************************************************************************
* machoUUIDsMatch()
*
* Return value: -1 error
*                0 no match
*                1 success
*******************************************************************************/
int machoUUIDsMatch(
    mach_port_t host_port,
    const char * kernel_file_1,
    const char * kernel_file_2);

/*******************************************************************************
* machoFileMatchesUUID()
*
* Return value: -1 error
*                0 no match
*                1 success
*******************************************************************************/
int machoFileMatchesUUID(
    const char * file,
    char * running_uuid,
    unsigned int running_uuid_size);
