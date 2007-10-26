 #include <stdio.h>
#include <string.h>		//used for strcpy, etc.
#include <stdlib.h>		//used for malloc
#include <stdarg.h>
#include <ctype.h>
#include <sysexits.h>
#include <errno.h>

#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <openssl/md5.h>
#include <openssl/bn.h>
#include <openssl/blowfish.h>

int OLConvertBinaryToHex( const unsigned char *inData, long len, char *outHexStr );

#include <PasswordServer/SASLCode.h>

// #include <PasswordServer/CPSUtilities.h> // Tiger
#include <PasswordServer/PSUtilitiesDefs.h>
#include <PasswordServer/key.h>

#include "psauth.h"
#include "sasl.h"
#include "saslutil.h"


#define kBigBuffSize						4096

#define kSASLListPrefix						"(SASL "
#define kPasswordServerErrPrefixStr			"-ERR "
#define kPasswordServerSASLErrPrefixStr		"SASL "

typedef struct sSASLContext {
	sasl_secret_t *secret;
	char username[35];
} sSASLContext;

int getrealm(void *context /*__attribute__((unused))*/, 
		    int id,
		    const char **availrealms,
		    const char **result);
int ol_simple(void *context /*__attribute__((unused))*/,
		  int id,
		  const char **result,
		  unsigned *len);
int
ol_getsecret(sasl_conn_t *conn,
	  void *context /*__attribute__((unused))*/,
	  int id,
	  sasl_secret_t **psecret);

int DoRSAValidation ( sPSContextData *inContext, const char *inUserKey );
long BeginServerSession( sPSContextData *inContext, int inSock );
long EndServerSession( sPSContextData *inContext, bool inSendQuit );
int GetRSAPublicKey( sPSContextData *inContext, char *inData );
long OLHandleFirstContact( sPSContextData *inContext, const char *inIP, const char *inKeyHash );

//-------------

int ol_simple(void *context /*__attribute__((unused))*/,
		  int id,
		  const char **result,
		  unsigned *len)
{
#ifdef DEBUG_PRINTFS
    printf("in simple\n");
#endif

    /* paranoia check */
    if (! result)
        return SASL_BADPARAM;
    
    switch (id) {
        case SASL_CB_USER:
        case SASL_CB_AUTHNAME:
            //printf("please enter an authorization id: ");
            *result = ((sSASLContext *)context)->username;
            if (len != NULL)
                *len = strlen(((sSASLContext *)context)->username);
#ifdef DEBUG_PRINTFS
            printf("simple - user = %s (len = %d)\n", *result, *len);
#endif
            break;
        
        default:
            return SASL_BADPARAM;
    }
  
    return SASL_OK;
}


int
ol_getsecret(sasl_conn_t *conn,
	  void *context /*__attribute__((unused))*/,
	  int id,
	  sasl_secret_t **psecret)
{    
#ifdef DEBUG_PRINTFS
    printf("in getsecret\n");
#endif
    /* paranoia check */
    if (! conn || ! psecret || id != SASL_CB_PASS)
        return SASL_BADPARAM;
    
    *psecret = ((sSASLContext *)context)->secret;
    return SASL_OK;
}


void OLCalcServerUniqueID( const char *inRSAPublicKey, char *outHexHash )
{
	MD5_CTX ctx;
	unsigned char pubKeyHash[MD5_DIGEST_LENGTH];
	
	if ( inRSAPublicKey == NULL || outHexHash == NULL )
		return;
	
	MD5_Init( &ctx );
	MD5_Update( &ctx, inRSAPublicKey, strlen(inRSAPublicKey) );
	MD5_Final( pubKeyHash, &ctx );
	
	outHexHash[0] = 0;
	OLConvertBinaryToHex( pubKeyHash, MD5_DIGEST_LENGTH, outHexHash );
}


//-----------------------------------------------------------------------------
//	OLConvertBinaryToHex
//-----------------------------------------------------------------------------

