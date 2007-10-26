/*
 * Copyright (c) 2006 - 2007 Apple Inc. All rights reserved.
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
#ifndef _PARSE_URL_H_
#define _PARSE_URL_H_

#define SMB_SCHEMA_STRING  "smb"

void CreateSMBFromName(struct smb_ctx *ctx, char *fromname, int maxlen);
int ParseSMBURL(struct smb_ctx *ctx, int sharetype);
int CreateSMBURL(struct smb_ctx *ctx, const char *url);
int smb_url_to_dictionary(CFURLRef url, CFDictionaryRef *dict);
int smb_dictionary_to_url(CFDictionaryRef dict, CFURLRef *url);

#endif /* _PARSE_URL_H_ */
