/*
 * Copyright (c) 2007 Apple Computer, Inc. All Rights Reserved.
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

/*
 *  readerstate.cpp
 *  SmartCardServices
*/

#include "readerstate.h"
#include "pcsclite.h"
#include "eventhandler.h"
#include <security_utilities/debugging.h>

DWORD SharedReaderState_State(READER_STATE *rs)
{
	PCSCD::SharedReaderState *srs = PCSCD::SharedReaderState::overlay(rs);
	return srs->xreaderState();
}

DWORD SharedReaderState_Protocol(READER_STATE *rs)
{
	PCSCD::SharedReaderState *srs = PCSCD::SharedReaderState::overlay(rs);
	return srs->xcardProtocol();
}

DWORD SharedReaderState_Sharing(READER_STATE *rs)
{
	PCSCD::SharedReaderState *srs = PCSCD::SharedReaderState::overlay(rs);
	return srs->sharing();
}

size_t SharedReaderState_CardAtrLength(READER_STATE *rs)
{
	PCSCD::SharedReaderState *srs = PCSCD::SharedReaderState::overlay(rs);
	return srs->xcardAtrLength();
}

LONG SharedReaderState_ReaderID(READER_STATE *rs)
{
	PCSCD::SharedReaderState *srs = PCSCD::SharedReaderState::overlay(rs);
	return srs->xreaderID();
}

const unsigned char *SharedReaderState_CardAtr(READER_STATE *rs)
{
	PCSCD::SharedReaderState *srs = PCSCD::SharedReaderState::overlay(rs);
	return srs->xcardAtr();
}

const char *SharedReaderState_ReaderName(READER_STATE *rs)
{
	PCSCD::SharedReaderState *srs = PCSCD::SharedReaderState::overlay(rs);
	return srs->xreaderName();
}

int SharedReaderState_ReaderNameIsEqual(READER_STATE *rs, const char *otherName)
{
	if (otherName)
	{
		PCSCD::SharedReaderState *srs = PCSCD::SharedReaderState::overlay(rs);
		return (strcmp(otherName, srs->xreaderName()) == 0);
	}
	else
		return 0;
}

void SharedReaderState_SetState(READER_STATE *rs, DWORD state)
{
	PCSCD::SharedReaderState *srs = PCSCD::SharedReaderState::overlay(rs);
	srs->xreaderState(state);
}

void SharedReaderState_SetProtocol(READER_STATE *rs, DWORD newprotocol)
{
	PCSCD::SharedReaderState *srs = PCSCD::SharedReaderState::overlay(rs);
	srs->xcardProtocol(newprotocol);
}

void SharedReaderState_SetCardAtrLength(READER_STATE *rs, size_t len)
{
	PCSCD::SharedReaderState *srs = PCSCD::SharedReaderState::overlay(rs);
	srs->xcardAtrLength(len);
}


#pragma mark ---------- C Interface ----------


