/*
 * socket.h -- declarations for socket library functions
 *
 * For license terms, see the file COPYING in this directory.
 */

#ifndef SOCKET__
#define SOCKET__

/* Create a new client socket; returns (FILE *)NULL on error */
#if INET6
int SockOpen(const char *host, const char *service, const char *options,
	     const char *plugin);
#else /* INET6 */
int SockOpen(const char *host, int clientPort, const char *options,
	     const char *plugin);
#endif /* INET6 */

/* 
Get a string terminated by an '\n' (matches interface of fgets).
Pass it a valid socket, a buffer for the string, and
the length of the buffer (including the trailing \0)
returns length of buffer on success, -1 on failure. 
*/
int SockRead(int sock, char *buf, int len);

/*
 * Peek at the next socket character without actually reading it.
 */
int SockPeek(int sock);

/*
Write a chunk of bytes to the socket (matches interface of fwrite).
Returns number of bytes successfully written.
*/
int SockWrite(int sock, char *buf, int size);

/* 
Send formatted output to the socket (matches interface of fprintf).
Returns number of bytes successfully written.
*/
#if defined(HAVE_STDARG_H)
int SockPrintf(int sock, const char *format, ...) ;
#else
int SockPrintf();
#endif
 
/*
Close a socket previously opened by SockOpen.  This allows for some
additional clean-up if necessary.
*/
int SockClose(int sock);

#endif /* SOCKET__ */
