/*** BlobList class for managing groups of raw certs and CRLs ***/

#include "BlobList.h"
#include <utilLib/fileIo.h>
#include <utilLib/common.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <security_cdsa_utils/cuPem.h>

BlobList::~BlobList()
{
	for(uint32 dex=0; dex<mNumBlobs; dex++) {
		free(mBlobList[dex].Data);		// mallocd by readFile()
	}
	free(mBlobList);
	mBlobList = NULL;
	mNumBlobs = 0;
}

/* blob is mallocd & copied; its referent is not copied */
void BlobList::addBlob(const CSSM_DATA &blob, CSSM_BOOL copyBlob /* = CSSM_FALSE */)
{
	++mNumBlobs;
	mBlobList = (CSSM_DATA_PTR)realloc(mBlobList, mNumBlobs * sizeof(CSSM_DATA));
	CSSM_DATA_PTR dst = &mBlobList[mNumBlobs - 1];
	if(copyBlob) {
		/* can't use appCopyCssmData since we free with free(), not appFree() */
		dst->Length = blob.Length;
		dst->Data = (uint8 *)malloc(dst->Length);
		memmove(dst->Data, blob.Data, dst->Length);
	}
	else {
		*dst = blob;	
	}
}


int BlobList::addFile(const char *fileName,
	const char *dirName /* = NULL */)
{
	CSSM_DATA blob;
	int rtn;
	char *fullName;
	unsigned blobDataLen;
	unsigned char *blobData;
	
	if(dirName) {
		int len = strlen(dirName) + strlen(fileName) + 2;
		fullName = (char *)malloc(len);
		sprintf(fullName, "%s/%s", dirName, fileName);
	}
	else {
		fullName = (char *)fileName;
	}
	rtn = cspReadFile(fullName, &blobData, &blobDataLen);
	if(rtn) {
		printf("***Error reading file %s\n", fullName);
		if(dirName) {
			free(fullName);
		}
		return rtn;
	}
	
	/* convert from PEM to DER if appropriate */
	if(isPem(blobData, blobDataLen)) {
		unsigned derDataLen;
		unsigned char *derData;
		
		if(pemDecode(blobData, blobDataLen, &derData, &derDataLen)) {
			printf("***Error PEM-decoding file %s; using as raw data\n", fileName);
			blob.Data = blobData;
			blob.Length = blobDataLen;
		}
		else {
			blob.Data = derData;
			blob.Length = derDataLen;
			free(blobData);
		}
	}
	else {
		/* raw file data is the stuff we'll use */
		blob.Data = blobData;
		blob.Length = blobDataLen;
	}
	addBlob(blob);
	if(dirName) {
		free(fullName);
	}
	return 0;
}

