/*
	File:		CPSPlugIn.cpp

	Contains:	PasswordServer plugin implementation to interface with Directory Services

	Copyright:	© 2002 by Apple Computer, Inc., all rights reserved.

	NOT_FOR_OPEN_SOURCE <to be reevaluated at a later time>

*/


#include "CPSPlugIn.h"

using namespace std;


#include <DirectoryServiceCore/ServerModuleLib.h>

#include <DirectoryServiceCore/CRCCalc.h>

#include <DirectoryServiceCore/CPlugInRef.h>
#include <DirectoryServiceCore/DSCThread.h>
#include <DirectoryServiceCore/CContinue.h>
#include <DirectoryServiceCore/DSEventSemaphore.h>
#include <DirectoryServiceCore/DSMutexSemaphore.h>
#include <DirectoryServiceCore/CSharedData.h>
#include <DirectoryServiceCore/DSUtils.h>
#include <DirectoryServiceCore/PrivateTypes.h>


#define kPasswordServerPrefixStr			"/PasswordServer/"
#define kSASLListPrefix						"(SASL "
#define kPasswordServerErrPrefixStr			"-ERR "
#define kPasswordServerSASLErrPrefixStr		"SASL "

#define kChangePassPaddedBufferSize			512
#define kOneKBuffer							1024

#if 0
#define DEBUGLOG(A,args...)		CShared::LogIt( 0x0F, (A), ##args )
#else
#define DEBUGLOG(A,args...)		
#endif

#define Throw_NULL(A,B)			if ((A)==NULL) throw((sInt32)B)

extern "C" {
int getnameinfo(const struct sockaddr *, size_t, char *, size_t, char *, size_t, int);
typedef int sasl_cbproc();
int getrealm(void *context /*__attribute__((unused))*/, 
		    int id,
		    const char **availrealms,
		    const char **result);
int simple(void *context /*__attribute__((unused))*/,
		  int id,
		  const char **result,
		  unsigned *len);
int
getsecret(sasl_conn_t *conn,
	  void *context /*__attribute__((unused))*/,
	  int id,
	  sasl_secret_t **psecret);
}

// --------------------------------------------------------------------------------
//	Globals

CPlugInRef				   *gPSContextTable	= NULL;
static DSEventSemaphore	   *gKickSearchRequests	= NULL;
static DSMutexSemaphore	   *gSASLMutex = NULL;
CContinue	 			   *gContinue = NULL;

// Consts ----------------------------------------------------------------------------

static const	uInt32	kBuffPad	= 16;

static void myMemcpy ( void *s1, void *s2, size_t n );

void myMemcpy ( void *s1, void *s2, size_t n )
//adapted from NetInfo plugin code
//routine required due to Gonzo 1J compiler problem
{
	::memcpy( s1, s2, n );
}

extern "C" {
CFUUIDRef ModuleFactoryUUID = CFUUIDGetConstantUUIDWithBytes ( NULL, \
								0xF8, 0xAC, 0xD8, 0x6B, 0x3C, 0x66, 0x11, 0xD6, \
								0x93, 0x9C, 0x00, 0x03, 0x93, 0x50, 0xEB, 0x4E );

}


void writeToServer( FILE *out, char *buf );
void writeToServer( FILE *out, char *buf )
{
    DEBUGLOG( "sending: %s\n", buf);
    
    if ( buf != NULL )
    {
        fwrite(buf, strlen(buf), 1, out);
        fflush(out);
    }
}


PWServerError readFromServer( FILE *in, char *buf, unsigned long bufLen );
PWServerError readFromServer( FILE *in, char *buf, unsigned long bufLen )
{
    char readChar;
    char *tstr = buf;
    PWServerError result = {0, kPolicyError};
    int compareLen;
    
    if ( buf == nil || bufLen < 2 ) {
        result.err = -1;
        return result;
    }
    
    *buf = '\0';
    do
    {
        fscanf( in, "%c", &readChar );
        /*
        if ( isprint((unsigned char) readChar) )
            printf( "%c ", readChar );
        else
            printf( "%x ", readChar );
        */
        if ( (unsigned long)(tstr - buf) < bufLen - 1 )
            *tstr++ = readChar;
    }
    while ( readChar && readChar != '\n' );
    
    *tstr = '\0';
    //printf( "\n" );
    DEBUGLOG( "received: %s\n", buf);
    
    tstr = buf;
    compareLen = strlen(kPasswordServerErrPrefixStr);
    if ( strncmp( tstr, kPasswordServerErrPrefixStr, compareLen ) == 0 )
    {
        tstr += compareLen;
        
        // check if err is a PasswordServer or SASL error
        compareLen = strlen(kPasswordServerSASLErrPrefixStr);
        if ( strncmp(tstr, kPasswordServerSASLErrPrefixStr, compareLen) == 0 ) {
            tstr += compareLen;
            result.type = kSASLError;
        }
        
        sscanf( tstr, "%d", &result.err );
    }
    
    return result;
}


/* remove \r\n at end of the line */
static void chop(char *s)
{
    char *p;

    if (s==NULL)
        return;
    
    p = s + strlen(s) - 1;
    if (p[0] == '\n') {
        *p-- = '\0';
    }
    if (p >= s && p[0] == '\r') {
        *p-- = '\0';
    }
}

// --------------------------------------------------------------------------------
//	ConvertBinaryToHex
// --------------------------------------------------------------------------------
void ConvertBinaryToHex( const unsigned char *inData, unsigned long inLen, char *outHexStr )
{
    const unsigned char *sptr = inData;
    char *tptr = outHexStr;
    unsigned long index;
    char high, low;
    
    for ( index = 0; index < inLen; index++ )
    {
        high = (*sptr & 0xF0) >> 4;
        low = (*sptr & 0x0F);
        
        if ( high >= 0x0A )
            *tptr++ = (high - 0x0A) + 'A';
        else
            *tptr++ = high + '0';
            
        if ( low >= 0x0A )
            *tptr++ = (low - 0x0A) + 'A';
        else
            *tptr++ = low + '0';
            
        sptr++;
    }
    
    *tptr = '\0';
}


// --------------------------------------------------------------------------------
//	ConvertHexToBinary
// --------------------------------------------------------------------------------
void ConvertHexToBinary( const char *inHexStr, unsigned char *outData, unsigned long *outLen )
{
    unsigned char *tptr = outData;
    unsigned char val;
    
    while ( *inHexStr && *(inHexStr+1) )
    {
        if ( *inHexStr >= 'A' )
            val = (*inHexStr - 'A' + 0x0A) << 4;
        else
            val = (*inHexStr - '0') << 4;
        
        inHexStr++;
        
        if ( *inHexStr >= 'A' )
            val += (*inHexStr - 'A' + 0x0A);
        else
            val += (*inHexStr - '0');
        
        inHexStr++;
        
        *tptr++ = val;
    }
    
    *outLen = (tptr - outData);
}

// --------------------------------------------------------------------------------
//	ConvertBinaryTo64
//
//	Since Base64 rounds up to the nearest multiple of 4/3, prepend
//	the original length of the data to the string.
// --------------------------------------------------------------------------------
int ConvertBinaryTo64( const char *inData, unsigned long inLen, char *outHexStr )
{
    int result;
    unsigned int outLen;
    char *tempBuf;
    unsigned long bufLen = (inLen+3) * 4 / 3 + 1;
    
    tempBuf = (char *) malloc( bufLen + 1 );
    if ( tempBuf == NULL )
        return -1;
    
    result = sasl_encode64( (char *)inData, inLen, tempBuf, bufLen, &outLen );
    tempBuf[outLen] = '\0';
    sprintf( outHexStr, "{%lu}%s", inLen, tempBuf );
    
    free( tempBuf );
    
    return result;
}


// --------------------------------------------------------------------------------
//	Convert64ToBinary
// --------------------------------------------------------------------------------
int Convert64ToBinary( const char *inHexStr, char *outData, unsigned long *outLen )
{
    int result;
    unsigned int sasl_outlen;
    unsigned long attached_outlen = 0;
    const char *readPtr = inHexStr;
    
    if ( readPtr == NULL )
        return -1;
    
    // get the original length
    if ( *readPtr == '{' )
    {
        sscanf( readPtr + 1, "%lu", &attached_outlen );
        
        readPtr = strchr( readPtr, '}' );
        if ( readPtr == NULL )
            return -1;
        
        readPtr++;
    }
    
    result = sasl_decode64( readPtr, strlen(readPtr), (char *)outData, 256, &sasl_outlen );
    
    *outLen = (attached_outlen > 0) ? attached_outlen : (unsigned long)sasl_outlen;
    
    return result;
}



//-------------
int getrealm(void *context /*__attribute__((unused))*/, 
		    int id,
		    const char **availrealms,
		    const char **result)
{
    /* paranoia check */
    if (id != SASL_CB_GETREALM) return SASL_BADPARAM;
    if (!result) return SASL_BADPARAM;

    if ( availrealms ) {
        *result = *availrealms;
    }
    
    return SASL_OK;
}


int simple(void *context /*__attribute__((unused))*/,
		  int id,
		  const char **result,
		  unsigned *len)
{
    DEBUGLOG( "in simple\n");

    /* paranoia check */
    if (! result)
        return SASL_BADPARAM;
    
    *result = NULL;
    
    switch (id) {
        case SASL_CB_USER:
        case SASL_CB_AUTHNAME:
            //printf("please enter an authentication id: ");
            *result = ((sPSContextData *)context)->last.username;
            break;
            
        default:
            return SASL_BADPARAM;
    }
    
    if (*result != NULL && len != NULL)
        *len = strlen(*result);
    
    return SASL_OK;
}


int
getsecret(sasl_conn_t *conn,
	  void *context /*__attribute__((unused))*/,
	  int id,
	  sasl_secret_t **psecret)
{
    size_t len = 0;
    sasl_secret_t *xsec = NULL;
    
    DEBUGLOG( "in getsecret\n");

    /* paranoia check */
    if (! conn || ! psecret || id != SASL_CB_PASS)
        return SASL_BADPARAM;
    
    *psecret = NULL;
    
    if (((sPSContextData *)context)->last.password != NULL)
    {
        len = ((sPSContextData *)context)->last.passwordLen;
        
        xsec = (sasl_secret_t *) malloc(sizeof(sasl_secret_t) + len + 1);
    	if (xsec == NULL)
            return SASL_NOMEM;
        
        xsec->len = len;
        memcpy( xsec->data, ((sPSContextData *)context)->last.password, len );
        xsec->data[len] = '\0';
    }
    
    *psecret = xsec;
    return SASL_OK;
}


//---------

sInt32 getconn(const char *host, const char *port, int *outSocket)
{
    char servername[1024];
    struct sockaddr_in sin;
    struct hostent *hp;
    int sock = 0;
    sInt32 siResult = eDSNoErr;
    int rc;
	struct in_addr inetAddr;
	char *endPtr = NULL;
	
    if ( host==NULL || port==NULL || outSocket==NULL )
        return eParameterError;
    
    try
    {
        strncpy(servername, host, sizeof(servername) - 1);
        servername[sizeof(servername) - 1] = '\0';
        
		/* map hostname -> IP */
        rc = inet_aton(servername, &inetAddr);
        if ( rc == 1 )
		{
			sin.sin_addr.s_addr = inetAddr.s_addr;
        }
		else
		{
			if ((hp = gethostbyname(servername)) == NULL) {
				DEBUGLOG("gethostbyname");
				throw((sInt32)eDSServiceUnavailable);
			}
			memcpy(&sin.sin_addr, hp->h_addr, hp->h_length);
        }
        
		/* map port -> num */
		sin.sin_port = htons(strtol(port, &endPtr, 10));
        if ((sin.sin_port == 0) || (endPtr == port)) {
			DEBUGLOG( "port '%s' unknown\n", port);
			throw((sInt32)eParameterError);
		}
		
        sin.sin_family = AF_INET;
        
        /* connect */
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            DEBUGLOG("socket");
            throw((sInt32)eDSServiceUnavailable);
        }
        
        if (connect(sock, (struct sockaddr *) &sin, sizeof (sin)) < 0) {
            DEBUGLOG("connect");
            throw((sInt32)eDSServiceUnavailable);
        }
    }
    
    catch( sInt32 error )
    {
        siResult = error;
    }
    
    *outSocket = sock;
    
    return siResult;
}


static CDSServerModule* _Creator ( void )
{
	return( new CPSPlugIn );
}

CDSServerModule::tCreator CDSServerModule::sCreator = _Creator;

// --------------------------------------------------------------------------------
//	* CPSPlugIn ()
// --------------------------------------------------------------------------------

CPSPlugIn::CPSPlugIn ( void )
{        
	fState = kUnknownState;
    fHasInitializedSASL = false;
    
    try
    {
        if ( gPSContextTable == NULL )
        {
            gPSContextTable = new CPlugInRef( CPSPlugIn::ContextDeallocProc );
            Throw_NULL( gPSContextTable, eMemoryAllocError );
        }
        
        if ( gKickSearchRequests == NULL )
        {
            gKickSearchRequests = new DSEventSemaphore();
            Throw_NULL( gKickSearchRequests, eMemoryAllocError );
        }
        
        if ( gSASLMutex == NULL )
        {
            gSASLMutex = new DSMutexSemaphore();
            Throw_NULL( gSASLMutex, eMemoryAllocError );
        }
		
		if ( gContinue == NULL )
        {
            gContinue = new CContinue( CPSPlugIn::ContinueDeallocProc );
			Throw_NULL( gContinue, eMemoryAllocError );
		}
    }
    
    catch (sInt32 err)
    {
	    DEBUGLOG( "CPSPlugIn::CPSPlugIn failed: eMemoryAllocError\n");
        throw( err );
    }
} // CPSPlugIn


// --------------------------------------------------------------------------------
//	* ~CPSPlugIn ()
// --------------------------------------------------------------------------------

CPSPlugIn::~CPSPlugIn ( void )
{
    // TODO clean up the gPSContextTable here
    
} // ~CPSPlugIn


// --------------------------------------------------------------------------------
//	* Validate ()
// --------------------------------------------------------------------------------

sInt32 CPSPlugIn::Validate ( const char *inVersionStr, const uInt32 inSignature )
{
	fSignature = inSignature;

	return( noErr );

} // Validate


// --------------------------------------------------------------------------------
//	* Initialize ()
// --------------------------------------------------------------------------------

sInt32 CPSPlugIn::Initialize ( void )
{
    sInt32 siResult = eDSNoErr;
	
	//TODO setup what needs to be set up to accept opendirnode calls
	
	// set the active and initted flags
	fState = kUnknownState;
	fState += kInitalized;
	fState += kActive;
	
	WakeUpRequests();

	return( siResult );

} // Initialize


// --------------------------------------------------------------------------------
//	* FillWithRandomData ()
// --------------------------------------------------------------------------------
void CPSPlugIn::FillWithRandomData( char *inBuffer, uInt32 inLen )
{
}

    
// --------------------------------------------------------------------------------
//	* SetPluginState ()
// --------------------------------------------------------------------------------

sInt32 CPSPlugIn::SetPluginState ( const uInt32 inState )
{

// don't allow any changes other than active / in-active

	if (kActive & inState) //want to set to active
    {
		if (fState & kActive) //if already active
		{
			//TODO ???
		}
		else
		{
			//call to Init so that we re-init everything that requires it
			Initialize(); //TODO method needs to be re-entrant
		}
    }

	if (kInactive & inState) //want to set to in-active
    {

		//TODO do something
		
        if (!(fState & kInactive))
        {
            fState += kInactive;
        }
        if (fState & kActive)
        {
            fState -= kActive;
        }
    }

	return( eDSNoErr );

} // SetPluginState


//--------------------------------------------------------------------------------------------------
//	* WakeUpRequests() (static)
//
//--------------------------------------------------------------------------------------------------

void CPSPlugIn::WakeUpRequests ( void )
{
	gKickSearchRequests->Signal();
} // WakeUpRequests


// ---------------------------------------------------------------------------
//	* WaitForInit
//
// ---------------------------------------------------------------------------

