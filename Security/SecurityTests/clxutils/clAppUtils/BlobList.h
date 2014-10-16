#ifndef	_BLOBLIST_H_
#define _BLOBLIST_H_

#include <Security/cssmtype.h>

#ifndef	NULL
#define NULL (0)
#endif

/*** BlobList class for managing groups of raw certs and CRLs ***/

class BlobList
{
public:
	BlobList()
		: mNumBlobs(0), mBlobList(NULL) { }
	~BlobList();
	/* blob is mallocd & copied; its referent is only copied if copyBlob is
	 * true and is always freed in ~BlobList */
	void addBlob(const CSSM_DATA &blob, CSSM_BOOL copyBlob = CSSM_FALSE);
	int addFile(const char *fileName, const char *dirName = NULL);	
	uint32 numBlobs() 			{ return mNumBlobs; }
	CSSM_DATA_PTR	blobList()	{ return mBlobList; }
	
private:
	uint32			mNumBlobs;
	CSSM_DATA_PTR	mBlobList;
};

#endif	/* _BLOBLIST_H_ */
