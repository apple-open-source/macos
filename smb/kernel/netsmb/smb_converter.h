/*
 *  smb_converter.h
 *  smb
 *
 *  Created by George Colley on 5/2/08.
 *  Copyright 2008 Apple Inc. All rights reserved.
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

#ifndef _SMB_CONVERTER_H_
#define _SMB_CONVERTER_H_

#include <sys/utfconv.h>

#define NO_SFM_CONVERSIONS      0       /* Do not use the SFM conversion tables */

int smb_convert_to_network(const char **inbuf, size_t *inbytesleft, char **outbuf, 
						   size_t *outbytesleft, int flags, int unicode);
int smb_convert_from_network(const char **inbuf, size_t *inbytesleft, char **outbuf, 
							 size_t *outbytesleft, int flags, int unicode);
size_t smb_strtouni(u_int16_t *dst, const char *src, size_t inlen, int flags);
size_t smb_unitostr(char *dst, const u_int16_t *src, size_t inlen, size_t maxlen, int flags);
size_t smb_utf16_strnlen(const uint16_t *, size_t /* max */);

#endif // _SMB_CONVERTER_H_
