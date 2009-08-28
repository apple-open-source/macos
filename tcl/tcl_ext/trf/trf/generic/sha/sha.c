/* NIST Secure Hash Algorithm */
/* heavily modified by Uwe Hollerbach uh@alumni.caltech edu */
/* from Peter C. Gutmann's implementation as found in */
/* Applied Cryptography by Bruce Schneier */

/* NIST's proposed modification to SHA of 7/11/94 may be */
/* activated by defining USE_MODIFIED_SHA */

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#else
#include "../../compat/stdlib.h"
#endif

#include <stdio.h>
#include <string.h>
#include "sha.h"

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

/* 32-bit rotate */

#define ROT32(x,n)	((x << n) | (x >> (32 - n)))

#define FUNC(n,i)						\
    temp = ROT32(A,5) + f##n(B,C,D) + E + W[i] + CONST##n;	\
    E = D; D = C; C = ROT32(B,30); B = A; A = temp

#define FUNC1(i)						\
    temp = ROT32(A,5) + f1(B,C,D) + E + W[i] + CONST1;	\
    E = D; D = C; C = ROT32(B,30); B = A; A = temp
#define FUNC2(i)						\
    temp = ROT32(A,5) + f2(B,C,D) + E + W[i] + CONST2;	\
    E = D; D = C; C = ROT32(B,30); B = A; A = temp
#define FUNC3(i)						\
    temp = ROT32(A,5) + f3(B,C,D) + E + W[i] + CONST3;	\
    E = D; D = C; C = ROT32(B,30); B = A; A = temp
#define FUNC4(i)						\
    temp = ROT32(A,5) + f4(B,C,D) + E + W[i] + CONST4;	\
    E = D; D = C; C = ROT32(B,30); B = A; A = temp

/* do SHA transformation */

static void sha_transform(sha_info)
SHA_INFO *sha_info;
{
    int i;
    UINT32 temp, A, B, C, D, E, W[80];

    for (i = 0; i < 16; ++i) {
	W[i] = sha_info->data[i];
    }
    for (i = 16; i < 80; ++i) {
	W[i] = W[i-3] ^ W[i-8] ^ W[i-14] ^ W[i-16];
#ifdef USE_MODIFIED_SHA
	W[i] = ROT32(W[i], 1);
#endif /* USE_MODIFIED_SHA */
    }
    A = sha_info->digest[0];
    B = sha_info->digest[1];
    C = sha_info->digest[2];
    D = sha_info->digest[3];
    E = sha_info->digest[4];
#ifdef UNROLL_LOOPS
    FUNC1( 0);  FUNC1( 1);  FUNC1( 2);  FUNC1( 3);  FUNC1( 4);
    FUNC1( 5);  FUNC1( 6);  FUNC1( 7);  FUNC1( 8);  FUNC1( 9);
    FUNC1(10);  FUNC1(11);  FUNC1(12);  FUNC1(13);  FUNC1(14);
    FUNC1(15);  FUNC1(16);  FUNC1(17);  FUNC1(18);  FUNC1(19);

    FUNC2(20);  FUNC2(21);  FUNC2(22);  FUNC2(23);  FUNC2(24);
    FUNC2(25);  FUNC2(26);  FUNC2(27);  FUNC2(28);  FUNC2(29);
    FUNC2(30);  FUNC2(31);  FUNC2(32);  FUNC2(33);  FUNC2(34);
    FUNC2(35);  FUNC2(36);  FUNC2(37);  FUNC2(38);  FUNC2(39);

    FUNC3(40);  FUNC3(41);  FUNC3(42);  FUNC3(43);  FUNC3(44);
    FUNC3(45);  FUNC3(46);  FUNC3(47);  FUNC3(48);  FUNC3(49);
    FUNC3(50);  FUNC3(51);  FUNC3(52);  FUNC3(53);  FUNC3(54);
    FUNC3(55);  FUNC3(56);  FUNC3(57);  FUNC3(58);  FUNC3(59);

    FUNC4(60);  FUNC4(61);  FUNC4(62);  FUNC4(63);  FUNC4(64);
    FUNC4(65);  FUNC4(66);  FUNC4(67);  FUNC4(68);  FUNC4(69);
    FUNC4(70);  FUNC4(71);  FUNC4(72);  FUNC4(73);  FUNC4(74);
    FUNC4(75);  FUNC4(76);  FUNC4(77);  FUNC4(78);  FUNC4(79);
#else /* !UNROLL_LOOPS */
    for (i = 0; i < 20; ++i) {
	FUNC1(i);
    }
    for (i = 20; i < 40; ++i) {
	FUNC2(i);
    }
    for (i = 40; i < 60; ++i) {
	FUNC3(i);
    }
    for (i = 60; i < 80; ++i) {
	FUNC4(i);
    }
#endif /* !UNROLL_LOOPS */
    sha_info->digest[0] += A;
    sha_info->digest[1] += B;
    sha_info->digest[2] += C;
    sha_info->digest[3] += D;
    sha_info->digest[4] += E;
}

