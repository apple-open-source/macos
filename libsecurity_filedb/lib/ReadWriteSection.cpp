#include "ReadWriteSection.h"

uint32 WriteSection::put(uint32 inOffset, uint32 inValue)
{
	uint32 aLength = CheckUInt32Add(inOffset, sizeof(inValue));
	if (aLength > mCapacity)
		grow(aLength);

	if (mAddress == NULL)
		CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);

	*reinterpret_cast<uint32 *>(mAddress + inOffset) = htonl(inValue);
	return aLength;
}



uint32 WriteSection::put(uint32 inOffset, uint32 inLength, const uint8 *inData)
{
	// if we are being asked to put 0 bytes, just return
	if (inLength == 0 || inData == NULL)
	{
		return inOffset;
	}
	
	uint32 aLength = CheckUInt32Add(inOffset, inLength);
	
	// Round up to nearest multiple of 4 bytes, to pad with zeros
	uint32 aNewOffset = align(aLength);
	if (aNewOffset > mCapacity)
		grow(aNewOffset);

	if (mAddress == NULL)
		CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);

	memcpy(mAddress + inOffset, inData, inLength);

	for (uint32 anOffset = aLength; anOffset < aNewOffset; anOffset++)
		mAddress[anOffset] = 0;

	return aNewOffset;
}



void WriteSection::grow(size_t inNewCapacity)
{
	size_t n = CheckUInt32Multiply(mCapacity, 2);
	size_t aNewCapacity = max(n, inNewCapacity);
	mAddress = reinterpret_cast<uint8 *>(mAllocator.realloc(mAddress, aNewCapacity));
	memset(mAddress + mCapacity, 0, aNewCapacity - mCapacity);
	mCapacity = aNewCapacity;
}
