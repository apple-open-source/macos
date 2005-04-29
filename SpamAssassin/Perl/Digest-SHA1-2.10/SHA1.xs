/* $Id: SHA1.xs,v 1.1 2004/04/09 17:04:44 dasenbro Exp $ */

#ifdef __cplusplus
extern "C" {
#endif
#define PERL_NO_GET_CONTEXT     /* we want efficiency */
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#ifdef __cplusplus
}
#endif

#ifndef PERL_VERSION
#    include <patchlevel.h>
#    if !(defined(PERL_VERSION) || (SUBVERSION > 0 && defined(PATCHLEVEL)))
#        include <could_not_find_Perl_patchlevel.h>
#    endif
#    define PERL_REVISION       5
#    define PERL_VERSION        PATCHLEVEL
#    define PERL_SUBVERSION     SUBVERSION
#endif

#if PERL_VERSION <= 4 && !defined(PL_dowarn)
   #define PL_dowarn dowarn
#endif

#ifdef G_WARN_ON
   #define DOWARN (PL_dowarn & G_WARN_ON)
#else
   #define DOWARN PL_dowarn
#endif

#ifdef SvPVbyte
   #if PERL_REVISION == 5 && PERL_VERSION < 7
       /* SvPVbyte does not work in perl-5.6.1, borrowed version for 5.7.3 */
       #undef SvPVbyte
       #define SvPVbyte(sv, lp) \
          ((SvFLAGS(sv) & (SVf_POK|SVf_UTF8)) == (SVf_POK) \
           ? ((lp = SvCUR(sv)), SvPVX(sv)) : my_sv_2pvbyte(aTHX_ sv, &lp))

       static char *
       my_sv_2pvbyte(pTHX_ register SV *sv, STRLEN *lp)
       {
           sv_utf8_downgrade(sv,0);
           return SvPV(sv,*lp);
       }
   #endif
#else
   #define SvPVbyte SvPV
#endif

#ifndef dTHX
   #define pTHX_
   #define aTHX_
#endif

/* NIST Secure Hash Algorithm */
/* heavily modified by Uwe Hollerbach <uh@alumni.caltech edu> */
/* from Peter C. Gutmann's implementation as found in */
/* Applied Cryptography by Bruce Schneier */
/* Further modifications to include the "UNRAVEL" stuff, below */

/* This code is in the public domain */

/* Useful defines & typedefs */

#if defined(U64TYPE) && (defined(USE_64_BIT_INT) || ((BYTEORDER != 0x1234) && (BYTEORDER != 0x4321)))
typedef U64TYPE ULONG;
# if BYTEORDER == 0x1234
#   undef BYTEORDER
#   define BYTEORDER 0x12345678
# elif BYTEORDER == 0x4321
#   undef BYTEORDER
#   define BYTEORDER 0x87654321   
# endif
#else
typedef unsigned long ULONG;     /* 32-or-more-bit quantity */
#endif

#define SHA_BLOCKSIZE		64
#define SHA_DIGESTSIZE		20

typedef struct {
    ULONG digest[5];		/* message digest */
    ULONG count_lo, count_hi;	/* 64-bit bit count */
    U8 data[SHA_BLOCKSIZE];	/* SHA data buffer */
    int local;			/* unprocessed amount in data */
} SHA_INFO;


/* UNRAVEL should be fastest & biggest */
/* UNROLL_LOOPS should be just as big, but slightly slower */
/* both undefined should be smallest and slowest */

#define SHA_VERSION 1
#define UNRAVEL
/* #define UNROLL_LOOPS */

/* SHA f()-functions */
#define f1(x,y,z)	((x & y) | (~x & z))
#define f2(x,y,z)	(x ^ y ^ z)
#define f3(x,y,z)	((x & y) | (x & z) | (y & z))
#define f4(x,y,z)	(x ^ y ^ z)

