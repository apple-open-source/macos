/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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

#ifdef SMB_DEBUG
int testGettingDfsReferralDict(struct smb_ctx * ctx, const char *referral);
#endif // SMB_DEBUG

int checkForDfsReferral(struct smb_ctx * in_ctx, struct smb_ctx ** out_ctx,
                        char *tmscheme, CFMutableArrayRef dfsReferralDictArray);
int decodeDfsReferral(struct smb_ctx *inConn, mdchain_t mdp,
                      char *rcv_buffer, uint32_t rcv_buffer_len,
                      const char *dfs_referral_str,
                      CFMutableDictionaryRef *outReferralDict);
int getDfsReferralDict(struct smb_ctx * inConn, CFStringRef referralStr,
                       uint16_t maxReferralVersion, CFMutableDictionaryRef *outReferralDict);
int getDfsReferralList(struct smb_ctx * inConn, CFMutableDictionaryRef dfsReferralDict);
