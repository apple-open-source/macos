#if defined __arm64__
/*
	This x86_64 assembly code implements crc32_little_aligned_vector that computes the CRC32 of an initial crc 
	and a 16-byte aligned byte sequence. 

	uint32_t	crc32_little_aligned_vector(uint32_t crc, unsigned char *input, int	len);

	This function SHOULD NOT be called directly. It should be called in a wrapper
	function (such as crc32_little in crc32.c) that 1st align an input buffer to 16-byte (update crc along the way),
	and make sure that len is at least 16 and SHOULD be a multiple of 16.

	input :
		uint32_t	crc;		// input crc
		uint8_t		*input;		// input byte sequence, this one MUST be 16-byte aligned in the caller
		int			len;		// len MUST be no less than 16, and is a multiple of 16

	output :
		the final 32-bit crc

	The implementation here is based on an Intel white paper

	"White Paper: Fast CRC Computation for Generic Polynomials Using PCLMULQDQ Instruction"

	http://download.intel.com/design/intarch/papers/323102.pdf

	The implementation here implements the bit-reflected version of CRC32, 
	for which the generator polynomial is 0x1DB710641 (in bit-reflected manner).

	1. The input crc is xored with the 1st 4 bytes of the input word.

	2. let m(x) = D(x)*x^T + G(x), m(x) mod P(x) = [D(x) * (x^T mod P(x)) + G(x) ] mod P(x)
		this means a vector D(x) that is scaled up x^T can be multiplied with precomputed (x^T mod P(x)),
		and then be added to G(x). This will be congruent to the result m(x) mod P(x). This is called folding a
		vector.

		let there be at least 8 128-bit vectors v0, v1, v2, v3, v4, v5, v6, v7
		we can apply the folding to (v0,x4), (v1,v5), (v2,v6), (v3,v7), respectively, with T = 512
		for example, v4' = v0 * (x^512 mod P(x)) + v4;
		this effectively shortens the sequence by 4 vectors, producing a congruent sequence v4', v5', v6', v7'
		this is called folding by 4

	3. after len <= 7*16, we keep doing folding by 1 until we are down to 128-bit.
			v1' = v0 * (x^128 mod P(x)) + v1; 


		regarding the folding operation above, v0 is 128-bit and (x^T mod P(x)) is 32-bit.
		We can write v0 = H(x) ^ X^64 + L(x). and the 128-bit * 32-bit multiply  operation becomes
			H(x) * (x^(T+64) mod P(x)) + L(x) * (x^T mod P(x)

		we can then use pclmulqdq instruction twice and xor them to get the result of v0 * (x^T mod P(x)) 

	4 when we are down to 128-bit v0, we need to append 32-bit zero to the end to compute the final crc
		
		a. v0 = (H(x)*x^96 + L(x)*x^32, a P(x) congruent vector is v1 = H(x) * (x^96 mod P(x)) + L(x) * x^32, 
			which is 96-bit
		b. v1 = H'(x)*x^64 + L'(x), a P(x) congruen vector is v2 = H'(x) * (x^64 mod P(x)) + L'(x), which is 64-bit.
		c. use Barrett Reduction to divide 64-bit v2 by P(x) into the final 32-bit crc.
		 	  i. T1 = floor(v2/x^32) * u;     // u is actually 1/P(x)
			 ii. T2 = floor(T1/x^32) * P(x);
			iii. crc = (v2 + T2) mod x^32;
 
*/

	.text
	.align 2
.globl _crc32_vec
_crc32_vec:

	// input :
	//		 crc : w0
	//		 buf : x1
	// 		 len : w2

#if KERNEL
    sub     x3, sp, #12*16   
    sub     sp, sp, #12*16   
    st1.4s  {v0,v1,v2,v3},[x3],#4*16
    st1.4s  {v4,v5,v6,v7},[x3],#4*16
    st1.4s  {v16,v17,v18,v19},[x3],#4*16
#endif

	#define	crc		w0
	#define	buf		x1
	#define	len		w2
	#define	t		x3

	#define	K12		v16
	#define	K34		v17
	#define	K56		v18
	#define	uPx		v19

	ld1.4s	{v1}, [buf], #16					// 1st vector
	adrp	t, L_coefficients@PAGE
	eor.16b	v0, v0, v0
	rev64.16b	v1, v1
	add		t, t, L_coefficients@PAGEOFF		// t points to constants table
	ins		v0.s[3], crc						// v0 = initial crc
	ext.16b	v1, v1, v1, #8
	ld1.4s	{K12,K34,K56,uPx}, [t]				// store parameters in v16-v19
	eor.16b	v0, v0, v1							// v0 = initial crc xored with 1st vector	

	/* if only one vector, we are down to final 128-bit vector */
	subs	len, len, #16
	b.le 	L_128bits

	/* check if there are at least 3 more vectors */
	cmp		len, #48
	b.lt	L_no_more_4_vectors

	/* read the next 3 vectors */
	ld1.4s	{v1,v2,v3}, [buf], #48
	rev64.16b	v1, v1
	rev64.16b	v2, v2
	rev64.16b	v3, v3
	ext.16b	v1, v1, v1, #8
	ext.16b	v2, v2, v2, #8
	ext.16b	v3, v3, v3, #8

	/* adjust len (-48), and pre-decrement len by 64 to check whether there are at least 4 more vectors */
	subs	len, len, #(48+64)
	b.lt	L_foldv13