int OLConvertBinaryToHex( const unsigned char *inData, long len, char *outHexStr )
{
    bool result = true;
	char *tptr = outHexStr;
	int idx;
	char base16table[16] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };
	
    if ( inData == nil || outHexStr == nil )
        return false;
    
	for ( idx = 0; idx < len; idx++ )
	{
		*tptr++ = base16table[(inData[idx] >> 4) & 0x0F];
		*tptr++ = base16table[(inData[idx] & 0x0F)];
	}
	*tptr = '\0';
		
	return result;
}


// ---------------------------------------------------------------------------
//	* OLHandleFirstContact
// ---------------------------------------------------------------------------

long OLHandleFirstContact( sPSContextData *inContext, const char *inIP, const char *inKeyHash )
{
	long siResult = 0;
	char *psName;
	CFDataRef serverRef;
	bool usingLocalCache = false;
	bool usingConfigRecord = false;
	int sock = -1;
	sPSServerEntry anEntry;
	
	bzero( &anEntry, sizeof(anEntry) );
	
	do
	{
		if ( inContext->serverList != NULL )
			CFRelease( inContext->serverList );
		
		// try the directory's config record if provided
		if ( inContext->replicaFile != NULL )
		{
			siResult = GetServerListFromConfig( &inContext->serverList, inContext->replicaFile );
			if ( siResult == 0 && inContext->serverList != NULL && CFArrayGetCount( inContext->serverList ) > 0 )
				siResult = IdentifyReachableReplica( inContext->serverList, inKeyHash, &anEntry, &sock );
			
			// if provided, the config list is the only list used
			if ( siResult != 0 )
				break;
				
			usingConfigRecord = true;
		}
		
		if ( ! usingConfigRecord )
		{
			// try the local cache
			siResult = GetPasswordServerList( &inContext->serverList, kPWSearchLocalFile );
			if ( siResult == 0 && inContext->serverList != NULL && CFArrayGetCount( inContext->serverList ) > 0 )
			{
				siResult = IdentifyReachableReplica( inContext->serverList, inKeyHash, &anEntry, &sock );
				usingLocalCache = ( siResult == 0 );
			}
		
			// try the replication database
			if ( siResult != 0 || !usingLocalCache )
			{
				if ( inContext->serverList != NULL )
					CFRelease( inContext->serverList );
					
				siResult = GetPasswordServerList( &inContext->serverList, kPWSearchReplicaFile );
				if ( siResult == 0 && inContext->serverList != NULL && CFArrayGetCount( inContext->serverList ) > 0 )
					siResult = IdentifyReachableReplica( inContext->serverList, inKeyHash, &anEntry, &sock );
			}
			
			// try rendezvous
			if ( siResult != 0 )
			{
				if ( inContext->serverList != NULL )
					CFRelease( inContext->serverList );
					
				siResult = GetPasswordServerList( &inContext->serverList, kPWSearchRegisteredServices );
				if ( siResult == 0 && inContext->serverList != NULL && CFArrayGetCount( inContext->serverList ) > 0 )
					siResult = IdentifyReachableReplica( inContext->serverList, inKeyHash, &anEntry, &sock );
			}
		}
		
		// try node IP
		if ( inContext->serverList == NULL || siResult != 0 )
		{
			if ( inContext->serverList != NULL )
				CFRelease( inContext->serverList );
				
			inContext->serverList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
			if ( inContext->serverList == NULL ) {
				siResult = kAuthOtherError;
				break;
			}
			
			serverRef = CFDataCreate( kCFAllocatorDefault, (const unsigned char *)&inContext->serverProvidedFromNode, sizeof(sPSServerEntry) );
			if ( serverRef == NULL ) {
				siResult = kAuthOtherError;
				break;
			}
			
			CFArrayAppendValue( inContext->serverList, serverRef );
			CFRelease( serverRef );
			
			siResult = IdentifyReachableReplica( inContext->serverList, inKeyHash, &anEntry, &sock );
		}
		
		if ( siResult != 0 || anEntry.ip[0] == '\0' ) {
			siResult = kAuthOtherError;
			break;
		}
		
		psName = (char *) calloc( 1, strlen(anEntry.ip) + 1 );
		if ( psName == NULL ) {
			siResult = kAuthOtherError;
			break;
		}
		
		strcpy( psName, anEntry.ip );
		
		if ( inContext->psName != NULL )
			free( inContext->psName );
		inContext->psName = psName;
		
		strncpy(inContext->psPort, anEntry.port, 10);
		inContext->psPort[9] = '\0';
		
		siResult = BeginServerSession( inContext, sock );
		if ( siResult == 0 )
		{
			if ( inKeyHash != NULL && strcmp( inKeyHash, inContext->rsaPublicKeyHash ) != 0 )
			{
				EndServerSession( inContext, true );
				siResult = kAuthOtherError;
				
				if ( usingLocalCache )
				{
					struct stat sb;
					
					if ( stat( kPWReplicaLocalFile, &sb ) == 0 )
					{
						remove( kPWReplicaLocalFile );
						siResult = OLHandleFirstContact( inContext, inIP, inKeyHash );
					}
				}
			}
		}
	}
	while ( 0 );
	
	return siResult;
}


