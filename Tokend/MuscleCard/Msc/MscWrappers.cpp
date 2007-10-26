/*
 *  Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
 *  MscWrappers.cpp
 *  TokendMuscle
 */

#include "MscWrappers.h"

inline bool operator == (const MSCObjectInfo &s1, const MSCObjectInfo &s2)
{
    return ::strcmp(s1.objectID,s2.objectID)==0;
}

inline bool operator != (const MSCObjectInfo &s1, const MSCObjectInfo &s2)
{
	return !(s1 == s2);
}

MscTokenInfo::MscTokenInfo(const MSCTokenInfo &rTokenInfo)
{
	// Set basic fields
	tokenAppLen = rTokenInfo.tokenAppLen;		// Default AID Length
	tokenIdLength = rTokenInfo.tokenIdLength;	// ID Length (ATR Length)
	tokenState = rTokenInfo.tokenState;			// State (dwEventState)
	tokenType = rTokenInfo.tokenType;			// Type - RFU
	addParams = rTokenInfo.addParams;			// Additional Data
	addParamsSize = rTokenInfo.addParamsSize;	// Size of additional data

	// Now copy the strings
	::memcpy(tokenName, rTokenInfo.tokenName, sizeof(tokenName));		// Token name
	::memcpy(slotName, rTokenInfo.slotName, sizeof(slotName));			// Slot/reader name
	::memcpy(svProvider, rTokenInfo.svProvider, sizeof(svProvider));	// Library
	::memcpy(reinterpret_cast<unsigned char *>(tokenId), reinterpret_cast<const unsigned char *>(rTokenInfo.tokenId), sizeof(tokenId));		// Token ID (ATR)
	::memcpy(reinterpret_cast<unsigned char *>(tokenApp), reinterpret_cast<const unsigned char *>(rTokenInfo.tokenApp), sizeof(tokenApp));	// Default app ID
}

MscTokenInfo::MscTokenInfo(const SCARD_READERSTATE &readerState)
{
	// An ss is enough info to be able to open a connection
	::memset(this, 0, sizeof(*this));			// overkill, but what the heck
	::strncpy(slotName, readerState.szReader, sizeof(slotName));			// Slot/reader name
	size_t idsz = min(size_t(readerState.cbAtr),size_t(sizeof(tokenId)));
	::memcpy(reinterpret_cast<unsigned char *>(tokenId), reinterpret_cast<const unsigned char *>(readerState.rgbAtr), idsz);		// Token ID (ATR)
	tokenIdLength = idsz;
	tokenState = readerState.dwEventState;
}

MscTokenInfo &MscTokenInfo::operator = (const MSCTokenInfo &rTokenInfo)
{
	// how do we avoid duplication of copy constructor code?

	// Set basic fields
	tokenAppLen = rTokenInfo.tokenAppLen;		// Default AID Length
	tokenIdLength = rTokenInfo.tokenIdLength;	// ID Length (ATR Length)
	tokenState = rTokenInfo.tokenState;			// State (dwEventState)
	tokenType = rTokenInfo.tokenType;			// Type - RFU
	addParams = rTokenInfo.addParams;			// Additional Data
	addParamsSize = rTokenInfo.addParamsSize;	// Size of additional data

	// Now copy the strings
	::memcpy(tokenName, rTokenInfo.tokenName, sizeof(tokenName));		// Token name
	::memcpy(slotName, rTokenInfo.slotName, sizeof(slotName));			// Slot/reader name
	::memcpy(svProvider, rTokenInfo.svProvider, sizeof(svProvider));	// Library
	::memcpy(reinterpret_cast<unsigned char *>(tokenId), reinterpret_cast<const unsigned char *>(rTokenInfo.tokenId), sizeof(tokenId));		// Token ID (ATR)
	::memcpy(reinterpret_cast<unsigned char *>(tokenApp), reinterpret_cast<const unsigned char *>(rTokenInfo.tokenApp), sizeof(tokenApp));	// Default app ID

	return *this;
}

#pragma mark ---------------- ostream methods --------------

#ifdef _DEBUG_OSTREAM

#include <iomanip>

std::ostream& operator << (std::ostream& strm, const MscObjectACL& oa)
{
	strm << "RD: " << oa.readPermission << " WR: " << oa.writePermission << " DEL: " << oa.deletePermission;
	return strm;
}

std::ostream& operator << (std::ostream& strm, const MscObjectInfo& oi)
{
	strm << "ID: " << oi.objectID << " Size: " << oi.objectSize << " ACL: " << MscObjectACL(oi.objectACL);
	return strm;
}

std::ostream& operator << (std::ostream& strm, const MscTokenInfo& ti)
{
	strm << "Token name     : " << ti.tname() << "\n";
	strm << "Slot name      : " << ti.sname() << "\n";
	strm << "Token id (ATR) : [" << std::dec << ti.tokenIdLength << "] ";
	const unsigned char *tid = ti.tid();
	for (unsigned int jx=0;jx < ti.tokenIdLength;jx++)
	{
		strm << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(tid[jx]);
		if (((jx+1) % 4)==0)
			strm << " ";
	}
	strm << "\nToken state	 :  " << ti.tokenState << "\n";
	strm << "Provider       : " << ti.provider() << "\n";
	strm << "App ID         : [" << std::dec << ti.tokenAppLen << "] " << ti.app() << "\n";
	strm << "Type           :  " << ti.tokenType << "\n";	// Type - RFU

	strm << "Addl Params    : [" << ti.addParamsSize << "] " << ti.app() << "\n";
	const unsigned char *tap = reinterpret_cast<const unsigned char *>(ti.addParams);
	for (unsigned int jx=0;jx < ti.addParamsSize;jx++)
		strm << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << tap[jx];
//		strm << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(tap[jx]);
	return strm;
}

std::ostream& operator << (std::ostream& strm, const MscStatusInfo& si)
{
//	strm << "GetStatus returns           : " << MscError(rv) << "\n";
	strm << "Protocol version            : 0x" <<
		std::hex << std::uppercase << std::setw(4) << std::setfill('0') << si.appVersion << "\n";
	strm << "Applet version              : 0x" <<
		std::hex << std::uppercase << std::setw(4) << std::setfill('0') << si.swVersion << "\n";
	strm << "Total object memory         : " <<
		std::dec << std::setw(8) << std::setfill('0') << si.totalMemory << "\n";
	strm << "Free object memory          : " <<
		std::dec << std::setw(8) << std::setfill('0') << si.freeMemory << "\n";
	strm << "Number of used PINs         : " <<
		std::dec << std::setw(2) << std::setfill('0') << si.usedPINs << "\n";
	strm << "Number of used Keys         : " <<
		std::dec << std::setw(2) << std::setfill('0') << si.usedKeys << "\n";
	strm << "Currently logged identities : 0x" <<
		std::hex << std::uppercase << std::setw(4) << std::setfill('0') << si.loggedID << "\n";
	return strm;
}
#endif	// _DEBUG_OSTREAM