void CPSPlugIn::WaitForInit ( void )
{
	volatile	uInt32		uiAttempts	= 0;

	if (!(fState & kActive))
	{
	while ( !(fState & kInitalized) &&
			!(fState & kFailedToInit) )
	{
		try
		{
			// Try for 2 minutes before giving up
			if ( uiAttempts++ >= 240 )
			{
				return;
			}

			// Now wait until we are told that there is work to do or
			//	we wake up on our own and we will look for ourselves

			gKickSearchRequests->Wait( (uInt32)(.5 * kMilliSecsPerSec) );
            
			try
			{
				gKickSearchRequests->Reset();
			}

			catch( long err )
			{
			}
		}

		catch( long err1 )
		{
		}
	}
	}//NOT already Active
} // WaitForInit


// ---------------------------------------------------------------------------
//	* ProcessRequest
//
// ---------------------------------------------------------------------------

sInt32 CPSPlugIn::ProcessRequest ( void *inData )
{
	sInt32		siResult	= 0;

	if ( inData == NULL )
	{
		return( ePlugInDataError );
	}
    
    WaitForInit();

	if ( (fState & kFailedToInit) )
	{
        return( ePlugInFailedToInitialize );
	}

	//TODO - recheck whether we allow access to plugin even when inactive
	//likely not an issue since HI will not control this plugin ie. always active
	if ( ((fState & kInactive) || !(fState & kActive))
		  && (((sHeader *)inData)->fType != kDoPlugInCustomCall)
		  && (((sHeader *)inData)->fType != kOpenDirNode) )
	{
        return( ePlugInNotActive );
	}
    
	if ( ((sHeader *)inData)->fType == kHandleNetworkTransition )
	{
		siResult = Initialize(); //TODO this is one option - otherwise call something else
	}
	else
	{
		siResult = HandleRequest( inData );
	}

	return( siResult );

} // ProcessRequest



// ---------------------------------------------------------------------------
//	* HandleRequest
//
// ---------------------------------------------------------------------------

sInt32 CPSPlugIn::HandleRequest ( void *inData )
{
	sInt32	siResult	= 0;
	sHeader	*pMsgHdr	= NULL;

	if ( inData == NULL )
	{
		return( -8088 );
	}

	pMsgHdr = (sHeader *)inData;

	switch ( pMsgHdr->fType )
	{
		case kOpenDirNode:
			siResult = OpenDirNode( (sOpenDirNode *)inData );
			break;
			
		case kCloseDirNode:
			siResult = CloseDirNode( (sCloseDirNode *)inData );
			break;
			
		case kGetDirNodeInfo:
			siResult = GetDirNodeInfo( (sGetDirNodeInfo *)inData );
			break;
			
		case kGetAttributeEntry:
			siResult = GetAttributeEntry( (sGetAttributeEntry *)inData );
			break;
			
		case kGetAttributeValue:
			siResult = GetAttributeValue( (sGetAttributeValue *)inData );
			break;
			
		case kCloseAttributeList:
			siResult = CloseAttributeList( (sCloseAttributeList *)inData );
			break;

		case kCloseAttributeValueList:
			siResult = CloseAttributeValueList( (sCloseAttributeValueList *)inData );
			break;

		case kDoDirNodeAuth:
			siResult = DoAuthentication( (sDoDirNodeAuth *)inData );
			break;
			
		case kDoPlugInCustomCall:
			siResult = DoPlugInCustomCall( (sDoPlugInCustomCall *)inData );
			break;
			
		default:
			siResult = eNotHandledByThisNode;
			break;
	}

	pMsgHdr->fResult = siResult;

	return( siResult );

} // HandleRequest


//------------------------------------------------------------------------------------
//	* ReleaseContinueData
//------------------------------------------------------------------------------------

sInt32 CPSPlugIn::ReleaseContinueData ( sReleaseContinueData *inData )
{
	sInt32	siResult	= eDSNoErr;
    
	// RemoveItem calls our ContinueDeallocProc to clean up
	if ( gContinue->RemoveItem( inData->fInContinueData ) != eDSNoErr )
	{
		siResult = eDSInvalidContext;
	}
    
	return( siResult );

} // ReleaseContinueData


//------------------------------------------------------------------------------------
//	* OpenDirNode
//------------------------------------------------------------------------------------

sInt32 CPSPlugIn::OpenDirNode ( sOpenDirNode *inData )
{
	sInt32				siResult		= eDSNoErr;
    tDataListPtr		pNodeList		= NULL;
	char			   *pathStr			= NULL;
    char			   *psName			= NULL;
    char			   *subStr			= NULL;
	sPSContextData	   *pContext		= NULL;

    pNodeList	=	inData->fInDirNodeName;
    
    DEBUGLOG( "CPSPlugIn::OpenDirNode \n");
    
	try
	{
		if ( inData != NULL )
		{
			pathStr = dsGetPathFromListPriv( pNodeList, (char *)"/" );
            Throw_NULL( pathStr, eDSNullNodeName );
            
            DEBUGLOG( "CPSPlugIn::OpenDirNode path = %s\n", pathStr);

            unsigned int prefixLen = strlen(kPasswordServerPrefixStr);
            
            //special case for the configure PS node?
            if (::strcmp(pathStr,"/PasswordServer") == 0)
            {
                // set up the context data now with the relevant parameters for the configure PasswordServer node
                // DS API reference number is used to access the reference table
                pContext = MakeContextData();
                pContext->psName = new char[1+::strlen("PasswordServer Configure")];
                ::strcpy(pContext->psName,"PasswordServer Configure");
                // add the item to the reference table
                gPSContextTable->AddItem( inData->fOutNodeRef, pContext );
            }
            // check that there is something after the delimiter or prefix
            // strip off the PasswordServer prefix here
            else
            if ( (strlen(pathStr) > prefixLen) && (::strncmp(pathStr,kPasswordServerPrefixStr,prefixLen) == 0) )
            {
                int result;
                unsigned count;
                char *tptr, *end;
                int salen;
                
                subStr = pathStr + prefixLen;
                
                if ( strncmp( subStr, "ipv4/", 5 ) == 0 )
                    subStr += 5;
                else
                if ( strncmp( subStr, "ipv6/", 5 ) == 0 )
                    subStr += 5;
                else
                if ( strncmp( subStr, "dns/", 4 ) == 0 )
                    subStr += 4;
                
                psName = (char *) calloc( 1, strlen(subStr) );
                Throw_NULL( psName, eDSNullNodeName );
                    
                ::strcpy( psName, subStr );
				
                pContext = MakeContextData();
                pContext->psName = psName;
				
                char *portNumStr = strchr( pContext->psName, ':' );
				if ( portNumStr != NULL )
				{
					*portNumStr = '\0';
					strncpy(pContext->psPort, portNumStr+1, 10);
                    pContext->psPort[9] = '\0';
				}
				else
				{
					strcpy(pContext->psPort, "106");
				}
				
                // 2.x
                char hbuf[NI_MAXHOST], pbuf[NI_MAXSERV];
                struct sockaddr_storage local_ip, remote_ip;
                char buf[4096];
                
                if ( !fHasInitializedSASL )
                {
                    gSASLMutex->Wait();
                    result = sasl_client_init(NULL);
                    gSASLMutex->Signal();
                    
                    if ( result != SASL_OK ) {
                        DEBUGLOG( "sasl_client_init failed.\n");
                        return eDSOpenNodeFailed;
                    }
                    
                    fHasInitializedSASL = true;
                }
                
                // connect to remote server
                siResult = ConnectToServer( pContext );
                if ( siResult != eDSNoErr )
                    throw( siResult );
                
                // set ip addresses
                salen = sizeof(local_ip);
                if (getsockname(pContext->fd, (struct sockaddr *)&local_ip, &salen) < 0) {
                    DEBUGLOG("getsockname");
                }
                
                getnameinfo((struct sockaddr *)&local_ip, salen,
                            hbuf, sizeof(hbuf), pbuf, sizeof(pbuf),
                            NI_NUMERICHOST | NI_WITHSCOPEID | NI_NUMERICSERV);
                snprintf(pContext->localaddr, sizeof(pContext->localaddr), "%s;%s", hbuf, pbuf);
            
                salen = sizeof(remote_ip);
                if (getpeername(pContext->fd, (struct sockaddr *)&remote_ip, &salen) < 0) {
                    DEBUGLOG("getpeername");
                }
				
                getnameinfo((struct sockaddr *)&remote_ip, salen,
                            hbuf, sizeof(hbuf), pbuf, sizeof(pbuf),
                            NI_NUMERICHOST | NI_WITHSCOPEID | NI_NUMERICSERV);
                snprintf(pContext->remoteaddr, sizeof(pContext->remoteaddr), "%s;%s", hbuf, pbuf);
                
                // retrieve the password server's public RSA key
				siResult = this->GetRSAPublicKey(pContext);
				if ( siResult != eDSNoErr ) {
					DEBUGLOG( "rsapublic = %l\n", siResult);
					throw(siResult);
				}
				
                // retrieve the password server's list of available auth methods
                writeToServer(pContext->serverOut, "LIST\r\n");
                readFromServer(pContext->serverIn, buf, 4096);
                chop(buf);
                tptr = buf;
                for (count=0; tptr; count++ ) {
                    tptr = strchr( tptr, ' ' );
                    if (tptr) tptr++;
                }
                
                if (count > 0) {
                    pContext->mech = (AuthMethName *)calloc(count, sizeof(AuthMethName));
                    Throw_NULL( pContext->mech, eMemoryAllocError );
                    
                    pContext->mechCount = count;
                }
                
                tptr = strstr( buf, kSASLListPrefix );
                if ( tptr )
                {
                    tptr += strlen( kSASLListPrefix );
                    
                    for ( ; tptr && count > 0; count-- )
                    {
                        if ( *tptr == '\"' )
                            tptr++;
                        else
                            break;
                        
                        end = strchr( tptr, '\"' );
                        if ( end != NULL )
                            *end = '\0';
                            
                        strcpy( pContext->mech[count-1].method, tptr );
						DEBUGLOG( "mech=%s\n", tptr);
                        
                        tptr = end;
                        if ( tptr != NULL )
                            tptr += 2;
                    }
                }

                
                // add the item to the reference table
                gPSContextTable->AddItem( inData->fOutNodeRef, pContext );
            } // there was some name passed in here ie. length > 1
            else
            {
                siResult = eDSOpenNodeFailed;
            }

        } // inData != NULL
	} // try
	catch( sInt32 err )
	{
		siResult = err;
		if (pContext != NULL)
		{
			gPSContextTable->RemoveItem( inData->fOutNodeRef );
		}
	}
	
	if (pathStr != NULL)
	{
		delete( pathStr );
		pathStr = NULL;
	}

	return( siResult );

} // OpenDirNode

//------------------------------------------------------------------------------------
//	* CloseDirNode
//------------------------------------------------------------------------------------

sInt32 CPSPlugIn::CloseDirNode ( sCloseDirNode *inData )
{
	sInt32				siResult	= eDSNoErr;
	sPSContextData	   *pContext	= NULL;
    char				buf[kOneKBuffer];
    
	try
	{
		pContext = (sPSContextData *)gPSContextTable->GetItemData( inData->fInNodeRef );
        Throw_NULL( pContext, eDSBadContextData );
		
		// do whatever to close out the context
        writeToServer(pContext->serverOut, "QUIT\r\n");
        readFromServer( pContext->serverIn, buf, kOneKBuffer );
        
		this->CleanContextData( pContext );
        
		gPSContextTable->RemoveItem( inData->fInNodeRef );
        gContinue->RemoveItems( inData->fInNodeRef );
	}
    
	catch( sInt32 err )
	{
		siResult = err;
	}
    
	return( siResult );

} // CloseDirNode


// ---------------------------------------------------------------------------
//	* ConnectToServer
// ---------------------------------------------------------------------------

sInt32 CPSPlugIn::ConnectToServer( sPSContextData *inContext )
{
    sInt32 siResult = eDSNoErr;
    char buf[kOneKBuffer];
    
    // connect to remote server
    siResult = getconn(inContext->psName, inContext->psPort, &inContext->fd);
    if ( siResult != eDSNoErr )
        return( siResult );
    
    inContext->serverIn = fdopen(inContext->fd, "r");
    inContext->serverOut = fdopen(inContext->fd, "w");
    
    // yank "hi there" text
    readFromServer(inContext->serverIn, buf, kOneKBuffer);
    
    return siResult;
}

// ----------------------------------------------------------------------------
//	* Connected
//
//	Is the socket connection still open?
// ----------------------------------------------------------------------------

Boolean CPSPlugIn::Connected ( sPSContextData *inContext )
{
	int		bytesReadable = 0;
	char	temp[1];

	bytesReadable = ::recvfrom( inContext->fd, temp, sizeof (temp), (MSG_DONTWAIT | MSG_PEEK), NULL, NULL );
	
	DEBUGLOG( "CPSPlugIn::Connected bytesReadable = %d, errno = %d", bytesReadable, errno );
	
	if ( bytesReadable == -1 )
	{
		// The problem here is that the plug-in is multi-threaded and it is
		// too likely that errno could be changed before it can be read.
		// However, since a disconnected socket typically returns bytesReadable==0,
		// it is more or less safe to assume that -1 means the socket is still open.
		
		switch ( errno )
		{
			case EAGAIN:
				// no data in the socket but socket is still open and connected
				return true;
				break;
			
			case EBADF:
			case ENOTCONN:
			case ENOTSOCK:
			case EINTR:
			case EFAULT:
				// valid failing error
				return false;
				break;
			
			default:
				// invalid error
				return true;
				break;
		}
	}
	
	// recvfrom() only returns 0 when the peer has closed the connection (read an EOF)
	if ( bytesReadable == 0 )
	{
		return( false );
	}

	return( true );

} // Connected


// ---------------------------------------------------------------------------
//	* GetRSAPublicKey
// ---------------------------------------------------------------------------

sInt32 CPSPlugIn::GetRSAPublicKey( sPSContextData *inContext )
{
	sInt32				siResult			= eDSNoErr;
	PWServerError		serverResult;
    char				buf[kOneKBuffer];
    char				*keyStr;
    int					bits				= 0;
    
	try
	{
        Throw_NULL( inContext, eDSBadContextData );
        
		// get string
        writeToServer( inContext->serverOut, "RSAPUBLIC\r\n" );
        serverResult = readFromServer( inContext->serverIn, buf, kOneKBuffer );
        if ( serverResult.err != 0 )
        {
            DEBUGLOG("no public key\n");
            throw(eDSAuthServerError);
        }
        
		chop( buf );
		inContext->rsaPublicKeyStr = (char *) calloc( 1, strlen(buf)+1 );
        Throw_NULL( inContext->rsaPublicKeyStr, eMemoryAllocError );
        		
		strcpy( inContext->rsaPublicKeyStr, buf + 4 );
		
		// get as struct
        inContext->rsaPublicKey = key_new( KEY_RSA );
        Throw_NULL( inContext->rsaPublicKey, eDSAllocationFailed );
        
        keyStr = buf + 4;
        bits = key_read(inContext->rsaPublicKey, &keyStr);
        if (bits == 0) {
            DEBUGLOG( "no key bits\n");
            throw( (sInt32)eDSAuthServerError );
        }
 	}
	
	catch( sInt32 err )
	{
		DEBUGLOG( "catch in GetRSAPublicKey = %l\n", err);
		siResult = err;
	}
    
    return siResult;
}


// ---------------------------------------------------------------------------
//	* DoRSAValidation
// ---------------------------------------------------------------------------

