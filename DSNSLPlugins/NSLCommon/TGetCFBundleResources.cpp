
#include <CoreFoundation/CoreFoundation.h>

#include "TGetCFBundleResources.h"
#include "NSLDebugLog.h"


//----------------------------------------------------------------------------------------

TCFResources::TCFResources( CFStringRef bundleIdentifier )
{
    // CFBundleGetMainBundle() gets the bundle of the current application, not a framework
    mMainBundleRef = CFBundleGetMainBundle();

    // get the manager's bundle
    mCurrentBundleRef = CFBundleGetBundleWithIdentifier( bundleIdentifier);
//    DBGLOG( "CFBundleGetBundleWithIdentifier = %lx\n", (unsigned long)mCurrentBundleRef );
}


//----------------------------------------------------------------------------------------

TCFResources::TCFResources(CFBundleRef bundleID)
{
    mMainBundleRef = nil;
    mBundleArray = nil;
    mBundleCount = 1;
    mCurrentBundleRef = bundleID;
}


//----------------------------------------------------------------------------------------

TCFResources::~TCFResources()
{
    
}


//----------------------------------------------------------------------------------------

char *
TCFResources::GetResource(ResType type, short id, long *dataLen, CFBundleRef bundleToUse)
{
    char keyStr[256];
    char *binPtr = NULL;
    CFIndex bufferLen = kMaxResourceSize;
    unsigned char hexPtr[kMaxResourceSize];
    long locDataLen = 0;

    char *typePtr = (char*)&type;
    DBGLOG(  "GetResource %c%c%c%c %d\n", typePtr [0], typePtr [1], typePtr [2], typePtr[3], id);
    this->MakeKeyFromResInfo( type, id, 0, keyStr );
    if ( this->GetLocalizedCString( keyStr, (char *)hexPtr, &bufferLen, bundleToUse ) )
	{
        DBGLOG(  "string = %s\n", hexPtr);
    	binPtr = (char *)malloc( bufferLen/2 + 1 );
		locDataLen = this->HexStringToBinary( hexPtr, binPtr );
	}

	if ( dataLen )
		*dataLen = locDataLen;

    return binPtr;
}


//----------------------------------------------------------------------------------------

SInt32
TCFResources::GetIndString(char *theString, short id, unsigned short index, CFBundleRef bundleToUse)
{
    char keyStr[256];
    CFIndex bufferLen;

    this->MakeKeyFromResInfo( 'STR#', id, (long)index, keyStr );
   
	bufferLen = 256;
    DBGLOG( "TCFResources::GetIndString called, id:%d, index:%d, keyStr:%s\n", id, index, keyStr );
    this->GetLocalizedCString( keyStr, theString, &bufferLen, bundleToUse );

    return (SInt32)bufferLen;
}


//----------------------------------------------------------------------------------------
// protected methods

//----------------------------------------------------------------------------------------

void
TCFResources::MakeKeyFromResInfo(ResType type, short id, long index, char *key)
{
    char *typePtr = (char *)&type;

    switch (type)
    {
        case 'STR#':
        case 'NSLE':
            sprintf( key, "%c%c%c%c%d %ld", typePtr[0],  typePtr[1],  typePtr[2],  typePtr[3], id, index );
            break;
            
        default:
            sprintf( key, "%c%c%c%c%d", typePtr[0],  typePtr[1],  typePtr[2],  typePtr[3], id );
            break;
    }
}

//----------------------------------------------------------------------------------------
// returns the length of the data

long
TCFResources::HexStringToBinary(unsigned char *hexStr, void *data)
{
	unsigned char *dataStr = (unsigned char *)data;
	unsigned char topnibble, bottomnibble;
	
	while (*hexStr)
	{
		topnibble = (unsigned char) *hexStr++;
		topnibble -= ( topnibble > '9' ) ? ('A'-10) : '0';
		
		if ( *hexStr )
		{
			bottomnibble = (unsigned char) *hexStr++;
			bottomnibble -= ( bottomnibble > '9' ) ? ('A'-10) : '0';
		}
		else
		{
			bottomnibble = 0;
		}
		
		*dataStr++ = (topnibble << 4) | bottomnibble;
	}
	
	return (dataStr - (unsigned char*)data);
}


//----------------------------------------------------------------------------------------

Boolean
TCFResources::GetLocalizedCString(char *keyStr, char *buffer, CFIndex *bufferLen, CFBundleRef bundleToUse)
{
    CFStringRef cfKeyStr;
    CFStringRef valueStr = NULL;
    Boolean result = false;

    //fprintf(stderr, "in GetLocalizedCString\n");

    if ( bundleToUse == NULL )
        bundleToUse = mCurrentBundleRef;

    cfKeyStr = CFStringCreateWithCString( NULL, keyStr, CFStringGetSystemEncoding() );
    
    if ( cfKeyStr ) {
        valueStr = CFBundleCopyLocalizedString( bundleToUse, cfKeyStr, NULL, NULL );
    }
    
    if ( valueStr ) {
        result = CFStringGetCString( valueStr, buffer, *bufferLen, CFStringGetSystemEncoding() );
        if ( cfKeyStr == valueStr ) {
            DBGLOG( "We got our own pointer back\n");
            CFRelease( valueStr );
        }
        else {
            CFRelease( valueStr );
            CFRelease( cfKeyStr );
        }
    }
    
    // if we got our key back, then we didn't get resource data.
    if ( result && strcmp( keyStr, buffer ) == 0 )
        result = false;
    
    *bufferLen = result ? strlen( buffer ) : 0;
    
    return result;
}







