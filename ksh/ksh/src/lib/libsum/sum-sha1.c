/***********************************************************************
*                                                                      *
*               This software is part of the ast package               *
*           Copyright (c) 1996-2007 AT&T Knowledge Ventures            *
*                      and is licensed under the                       *
*                  Common Public License, Version 1.0                  *
*                      by AT&T Knowledge Ventures                      *
*                                                                      *
*                A copy of the License is available at                 *
*            http://www.opensource.org/licenses/cpl1.0.txt             *
*         (with md5 checksum 059e8cd6165cb4c31e351f2b69388fd9)         *
*                                                                      *
*              Information and Software Systems Research               *
*                            AT&T Research                             *
*                           Florham Park NJ                            *
*                                                                      *
*                 Glenn Fowler <gsf@research.att.com>                  *
*                                                                      *
***********************************************************************/
#pragma prototyped

/*
 * FIPS 180-1 compliant SHA-1 implementation,
 * by Christophe Devine <devine@cr0.net>;
 * this program is licensed under the GPL.
 */

#define sha1_description "FIPS 180-1 SHA-1 secure hash algorithm 1."
#define sha1_options	"[+(version)?sha1 (FIPS 180-1) 1993-05-11]\
			 [+(author)?Christophe Devine <devine@cr0.net>]"
#define sha1_match	"sha1|SHA1|sha-1|SHA-1"
#define sha1_scale	0

#define uint8		uint8_t
#define uint32		uint32_t

#define sha1_padding	md5_pad

typedef struct Sha1_s
{
	_SUM_PUBLIC_
	_SUM_PRIVATE_
	uint32	total[2];
	uint32	state[5];
	uint8	buffer[64];
	uint8	digest[20];
	uint8	digest_sum[20];
} Sha1_t;

#define GET_UINT32(n,b,i)                                       \
{                                                               \
    (n) = (uint32) ((uint8 *) b)[(i)+3]                         \
      | (((uint32) ((uint8 *) b)[(i)+2]) <<  8)                 \
      | (((uint32) ((uint8 *) b)[(i)+1]) << 16)                 \
      | (((uint32) ((uint8 *) b)[(i)]  ) << 24);                \
}

#define PUT_UINT32(n,b,i)                                       \
{                                                               \
    (((uint8 *) b)[(i)+3]) = (uint8) (((n)      ) & 0xFF);      \
    (((uint8 *) b)[(i)+2]) = (uint8) (((n) >>  8) & 0xFF);      \
    (((uint8 *) b)[(i)+1]) = (uint8) (((n) >> 16) & 0xFF);      \
    (((uint8 *) b)[(i)]  ) = (uint8) (((n) >> 24) & 0xFF);      \
}

