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
 *  CACRecord.cpp
 *  TokendMuscle
 */

#include "CACRecord.h"

#include "CACError.h"
#include "CACToken.h"
#include "Attribute.h"
#include "MetaAttribute.h"
#include "MetaRecord.h"
#include <security_cdsa_client/aclclient.h>
#include <Security/SecKey.h>

#include <zlib.h>

//
// CACRecord
//
CACRecord::~CACRecord()
{
}


//
// CACCertificateRecord
//
CACCertificateRecord::~CACCertificateRecord()
{
}

#define CAC_MAXSIZE_CERT           4000

Tokend::Attribute *CACCertificateRecord::getDataAttribute(Tokend::TokenContext *tokenContext)
{
	CACToken &cacToken = dynamic_cast<CACToken &>(*tokenContext);
	CssmData data;
	if (cacToken.cachedObject(0, mDescription, data))
	{
		Tokend::Attribute *attribute =
			new Tokend::Attribute(data.Data, data.Length);
		free(data.Data);
		return attribute;
	}

	unsigned char command[] = { 0x80, 0x36, 0x00, 0x00, 0x64 };
	unsigned char result[MAX_BUFFER_SIZE];
	size_t resultLength = sizeof(result);
	uint8 certificate[CAC_MAXSIZE_CERT];
	uint8 uncompressed[CAC_MAXSIZE_CERT];
	size_t certificateLength = 0;

	try
	{
		PCSC::Transaction _(cacToken);
		cacToken.select(mApplication);
		uint32_t cacreturn;
		do
		{
			cacreturn = cacToken.exchangeAPDU(command, sizeof(command), result,
				resultLength);

			if ((cacreturn & 0xFF00) != 0x6300)
				CACError::check(cacreturn);

			size_t requested = command[4];
			if (resultLength != requested + 2)
                PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);

			memcpy(certificate + certificateLength, result, resultLength - 2);
			certificateLength += resultLength - 2;
			// Number of bytes to fetch next time around is in the last byte
			// returned.
			command[4] = cacreturn & 0xFF;
		} while ((cacreturn & 0xFF00) == 0x6300);
	}
	catch (...)
	{
		return NULL;
	}
	
	if (certificate[0] == 1)
	{
		/* The certificate is compressed */
		secdebug("cac", "uncompressing compressed %s", mDescription);
		size_t uncompressedLength = sizeof(uncompressed);
		int rv = uncompress(uncompressed, &uncompressedLength, certificate + 1,
			certificateLength - 1);
		if (rv != Z_OK)
		{
			secdebug("zlib", "uncompressing %s failed: %d", mDescription, rv);
			CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
		}

		data.Data = uncompressed;
		data.Length = uncompressedLength;
	}
	else
	{
		data.Data = certificate;
		data.Length = certificateLength;
	}

	cacToken.cacheObject(0, mDescription, data);
	return new Tokend::Attribute(data.Data, data.Length);
}


//
// CACKeyRecord
//
CACKeyRecord::CACKeyRecord(const unsigned char *application,
	const char *description, const Tokend::MetaRecord &metaRecord) :
    CACRecord(application, description)
{
	// Allow all keys to decrypt, unwrap, sign
    attributeAtIndex(metaRecord.metaAttribute(kSecKeyDecrypt).attributeIndex(),
                     new Tokend::Attribute(true));
    attributeAtIndex(metaRecord.metaAttribute(kSecKeyUnwrap).attributeIndex(),
                     new Tokend::Attribute(true));
    attributeAtIndex(metaRecord.metaAttribute(kSecKeySign).attributeIndex(),
                     new Tokend::Attribute(true));
}

CACKeyRecord::~CACKeyRecord()
{
}

void CACKeyRecord::computeCrypt(CACToken &cacToken, bool sign,
	const unsigned char *data, size_t dataLength, unsigned char *output,
	size_t &outputLength)
{
	if (dataLength > sizeInBits() / 8)
		CssmError::throwMe(CSSMERR_CSP_BLOCK_SIZE_MISMATCH);

	PCSC::Transaction _(cacToken);
	cacToken.select(mApplication);
	size_t apduSize = dataLength + 5;
	unsigned char apdu[apduSize];
	size_t resultLength = sizeInBits() / 8 + 2;
	unsigned char result[resultLength];

	apdu[0] = 0x80;
	apdu[1] = 0x42;
	apdu[2] = 0x00;
	apdu[3] = 0x00;
	apdu[4] = dataLength;
	memcpy(apdu + 5, data, dataLength);
	CACError::check(cacToken.exchangeAPDU(apdu, apduSize, result,
		resultLength));
	if (resultLength != sizeInBits() / 8 + 2)
	{
		secdebug("cac", " %s: computeCrypt: expected size: %ld, got: %ld",
			mDescription, sizeInBits() / 8 + 2, resultLength);
		PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
	}

	if (outputLength < resultLength - 2)
		CssmError::throwMe(CSSMERR_CSP_BLOCK_SIZE_MISMATCH);

	outputLength = resultLength - 2;
	memcpy(output, result, outputLength);
}