// ---------------------------------------------------------------------------
//	* BeginServerSession
// ---------------------------------------------------------------------------

long BeginServerSession( sPSContextData *inContext, int inSock )
{
	long siResult = 0;
	unsigned count;
	char *tptr, *end;
	char buf[4096];
	PWServerError serverResult;
	int salen;
	char hbuf[NI_MAXHOST], pbuf[NI_MAXSERV];
	struct sockaddr_storage local_ip;
	
	do
	{
		if ( inSock != -1 )
		{
			inContext->fd = inSock;
			inContext->serverOut = fdopen(inSock, "w");
			
			// discard the greeting message
			readFromServer(inSock, buf, sizeof(buf));
		}
		else
		{
			// connect to remote server
			siResult = ConnectToServer( inContext );
			if ( siResult != 0 )
				break;
		}
		
		// set ip addresses
		salen = sizeof(local_ip);
		if (getsockname(inContext->fd, (struct sockaddr *)&local_ip, &salen) < 0) {
		}
		
		getnameinfo((struct sockaddr *)&local_ip, salen,
					hbuf, sizeof(hbuf), pbuf, sizeof(pbuf),
					NI_NUMERICHOST | NI_WITHSCOPEID | NI_NUMERICSERV);
		snprintf(inContext->localaddr, sizeof(inContext->localaddr), "%s;%s", hbuf, pbuf);
		snprintf(inContext->remoteaddr, sizeof(inContext->remoteaddr), "%s;%s", inContext->psName, inContext->psPort);
		
		// retrieve the password server's list of available auth methods
		serverResult = SendFlushRead( inContext, "LIST RSAPUBLIC", NULL, NULL, buf, sizeof(buf) );
		if ( serverResult.err != 0 ) {
			siResult = kAuthOtherError;
			break;
		}
		
		sasl_chop(buf);
		tptr = buf;
		for (count=0; tptr; count++ ) {
			tptr = strchr( tptr, ' ' );
			if (tptr) tptr++;
		}
		
		if (count > 0) {
			inContext->mech = (AuthMethName *)calloc(count, sizeof(AuthMethName));
			if ( inContext->mech == NULL ) {
				siResult = kAuthOtherError;
				break;
			}
			
			inContext->mechCount = count;
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
					
				strcpy( inContext->mech[count-1].method, tptr );
				
				tptr = end;
				if ( tptr != NULL )
					tptr += 2;
			}
		}
		
		// did the rsa public key come too?
		if ( recvfrom( inContext->fd, buf, 1, (MSG_DONTWAIT | MSG_PEEK), NULL, NULL ) > 0 )
		{
			serverResult = readFromServer( inContext->fd, buf, sizeof(buf) );
			if ( serverResult.err != 0 ) {
				siResult = kAuthOtherError;
				break;
			}
			
			siResult = GetRSAPublicKey( inContext, buf );
		}
		else
		{
			// retrieve the password server's public RSA key
			siResult = GetRSAPublicKey( inContext, NULL );
		}
		
		if ( siResult != 0 )
			break;
	}
	while ( 0 );
	
	return siResult;
}
    