static void
sha1_process(Sha1_t* sha, uint8 data[64] )
{
    uint32 temp, A, B, C, D, E, W[16];

    GET_UINT32( W[0],  data,  0 );
    GET_UINT32( W[1],  data,  4 );
    GET_UINT32( W[2],  data,  8 );
    GET_UINT32( W[3],  data, 12 );
    GET_UINT32( W[4],  data, 16 );
    GET_UINT32( W[5],  data, 20 );
    GET_UINT32( W[6],  data, 24 );
    GET_UINT32( W[7],  data, 28 );
    GET_UINT32( W[8],  data, 32 );
    GET_UINT32( W[9],  data, 36 );
    GET_UINT32( W[10], data, 40 );
    GET_UINT32( W[11], data, 44 );
    GET_UINT32( W[12], data, 48 );
    GET_UINT32( W[13], data, 52 );
    GET_UINT32( W[14], data, 56 );
    GET_UINT32( W[15], data, 60 );

#define S(x,n) ((x << n) | ((x & 0xFFFFFFFF) >> (32 - n)))

#define R(t)                                            \
(                                                       \
    temp = W[(t -  3) & 0x0F] ^ W[(t - 8) & 0x0F] ^     \
           W[(t - 14) & 0x0F] ^ W[ t      & 0x0F],      \
    ( W[t & 0x0F] = S(temp,1) )                         \
)

#define P(a,b,c,d,e,x)                                  \
{                                                       \
    e += S(a,5) + F(b,c,d) + K + x; b = S(b,30);        \
}

    A = sha->state[0];
    B = sha->state[1];
    C = sha->state[2];
    D = sha->state[3];
    E = sha->state[4];

#undef	F

#define F(x,y,z) (z ^ (x & (y ^ z)))
#define K 0x5A827999

    P( A, B, C, D, E, W[0]  );
    P( E, A, B, C, D, W[1]  );
    P( D, E, A, B, C, W[2]  );
    P( C, D, E, A, B, W[3]  );
    P( B, C, D, E, A, W[4]  );
    P( A, B, C, D, E, W[5]  );
    P( E, A, B, C, D, W[6]  );
    P( D, E, A, B, C, W[7]  );
    P( C, D, E, A, B, W[8]  );
    P( B, C, D, E, A, W[9]  );
    P( A, B, C, D, E, W[10] );
    P( E, A, B, C, D, W[11] );
    P( D, E, A, B, C, W[12] );
    P( C, D, E, A, B, W[13] );
    P( B, C, D, E, A, W[14] );
    P( A, B, C, D, E, W[15] );
    P( E, A, B, C, D, R(16) );
    P( D, E, A, B, C, R(17) );
    P( C, D, E, A, B, R(18) );
    P( B, C, D, E, A, R(19) );

#undef K
#undef F

#define F(x,y,z) (x ^ y ^ z)
#define K 0x6ED9EBA1

    P( A, B, C, D, E, R(20) );
    P( E, A, B, C, D, R(21) );
    P( D, E, A, B, C, R(22) );
    P( C, D, E, A, B, R(23) );
    P( B, C, D, E, A, R(24) );
    P( A, B, C, D, E, R(25) );
    P( E, A, B, C, D, R(26) );
    P( D, E, A, B, C, R(27) );
    P( C, D, E, A, B, R(28) );
    P( B, C, D, E, A, R(29) );
    P( A, B, C, D, E, R(30) );
    P( E, A, B, C, D, R(31) );
    P( D, E, A, B, C, R(32) );
    P( C, D, E, A, B, R(33) );
    P( B, C, D, E, A, R(34) );
    P( A, B, C, D, E, R(35) );
    P( E, A, B, C, D, R(36) );
    P( D, E, A, B, C, R(37) );
    P( C, D, E, A, B, R(38) );
    P( B, C, D, E, A, R(39) );

#undef K
#undef F

#define F(x,y,z) ((x & y) | (z & (x | y)))
#define K 0x8F1BBCDC

    P( A, B, C, D, E, R(40) );
    P( E, A, B, C, D, R(41) );
    P( D, E, A, B, C, R(42) );
    P( C, D, E, A, B, R(43) );
    P( B, C, D, E, A, R(44) );
    P( A, B, C, D, E, R(45) );
    P( E, A, B, C, D, R(46) );
    P( D, E, A, B, C, R(47) );
    P( C, D, E, A, B, R(48) );
    P( B, C, D, E, A, R(49) );
    P( A, B, C, D, E, R(50) );
    P( E, A, B, C, D, R(51) );
    P( D, E, A, B, C, R(52) );
    P( C, D, E, A, B, R(53) );
    P( B, C, D, E, A, R(54) );
    P( A, B, C, D, E, R(55) );
    P( E, A, B, C, D, R(56) );
    P( D, E, A, B, C, R(57) );
    P( C, D, E, A, B, R(58) );
    P( B, C, D, E, A, R(59) );

#undef K
#undef F

#define F(x,y,z) (x ^ y ^ z)
#define K 0xCA62C1D6

    P( A, B, C, D, E, R(60) );
    P( E, A, B, C, D, R(61) );
    P( D, E, A, B, C, R(62) );
    P( C, D, E, A, B, R(63) );
    P( B, C, D, E, A, R(64) );
    P( A, B, C, D, E, R(65) );
    P( E, A, B, C, D, R(66) );
    P( D, E, A, B, C, R(67) );
    P( C, D, E, A, B, R(68) );
    P( B, C, D, E, A, R(69) );
    P( A, B, C, D, E, R(70) );
    P( E, A, B, C, D, R(71) );
    P( D, E, A, B, C, R(72) );
    P( C, D, E, A, B, R(73) );
    P( B, C, D, E, A, R(74) );
    P( A, B, C, D, E, R(75) );
    P( E, A, B, C, D, R(76) );
    P( D, E, A, B, C, R(77) );
    P( C, D, E, A, B, R(78) );
    P( B, C, D, E, A, R(79) );

#undef K
#undef F

    sha->state[0] += A;
    sha->state[1] += B;
    sha->state[2] += C;
    sha->state[3] += D;
    sha->state[4] += E;
}

