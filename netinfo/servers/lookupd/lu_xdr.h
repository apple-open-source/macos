#include <sys/types.h>

typedef struct
{
	char *buf;
	char *cursor;
	u_int32_t datalen;
	u_int32_t pos;
	u_int32_t buflen;
	u_int32_t delta;
	u_int32_t code;
} lu_xdr_t;

#define LU_XDR_DEFAULT_LENGTH 8192
#define LU_XDR_DEFAULT_DELTA 1024
#define LU_XDR_ENCODE 0
#define LU_XDR_DECODE 1

lu_xdr_t *lu_xdr_alloc(u_int32_t len, u_int32_t delta);
lu_xdr_t *lu_xdr_from_buffer(char *b, u_int32_t len, u_int32_t op);
void lu_xdr_free(lu_xdr_t *x);

int32_t lu_xdr_encode(lu_xdr_t *x);
int32_t lu_xdr_decode(lu_xdr_t *x);

u_int32_t lu_xdr_getpos(lu_xdr_t *x);
int32_t lu_xdr_setpos(lu_xdr_t *x, u_int32_t p);

int32_t lu_xdr_u_int_32(lu_xdr_t *x, u_int32_t *i);
int32_t lu_xdr_int_32(lu_xdr_t *x, int32_t *i);
int32_t lu_xdr_buffer(lu_xdr_t *x, char **s, u_int32_t *l);
int32_t lu_xdr_string(lu_xdr_t *x, char **s);
