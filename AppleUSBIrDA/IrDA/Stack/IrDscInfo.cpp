/*
    File:       IrDscInfo.cpp

    Contains:   Implementation of IrDscInfo (XID discovery/device info) class


*/

#include "IrDscInfo.h"
#include "CBufferSegment.h"
//#include "IrConstData.h"

#define super OSObject
    OSDefineMetaClassAndStructors(TIrDscInfo, OSObject);

//--------------------------------------------------------------------------------
//      TIrDscInfo
//--------------------------------------------------------------------------------
TIrDscInfo *
TIrDscInfo::tIrDscInfo()
{
    TIrDscInfo *obj = new TIrDscInfo;
    if (obj && !obj->init()) {
	obj->release();
	obj = nil;
    }
    return obj;
}

void
TIrDscInfo::free()
{
    super::free();
}


bool
TIrDscInfo::init()
{

    fDevAddr = 0;
    fHints = 0;
    fVersion = 0;
    fCharset = 0;
    bzero(fNickname, sizeof(fNickname));
    bzero(fHintCount, sizeof(fHintCount));

    if (!super::init()) return false;
    
    return true;
} // TIrDscInfo::init


//--------------------------------------------------------------------------------
//      SetServiceHints
//--------------------------------------------------------------------------------
void TIrDscInfo::SetServiceHints( ULong hints )
{   
    fHints |= hints;
    
    for( int index = 0; index < kHintCount; index++ ) {
	if( hints & ( 1<<index ) )
	    fHintCount[index]++;
    }
}


//--------------------------------------------------------------------------------
//      RemoveServiceHints
//--------------------------------------------------------------------------------
void TIrDscInfo::RemoveServiceHints( ULong hints )
{
    for( int index = 0; index < kHintCount; index++ ) {
	ULong   hintMask = 1<<index;
	if( hints & hintMask ) {
	    fHintCount[index]--;
	    if( fHintCount[index] == 0 )
		fHints &= ~hintMask;
	}
    }
}


//--------------------------------------------------------------------------------
//      SetNickname
//--------------------------------------------------------------------------------
IrDAErr TIrDscInfo::SetNickname(const char* name)
{
    ULong nameLen;

    nameLen = strlen(name);
    if (nameLen > kMaxNicknameLen) {
	return errBadArg;
    }

    strlcpy((char*)&fNickname[0], name, sizeof(fNickname));
    return noErr;

} // TIrDscInfo::SetNickname


//--------------------------------------------------------------------------------
//      GetNickname
//--------------------------------------------------------------------------------
void TIrDscInfo::GetNickname(UChar* name, int maxnamelen)
{
    // Note: name provided must be at least kMaxNicknameLen chars long
    // It is responsibility of IrGlue code that uses this to ensure that.
    strlcpy((char*)name, (const char*)&fNickname[0], maxnamelen);

} // TIrDscInfo::GetNickname


//--------------------------------------------------------------------------------
//      AddDevInfoToBuffer
//--------------------------------------------------------------------------------
ULong TIrDscInfo::AddDevInfoToBuffer(UByte* buffer, ULong maxBytes)
{
#pragma unused(maxBytes)

    UByte hintByte;
    ULong index;
    ULong hints;
    ULong nameLen;
    ULong devInfoLen = 0;

    // Store the hint byte(s)
    for (hints = fHints, index = 0; index < 4; index++) {
	// Mask off next hint byte
	hintByte = (UByte)(hints & 0xFF);
	hints >>= 8;

	// If more hint bytes, set the extension bit
	if (hints != 0) {
	    hintByte |= kDevInfoHintExtension1;
	}
	else        // jdg: protect us from garbage hints being set
	    hintByte &= ~kDevInfoHintExtension1;    // make sure it's off   

	// Store the hintByte, inc byte count
	*buffer++ = hintByte;
	devInfoLen++;

	// No more hint bytes?
	if (hints == 0) break;
    }

    // Add the character set encoding (I'm only supporting ASCII for now)
    *buffer++ = GetCharacterSet();
    devInfoLen++;

    // Figure out how much of the name can be written out
    nameLen = Min(strlen((const char*)&fNickname[0]), kMaxNicknameLen - (devInfoLen - 2));
    memcpy(buffer, fNickname, (unsigned int)nameLen);
    devInfoLen += nameLen;

    return devInfoLen;

} // TIrDscInfo::AddDevInfoToBuffer


//--------------------------------------------------------------------------------
//      ExtractDevInfoFromBuffer
//--------------------------------------------------------------------------------
IrDAErr TIrDscInfo::ExtractDevInfoFromBuffer(CBufferSegment *buffer)
{
    UByte hintByte;
    ULong index;
    ULong bytesRead;
    UByte devInfo[4 + 1 + kMaxNicknameLen];

    // Initialize fields (in case no devInfo was provided - which is supposedly okay?)
    fHints = 0;
    fNickname[0] = 0;

    // Read in maximum
    bytesRead = buffer->Getn(&devInfo[0], sizeof(devInfo));

    // Parse the hint byte(s)
    for (index = 0; index < bytesRead; ) {
	hintByte = devInfo[index];
	fHints |= (hintByte & ~kDevInfoHintExtension1) << (index * 8);
	index++;
	if ((hintByte & kDevInfoHintExtension1) == 0) break;
    }

    // Parse the char set byte
    if (index < bytesRead) {
	SetCharacterSet(devInfo[index++]);
    }

    // Now extract the device name (and null terminate it)
    memcpy(fNickname, &devInfo[index], (unsigned int)(bytesRead - index));
    fNickname[bytesRead - index] = 0;

    return noErr;

} // TIrDscInfo::ExtractDevInfoFromBuffer
	
