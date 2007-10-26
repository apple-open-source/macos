#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/kobj.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/smb_apple.h>
#include <sys/smb_iconv.h>
#include <sys/malloc.h>

#ifdef ICONV_DEBUG

static char csfrom[20] = "koi8-r", csto[20] = "big5";
static char inbuf[100], outbuf[100];
static int inblen, outblen;

SYSCTL_DECL(_net_smb_fs_iconv);

SYSCTL_STRING(_net_smb_fs_iconv, OID_AUTO, csfrom, CTLFLAG_RW, 
	    &csfrom, sizeof(csfrom), "source cs name");
SYSCTL_STRING(_net_smb_fs_iconv, OID_AUTO, csto, CTLFLAG_RW, 
	    &csto, sizeof(csto), "dest cs name");

static int
iconv_sysctl_conv(SYSCTL_HANDLER_ARGS)
{
	const char *src;
	char *dst;
	void *handle;
	int error, datasz, reslen;

	SYSCTL_OUT(req, outbuf, outblen);
	ICDEBUG("entering\n");
	if (csfrom[0] == 0 || csto[0] == 0 || req->newptr == NULL)
		return 0;
	ICDEBUG("first check passed\n");
	datasz = req->newlen - req->newidx;
	if (datasz > sizeof(inbuf))
		return EINVAL;
	ICDEBUG("second check passed\n");
	error = SYSCTL_IN(req, inbuf, datasz);
	if (error)
		return error;
	ICDEBUG("SYSCTL_IN passed\n");
	error = iconv_open(csto, csfrom, &handle);
	if (error)
		return error;
	ICDEBUG("iconv_open() passed\n");
	src = inbuf;
	dst = outbuf;
	reslen = sizeof(outbuf);
	error = iconv_conv(handle, &src, &datasz, &dst, &reslen, NO_SFM_CONVERSIONS);
	iconv_close(handle);
	outblen = sizeof(outbuf) - reslen;
/*	SYSCTL_OUT(req, outbuf, reslen);*/
	return error;
}

SYSCTL_PROC(_net_smb_fs_iconv, OID_AUTO, conv, CTLFLAG_RW | CTLTYPE_STRING,
	    NULL, 0, iconv_sysctl_conv, "S,NULL", "convert");

#endif