// ---------------------------------------------------------------------------
//	* EndServerSession
// ---------------------------------------------------------------------------

long EndServerSession( sPSContextData *inContext, bool inSendQuit )
{
	if ( inSendQuit )
	{
		if ( Connected( inContext ) )
		{
			int result;
			struct timeval recvTimeoutVal = { 0, 150000 };
			char buf[1024];
			
			result = setsockopt( inContext->fd, SOL_SOCKET, SO_RCVTIMEO, &recvTimeoutVal, sizeof(recvTimeoutVal) );
			
			writeToServer( inContext->serverOut, "QUIT\r\n" );
            readFromServer( inContext->fd, buf, sizeof(buf) );
        }
	}
	
	if ( inContext->serverOut != NULL ) {
		fpurge( inContext->serverOut );
		fclose( inContext->serverOut );
		inContext->serverOut = NULL;
	}
	if ( inContext->fd > 0 ) {
		close( inContext->fd );
	}
	// always set to -1
	inContext->fd = -1;
	
	return 0;
}


// ---------------------------------------------------------------------------
//	* GetRSAPublicKey
// ---------------------------------------------------------------------------

int GetRSAPublicKey( sPSContextData *inContext, char *inData )
{
	char				*keyStr				= NULL;
	char				*bufPtr				= NULL;
    int					bits				= 0;
    PWServerError		serverResult;
    char				buf[1024];
    
    if ( inContext == NULL )
        return kAuthOtherError;

	if ( inData == NULL )
	{
		// get string
		serverResult = SendFlushRead( inContext, "RSAPUBLIC", NULL, NULL, buf, sizeof(buf) );
		if ( serverResult.err != 0 )
			return kAuthKeyError;
		
		bufPtr = buf;
	}
	else
	{
		bufPtr = inData;
	}
		
	sasl_chop( bufPtr );
	inContext->rsaPublicKeyStr = (char *) calloc( 1, strlen(bufPtr)+1 );
	if ( inContext->rsaPublicKeyStr == NULL )
		return kAuthKeyError;
	
	strcpy( inContext->rsaPublicKeyStr, bufPtr + 4 );
	
	// get as struct
	inContext->rsaPublicKey = key_new( KEY_RSA );
	if ( inContext->rsaPublicKey == NULL )
		return kAuthKeyError;
        
	keyStr = bufPtr + 4;
	bits = pwsf_key_read(inContext->rsaPublicKey, &keyStr);
	if (bits == 0)
		return kAuthKeyError;
	
	// calculate the hash so we can check this later
	OLCalcServerUniqueID( inContext->rsaPublicKeyStr, inContext->rsaPublicKeyHash );
	
    return kAuthNoError;
}


// ---------------------------------------------------------------------------
//	* InitConnection
// ---------------------------------------------------------------------------

