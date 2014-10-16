/*
 * io_sock.c - SecureTransport sample I/O module, X sockets version
 */

#include "ioSockThr.h"
#include <errno.h>
#include <stdio.h>

#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <time.h>
#include <strings.h>

/* debugging for this module */
#define SSL_OT_DEBUG		1

/* log errors to stdout */
#define SSL_OT_ERRLOG		1

/* trace all low-level network I/O */
#define SSL_OT_IO_TRACE		0

/* if SSL_OT_IO_TRACE, only log non-zero length transfers */
#define SSL_OT_IO_TRACE_NZ	1

/* pause after each I/O (only meaningful if SSL_OT_IO_TRACE == 1) */
#define SSL_OT_IO_PAUSE		0

/* print a stream of dots while I/O pending */
#define SSL_OT_DOT			1

/* dump some bytes of each I/O (only meaningful if SSL_OT_IO_TRACE == 1) */
#define SSL_OT_IO_DUMP		0
#define SSL_OT_IO_DUMP_SIZE	64

/* general, not-too-verbose debugging */
#if		SSL_OT_DEBUG
#define dprintf(s)	printf s
#else	
#define dprintf(s)
#endif

/* errors --> stdout */
#if		SSL_OT_ERRLOG
#define eprintf(s)	printf s
#else	
#define eprintf(s)
#endif

/* enable nonblocking I/O - maybe should be an arg to MakeServerConnection() */
#define NON_BLOCKING	0

/* trace completion of every r/w */
#if		SSL_OT_IO_TRACE
static void tprintf(
	const char *str, 
	UInt32 req, 
	UInt32 act,
	const UInt8 *buf)	
{
	#if	SSL_OT_IO_TRACE_NZ
	if(act == 0) {
		return;
	}
	#endif
	printf("%s(%d): moved %d bytes\n", str, req, act);
	#if	SSL_OT_IO_DUMP
	{
		int i;
		
		for(i=0; i<act; i++) {
			printf("%02X ", buf[i]);
			if(i >= (SSL_OT_IO_DUMP_SIZE - 1)) {
				break;
			}
		}
		printf("\n");
	}
	#endif
	#if SSL_OT_IO_PAUSE
	{
		char instr[20];
		printf("CR to continue: ");
		gets(instr);
	}
	#endif
}

#else	
#define tprintf(str, req, act, buf)
#endif	/* SSL_OT_IO_TRACE */

/*
 * If SSL_OT_DOT, output a '.' every so often while waiting for
 * connection. This gives user a chance to do something else with the
 * UI.
 */

#if	SSL_OT_DOT

static time_t lastTime = (time_t)0;
#define TIME_INTERVAL		3

static void outputDot()
{
	time_t thisTime = time(0);
	
	if((thisTime - lastTime) >= TIME_INTERVAL) {
		printf("."); fflush(stdout);
		lastTime = thisTime;
	}
}
#else
#define outputDot()
#endif


/*
 * One-time only init.
 */
void initSslOt()
{

}

/*
 * Connect to server. 
 *
 * Seeing a lot of soft errors...for threadTest (only) let's retry.
 */
#define CONNECT_RETRIES	10

OSStatus MakeServerConnection(
	const char *hostName, 
	int port, 
	otSocket *socketNo, 	// RETURNED
	PeerSpec *peer)			// RETURNED
{
    struct sockaddr_in  addr;
    struct hostent      *ent;
    struct in_addr      host;
	int					sock = 0;
	int					i;
	
	*socketNo = NULL;
    if (hostName[0] >= '0' && hostName[0] <= '9') {
        host.s_addr = inet_addr(hostName);
    }
    else {
		for(i=0; i<CONNECT_RETRIES; i++) {
			/* seeing a lot of spurious "No address associated with name"
			 * failures on known good names (www.amazon.com) */
			ent = gethostbyname(hostName);
			if (ent == NULL) {
				printf("gethostbyname failed\n");
				herror("hostName");

			}
			else {
				memcpy(&host, ent->h_addr, sizeof(struct in_addr));
				break;
			}
		}
		if(ent == NULL) {
			return ioErr;
		}
    }
    addr.sin_family = AF_INET;
	for(i=0; i<CONNECT_RETRIES; i++) {
		sock = socket(AF_INET, SOCK_STREAM, 0);
		addr.sin_addr = host;
		addr.sin_port = htons((u_short)port);

		if (connect(sock, (struct sockaddr *) &addr, 
				sizeof(struct sockaddr_in)) == 0) {
			break;
		}
		/* retry */
		close(sock);
		fprintf(stderr, "%s ", hostName);
		perror("connect");
	}
	if(i == CONNECT_RETRIES) {
		return ioErr;
	}

	#if		NON_BLOCKING
	/* OK to do this after connect? */
	{
		int rtn = fcntl(sock, F_SETFL, O_NONBLOCK);
		if(rtn == -1) {
			perror("fctnl(O_NONBLOCK)");
			return ioErr;
		}
	}
	#endif	/* NON_BLOCKING*/
	
    peer->ipAddr = addr.sin_addr.s_addr;
    peer->port = htons((u_short)port);
	*socketNo = (otSocket)sock;
    return noErr;
}

