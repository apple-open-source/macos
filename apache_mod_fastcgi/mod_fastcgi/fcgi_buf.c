/*
 * $Id: fcgi_buf.c,v 1.18 2003/02/03 23:07:37 robs Exp $
 */

#include "fcgi.h"

#ifdef WIN32
#pragma warning( disable : 4127 )
#else
#ifdef APACHE2
#include <unistd.h>
#endif
#endif

/*******************************************************************************
 * Check buffer consistency with assertions.
 */
#ifdef DEBUG
static void fcgi_buf_check(Buffer *buf)
{
    ASSERT(buf->size > 0);
    ASSERT(buf->length >= 0);
    ASSERT(buf->length <= buf->size);

    ASSERT(buf->begin >= buf->data);
    ASSERT(buf->begin < buf->data + buf->size);
    ASSERT(buf->end >= buf->data);
    ASSERT(buf->end < buf->data + buf->size);

    ASSERT(((buf->end - buf->begin + buf->size) % buf->size)
            == (buf->length % buf->size));
}
#else
#define fcgi_buf_check(a) ((void) 0)
#endif
 
/*******************************************************************************
 * Reset buffer, losing any data that's in it.
 */
void fcgi_buf_reset(Buffer *buf)
{
    buf->length = 0;
    buf->begin = buf->end = buf->data;
}

/*******************************************************************************
 * Allocate and intialize a new buffer of the specified size.
 */
Buffer *fcgi_buf_new(pool *p, int size)
{
    Buffer *buf;

    buf = (Buffer *)apr_pcalloc(p, sizeof(Buffer) + size);
    buf->size = size;
    fcgi_buf_reset(buf);
    return buf;
}

void fcgi_buf_removed(Buffer * const b, unsigned int len)
{
    b->length -= len;
    b->begin += len;

    if (b->length == 0)
    {
        b->begin = b->end = b->data;
    }
    else if (b->begin >= b->data + b->size)
    {
        b->begin -= b->size;
    }
}

void fcgi_buf_added(Buffer * const b, const unsigned int len)
{
    b->length += len;
    b->end += len;

    if (b->end >= b->data + b->size)
    {
        b->end -= b->size;
    }
}

#ifdef WIN32

static int socket_recv(SOCKET fd, char *buf, int len)
{
    int bytes_read = recv(fd, buf, len, 0);

    if (bytes_read == SOCKET_ERROR) 
    {
        return -1;
    }
    return bytes_read;
}

static int socket_send(SOCKET fd, char * buf, int len)
{
    int bytes_sent = send(fd, buf, len, 0);

    if (bytes_sent == SOCKET_ERROR) 
    {
        return -1;
    }
    return bytes_sent;
}

#else /* !WIN32 */

static int socket_recv(int fd, char * buf, int len)
{
    int bytes_read;

    do {
        bytes_read = read(fd, buf, len);

        if (bytes_read < 0)
        {
#ifdef EWOULDBLOCK
            ASSERT(errno != EWOULDBLOCK);
#endif
#ifdef EAGAIN
            ASSERT(errno != EAGAIN);
#endif
        }
    } while (bytes_read == -1 && errno == EINTR);

    return bytes_read;
}

static int socket_send(int fd, char * buf, int len)
{
    int bytes_sent;

    do {
        bytes_sent = write(fd, buf, len);

        if (bytes_sent < 0)
        {
#ifdef EWOULDBLOCK
            ASSERT(errno != EWOULDBLOCK);
#endif
#ifdef EAGAIN
            ASSERT(errno != EAGAIN);
#endif
        }
    } 
    while (bytes_sent == -1 && errno == EINTR);

    return bytes_sent;
}

#endif /* !WIN32 */

/*******************************************************************************
 * Read from an open file descriptor into buffer.
 *
 * The caller should disable the default Apache SIGPIPE handler,
 * otherwise a bad script could cause the request to abort and appear
 * as though the client's fd caused it.
 *
 * Results:
 *      <0 error, errno is set
 *      =0 EOF reached
 *      >0 successful read or no room in buffer (NOT # of bytes read)
 */