int InitConnection(sPSContextData* pContext, sSASLContext *saslContext, char* psName, char* rsakey, sasl_callback_t callbacks[5])
{
	long result;
	sasl_security_properties_t secprops = {0,65535,4096,0,NULL,NULL};
	char hexHash[34];
	
	pContext->fd = -1;
	
	// callbacks we support
	callbacks[0].id = SASL_CB_GETREALM;
	callbacks[0].proc = (sasl_cbproc *)&getrealm;
	callbacks[0].context = saslContext;
	
	callbacks[1].id = SASL_CB_USER;
	callbacks[1].proc = (sasl_cbproc *)&ol_simple;
	callbacks[1].context = saslContext;
	
	callbacks[2].id = SASL_CB_AUTHNAME;
	callbacks[2].proc = (sasl_cbproc *)&ol_simple;
	callbacks[2].context = saslContext;
	
	callbacks[3].id = SASL_CB_PASS;
	callbacks[3].proc = (sasl_cbproc *)&ol_getsecret;
	callbacks[3].context = saslContext;
	
	callbacks[4].id = SASL_CB_LIST_END;
	callbacks[4].proc = NULL;
	callbacks[4].context = NULL;
	
	if ( rsakey != NULL )
	{
		OLCalcServerUniqueID( rsakey, hexHash );
		result = OLHandleFirstContact( pContext, NULL, hexHash );
	}
	else
	{
		result = OLHandleFirstContact( pContext, NULL, NULL );
	}
	
	if ( result != kAuthNoError )
		return result;
	
	result = sasl_client_new("rcmd", psName, pContext->localaddr, pContext->remoteaddr, callbacks, 0, &pContext->conn);
	if ( result != SASL_OK || pContext->conn == NULL ) {
#ifdef DEBUG_PRINTFS
		printf("sasl_client_new failed.\n");
#endif
		return kAuthOtherError;
	}
	
	result = sasl_setprop(pContext->conn, SASL_SEC_PROPS, &secprops);
	
    return kAuthNoError;
}

