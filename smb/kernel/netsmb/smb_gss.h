/*
 * Copyright (c) 2006 - 2008, 2010 Apple Inc. All rights reserved.
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
#ifndef SMB_GSS_H
#define SMB_GSS_H

#define GSS_MACH_MAX_RETRIES 3
#define SKEYLEN 8

#ifndef GSS_S_COMPLETE
#define GSS_S_COMPLETE 0
#endif

#ifndef GSS_S_CONTINUE_NEEDED
#define GSS_S_CONTINUE_NEEDED 1
#endif

#define SMB_USE_GSS(vp) (IPC_PORT_VALID((vp)->vc_gss.gss_mp))
#define SMB_GSS_CONTINUE_NEEDED(p) ((p)->gss_major == GSS_S_CONTINUE_NEEDED)
#define SMB_GSS_ERROR(p) ((p)->gss_major != GSS_S_COMPLETE && \
	(p)->gss_major != GSS_S_CONTINUE_NEEDED)
#define SMB_GSS_COMPLETE(p) ((p)->gss_major == GSS_S_COMPLETE)
int smb_gss_negotiate(struct smb_vc *vcp, vfs_context_t context);
int smb_gss_ssnsetup(struct smb_vc *vcp, vfs_context_t context);
void smb_gss_destroy(struct smb_gss *gp);

void smb_gss_ref_cred(struct smb_vc *vcp);
void smb_gss_rel_cred(struct smb_vc *vcp);

#ifdef SMB_DEBUG
//#define DEBUG_TURN_OFF_EXT_SEC 1
#endif // SMB_DEBUG

#endif
