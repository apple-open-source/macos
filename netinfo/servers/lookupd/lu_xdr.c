#include "lu_xdr.h"
#include <stdlib.h>
#include <string.h>

lu_xdr_t *
lu_xdr_alloc(u_int32_t len, u_int32_t delta)
{
	lu_xdr_t *x;

	if (len == 0) len = LU_XDR_DEFAULT_LENGTH;
	if (delta == 0) delta = LU_XDR_DEFAULT_DELTA;

	x = (lu_xdr_t *)calloc(1, sizeof(lu_xdr_t));
	if (x == NULL) return NULL;

	x->buf = calloc(1, len);
	if (x->buf == NULL)
	{
		free(x);
		return NULL;
	}

	x->buflen = len;
	x->cursor = x->buf;
	x->datalen = 0;
	x->pos = 0;
	x->delta = delta;
	x->code = LU_XDR_ENCODE;

	return x;
}

lu_xdr_t *
lu_xdr_from_buffer(char *b, u_int32_t len, u_int32_t op)
{
	lu_xdr_t *x;

	if (b == NULL) return NULL;
	if (len == 0) return NULL;

	x = (lu_xdr_t *)calloc(1, sizeof(lu_xdr_t));
	if (x == NULL) return NULL;

	x->buf = b;
	x->buflen = len;
	x->cursor = x->buf;
	x->delta = 0;
	x->code = op;
	x->pos = 0;
	x->datalen = 0;
	if (op == LU_XDR_DECODE) x->datalen = len;

	return x;
}

void
lu_xdr_free(lu_xdr_t *x)
{
	if (x == NULL) return;

	if ((x->delta != 0) && (x->buf != NULL)) free(x->buf);
	free(x);
}

/*
 * Switch from decode to encode
 * Rewinds (re-uses) the buffer
 */
int32_t
lu_xdr_encode(lu_xdr_t *x)
{
	if (x == NULL) return -1;
	if (x->code == LU_XDR_ENCODE) return 0;

	x->cursor = x->buf;
	x->datalen = 0;
	x->pos = 0;
	x->code = LU_XDR_ENCODE;

	return 0;
}
	
/*
 * Switch from encode to decode
 * Rewinds the buffer
 */
int32_t
lu_xdr_decode(lu_xdr_t *x)
{
	if (x == NULL) return -1;
	if (x->code == LU_XDR_DECODE) return 0;

	x->cursor = x->buf;
	x->pos = 0;
	x->code = LU_XDR_DECODE;

	return 0;
}

u_int32_t
lu_xdr_getpos(lu_xdr_t *x)
{
	if (x == NULL) return 0;
	return x->pos;
}

int32_t
lu_xdr_setpos(lu_xdr_t *x, u_int32_t p)
{
	if (x == NULL) return -1;
	if (p > x->datalen) return -1;

	x->cursor = x->buf + p;
	return 0;
}

int32_t
lu_xdr_u_int_32(lu_xdr_t *x, u_int32_t *i)
{
	u_int32_t d, n;
	int32_t need;

	if (x == NULL) return -1;

	if (x->code == LU_XDR_DECODE)
	{
		if ((x->pos + 4) > x->datalen) return -1;

		memmove(&d, x->cursor, 4);
		*i = ntohl(d);
		x->cursor += 4;
		x->pos += 4;
		return 0;
	}

	n = 0;
	need = 4 - (x->buflen - x->pos);

	if (need > 0)
	{
		if (x->delta == 0) return -1;
		n = (need + (x->delta - 1)) / x->delta;
	}

	if (n > 0)
	{
		x->buflen += (n * x->delta);
		x->buf = realloc(x->buf, x->buflen);
		if (x->buf == NULL) return -1;
		x->cursor = x->buf + x->pos;
	}

	d = htonl(*i);
	memmove(x->cursor, &d, 4);
	x->cursor +=4;
	x->pos += 4;
	x->datalen += 4;
	return 0;
}

int32_t
lu_xdr_int_32(lu_xdr_t *x, int32_t *i)
{
	return lu_xdr_u_int_32(x, (u_int32_t *)i);
}

int32_t
lu_xdr_buffer(lu_xdr_t *x, char **s, u_int32_t *l)
{
	int32_t status, need;
	u_int32_t n, len, xlen;
	char *t;

	if (x == NULL) return -1;
	if (s == NULL) return -1;

	if (x->code == LU_XDR_DECODE)
	{
		status = lu_xdr_u_int_32(x, l);
		if (status != 0) return status;

		if (*l == 0)
		{
			*s = NULL;
			return 0;
		}

		len = *l;
		if ((x->pos + len) > x->datalen) return -1;

		t = malloc(len);
		if (t == NULL) return -1;

		memmove(t, x->cursor, len);
		*s = t;
		xlen = ((len + 3) / 4) * 4;
		x->cursor += xlen;
		x->pos += xlen;
		return 0;
	}

	len = *l;
	xlen = ((len + 3) / 4) * 4;

	n = 0;
	need = (xlen + 4) - (x->buflen - x->pos);

	if (need > 0)
	{
		if (x->delta == 0) return -1;
		n = (need + (x->delta - 1)) / x->delta;
	}

	if (n > 0)
	{
		x->buflen += (n * x->delta);
		x->buf = realloc(x->buf, x->buflen);
		if (x->buf == NULL) return -1;
		x->cursor = x->buf + x->pos;
	}

	n = htonl(len);
	memmove(x->cursor, &n, 4);
	x->cursor += 4;
	x->pos += 4;
	x->datalen += 4;

	memset(x->cursor, 0, xlen);
	memmove(x->cursor, *s, len);
	x->cursor += xlen;
	x->pos += xlen;
	x->datalen += xlen;

	return 0;
}

int32_t
lu_xdr_string(lu_xdr_t *x, char **s)
{
	u_int32_t len;
	int32_t status;

	if (x == NULL) return -1;
	if (s == NULL) return -1;

	if (x->code == LU_XDR_DECODE)
	{
		status = lu_xdr_u_int_32(x, &len);
		if (status != 0) return status;

		if (len == 0)
		{
			*s = calloc(1, 1);
			if ((*s) == NULL) return -1;
			return 0;
		}

		if ((x->pos + len) > x->datalen) return -1;

		*s = calloc(1, len + 1);
		if ((*s) == NULL) return -1;

		memmove(*s, x->cursor, len);
		len = ((len + 3) / 4) * 4;
		x->cursor += len;
		x->pos += len;
		return 0;
	}

	if (*s == NULL) return -1;
	len = strlen(*s) + 1;

	return lu_xdr_buffer(x, s, &len);
}
