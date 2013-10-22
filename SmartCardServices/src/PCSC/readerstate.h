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
 *  readerstate.h
 *  SmartCardServices
 */

#ifndef _H_PCSCD_READER_STATE
#define _H_PCSCD_READER_STATE

#include "wintypes.h"
#include "pcsclite.h"
#include "readerfactory.h"
#include "eventhandler.h"
#include <MacTypes.h>

#ifdef __cplusplus
extern "C"
{
#endif

DWORD SharedReaderState_State(READER_STATE *rs);
DWORD SharedReaderState_Protocol(READER_STATE *rs);
DWORD SharedReaderState_Sharing(READER_STATE *rs);
size_t SharedReaderState_CardAtrLength(READER_STATE *rs);
LONG SharedReaderState_ReaderID(READER_STATE *rs);
const unsigned char *SharedReaderState_CardAtr(READER_STATE *rs);
const char *SharedReaderState_ReaderName(READER_STATE *rs);
int SharedReaderState_ReaderNameIsEqual(READER_STATE *rs, const char *otherName);
void SharedReaderState_SetState(READER_STATE *rs, DWORD state);
void SharedReaderState_SetProtocol(READER_STATE *rs, DWORD newprotocol);
void SharedReaderState_SetCardAtrLength(READER_STATE *rs, size_t len);

#ifdef __cplusplus
}
#endif


#if defined(__cplusplus)

#include <security_utilities/threading.h>

namespace PCSCD {

//
// NB: We are using the fact that on our systems, mutexes provide read/write
// memory barrier as a side effect to avoid having to flush the shared memory
// region to disk
//


//
// A PODWrapper for the PCSC ReaderState structure
//
class SharedReaderState : public PodWrapper<SharedReaderState, READER_STATE>
{
public:

	LONG xreaderID() const {  Atomic<int>::barrier(); return ntohl(readerID); }
	void xreaderID(LONG rid) { Atomic<int>::barrier(); readerID = htonl(rid); }
	
	DWORD xreaderState() const { Atomic<int>::barrier(); return ntohl(readerState); }
	void xreaderState(DWORD state) { Atomic<int>::barrier(); readerState = htonl(state); }

	DWORD sharing() const { Atomic<int>::barrier(); return ntohl(readerSharing); }
	void sharing(DWORD sharing) { Atomic<int>::barrier(); readerSharing = htonl(sharing); }

	DWORD xlockState() const { Atomic<int>::barrier(); return ntohl(lockState); }
	void xlockState(DWORD state) { Atomic<int>::barrier(); lockState = htonl(state); }

	DWORD xcardProtocol() const { Atomic<int>::barrier(); return ntohl(cardProtocol); }
	void xcardProtocol(DWORD prot) { Atomic<int>::barrier(); cardProtocol = htonl(prot); }

	// strings
	const char *xreaderName() const	{ Atomic<int>::barrier(); return readerName; }
	void xreaderName(const char *rname, size_t len = MAX_READERNAME)	{ Atomic<int>::barrier(); strlcpy(readerName, rname, len); }
	size_t readerNameLength() const { return strlen(readerName); }
	void xreaderNameClear()	{ Atomic<int>::barrier(); memset(readerName, 0, sizeof(readerName));  }

	const unsigned char *xcardAtr() const	{ Atomic<int>::barrier(); return cardAtr; }
	unsigned char *xcardAtr() 	{ Atomic<int>::barrier(); return cardAtr; }
	void xcardAtr(const unsigned char *atr, size_t len)	{ Atomic<int>::barrier(); 
		memcpy((char *)&cardAtr[0], (const char *)atr, len); cardAtrLength = htonl(len); }
	size_t xcardAtrLength() const { Atomic<int>::barrier(); return ntohl(cardAtrLength); }
	void xcardAtrLength(DWORD len)  { Atomic<int>::barrier(); cardAtrLength = htonl(len); }
	void xcardAtrClear()	{ Atomic<int>::barrier(); memset(cardAtr, 0, sizeof(cardAtr));  }
};



} // end namespace PCSCD

#endif /* __cplusplus__ */

#endif //_H_PCSCD_READER_STATE

