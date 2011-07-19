/**
 * \file socket.h -- declarations for socket library functions
 *
 * For license terms, see the file COPYING in this directory.
 */

#ifndef SOCKET__
#define SOCKET__

struct addrinfo;

#include <config.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#elif HAVE_NET_SOCKET_H
#include <net/socket.h>
#endif
#include <netdb.h>

/** Create a new client socket; returns -1 on error */
int SockOpen(const char *host, const char *service, const char *plugin, struct addrinfo **);

/** Returns 1 if socket \a fd is OK, 0 if it isn't select()able
 * on - probably because it's been closed. You should
 * always check this function before passing stuff to the
 * select()-based waiter, as otherwise it may loop. 
 */
int SockCheckOpen(int fd);

/** 
Get a string terminated by an '\n' (matches interface of fgets).
Pass it a valid socket, a buffer for the string, and
the length of the buffer (including the trailing \0)
returns length of buffer on success, -1 on failure. 
*/
int SockRead(int sock, char *buf, int len);

/**
 * Peek at the next socket character without actually reading it.
 */
int SockPeek(int sock);

/**
Write a chunk of bytes to the socket (matches interface of fwrite).
Returns number of bytes successfully written.
*/
int SockWrite(int sock, const char *buf, int size);

/* from /usr/include/sys/cdefs.h */
#if !defined __GNUC__ || __GNUC__ < 2
# define __attribute__(xyz)    /* Ignore. */
#endif

/**
Send formatted output to the socket (matches interface of fprintf).
Returns number of bytes successfully written.
*/
#if defined(HAVE_STDARG_H)
int SockPrintf(int sock, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)))
    ;
#else
int SockPrintf();
#endif
 
/**
Close a socket previously opened by SockOpen.  This allows for some
additional clean-up if necessary.
*/
int SockClose(int sock);

/**
 \todo document this
*/
int UnixOpen(const char *path);

#ifdef SSL_ENABLE
int SSLOpen(int sock, char *mycert, char *mykey, const char *myproto, int certck, char *cacertfile, char *cacertpath,
    char *fingerprint, char *servercname, char *label, char **remotename);
#endif /* SSL_ENABLE */

#endif /* SOCKET__ */
