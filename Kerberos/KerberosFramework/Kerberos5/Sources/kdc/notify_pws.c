

/* code to notify the Apple Password Server of Kerberos logins	*/


#ifndef APPLE_KDC_MODS
#define APPLE_KDC_MODS
#endif
#ifdef APPLE_KDC_MODS
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* there may be value in copying the source for  readFromServer & writeToServer
into this file and removing the link dependency on the password server framework.

*/

//#include <PasswordServer/CPSUtilities.h>

typedef enum PWServerErrorType {
	kPolicyError,
	kSASLError,
	kConnectionError
} PWServerErrorType;

typedef struct PWServerError {
    int err;
    PWServerErrorType type;
} PWServerError;

void 			writeToServer( FILE *out, char *buf );
PWServerError 	readFromServer( int fd, char *buf, unsigned long bufLen );
PWServerError 	readFromServerGetData( int fd, char *buf, unsigned long bufLen, unsigned long *outByteCount );
PWServerError 	readFromServerGetLine( int fd, char *buf, unsigned long bufLen, int inCanReadMore, unsigned long *inOutByteCount );
PWServerError 	readFromServerGetErrorCode( char *buf );

extern int errno;

#define noErr   0

int kdc_contact_pws(void);
int kdc_update_pws( const char *inPrinciple, int inError, int inCheck);

#define kUpdateAuthMethod   "KERBEROS-LOGIN-CHECK"
#define kPWSPort			106

// password server error strings
#define kPasswordServerErrPrefixStr			"-ERR "
#define kPasswordServerAuthErrPrefixStr		"-AUTHERR "
#define kPasswordServerSASLErrPrefixStr		"SASL "

// Reposonse Codes (used numerically)
enum {
    kAuthOK = 0,
    kAuthFail = -1,
    kAuthUserDisabled = -2,
    kAuthNeedAdminPrivs = -3,
    kAuthUserNotSet = -4,
    kAuthUserNotAuthenticated = -5,
    kAuthPasswordExpired = -6,
    kAuthPasswordNeedsChange = -7,
    kAuthPasswordNotChangeable = -8,
    kAuthPasswordTooShort = -9,
    kAuthPasswordTooLong = -10,
    kAuthPasswordNeedsAlpha = -11,
    kAuthPasswordNeedsDecimal = -12,
    kAuthMethodTooWeak = -13,
	kAuthPasswordNeedsMixedCase = -14,
	kAuthPasswordHasGuessablePattern = -15
};

#define	KDC_ERR_POLICY	12

int kdc_update_pws( const char *inPrinciple, int inError, int inCheck)
{
	char commandBuf[4096];
	char replyBuf[4096];
	int pwsFD = kdc_contact_pws();
	FILE *serverOut = NULL;
	int reply = -1;
	PWServerError   pwsReply;
	
	if(pwsFD == -1)
	{
		return errno;
	}
	
	serverOut = fdopen(pwsFD, "w");
	
	// discard the greeting message
	readFromServer(pwsFD, replyBuf, sizeof(replyBuf));
	
	// build the initial request 
	if(inCheck == 1)
	{
		snprintf(commandBuf, sizeof(commandBuf), "AUTH %s %s ?\r\n", kUpdateAuthMethod, inPrinciple);
	} else {
		snprintf(commandBuf, sizeof(commandBuf), "AUTH %s %s %c\r\n", kUpdateAuthMethod, inPrinciple, (inError == noErr) ? '+' : '-');
	}
	// send it to the server
	 writeToServer(serverOut, commandBuf);

	// get the reply
	pwsReply = readFromServer(pwsFD, replyBuf, sizeof(replyBuf));


	if(pwsReply.err != noErr)
	{
		if(pwsReply.err == kAuthUserDisabled)
			reply = KDC_ERR_POLICY;
		else if(pwsReply.err == kAuthUserNotSet) // not found in the db (not really an error)
			reply = 0;
		else if(pwsReply.err == kAuthPasswordNeedsChange) // password needs change (not really an error)
			reply = 0;
		else
			reply = KDC_ERR_POLICY;
	} else {
		reply = 0;
	}

	snprintf(commandBuf, sizeof(commandBuf), "QUIT\r\n");

	// send it to the server
	writeToServer(serverOut, commandBuf);

	// get the reply
	pwsReply = readFromServer(pwsFD, replyBuf, sizeof(replyBuf));
	
	// clean up
	fclose(serverOut);  // this closes pwsFD as well
	
	return reply;
}