static int
sha1_block(register Sum_t* p, const void* s, size_t length)
{
    Sha1_t*	sha = (Sha1_t*)p;
    uint8*	input = (uint8*)s;
    uint32	left, fill;

    if( ! length ) return 0;

    left = ( sha->total[0] >> 3 ) & 0x3F;
    fill = 64 - left;

    sha->total[0] += length <<  3;
    sha->total[1] += length >> 29;

    sha->total[0] &= 0xFFFFFFFF;
    sha->total[1] += sha->total[0] < length << 3;

    if( left && length >= fill )
    {
        memcpy( (void *) (sha->buffer + left), (void *) input, fill );
        sha1_process( sha, sha->buffer );
        length -= fill;
        input  += fill;
        left = 0;
    }

    while( length >= 64 )
    {
        sha1_process( sha, input );
        length -= 64;
        input  += 64;
    }

    if( length )
        memcpy( (void *) (sha->buffer + left), (void *) input, length );

    return 0;
}

static int
sha1_init(Sum_t* p)
{
	register Sha1_t*	sha = (Sha1_t*)p;

	sha->total[0] = sha->total[1] = 0;
	sha->state[0] = 0x67452301;
	sha->state[1] = 0xEFCDAB89;
	sha->state[2] = 0x98BADCFE;
	sha->state[3] = 0x10325476;
	sha->state[4] = 0xC3D2E1F0;

	return 0;
}

static Sum_t*
sha1_open(const Method_t* method, const char* name)
{
	Sha1_t*	sha;

	if (sha = newof(0, Sha1_t, 1, 0))
	{
		sha->method = (Method_t*)method;
		sha->name = name;
		sha1_init((Sum_t*)sha);
	}
	return (Sum_t*)sha;
}

static int
sha1_done(Sum_t* p)
{
    Sha1_t*	sha = (Sha1_t*)p;
    uint32	last, padn;
    uint8	msglen[8];

    PUT_UINT32( sha->total[1], msglen, 0 );
    PUT_UINT32( sha->total[0], msglen, 4 );

    last = ( sha->total[0] >> 3 ) & 0x3F;
    padn = ( last < 56 ) ? ( 56 - last ) : ( 120 - last );

    sha1_block( p, sha1_padding, padn );
    sha1_block( p, msglen, 8 );

    PUT_UINT32( sha->state[0], sha->digest,  0 );
    PUT_UINT32( sha->state[1], sha->digest,  4 );
    PUT_UINT32( sha->state[2], sha->digest,  8 );
    PUT_UINT32( sha->state[3], sha->digest, 12 );
    PUT_UINT32( sha->state[4], sha->digest, 16 );

    /* accumulate the digests */
    for (last = 0; last < elementsof(sha->digest); last++)
	sha->digest_sum[last] ^= sha->digest[last];
    return 0;
}

static int
sha1_print(Sum_t* p, Sfio_t* sp, register int flags)
{
	register Sha1_t*	sha = (Sha1_t*)p;
	register unsigned char*	d;
	register int		n;

	d = (flags & SUM_TOTAL) ? sha->digest_sum : sha->digest;
	for (n = 0; n < elementsof(sha->digest); n++)
		sfprintf(sp, "%02x", d[n]);
	return 0;
}

static int
sha1_data(Sum_t* p, Sumdata_t* data)
{
	register Sha1_t*	sha = (Sha1_t*)p;

	data->size = elementsof(sha->digest);
	data->num = 0;
	data->buf = sha->digest;
	return 0;
}
