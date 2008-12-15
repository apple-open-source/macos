/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

#ifndef __KERBEROSSERVICESETUP_H__
#define __KERBEROSSERVICESETUP_H__

__BEGIN_DECLS

CFErrorRef SetAFPPrincipal(CFStringRef inPrincipal);
CFErrorRef SetFTPPrincipal(CFStringRef inPrincipal);
CFErrorRef SetIMAPPrincipal(CFStringRef inPrincipal);
CFErrorRef SetPOPPrincipal(CFStringRef inPrincipal);
CFErrorRef SetSMTPPrincipal(CFStringRef inPrincipal);
CFErrorRef SetHTTPPrincipal(CFStringRef inPrincipal);
CFErrorRef SetIPPPrincipal(CFStringRef inPrincipal);
CFErrorRef SetJABBERPrincipal(CFStringRef inPrincipal);
CFErrorRef SetNFSPrincipal(CFStringRef inPrincipal);
CFErrorRef SetVPNPrincipal(CFStringRef inPrincipal);
CFErrorRef SetSSHPrincipal(CFStringRef inPrincipal);
CFErrorRef SetLDAPPrincipal(CFStringRef inPrincipal);
CFErrorRef AddXGridPrincipal(CFStringRef inPrincipal);
CFErrorRef SetXGridPrincipal(CFStringRef inPrincipal);
CFErrorRef SetSMBPrincipal(CFStringRef inPrincipal, CFStringRef inAdminName, const char *inPassword);
CFErrorRef SetFTPPrincipal(CFStringRef inPrincipal);
CFErrorRef SetVNCPrincipal(CFStringRef inPrincipal);
CFErrorRef SetPCastPrincipal(CFStringRef inPrincipal);
CFErrorRef SetFCSvrPrincipal(CFStringRef inPrincipal);

__END_DECLS

#endif