/* returns the socket of the local pws or -1 & sets errno   */
int kdc_contact_pws(void)
{
	int pws_socket;
	struct sockaddr_un  addr;
	struct timeval sendTimeoutVal = { 3 , 0 };  // three seconds
	int val = 1;
	
	addr.sun_family = AF_UNIX;
	addr.sun_len = sizeof(struct sockaddr_un);
	strcpy(addr.sun_path, "/var/run/passwordserver");

	pws_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	
	// set a timeout on the socket
	// SO_SNDTIMEO - send timeout

    if(setsockopt(pws_socket, SOL_SOCKET, SO_SNDTIMEO, &sendTimeoutVal, sizeof(sendTimeoutVal)) == -1)
	{
		return -1;	 
	}

	if(connect(pws_socket, (const struct sockaddr *)&addr, sizeof(addr)) == -1)
		return -1;
	else
		return pws_socket;
}

void writeToServer( FILE *out, char *buf )
{
    if ( buf != NULL )
    {
        fwrite(buf, strlen(buf), 1, out);
        fflush(out);
    }
}

PWServerError readFromServer( int fd, char *buf, unsigned long bufLen )
{
    PWServerError result;
	unsigned long byteCount;
	
	result = readFromServerGetData( fd, buf, bufLen, &byteCount );
	if ( result.err == 0 )
		result = readFromServerGetLine( fd, buf, bufLen, 1, &byteCount );
	if ( result.err == 0 )
		result = readFromServerGetErrorCode( buf );
	    
    return result;
}

PWServerError readFromServerGetData( int fd, char *buf, unsigned long bufLen, unsigned long *outByteCount )
{
    char readChar = '\0';
    PWServerError result = {0, kPolicyError};
	ssize_t byteCount = 0;
	
	if ( buf == NULL || bufLen < 3 ) {
        result.err = -1;
        return result;
    }
    
	if ( outByteCount != NULL )
		*outByteCount = 0;
	
	buf[0] = '\0';
	
	// wait for the first character to arrive
	byteCount = recvfrom( fd, &readChar, sizeof(readChar), (MSG_WAITALL | MSG_PEEK), NULL, NULL );
	if ( byteCount == 0 || byteCount == -1 )
	{
		result.err = -1;
		result.type = kConnectionError;
		return result;
	}
	
	// peek the buffer to get the length
	byteCount = recvfrom( fd, buf, bufLen - 1, (MSG_DONTWAIT | MSG_PEEK), NULL, NULL );
	//DEBUGLOG( "byteCount (peek): %d", (int)byteCount);
	
	if ( outByteCount != NULL )
		*outByteCount = byteCount;
	
    return result;
}


PWServerError readFromServerGetLine( int fd, char *buf, unsigned long bufLen, int inCanReadMore, unsigned long *inOutByteCount )
{
    char readChar = '\0';
    char *tstr = NULL;
	char *consumeBuf;
    PWServerError result = {0, kPolicyError};
	size_t byteCount = *inOutByteCount;
	size_t consumeLen;
	
	if ( buf == NULL || bufLen < 3 ) {
        result.err = -1;
        return result;
    }
    
	// pull to EOL or available data
	if ( byteCount >= 2 )
	{
		if ( inCanReadMore )
		{
			buf[byteCount] = '\0';
			tstr = strstr( buf, "\r\n" );
		}
		consumeLen = (tstr != NULL) ? (tstr - buf + 2) : byteCount;
		consumeBuf = (char *) malloc( consumeLen );
		if ( consumeBuf == NULL ) {
			result.err = -1;
			return result;
		}
		byteCount = recvfrom( fd, consumeBuf, consumeLen, MSG_DONTWAIT, NULL, NULL );
		free( consumeBuf );
		if ( inOutByteCount != NULL )
			*inOutByteCount = byteCount;
		buf[byteCount] = '\0';
	}
	
	// if not at EOL, pull by character until one arrives 
	if ( inCanReadMore && tstr == NULL && byteCount < (ssize_t)bufLen - 1 )
	{
		tstr = buf + byteCount;
		do
		{
			byteCount = recvfrom( fd, &readChar, sizeof(readChar), MSG_WAITALL, NULL, NULL );
			if ( byteCount == 0 || byteCount == -1 )
			{
				*tstr = '\0';
				result.err = -1;
				result.type = kConnectionError;
				return result;
			}
			
			if ( (unsigned long)(tstr - buf) < bufLen - 1 )
				*tstr++ = readChar;
			
			if ( inOutByteCount != NULL )
				(*inOutByteCount)++;
		}
		while ( readChar != '\n' );
		*tstr = '\0';
	}
		
    return result;
}


PWServerError readFromServerGetErrorCode( char *buf )
{
    char *tstr = NULL;
    PWServerError result = {0, kPolicyError};
	int compareLen;

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
		if ( result.err == 0 )
			result.err = -1;
    }
    else
	{
		compareLen = strlen(kPasswordServerAuthErrPrefixStr);
		if ( strncmp( tstr, kPasswordServerAuthErrPrefixStr, compareLen ) == 0 )
		{
			tstr += compareLen;
			sscanf( tstr, "%d", &result.err );
			if ( result.err == 0 )
				result.err = -1;
		}
	}
	
    return result;
}

#endif /* APPLE_KDC_MODS */

