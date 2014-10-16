/*
 * ioSock.h - socket-based I/O routines for SecureTransport tests
 */

#ifndef	_IO_SOCK_H_
#define _IO_SOCK_H_

#include <MacTypes.h>
#include <Security/SecureTransport.h>
#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Opaque reference to an Open Transport connection.
 */
typedef int otSocket;

/*
 * info about a peer returned from MakeServerConnection() and 
 * AcceptClientConnection().
 */
typedef struct
{   UInt32      ipAddr;
    int         port;
} PeerSpec;

/*
 * Ont-time only init.
 */
void initSslOt();

/*
 * Connect to server. 
 */
extern OSStatus MakeServerConnection(
	const char *hostName, 
	int port, 
	int nonBlocking,		// 0 or 1
	otSocket *socketNo, 	// RETURNED
	PeerSpec *peer);		// RETURNED

/*
 * Set up an otSocket to listen for client connections. Call once, then
 * use multiple AcceptClientConnection calls. 
 */
OSStatus ListenForClients(
	int port, 
	int nonBlocking,		// 0 or 1
	otSocket *socketNo); 	// RETURNED

/*
 * Accept a client connection. Call endpointShutdown() for each successful;
 * return from this function. 
 */
OSStatus AcceptClientConnection(
	otSocket listenSock, 		// obtained from ListenForClients
	otSocket *acceptSock, 		// RETURNED
	PeerSpec *peer);			// RETURNED

/*
 * Shut down a connection.
 */
void endpointShutdown(
	otSocket socket);
	
/*
 * R/W. Called out from SSL.
 */
OSStatus SocketRead(
	SSLConnectionRef 	connection,
	void 				*data, 			/* owned by 
	 									 * caller, data
	 									 * RETURNED */
	size_t 				*dataLength);	/* IN/OUT */ 
	
OSStatus SocketWrite(
	SSLConnectionRef 	connection,
	const void	 		*data, 
	size_t 				*dataLength);	/* IN/OUT */ 

#ifdef	__cplusplus
}
#endif

#endif	/* _IO_SOCK_H_ */
