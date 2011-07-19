/*!
    @header
        String compatibility routines.
    @indexgroup
        HeaderDoc C Libraries
 */

/*
   Function descriptions came out of strcompat.c.  See that
   file for copyright details.
 */

/*!
 * @abstract
 * Copies one string to another string of a given storage size.
 * @discussion
 * Copy src to string dst of size siz.  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t strlcpy(char *dst, const char *src, size_t siz);

/*!
 * @abstract
 * Appends one string to another string of a given storage size.
 * @discussion
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz <= strlen(dst)).
 * Returns strlen(src) + MIN(siz, strlen(initial dst)).
 * If retval >= siz, truncation occurred.
 */
size_t strlcat(char *dst, const char *src, size_t siz);

