/* Copyright (c) 1998,2011,2014 Apple Inc.  All Rights Reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 *
 * feeCipherFile.c
 *
 * Revision History
 * ----------------
 * 10/06/98		ap
 *	Changed to compile with C++.
 * 05 Feb 97 at Apple
 *	Modified to use portable byte representation.
 * 23 Oct 96 at NeXT
 *	Created.
 */

#include "feeCipherFile.h"
#include "falloc.h"
#include "platform.h"
#include "feeDebug.h"
#include "byteRep.h"

#ifndef	NULL
#define NULL ((void *)0)
#endif	/* NULL */

/*
 * These must match constants of same name in CipherFileAtom.java.
 */
#define CFILE_MAGIC		0xfeecf111
#define CFILE_VERSION		1
#define CFILE_MIN_VERSION	1

/*
 * Format of a feeCipherFile header.
 * Offsets and lengths refer to locations of components in cFileInst.dataRep.
 * This struct appears at the start of a feeCipherFile data representation.
 */
typedef struct {
	unsigned		magic;
	unsigned		version;
	unsigned		minVersion;
	unsigned		totalLength;		// equals dataRepLen
	cipherFileEncrType	encrType;
	unsigned		cipherTextOffset;	// offset of ciphertext
	unsigned		cipherTextLen;		// in bytes
	unsigned		sendPubKeyDataOffset;	// optional
	unsigned		sendPubKeyDataLen;
	unsigned		otherKeyDataOffset;	// optional
	unsigned		otherKeyDataLen;
	unsigned		sigDataOffset;		// optional
	unsigned		sigDataLen;		// 0 means no signature
	unsigned		userData;
} cFileHeader;

/*
 * Private data, represented by a feeCipherFile handle.
 */
typedef struct {
	cFileHeader 	header;
	unsigned char 	*dataRep;		// raw data
	unsigned 	dataRepLen;
} cFileInst;

static unsigned lengthOfByteRepCfileHdr(void);
static unsigned cfileHdrToByteRep(cFileHeader *hdr,
	unsigned char *s);
static void byteRepToCfileHdr(const unsigned char *s,
	cFileHeader *hdr);


/*
 * alloc, free cFileInst
 */
static cFileInst *cFileInstAlloc()
{
	cFileInst *cfinst = (cFileInst *) fmalloc(sizeof(cFileInst));

	bzero(cfinst, sizeof(cFileInst));
	return cfinst;
}

static void cFileInstFree(cFileInst *cfinst)
{
	if(cfinst->dataRep) {
		ffree(cfinst->dataRep);
	}
	ffree(cfinst);
}

/*
 * Alloc and return a new feeCipherFile object associated with the specified
 * data.
 */
feeCipherFile feeCFileNewFromCipherText(cipherFileEncrType encrType,
	const unsigned char *cipherText,
	unsigned cipherTextLen,
	const unsigned char *sendPubKeyData,	// optional
	unsigned sendPubKeyDataLen,		// 0 if sendPubKeyData is NULL
	const unsigned char *otherKeyData,	// optional
	unsigned otherKeyDataLen,		// 0 if otherKeyData is NULL
	const unsigned char *sigData,	// optional; NULL means no signature
	unsigned sigDataLen,		// 0 if sigData is NULL
	unsigned userData)		// for caller's convenience
{
	cFileInst *cfinst;
	cFileHeader *header;
	unsigned char *data;

	if(cipherTextLen == 0) {
		return NULL;
	}
	cfinst = cFileInstAlloc();
	header = &cfinst->header;

	/*
	 * Init the header.
	 */
	header->magic 		  = CFILE_MAGIC;
	header->version 	  = CFILE_VERSION;
	header->minVersion	  = CFILE_MIN_VERSION;
	header->totalLength	  = lengthOfByteRepCfileHdr() + cipherTextLen +
				    sendPubKeyDataLen + otherKeyDataLen +
				    sigDataLen;
	header->encrType	     = encrType;
	header->cipherTextOffset     = lengthOfByteRepCfileHdr();
	header->cipherTextLen        = cipherTextLen;
	header->sendPubKeyDataOffset = header->cipherTextOffset +
				       cipherTextLen;
	header->sendPubKeyDataLen    = sendPubKeyDataLen;
	header->otherKeyDataOffset   = header->sendPubKeyDataOffset +
				       sendPubKeyDataLen;
	header->otherKeyDataLen      = otherKeyDataLen;
	header->sigDataOffset	     = header->otherKeyDataOffset +
				       otherKeyDataLen;
	header->sigDataLen	     = sigDataLen;
	header->userData	     = userData;

	/*
	 * Alloc a data representation, copy various components to it.
	 */
	cfinst->dataRepLen = header->totalLength;
	data = cfinst->dataRep = (unsigned char*) fmalloc(cfinst->dataRepLen);
	cfileHdrToByteRep(header, data);

	data = cfinst->dataRep + header->cipherTextOffset;
	bcopy(cipherText, data, cipherTextLen);
	if(sendPubKeyDataLen) {
		data = cfinst->dataRep + header->sendPubKeyDataOffset;
		bcopy(sendPubKeyData, data, sendPubKeyDataLen);
	}
	if(otherKeyDataLen) {
		data = cfinst->dataRep + header->otherKeyDataOffset;
		bcopy(otherKeyData, data, otherKeyDataLen);
	}
	if(sigDataLen) {
		data = cfinst->dataRep + header->sigDataOffset;
		bcopy(sigData, data, sigDataLen);
	}
	return (feeCipherFile)cfinst;
}

