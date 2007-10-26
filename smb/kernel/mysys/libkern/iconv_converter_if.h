/*
 * This file is produced automatically.
 * Do not modify anything in here by hand.
 *
 * Created from source file
 *   iconv_converter_if.m
 * with
 *   ../../sys5/kern/makeobjops.pl
 *
 * See the source file for legal information
 */

#ifndef _iconv_converter_if_h_
#define _iconv_converter_if_h_

extern struct kobjop_desc iconv_converter_open_desc;
typedef int iconv_converter_open_t(struct iconv_converter_class *dcp,
                                   struct iconv_cspair *cspto,
                                   struct iconv_cspair *cspfrom, void **hpp);
static __inline int ICONV_CONVERTER_OPEN(struct iconv_converter_class *dcp,
                         struct iconv_cspair *cspto,
                         struct iconv_cspair *cspfrom, void **hpp)
{
	kobjop_t _m;
	KOBJOPLOOKUP(dcp->ops,iconv_converter_open);
	return ((iconv_converter_open_t *) _m)(dcp, cspto, cspfrom, hpp);
}

extern struct kobjop_desc iconv_converter_close_desc;
typedef int iconv_converter_close_t(void *handle);
static __inline int ICONV_CONVERTER_CLOSE(void *handle)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)handle)->ops,iconv_converter_close);
	return ((iconv_converter_close_t *) _m)(handle);
}

extern struct kobjop_desc iconv_converter_conv_desc;
typedef int iconv_converter_conv_t(void *handle, const char **inbuf,
                                   size_t *inbytesleft, char **outbuf,
                                   size_t *outbytesleft, int flags);
static __inline int ICONV_CONVERTER_CONV(void *handle, const char **inbuf,
                         size_t *inbytesleft, char **outbuf,
                         size_t *outbytesleft, int flags)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)handle)->ops,iconv_converter_conv);
	return ((iconv_converter_conv_t *) _m)(handle, inbuf, inbytesleft, outbuf, outbytesleft, flags);
}

extern struct kobjop_desc iconv_converter_init_desc;
typedef int iconv_converter_init_t(struct iconv_converter_class *dcp);
static __inline int ICONV_CONVERTER_INIT(struct iconv_converter_class *dcp)
{
	kobjop_t _m;
	KOBJOPLOOKUP(dcp->ops,iconv_converter_init);
	return ((iconv_converter_init_t *) _m)(dcp);
}

extern struct kobjop_desc iconv_converter_done_desc;
typedef void iconv_converter_done_t(struct iconv_converter_class *dcp);
static __inline void ICONV_CONVERTER_DONE(struct iconv_converter_class *dcp)
{
	kobjop_t _m;
	KOBJOPLOOKUP(dcp->ops,iconv_converter_done);
	((iconv_converter_done_t *) _m)(dcp);
}

extern struct kobjop_desc iconv_converter_name_desc;
typedef const char * iconv_converter_name_t(struct iconv_converter_class *dcp);
static __inline const char * ICONV_CONVERTER_NAME(struct iconv_converter_class *dcp)
{
	kobjop_t _m;
	KOBJOPLOOKUP(dcp->ops,iconv_converter_name);
	return ((iconv_converter_name_t *) _m)(dcp);
}

#endif /* _iconv_converter_if_h_ */
