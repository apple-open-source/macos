/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header CDataBuff
 */

#ifndef __CDataBuff_h__
#define __CDataBuff_h__	1

#include "DirServicesTypes.h"
#include "PrivateTypes.h"

const uInt32 kDefaultSize = 512;

// Class --------------------------------------------------------------------

class CDataBuff {
public:
				CDataBuff		( uInt32 inSize = 0 );
	virtual	   ~CDataBuff		( void );

	void		AppendString	( const char *inStr );
	void		AppendLong		( uInt32 inInt );
	void		AppendShort		( uInt16 inShort );
	void		AppendBlock		( const void *inData, uInt32 inLength );

	void		Clear			( uInt32 inSize = 0 );

	uInt32		GetSize			( void );
	uInt32		GetLength		( void );
	char*		GetData			( void );

protected:

private:
	void		GrowBuff		( uInt32 inNewSize = 0 );

	uInt32		fSize;
	uInt32		fLength;
	char	   *fData;
};

#endif
