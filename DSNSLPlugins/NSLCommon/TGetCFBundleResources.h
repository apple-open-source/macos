/*
 File:	TGetCFBundleResources.h

*/

#ifndef __TGetCFBundleResources__
#define __TGetCFBundleResources__

#include <CoreServices/CoreServices.h>

#define kMaxResourceSize			2048

class TCFResources
{
public:

                            TCFResources				( CFStringRef bundleIdentifier );
                            TCFResources				( CFBundleRef bundleID );
    virtual					~TCFResources				();

    virtual char *			GetResource					(ResType type, short id, long *dataLen, CFBundleRef bundleToUse=NULL);
    virtual SInt32			GetIndString				(char *theString, short id, unsigned short index, CFBundleRef bundleToUse=NULL);
//    virtual void			GetErrorStrings				(OSStatus theErr, short id, char *errorString, char *solutionString, CFBundleRef bundleToUse);
//	virtual OSErr			GetPluginInfo				(CFBundleRef bundleToUse, NSLPluginDataPtr *theData);
    virtual	CFBundleRef		GetPluginBundleRef			( void )	 { return mCurrentBundleRef; }
protected:

	virtual void			MakeKeyFromResInfo			(ResType type, short id, long index, char *key);
    virtual long			HexStringToBinary			(unsigned char *hexStr, void *data);
    virtual Boolean			GetLocalizedCString			(char *keyStr, char *buffer, CFIndex *bufferLen, CFBundleRef bundleToUse);
    
    CFBundleRef mMainBundleRef;
    CFBundleRef mCurrentBundleRef;
	CFArrayRef mBundleArray;
	SInt16 mBundleCount;
};

#endif