int DoSASLAuth(
    sPSContextData *inContext,
    sSASLContext *inSASLContext,
    const char *userName,
    const char *password,
    const char *inMechName )
{
	int			siResult			= kAuthOtherError;
	int			nameLen				= 0;
	int			pwdLen				= 0;
	
    // need username length, password length, and username must be at least 1 character

    // Get the length of the user name
    nameLen = strlen(userName);
    
    // Get the length of the user password
    pwdLen = strlen(password);
    
    //TODO yes do it here
    {
        char buf[kBigBuffSize];
        const char *data;
        char dataBuf[kBigBuffSize];
        const char *chosenmech = NULL;
        unsigned int len = 0;
        int r;
        PWServerError serverResult;
                    
        // attach the username and password to the sasl connection's context
        // set these before calling sasl_client_start
        strncpy(inSASLContext->username, userName, 35);
        inSASLContext->username[34] = '\0';
        
    
        inSASLContext->secret = (sasl_secret_t *) malloc(sizeof(sasl_secret_t) + pwdLen + 1);
        if ( inSASLContext->secret == NULL )
            return SASL_NOMEM;
        
        inSASLContext->secret->len = pwdLen;
        strcpy((char *)inSASLContext->secret->data, password);
		        
#ifdef DEBUG_PRINTFS
        const char **gmechs = sasl_global_listmech();
        for (r=0; gmechs[r] != NULL; r++)
            fprintf(stderr, "gmech=%s\n", gmechs[r]);
#endif
        
        r = sasl_client_start(inContext->conn, inMechName, NULL, &data, &len, &chosenmech); 
#ifdef DEBUG_PRINTFS
        printf("chosenmech=%s, datalen=%u\n", chosenmech, len);
#endif        
        if (r != SASL_OK && r != SASL_CONTINUE) {
#ifdef DEBUG_PRINTFS
            printf("starting SASL negotiation, err=%d\n", r);
#endif
            return kAuthSASLError;
        }
        
        // set a user
#ifdef DEBUG_PRINTFS
        printf("USER %s\r\n", userName);
#endif
        snprintf(dataBuf, sizeof(dataBuf), "USER %s\r\n", userName);
        writeToServer(inContext->serverOut, dataBuf);
        
        // flush the read buffer; don't care what the response is for now
        readFromServer( inContext->fd, buf, sizeof(buf) );
        
        // send the auth method
        dataBuf[0] = 0;
        if ( len > 0 )
            OLConvertBinaryToHex( (const unsigned char *)data, len, dataBuf );
        
#ifdef DEBUG_PRINTFS
        printf("AUTH %s %s\r\n", chosenmech, dataBuf);
#endif
        if ( len > 0 )
            snprintf(buf, sizeof(buf), "AUTH %s %s\r\n", chosenmech, dataBuf);
        else
            snprintf(buf, sizeof(buf), "AUTH %s\r\n", chosenmech);
        writeToServer(inContext->serverOut, buf);
        
        // get server response
        serverResult = readFromServer(inContext->fd, buf, sizeof(buf));
        if (serverResult.err != 0) {
#ifdef DEBUG_PRINTFS
            printf("server returned an error, err=%d\n", serverResult);
#endif
            return kAuthenticationError;
        }
        
        sasl_chop(buf);
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
                    r = sasl_client_step(inContext->conn, dataBuf, binLen, NULL, &data, &len);
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
                r = -1;
            if (r != SASL_OK && r != SASL_CONTINUE) {
#ifdef DEBUG_PRINTFS
                printf("sasl_client_step=%d\n", r);
#endif
                return kAuthSASLError;
            }
            
            if (data && len != 0)
            {
#ifdef DEBUG_PRINTFS
                printf("sending response length %d...\n", len);
#endif
                OLConvertBinaryToHex( (const unsigned char *)data, len, dataBuf );
                
#ifdef DEBUG_PRINTFS
                printf("AUTH2 %s\r\n", dataBuf);
#endif
                fprintf(inContext->serverOut, "AUTH2 %s\r\n", dataBuf );
                fflush(inContext->serverOut);
            }
            else
            if (r==SASL_CONTINUE)
            {
#ifdef DEBUG_PRINTFS
                printf("sending null response...\n");
#endif
                //send_string(out, "", 0);
                fprintf(inContext->serverOut, "AUTH2 \r\n" );
                fflush(inContext->serverOut);
            }
            else
                break;
            
            serverResult = readFromServer(inContext->fd, buf, sizeof(buf) );
            if ( serverResult.err != 0 ) {
#ifdef DEBUG_PRINTFS
                printf("server returned an error, err=%d\n", serverResult);
#endif
                return kAuthenticationError;
            }
            
            sasl_chop(buf);
            len = strlen(buf);
            
            if ( r != SASL_CONTINUE )
                break;
        }
        
        return r;
    }
	
	return( siResult );

} // DoSASLAuth