/* SHA constants */
#define CONST1		0x5a827999L
#define CONST2		0x6ed9eba1L
#define CONST3		0x8f1bbcdcL
#define CONST4		0xca62c1d6L

/* truncate to 32 bits -- should be a null op on 32-bit machines */
#define T32(x)	((x) & 0xffffffffL)

/* 32-bit rotate */
#define R32(x,n)	T32(((x << n) | (x >> (32 - n))))

/* the generic case, for when the overall rotation is not unraveled */
#define FG(n)	\
    T = T32(R32(A,5) + f##n(B,C,D) + E + *WP++ + CONST##n);	\
    E = D; D = C; C = R32(B,30); B = A; A = T

/* specific cases, for when the overall rotation is unraveled */
#define FA(n)	\
    T = T32(R32(A,5) + f##n(B,C,D) + E + *WP++ + CONST##n); B = R32(B,30)

#define FB(n)	\
    E = T32(R32(T,5) + f##n(A,B,C) + D + *WP++ + CONST##n); A = R32(A,30)

#define FC(n)	\
    D = T32(R32(E,5) + f##n(T,A,B) + C + *WP++ + CONST##n); T = R32(T,30)

#define FD(n)	\
    C = T32(R32(D,5) + f##n(E,T,A) + B + *WP++ + CONST##n); E = R32(E,30)

#define FE(n)	\
    B = T32(R32(C,5) + f##n(D,E,T) + A + *WP++ + CONST##n); D = R32(D,30)

#define FT(n)	\
    A = T32(R32(B,5) + f##n(C,D,E) + T + *WP++ + CONST##n); C = R32(C,30)


static void sha_transform(SHA_INFO *sha_info)
{
    int i;
    U8 *dp;
    ULONG T, A, B, C, D, E, W[80], *WP;

    dp = sha_info->data;

/*
the following makes sure that at least one code block below is
traversed or an error is reported, without the necessity for nested
preprocessor if/else/endif blocks, which are a great pain in the
nether regions of the anatomy...
*/
#undef SWAP_DONE

#if BYTEORDER == 0x1234
#define SWAP_DONE
    /* assert(sizeof(ULONG) == 4); */
    for (i = 0; i < 16; ++i) {
	T = *((ULONG *) dp);
	dp += 4;
	W[i] =  ((T << 24) & 0xff000000) | ((T <<  8) & 0x00ff0000) |
		((T >>  8) & 0x0000ff00) | ((T >> 24) & 0x000000ff);
    }
#endif

#if BYTEORDER == 0x4321
#define SWAP_DONE
    /* assert(sizeof(ULONG) == 4); */
    for (i = 0; i < 16; ++i) {
	T = *((ULONG *) dp);
	dp += 4;
	W[i] = T32(T);
    }
#endif

#if BYTEORDER == 0x12345678
#define SWAP_DONE
    /* assert(sizeof(ULONG) == 8); */
    for (i = 0; i < 16; i += 2) {
	T = *((ULONG *) dp);
	dp += 8;
	W[i] =  ((T << 24) & 0xff000000) | ((T <<  8) & 0x00ff0000) |
		((T >>  8) & 0x0000ff00) | ((T >> 24) & 0x000000ff);
	T >>= 32;
	W[i+1] = ((T << 24) & 0xff000000) | ((T <<  8) & 0x00ff0000) |
		 ((T >>  8) & 0x0000ff00) | ((T >> 24) & 0x000000ff);
    }
#endif

#if BYTEORDER == 0x87654321
#define SWAP_DONE
    /* assert(sizeof(ULONG) == 8); */
    for (i = 0; i < 16; i += 2) {
	T = *((ULONG *) dp);
	dp += 8;
	W[i] = T32(T >> 32);
	W[i+1] = T32(T);
    }
#endif

#ifndef SWAP_DONE
#error Unknown byte order -- you need to add code here
#endif /* SWAP_DONE */

    for (i = 16; i < 80; ++i) {
	W[i] = W[i-3] ^ W[i-8] ^ W[i-14] ^ W[i-16];
#if (SHA_VERSION == 1)
	W[i] = R32(W[i], 1);
#endif /* SHA_VERSION */
    }
    A = sha_info->digest[0];
    B = sha_info->digest[1];
    C = sha_info->digest[2];
    D = sha_info->digest[3];
    E = sha_info->digest[4];
    WP = W;
#ifdef UNRAVEL
    FA(1); FB(1); FC(1); FD(1); FE(1); FT(1); FA(1); FB(1); FC(1); FD(1);
    FE(1); FT(1); FA(1); FB(1); FC(1); FD(1); FE(1); FT(1); FA(1); FB(1);
    FC(2); FD(2); FE(2); FT(2); FA(2); FB(2); FC(2); FD(2); FE(2); FT(2);
    FA(2); FB(2); FC(2); FD(2); FE(2); FT(2); FA(2); FB(2); FC(2); FD(2);
    FE(3); FT(3); FA(3); FB(3); FC(3); FD(3); FE(3); FT(3); FA(3); FB(3);
    FC(3); FD(3); FE(3); FT(3); FA(3); FB(3); FC(3); FD(3); FE(3); FT(3);
    FA(4); FB(4); FC(4); FD(4); FE(4); FT(4); FA(4); FB(4); FC(4); FD(4);
    FE(4); FT(4); FA(4); FB(4); FC(4); FD(4); FE(4); FT(4); FA(4); FB(4);
    sha_info->digest[0] = T32(sha_info->digest[0] + E);
    sha_info->digest[1] = T32(sha_info->digest[1] + T);
    sha_info->digest[2] = T32(sha_info->digest[2] + A);
    sha_info->digest[3] = T32(sha_info->digest[3] + B);
    sha_info->digest[4] = T32(sha_info->digest[4] + C);
#else /* !UNRAVEL */
#ifdef UNROLL_LOOPS
    FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1);
    FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1);
    FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2);
    FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2);
    FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3);
    FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3);
    FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4);
    FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4);