#ifdef LITTLE_ENDIAN

/* change endianness of data */

static void byte_reverse(buffer, count)
UINT32 *buffer; int count;
{
    int i;
    BYTE ct[4], *cp;

    count /= sizeof(UINT32);
    cp = (BYTE *) buffer;
    for (i = 0; i < count; ++i) {
	ct[0] = cp[0];
	ct[1] = cp[1];
	ct[2] = cp[2];
	ct[3] = cp[3];
	cp[0] = ct[3];
	cp[1] = ct[2];
	cp[2] = ct[1];
	cp[3] = ct[0];
	cp += sizeof(UINT32);
    }
}

#endif /* LITTLE_ENDIAN */

/* initialize the SHA digest */

void sha_init(sha_info)
SHA_INFO *sha_info;
{
    sha_info->digest[0] = 0x67452301L;
    sha_info->digest[1] = 0xefcdab89L;
    sha_info->digest[2] = 0x98badcfeL;
    sha_info->digest[3] = 0x10325476L;
    sha_info->digest[4] = 0xc3d2e1f0L;
    sha_info->count_lo = 0L;
    sha_info->count_hi = 0L;
}

/* update the SHA digest */

void sha_update(sha_info, buffer, count)
SHA_INFO *sha_info; BYTE *buffer; int count;
{
    if ((sha_info->count_lo + ((UINT32) count << 3)) < sha_info->count_lo) {
	++sha_info->count_hi;
    }
    sha_info->count_lo += (UINT32) count << 3;
    sha_info->count_hi += (UINT32) count >> 29;
    while (count >= SHA_BLOCKSIZE) {
	memcpy(sha_info->data, buffer, SHA_BLOCKSIZE);
#ifdef LITTLE_ENDIAN
	byte_reverse(sha_info->data, SHA_BLOCKSIZE);
#endif /* LITTLE_ENDIAN */
	sha_transform(sha_info);
	buffer += SHA_BLOCKSIZE;
	count -= SHA_BLOCKSIZE;
    }
    memcpy(sha_info->data, buffer, count);
}

/* finish computing the SHA digest */

void sha_final(sha_info)
SHA_INFO *sha_info;
{
    int count;
    UINT32 lo_bit_count, hi_bit_count;

    lo_bit_count = sha_info->count_lo;
    hi_bit_count = sha_info->count_hi;
    count = (int) ((lo_bit_count >> 3) & 0x3f);
    ((BYTE *) sha_info->data)[count++] = 0x80;
    if (count > 56) {
	memset((BYTE *) sha_info->data + count, 0, 64 - count);
#ifdef LITTLE_ENDIAN
	byte_reverse(sha_info->data, SHA_BLOCKSIZE);
#endif /* LITTLE_ENDIAN */
	sha_transform(sha_info);
	memset(sha_info->data, 0, 56);
    } else {
	memset((BYTE *) sha_info->data + count, 0, 56 - count);
    }
#ifdef LITTLE_ENDIAN
    byte_reverse(sha_info->data, SHA_BLOCKSIZE);
#endif /* LITTLE_ENDIAN */
    sha_info->data[14] = hi_bit_count;
    sha_info->data[15] = lo_bit_count;
    sha_transform(sha_info);
}

/* compute the SHA digest of a FILE stream */

#define BLOCK_SIZE	8192

void sha_stream(sha_info, fin)
SHA_INFO *sha_info; FILE *fin;
{
    int i;
    BYTE data[BLOCK_SIZE];

    sha_init(sha_info);
    while ((i = fread(data, 1, BLOCK_SIZE, fin)) > 0) {
	sha_update(sha_info, data, i);
    }
    sha_final(sha_info);
}

/* print a SHA digest */

void sha_print(sha_info)
SHA_INFO *sha_info;
{
    printf("%08lx %08lx %08lx %08lx %08lx\n",
	sha_info->digest[0], sha_info->digest[1], sha_info->digest[2],
	sha_info->digest[3], sha_info->digest[4]);
}
