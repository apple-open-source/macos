/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

/*!
 * @header CBuff
 */

/*
Start of buffer:

Buffer Type	  Dat Block Cnt 	Offset 1   Offset 2  Offset n	EndTag = 'EndT'
	4			     4			   4		  4		    4		  4


	Total Rec Len	Rec Type Len	Rec Type	Rec Name Len	Rec Name
		4				2				n			2				n

	Attr Cnt	Attr Len  AttrName Len	Attr Name
		2			2			2			n
		  Value Cnt	 Value1 Len    Value1	Value2 Len	 Value2
			  2			2			n			2			n


	o) The data is stacked on at the end of the buffer.

*/

#ifndef __CBuff_h__
#define	__CBuff_h__	1

#include <DirectoryServiceCore/PrivateTypes.h>


class CBuff {
public:

enum {
	kNotInitalized		= -119,
	kBuffNull			= -120,
	kBuffFull			= -121,
	kBuffTooSmall		= -122,
	kBuffInvalFormat	= -123
};

enum {
	kEndTag		= 'EndT'
};

public:
					CBuff					( void );
	virtual		   ~CBuff					( void );

	sInt32			Initialize				( tDataBuffer *inBuff, bool inClear = false );

	sInt32			GetBuffType				( uInt32 *outType );
	sInt32			SetBuffType				( uInt32 inBuffType );

	sInt32			SetBuffLen				( uInt32 inBuffLen );

	tDataBuffer*	GetBuffer				( void );
	sInt32			GetBuffStatus			( void );

	sInt32			AddData					( char *inData, uInt32 inLen );
	sInt32			GetDataBlockCount		( uInt32 *outCount );
	char*			GetDataBlock			( uInt32 inIndex, uInt32 *outOffset );
	uInt32			GetDataBlockLength		( uInt32 inIndex );

	void			ClearBuff				( void );
	void			SetLengthToSize			( void );

protected:
	sInt32			SetNextOffset			( uInt32 inLen );

private:
	sInt32			fStatus;
	uInt32			fOffset;
	uInt32			fWhatsLeft;
	tDataBuffer	   *fBuff;
};

#endif

