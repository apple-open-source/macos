/*
 *  Copyright (c) 2000-2007 Apple Inc. All Rights Reserved.
 * 
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*
 *  prothandler.h
 *  SmartCardServices
 */

/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999
 *  David Corcoran <corcoran@linuxnet.com>
 * Copyright (C) 2004
 *  Ludovic Rousseau <ludovic.rousseau@free.fr>
 *
 * $Id: prothandler.h 123 2010-03-27 10:50:42Z ludovic.rousseau@gmail.com $
 */

/**
 * @file
 * @brief This handles protocol defaults, PTS, etc.
 */

#ifndef __prothandler_h__
#define __prothandler_h__

#ifdef __cplusplus
extern "C"
{
#endif

	UCHAR PHGetDefaultProtocol(const unsigned char *, DWORD);
	UCHAR PHGetAvailableProtocols(const unsigned char *, DWORD);
	DWORD PHSetProtocol(struct ReaderContext *, DWORD, UCHAR, UCHAR);

#define SET_PROTOCOL_WRONG_ARGUMENT -1
#define SET_PROTOCOL_PPS_FAILED -2

#ifdef __cplusplus
}
#endif

#endif							/* __prothandler_h__ */