sInt32 CPSPlugIn::DoRSAValidation ( sPSContextData *inContext, const char *inUserKey )
{
	sInt32				siResult			= eDSNoErr;
	char				*encodedStr			= NULL;
    PWServerError		serverResult;
    char				buf[kOneKBuffer];
    BIGNUM				*nonce;
	BN_CTX				*ctx;
	char				*bnStr				= NULL;
	int					len;
    
	try
	{
        Throw_NULL( inContext, eDSBadContextData );
		
		// make sure we are talking to the right server
		if ( strcmp(inContext->rsaPublicKeyStr, inUserKey) != 0 )
			throw( (sInt32)eDSAuthServerError );
		
		// make nonce
        nonce = BN_new();
        Throw_NULL( nonce, eDSAllocationFailed );
        
        // Generate a random challenge
        BN_rand(nonce, 256, 0, 0);
        ctx = BN_CTX_new();
        BN_mod(nonce, nonce, inContext->rsaPublicKey->rsa->n, ctx);
        BN_CTX_free(ctx);
        
        bnStr = BN_bn2dec(nonce);
        BN_clear_free(nonce);
        DEBUGLOG( "nonce = %s\n", bnStr);
        
        if ( bnStr == NULL )
            throw( (sInt32)eMemoryError );
        
        int nonceLen = strlen(bnStr);
        encodedStr = (char *)malloc(kOneKBuffer);
        Throw_NULL( encodedStr, eMemoryError );
        
		len = RSA_public_encrypt(nonceLen,
								 (unsigned char *)bnStr,
								 (unsigned char *)encodedStr,
								 inContext->rsaPublicKey->rsa,
								 RSA_PKCS1_PADDING);
		
        if ( len <= 0 ) {
            DEBUGLOG( "rsa_public_encrypt() failed");
            throw( (sInt32)eDSAuthServerError );
        }
        
        if ( ConvertBinaryTo64( encodedStr, (unsigned)len, buf ) == SASL_OK )
        {
            char writeBuf[kOneKBuffer];
            UInt32 encodedStrLen;
            
            snprintf( writeBuf, kOneKBuffer, "RSAVALIDATE %s\r\n", buf );
            writeBuf[kOneKBuffer-1] = '\0';
            writeToServer( inContext->serverOut, writeBuf );
            
            serverResult = readFromServer( inContext->serverIn, buf, kOneKBuffer );
            
            if ( Convert64ToBinary( buf + 4, encodedStr, &encodedStrLen ) == SASL_OK )
            {
                encodedStr[nonceLen] = '\0';
                DEBUGLOG( "nonce = %s\n", encodedStr);
                
                if (memcmp(bnStr, encodedStr, nonceLen) != 0)
                    siResult = eDSAuthServerError;
            }
        }

 	}
    
	catch( sInt32 err )
	{
		siResult = err;
	}       
    
	if ( bnStr != NULL )
		free( bnStr );
    if ( encodedStr != NULL )
        free( encodedStr );
    
    return siResult;
}


// ---------------------------------------------------------------------------
//	* MakeContextData
// ---------------------------------------------------------------------------

sPSContextData* CPSPlugIn::MakeContextData ( void )
{
    sPSContextData  	*pOut		= NULL;
    sInt32				siResult	= eDSNoErr;

    pOut = (sPSContextData *) calloc(1, sizeof(sPSContextData));
    if ( pOut != NULL )
    {
        //do nothing with return here since we know this is new
        //and we did a calloc above
        siResult = CleanContextData(pOut);
    }

    return( pOut );

} // MakeContextData

// ---------------------------------------------------------------------------
//	* CleanContextData
// ---------------------------------------------------------------------------

sInt32 CPSPlugIn::CleanContextData ( sPSContextData *inContext )
{
    sInt32	siResult = eDSNoErr;
	   
    if ( inContext == NULL )
    {
        siResult = eDSBadContextData;
	}
    else
    {
        if (inContext->psName != NULL)
        {
            free( inContext->psName );
            inContext->psName = NULL;
        }
		
        inContext->offset = 0;
        
        if (inContext->conn != NULL)
        {
            gSASLMutex->Wait();
            sasl_dispose(&inContext->conn);
            gSASLMutex->Signal();
            
            inContext->conn = NULL;
        }
        
        if (inContext->serverIn != NULL)
        {
            fclose(inContext->serverIn);
            inContext->serverIn = NULL;
        }
        
        if (inContext->serverOut != NULL)
        {
            fclose(inContext->serverOut);
            inContext->serverOut = NULL;
        }
        
        if (inContext->fd != NULL)
        {
            close(inContext->fd);
            inContext->fd = NULL;
        }
        
		if (inContext->rsaPublicKeyStr != NULL)
		{
			free(inContext->rsaPublicKeyStr);
			inContext->rsaPublicKeyStr = NULL;
		}
		
		if (inContext->rsaPublicKey != NULL)
		{
			key_free(inContext->rsaPublicKey);
			inContext->rsaPublicKey = NULL;
		}
		
        if (inContext->mech != NULL)
        {
            free(inContext->mech);
            inContext->mech = NULL;
        }
        
        inContext->mechCount = 0;
        
        memset(inContext->last.username, 0, sizeof(inContext->last.username));
        
        if (inContext->last.password != NULL)
        {
            memset(inContext->last.password, 0, inContext->last.passwordLen);
            free(inContext->last.password);
            inContext->last.password = NULL;
        }
        inContext->last.passwordLen = 0;
        inContext->last.successfulAuth = false;
        
        memset(inContext->nao.username, 0, sizeof(inContext->nao.username));
        
        if (inContext->nao.password != NULL)
        {
            memset(inContext->nao.password, 0, inContext->nao.passwordLen);
            free(inContext->nao.password);
            inContext->nao.password = NULL;
        }
        inContext->nao.passwordLen = 0;
        inContext->nao.successfulAuth = false;
	}
    
    return( siResult );

} // CleanContextData

//------------------------------------------------------------------------------------
//	* GetAttributeEntry
//------------------------------------------------------------------------------------

sInt32 CPSPlugIn::GetAttributeEntry ( sGetAttributeEntry *inData )
{
	sInt32					siResult			= eDSNoErr;
	uInt16					usAttrTypeLen		= 0;
	uInt16					usAttrCnt			= 0;
	uInt16					usAttrLen			= 0;
	uInt16					usValueCnt			= 0;
	uInt16					usValueLen			= 0;
	uInt32					i					= 0;
	uInt32					uiIndex				= 0;
	uInt32					uiAttrEntrySize		= 0;
	uInt32					uiOffset			= 0;
	uInt32					uiTotalValueSize	= 0;
	uInt32					offset				= 4;
	uInt32					buffSize			= 0;
	uInt32					buffLen				= 0;
	char				   *p			   		= NULL;
	char				   *pAttrType	   		= NULL;
	tDataBufferPtr			pDataBuff			= NULL;
	tAttributeEntryPtr		pAttribInfo			= NULL;
	sPSContextData		   *pAttrContext		= NULL;
	sPSContextData		   *pValueContext		= NULL;

	try
	{
		Throw_NULL( inData, eMemoryError );

		pAttrContext = (sPSContextData *)gPSContextTable->GetItemData( inData->fInAttrListRef );
		Throw_NULL( pAttrContext, eDSBadContextData );

		uiIndex = inData->fInAttrInfoIndex;
		if (uiIndex == 0)
            throw( eDSInvalidIndex );
        
		pDataBuff = inData->fInOutDataBuff;
		Throw_NULL( pDataBuff, eDSNullDataBuff );
		
		buffSize	= pDataBuff->fBufferSize;
		//buffLen		= pDataBuff->fBufferLength;
		//here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
		//and the fBufferLength is the overall length of the data for all blocks at the end of the data block
		//the value ALSO includes the bookkeeping data at the start of the data block
		//so we need to read it here

		p		= pDataBuff->fBufferData + pAttrContext->offset;
		offset	= pAttrContext->offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if ( 2 > (sInt32)(buffSize - offset) )
            throw( eDSInvalidBuffFormat );
        
		// Get the attribute count
		::myMemcpy( &usAttrCnt, p, 2 );
		if (uiIndex > usAttrCnt)
            throw( eDSInvalidIndex );
        
		// Move 2 bytes
		p		+= 2;
		offset	+= 2;

		// Skip to the attribute that we want
		for ( i = 1; i < uiIndex; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (2 > (sInt32)(buffSize - offset) )
                throw( eDSInvalidBuffFormat );
            
			// Get the length for the attribute
			::myMemcpy( &usAttrLen, p, 2 );

			// Move the offset past the length word and the length of the data
			p		+= 2 + usAttrLen;
			offset	+= 2 + usAttrLen;
		}

		// Get the attribute offset
		uiOffset = offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffSize - offset))
            throw( eDSInvalidBuffFormat );
		
		// Get the length for the attribute block
		::myMemcpy( &usAttrLen, p, 2 );

		// Skip past the attribute length
		p		+= 2;
		offset	+= 2;

		//set the bufLen to stricter range
		buffLen = offset + usAttrLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffLen - offset))
            throw( eDSInvalidBuffFormat );
		
		// Get the length for the attribute type
		::myMemcpy( &usAttrTypeLen, p, 2 );
		
		pAttrType = p + 2;
		p		+= 2 + usAttrTypeLen;
		offset	+= 2 + usAttrTypeLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffLen - offset))
            throw( eDSInvalidBuffFormat );
		
		// Get number of values for this attribute
		::myMemcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;
		
		for ( i = 0; i < usValueCnt; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (2 > (sInt32)(buffLen - offset))
                throw( eDSInvalidBuffFormat );
            
			// Get the length for the value
			::myMemcpy( &usValueLen, p, 2 );
			
			p		+= 2 + usValueLen;
			offset	+= 2 + usValueLen;
			
			uiTotalValueSize += usValueLen;
		}

		uiAttrEntrySize = sizeof( tAttributeEntry ) + usAttrTypeLen + kBuffPad;
		pAttribInfo = (tAttributeEntry *)::calloc( 1, uiAttrEntrySize );

		pAttribInfo->fAttributeValueCount				= usValueCnt;
		pAttribInfo->fAttributeDataSize					= uiTotalValueSize;
		pAttribInfo->fAttributeValueMaxSize				= 512;				// <- need to check this xxxxx
		pAttribInfo->fAttributeSignature.fBufferSize	= usAttrTypeLen + kBuffPad;
		pAttribInfo->fAttributeSignature.fBufferLength	= usAttrTypeLen;
		::memcpy( pAttribInfo->fAttributeSignature.fBufferData, pAttrType, usAttrTypeLen );

		pValueContext = MakeContextData();
		Throw_NULL( pValueContext , eMemoryAllocError );

		pValueContext->offset = uiOffset;

		gPSContextTable->AddItem( inData->fOutAttrValueListRef, pValueContext );

		inData->fOutAttrInfoPtr = pAttribInfo;
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetAttributeEntry


//------------------------------------------------------------------------------------
//	* GetAttributeValue
//------------------------------------------------------------------------------------

sInt32 CPSPlugIn::GetAttributeValue ( sGetAttributeValue *inData )
{
	sInt32						siResult		= eDSNoErr;
	uInt16						usValueCnt		= 0;
	uInt16						usValueLen		= 0;
	uInt16						usAttrNameLen	= 0;
	uInt32						i				= 0;
	uInt32						uiIndex			= 0;
	uInt32						offset			= 0;
	char					   *p				= NULL;
	tDataBuffer				   *pDataBuff		= NULL;
	tAttributeValueEntry	   *pAttrValue		= NULL;
	sPSContextData		   *pValueContext	= NULL;
	uInt32						buffSize		= 0;
	uInt32						buffLen			= 0;
	uInt16						attrLen			= 0;

	try
	{
		pValueContext = (sPSContextData *)gPSContextTable->GetItemData( inData->fInAttrValueListRef );
		Throw_NULL( pValueContext , eDSBadContextData );

		uiIndex = inData->fInAttrValueIndex;
		if (uiIndex == 0)
            throw( eDSInvalidIndex );
		
		pDataBuff = inData->fInOutDataBuff;
		Throw_NULL( pDataBuff , eDSNullDataBuff );

		buffSize	= pDataBuff->fBufferSize;
		//buffLen		= pDataBuff->fBufferLength;
		//here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
		//and the fBufferLength is the overall length of the data for all blocks at the end of the data block
		//the value ALSO includes the bookkeeping data at the start of the data block
		//so we need to read it here

		p		= pDataBuff->fBufferData + pValueContext->offset;
		offset	= pValueContext->offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffSize - offset))
            throw( eDSInvalidBuffFormat );
				
		// Get the buffer length
		::myMemcpy( &attrLen, p, 2 );

		//now add the offset to the attr length for the value of buffLen to be used to check for buffer overruns
		//AND add the length of the buffer length var as stored ie. 2 bytes
		buffLen		= attrLen + pValueContext->offset + 2;
		if (buffLen > buffSize)
            throw( eDSInvalidBuffFormat );
        
		// Skip past the attribute length
		p		+= 2;
		offset	+= 2;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffLen - offset))
            throw( eDSInvalidBuffFormat );
		
		// Get the attribute name length
		::myMemcpy( &usAttrNameLen, p, 2 );
		
		p		+= 2 + usAttrNameLen;
		offset	+= 2 + usAttrNameLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffLen - offset))
            throw( eDSInvalidBuffFormat );
		
		// Get the value count
		::myMemcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;

		if (uiIndex > usValueCnt)
            throw( eDSInvalidIndex );

		// Skip to the value that we want
		for ( i = 1; i < uiIndex; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (2 > (sInt32)(buffLen - offset))
                throw( eDSInvalidBuffFormat );
		
			// Get the length for the value
			::myMemcpy( &usValueLen, p, 2 );
			
			p		+= 2 + usValueLen;
			offset	+= 2 + usValueLen;
		}

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffLen - offset))
            throw( eDSInvalidBuffFormat );
		
		::myMemcpy( &usValueLen, p, 2 );
		
		p		+= 2;
		offset	+= 2;

		//if (usValueLen == 0) throw (eDSInvalidBuffFormat ); //if zero is it okay?
        
		pAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + usValueLen + kBuffPad );
        Throw_NULL(pAttrValue, eMemoryAllocError);
        
		pAttrValue->fAttributeValueData.fBufferSize		= usValueLen + kBuffPad;
		pAttrValue->fAttributeValueData.fBufferLength	= usValueLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if ( usValueLen > (sInt32)(buffLen - offset) )
            throw ( eDSInvalidBuffFormat );
		
		::memcpy( pAttrValue->fAttributeValueData.fBufferData, p, usValueLen );

			// Set the attribute value ID
		pAttrValue->fAttributeValueID = CalcCRC( pAttrValue->fAttributeValueData.fBufferData );

		inData->fOutAttrValue = pAttrValue;
			
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetAttributeValue



// ---------------------------------------------------------------------------
//	* CalcCRC
// ---------------------------------------------------------------------------

uInt32 CPSPlugIn::CalcCRC ( char *inStr )
{
	char		   *p			= inStr;
	sInt32			siI			= 0;
	sInt32			siStrLen	= 0;
	uInt32			uiCRC		= 0xFFFFFFFF;
	CRCCalc			aCRCCalc;

	if ( inStr != NULL )
	{
		siStrLen = ::strlen( inStr );

		for ( siI = 0; siI < siStrLen; ++siI )
		{
			uiCRC = aCRCCalc.UPDC32( *p, uiCRC );
			p++;
		}
	}

	return( uiCRC );

} // CalcCRC


//------------------------------------------------------------------------------------
//	* GetDirNodeInfo
//------------------------------------------------------------------------------------