int DoRSAValidation ( sPSContextData *inContext, const char *inUserKey )
{
	int					siResult			= kAuthNoError;
	PWServerError		serverResult;
    BIGNUM				*nonce;
	BN_CTX				*ctx;
	char				*bnStr				= NULL;
	int					len;
    int					nonceLen			= 0;
    char				buf[1024];
    char				encodedStr[1024];
    
    if ( inContext == NULL )
        return kAuthOtherError;
    
    // make sure we are talking to the right server
    if ( strcmp(inContext->rsaPublicKeyStr, inUserKey) != 0 )
        return kAuthKeyError;
    
    // make nonce
    nonce = BN_new();
    if ( nonce == NULL )
        return kAuthOtherError;
    
    // Generate a random challenge
    BN_rand(nonce, 256, 0, 0);
    ctx = BN_CTX_new();
    BN_mod(nonce, nonce, inContext->rsaPublicKey->rsa->n, ctx);
    BN_CTX_free(ctx);
    
    bnStr = BN_bn2dec(nonce);
	BN_clear_free(nonce);
	
#ifdef DEBUG_PRINTFS
    printf("nonce = %s\n", bnStr);
#endif    
    nonceLen = strlen(bnStr);
    len = RSA_public_encrypt(nonceLen,
                                (unsigned char *)bnStr,
                                (unsigned char *)encodedStr,
                                inContext->rsaPublicKey->rsa,
                                RSA_PKCS1_PADDING);
    
    if (len <= 0) {
#ifdef DEBUG_PRINTFS
        printf("rsa_public_encrypt() failed");
#endif
		if ( bnStr != NULL )
			free( bnStr );
        return kAuthKeyError;
    }
    
    if ( ConvertBinaryTo64( encodedStr, (unsigned)len, buf ) == SASL_OK )
    {
        char writeBuf[1024];
        unsigned long encodedStrLen;
        
        snprintf( writeBuf, sizeof(writeBuf), "RSAVALIDATE %s\r\n", buf );
        writeToServer( inContext->serverOut, writeBuf );
        
        serverResult = readFromServer( inContext->fd, buf, sizeof(buf) );
        
        if ( Convert64ToBinary( buf + 4, encodedStr, sizeof(encodedStr), &encodedStrLen ) == SASL_OK )
        {
            encodedStr[nonceLen] = '\0';
#ifdef DEBUG_PRINTFS
            printf("nonce = %s\n", encodedStr);
#endif            
            if (memcmp(bnStr, encodedStr, nonceLen) != 0)
                siResult = kAuthKeyError;
        }
    }
    
	if ( bnStr != NULL )
		free( bnStr );
	
    return siResult;
}

int OLCloseConnection ( sPSContextData *pContext )
{
	int siResult = kAuthNoError;
    
	EndServerSession( pContext, true );
	
	return( siResult );

} // OLCloseConnection


// ---------------------------------------------------------------------------
//	* CleanContextData
// ---------------------------------------------------------------------------

int CleanContextData ( sPSContextData *inContext )
{
    int	siResult = kAuthNoError;
	   
    if ( inContext == NULL )
    {
        siResult = kAuthOtherError;
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
            sasl_dispose(&inContext->conn);
            inContext->conn = NULL;
        }
                
        if (inContext->serverOut != NULL)
        {
            fclose(inContext->serverOut);
            inContext->serverOut = NULL;
        }
        
        if (inContext->fd > 0)
        {
            close(inContext->fd);
        }
		inContext->fd = -1;
        
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
		
        if ( inContext->serverList != NULL )
        {
            CFRelease( inContext->serverList );
            inContext->serverList = NULL;
        }
        
        if ( inContext->syncFilePath != NULL )
        {
            remove( inContext->syncFilePath );
            free( inContext->syncFilePath );
            inContext->syncFilePath = NULL;
        }
        
        bzero( &inContext->rc5Key, sizeof(RC5_32_KEY) );
        inContext->madeFirstContact = false;
	}
	
    return( siResult );

} // CleanContextData

int ParseAuthorityData(char* inAuthAuthorityData, char** vers, 
                        char** type, char** id, char** addr, char** key, bool *addrIsID)
{
    char* temp;
    
	*addrIsID = false;
	
    *vers = inAuthAuthorityData;
    temp = strchr(*vers, ';');
    if (temp == NULL) return 0;
    *temp = '\0';
    *type = temp+1;
    temp = strchr(*type, ';');
    if (temp == NULL) return 0;
    *temp = '\0';
    *id = temp+1;
    temp = strchr(*id, ',');
	if (temp == NULL) return 0;
	*temp = '\0';
	*key = temp+1;
	temp = strchr(*key, ':');
	if (temp == NULL) return 0;
    *temp = '\0';
    *addr = temp+1;
	if ( strncmp( *addr, "ipv4/", 5 ) == 0 )
		*addr += 5;
	else
	if ( strncmp( *addr, "ipv6/", 5 ) == 0 )
		*addr += 5;
	else
	if ( strncmp( *addr, "dns/", 4 ) == 0 )
		*addr += 4;
	else
	if ( strncmp( *addr, "id/", 3 ) == 0 ) {
		*addr += 3;
		*addrIsID = true;
	}
	
    return 1;
}