#else /* !UNROLL_LOOPS */
    for (i =  0; i < 20; ++i) { FG(1); }
    for (i = 20; i < 40; ++i) { FG(2); }
    for (i = 40; i < 60; ++i) { FG(3); }
    for (i = 60; i < 80; ++i) { FG(4); }
#endif /* !UNROLL_LOOPS */
    sha_info->digest[0] = T32(sha_info->digest[0] + A);
    sha_info->digest[1] = T32(sha_info->digest[1] + B);
    sha_info->digest[2] = T32(sha_info->digest[2] + C);
    sha_info->digest[3] = T32(sha_info->digest[3] + D);
    sha_info->digest[4] = T32(sha_info->digest[4] + E);
#endif /* !UNRAVEL */
}

/* initialize the SHA digest */

static void sha_init(SHA_INFO *sha_info)
{
    sha_info->digest[0] = 0x67452301L;
    sha_info->digest[1] = 0xefcdab89L;
    sha_info->digest[2] = 0x98badcfeL;
    sha_info->digest[3] = 0x10325476L;
    sha_info->digest[4] = 0xc3d2e1f0L;
    sha_info->count_lo = 0L;
    sha_info->count_hi = 0L;
    sha_info->local = 0;
}

/* update the SHA digest */

static void sha_update(SHA_INFO *sha_info, U8 *buffer, int count)
{
    int i;
    ULONG clo;

    clo = T32(sha_info->count_lo + ((ULONG) count << 3));
    if (clo < sha_info->count_lo) {
	++sha_info->count_hi;
    }
    sha_info->count_lo = clo;
    sha_info->count_hi += (ULONG) count >> 29;
    if (sha_info->local) {
	i = SHA_BLOCKSIZE - sha_info->local;
	if (i > count) {
	    i = count;
	}
	memcpy(((U8 *) sha_info->data) + sha_info->local, buffer, i);
	count -= i;
	buffer += i;
	sha_info->local += i;
	if (sha_info->local == SHA_BLOCKSIZE) {
	    sha_transform(sha_info);
	} else {
	    return;
	}
    }
    while (count >= SHA_BLOCKSIZE) {
	memcpy(sha_info->data, buffer, SHA_BLOCKSIZE);
	buffer += SHA_BLOCKSIZE;
	count -= SHA_BLOCKSIZE;
	sha_transform(sha_info);
    }
    memcpy(sha_info->data, buffer, count);
    sha_info->local = count;
}


