/*
 * socket.h -- declarations for socket library functions
 *
 * For license terms, see the file COPYING in this directory.
 */

#ifndef SOCKET__
#define SOCKET__

/* Create a new client socket; returns -1 on error */
int SockOpen(const char *host, const char *service, const char *plugin);

/* Returns 1 if this socket is OK, 0 if it isn't select()able
 * on - probably because it's been closed. You should
 * always check this function before passing stuff to the
 * select()-based waiter, as otherwise it may loop. 
 */
int SockCheckOpen(int fd);

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

/*
FIXME: document this
*/
int UnixOpen(const char *path);

#ifdef SSL_ENABLE
int SSLOpen(int sock, char *mycert, char *mykey, char *myproto, int certck, char *certpath,
    char *fingerprint, char *servercname, char *label);
#endif /* SSL_ENABLE */

#endif /* SOCKET__ */