int CheckAuthType(char* inAuthAuthorityData, char* authType)
{
    char* temp;
    temp = strchr(inAuthAuthorityData, ';');
    return ((temp != NULL) && (strncmp(temp+1, authType, strlen(authType)) == 0));
}

int DoPSAuth(char* userName, char* password, char* inAuthAuthorityData)
{
    char* infoVersion = NULL;
    char* authType = NULL;
    char* userID = NULL;
    char* serverAddr = NULL;
    char* rsaKey = NULL;
    char* authDataCopy = NULL;
	int result;
	bool addrIsID = false;
	int rc;
	int error_num;
	sPSContextData context;
    sSASLContext saslContext;
	sPSServerEntry anEntry;
	struct in_addr inetAddr;
	struct hostent *hostEnt;
	sasl_callback_t callbacks[5];
	
	bzero( &context, sizeof(sPSContextData) );
	bzero( &saslContext, sizeof(saslContext) );
	CleanContextData ( &context );
	
	do { // not a loop
    // copy to our own buffer
    authDataCopy = (char *)malloc(strlen(inAuthAuthorityData) + 1);
    if ( authDataCopy == NULL ) {
        result = kAuthOtherError;
		break;
	}
    
    strcpy(authDataCopy, inAuthAuthorityData);
    if (!ParseAuthorityData(authDataCopy, &infoVersion, &authType, 
                            &userID, &serverAddr, &rsaKey, &addrIsID)) {
		result = kAuthOtherError;
		break;
	}

    // check auth info type
    if (strcmp(authType, PASSWORD_SERVER_AUTH_TYPE) != 0) {
		result = kAuthOtherError;
		break;
	}
	
    if ( addrIsID && strlen(serverAddr) >= sizeof(anEntry.id) ) {
		result = kAuthOtherError;
		break;
	} else if ( strlen(serverAddr) >= sizeof(anEntry.ip) ) {
		result = kAuthOtherError;
		break;
	}
	
	bzero( &anEntry, sizeof(anEntry) );
		
	if ( addrIsID )
	{
		strcpy( anEntry.id, serverAddr );
	}
	else
	{
		// is it an IP address?
		rc = inet_aton( serverAddr, &inetAddr );
		if ( rc == 1 )
		{
			strcpy( anEntry.ip, serverAddr );
		}
		else
		{
			strlcpy( anEntry.dns, serverAddr, sizeof(anEntry.dns) );
			
			// resolve if possible
			
			hostEnt = getipnodebyname( anEntry.dns, AF_INET, AI_DEFAULT, &error_num );
			if ( hostEnt != NULL )
			{
				if ( hostEnt->h_addr_list[0] != NULL ) {
					if ( inet_ntop(AF_INET, hostEnt->h_addr_list[0], anEntry.ip, sizeof(anEntry.ip)) == NULL )
						anEntry.ip[0] = 0;
				}
				freehostent( hostEnt );
			}
		}
		
		anEntry.ipFromNode = true;
	}
	context.serverProvidedFromNode = anEntry;
	
    result = InitConnection(&context, &saslContext, serverAddr, rsaKey, callbacks);
    if (result != kAuthNoError)
		break;
        
    if (rsaKey != NULL)
    {
        result = DoRSAValidation(&context, rsaKey);
    }
    
    if (result == kAuthNoError)
    	result = DoSASLAuth(&context, &saslContext, userID, password, "CRAM-MD5");
	} while (0);
	
    if (saslContext.secret != NULL) {
        bzero(saslContext.secret->data,saslContext.secret->len);
        free(saslContext.secret);
        saslContext.secret = NULL;
    }
    
    OLCloseConnection(&context);
    CleanContextData(&context);
	if (authDataCopy != NULL)
		free(authDataCopy);
	
    return result;
}

