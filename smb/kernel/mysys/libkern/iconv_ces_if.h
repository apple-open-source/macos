/*
 * This file is produced automatically.
 * Do not modify anything in here by hand.
 *
 * Created from source file
 *   iconv_ces_if.m
 * with
 *   ../../sys5/kern/makeobjops.pl
 *
 * See the source file for legal information
 */

#ifndef _iconv_ces_if_h_
#define _iconv_ces_if_h_

extern struct kobjop_desc iconv_ces_open_desc;
typedef int iconv_ces_open_t(struct iconv_ces *ces, const char *name);
static __inline int ICONV_CES_OPEN(struct iconv_ces *ces, const char *name)
{
	kobjop_t _m;
	KOBJOPLOOKUP(ces->ops,iconv_ces_open);
	return ((iconv_ces_open_t *) _m)(ces, name);
}

extern struct kobjop_desc iconv_ces_close_desc;
typedef void iconv_ces_close_t(struct iconv_ces *ces);
static __inline void ICONV_CES_CLOSE(struct iconv_ces *ces)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)ces)->ops,iconv_ces_close);
	((iconv_ces_close_t *) _m)(ces);
}

extern struct kobjop_desc iconv_ces_reset_desc;
typedef void iconv_ces_reset_t(struct iconv_ces *ces);
static __inline void ICONV_CES_RESET(struct iconv_ces *ces)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)ces)->ops,iconv_ces_reset);
	((iconv_ces_reset_t *) _m)(ces);
}

extern struct kobjop_desc iconv_ces_names_desc;
typedef char ** iconv_ces_names_t(struct iconv_ces_class *cesd);
static __inline char ** ICONV_CES_NAMES(struct iconv_ces_class *cesd)
{
	kobjop_t _m;
	KOBJOPLOOKUP(cesd->ops,iconv_ces_names);
	return ((iconv_ces_names_t *) _m)(cesd);
}

extern struct kobjop_desc iconv_ces_nbits_desc;
typedef int iconv_ces_nbits_t(struct iconv_ces *ces);
static __inline int ICONV_CES_NBITS(struct iconv_ces *ces)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)ces)->ops,iconv_ces_nbits);
	return ((iconv_ces_nbits_t *) _m)(ces);
}

extern struct kobjop_desc iconv_ces_nbytes_desc;
typedef int iconv_ces_nbytes_t(struct iconv_ces *ces);
static __inline int ICONV_CES_NBYTES(struct iconv_ces *ces)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)ces)->ops,iconv_ces_nbytes);
	return ((iconv_ces_nbytes_t *) _m)(ces);
}

extern struct kobjop_desc iconv_ces_fromucs_desc;
typedef int iconv_ces_fromucs_t(struct iconv_ces *ces, ucs_t in,
                                u_char **outbuf, size_t *outbytesleft);
static __inline int ICONV_CES_FROMUCS(struct iconv_ces *ces, ucs_t in,
                      u_char **outbuf, size_t *outbytesleft)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)ces)->ops,iconv_ces_fromucs);
	return ((iconv_ces_fromucs_t *) _m)(ces, in, outbuf, outbytesleft);
}

extern struct kobjop_desc iconv_ces_toucs_desc;
typedef ucs_t iconv_ces_toucs_t(struct iconv_ces *ces, const u_char **inbuf,
                                size_t *inbytesleft);
static __inline ucs_t ICONV_CES_TOUCS(struct iconv_ces *ces,
                      const u_char **inbuf, size_t *inbytesleft)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)ces)->ops,iconv_ces_toucs);
	return ((iconv_ces_toucs_t *) _m)(ces, inbuf, inbytesleft);
}

extern struct kobjop_desc iconv_ces_init_desc;
typedef int iconv_ces_init_t(struct iconv_ces_class *cesd);
static __inline int ICONV_CES_INIT(struct iconv_ces_class *cesd)
{
	kobjop_t _m;
	KOBJOPLOOKUP(cesd->ops,iconv_ces_init);
	return ((iconv_ces_init_t *) _m)(cesd);
}

extern struct kobjop_desc iconv_ces_done_desc;
typedef void iconv_ces_done_t(struct iconv_ces_class *cesd);
static __inline void ICONV_CES_DONE(struct iconv_ces_class *cesd)
{
	kobjop_t _m;
	KOBJOPLOOKUP(cesd->ops,iconv_ces_done);
	((iconv_ces_done_t *) _m)(cesd);
}

#endif /* _iconv_ces_if_h_ */