/*
 * Obtain the contents of a feeCipherFile as a byte stream.
 */
feeReturn feeCFileDataRepresentation(feeCipherFile cipherFile,
	const unsigned char **dataRep,
	unsigned *dataRepLen)
{
	cFileInst *cfinst = (cFileInst *)cipherFile;

	if(cfinst->dataRepLen == 0) {
		*dataRep = NULL;
		*dataRepLen = 0;
		return FR_BadCipherFile;
	}
	*dataRep = (unsigned char*) fmallocWithData(cfinst->dataRep, cfinst->dataRepLen);
	*dataRepLen = cfinst->dataRepLen;
	return FR_Success;
}

/*
 * Alloc and return a new feeCipherFile object, given a byte stream (originally
 * obtained from feeCFDataRepresentation()).
 */
feeReturn feeCFileNewFromDataRep(const unsigned char *dataRep,
	unsigned dataRepLen,
	feeCipherFile *cipherFile)	// RETURNED if sucessful
{
	cFileInst *cfinst = cFileInstAlloc();
	cFileHeader *header;

	if(dataRepLen < lengthOfByteRepCfileHdr()) {
		dbgLog(("datRep too short\n"));
		goto abort;
	}
	cfinst->dataRep = (unsigned char*) fmallocWithData(dataRep, dataRepLen);
	cfinst->dataRepLen = dataRepLen;
	header = &cfinst->header;
	byteRepToCfileHdr(dataRep, header);

	/*
	 * As much consistency checking as we can manage here.
	 */
	if(header->magic != CFILE_MAGIC) {
		dbgLog(("Bad cipherFile magic number\n"));
		goto abort;
	}
	if(header->minVersion > CFILE_VERSION) {
		dbgLog(("Incompatible cipherFile version\n"));
		goto abort;
	}
	if(header->totalLength != dataRepLen) {
		dbgLog(("Bad totalLength in cipherFile header\n"));
		goto abort;
	}
	if(((header->cipherTextOffset + header->cipherTextLen) >
			header->totalLength) ||
	   ((header->sendPubKeyDataOffset + header->sendPubKeyDataLen) >
			header->totalLength) ||
	   ((header->otherKeyDataOffset + header->otherKeyDataLen) >
			header->totalLength) ||
	   ((header->sigDataOffset  + header->sigDataLen) >
			header->totalLength)) {
		dbgLog(("Bad element lengths in cipherFile header\n"));
		goto abort;
	}

	/*
	 * OK, looks good.
	 */
	*cipherFile = (feeCipherFile)cfinst;
	return FR_Success;
abort:
	cFileInstFree(cfinst);
	*cipherFile = NULL;
	return FR_BadCipherFile;
}

/*
 * Free a feeCipherFile object.
 */
void feeCFileFree(feeCipherFile cipherFile)
{
	cFileInstFree((cFileInst *)cipherFile);
}

/*
 * Given a feeCipherFile object (typically obtained from
 * feeCFileNewFromData()), obtain its constituent parts.
 *
 * Data returned must be freed by caller.
 * feeCFileSigData() may return NULL, indicating no signature present.
 */
cipherFileEncrType feeCFileEncrType(feeCipherFile cipherFile)
{
	cFileInst *cfinst = (cFileInst *)cipherFile;

	return cfinst->header.encrType;
}

