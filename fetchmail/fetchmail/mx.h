/* mx.h -- name-to-preference association for MX records.
 * For license terms, see the file COPYING in this directory.
 */

struct mxentry
{
    unsigned char	*name;
    int			pref;
};

extern struct mxentry * getmxrecords(const char *);

/* some versions of FreeBSD should declare this but don't */
/* But only declare it if it isn't already */
#ifndef h_errno
extern int h_errno;
#endif /* ndef h_errno */

/* mx.h ends here */