sInt32 CPSPlugIn::GetDirNodeInfo ( sGetDirNodeInfo *inData )
{
	sInt32				siResult		= eDSNoErr;
	uInt32				uiOffset		= 0;
	uInt32				uiCntr			= 1;
	uInt32				uiAttrCnt		= 0;
	CAttributeList	   *inAttrList		= NULL;
	char			   *pAttrName		= NULL;
	char			   *pData			= NULL;
	sPSContextData	   *pContext		= NULL;
	sPSContextData	   *pAttrContext	= NULL;
	CBuff				outBuff;
	CDataBuff		   *aRecData		= NULL;
	CDataBuff		   *aAttrData		= NULL;
	CDataBuff		   *aTmpData		= NULL;

// Can extract here the following:
// kDSAttributesAll
// kDSNAttrNodePath
// kDS1AttrReadOnlyNode
// kDSNAttrAuthMethod
//KW need to add mappings info next

	try
	{
		Throw_NULL( inData , eMemoryError );

		pContext = (sPSContextData *)gPSContextTable->GetItemData( inData->fInNodeRef );
		Throw_NULL( pContext , eDSBadContextData );

		inAttrList = new CAttributeList( inData->fInDirNodeInfoTypeList );
		Throw_NULL( inAttrList, eDSNullNodeInfoTypeList );
		if (inAttrList->GetCount() == 0)
            throw( eDSEmptyNodeInfoTypeList );

		siResult = outBuff.Initialize( inData->fOutDataBuff, true );
		if ( siResult != eDSNoErr )
            throw( siResult );

		siResult = outBuff.SetBuffType( 'Gdni' );  //can't use 'StdB' since a tRecordEntry is not returned
		if ( siResult != eDSNoErr )
            throw( siResult );
        
		aRecData = new CDataBuff();
		Throw_NULL( aRecData , eMemoryError );
		aAttrData = new CDataBuff();
		Throw_NULL( aAttrData , eMemoryError );
		aTmpData = new CDataBuff();
		Throw_NULL( aTmpData , eMemoryError );

		// Set the record name and type
		aRecData->AppendShort( ::strlen( "dsAttrTypeStandard:DirectoryNodeInfo" ) );
		aRecData->AppendString( (char *)"dsAttrTypeStandard:DirectoryNodeInfo" );
		aRecData->AppendShort( ::strlen( "DirectoryNodeInfo" ) );
		aRecData->AppendString( (char *)"DirectoryNodeInfo" );

		while ( inAttrList->GetAttribute( uiCntr++, &pAttrName ) == eDSNoErr )
		{
			//package up all the dir node attributes dependant upon what was asked for
			if ((::strcmp( pAttrName, kDSAttributesAll ) == 0) ||
				(::strcmp( pAttrName, kDSNAttrNodePath ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDSNAttrNodePath ) );
				aTmpData->AppendString( kDSNAttrNodePath );

				if ( inData->fInAttrInfoOnly == false )
				{
					// Attribute value count always two
					aTmpData->AppendShort( 2 );
					
					// Append attribute value
					aTmpData->AppendShort( ::strlen( "PasswordServer" ) );
					aTmpData->AppendString( (char *)"PasswordServer" );

					char *tmpStr = NULL;
					if (pContext->psName != NULL)
					{
						tmpStr = new char[1+::strlen(pContext->psName)];
						::strcpy( tmpStr, pContext->psName );
					}
					else
					{
						tmpStr = new char[1+::strlen("Unknown Node Location")];
						::strcpy( tmpStr, "Unknown Node Location" );
					}
					
					// Append attribute value
					aTmpData->AppendShort( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );

				} // fInAttrInfoOnly is false
				
				// Add the attribute length
				aAttrData->AppendShort( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
			} // kDSAttributesAll or kDSNAttrNodePath
			
			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDS1AttrReadOnlyNode ) == 0) )
			{
				aTmpData->Clear();

				uiAttrCnt++;

				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDS1AttrReadOnlyNode ) );
				aTmpData->AppendString( kDS1AttrReadOnlyNode );

				if ( inData->fInAttrInfoOnly == false )
				{
					// Attribute value count
					aTmpData->AppendShort( 1 );

					//possible for a node to be ReadOnly, ReadWrite, WriteOnly
					//note that ReadWrite does not imply fully readable or writable
					
					// Add the root node as an attribute value
					aTmpData->AppendShort( ::strlen( "ReadOnly" ) );
					aTmpData->AppendString( "ReadOnly" );

				}
				// Add the attribute length and data
				aAttrData->AppendShort( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

				// Clear the temp block
				aTmpData->Clear();
			}
				 
			if ((::strcmp( pAttrName, kDSAttributesAll ) == 0) ||
				(::strcmp( pAttrName, kDSNAttrAuthMethod ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDSNAttrAuthMethod ) );
				aTmpData->AppendString( kDSNAttrAuthMethod );
				
				if ( inData->fInAttrInfoOnly == false )
				{
					int idx, mechCount = 0;
					char dsTypeStr[256];
					
					// get the count for the mechs that get returned
					for ( idx = 0; idx < pContext->mechCount; idx++ )
					{
						GetAuthMethodFromSASLName( pContext->mech[idx].method, dsTypeStr );
						if ( dsTypeStr[0] != '\0' )
							mechCount++;
                    }
					
					// Attribute value count
					aTmpData->AppendShort( 7 + mechCount );
					
					aTmpData->AppendShort( ::strlen( kDSStdAuthClearText ) );
					aTmpData->AppendString( kDSStdAuthClearText );
					aTmpData->AppendShort( ::strlen( kDSStdAuthSetPasswd ) );
					aTmpData->AppendString( kDSStdAuthSetPasswd );
					aTmpData->AppendShort( ::strlen( kDSStdAuthChangePasswd ) );
					aTmpData->AppendString( kDSStdAuthChangePasswd );
					aTmpData->AppendShort( ::strlen( kDSStdAuthSetPasswdAsRoot ) );
					aTmpData->AppendString( kDSStdAuthSetPasswdAsRoot );
					aTmpData->AppendShort( ::strlen( kDSStdAuthNodeNativeClearTextOK ) );
					aTmpData->AppendString( kDSStdAuthNodeNativeClearTextOK );
					aTmpData->AppendShort( ::strlen( kDSStdAuthNodeNativeNoClearText ) );
					aTmpData->AppendString( kDSStdAuthNodeNativeNoClearText );
					
					// password server supports kDSStdAuth2WayRandomChangePasswd
					// with or without the plug-in
					aTmpData->AppendShort( ::strlen( kDSStdAuth2WayRandomChangePasswd ) );
					aTmpData->AppendString( kDSStdAuth2WayRandomChangePasswd );
					
					for ( idx = 0; idx < pContext->mechCount; idx++ )
					{
						GetAuthMethodFromSASLName( pContext->mech[idx].method, dsTypeStr );
						if ( dsTypeStr[0] != '\0' )
						{
							// Append first attribute value
							aTmpData->AppendShort( ::strlen( dsTypeStr ) );
							aTmpData->AppendString( dsTypeStr );
						}
                    }
				} // fInAttrInfoOnly is false
                
				// Add the attribute length
				aAttrData->AppendShort( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
			
			} // kDSAttributesAll or kDSNAttrAuthMethod

		} // while
		
		aRecData->AppendShort( uiAttrCnt );
		if (uiAttrCnt > 0)
		{
			aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
		}

		outBuff.AddData( aRecData->GetData(), aRecData->GetLength() );
		inData->fOutAttrInfoCount = uiAttrCnt;

		pData = outBuff.GetDataBlock( 1, &uiOffset );
		if ( pData != NULL )
		{
			pAttrContext = MakeContextData();
			Throw_NULL( pAttrContext , eMemoryAllocError );
			
		//add to the offset for the attr list the length of the GetDirNodeInfo fixed record labels
//		record length = 4
//		aRecData->AppendShort( ::strlen( "dsAttrTypeStandard:DirectoryNodeInfo" ) ); = 2
//		aRecData->AppendString( "dsAttrTypeStandard:DirectoryNodeInfo" ); = 36
//		aRecData->AppendShort( ::strlen( "DirectoryNodeInfo" ) ); = 2
//		aRecData->AppendString( "DirectoryNodeInfo" ); = 17
//		total adjustment = 4 + 2 + 36 + 2 + 17 = 61

			pAttrContext->offset = uiOffset + 61;

			gPSContextTable->AddItem( inData->fOutAttrListRef, pAttrContext );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if ( inAttrList != NULL )
	{
		delete( inAttrList );
		inAttrList = NULL;
	}
	if ( aRecData != NULL )
	{
		delete( aRecData );
		aRecData = NULL;
	}
	if ( aAttrData != NULL )
	{
		delete( aAttrData );
		aAttrData = NULL;
	}
	if ( aTmpData != NULL )
	{
		delete( aTmpData );
		aTmpData = NULL;
	}

	return( siResult );

} // GetDirNodeInfo


//------------------------------------------------------------------------------------
//	* CloseAttributeList
//------------------------------------------------------------------------------------

sInt32 CPSPlugIn::CloseAttributeList ( sCloseAttributeList *inData )
{
	sInt32				siResult		= eDSNoErr;
	sPSContextData	   *pContext		= NULL;

	pContext = (sPSContextData *)gPSContextTable->GetItemData( inData->fInAttributeListRef );
	if ( pContext != NULL )
	{
		//only "offset" should have been used in the Context
		gPSContextTable->RemoveItem( inData->fInAttributeListRef );
	}
	else
	{
		siResult = eDSInvalidAttrListRef;
	}

	return( siResult );

} // CloseAttributeList


//------------------------------------------------------------------------------------
//	* CloseAttributeValueList
//------------------------------------------------------------------------------------

sInt32 CPSPlugIn::CloseAttributeValueList ( sCloseAttributeValueList *inData )
{
	sInt32				siResult		= eDSNoErr;
	sPSContextData	   *pContext		= NULL;

	pContext = (sPSContextData *)gPSContextTable->GetItemData( inData->fInAttributeValueListRef );
	if ( pContext != NULL )
	{
		//only "offset" should have been used in the Context
		gPSContextTable->RemoveItem( inData->fInAttributeValueListRef );
	}
	else
	{
		siResult = eDSInvalidAttrValueRef;
	}

	return( siResult );

} // CloseAttributeValueList


// ---------------------------------------------------------------------------
//	* GetStringFromAuthBuffer
//    retrieve a string from a standard auth buffer
//    buffer format should be 4 byte length followed by username, then optional
//    additional data after. Buffer length must be at least 5 (length + 1 character name)
// ---------------------------------------------------------------------------

sInt32 CPSPlugIn::GetStringFromAuthBuffer(tDataBufferPtr inAuthData, int stringNum, char **outString)
{
	tDataListPtr dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
	if (dataList != NULL)
	{
		*outString = dsDataListGetNodeStringPriv(dataList, stringNum);
		// this allocates a copy of the string
		
		dsDataListDeallocatePriv(dataList);
		free(dataList);
		dataList = NULL;
		return eDSNoErr;
	}
    
	return eDSInvalidBuffFormat;
}


// ---------------------------------------------------------------------------
//	* GetDataFromAuthBuffer
//    retrieve data from a standard auth buffer
//    buffer format should be 4 byte length followed by username, then optional
//    additional data after. Buffer length must be at least 5 (length + 1 character name)
// ---------------------------------------------------------------------------

sInt32 CPSPlugIn::GetDataFromAuthBuffer(tDataBufferPtr inAuthData, int nodeNum, unsigned char **outData, long *outLen)
{
    tDataNodePtr pDataNode;
    tDirStatus status;
    
    *outLen = 0;
    
	tDataListPtr dataList = dsAuthBufferGetDataListAllocPriv(inAuthData);
	if (dataList != NULL)
	{
		status = dsDataListGetNodePriv(dataList, nodeNum, &pDataNode);
        if ( status != eDSNoErr )
            return status;
        
		*outData = (unsigned char *) malloc(pDataNode->fBufferLength);
        if ( ! (*outData) )
            return eMemoryAllocError;
        
        memcpy(*outData, ((tDataBufferPriv*)pDataNode)->fBufferData, pDataNode->fBufferLength);
        *outLen = pDataNode->fBufferLength;
        
        dsDataListDeallocatePriv(dataList);
		free(dataList);
		dataList = NULL;
		return eDSNoErr;
	}
    
	return eDSInvalidBuffFormat;
}


//------------------------------------------------------------------------------------
//	* DoAuthentication
//------------------------------------------------------------------------------------