int fcgi_buf_socket_recv(Buffer *buf, SOCKET fd)
{
    int len;

    fcgi_buf_check(buf);

    if (buf->length == buf->size)
        /* there's no room in the buffer, return "success" */
        return 1;

    if (buf->length == 0)
        /* the buffer is empty so defrag */
        buf->begin = buf->end = buf->data;

    len = min(buf->size - buf->length, buf->data + buf->size - buf->end);

#ifndef NO_WRITEV

    /* assume there is a readv() since there is a writev() */
    if (len == buf->size - buf->length) 
    {
#endif

        len = socket_recv(fd, buf->end, len);

#ifndef NO_WRITEV
    } 
    else 
    {
        /* the buffer is wrapped, use readv() */
        struct iovec vec[2];

        vec[0].iov_base = buf->end;
        vec[0].iov_len = len;
        vec[1].iov_base = buf->data;
        vec[1].iov_len = buf->size - buf->length - len;

        ASSERT(len);
        ASSERT(vec[1].iov_len);

        do
        {
            len = readv(fd, vec, 2);
        }
        while (len == -1 && errno == EINTR);
    }
#endif

    if (len <= 0) return len;

    fcgi_buf_added(buf, len);

    return len;     /* this may not contain the number of bytes read */
}


/*******************************************************************************
 * Write from the buffer to an open file descriptor.
 *
 * The caller should disable the default Apache SIGPIPE handler,
 * otherwise a bad script could cause the request to abort appearing
 * as though the client's fd caused it.
 *
 * Results:
 *      <0 if an error occured (bytes may or may not have been written)
 *      =0 if no bytes were written
 *      >0 successful write
 */
int fcgi_buf_socket_send(Buffer *buf, SOCKET fd)
{
    int len;

    fcgi_buf_check(buf);

    if (buf->length == 0)
        return 0;

    len = min(buf->length, buf->data + buf->size - buf->begin);

#ifndef NO_WRITEV
    if (len == buf->length) 
    {
#endif

        len = socket_send(fd, buf->begin, len);

#ifndef NO_WRITEV
    } 
    else 
    {
        struct iovec vec[2];

        vec[0].iov_base = buf->begin;
        vec[0].iov_len = len;
        vec[1].iov_base = buf->data;
        vec[1].iov_len = buf->length - len;

        do
        {
            len = writev(fd, vec, 2);
        }
        while (len == -1 && errno == EINTR);
    }
#endif

    if (len <= 0) return len;

    fcgi_buf_removed(buf, len);

    return len;
}

/*******************************************************************************
 * Return the data block start address and the length of the block.
 */
void fcgi_buf_get_block_info(Buffer *buf, char **beginPtr, int *countPtr)
{
    fcgi_buf_check(buf);

    *beginPtr = buf->begin;
    *countPtr = min(buf->length, buf->data + buf->size - buf->begin);
}

/*******************************************************************************
 * Throw away bytes from buffer.
 */
void fcgi_buf_toss(Buffer *buf, int count)
{
    fcgi_buf_check(buf);
    ASSERT(count >= 0);
    ASSERT(count <= buf->length);

    buf->length -= count;
    buf->begin += count;
    if(buf->begin >= buf->data + buf->size) {
        buf->begin -= buf->size;
    }
}

/*******************************************************************************
 * Return the free data block start address and the length of the block.
 */
void fcgi_buf_get_free_block_info(Buffer *buf, char **endPtr, int *countPtr)
{
    fcgi_buf_check(buf);

    *endPtr = buf->end;
    *countPtr = min(buf->size - buf->length,
                    buf->data + buf->size - buf->end);
}

/*******************************************************************************
 * Updates the buf to reflect recently added data.
 */
void fcgi_buf_add_update(Buffer *buf, int count)
{
    fcgi_buf_check(buf);
    ASSERT(count >= 0);
    ASSERT(count <= BufferFree(buf));

    buf->length += count;
    buf->end += count;
    if(buf->end >= buf->data + buf->size) {
        buf->end -= buf->size;
    }

    fcgi_buf_check(buf);
}

/*******************************************************************************
 * Adds a block of data to a buffer, returning the number of bytes added.
 */
int fcgi_buf_add_block(Buffer *buf, char *data, int datalen)
{
    char *end;
    int copied = 0;     /* Number of bytes actually copied. */
    int canCopy;        /* Number of bytes to copy in a given op. */

    ASSERT(data != NULL);
    ASSERT(datalen >= 0);

    if(datalen == 0) {
        return 0;
    }

    ASSERT(datalen > 0);
    fcgi_buf_check(buf);
    end = buf->data + buf->size;

    /*
     * Copy the first part of the data:  from here to the end of the
     * buffer, or the end of the data, whichever comes first.
     */
    datalen = min(BufferFree(buf), datalen);
    canCopy = min(datalen, end - buf->end);
    memcpy(buf->end, data, canCopy);
    buf->length += canCopy;
    buf->end += canCopy;
    copied += canCopy;
    if (buf->end >= end) {
        buf->end = buf->data;
    }
    datalen -= canCopy;

    /*
     * If there's more to go, copy the second part starting from the
     * beginning of the buffer.
     */
    if (datalen > 0) {
        data += canCopy;
        memcpy(buf->end, data, datalen);
        buf->length += datalen;
        buf->end += datalen;
        copied += datalen;
    }
    return(copied);
}

