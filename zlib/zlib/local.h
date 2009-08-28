/*
 * Copyright (c) 2008 Apple Inc. All rights reserved.
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
#ifdef WIN32
#define SNPRINTF(...)	sprintf_s(__VA_ARGS__)
#define STRLCAT(d,s,n)	strcat_s((d),(n),(s))
#define STRLCPY(d,s,n)	strcpy_s((d),(n),(s))
#else /* !WIN32 */
#define SNPRINTF(...)	snprintf(__VA_ARGS__)
#define STRLCAT(d,s,n)	strlcat((d),(s),(n))
#define STRLCPY(d,s,n)	strlcpy((d),(s),(n))
#endif /* WIN32 */
