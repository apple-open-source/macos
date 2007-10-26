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
 *  eventhandler.h
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
 * $Id: eventhandler.h 2151 2006-09-06 20:02:47Z rousseau $
 */

/**
 * @file
 * @brief This handles card insertion/removal events, updates ATR,
 * protocol, and status information.
 */

#ifndef __eventhandler_h__
#define __eventhandler_h__

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * Define an exported public reader state structure so each
	 * application gets instant notification of changes in state.
	 */
	typedef struct pubReaderStatesList
	{
		LONG readerID;
		char readerName[MAX_READERNAME];
		DWORD readerState;
		LONG readerSharing;
		DWORD lockState;

		UCHAR cardAtr[MAX_ATR_SIZE];
		DWORD cardAtrLength;
		DWORD cardProtocol;
	}
	READER_STATE, *PREADER_STATE;

	LONG EHInitializeEventStructures(void);
	LONG EHSpawnEventHandler(PREADER_CONTEXT);
	LONG EHDestroyEventHandler(PREADER_CONTEXT);
	void EHSetSharingEvent(PREADER_CONTEXT, DWORD);

#ifdef __cplusplus
}
#endif

#endif							/* __eventhandler_h__ */
