#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/smb_apple.h>
#include <sys/smb_iconv.h>


#include "iconv_ces_if.h"

struct iconv_ces_table {
	struct iconv_ces	ces;
	struct iconv_ccs	*ccs;
};

#define	OBJTOTBL(obj)	((struct iconv_ces_table*)(obj))
#define	NBITS(ces)	((ces)->ccs->cs_desc->cd_nbits)
#define iconv_char32bit(ch)	((ch) & 0xFFFF0000)

static int
iconv_ces_table_open(struct iconv_ces *cesobj, const char *name)
{
	struct iconv_ces_table *ces = OBJTOTBL(cesobj);
	int error;

	if (name == NULL)
		return ENOENT;
	ces->ccs = NULL;
	error = iconv_ccs_open(name, &ces->ccs);
	printf("got new ccs %x\n", (u_int)ces->ccs->cs_desc);
	return error;
}

static int
iconv_ces_table_close(struct iconv_ces *cesobj)
{
	struct iconv_ces_table *ces = OBJTOTBL(cesobj);

	if (ces->ccs)
		iconv_ccs_close(ces->ccs);
	return 0;
}

static const char *iconv_ces_table_namelist[] = {
	"table",
	NULL
};

static const char *const *
iconv_ces_table_names(struct iconv_ces *cesobj)
{

	return iconv_ces_table_namelist;
}

static int
iconv_ces_table_nbits(struct iconv_ces *cesobj)
{
	struct iconv_ces_table *ces = OBJTOTBL(cesobj);
	int nb = NBITS(ces);

	return nb > 8 ? nb >> 1 : nb;
}

static int
iconv_ces_table_nbytes(struct iconv_ces *cesobj)
{
	struct iconv_ces_table *ces = OBJTOTBL(cesobj);
	int nb = NBITS(ces);

	return nb == 16 ? 0 : (nb > 8 ? 2 : 1);
}

static ssize_t
iconv_ces_table_fromucs(struct iconv_ces *cesobj, ucs_t in,
	unsigned char **outbuf, size_t *outbytesleft)
{
	struct iconv_ces_table *ces = OBJTOTBL(cesobj);
	ucs_t res;
	size_t bytes;

	if (in == UCS_CHAR_NONE)
		return 1;	/* No state reinitialization for table charsets */
	if (iconv_char32bit(in))
		return -1;
	res = ICONV_CCS_FROMUCS(ces->ccs, in);
	if (res == UCS_CHAR_INVALID)
		return -1;	/* No character in output charset */
	bytes = res & 0xFF00 ? 2 : 1;
	if (*outbytesleft < bytes)
		return 0;	/* No space in output buffer */
	if (bytes == 2)
		*(*outbuf)++ = (res >> 8) & 0xFF;
	*(*outbuf)++ = res & 0xFF;
	*outbytesleft -= bytes;
	return 1;
}

static ucs_t
iconv_ces_table_toucs(struct iconv_ces *cesobj, const unsigned char **inbuf,
	size_t *inbytesleft)
{
	struct iconv_ces_table *ces = OBJTOTBL(cesobj);
	unsigned char byte = *(*inbuf);
	ucs_t res = ICONV_CCS_TOUCS(ces->ccs, byte);
	size_t bytes = (res == UCS_CHAR_INVALID && NBITS(ces) > 8) ? 2 : 1;

	if (*inbytesleft < bytes)
		return UCS_CHAR_NONE;	/* Not enough bytes in the input buffer */
	if (bytes == 2)
    		res = ICONV_CCS_TOUCS(ces->ccs, (byte << 8) | (* ++(*inbuf)));
	(*inbuf) ++;
	*inbytesleft -= bytes;
	return res;
}

static kobj_method_t iconv_ces_table_methods[] = {
	KOBJMETHOD(iconv_ces_open,	iconv_ces_table_open),
	KOBJMETHOD(iconv_ces_close,	iconv_ces_table_close),
	KOBJMETHOD(iconv_ces_names,	iconv_ces_table_names),
	KOBJMETHOD(iconv_ces_nbits,	iconv_ces_table_nbits),
	KOBJMETHOD(iconv_ces_nbytes,	iconv_ces_table_nbytes),
	KOBJMETHOD(iconv_ces_fromucs,	iconv_ces_table_fromucs),
	KOBJMETHOD(iconv_ces_toucs,	iconv_ces_table_toucs),
#if 0
	KOBJMETHOD(iconv_ces_init,	iconv_ces_table_init),
	KOBJMETHOD(iconv_ces_done,	iconv_ces_table_done),
#endif
	{0, 0}
};

KICONV_CES(table, sizeof(struct iconv_ces_table));
