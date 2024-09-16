/*
 * Copyright (c) 2024 Apple Computer, Inc. All rights reserved.
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

#ifndef fskit_support_h
#define fskit_support_h

typedef enum : int {
    check_fs_op,
    mount_fs_op,
    format_fs_op,
} fskit_command_t;

/*
 * invoke_tool_from_fskit - fsck_, mount_, or newfs_ using FSKit
 *
 *      This routine determines if FSKit is present, and if so,
 * attempts to invoke the tool using the supplied arguments.
 *
 *      This routine returns if FSKit is unavailable, if the named
 * FSModule is unknown, or if the named FSModule does not support this tool.
 *
 *      In case of successful tool invocation or syntax error, this
 * routine exits the calling program.
 *
 *      In the mount_fs_op case, this function will add "nofollow"
 * if MNT_NOFOLLOW is set in flags and the module supports it.
 */
int
invoke_tool_from_fskit(fskit_command_t operation, int flags,
                       int argc, char * const *argv);

#endif /* fskit_support_h */