L_FOLD_BY_4:	

/*	
	.macro	FOLD4
	ld1.4s	{v4}, [buf], #16			// new vector	
	pmull.1q	q6, $0, K12				// H(x) * {x^[512+64] mod P(x)}
	pmull2.1q	$1, $0, K12				// L(x) * {x^[512] mod P(x)}
	eor.16b		v4, v4, v6				// H(x) * {x^[512+64] mod P(x)} xor with the new vector with offset of 512 bits
	eor.16b		$0, $0, v4				// xor with L(x) * {x^[512] mod P(x)}
	.endm

	FOLD4	v0, q0
	FOLD4	v1, q1
	FOLD4	v2, q2
	FOLD4	v3, q3

	the above code snippet is unrolled to give better performance 
*/

	ld1.4s	{v4,v5}, [buf], #32
	pmull.1q	v6, v0, K12
	pmull.1q	v7, v1, K12
	rev64.16b	v4, v4
	rev64.16b	v5, v5
	pmull2.1q	v0, v0, K12
	pmull2.1q	v1, v1, K12
	ext.16b	v4, v4, v4, #8
	ext.16b	v5, v5, v5, #8
	eor.16b		v4, v4, v6
	eor.16b		v5, v5, v7
	eor.16b		v0, v0, v4
	eor.16b		v1, v1, v5
	ld1.4s	{v4,v5}, [buf], #32
	pmull.1q	v6, v2, K12
	pmull.1q	v7, v3, K12
	rev64.16b	v4, v4
	rev64.16b	v5, v5
	pmull2.1q	v2, v2, K12
	pmull2.1q	v3, v3, K12
	ext.16b	v4, v4, v4, #8
	ext.16b	v5, v5, v5, #8
	eor.16b		v4, v4, v6
	eor.16b		v5, v5, v7
	eor.16b		v2, v2, v4
	eor.16b		v3, v3, v5

	/* decrement len by 64, repeat the loop if len>0 */
	subs     	len, len, #64
	b.gt		L_FOLD_BY_4


	.macro	FOLD1
	pmull.1q	v4, v0, K34			// H(x) * {x^[128+64] mod P(x)}
	pmull2.1q	v0, v0, K34			// L(x) * {x^128 mod P(x)}
	eor.16b		v4, v4, $0			// H(x) * {x^[128+64] mod P(x)} xor with the new vector
	eor.16b		v0, v0, v4			// xor with L(x) * {x^128 mod P(x)}
	.endm

	/* FOLD1 of v1-v3 into v0 */
L_foldv13:
	FOLD1	v1
	FOLD1	v2
	FOLD1	v3

	/* post-increment len by 64 */
	add		len, len, #64

L_no_more_4_vectors:

	/* pre-decrement len by 16 to detect whether there is still some vector to process */
	subs		len, len, #16
	b.lt 		L_128bits
L_FOLD_BY_1:	
	ld1.4s		{v1}, [buf], #16
	rev64.16b	v1, v1
	ext.16b	v1, v1, v1, #8
	FOLD1		v1				/* folding into the new vector */
	subs		len, len, #16
	b.ge		L_FOLD_BY_1		/* until there is no more vector */

L_128bits:		/* we've arrived at the final 128-bit vector */

    /* reduction from 128-bits to 64-bits */
	eor.16b		v2, v2, v2
	eor.16b		v3, v3, v3
	pmull2.1q	v1, v0, K56				// 96-bit K5*HIGH64 
	ins			v2.s[1], v0.s[0]
	ins			v2.s[2], v0.s[1]
	eor.16b		v0, v2, v1 
	ins			v3.s[0], v0.s[2]
	pmull.1q	v1, v3, K56				// 64-bit K6 * HIGH32
	eor.16b		v0, v0, v1 
	
	/* 
        barrett reduction:

            T1 = floor(R(x)/x^32) * [1/P(x)];   R/P
            T2 = floor(T1/x^32) * P(x);         int(R/P)*P;
            CRC = (R+int(R/P)*P) mod x^32;      R-int(R/P)*P
            
    */
	ins			v3.s[0], v0.s[1]
	pmull.1q	v1, v3, uPx
	ins			v3.s[2], v1.s[1]
	pmull2.1q	v1, v3, uPx
	eor.16b		v0, v0, v1
	fmov		w0, s0
	
#if KERNEL
    ld1.4s  {v0,v1,v2,v3},[sp],#4*16
    ld1.4s  {v4,v5,v6,v7},[sp],#4*16
    ld1.4s  {v16,v17,v18,v19},[sp],#4*16
#endif
	ret		lr

	.const
	.align	4
L_coefficients:	// used for vectorizing crc32 computation using pmull

#define	K1  0x8833794C	
#define	K2	0xE6228B11
#define	K3	0xC5B9CD4C
#define	K4	0xE8A45605
#define	K5	0xF200AA66
#define	K6	0x490D678D
#define	ux	0x104D101DF
#define	Px	0x104C11DB7

	.quad	K2
	.quad	K1
	.quad	K4
	.quad	K3
	.quad	K6
	.quad	K5
	.quad	ux
	.quad	Px	

#endif // defined VEC_OPTIMIZE