static void sha_transform_and_copy(unsigned char digest[20], SHA_INFO *sha_info)
{
    sha_transform(sha_info);
    digest[ 0] = (unsigned char) ((sha_info->digest[0] >> 24) & 0xff);
    digest[ 1] = (unsigned char) ((sha_info->digest[0] >> 16) & 0xff);
    digest[ 2] = (unsigned char) ((sha_info->digest[0] >>  8) & 0xff);
    digest[ 3] = (unsigned char) ((sha_info->digest[0]      ) & 0xff);
    digest[ 4] = (unsigned char) ((sha_info->digest[1] >> 24) & 0xff);
    digest[ 5] = (unsigned char) ((sha_info->digest[1] >> 16) & 0xff);
    digest[ 6] = (unsigned char) ((sha_info->digest[1] >>  8) & 0xff);
    digest[ 7] = (unsigned char) ((sha_info->digest[1]      ) & 0xff);
    digest[ 8] = (unsigned char) ((sha_info->digest[2] >> 24) & 0xff);
    digest[ 9] = (unsigned char) ((sha_info->digest[2] >> 16) & 0xff);
    digest[10] = (unsigned char) ((sha_info->digest[2] >>  8) & 0xff);
    digest[11] = (unsigned char) ((sha_info->digest[2]      ) & 0xff);
    digest[12] = (unsigned char) ((sha_info->digest[3] >> 24) & 0xff);
    digest[13] = (unsigned char) ((sha_info->digest[3] >> 16) & 0xff);
    digest[14] = (unsigned char) ((sha_info->digest[3] >>  8) & 0xff);
    digest[15] = (unsigned char) ((sha_info->digest[3]      ) & 0xff);
    digest[16] = (unsigned char) ((sha_info->digest[4] >> 24) & 0xff);
    digest[17] = (unsigned char) ((sha_info->digest[4] >> 16) & 0xff);
    digest[18] = (unsigned char) ((sha_info->digest[4] >>  8) & 0xff);
    digest[19] = (unsigned char) ((sha_info->digest[4]      ) & 0xff);
}

/* finish computing the SHA digest */
static void sha_final(unsigned char digest[20], SHA_INFO *sha_info)
{
    int count;
    ULONG lo_bit_count, hi_bit_count;

    lo_bit_count = sha_info->count_lo;
    hi_bit_count = sha_info->count_hi;
    count = (int) ((lo_bit_count >> 3) & 0x3f);
    ((U8 *) sha_info->data)[count++] = 0x80;
    if (count > SHA_BLOCKSIZE - 8) {
	memset(((U8 *) sha_info->data) + count, 0, SHA_BLOCKSIZE - count);
	sha_transform(sha_info);
	memset((U8 *) sha_info->data, 0, SHA_BLOCKSIZE - 8);
    } else {
	memset(((U8 *) sha_info->data) + count, 0,
	    SHA_BLOCKSIZE - 8 - count);
    }
    sha_info->data[56] = (hi_bit_count >> 24) & 0xff;
    sha_info->data[57] = (hi_bit_count >> 16) & 0xff;
    sha_info->data[58] = (hi_bit_count >>  8) & 0xff;
    sha_info->data[59] = (hi_bit_count >>  0) & 0xff;
    sha_info->data[60] = (lo_bit_count >> 24) & 0xff;
    sha_info->data[61] = (lo_bit_count >> 16) & 0xff;
    sha_info->data[62] = (lo_bit_count >>  8) & 0xff;
    sha_info->data[63] = (lo_bit_count >>  0) & 0xff;
    sha_transform_and_copy(digest, sha_info);
}