/*
 * Accept a client connection.
 */
OSStatus AcceptClientConnection(
	int port, 
	otSocket *socketNo, 	// RETURNED
	PeerSpec *peer)			// RETURNED
{
	/* maybe some day */
	return unimpErr;
}

/*
 * Shut down a connection.
 */
void endpointShutdown(
	otSocket socket)
{
	close((int)socket);
}
	
/*
 * R/W. Called out from SSL.
 */
OSStatus SocketRead(
	SSLConnectionRef 	connection,
	void 				*data, 			/* owned by 
	 									 * caller, data
	 									 * RETURNED */
	size_t 				*dataLength)	/* IN/OUT */ 
{
	size_t			bytesToGo = *dataLength;
	size_t 			initLen = bytesToGo;
	UInt8			*currData = (UInt8 *)data;
	int		        sock = (int)((long)connection);
	OSStatus		rtn = noErr;
	size_t			bytesRead;
	int				rrtn;
	
	*dataLength = 0;

	for(;;) {
		bytesRead = 0;
		rrtn = read(sock, currData, bytesToGo);
		if (rrtn <= 0) {
			/* this is guesswork... */
			switch(errno) {
				case ENOENT:
					/* connection closed */
					rtn = errSSLClosedGraceful; 
					break;
				#if	NON_BLOCKING
				case EAGAIN:
				#else
				case 0:		/* ??? */
				#endif
					rtn = errSSLWouldBlock;
					break;
				default:
					dprintf(("SocketRead: read(%lu) error %d\n", 
						bytesToGo, errno));
					rtn = ioErr;
					break;
			}
			break;
		}
		else {
			bytesRead = rrtn;
		}
		bytesToGo -= bytesRead;
		currData  += bytesRead;
		
		if(bytesToGo == 0) {
			/* filled buffer with incoming data, done */
			break;
		}
	}
	*dataLength = initLen - bytesToGo;
	tprintf("SocketRead", initLen, *dataLength, (UInt8 *)data);
	
	#if SSL_OT_DOT || (SSL_OT_DEBUG && !SSL_OT_IO_TRACE)
	if((rtn == 0) && (*dataLength == 0)) {
		/* keep UI alive */
		outputDot();
	}
	#endif
	return rtn;
}

OSStatus SocketWrite(
	SSLConnectionRef 	connection,
	const void	 		*data, 
	size_t 				*dataLength)	/* IN/OUT */ 
{
	size_t		bytesSent = 0;
	int			sock = (int)((long)connection);
	int 		length;
	UInt32		dataLen = *dataLength;
	const UInt8 *dataPtr = (UInt8 *)data;
	OSStatus	ortn;
	
	*dataLength = 0;

    do {
        length = write(sock, 
				(char*)dataPtr + bytesSent, 
				dataLen - bytesSent);
    } while ((length > 0) && 
			 ( (bytesSent += length) < dataLen) );
	
	if(length == 0) {
		if(errno == EAGAIN) {
			ortn = errSSLWouldBlock;
		}
		else {
			ortn = ioErr;
		}
	}
	else {
		ortn = noErr;
	}
	tprintf("SocketWrite", dataLen, bytesSent, dataPtr);
	*dataLength = bytesSent;
	return ortn;
}
