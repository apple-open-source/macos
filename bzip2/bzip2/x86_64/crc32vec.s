#if defined __x86_64__

	.text
	.align 4,0x90
.globl _crc32_vec
_crc32_vec:

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
	#define	L_shufb	64(tab)

	/* load the initial crc and xor with the 1st 16-byte vector */
	movd	crc, v0
	movdqu	(buf), v1
	pslldq	$12, v0			// shift up to the most significant word in v0
	pshufb	L_shufb, v1	
	pxor	v1, v0

	/* if this is the only vector, we've achieve the final 128-bit vector */ 
	add		$16, buf
	sub		$16, len
	jle		L_128bits

	/* make sure there are at least 3 more vectors */
	cmp		$48, len
	jl		L_no_more_4_vectors

	/* read the next 3 vectors*/
	movdqu	(buf), v1	
	movdqu	16(buf), v2	
	movdqu	32(buf), v3	
	pshufb		L_shufb, v1
	pshufb		L_shufb, v2
	pshufb		L_shufb, v3
	
	add		$48, buf

	/* pre-decrement len by 64, to check whether there are at least 4 more vectors */
	sub		$48+64, len
	jl		L_foldv13

	/*	-------------------------------------------------
		the main loop, folding 4 vectors per iterations
		------------------------------------------------- 
	*/
L_FOLD_BY_4:	

	movdqa		v0, v4
	movdqa		v1, v5
	pclmulqdq	$0x11, K12, v0
	pclmulqdq	$0x11, K12, v1
	pclmulqdq	$0x00, K12, v4
	pclmulqdq	$0x00, K12, v5
	pxor		v4, v0
	pxor		v5, v1
	movdqu		0(buf), v4
	movdqu		16(buf), v5
	pshufb		L_shufb, v4
	pshufb		L_shufb, v5
	pxor		v4, v0
	pxor		v5, v1
	movdqa		v2, v4
	movdqa		v3, v5
	pclmulqdq	$0x11, K12, v2
	pclmulqdq	$0x11, K12, v3
	pclmulqdq	$0x00, K12, v4
	pclmulqdq	$0x00, K12, v5
	pxor		v4, v2
	pxor		v5, v3
	movdqu		32(buf), v4
	movdqu		48(buf), v5
	pshufb		L_shufb, v4
	pshufb		L_shufb, v5
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
	pclmulqdq	$$0x11, K34, v0		// H(x) * {x^[128+64] mod P(x)}
	pclmulqdq	$$0x00, K34, v4		// L(x) * {x^128 mod P(x)}
	pxor		v4, v0				// xor with L(x) * {x^128 mod P(x)}
	pxor		$0, v0				// H(x) * {x^[128+64] mod P(x)} xor with the new vector v1/v2/v3
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
	movdqu		(buf), v5
	pshufb		L_shufb, v5
	FOLD1		v5					/* folding into the new vector */
	add			$16, buf
	sub			$16, len
	jae			L_FOLD_BY_1		/* until no more new vector */

L_128bits:		/* we've arrived at the final 128-bit vector */

	/* reduction from 128-bits to 64-bits */
	movdqa		v0, v1
	pclmulqdq	$0x11, K56, v0		// v0 = H(x) * K5 96-bits
	pslldq		$8, v1				// v1 = L(x) 64-bits
	psrldq		$4, v1				// v1 = L(x) 64-bits in the right position
	pxor		v1, v0
	movdqa		v0, v1
	pclmulqdq	$0x01, K56, v1
	pxor		v1, v0

	/* 
		barrett reduction:

			T1 = floor(R(x)/x^32) * [1/P(x)];	R/P
			T2 = floor(T1/x^32) * P(x);			int(R/P)*P;
			CRC = (R+int(R/P)*P) mod x^32;		R-int(R/P)*P
			
	*/
	movq		v0, v1
	psrldq		$4, v1				// R/x^32
	pclmulqdq	$0x00, uPx, v1		// T1 = floor(R/x^32)*u
	psrldq		$4, v1				// T1/x^32
	pclmulqdq	$0x10, uPx, v1		// T2 = floor(T1/x^32)*P
	pxor		v1, v0
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
	.quad	0x08090a0b0c0d0e0f
	.quad	0x0001020304050607


#endif // defined VEC_OPTIMIZE