/*----------------------------------------------------------------*/
#ifndef INT2PTR
#define INT2PTR(any,d)	(any)(d)
#endif

static SHA_INFO* get_sha_info(pTHX_ SV* sv)
{
    if (sv_derived_from(sv, "Digest::SHA1"))
	return INT2PTR(SHA_INFO*, SvIV(SvRV(sv)));
    croak("Not a reference to a Digest::SHA1 object");
    return (SHA_INFO*)0; /* some compilers insist on a return value */
}


static char* hex_20(const unsigned char* from, char* to)
{
    static char *hexdigits = "0123456789abcdef";
    const unsigned char *end = from + 20;
    char *d = to;

    while (from < end) {
	*d++ = hexdigits[(*from >> 4)];
	*d++ = hexdigits[(*from & 0x0F)];
	from++;
    }
    *d = '\0';
    return to;
}

static char* base64_20(const unsigned char* from, char* to)
{
    static char* base64 =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const unsigned char *end = from + 20;
    unsigned char c1, c2, c3;
    char *d = to;

    while (1) {
	c1 = *from++;
	c2 = *from++;
	*d++ = base64[c1>>2];
	*d++ = base64[((c1 & 0x3) << 4) | ((c2 & 0xF0) >> 4)];
	if (from == end) {
	    *d++ = base64[(c2 & 0xF) << 2];
	    break;
	}
	c3 = *from++;
	*d++ = base64[((c2 & 0xF) << 2) | ((c3 & 0xC0) >>6)];
	*d++ = base64[c3 & 0x3F];
    }
    *d = '\0';
    return to;
}

/* Formats */
#define F_BIN 0
#define F_HEX 1
#define F_B64 2

static SV* make_mortal_sv(pTHX_ const unsigned char *src, int type)
{
    STRLEN len;
    char result[41];
    char *ret;
    
    switch (type) {
    case F_BIN:
	ret = (char*)src;
	len = 20;
	break;
    case F_HEX:
	ret = hex_20(src, result);
	len = 40;
	break;
    case F_B64:
	ret = base64_20(src, result);
	len = 27;
	break;
    default:
	croak("Bad convertion type (%d)", type);
	break;
    }
    return sv_2mortal(newSVpv(ret,len));
}


/********************************************************************/

typedef PerlIO* InputStream;

MODULE = Digest::SHA1		PACKAGE = Digest::SHA1

PROTOTYPES: DISABLE

void
new(xclass)
	SV* xclass
    PREINIT:
	SHA_INFO* context;
    PPCODE:
	if (!SvROK(xclass)) {
	    STRLEN my_na;
	    char *sclass = SvPV(xclass, my_na);
	    New(55, context, 1, SHA_INFO);
	    ST(0) = sv_newmortal();
	    sv_setref_pv(ST(0), sclass, (void*)context);
	    SvREADONLY_on(SvRV(ST(0)));
	} else {
	    context = get_sha_info(aTHX_ xclass);
	}
	sha_init(context);
	XSRETURN(1);

void
clone(self)
        SV* self
    PREINIT:
        SHA_INFO* cont = get_sha_info(aTHX_ self);
        char *myname = sv_reftype(SvRV(self),TRUE);
        SHA_INFO* context;
    PPCODE:
        STRLEN my_na;
        New(55, context, 1, SHA_INFO);
        ST(0) = sv_newmortal();
        sv_setref_pv(ST(0), myname , (void*)context);
        SvREADONLY_on(SvRV(ST(0)));
        memcpy(context,cont,sizeof(SHA_INFO));
        XSRETURN(1);

