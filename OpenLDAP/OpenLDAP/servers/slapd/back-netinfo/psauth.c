#include "psauth.h"
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

#include <openssl/bn.h>
#include <openssl/blowfish.h>

#include "sasl.h"
#include "saslutil.h"

#include "key.h"

#define kBigBuffSize						4096

#define kSASLListPrefix						"(SASL "
#define kPasswordServerErrPrefixStr			"-ERR "
#define kPasswordServerSASLErrPrefixStr		"SASL "

typedef enum PWServerErrorType {
    kPolicyError,
    kSASLError
} PWServerErrorType;

typedef struct PWServerError {
    int err;
    PWServerErrorType type;
} PWServerError;


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

typedef struct AuthMethName {
    char method[SASL_MECHNAMEMAX + 1];
} AuthMethName;

// Context data structure
typedef struct sPSContextData {
//	char	   *psName;					//domain or ip address of passwordserver
	int		offset;					//offset for GetDirNodeInfo data extraction
										// TODO ... more as needed
    sasl_conn_t *conn;
    FILE *serverIn, *serverOut;
    int fd;
    AuthMethName *mech;
    int mechCount;
    
	char *rsaPublicKeyStr;
    Key *rsaPublicKey;
	
    char username[35];
    const char *password;
} sPSContextData;

int DoRSAValidation ( sPSContextData *inContext, const char *inUserKey );

void writeToServer( FILE *out, char *buf );
void writeToServer( FILE *out, char *buf )
{
    fwrite(buf, strlen(buf), 1, out);
    fflush(out);
    
}


