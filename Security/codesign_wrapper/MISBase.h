#ifndef MISBASE_H
#define MISBASE_H

/* Define the decoration we use to indicate public functions */
#if defined(__GNUC__)
#define MIS_EXPORT extern __attribute__((visibility ("default")))
#else
#define MIS_EXPORT
#endif

/* Define ssize_t if needed */
#if defined(__WIN32__)
typedef long ssize_t;
#endif

/* Maximum path length */
#if !defined(MAXPATHLEN)
#if defined(__WIN32__)
/*
 * On windows, we handle paths up to length MAX_PATH.  However,
 * we convert paths into UTF8 in a number of places and since
 * each character in the path can map to up to 4 bytes in UTF8,
 * we need to account for that.
 */
#define MAXPATHLEN (MAX_PATH * 4)
#else
#define MAXPATHLEN 1024
#endif
#endif

/*
 *  The crazy CF situation on Windows requires this macro so we can build
 *  against either version
 *  The version of CF used by iTunes uses the system encoding as the file
 *  encoding.  However we need paths to be UTF8 when we pass them into
 *  various file system APIs so we use CFStringGetCString instead.  I'm
 *  told that the other CF does return UTF8 so if iTunes ever moves to
 *  that we can switch back to CFStringGetFileSystemRepresentation (5513199)
 */
#if defined(__WIN32__)
#define CFStringGetFileSystemRepresentation(str, buf, len) CFStringGetCString(str, buf, len, kCFStringEncodingUTF8)
#endif

#endif
