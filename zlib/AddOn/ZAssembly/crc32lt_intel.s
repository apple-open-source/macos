#if defined(VEC_OPTIMIZE) && defined(__x86_64__)
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
	.align 4,0x90
.globl _crc32_little_aligned_vector
_crc32_little_aligned_vector:

	// input :
	//		 crc : edi
	//		 buf : rsi
	// 		 len : rdx

	// symbolizing x86_64 registers

	#define	crc		%edi
	#define	buf		%rsi
	#define	len		%rdx
	#define	tab		%rcx

	#define	v0		%xmm0
	#define	v1		%xmm1
	#define	v2		%xmm2
	#define	v3		%xmm3
	#define	v4		%xmm4
	#define	v5		%xmm5

	// push rbp, sp should now be 16-byte aligned
	pushq	%rbp
	movq	%rsp, %rbp

#ifdef	KERNEL
	/* 
		allocate 6*16 = 96 stack space and save %xmm0-%xmm7
	*/
	subq	$96, %rsp
	movaps	v0, -16(%rbp)
	movaps	v1, -32(%rbp)
	movaps	v2, -48(%rbp)
	movaps	v3, -64(%rbp)
	movaps	v4, -80(%rbp)
	movaps	v5, -96(%rbp)
#endif

	/*
		set up the table pointer and use 16-byte data directly in pclmulqdq
		tried movaps to %xmm7, and use %xmm7, performance about the same
	*/
	leaq    L_coefficients(%rip), tab
	#define	K12		(tab)
	#define	K34		16(tab)
	#define	K56		32(tab)
	#define	uPx		48(tab)

	/* load the initial crc and xor with the 1st 16-byte vector */
	movd	crc, v0
	pxor	(buf), v0

	/* if this is the only vector, we've achieve the final 128-bit vector */ 
	add		$16, buf
	sub		$16, len
	jle		L_128bits

	/* make sure there are at least 3 more vectors */
	cmp		$48, len
	jl		L_no_more_4_vectors

	/* read the next 3 vectors*/
	movdqa	(buf), v1	
	movdqa	16(buf), v2	
	movdqa	32(buf), v3	
	add		$48, buf

	/* pre-decrement len by 64, to check whether there are at least 4 more vectors */
	sub		$48+64, len
	jl		L_foldv13

	/*	-------------------------------------------------
		the main loop, folding 4 vectors per iterations
		------------------------------------------------- 
	*/
L_FOLD_BY_4:	

/*
	.macro	FOLD4
	movdqa		$0, v4					// a copy of H(x)x^64 + L(x)
	pclmulqdq	$$0x00, K12, $0			// H(x) * {x^[512+64] mod P(x)}
	pclmulqdq	$$0x11, K12, v4			// L(x) * {x^[512] mod P(x)}
	pxor		$1, $0					// H(x) * {x^[512+64] mod P(x)} xor with the new vector with offset of 512 bits
	pxor		v4, $0					// xor with L(x) * {x^[512] mod P(x)}
	.endm

	FOLD4	v0, 0(buf)
	FOLD4	v1, 16(buf)
	FOLD4	v2, 32(buf)
	FOLD4	v3, 48(buf)

	the above code snippet is unfolded to save ~ 0.02 cycle/byte

*/

	movdqa		v0, v4
	movdqa		v1, v5
	pclmulqdq	$0x00, K12, v0
	pclmulqdq	$0x00, K12, v1
	pclmulqdq	$0x11, K12, v4
	pclmulqdq	$0x11, K12, v5
	pxor		0(buf), v0
	pxor		16(buf), v1
	pxor		v4, v0
	pxor		v5, v1
	movdqa		v2, v4
	movdqa		v3, v5
	pclmulqdq	$0x00, K12, v2
	pclmulqdq	$0x00, K12, v3
	pclmulqdq	$0x11, K12, v4
	pclmulqdq	$0x11, K12, v5
	pxor		32(buf), v2
	pxor		48(buf), v3
	pxor		v4, v2
	pxor		v5, v3

	add			$64, buf
	sub     	$64, len
	ja			L_FOLD_BY_4


	/*
		now sequentially fold v0 into v1,v2,v3
	*/
L_foldv13:

	.macro	FOLD1
	movdqa		v0, v4				// a copy of v0 = H(x)x^64 + L(x)
	pclmulqdq	$$0x00, K34, v0		// H(x) * {x^[128+64] mod P(x)}
	pclmulqdq	$$0x11, K34, v4		// L(x) * {x^128 mod P(x)}
	pxor		$0, v0				// H(x) * {x^[128+64] mod P(x)} xor with the new vector v1/v2/v3
	pxor		v4, v0				// xor with L(x) * {x^128 mod P(x)}
	.endm

	/* FOLD1 of v1-v3 into v0 */
	FOLD1	v1
	FOLD1	v2
	FOLD1	v3

	/* post-increment len by 64 */
	add		$64, len

L_no_more_4_vectors:

	/* pre-decrement len by 16 to detect whether there is still some vector to process */
	sub			$16, len
	jl			L_128bits
L_FOLD_BY_1:	
	FOLD1		(buf)			/* folding into the new vector */
	add			$16, buf
	sub			$16, len
	jae			L_FOLD_BY_1		/* until no more new vector */

L_128bits:		/* we've arrived at the final 128-bit vector */

	/* reduction from 128-bits to 64-bits */
	movdqa		v0, v1
	pclmulqdq	$0x00, K56, v1		// v1 = H(x) * K5 96-bits
	psrldq		$8, v0				// v0 = L(x) 64-bits
	pxor		v2, v2
	pxor		v1, v0
	movss		v0, v2
	psrldq		$4, v0				// low 64-bit
	pclmulqdq	$0x10, K56, v2
	pxor		v2, v0

	/* 
		barrett reduction:

			T1 = floor(R(x)/x^32) * [1/P(x)];	R/P
			T2 = floor(T1/x^32) * P(x);			int(R/P)*P;
			CRC = (R+int(R/P)*P) mod x^32;		R-int(R/P)*P
			
	*/
	pxor		v1, v1
	pxor		v2, v2
	movss		v0, v1
	pclmulqdq	$0x00, uPx, v1		// T1 = floor(R/x^32)*u
	movss		v1, v2
	pclmulqdq	$0x10, uPx, v2		// T2 = floor(T1/x^32)*P
	pxor		v2, v0
	psrldq		$4, v0				// low 64-bit
	movd		v0, %eax


#ifdef	KERNEL
	// restore xmm0-xmm7, and deallocate 96 bytes from stack 
	movaps	-16(%rbp), v0
	movaps	-32(%rbp), v1
	movaps	-48(%rbp), v2
	movaps	-64(%rbp), v3
	movaps	-80(%rbp), v4
	movaps	-96(%rbp), v5
	addq	$96, %rsp
#endif

	leave
	ret

	.const
	.align	4
L_coefficients: // used for vectorizing crc32 computation using pclmulqdq 

#define	K1	0x154442bd4
#define	K2	0x1c6e41596
#define	K3	0x1751997d0
#define	K4	0x0ccaa009e
#define	K5	0x0ccaa009e		// x^96 mod	P(x)
#define	K6	0x163cd6124		// x^64 mod P(x)
#define	ux	0x1F7011641
#define	Px	0x1DB710641

	.quad	K1
	.quad	K2
	.quad	K3
	.quad	K4
	.quad	K5
	.quad	K6
	.quad	ux
	.quad	Px	


#endif // VEC_OPTIMIZE && __x86_64__