PWServerError readFromServer( FILE *in, char *buf, unsigned long bufLen );
PWServerError readFromServer( FILE *in, char *buf, unsigned long bufLen )
{
    char readChar;
    char *tstr = buf;
    PWServerError result = {0, kPolicyError};
    int compareLen;
    
    if ( buf == NULL || bufLen < 2 ) {
        result.err = -1;
        return result;
    }
    
    *buf = '\0';
    do
    {
        fscanf( in, "%c", &readChar );
#ifdef DEBUG_PRINTFS
        if ( isprint((unsigned char) readChar) )
            printf( "%c ", readChar );
        else
            printf( "%x ", readChar );
#endif
        if ( (unsigned long)(tstr - buf) < bufLen - 1 )
            *tstr++ = readChar;
    }
    while ( readChar && readChar != '\n' );
    
    *tstr = '\0';
#ifdef DEBUG_PRINTFS
    printf( "\n" );
#endif
    
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
        return kAuthOtherError;
    
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
    
    // get the original length
    if ( *readPtr == '{' )
    {
        sscanf( readPtr + 1, "%lu", &attached_outlen );
        
        readPtr = strchr( readPtr, '}' );
        if ( readPtr == NULL )
            return kAuthOtherError;
        
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

	if (availrealms != NULL)
		*result = *availrealms;
		  
    return SASL_OK;
}

int simple(void *context /*__attribute__((unused))*/,
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
            *result = ((sPSContextData *)context)->username;
            if (len != NULL)
                *len = strlen(((sPSContextData *)context)->username);
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
getsecret(sasl_conn_t *conn,
	  void *context /*__attribute__((unused))*/,
	  int id,
	  sasl_secret_t **psecret)
{
    char *password = NULL;
    size_t len = 0;
	sasl_secret_t *xsec = NULL;
    
#ifdef DEBUG_PRINTFS
    printf("in getsecret\n");
#endif
    /* paranoia check */
    if (! conn || ! psecret || id != SASL_CB_PASS)
        return SASL_BADPARAM;
    
    *psecret = NULL;
    
    /*
    password = getpassphrase("Password: ");
    if (! password)
	return SASL_FAIL;
    */
    if (((sPSContextData *)context)->password)
    {
        len = strlen(((sPSContextData *)context)->password);
        xsec = (sasl_secret_t *) malloc(sizeof(sasl_secret_t) + len + 1);
    	if ( xsec == NULL )
            return SASL_NOMEM;
        
        xsec->len = len;
        strcpy((char *)xsec->data, ((sPSContextData *)context)->password);
    }
    
    *psecret = xsec;
    return SASL_OK;
}


//---------

int getconn(const char *host, const char *port)
{
    char servername[1024];
	char *endPtr = NULL;
    struct sockaddr_in sin;
	int rc;
	struct in_addr inetAddr;
    struct hostent *hp;
    int sock;

    strncpy(servername, host, sizeof(servername) - 1);
    servername[sizeof(servername) - 1] = '\0';
    
    /* map hostname -> IP */
	rc = inet_aton(servername, &inetAddr);
	if ( rc == 1 ) {
		sin.sin_addr.s_addr = inetAddr.s_addr;
	} else {
		if ((hp = gethostbyname(servername)) == NULL) {
#ifdef DEBUG_PRINTFS
			perror("gethostbyname");
#endif
			return -1;
		}
		memcpy(&sin.sin_addr, hp->h_addr, hp->h_length);
	}
    
    /* map port -> num */
    sin.sin_port = htons(strtol(port, &endPtr, 10));
	if ((sin.sin_port == 0) || (endPtr == port)) {
#ifdef DEBUG_PRINTFS
		printf("port '%s' unknown\n", port);
#endif
		return -1;
    }

    sin.sin_family = AF_INET;

    /* connect */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
#ifdef DEBUG_PRINTFS
        perror("socket");
#endif
        return -1;
    }
    
    if (connect(sock, (struct sockaddr *) &sin, sizeof (sin)) < 0) {
#ifdef DEBUG_PRINTFS
        perror("connect");
#endif
        return -1;
    }

    return sock;
}

// ---------------------------------------------------------------------------
//	* GetRSAPublicKey
// ---------------------------------------------------------------------------

int GetRSAPublicKey( sPSContextData *inContext )
{
    PWServerError		serverResult;
    char				buf[1024];
    char				*keyStr;
    int					bits				= 0;
    
    if ( inContext == NULL )
        return kAuthOtherError;
    
    // get string
    writeToServer( inContext->serverOut, "RSAPUBLIC\r\n" );
    serverResult = readFromServer( inContext->serverIn, buf, 1024 );
    if (serverResult.err != 0)
    {
#ifdef DEBUG_PRINTFS
        printf("no public key\n");
#endif
        return kAuthKeyError;
    }
    
    chop(buf);
    inContext->rsaPublicKeyStr = (char *) calloc(1, strlen(buf)+1);
    if ( inContext->rsaPublicKeyStr == NULL )
        return kAuthKeyError;
    
    strcpy(inContext->rsaPublicKeyStr, buf + 4);
    
    // get as struct
    inContext->rsaPublicKey = PS_key_new(KEY_RSA);
    if ( inContext->rsaPublicKey == NULL )
        return kAuthKeyError;
    
    keyStr = buf + 4;
    bits = PS_key_read(inContext->rsaPublicKey, &keyStr);
    if (bits == 0) {
#ifdef DEBUG_PRINTFS
        printf("no bits\n");
#endif
        return kAuthKeyError;
    }
    
    return kAuthNoError;
}


int InitConnection(sPSContextData* pContext, char* psName, char* rsakey)
{
	int result;
	unsigned count;
	char *tptr, *end;
	sasl_security_properties_t secprops = {0,65535,4096,0,NULL,NULL};
	int salen;
	
	pContext->fd = -1;
    
    // 2.x
	char localaddr[NI_MAXHOST + NI_MAXSERV],
	remoteaddr[NI_MAXHOST + NI_MAXSERV];
	char hbuf[NI_MAXHOST], pbuf[NI_MAXSERV];
	struct sockaddr_storage local_ip, remote_ip;
	char buf[kBigBuffSize];
	static sasl_callback_t callbacks[5];
	
	// callbacks we support
	callbacks[0].id = SASL_CB_GETREALM;
	callbacks[0].proc = (sasl_cbproc *)&getrealm;
	callbacks[0].context = pContext;
	
	callbacks[1].id = SASL_CB_USER;
	callbacks[1].proc = (sasl_cbproc *)&simple;
	callbacks[1].context = pContext;
	
	callbacks[2].id = SASL_CB_AUTHNAME;
	callbacks[2].proc = (sasl_cbproc *)&simple;
	callbacks[2].context = pContext;
	
	callbacks[3].id = SASL_CB_PASS;
	callbacks[3].proc = (sasl_cbproc *)&getsecret;
	callbacks[3].context = pContext;
	
	callbacks[4].id = SASL_CB_LIST_END;
	callbacks[4].proc = NULL;
	callbacks[4].context = NULL;
	
	result = sasl_client_init(callbacks);
	if ( result != SASL_OK ) {
#ifdef DEBUG_PRINTFS
		printf("sasl_client_init failed.\n");
#endif
		return kAuthOtherError;
	}
	
	// connect to remote server
	pContext->fd = getconn(psName, "106");
    if (pContext->fd == -1)
        return kAuthOtherError;
	
	// set ip addresses
	salen = sizeof(local_ip);
	if (getsockname(pContext->fd, (struct sockaddr *)&local_ip, &salen) < 0) {
		perror("getsockname");
	}
	
	getnameinfo((struct sockaddr *)&local_ip, salen,
				hbuf, sizeof(hbuf), pbuf, sizeof(pbuf),
				NI_NUMERICHOST | NI_WITHSCOPEID | NI_NUMERICSERV);
	snprintf(localaddr, sizeof(localaddr), "%s;%s", hbuf, pbuf);
	
	salen = sizeof(remote_ip);
	if (getpeername(pContext->fd, (struct sockaddr *)&remote_ip, &salen) < 0) {
#ifdef DEBUG_PRINTFS
		perror("getpeername");
#endif
	}
	
	getnameinfo((struct sockaddr *)&remote_ip, salen,
				hbuf, sizeof(hbuf), pbuf, sizeof(pbuf),
				NI_NUMERICHOST | NI_WITHSCOPEID | NI_NUMERICSERV);
	snprintf(remoteaddr, sizeof(remoteaddr), "%s;%s", hbuf, pbuf);
	
	result = sasl_client_new("rcmd", psName, localaddr, remoteaddr, NULL, 0, &pContext->conn);
	if ( result != SASL_OK || pContext->conn == NULL ) {
#ifdef DEBUG_PRINTFS
		printf("sasl_client_new failed.\n");
#endif
		return kAuthOtherError;
	}
	
	result = sasl_setprop(pContext->conn, SASL_SEC_PROPS, &secprops);
	
	pContext->serverIn = fdopen(pContext->fd, "r");
	pContext->serverOut = fdopen(pContext->fd, "w");
	
	// yank hi there text
	readFromServer(pContext->serverIn, buf, sizeof(buf));
	
    // retrieve the password server's public RSA key
    result = GetRSAPublicKey(pContext);
    if ( result != kAuthNoError ) {
        return result;
    }
	
	writeToServer(pContext->serverOut, "LIST\r\n");
	readFromServer(pContext->serverIn, buf, sizeof(buf));
	chop(buf);
	tptr = buf;
	for (count=0; tptr; count++ ) {
		tptr = strchr( tptr, ' ' );
		if (tptr) tptr++;
	}
	
	if (count > 0) {
		pContext->mech = (AuthMethName *)calloc(count, sizeof(AuthMethName));
		if ( pContext->mech == NULL )
			return( kAuthOtherError );
		
		pContext->mechCount = count;
	}
	
	tptr = strstr(buf, kSASLListPrefix);
	if ( tptr )
	{
		tptr += strlen(kSASLListPrefix);
		
		for ( ; tptr && count > 0; count-- )
		{
			if ( *tptr == '\"' )
				tptr++;
			else
				break;
			
			end = strchr(tptr, '\"');
			if (end)
				*end = '\0';
				
			strcpy(pContext->mech[count-1].method, tptr);
#ifdef DEBUG_PRINTFS
            printf("mech=%s\n", tptr);
#endif			
			tptr = end;
			if ( tptr )
				tptr+=2;
		}
	}
    
    return kAuthNoError;
}

int DoSASLAuth(
    sPSContextData *inContext,
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
        strncpy(inContext->username, userName, 35);
        inContext->username[34] = '\0';
        
        inContext->password = password;
        
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
        readFromServer( inContext->serverIn, buf, sizeof(buf) );
        
        // send the auth method
        dataBuf[0] = 0;
        if ( len > 0 )
            ConvertBinaryToHex( (const unsigned char *)data, len, dataBuf );
        
#ifdef DEBUG_PRINTFS
        printf("AUTH %s %s\r\n", chosenmech, dataBuf);
#endif
        if ( len > 0 )
            snprintf(buf, sizeof(buf), "AUTH %s %s\r\n", chosenmech, dataBuf);
        else
            snprintf(buf, sizeof(buf), "AUTH %s\r\n", chosenmech);
        writeToServer(inContext->serverOut, buf);
        
        // get server response
        serverResult = readFromServer(inContext->serverIn, buf, sizeof(buf));
        if (serverResult.err != 0) {
#ifdef DEBUG_PRINTFS
            printf("server returned an error, err=%d\n", serverResult);
#endif
            return kAuthenticationError;
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
                ConvertBinaryToHex( (const unsigned char *)data, len, dataBuf );
                
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
            
            serverResult = readFromServer(inContext->serverIn, buf, sizeof(buf) );
            if ( serverResult.err != 0 ) {
#ifdef DEBUG_PRINTFS
                printf("server returned an error, err=%d\n", serverResult);
#endif
                return kAuthenticationError;
            }
            
            chop(buf);
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
    char				buf[1024];
    BIGNUM				*nonce;
	BN_CTX				*ctx;
	char				*bnStr;
	int					len;
    
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
#ifdef DEBUG_PRINTFS
    printf("nonce = %s\n", bnStr);
#endif    
    int nonceLen = strlen(bnStr);
    char *encodedStr = (char *)malloc(1024);
    
    if ( encodedStr == NULL )
        return kAuthOtherError;
    
    len = RSA_public_encrypt(nonceLen,
                                (unsigned char *)bnStr,
                                (unsigned char *)encodedStr,
                                inContext->rsaPublicKey->rsa,
                                RSA_PKCS1_PADDING);
    
    if (len <= 0) {
#ifdef DEBUG_PRINTFS
        printf("rsa_public_encrypt() failed");
#endif
        return kAuthKeyError;
    }
    
    if ( ConvertBinaryTo64( encodedStr, (unsigned)len, buf ) == SASL_OK )
    {
        char writeBuf[1024];
        unsigned long encodedStrLen;
        
        snprintf( writeBuf, sizeof(writeBuf), "RSAVALIDATE %s\r\n", buf );
        writeToServer( inContext->serverOut, writeBuf );
        
        serverResult = readFromServer( inContext->serverIn, buf, sizeof(buf) );
        
        if ( Convert64ToBinary( buf + 4, encodedStr, &encodedStrLen ) == SASL_OK )
        {
            encodedStr[nonceLen] = '\0';
#ifdef DEBUG_PRINTFS
            printf("nonce = %s\n", encodedStr);
#endif            
            if (memcmp(bnStr, encodedStr, nonceLen) != 0)
                siResult = kAuthKeyError;
        }
    }
    
    return siResult;
}

int CloseConnection ( sPSContextData *pContext )
{
	int				siResult	= kAuthNoError;
    char			buf[1024];
    
    //TODO do whatever to close out the context
    writeToServer(pContext->serverOut, "QUIT\r\n");

    // crashes
    readFromServer( pContext->serverIn, buf, sizeof(buf) );
    
	return( siResult );

} // CloseConnection

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
//        if (inContext->psName != NULL)
//        {
//            free( inContext->psName );
//            inContext->psName = NULL;
//        }
		
        inContext->offset = 0;
        
        if (inContext->conn != NULL)
        {
            sasl_dispose(&inContext->conn);
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
        
        if (inContext->fd != -1)
        {
            close(inContext->fd);
            inContext->fd = -1;
        }
        
		if (inContext->rsaPublicKeyStr != NULL)
		{
			free(inContext->rsaPublicKeyStr);
			inContext->rsaPublicKeyStr = NULL;
		}
		
		if (inContext->rsaPublicKey != NULL)
		{
			PS_key_free(inContext->rsaPublicKey);
			inContext->rsaPublicKey = NULL;
		}
		
        if (inContext->mech != NULL)
        {
            free(inContext->mech);
            inContext->mech = NULL;
        }
        
        inContext->mechCount = 0;
        
        memset(inContext->username, 0, sizeof(inContext->username));
        
        if (inContext->password != NULL)
            inContext->password = NULL;
	}

    return( siResult );

} // CleanContextData

int ParseAuthorityData(char* inAuthAuthorityData, char** vers, 
                        char** type, char** id, char** addr, char** key)
{
    char* temp;
    
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
    sPSContextData context;
    char* infoVersion = NULL;
    char* authType = NULL;
    char* userID = NULL;
    char* serverAddr = NULL;
    char* rsaKey = NULL;
    char* authDataCopy;
    
    // copy to our own buffer
    authDataCopy = malloc(strlen(inAuthAuthorityData) + 1);
    if ( authDataCopy == NULL )
        return kAuthOtherError;
    
    strcpy(authDataCopy, inAuthAuthorityData);
    if (!ParseAuthorityData(authDataCopy, &infoVersion, &authType, 
                            &userID, &serverAddr, &rsaKey))
        return kAuthOtherError;

    // check version???
    // if (strcmp(infoVersion, "1")) return kAuthOtherError;

    // check auth info type
    if (strcmp(authType, PASSWORD_SERVER_AUTH_TYPE) != 0) return kAuthOtherError;
    
    int result = InitConnection(&context, serverAddr, rsaKey);
    if (result != kAuthNoError)
        return result;
        
    if (rsaKey != NULL)
    {
        result = DoRSAValidation(&context, rsaKey);
    }
    
    if (result == kAuthNoError)
    	result = DoSASLAuth(&context, userID, password, "DIGEST-MD5");
    CloseConnection(&context);
    CleanContextData(&context);
    free(authDataCopy);
    return result;
}
