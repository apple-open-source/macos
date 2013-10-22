/*
 * Copyright (c) 2011-2012  Apple Inc. All rights reserved.
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

#include <sys/smb_apple.h>

#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_rq_2.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_conn_2.h>
#include <netsmb/smb_gss_2.h>

int
smb_gss_ssandx(struct smb_vc *vcp, uint32_t caps, uint16_t *action, 
               vfs_context_t context)
{
    int retval;
    uint16_t session_flags = 0;
    
    if (vcp->vc_flags & SMBV_SMB2) {
        retval = smb2_smb_gss_session_setup(vcp, &session_flags, context);
        if (retval == 0) {
            /* Remap SMB2 session flags to SMB1 action flags */
            if (session_flags & SMB2_SESSION_FLAG_IS_GUEST) {
                /* Return that we got logged in as Guest */
                *action |= SMB_ACT_GUEST;
            }
        }
    }
    else {
        retval = smb1_gss_ssandx(vcp, caps, action, context);
    }
    return (retval);
}