void CACKeyRecord::getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls)
{
	if (!mAclEntries) {
		mAclEntries.allocator(Allocator::standard());
        // Anyone can read the DB record for this key (which is a reference
		// CSSM_KEY)
		mAclEntries.add(CssmClient::AclFactory::AnySubject(
			mAclEntries.allocator()),
			AclAuthorizationSet(CSSM_ACL_AUTHORIZATION_DB_READ, 0));

		// Using this key to sign or decrypt will require PIN1
		char tmptag[20];
		const uint32 slot = 1;	// hardwired for now, but...
		snprintf(tmptag, sizeof(tmptag), "PIN%d", slot);
		mAclEntries.add(CssmClient::AclFactory::PinSubject(
			mAclEntries.allocator(), 1),
			AclAuthorizationSet(CSSM_ACL_AUTHORIZATION_SIGN, CSSM_ACL_AUTHORIZATION_DECRYPT, 0),
			tmptag);
	}
	count = mAclEntries.size();
	acls = mAclEntries.entries();
}

//
// CACTBRecord
//
CACTBRecord::~CACTBRecord()
{
}

void 
CACTBRecord::getSize(CACToken &cacToken, size_t &tbsize, size_t &vbsize)
{
	cacToken.select(mApplication);
	unsigned char apdu[] = { 0x80, 0x56, 0x00, 0x00, 0x2E };
	unsigned char result[MAX_BUFFER_SIZE];
	size_t resultLength = sizeof(result);
	uint32_t cacresult = cacToken.exchangeAPDU(apdu, sizeof(apdu), result,
		resultLength);
    if ((cacresult & 0x6C00) == 0x6C00 && (cacresult & 0xFF) > 0x1E)
    {
        /* We requested the wrong length, try again */
        apdu[4] = cacresult & 0xFF;
        resultLength = sizeof(result);
        cacresult = cacToken.exchangeAPDU(apdu, sizeof(apdu), result,
			resultLength);
    }

    CACError::check(cacresult);

    if (resultLength - 2 != apdu[4])
		PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);

    CACError::check(result[resultLength - 2] << 8 + result[resultLength - 1]);

    tbsize = result[0x1C] + (result[0x1D] << 8);
    vbsize = result[0x1E] + (result[0x1F] << 8);
}

#define MAX_READ 0xFF	// 200 redefine to avoid SCardTransmitExt -- was 0xFF

#if 0
		// With extended APDUs, we can get another 0x61xx result
		if (resultLength == 2 && result[0] == 0x61)
		{
			apdusize = 5;
			apdu[0] = 0x00; apdu[1] = 0xC0; apdu[2] = 0x00; apdu[3] = 0x00; apdu[4] = result[1];
			continue;
		}
#endif

/*
	See NIST IR 6887 Ð 2003 EDITION, GSC-IS VERSION 2.1
	5.3.4 Generic Container Provider Virtual Machine Card Edge Interface
	for a description of how this command works
	
	READ BUFFER 0x80 0x52 Off/H Off/L 0x02 <buffer & number bytes to read> Ð 
*/

Tokend::Attribute *CACTBRecord::getDataAttribute(CACToken &cacToken,
	bool getTB)
{
    size_t size, tbsize, vbsize;
	cacToken.select(mApplication);
	size_t resultLength;

	PCSC::Transaction _(cacToken);
	getSize(cacToken, tbsize, vbsize);
	size = getTB ? tbsize : vbsize;

    unsigned char outputData[size + 2];
    unsigned int offset, bytes_left;
	
    for (offset = 0, bytes_left = size; bytes_left;)
    {
    //    resultLength = size + 2 - offset;
        unsigned char toread = bytes_left > MAX_READ ? MAX_READ : bytes_left;
		unsigned char apdu[] = { 0x80, 0x52,
				offset >> 8, offset & 0xFF,
				0x02, (getTB ? 0x01 : 0x02),
				toread };
		resultLength = toread + 2;
        uint32_t cacresult = cacToken.exchangeAPDU(apdu, sizeof(apdu),
                                                   outputData + offset,
												   resultLength);

        CACError::check(cacresult);

        if (resultLength - 2 != toread)
			PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);

        resultLength -= 2;
        offset += resultLength;
        bytes_left -= resultLength;
    }

    return new Tokend::Attribute(outputData, offset);
}

#if 0
Tokend::Attribute *CACTBRecord::getDataAttribute(CACToken &cacToken, bool getTB)
{
    size_t size, tbsize, vbsize;
	cacToken.select(mApplication);
	size_t resultLength;

	PCSC::Transaction _(cacToken);
	getSize(cacToken, tbsize, vbsize);
	size = getTB ? tbsize : vbsize;

	CssmData data;
	
	cacToken.getDataCore(mApplication, mApplicationSize, mDescription, mIsCertificate, mAllowCaching, data);
	
	return new Tokend::Attribute(data.Data, data.Length);
}
#endif

Tokend::Attribute *CACTBRecord::getDataAttribute(Tokend::TokenContext *tokenContext)
{
	CACToken &cacToken = dynamic_cast<CACToken &>(*tokenContext);
	return getDataAttribute(cacToken, true);
}


//
// CACVBRecord
//
CACVBRecord::~CACVBRecord()
{
}

Tokend::Attribute *CACVBRecord::getDataAttribute(Tokend::TokenContext *tokenContext)
{
	CACToken &cacToken = dynamic_cast<CACToken &>(*tokenContext);
	return CACTBRecord::getDataAttribute(cacToken, false);
}

void CACVBRecord::getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls)
{
	if (!mAclEntries) {
		mAclEntries.allocator(Allocator::standard());
        // Reading this objects data requires PIN1
		mAclEntries.add(CssmClient::AclFactory::PinSubject(
			mAclEntries.allocator(), 1),
			AclAuthorizationSet(CSSM_ACL_AUTHORIZATION_DB_READ, 0));
	}
	count = mAclEntries.size();
	acls = mAclEntries.entries();
}