/*******************************************************************************
 * Add a string to a buffer, returning the number of bytes added.
 */
int fcgi_buf_add_string(Buffer *buf, char *str)
{
    return fcgi_buf_add_block(buf, str, strlen(str));
}

/*******************************************************************************
 * Gets a data block from a buffer, returning the number of bytes copied.
 */
int fcgi_buf_get_to_block(Buffer *buf, char *data, int datalen)
{
    char *end;
    int copied = 0;                /* Number of bytes actually copied. */
    int canCopy;                   /* Number of bytes to copy in a given op. */

    ASSERT(data != NULL);
    ASSERT(datalen > 0);
    fcgi_buf_check(buf);

    end = buf->data + buf->size;

    /*
     * Copy the first part out of the buffer: from here to the end
     * of the buffer, or all of the requested data.
     */
    canCopy = min(buf->length, datalen);
    canCopy = min(canCopy, end - buf->begin);

    memcpy(data, buf->begin, canCopy);

    buf->length -= canCopy;
    buf->begin += canCopy;
    copied += canCopy;
    if (buf->begin >= end) {
        buf->begin = buf->data;
    }

    /*
     * If there's more to go, copy the second part starting from the
     * beginning of the buffer.
     */
    if (copied < datalen && buf->length > 0) {
        data += copied;
        canCopy = min(buf->length, datalen - copied);

        memcpy(data, buf->begin, canCopy);

        buf->length -= canCopy;
        buf->begin += canCopy;
        copied += canCopy;
    }

    fcgi_buf_check(buf);
    return(copied);
}

/*******************************************************************************
 * Move 'len' bytes from 'src' buffer to 'dest' buffer.  There must be at
 * least 'len' bytes available in the source buffer and space for 'len'
 * bytes in the destination buffer.
 */
void fcgi_buf_get_to_buf(Buffer *dest, Buffer *src, int len)
{
    char *dest_end, *src_begin;
    int dest_len, src_len, move_len;

    ASSERT(len > 0);
    ASSERT(BufferLength(src) >= len);
    ASSERT(BufferFree(dest) >= len);

    fcgi_buf_check(src);
    fcgi_buf_check(dest);

    for (;;) {
        if (len == 0)
            return;

        fcgi_buf_get_free_block_info(dest, &dest_end, &dest_len);
        fcgi_buf_get_block_info(src, &src_begin, &src_len);

        move_len = min(dest_len, src_len);
        move_len = min(move_len, len);

        if (move_len == 0)
            return;

        memcpy(dest_end, src_begin, move_len);
        fcgi_buf_toss(src, move_len);
        fcgi_buf_add_update(dest, move_len);
        len -= move_len;
    }
}

static void array_grow(array_header *arr, int n)
{
    if (n <= 0)
        return;

    if (arr->nelts + n > arr->nalloc) {
        char *new_elts;
        int new_nalloc = (arr->nalloc <= 0) ? n : arr->nelts + n;

        new_elts = apr_pcalloc(arr->pool, arr->elt_size * new_nalloc);
        memcpy(new_elts, arr->elts, arr->nelts * arr->elt_size);

        arr->elts = new_elts;
        arr->nalloc = new_nalloc;
    }
}

static void array_cat_block(array_header *arr, void *block, int n)
{
    array_grow(arr, n);
    memcpy(arr->elts + arr->nelts * arr->elt_size, block, n * arr->elt_size);
    arr->nelts += n;
}

/*----------------------------------------------------------------------
 * Append "len" bytes from "buf" into "arr".  Apache arrays are used
 * whenever the data being handled is binary (may contain null chars).
 */
void fcgi_buf_get_to_array(Buffer *buf, array_header *arr, int len)
{
    int len1 = min(buf->length, buf->data + buf->size - buf->begin);

    fcgi_buf_check(buf);
    ASSERT(len > 0);
    ASSERT(len <= BufferLength(buf));

    array_grow(arr, len);

    len1 = min(len1, len);
    array_cat_block(arr, buf->begin, len1);

    if (len1 < len)
        array_cat_block(arr, buf->data, len - len1);

    fcgi_buf_toss(buf, len);
}