sInt32 CPSPlugIn::DoAuthentication ( sDoDirNodeAuth *inData )
{
	sInt32				siResult			= noErr;
	UInt32				uiAuthMethod		= 0;
	sPSContextData	   *pContext			= NULL;
    char 				saslMechNameStr[256];
	char				*userName			= NULL;
	char				*password			= NULL;
    long				passwordLen			= 0;
    char				*challenge			= NULL;
    char 				*userIDToSet		= NULL;
	char 				*paramStr			= NULL;
    Boolean				bHasValidAuth		= false;
    sPSContinueData		*pContinue			= NULL;
    
    DEBUGLOG( "CPSPlugIn::DoAuthentication\n");
	try
	{
		pContext = (sPSContextData *)gPSContextTable->GetItemData( inData->fInNodeRef );
		Throw_NULL( pContext, eDSBadContextData );
        
		// make sure there is a connection
		if ( ! Connected(pContext) )
		{
			siResult = ConnectToServer( pContext );
			if ( siResult != 0 )
				throw( siResult );
		}
		
		siResult = GetAuthMethodConstant( pContext, inData->fInAuthMethod, &uiAuthMethod, saslMechNameStr );
        
        DEBUGLOG( "GetAuthMethodConstant siResult=%l, uiAuthMethod=%l, mech=%s\n", siResult, uiAuthMethod,saslMechNameStr);
        
        if ( siResult == noErr &&
			 uiAuthMethod != kAuthNativeMethod &&
			 uiAuthMethod != kAuth2WayRandomChangePass )
		{
            siResult = GetAuthMethodSASLName( uiAuthMethod, inData->fInDirNodeAuthOnlyFlag, saslMechNameStr );
			DEBUGLOG( "GetAuthMethodSASLName siResult=%l, mech=%s\n", siResult, saslMechNameStr);
        }
		
        if ( inData->fIOContinueData == NULL )
        {
            siResult = UnpackUsernameAndPassword( pContext,
                                                    uiAuthMethod,
                                                    inData->fInAuthStepData,
                                                    &userName,
                                                    &password,
                                                    &passwordLen,
                                                    &challenge );
        }
        else
        {
            if ( gContinue->VerifyItem( inData->fIOContinueData ) == false )
                throw( (sInt32)eDSInvalidContinueData );
        }
        
        if ( siResult == noErr && 
             (pContext->last.successfulAuth || pContext->nao.successfulAuth) &&
             userName != NULL )
        {
            long len = strlen( userName );
            char *strippedUserName = (char *) malloc( len + 1 );
            
            Throw_NULL( strippedUserName, eMemoryError );
            
            strcpy( strippedUserName, userName );
            StripRSAKey( strippedUserName );
            if ( strcmp( strippedUserName, pContext->last.username ) == 0 )
            {
                // if the name is a match for the last authentication, then
                // the state is correct.
				switch (uiAuthMethod)
				{
					case kAuthGetPolicy:
					case kAuthSetPolicy:
					case kAuthGetGlobalPolicy:
					case kAuthSetGlobalPolicy:
					case kAuthGetUserName:
					case kAuthSetUserName:
					case kAuthGetUserData:
					case kAuthSetUserData:
					case kAuthDeleteUser:
					case kAuthNewUser:
					case kAuthSetPasswdAsRoot:
					case kAuthGetIDByName:
						bHasValidAuth = true;
						break;
					
					default:
						bHasValidAuth = false;
				}
            }
            else
            if ( pContext->nao.successfulAuth && strcmp( strippedUserName, pContext->nao.username ) == 0 )
            {
                // if the name is a match for the saved authentication (but
                // not the last one), then the state needs to be reset
                memcpy( pContext->last.username, pContext->nao.username, kMaxUserNameLength + 1 );
                
                if ( pContext->last.password != NULL ) {
                    memset( pContext->last.password, 0, pContext->last.passwordLen );
                    free( pContext->last.password );
                    pContext->last.password = NULL;
                    pContext->last.passwordLen = 0;
                }
                
                pContext->last.password = (char *) malloc( pContext->nao.passwordLen + 1 );
                Throw_NULL( pContext->last.password, eMemoryError );
                
                memcpy( pContext->last.password, pContext->nao.password, pContext->nao.passwordLen );
                pContext->last.password[pContext->nao.passwordLen] = '\0';
                
                pContext->last.passwordLen = pContext->nao.passwordLen;
            }
            
            free( strippedUserName );
        }
        
        // do not authenticate for auth methods that do not need SASL authentication
        if ( !bHasValidAuth &&
             siResult == noErr &&
             uiAuthMethod != kAuthGetPolicy &&
             uiAuthMethod != kAuthGetGlobalPolicy &&
             uiAuthMethod != kAuthGetIDByName &&
			 uiAuthMethod != kAuth2WayRandomChangePass
           )
        {
            char *rsaKeyPtr;
            
            if ( userName != NULL )
            {
                rsaKeyPtr = strchr( userName, ',' );
                if ( rsaKeyPtr != NULL )
                    siResult = DoRSAValidation( pContext, rsaKeyPtr + 1 );
			}
            
            if ( siResult == noErr )
            {
                pContext->last.successfulAuth = false;
                
                if ( uiAuthMethod == kAuth2WayRandom )
                {
                    if ( inData->fIOContinueData == NULL )
                    {
                        pContinue = (sPSContinueData *)::calloc( 1, sizeof( sPSContinueData ) );
                        Throw_NULL( pContinue, eMemoryError );
                        
                        gContinue->AddItem( pContinue, inData->fInNodeRef );
                        inData->fIOContinueData = pContinue;
                        
                        pContinue->fAuthPass = 0;
                        pContinue->fData = NULL;
                        pContinue->fDataLen = 0;
                    }
                    
                    siResult = DoSASLTwoWayRandAuth( pContext,
                                                    userName,
                                                    saslMechNameStr,
                                                    inData );
                }
                else
                {
                    siResult = DoSASLAuth( pContext,
                                            userName,
                                            password,
                                            passwordLen,
                                            challenge,
                                            saslMechNameStr );
                }
                
                if ( siResult == noErr && uiAuthMethod != kAuth2WayRandom )
                {
                    pContext->last.successfulAuth = true;
                    
                    // If authOnly == false, copy the username and password for later use
                    // with SetPasswordAsRoot.
                    if ( inData->fInDirNodeAuthOnlyFlag == false )
                    {
                        memcpy( pContext->nao.username, pContext->last.username, kMaxUserNameLength + 1 );
                        
                        pContext->nao.password = (char *) malloc( pContext->last.passwordLen + 1 );
                        Throw_NULL( pContext->nao.password, eMemoryError );
                        
                        memcpy( pContext->nao.password, pContext->last.password, pContext->last.passwordLen );
                        pContext->nao.password[pContext->last.passwordLen] = '\0';
                        
                        pContext->nao.passwordLen = pContext->last.passwordLen;
                        pContext->nao.successfulAuth = true;
                    }
                }
            }
        }
        
        if ( siResult == eDSNoErr || siResult == eDSAuthNewPasswordRequired )
        {
            tDataBufferPtr outBuf = inData->fOutAuthStepDataResponse;
            const char *encodedStr;
            unsigned int encodedStrLen;
            char encoded64Str[kOneKBuffer];
            char buf[kOneKBuffer];
            PWServerError result;
            int saslResult;
            
            switch( uiAuthMethod )
			{
                case kAuthSetPasswd:
                    // buffer format is:
                    // len1 username
                    // len2 user's new password
                    // len3 authenticatorID
                    // len4 authenticatorPW
                case kAuthSetPasswdAsRoot:
                    // buffer format is:
                    // len1 username
                    // len2 user's new password
                    
                    #pragma mark kAuthSetPasswd
        
                    siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 1, &userIDToSet );
                    if ( siResult == noErr )
                    {
                        StripRSAKey(userIDToSet);
                        
                        siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 2, &paramStr );
                    }
                    
					if ( siResult == noErr )
                    {
                        if ( paramStr == NULL )
                            throw( eDSInvalidBuffFormat );
                        if (strlen(paramStr) > kChangePassPaddedBufferSize )
                            throw( eDSAuthParameterError );
                        
                        this->FillWithRandomData(buf, kChangePassPaddedBufferSize);
                        strcpy(buf, paramStr);
                        
                        gSASLMutex->Wait();
                        saslResult = sasl_encode(pContext->conn,
												 buf,
												 kChangePassPaddedBufferSize,
												 &encodedStr,
												 &encodedStrLen); 
                        gSASLMutex->Signal();
                    }
                    
                    if ( siResult == noErr && saslResult == SASL_OK && userIDToSet != NULL )
                    {
                        if ( ConvertBinaryTo64( encodedStr, encodedStrLen, encoded64Str ) == SASL_OK )
                        {
                            char *commandBuf;
                            long commandBufLen = 11+strlen(userIDToSet)+1+strlen(encoded64Str)+3 + 40;
                            
                            commandBuf = (char *) malloc( commandBufLen );
                            Throw_NULL(commandBuf, eMemoryAllocError);
                            
                            snprintf(commandBuf, commandBufLen, "CHANGEPASS %s %s\r\n", userIDToSet, encoded64Str);
                            commandBuf[commandBufLen-1] = '\0';
                            writeToServer(pContext->serverOut, commandBuf);
                            result = readFromServer( pContext->serverIn, buf, kOneKBuffer );
                            
                            free(commandBuf);
							
							if ( result.err != 0 )
								siResult = PWSErrToDirServiceError(result);
                        }
                    }
                    else
                    {
                        printf("encode64 failed\n");
                    }
                    break;
                    
                case kAuthChangePasswd:
                    #pragma mark kAuthChangePasswd
                    /*!
                    * @defined kDSStdAuthChangePasswd
                    * @discussion Change the password for a user. Does not require prior authentication.
                    *     The buffer is packed as follows:
                    *
                    *     4 byte length of username,
                    *     username in UTF8 encoding,
                    *     4 byte length of old password,
                    *     old password in UTF8 encoding,
                    *     4 byte length of new password,
                    *     new password in UTF8 encoding
                    */
                    
                    siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &paramStr );
                    if ( siResult == noErr )
                    {
                        if ( paramStr == NULL )
                            throw( eDSInvalidBuffFormat );
                        if ( strlen(paramStr) > kChangePassPaddedBufferSize )
                            throw( eDSAuthParameterError );
                        
                        this->FillWithRandomData(buf, kChangePassPaddedBufferSize);
                        strcpy(buf, paramStr);
                        
                        gSASLMutex->Wait();
                        saslResult = sasl_encode(pContext->conn,
												 buf,
												 kChangePassPaddedBufferSize,
												 &encodedStr,
												 &encodedStrLen); 
                        gSASLMutex->Signal();
                    }
                    
                    if ( siResult == noErr && saslResult == SASL_OK && userName != NULL )
                    {
                        if ( ConvertBinaryTo64( encodedStr, encodedStrLen, encoded64Str ) == SASL_OK )
                        {
                            char *commandBuf;
                            long commandBufLen = 11+strlen(userName)+1+strlen(encoded64Str)+3 + 40;
                            
                            commandBuf = (char *) malloc(commandBufLen);
                            Throw_NULL(commandBuf, eMemoryAllocError);
                            
                            StripRSAKey(userName);
                            snprintf(commandBuf, commandBufLen, "CHANGEPASS %s %s\r\n", userName, encoded64Str);
                            commandBuf[commandBufLen-1] = '\0';
                            writeToServer(pContext->serverOut, commandBuf);
                            result = readFromServer( pContext->serverIn, buf, kOneKBuffer );
                            
                            free(commandBuf);
							
							if ( result.err != 0 )
								siResult = PWSErrToDirServiceError(result);
                        }
                    }
                    else
                    {
                        printf("encode64 failed\n");
                    }
                    break;
                
                case kAuthNewUser:
                    #pragma mark kAuthNewUser
                    // buffer format is:
                    // len1 AuthenticatorID
                    // len2 AuthenticatorPW
                    // len3 user's name
                    // len4 user's initial password
                    siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &userIDToSet );
                    if ( siResult == noErr )
                        siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 4, &paramStr );
                    if ( siResult == noErr )
					{
                        if ( userIDToSet == NULL || paramStr == NULL )
                            throw( eDSInvalidBuffFormat );
						if ( strlen(paramStr) > kChangePassPaddedBufferSize )
                            throw( eDSAuthParameterError );
                        
						this->FillWithRandomData(buf, kChangePassPaddedBufferSize);
						strcpy(buf, paramStr);
						
                        gSASLMutex->Wait();
                        saslResult = sasl_encode(pContext->conn,
												 buf,
												 kChangePassPaddedBufferSize,
												 &encodedStr,
												 &encodedStrLen); 
                        gSASLMutex->Signal();
					}
					
                    if ( siResult == noErr && saslResult == SASL_OK )
                    {
                        if ( ConvertBinaryTo64( encodedStr, encodedStrLen, encoded64Str ) == SASL_OK )
                        {
                            if ( pContext->rsaPublicKeyStr == NULL )
                                throw( eDSAuthServerError );
                            
							fprintf(pContext->serverOut, "NEWUSER %s %s\r\n", userIDToSet, encoded64Str);
                            fflush(pContext->serverOut);

                            result = readFromServer( pContext->serverIn, buf, kOneKBuffer );
							if ( result.err != 0 )
								throw( PWSErrToDirServiceError(result) );
							
							chop( buf );
							
							// use encodedStrLen; it's available
                            encodedStrLen = strlen(buf) + 1 + strlen(pContext->rsaPublicKeyStr);
							if ( encodedStrLen > outBuf->fBufferSize )
								throw( eDSBufferTooSmall );
							
							// put a 4-byte length in the buffer
							encodedStrLen -= 4;
							memcpy( outBuf->fBufferData, &encodedStrLen, 4 );
							outBuf->fBufferLength = 4;
							
							// copy the ID
							encodedStrLen = strlen(buf+4);
							memcpy( outBuf->fBufferData + outBuf->fBufferLength, buf+4, encodedStrLen );
							outBuf->fBufferLength += encodedStrLen;
							
							// add a separator
							outBuf->fBufferData[outBuf->fBufferLength] = ',';
							outBuf->fBufferLength++;
							
							// copy the public key
							strcpy( outBuf->fBufferData + outBuf->fBufferLength, pContext->rsaPublicKeyStr );
							outBuf->fBufferLength += strlen(pContext->rsaPublicKeyStr);
                        }
                    }
                    else
                    {
                        printf("encode64 failed\n");
                    }
                    break;
                    
                case kAuthGetPolicy:
                    #pragma mark kAuthGetPolicy
                    // buffer format is:
                    // len1 AuthenticatorID
                    // len2 AuthenticatorPW
                    // len3 AccountID
                    siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &userIDToSet );
                    if ( siResult == noErr )
                    {
                        char commandBuf[256];
                        
                        if ( userIDToSet == NULL )
                            throw( eDSInvalidBuffFormat );
                        
                        StripRSAKey(userIDToSet);
                        	
                        snprintf(commandBuf, 256, "GETPOLICY %s\r\n", userIDToSet);
                        commandBuf[255] = '\0';
                        writeToServer(pContext->serverOut, commandBuf);
                        
                        result = readFromServer( pContext->serverIn, buf, kOneKBuffer );
						if ( result.err != 0 )
							throw( PWSErrToDirServiceError(result) );
							
						// use encodedStrLen; it's available
						encodedStrLen = strlen(buf);
						if ( encodedStrLen <= outBuf->fBufferSize )
						{
							// A nice coincidence: we need to strip off "+OK " (4 chars) and we
							// need to put a 4-byte length at the beginning of the buffer
							::memcpy( buf, &encodedStrLen, 4 );
							
							::memcpy( outBuf->fBufferData, buf, encodedStrLen );
							outBuf->fBufferLength = encodedStrLen;
						}
						else
						{
							siResult = eDSBufferTooSmall;
						}
                    }
                    break;
                    
                case kAuthSetPolicy:
                    #pragma mark kAuthSetPolicy
                    // buffer format is:
                    // len1 AuthenticatorID
                    // len2 AuthenticatorPW
                    // len3 AccountID
                    // len4 PolicyString
                    siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &userIDToSet );
                    if ( siResult == noErr ) 
					{
						StripRSAKey(userIDToSet);
                            
                        siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 4, &paramStr );
					}
					
                    if ( siResult == noErr )
                    {
                        fprintf(pContext->serverOut, "SETPOLICY %s %s\r\n", userIDToSet, paramStr );
                        fflush(pContext->serverOut);
                        result = readFromServer( pContext->serverIn, buf, kOneKBuffer );
						if ( result.err != 0 )
							siResult = PWSErrToDirServiceError(result);
                    }
                    break;
                    
                case kAuthGetGlobalPolicy:
                    #pragma mark kAuthGetGlobalPolicy
                    // buffer format is:
                    // len1 AuthenticatorID
                    // len2 AuthenticatorPW
                    writeToServer(pContext->serverOut, "GETGLOBALPOLICY\r\n");
                    result = readFromServer( pContext->serverIn, buf, kOneKBuffer );
					if ( result.err != 0 )
						throw( PWSErrToDirServiceError(result) );
						
					// use encodedStrLen; it's available
					encodedStrLen = strlen(buf);
					if ( encodedStrLen <= outBuf->fBufferSize )
					{
						// A nice coincidence: we need to strip off "+OK " (4 chars) and we
						// need to put a 4-byte length at the beginning of the buffer
						::memcpy( buf, &encodedStrLen, 4 );
						
						::memcpy( outBuf->fBufferData, buf, encodedStrLen );
						outBuf->fBufferLength = encodedStrLen;
					}
					else
					{
						siResult = eDSBufferTooSmall;
					}
                    break;
                    
                case kAuthSetGlobalPolicy:
                    #pragma mark kAuthSetGlobalPolicy
                    // buffer format is:
                    // len1 AuthenticatorID
                    // len2 AuthenticatorPW
                    // len3 PolicyString
                    siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &paramStr );
                    if ( siResult == noErr )
                    {
                        fprintf(pContext->serverOut, "SETGLOBALPOLICY %s\r\n", paramStr );
                        fflush(pContext->serverOut);
                        result = readFromServer( pContext->serverIn, buf, kOneKBuffer );
						if ( result.err != 0 )
							siResult = PWSErrToDirServiceError(result);
                    }
                    break;
                    
                case kAuthGetUserName:
                    #pragma mark kAuthGetUserName
                    // buffer format is:
                    // len1 AuthenticatorID
                    // len2 AuthenticatorPW
                    // len3 AccountID
                    siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &userIDToSet );
					if ( siResult == noErr )
                    {
                        StripRSAKey(userIDToSet);
                        
                        fprintf(pContext->serverOut, "GETUSERNAME %s\r\n", userIDToSet );
                        fflush(pContext->serverOut);

                        result = readFromServer( pContext->serverIn, buf, kOneKBuffer );
						if ( result.err != 0 )
							throw( PWSErrToDirServiceError(result) );
						
						// use encodedStrLen; it's available
						encodedStrLen = strlen(buf);
						if ( encodedStrLen <= outBuf->fBufferSize )
						{
							// A nice coincidence: we need to strip off "+OK " (4 chars) and we
							// need to put a 4-byte length at the beginning of the buffer
							::memcpy( buf, &encodedStrLen, 4 );
							
							::memcpy( outBuf->fBufferData, buf, encodedStrLen );
							outBuf->fBufferLength = encodedStrLen;
						}
						else
						{
							siResult = eDSBufferTooSmall;
						}
                    }
                    break;
                    
                case kAuthSetUserName:
                    #pragma mark kAuthSetUserName
                    // buffer format is:
                    // len1 AuthenticatorID
                    // len2 AuthenticatorPW
                    // len3 AccountID
                    // len4 NewUserName
                    siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &userIDToSet );
					if ( siResult == noErr )
                        siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 4, &paramStr );
                    if ( siResult == noErr )
                    {
                        StripRSAKey(userIDToSet);
                        
                        fprintf(pContext->serverOut, "SETUSERNAME %s %s\r\n", userIDToSet, paramStr );
                        fflush(pContext->serverOut);
                        result = readFromServer( pContext->serverIn, buf, kOneKBuffer );
						if ( result.err != 0 )
							siResult = PWSErrToDirServiceError(result);
                    }
                    break;
                    
                case kAuthGetUserData:
                    #pragma mark kAuthGetUserData
                    // buffer format is:
                    // len1 AuthenticatorID
                    // len2 AuthenticatorPW
                    // len3 AccountID
                    siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &userIDToSet );
					if ( siResult == noErr )
                    {
                        char *outData = NULL;
                        unsigned long decodedStrLen;
                        
                        StripRSAKey(userIDToSet);
                        
                        fprintf(pContext->serverOut, "GETUSERDATA %s\r\n", userIDToSet );
                        fflush(pContext->serverOut);

                        result = readFromServer( pContext->serverIn, buf, kOneKBuffer );
                        if ( result.err != 0 )
							siResult = PWSErrToDirServiceError(result);
						
                        if ( siResult == eDSNoErr )
                        {
                            // base64 decode user data
                            outData = (char *)malloc(strlen(buf));
                            Throw_NULL(outData, eMemoryError);
                            
                            if ( Convert64ToBinary( buf, outData, &decodedStrLen ) == 0 )
                            {
                                if ( decodedStrLen <= outBuf->fBufferSize )
                                {
                                    ::memcpy( outBuf->fBufferData, &decodedStrLen, 4 );
                                    ::memcpy( outBuf->fBufferData + 4, outData, decodedStrLen );
                                    outBuf->fBufferLength = decodedStrLen;
                                }
                                else
                                {
                                    siResult = eDSBufferTooSmall;
                                }
                            }
                            
                            free(outData);
                        }
                    }
                    break;
                    
                case kAuthSetUserData:
                    #pragma mark kAuthSetUserData
                    // buffer format is:
                    // len1 AuthenticatorID
                    // len2 AuthenticatorPW
                    // len3 AccountID
                    // len4 NewUserData
                    {
                        char *tptr;
                        long dataSegmentLen;
                        
                        siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &userIDToSet );
						if ( siResult == noErr )
                        {
                            StripRSAKey(userIDToSet);
                            
							tptr = inData->fInAuthStepData->fBufferData;
                            
                            for (int repeatCount = 3; repeatCount > 0; repeatCount--)
                            {
                                memcpy(&dataSegmentLen, tptr, 4);
                                tptr += 4 + dataSegmentLen;
                            }
                            
                            memcpy(&dataSegmentLen, tptr, 4);
                            
                            paramStr = (char *)malloc( dataSegmentLen * 4/3 + 20 );
                            Throw_NULL( paramStr, eMemoryError );
                            
                            siResult = ConvertBinaryTo64( tptr, dataSegmentLen, paramStr );
                        }
                        
                        if ( siResult == noErr )
                        {
                            // base64 encode user data
                            fprintf(pContext->serverOut, "SETUSERDATA %s %s\r\n", userIDToSet, paramStr );
                            fflush(pContext->serverOut);
                            result = readFromServer( pContext->serverIn, buf, kOneKBuffer );
							if ( result.err != 0 )
								siResult = PWSErrToDirServiceError(result);
                        }
                    }
                    break;
                    
                case kAuthDeleteUser:
                    #pragma mark kAuthDeleteUser
                    // buffer format is:
                    // len1 AuthenticatorID
                    // len2 AuthenticatorPW
                    // len3 AccountID
                    siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &userIDToSet );
					if ( siResult == noErr )
                    {
                        StripRSAKey(userIDToSet);
                        
                        fprintf(pContext->serverOut, "DELETEUSER %s\r\n", userIDToSet );
                        fflush(pContext->serverOut);
                        result = readFromServer( pContext->serverIn, buf, kOneKBuffer );
						if ( result.err != 0 )
							siResult = PWSErrToDirServiceError(result);
                    }
                    break;
                
                case kAuthGetIDByName:
                    #pragma mark kAuthGetIDByName
                    // buffer format is:
                    // len1 AuthenticatorID
                    // len2 AuthenticatorPW
                    // len3 Name to look up
                    siResult = GetStringFromAuthBuffer( inData->fInAuthStepData, 3, &paramStr );
					if ( siResult == noErr )
                    {
                        fprintf(pContext->serverOut, "GETIDBYNAME %s\r\n", paramStr );
                        fflush(pContext->serverOut);
                        
                        result = readFromServer( pContext->serverIn, buf, kOneKBuffer );
						if ( result.err != 0 )
							throw( PWSErrToDirServiceError(result) );
						
                        chop(buf);
                        
                        // add the public rsa key
                        if ( pContext->rsaPublicKeyStr )
                        {
                            strcat(buf, ",");
                            strcat(buf, pContext->rsaPublicKeyStr);
                        }
                        
						encodedStrLen = strlen(buf);
						if ( encodedStrLen <= outBuf->fBufferSize )
						{
							// A nice coincidence: we need to strip off "+OK " (4 chars) and we
							// need to put a 4-byte length at the beginning of the buffer
							::memcpy( buf, &encodedStrLen, 4 );
							
							::memcpy( outBuf->fBufferData, buf, encodedStrLen );
							outBuf->fBufferLength = encodedStrLen;
						}
						else
						{
							siResult = eDSBufferTooSmall;
						}
                    }
                    break;
				
				case kAuth2WayRandomChangePass:
					#pragma mark kAuth2WayRandomChangePass
					StripRSAKey(userName);
					siResult = ConvertBinaryTo64( password, 8, encoded64Str );
					if ( siResult == noErr )
					{
						char *commandBuf;
						long commandBufLen = 16 + strlen(userName) + 1 + 16 + 1 + 16 + 3;
						
						commandBuf = (char *) malloc(commandBufLen);
						Throw_NULL(commandBuf, eMemoryAllocError);
						
						sprintf(commandBuf, "TWRNDCHANGEPASS %s %s ", userName, encoded64Str);
						
						siResult = ConvertBinaryTo64( password + 8, 8, encoded64Str );
						if ( siResult == noErr )
						{
							strcat( commandBuf, encoded64Str );
							strcat( commandBuf, "\r\n" );
							
							writeToServer(pContext->serverOut, commandBuf);
							result = readFromServer( pContext->serverIn, buf, sizeof(buf) );
						}
						
						free(commandBuf);
						
						if ( siResult == noErr && result.err != 0 )
							siResult = PWSErrToDirServiceError(result);
					}
					break;
            }
        }
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	inData->fResult = siResult;

    if ( userName != NULL )
        free( userName );
    if ( password != NULL )
        free( password );
    if ( userIDToSet != NULL )
        free( userIDToSet );
    if ( paramStr != NULL )
        free( paramStr );
    
    DEBUGLOG( "CPSPlugIn::DoAuthentication returning %l\n", siResult);
	return( siResult );

} // DoAuthentication


