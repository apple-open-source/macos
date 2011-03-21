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

/*
 * Don't use the SFM conversion tables, really defined so we know that we 
 * specifically do not want to use the SFM conversions.
 */
#define NO_SFM_CONVERSIONS			0x0000
/*
 * Used when calling the smb_convert_path_to_network and smb_convert_network_to_path
 * routines. Since UTF_SFM_CONVERSIONS is only defined in the kernel. The kernel
 * has it defined as "Use SFM mappings for illegal NTFS chars"
 */
#define SMB_UTF_SFM_CONVERSIONS		0x0020
/*
 * Used when calling the smb_convert_path_to_network and smb_convert_network_to_path
 * routines. Make sure the returned path is a full path, add a starting delimiter
 * if one does exist. The calling process must make sure the buffer is large
 * enough to hold the output plus the terminator size.
 */
#define SMB_FULLPATH_CONVERSIONS	0x0100

int smb_convert_to_network(const char **inbuf, size_t *inbytesleft, char **outbuf, 
						   size_t *outbytesleft, int flags, int unicode);
int smb_convert_from_network(const char **inbuf, size_t *inbytesleft, char **outbuf, 
							 size_t *outbytesleft, int flags, int unicode);
size_t smb_strtouni(u_int16_t *dst, const char *src, size_t inlen, int flags);
size_t smb_unitostr(char *dst, const u_int16_t *src, size_t inlen, size_t maxlen, int flags);
size_t smb_utf16_strnlen(const uint16_t *, size_t /* max */);
size_t smb_utf16_strnsize(const uint16_t *s, size_t n_bytes);
int smb_convert_path_to_network(char *path, size_t max_path_len, char *network, 
								size_t *ntwrk_len, char ntwrk_delimiter, int inflags, 
								int usingUnicode);
int smb_convert_network_to_path(char *network, size_t max_ntwrk_len, char *path, 
								size_t *path_len, char ntwrk_delimiter, int flags, 
								int usingUnicode);

#endif // _SMB_CONVERTER_H_