unsigned char *feeCFileCipherText(feeCipherFile cipherFile,
	unsigned *cipherTextLen)
{
	cFileInst *cfinst = (cFileInst *)cipherFile;

	if(cfinst->header.cipherTextLen) {
		*cipherTextLen = cfinst->header.cipherTextLen;
		return (unsigned char*) fmallocWithData(cfinst->dataRep +
			cfinst->header.cipherTextOffset, *cipherTextLen);
	}
	else {
		dbgLog(("feeCFileCipherText: no cipherText\n"));
		*cipherTextLen = 0;
		return NULL;
	}
}

unsigned char *feeCFileSendPubKeyData(feeCipherFile cipherFile,
	unsigned *sendPubKeyDataLen)
{
	cFileInst *cfinst = (cFileInst *)cipherFile;

	if(cfinst->header.sendPubKeyDataLen) {
		*sendPubKeyDataLen = cfinst->header.sendPubKeyDataLen;
		return (unsigned char*) fmallocWithData(cfinst->dataRep +
			cfinst->header.sendPubKeyDataOffset,
			*sendPubKeyDataLen);
	}
	else {
		*sendPubKeyDataLen = 0;
		return NULL;
	}
}

unsigned char *feeCFileOtherKeyData(feeCipherFile cipherFile,
	unsigned *otherKeyDataLen)
{
	cFileInst *cfinst = (cFileInst *)cipherFile;

	if(cfinst->header.otherKeyDataLen) {
		*otherKeyDataLen = cfinst->header.otherKeyDataLen;
		return (unsigned char*) fmallocWithData(cfinst->dataRep +
			cfinst->header.otherKeyDataOffset, *otherKeyDataLen);
	}
	else {
		*otherKeyDataLen = 0;
		return NULL;
	}
}

unsigned char *feeCFileSigData(feeCipherFile cipherFile,
	unsigned *sigDataLen)
{
	cFileInst *cfinst = (cFileInst *)cipherFile;

	if(cfinst->header.sigDataLen) {
		*sigDataLen = cfinst->header.sigDataLen;
		return (unsigned char*) fmallocWithData(cfinst->dataRep +
			cfinst->header.sigDataOffset, *sigDataLen);
	}
	else {
		/*
		 * Not an error
		 */
		*sigDataLen = 0;
		return NULL;
	}
}

unsigned feeCFileUserData(feeCipherFile cipherFile)
{
	cFileInst *cfinst = (cFileInst *)cipherFile;

	return cfinst->header.userData;
}

/*
 * Convert between cFileHeader and portable byte representation.
 */

/*
 * Return size of byte rep of cFileHeader. We just happen to know that
 * this is the same size as the header....
 */
static unsigned lengthOfByteRepCfileHdr(void)
{
	return sizeof(cFileHeader);
}

static unsigned cfileHdrToByteRep(cFileHeader *hdr,
	unsigned char *s)
{
	s += intToByteRep(hdr->magic, s);
	s += intToByteRep(hdr->version, s);
	s += intToByteRep(hdr->minVersion, s);
	s += intToByteRep(hdr->totalLength, s);
	s += intToByteRep(hdr->encrType, s);
	s += intToByteRep(hdr->cipherTextOffset, s);
	s += intToByteRep(hdr->cipherTextLen, s);
	s += intToByteRep(hdr->sendPubKeyDataOffset, s);
	s += intToByteRep(hdr->sendPubKeyDataLen, s);
	s += intToByteRep(hdr->otherKeyDataOffset, s);
	s += intToByteRep(hdr->otherKeyDataLen, s);
	s += intToByteRep(hdr->sigDataOffset, s);
	s += intToByteRep(hdr->sigDataLen, s);
	s += intToByteRep(hdr->userData, s);
	return sizeof(cFileHeader);
}

#define DEC_INT(n, b)		\
	n = byteRepToInt(b);	\
	b += sizeof(int);

static void byteRepToCfileHdr(const unsigned char *s,
	cFileHeader *hdr)
{
	DEC_INT(hdr->magic, s);
	DEC_INT(hdr->version, s);
	DEC_INT(hdr->minVersion, s);
	DEC_INT(hdr->totalLength, s);
//	DEC_INT(hdr->encrType, s);
	hdr->encrType = (cipherFileEncrType) byteRepToInt(s);
	s += sizeof(int);
	DEC_INT(hdr->cipherTextOffset, s);
	DEC_INT(hdr->cipherTextLen, s);
	DEC_INT(hdr->sendPubKeyDataOffset, s);
	DEC_INT(hdr->sendPubKeyDataLen, s);
	DEC_INT(hdr->otherKeyDataOffset, s);
	DEC_INT(hdr->otherKeyDataLen, s);
	DEC_INT(hdr->sigDataOffset, s);
	DEC_INT(hdr->sigDataLen, s);
	DEC_INT(hdr->userData, s);
}