// ---------------------------------------------------------------------------
//	* UnpackUsernameAndPassword
//
// ---------------------------------------------------------------------------

sInt32
CPSPlugIn::UnpackUsernameAndPassword(
    sPSContextData *inContext,
    UInt32 uiAuthMethod,
    tDataBufferPtr inAuthBuf,
    char **outUserName,
    char **outPassword,
    long *outPasswordLen,
    char **outChallenge )
{
    sInt32					siResult		= eDSNoErr;
    unsigned char			*challenge		= NULL;
    unsigned char			*digest			= NULL;
    long					len				= 0;
    
    // sanity
    if ( outUserName == NULL || outPassword == NULL || outPasswordLen == NULL || outChallenge == NULL )
        return eParameterError;
    
    // init vars
    *outUserName = NULL;
    *outPassword = NULL;
    *outPasswordLen = 0;
    *outChallenge = NULL;
    
    try
    {
        switch (uiAuthMethod)
        {
            case kAuthAPOP:
                siResult = GetStringFromAuthBuffer( inAuthBuf, 1, outUserName );
                if ( siResult == noErr )
                    siResult = GetStringFromAuthBuffer( inAuthBuf, 2, (char **)&challenge );
                if ( siResult == noErr )
                    siResult = GetStringFromAuthBuffer( inAuthBuf, 3, (char **)&digest );
                if ( siResult == noErr )
                {
                    if ( challenge == NULL || digest == NULL )
                    	throw( eDSAuthParameterError );
                    
                    long challengeLen = strlen((char *)challenge);
                    long digestLen = strlen((char *)digest);
                    
                    if ( challengeLen > 0 && digestLen > 0 )
                    {
                        *outPasswordLen = challengeLen + 1 + digestLen;
                        *outPassword = (char *) malloc( *outPasswordLen + 1 );
                        Throw_NULL( (*outPassword), eMemoryAllocError );
                        
                        strcpy( *outPassword, (char *)challenge );
                        strcat( *outPassword, " " );
                        strcat( *outPassword, (char *)digest );
                    }
                }
                break;
            
            case kAuthCRAM_MD5:
                siResult = GetStringFromAuthBuffer( inAuthBuf, 1, outUserName );
                if ( siResult == noErr )
                    siResult = GetStringFromAuthBuffer( inAuthBuf, 2, outChallenge );
                if ( siResult == noErr )
                    siResult = GetDataFromAuthBuffer( inAuthBuf, 3, &digest, &len );
                if ( siResult == noErr && digest != NULL )
                {
                    *outPassword = (char *) malloc( len + 1 );
                    Throw_NULL( (*outPassword), eMemoryAllocError );
                	
                    // put a leading null to tell the CRAM-MD5 plug-in we're sending
                    // a hash.
                    **outPassword = '\0';
                    memcpy( (*outPassword) + 1, digest, len );
                    *outPasswordLen = len + 1;
                }
                break;
            
            case kAuthSMB_NT_Key:
            case kAuthSMB_LM_Key:
                siResult = GetStringFromAuthBuffer( inAuthBuf, 1, outUserName );
                if ( siResult == noErr )
                {
                    *outPassword = (char *)malloc(32);
                    Throw_NULL( (*outPassword), eMemoryAllocError );
                    
                    *outPasswordLen = 32;
                    
                    siResult = GetDataFromAuthBuffer( inAuthBuf, 2, &challenge, &len );
                    if ( siResult != noErr || challenge == NULL || len != 8 )
                        throw( (sInt32)eDSInvalidBuffFormat );
                    
                    siResult = GetDataFromAuthBuffer( inAuthBuf, 3, &digest, &len );
                    if ( siResult != noErr || digest == NULL || len != 24 )
                        throw( (sInt32)eDSInvalidBuffFormat );
                    
                    memcpy( *outPassword, challenge, 8 );
                    memcpy( (*outPassword) + 8, digest, 24 );
                    
                    free( challenge );
                    challenge = NULL;
                    
                    free( digest );
                    digest = NULL;
                }
                break;
                
            case kAuth2WayRandom:
                // for 2way random the first buffer is the username
                if ( inAuthBuf->fBufferLength > inAuthBuf->fBufferSize )
                    throw( (sInt32)eDSInvalidBuffFormat );
                
                *outUserName = (char*)calloc( inAuthBuf->fBufferLength + 1, 1 );
                strncpy( *outUserName, inAuthBuf->fBufferData, inAuthBuf->fBufferLength );
                (*outUserName)[inAuthBuf->fBufferLength] = '\0';
                break;
			
			case kAuth2WayRandomChangePass:
				siResult = GetStringFromAuthBuffer( inAuthBuf, 1, outUserName );
                if ( siResult == noErr )
				{
					char *tempPWStr = NULL;
					siResult = GetStringFromAuthBuffer( inAuthBuf, 2, &tempPWStr );
					if ( siResult == noErr && tempPWStr != NULL && strlen(tempPWStr) == 8 )
					{
						*outPasswordLen = 16;
						*outPassword = (char *)malloc(16);
						memcpy( *outPassword, tempPWStr, 8 );
						free( tempPWStr );
						
						siResult = GetStringFromAuthBuffer( inAuthBuf, 3, &tempPWStr );
						if ( siResult == noErr && tempPWStr != NULL && strlen(tempPWStr) == 8 )
						{
							memcpy( *outPassword + 8, tempPWStr, 8 );
							free( tempPWStr );
						}
					}
                }
				break;
				
            case kAuthSetPasswd:
                siResult = GetStringFromAuthBuffer( inAuthBuf, 3, outUserName );
                if ( siResult == noErr )
                    siResult = GetStringFromAuthBuffer( inAuthBuf, 4, outPassword );
                if ( siResult == noErr && *outPassword != NULL )
                    *outPasswordLen = strlen( *outPassword );
                break;
            
            case kAuthSetPasswdAsRoot:
                // uses current credentials
                if ( inContext->nao.successfulAuth && inContext->nao.password != NULL )
                {
                    long pwLen;
                    
                    *outUserName = (char *)malloc(kUserIDLength + 1);
                    strncpy(*outUserName, inContext->nao.username, kUserIDLength);
                    (*outUserName)[kUserIDLength] = '\0';
                    
                    pwLen = strlen(inContext->nao.password);
                    *outPassword = (char *)malloc(pwLen + 1);
                    strncpy(*outPassword, inContext->nao.password, pwLen);
                    (*outPassword)[pwLen] = '\0';
                    *outPasswordLen = pwLen;   
                    siResult = eDSNoErr;
                }
                else
                {
                    siResult = eDSNotAuthorized;
                }
                break;
			
            default:
                siResult = GetStringFromAuthBuffer( inAuthBuf, 1, outUserName );
                if ( siResult == noErr )
                    siResult = GetStringFromAuthBuffer( inAuthBuf, 2, outPassword );
                if ( siResult == noErr && *outPassword != NULL )
                    *outPasswordLen = strlen( *outPassword );
        }
    }
    
    catch ( sInt32 error )
    {
        siResult = error;
    }
    
    catch (...)
    {
        DEBUGLOG( "PasswordServer PlugIn: uncasted throw" );
        siResult = eDSAuthFailed;
    }
    
    if ( challenge != NULL ) {
        free( challenge );
        challenge = NULL;
    }
    if ( digest != NULL ) {
        free( digest );
        digest = NULL;
    }
    
    // user name is a required value
    // kAuth2WayRandom is multi-pass and only has a username for pass 1
    if ( siResult == eDSNoErr && *outUserName == NULL && uiAuthMethod != kAuth2WayRandom )
        siResult = eDSUserUnknown;
    
    return siResult;
}


// ---------------------------------------------------------------------------
//	StripRSAKey
// ---------------------------------------------------------------------------

void
CPSPlugIn::StripRSAKey(	char *inOutUserID )
{
    if ( inOutUserID == NULL )
        return;
    
    char *delim = strchr( inOutUserID, ',' );
    if ( delim )
        *delim = '\0';
}


// ---------------------------------------------------------------------------
//	* GetAuthMethodConstant
//
//	Returns a constant that represents a DirectoryServices auth method.
//	If the auth method is a native type, this function also returns
//	the SASL mech name in outNativeAuthMethodSASLName.
// ---------------------------------------------------------------------------

sInt32
CPSPlugIn::GetAuthMethodConstant(
    sPSContextData *inContext,
    tDataNode *inData,
    uInt32 *outAuthMethod,
	char *outNativeAuthMethodSASLName )
{
	sInt32			siResult		= noErr;
	char		   *p				= NULL;
    sInt32			prefixLen;
    
	if ( inData == NULL )
	{
		*outAuthMethod = kAuthUnknownMethod;
		return( eDSAuthParameterError );
	}

    if ( outNativeAuthMethodSASLName != NULL )
        *outNativeAuthMethodSASLName = '\0';
    
	p = (char *)inData->fBufferData;

	DEBUGLOG( "PasswordServer PlugIn: Attempting use of authentication method %s", p );

    prefixLen = strlen(kDSNativeAuthMethodPrefix);
    if ( ::strncmp( p, kDSNativeAuthMethodPrefix, prefixLen ) == 0 )
    {
        sInt32 index;
        
        *outAuthMethod = kAuthUnknownMethod;
		siResult = eDSAuthMethodNotSupported;
        
        p += prefixLen;
        
        // check for GetIDByName
        if ( strcmp( p, "dsAuthGetIDByName" ) == 0 )
        {
            *outAuthMethod = kAuthGetIDByName;
            return eDSNoErr;
        }
        
        for ( index = inContext->mechCount - 1; index >= 0; index-- )
        {
            if ( strcmp( p, inContext->mech[index].method ) == 0 )
            {
                if ( outNativeAuthMethodSASLName != NULL )
                    strcpy( outNativeAuthMethodSASLName, inContext->mech[index].method );
                
                *outAuthMethod = kAuthNativeMethod;
                siResult = noErr;
                break;
            }
        }
    }
    else
	if ( ::strcmp( p, kDSStdAuthClearText ) == 0 )
	{
		// Clear text auth method
		*outAuthMethod = kAuthClearText;
	}
	else
    if ( ::strcmp( p, kDSStdAuthNodeNativeClearTextOK ) == 0 )
	{
		// Node native auth method
		*outAuthMethod = kAuthNativeClearTextOK;
	}
	else
    if ( ::strcmp( p, kDSStdAuthNodeNativeNoClearText ) == 0 )
	{
		// Node native auth method
		*outAuthMethod = kAuthNativeNoClearText;
	}
	else
    if ( ::strcmp( p, kDSStdAuthSetPasswd ) == 0 )
	{
		*outAuthMethod = kAuthSetPasswd;
	}
    else
    if ( ::strcmp( p, kDSStdAuthChangePasswd ) == 0 )
	{
		*outAuthMethod = kAuthChangePasswd;
	}
    else
    if ( ::strcmp( p, kDSStdAuthSetPasswdAsRoot ) == 0 )
	{
		*outAuthMethod = kAuthSetPasswdAsRoot;
	}
	else
    if ( ::strcmp( p, kDSStdAuthAPOP ) == 0 )
	{
		// Unix crypt auth
		*outAuthMethod = kAuthAPOP;
	}
    else
    if ( ::strcmp( p, kDSStdAuth2WayRandom ) == 0 )
	{
		*outAuthMethod = kAuth2WayRandom;
	}
	else
	if ( ::strcmp( p, kDSStdAuth2WayRandomChangePasswd ) == 0 )
	{
		*outAuthMethod = kAuth2WayRandomChangePass;
	}
	else
    if ( ::strcmp( p, kDSStdAuthSMB_NT_Key ) == 0 )
	{
		*outAuthMethod = kAuthSMB_NT_Key;
	}
    else
    if ( ::strcmp( p, kDSStdAuthSMB_LM_Key ) == 0 )
	{
		// Unix crypt auth
		*outAuthMethod = kAuthSMB_LM_Key;
	}
    else
    if ( ::strcmp( p, kDSStdAuthCRAM_MD5 ) == 0 )
	{
		// Unix crypt auth
		*outAuthMethod = kAuthCRAM_MD5;
	}
    else
    if ( ::strcmp( p, kDSStdAuthNewUser ) == 0 )
	{
        *outAuthMethod = kAuthNewUser;
    }
    else
    if ( ::strcmp( p, kDSStdAuthGetPolicy ) == 0 )
	{
		*outAuthMethod = kAuthGetPolicy;
	}
    else
    if ( ::strcmp( p, kDSStdAuthSetPolicy ) == 0 )
	{
		*outAuthMethod = kAuthSetPolicy;
	}
    else
    if ( ::strcmp( p, kDSStdAuthGetGlobalPolicy ) == 0 )
	{
		*outAuthMethod = kAuthGetGlobalPolicy;
	}
    else
    if ( ::strcmp( p, kDSStdAuthSetGlobalPolicy ) == 0 )
	{
		*outAuthMethod = kAuthSetGlobalPolicy;
	}
    else
    if ( ::strcmp( p, kDSStdAuthGetUserName ) == 0 )
	{
		*outAuthMethod = kAuthGetUserName;
	}
    else
    if ( ::strcmp( p, kDSStdAuthSetUserName ) == 0 )
	{
		*outAuthMethod = kAuthSetUserName;
	}
    else
    if ( ::strcmp( p, kDSStdAuthGetUserData ) == 0 )
	{
		*outAuthMethod = kAuthGetUserData;
	}
    else
    if ( ::strcmp( p, kDSStdAuthSetUserData ) == 0 )
	{
		*outAuthMethod = kAuthSetUserData;
	}
    else
    if ( ::strcmp( p, kDSStdAuthDeleteUser ) == 0 )
	{
		*outAuthMethod = kAuthDeleteUser;
	}
	else
	{
		*outAuthMethod = kAuthUnknownMethod;
		siResult = eDSAuthMethodNotSupported;
	}

	return( siResult );

} // GetAuthMethodConstant