void
DESTROY(context)
	SHA_INFO* context
    CODE:
        Safefree(context);

void
add(self, ...)
	SV* self
    PREINIT:
	SHA_INFO* context = get_sha_info(aTHX_ self);
	int i;
	unsigned char *data;
	STRLEN len;
    PPCODE:
	for (i = 1; i < items; i++) {
	    data = (unsigned char *)(SvPVbyte(ST(i), len));
	    sha_update(context, data, len);
	}
	XSRETURN(1);  /* self */

void
addfile(self, fh)
	SV* self
	InputStream fh
    PREINIT:
	SHA_INFO* context = get_sha_info(aTHX_ self);
	unsigned char buffer[4096];
	int  n;
    CODE:
        if (fh) {
	    /* Process blocks until EOF or error */
            while ( (n = PerlIO_read(fh, buffer, sizeof(buffer))) > 0) {
		sha_update(context, buffer, n);
	    }
	    if (PerlIO_error(fh)) {
		croak("Reading from filehandle failed");
	    }
        }
        else {
	    croak("No filehandle passed");
        }
 
	XSRETURN(1);  /* self */

void
digest(context)
	SHA_INFO* context
    ALIAS:
	Digest::SHA1::digest    = F_BIN
	Digest::SHA1::hexdigest = F_HEX
	Digest::SHA1::b64digest = F_B64
    PREINIT:
	unsigned char digeststr[20];
    PPCODE:
        sha_final(digeststr, context);
	sha_init(context);  /* In case it is reused */
        ST(0) = make_mortal_sv(aTHX_ digeststr, ix);
        XSRETURN(1);

void
sha1(...)
    ALIAS:
	Digest::SHA1::sha1        = F_BIN
	Digest::SHA1::sha1_hex    = F_HEX
	Digest::SHA1::sha1_base64 = F_B64
    PREINIT:
	SHA_INFO ctx;
	int i;
	unsigned char *data;
        STRLEN len;
	unsigned char digeststr[20];
    PPCODE:
	sha_init(&ctx);

	if (DOWARN) {
            char *msg = 0;
	    if (items == 1) {
		if (SvROK(ST(0))) {
                    SV* sv = SvRV(ST(0));
		    if (SvOBJECT(sv) && strEQ(HvNAME(SvSTASH(sv)), "Digest::SHA1"))
		        msg = "probably called as method";
		    else
			msg = "called with reference argument";
		}
	    }
	    else if (items > 1) {
		data = (unsigned char *)SvPVbyte(ST(0), len);
		if (len == 12 && memEQ("Digest::SHA1", data, 12)) {
		    msg = "probably called as class method";
		}
	    }
	    if (msg) {
		char *f = (ix == F_BIN) ? "sha1" :
                          (ix == F_HEX) ? "sha1_hex" : "sha1_base64";
	        warn("&Digest::SHA1::%s function %s", f, msg);
	    }
	}

	for (i = 0; i < items; i++) {
	    data = (unsigned char *)(SvPVbyte(ST(i), len));
	    sha_update(&ctx, data, len);
	}
	sha_final(digeststr, &ctx);
        ST(0) = make_mortal_sv(aTHX_ digeststr, ix);
        XSRETURN(1);

void
sha1_transform(data)
	SV* data
    PREINIT:
        SHA_INFO ctx;
        int i;
        unsigned char *data_pv;
        unsigned char test[64];
        STRLEN len;
        unsigned char digeststr[20];
    PPCODE:
        sha_init(&ctx);
        
        memset (test, 0, 64);
        data_pv = (unsigned char *)(SvPVbyte(data, len));
        memcpy (test, data_pv, len);
	memcpy ((&ctx)->data, test, 64);
        sha_transform_and_copy(digeststr, &ctx);
        ST(0) = newSVpv((char*)digeststr, 20);
        XSRETURN(1);

