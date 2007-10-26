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
 *  ifdwrapper.h
 *  SmartCardServices
 */

/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 1999-2004
 *  David Corcoran <corcoran@linuxnet.com>
 *  Ludovic Rousseau <ludovic.rouseau@free.fr>
 *
 * $Id: ifdwrapper.h 2151 2006-09-06 20:02:47Z rousseau $
 */

/**
 * @file
 * @brief This wraps the dynamic ifdhandler functions. The abstraction will
 * eventually allow multiple card slots in the same terminal.
 */

#ifndef __ifdwrapper_h__
#define __ifdwrapper_h__

#ifdef __cplusplus
extern "C"
{
#endif

	LONG IFDOpenIFD(PREADER_CONTEXT);
	LONG IFDCloseIFD(PREADER_CONTEXT);
	LONG IFDPowerICC(PREADER_CONTEXT, DWORD, const unsigned char *, PDWORD);
	LONG IFDStatusICC(PREADER_CONTEXT, PDWORD, const unsigned char *, PDWORD);
	LONG IFDControl_v2(PREADER_CONTEXT, PUCHAR, DWORD, PUCHAR, PDWORD);
	LONG IFDControl(PREADER_CONTEXT, DWORD, LPCVOID, DWORD, LPVOID,
		DWORD, LPDWORD);
	LONG IFDTransmit(PREADER_CONTEXT, SCARD_IO_HEADER,
		PUCHAR, DWORD, PUCHAR, PDWORD, PSCARD_IO_HEADER);
	LONG IFDSetPTS(PREADER_CONTEXT, DWORD, UCHAR, UCHAR, UCHAR, UCHAR);
	LONG IFDSetCapabilities(PREADER_CONTEXT, DWORD, DWORD, PUCHAR);
	LONG IFDGetCapabilities(PREADER_CONTEXT, DWORD, PDWORD, PUCHAR);

#ifdef __cplusplus
}
#endif

#endif							/* __ifdwrapper_h__ */