// ---------------------------------------------------------------------------
//	* GetAuthMethodSASLName
//
//	Returns the name of a SASL mechanism for
//	standard (kDSStdAuthMethodPrefix) auth mehthods 
// ---------------------------------------------------------------------------

sInt32
CPSPlugIn::GetAuthMethodSASLName ( uInt32 inAuthMethodConstant, bool inAuthOnly, char *outMechName )
{
    sInt32 result = noErr;
    
    if ( outMechName == NULL )
        return -1;
    *outMechName = '\0';
    
    switch ( inAuthMethodConstant )
    {
        case kAuthClearText:
            strcpy( outMechName, "PLAIN" );
            break;
            
        case kAuthCrypt:
            strcpy( outMechName, "CRYPT" );
            break;
            
        case kAuthSetPasswd:
            strcpy( outMechName, kDHX_SASL_Name );
            break;
            
        case kAuthChangePasswd:
            strcpy( outMechName, kDHX_SASL_Name );
            break;
            
        case kAuthSetPasswdAsRoot:
            strcpy( outMechName, kDHX_SASL_Name );
            break;
            
        case kAuthAPOP:
            strcpy( outMechName, "APOP" );
            break;
            
        case kAuth2WayRandom:
            strcpy( outMechName, "TWOWAYRANDOM" );
            break;
        
        case kAuthNativeClearTextOK:
            // If <inAuthOnly> == false, then a "kDSStdSetPasswdAsRoot" auth method
            // could be called later and will require DHX
            strcpy( outMechName, inAuthOnly ? kAuthNative_Priority : kDHX_SASL_Name );
            strcat( outMechName, " PLAIN" );
            break;
        
        case kAuthNativeNoClearText:
            // If <inAuthOnly> == false, then a "kDSStdSetPasswdAsRoot" auth method
            // could be called later and will require DHX
            strcpy( outMechName, inAuthOnly ? kAuthNative_Priority : kDHX_SASL_Name );
            break;
        
        case kAuthSMB_NT_Key:
            strcpy( outMechName, "SMB-NT" );
            break;
            
        case kAuthSMB_LM_Key:
            strcpy( outMechName, "SMB-LAN-MANAGER" );
            break;
        
        case kAuthCRAM_MD5:
            strcpy( outMechName, "CRAM-MD5" );
            break;
        
        case kAuthGetPolicy:
        case kAuthSetPolicy:
        case kAuthGetGlobalPolicy:
        case kAuthSetGlobalPolicy:
        case kAuthGetUserName:
        case kAuthSetUserName:
        case kAuthGetUserData:
        case kAuthSetUserData:
        case kAuthDeleteUser:
            strcpy( outMechName, kAuthNative_Priority );
            break;
        
        case kAuthNewUser:
            strcpy( outMechName, kDHX_SASL_Name );
            break;
        
        case kAuthUnknownMethod:
        case kAuthNativeMethod:
        default:
            result = eDSAuthMethodNotSupported;
    }
    
    return result;
}
                                                    

//------------------------------------------------------------------------------------
//	* GetAuthMethodFromSASLName
//------------------------------------------------------------------------------------
void CPSPlugIn::GetAuthMethodFromSASLName( const char *inMechName, char *outDSType )
{
	if ( outDSType == NULL )
		return;
	
	*outDSType = '\0';
	
	if ( inMechName == NULL )
		return;
	
	if ( strcmp( inMechName, "TWOWAYRANDOM" ) == 0 )
	{
		strcpy( outDSType, kDSStdAuth2WayRandom );
	}
	else
	if ( strcmp( inMechName, "SMB-NT" ) == 0 )
	{
		strcpy( outDSType, kDSStdAuthSMB_NT_Key );
	}
	else
	if ( strcmp( inMechName, "SMB-LAN-MANAGER" ) == 0 )
	{
		strcpy( outDSType, kDSStdAuthSMB_LM_Key );
	}
	else
	if ( strcmp( inMechName, "DIGEST-MD5" ) == 0 )
	{
		strcpy( outDSType, kDSStdAuthDIGEST_MD5 );
	}
	else
	if ( strcmp( inMechName, "CRAM-MD5" ) == 0 )
	{
		strcpy( outDSType, kDSStdAuthCRAM_MD5 );
	}
	else
	if ( strcmp( inMechName, "CRYPT" ) == 0 )
	{
		strcpy( outDSType, kDSStdAuthCrypt );
	}
	else
	if ( strcmp( inMechName, "APOP" ) == 0 )
	{
		strcpy( outDSType, kDSStdAuthAPOP );
	}
}

	
//------------------------------------------------------------------------------------
//	* PWSErrToDirServiceError
//------------------------------------------------------------------------------------

sInt32 CPSPlugIn::PWSErrToDirServiceError( PWServerError inError )
{
    sInt32 result = 0;
    
    if ( inError.err == 0 )
        return 0;
    
    switch ( inError.type )
    {
        case kPolicyError:
            result = PolicyErrToDirServiceError( inError.err );
            break;
        
        case kSASLError:
            result = SASLErrToDirServiceError( inError.err );
            break;
    }
    
    return result;
}


//------------------------------------------------------------------------------------
//	* SASLErrToDirServiceError
//------------------------------------------------------------------------------------

sInt32 CPSPlugIn::PolicyErrToDirServiceError( int inPolicyError )
{
    sInt32 dirServiceErr = eDSAuthFailed;
    
    switch( inPolicyError )
    {
        case kAuthOK:						dirServiceErr = eDSNoErr;							break;
        case kAuthFail:						dirServiceErr = eDSAuthFailed;						break;
        case kAuthUserDisabled:				dirServiceErr = eDSAuthAccountDisabled;				break;
        case kAuthNeedAdminPrivs:			dirServiceErr = eDSAuthFailed;						break;
        case kAuthUserNotSet:				dirServiceErr = eDSAuthUnknownUser;					break;
        case kAuthUserNotAuthenticated:		dirServiceErr = eDSAuthFailed;						break;
        case kAuthPasswordExpired:			dirServiceErr = eDSAuthAccountExpired;				break;
        case kAuthPasswordNeedsChange:		dirServiceErr = eDSAuthNewPasswordRequired;			break;
        case kAuthPasswordNotChangeable:	dirServiceErr = eDSAuthFailed;						break;
        case kAuthPasswordTooShort:			dirServiceErr = eDSAuthPasswordTooShort;			break;
        case kAuthPasswordTooLong:			dirServiceErr = eDSAuthPasswordTooLong;				break;
        case kAuthPasswordNeedsAlpha:		dirServiceErr = eDSAuthPasswordNeedsLetter;			break;
        case kAuthPasswordNeedsDecimal:		dirServiceErr = eDSAuthPasswordNeedsDigit;			break;
        case kAuthMethodTooWeak:			dirServiceErr = eDSAuthMethodNotSupported;			break;
    }
    
    return dirServiceErr;
}


//------------------------------------------------------------------------------------
//	* SASLErrToDirServiceError
//------------------------------------------------------------------------------------

sInt32 CPSPlugIn::SASLErrToDirServiceError(	int inSASLError )
{
    sInt32 dirServiceErr = eDSAuthFailed;

    switch (inSASLError)
    {
        case SASL_CONTINUE:		dirServiceErr = eDSNoErr;					break;
        case SASL_OK:			dirServiceErr = eDSNoErr;					break;
        case SASL_FAIL:			dirServiceErr = eDSAuthFailed;				break;
        case SASL_NOMEM:		dirServiceErr = eMemoryError;				break;
        case SASL_BUFOVER:		dirServiceErr = eDSBufferTooSmall;			break;
        case SASL_NOMECH:		dirServiceErr = eDSAuthMethodNotSupported;	break;
        case SASL_BADPROT:		dirServiceErr = eDSAuthParameterError;		break;
        case SASL_NOTDONE:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_BADPARAM:		dirServiceErr = eDSAuthParameterError;		break;
        case SASL_TRYAGAIN:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_BADMAC:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_NOTINIT:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_INTERACT:		dirServiceErr = eDSAuthParameterError;		break;
        case SASL_BADSERV:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_WRONGMECH:	dirServiceErr = eDSAuthParameterError;		break;
        case SASL_BADAUTH:		dirServiceErr = eDSAuthBadPassword;			break;
        case SASL_NOAUTHZ:		dirServiceErr = eDSAuthBadPassword;			break;
        case SASL_TOOWEAK:		dirServiceErr = eDSAuthMethodNotSupported;	break;
        case SASL_ENCRYPT:		dirServiceErr = eDSAuthInBuffFormatError;	break;
        case SASL_TRANS:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_EXPIRED:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_DISABLED:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_NOUSER:		dirServiceErr = eDSAuthUnknownUser;			break;
        case SASL_BADVERS:		dirServiceErr = eDSAuthServerError;			break;
        case SASL_UNAVAIL:		dirServiceErr = eDSAuthNoAuthServerFound;	break;
        case SASL_NOVERIFY:		dirServiceErr = eDSAuthNoAuthServerFound;	break;
        case SASL_PWLOCK:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_NOCHANGE:		dirServiceErr = eDSAuthFailed;				break;
        case SASL_WEAKPASS:		dirServiceErr = eDSAuthBadPassword;			break;
        case SASL_NOUSERPASS:	dirServiceErr = eDSAuthFailed;				break;
    }
    
    return dirServiceErr;
}


//------------------------------------------------------------------------------------
//	* DoSASLAuth
//------------------------------------------------------------------------------------

sInt32
CPSPlugIn::DoSASLAuth(
    sPSContextData *inContext,
    const char *userName,
    const char *password,
    long inPasswordLen,
    const char *inChallenge,
    const char *inMechName )
{
	sInt32			siResult			= eDSAuthFailed;
	
    DEBUGLOG( "CPSPlugIn::DoSASLAuth\n");
	try
	{
		Throw_NULL( inContext, eDSBadContextData );
        Throw_NULL( password, eParameterError );
        
		// need username length, password length, and username must be at least 1 character

		DEBUGLOG( "PasswordServer PlugIn: Attempting Authentication" );
        
		// yes do it here
        {
            char buf[4096];
            const char *data;
            char dataBuf[4096];
            const char *chosenmech = NULL;
            unsigned int len = 0;
            int r;
            PWServerError serverResult;
        	sasl_security_properties_t secprops = {0,65535,4096,0,NULL,NULL};
            
            // attach the username and password to the sasl connection's context
            // set these before calling sasl_client_start
            if ( userName != NULL )
            {
                long userNameLen;
                char *userNameEnd = strchr( userName, ',' );
                
                if ( userNameEnd != NULL )
                {
                    userNameLen = userNameEnd - userName;
                    if ( userNameLen >= kMaxUserNameLength )
                        throw( (sInt32)eDSAuthInvalidUserName );
                    
                    strncpy(inContext->last.username, userName, userNameLen );
                    inContext->last.username[userNameLen] = '\0';
                }
                else
                {
                    strncpy( inContext->last.username, userName, kMaxUserNameLength );
                    inContext->last.username[kMaxUserNameLength-1] = '\0';
                }
            }
            
            if ( inContext->last.password != NULL )
            {
                memset( inContext->last.password, 0, inContext->last.passwordLen );
                free( inContext->last.password );
                inContext->last.password = NULL;
                inContext->last.passwordLen = 0;
            }
            
            inContext->last.password = (char *) malloc( inPasswordLen + 1 );
            Throw_NULL( inContext->last.password, eMemoryError );
            
            memcpy( inContext->last.password, password, inPasswordLen );
            inContext->last.password[inPasswordLen] = '\0';
            inContext->last.passwordLen = inPasswordLen;
            
            /*
            const char **gmechs = sasl_global_listmech();
            for (r=0; gmechs[r] != NULL; r++)
                DEBUGLOG( "gmech=%s\n", gmechs[r]);
            */
            
            // clean up the old conn
            if ( inContext->conn != NULL )
            {
                gSASLMutex->Wait();
                sasl_dispose(&inContext->conn);
                gSASLMutex->Signal();
                
                inContext->conn = NULL;
            }
            
            // callbacks we support
            inContext->callbacks[0].id = SASL_CB_GETREALM;
            inContext->callbacks[0].proc = (sasl_cbproc *)&getrealm;
            inContext->callbacks[0].context = inContext;
            
            inContext->callbacks[1].id = SASL_CB_USER;
            inContext->callbacks[1].proc = (sasl_cbproc *)&simple;
            inContext->callbacks[1].context = inContext;
            
            inContext->callbacks[2].id = SASL_CB_AUTHNAME;
            inContext->callbacks[2].proc = (sasl_cbproc *)&simple;
            inContext->callbacks[2].context = inContext;
            
            inContext->callbacks[3].id = SASL_CB_PASS;
            inContext->callbacks[3].proc = (sasl_cbproc *)&getsecret;
            inContext->callbacks[3].context = inContext;

            inContext->callbacks[4].id = SASL_CB_LIST_END;
            inContext->callbacks[4].proc = NULL;
            inContext->callbacks[4].context = NULL;
            
            gSASLMutex->Wait();
            r = sasl_client_new( "rcmd",
                                inContext->psName,
                                inContext->localaddr,
                                inContext->remoteaddr,
                                inContext->callbacks,
                                0,
                                &inContext->conn);
            gSASLMutex->Signal();
            
            if ( r != SASL_OK || inContext->conn == NULL ) {
                DEBUGLOG( "sasl_client_new failed, err=%d.\n", r);
                throw( SASLErrToDirServiceError(r) );
            }
            
            gSASLMutex->Wait();
            r = sasl_setprop(inContext->conn, SASL_SEC_PROPS, &secprops);
            r = sasl_client_start( inContext->conn, inMechName, NULL, &data, &len, &chosenmech ); 
            gSASLMutex->Signal();
            
            DEBUGLOG( "chosenmech=%s, datalen=%u\n", chosenmech, len);
            
            if ( r != SASL_OK && r != SASL_CONTINUE ) {
                DEBUGLOG( "starting SASL negotiation, err=%d\n", r);
                throw( SASLErrToDirServiceError(r) );
            }
            
            // set a user
            snprintf(dataBuf, 4096, "USER %s\r\n", userName);
            dataBuf[4095] = '\0';
            writeToServer(inContext->serverOut, dataBuf);
            
            // flush the read buffer
            serverResult = readFromServer( inContext->serverIn, buf, 4096 );
            if (serverResult.err != 0) {
                DEBUGLOG( "server returned an error, err=%d\n", serverResult.err);
                throw( PWSErrToDirServiceError(serverResult) );
            }
            
            // send the auth method
            dataBuf[0] = 0;
            if ( len > 0 )
            	ConvertBinaryToHex( (const unsigned char *)data, len, dataBuf );
            else
            if ( inChallenge != NULL )
            {
                // for CRAM-MD5 and potentially DIGEST-MD5, we can attach the nonce to the
                // initial data.
                ConvertBinaryToHex( (const unsigned char *)inChallenge, strlen(inChallenge), dataBuf );
                len = strlen(dataBuf);
            }
            
            if ( len > 0 )
                snprintf(buf, 4096, "AUTH %s %s\r\n", chosenmech, dataBuf);
            else
                snprintf(buf, 4096, "AUTH %s\r\n", chosenmech);
            buf[4095] = '\0';
            writeToServer(inContext->serverOut, buf);
            
            // get server response
            serverResult = readFromServer(inContext->serverIn, buf, 4096);
            if (serverResult.err != 0) {
                DEBUGLOG( "server returned an error, err=%d\n", serverResult.err);
                throw( PWSErrToDirServiceError(serverResult) );
            }
            
            chop(buf);
            len = strlen(buf);
            
            while ( 1 )
            {
                // skip the "+OK " at the begining of the response
                if ( len >= 3 && strncmp( buf, "+OK", 3 ) == 0 )
                {
                    if ( len > 4 )
                    {
                        unsigned long binLen;
                        
                        ConvertHexToBinary( buf + 4, (unsigned char *) dataBuf, &binLen );
                        
                        gSASLMutex->Wait();
                        r = sasl_client_step(inContext->conn, dataBuf, binLen, NULL, &data, &len);
                        gSASLMutex->Signal();
                    }
                    else
                    {
                        // we're done
                        data = NULL;
                        len = 0;
                        r = SASL_OK;
                    }
                }
                else 
                    r = SASL_FAIL;
				
                if (r != SASL_OK && r != SASL_CONTINUE) {
                    DEBUGLOG( "sasl_client_step=%d\n", r);
                    throw( SASLErrToDirServiceError(r) );
                }
                
                if (data && len != 0)
                {
                    DEBUGLOG( "sending response length %d...\n", len);
                    //DEBUGLOG( "client step data = %s", data );
                    
                    ConvertBinaryToHex( (const unsigned char *)data, len, dataBuf );
                    
                    DEBUGLOG( "AUTH2 %s\r\n", dataBuf);
                    fprintf(inContext->serverOut, "AUTH2 %s\r\n", dataBuf );
                    fflush(inContext->serverOut);
                }
                else
                if (r==SASL_CONTINUE)
                {
                    DEBUGLOG( "sending null response...\n");
                    //send_string(out, "", 0);
                    fprintf(inContext->serverOut, "AUTH2 \r\n" );
                    fflush(inContext->serverOut);
                }
                else
                    break;
                
                serverResult = readFromServer(inContext->serverIn, buf, 4096 );
                if ( serverResult.err != 0 ) {
                    DEBUGLOG( "server returned an error, err=%d\n", serverResult.err);
                    throw( PWSErrToDirServiceError(serverResult) );
                }
                
                chop(buf);
                len = strlen(buf);
                
                if ( r != SASL_CONTINUE )
                    break;
            }
            
            throw( SASLErrToDirServiceError(r) );
        }

	}

	catch ( sInt32 err )
	{
		DEBUGLOG( "PasswordServer PlugIn: SASL authentication error %l", err );
		siResult = err;
	}
	catch ( ... )
	{
		DEBUGLOG( "PasswordServer PlugIn: SASL uncasted authentication error" );
		siResult = eDSAuthFailed;
	}
	
	return( siResult );

} // DoSASLAuth


//------------------------------------------------------------------------------------
//	* DoSASLTwoWayRandAuth
//------------------------------------------------------------------------------------

sInt32
CPSPlugIn::DoSASLTwoWayRandAuth(
    sPSContextData *inContext,
    const char *userName,
    const char *inMechName,
    sDoDirNodeAuth *inData )
{
	sInt32			siResult			= eDSAuthFailed;
    char			buf[4096];
    const char		*data;
    char			dataBuf[4096];
    const char		*chosenmech = NULL;
    unsigned int	len = 0;
    int				r;
    PWServerError	serverResult;
	
    sasl_security_properties_t secprops = {0,65535,4096,0,NULL,NULL};
	sPSContinueData *pContinue = (sPSContinueData *) inData->fIOContinueData;
    tDataBufferPtr outAuthBuff = inData->fOutAuthStepDataResponse;
    tDataBufferPtr inAuthBuff = inData->fInAuthStepData;
    
    DEBUGLOG( "CPSPlugIn::DoSASLTwoWayRandAuth\n");
	try
	{
		Throw_NULL( inContext, eDSBadContextData );
        Throw_NULL( inMechName, eParameterError );
        Throw_NULL( inData, eParameterError );
        Throw_NULL( inAuthBuff, eDSNullAuthStepData );
        Throw_NULL( outAuthBuff, eDSNullAuthStepDataResp );
        Throw_NULL( pContinue, eDSAuthContinueDataBad );
        
        if ( outAuthBuff->fBufferSize < 8 )
            throw( (sInt32)eDSAuthResponseBufTooSmall );
        
		// need username length, password length, and username must be at least 1 character
        // This information may not come in the first step, so check each step.
        
		DEBUGLOG( "PasswordServer PlugIn: Attempting Authentication" );
        
        if ( pContinue->fAuthPass == 0 )
        {
            // first pass contains the user name
            if ( userName != NULL )
            {
                long userNameLen;
                char *userNameEnd = strchr( userName, ',' );
                
                if ( userNameEnd != NULL )
                {
                    userNameLen = userNameEnd - userName;
                    if ( userNameLen >= kMaxUserNameLength )
                        throw( (sInt32)eDSAuthInvalidUserName );
                    
                    strncpy(inContext->last.username, userName, userNameLen );
                    inContext->last.username[userNameLen] = '\0';
                }
                else
                {
                    strncpy( inContext->last.username, userName, kMaxUserNameLength );
                    inContext->last.username[kMaxUserNameLength-1] = '\0';
                }
            }
            
            // clean up the old conn
            if ( inContext->conn != NULL ) {
                sasl_dispose(&inContext->conn);
                inContext->conn = NULL;
            }
            
			// callbacks we support
            inContext->callbacks[0].id = SASL_CB_GETREALM;
            inContext->callbacks[0].proc = (sasl_cbproc *)&getrealm;
            inContext->callbacks[0].context = inContext;
            
            inContext->callbacks[1].id = SASL_CB_USER;
            inContext->callbacks[1].proc = (sasl_cbproc *)&simple;
            inContext->callbacks[1].context = inContext;
            
            inContext->callbacks[2].id = SASL_CB_AUTHNAME;
            inContext->callbacks[2].proc = (sasl_cbproc *)&simple;
            inContext->callbacks[2].context = inContext;
            
            inContext->callbacks[3].id = SASL_CB_PASS;
            inContext->callbacks[3].proc = (sasl_cbproc *)&getsecret;
            inContext->callbacks[3].context = inContext;

            inContext->callbacks[4].id = SASL_CB_LIST_END;
            inContext->callbacks[4].proc = NULL;
            inContext->callbacks[4].context = NULL;
            
            gSASLMutex->Wait();
            r = sasl_client_new( "rcmd",
                                inContext->psName,
                                inContext->localaddr,
                                inContext->remoteaddr,
                                inContext->callbacks,
                                0,
                                &inContext->conn);
            gSASLMutex->Signal();
            
            if ( r != SASL_OK || inContext->conn == NULL ) {
                DEBUGLOG( "sasl_client_new failed, err=%d.\n", r);
                throw( SASLErrToDirServiceError(r) );
            }
            
            r = sasl_setprop(inContext->conn, SASL_SEC_PROPS, &secprops);
            
            // set a user
            snprintf(dataBuf, sizeof(dataBuf), "USER %s\r\n", userName);
            writeToServer(inContext->serverOut, dataBuf);
            
            // flush the read buffer
            serverResult = readFromServer( inContext->serverIn, buf, sizeof(buf) );
            if (serverResult.err != 0) {
                DEBUGLOG( "server returned an error, err=%d\n", serverResult.err);
                throw( PWSErrToDirServiceError(serverResult) );
            }
            
            // send the auth method
            snprintf(buf, sizeof(buf), "AUTH %s\r\n", inMechName);
            writeToServer(inContext->serverOut, buf);
            
            // get server response
            serverResult = readFromServer(inContext->serverIn, buf, sizeof(buf));
            if (serverResult.err != 0) {
                DEBUGLOG( "server returned an error, err=%d\n", serverResult.err);
                throw( PWSErrToDirServiceError(serverResult) );
            }
            
            chop(buf);
            len = strlen(buf);
            
            // skip the "+OK " at the begining of the response
            if ( len >= 3 && strncmp( buf, "+OK", 3 ) == 0 )
            {
                if ( len > 4 )
                {
                    unsigned long binLen;
                    unsigned long num1, num2;
                    char *num2Ptr = NULL;
                    unsigned char *saveData = NULL;
                    
                    ConvertHexToBinary( buf + 4, (unsigned char *) dataBuf, &binLen );
                    dataBuf[binLen] = '\0';
                    
                    // save a copy for the next pass (do not trust the client)
                    saveData = (unsigned char *) malloc(binLen + 1);
                    Throw_NULL( saveData, eMemoryError );
                    
                    memcpy(saveData, dataBuf, binLen+1);
                    pContinue->fData = saveData;
                    pContinue->fDataLen = binLen;
                    
                    // make an out buffer
                    num2Ptr = strchr( dataBuf, ' ' );
                    
                    if ( binLen < 3 || num2Ptr == NULL )
                        throw( eDSInvalidBuffFormat );
                    
                    sscanf(dataBuf, "%lu", &num1);
                    sscanf(num2Ptr+1, "%lu", &num2);
                    
                    outAuthBuff->fBufferLength = 8;
                    memcpy(outAuthBuff->fBufferData, &num1, sizeof(long));
                    memcpy(outAuthBuff->fBufferData + sizeof(long), &num2, sizeof(long));
                    
                    siResult = eDSNoErr;
                }
                else
                {
                    // we're done, although it would be odd to finish here since
                    // it's a multi-pass auth.
                    data = NULL;
                    len = 0;
                    r = SASL_OK;
                }
            }
            else
            {
                r = SASL_FAIL;
            }
        }
        else
        if ( pContinue->fAuthPass == 1 )
        {
            DEBUGLOG( "inAuthBuff->fBufferLength=%lu\n", inAuthBuff->fBufferLength);
            
            // buffer should be:
            // 8 byte DES digest
            // 8 bytes of random
            if ( inAuthBuff->fBufferLength < 16 )
                throw( (sInt32)eDSAuthInBuffFormatError );
            
            // attach the username and password to the sasl connection's context
            // set these before calling sasl_client_start
            if ( inContext->last.password != NULL )
            {
                memset( inContext->last.password, 0, inContext->last.passwordLen );
                free( inContext->last.password );
                inContext->last.password = NULL;
                inContext->last.passwordLen = 0;
            }
            
            inContext->last.password = (char *) malloc( inAuthBuff->fBufferLength );
            Throw_NULL( inContext->last.password, eMemoryError );
            
            memcpy( inContext->last.password, inAuthBuff->fBufferData, inAuthBuff->fBufferLength );
            inContext->last.passwordLen = inAuthBuff->fBufferLength;
            
            // start sasling
            r = sasl_client_start( inContext->conn, inMechName, NULL, &data, &len, &chosenmech ); 
            DEBUGLOG( "chosenmech=%s, datalen=%u\n", chosenmech, len);
            
            if ( r != SASL_OK && r != SASL_CONTINUE ) {
                DEBUGLOG( "starting SASL negotiation, err=%d\n", r);
                throw( SASLErrToDirServiceError(r) );
            }
            
            r = sasl_client_step(inContext->conn, (const char *)pContinue->fData, pContinue->fDataLen, NULL, &data, &len);
            
            // clean up
            if ( pContinue->fData != NULL ) {
                free( pContinue->fData );
                pContinue->fData = NULL;
            }
            pContinue->fDataLen = 0;
            
            if ( r != SASL_OK && r != SASL_CONTINUE ) {
                DEBUGLOG( "stepping SASL negotiation, err=%d\n", r);
                throw( SASLErrToDirServiceError(r) );
            }
            
            if (data && len != 0)
            {
                ConvertBinaryToHex( (const unsigned char *)data, len, dataBuf );
                
                DEBUGLOG( "AUTH2 %s\r\n", dataBuf);
                fprintf(inContext->serverOut, "AUTH2 %s\r\n", dataBuf );
                fflush(inContext->serverOut);
                
                // get server response
                serverResult = readFromServer(inContext->serverIn, buf, sizeof(buf));
                if (serverResult.err != 0) {
                    DEBUGLOG( "server returned an error, err=%d\n", serverResult.err);
                    throw( PWSErrToDirServiceError(serverResult) );
                }
                
                chop(buf);
                len = strlen(buf);
                
                // make an out buffer
                if ( len > 4 )
                {
                    unsigned long binLen;
                    
                    ConvertHexToBinary( buf + 4, (unsigned char *)dataBuf, &binLen );
                    if ( binLen > outAuthBuff->fBufferSize )
                        throw( (sInt32)eDSAuthResponseBufTooSmall );
                    
                    outAuthBuff->fBufferLength = binLen;
                    memcpy(outAuthBuff->fBufferData, dataBuf, binLen);
                    
                    siResult = eDSNoErr;
                }
            }
        }
        else
        {
            // too many passes
            siResult = eDSAuthFailed;
        }
	}

	catch ( sInt32 err )
	{
		DEBUGLOG( "PasswordServer PlugIn: SASL authentication error %l", err );
		siResult = err;
	}
	catch ( ... )
	{
		DEBUGLOG( "PasswordServer PlugIn: SASL uncasted authentication error" );
		siResult = eDSAuthFailed;
	}
	
    if ( pContinue->fAuthPass == 1 )
    {
        gContinue->RemoveItem( pContinue );
        inData->fIOContinueData = NULL;
    }
    else
    {
        pContinue->fAuthPass++;
    }
    
	return( siResult );
}
                                                        

//------------------------------------------------------------------------------------
//      * DoPlugInCustomCall
//------------------------------------------------------------------------------------ 

sInt32 CPSPlugIn::DoPlugInCustomCall ( sDoPlugInCustomCall *inData )
{
	sInt32			siResult	= eDSNoErr;

	//TODO do we need this?
	
	return( siResult );

} // DoPlugInCustomCall


// ---------------------------------------------------------------------------
//	* ContinueDeallocProc
// ---------------------------------------------------------------------------

void CPSPlugIn::ContinueDeallocProc ( void* inContinueData )
{
	sPSContinueData *pContinue = (sPSContinueData *)inContinueData;
    
	if ( pContinue != nil )
	{
		if ( pContinue->fData != NULL )
		{
			free( pContinue->fData );
			pContinue->fData = NULL;
		}
		
		free( pContinue );
		pContinue = nil;
	}
} // ContinueDeallocProc


// ---------------------------------------------------------------------------
//	* ContextDeallocProc
// ---------------------------------------------------------------------------

void CPSPlugIn::ContextDeallocProc ( void* inContextData )
{
	sPSContextData *pContext = (sPSContextData *) inContextData;

	if ( pContext != NULL )
	{
		CleanContextData( pContext );
		
		free( pContext );
		pContext = NULL;
	}
} // ContextDeallocProc

