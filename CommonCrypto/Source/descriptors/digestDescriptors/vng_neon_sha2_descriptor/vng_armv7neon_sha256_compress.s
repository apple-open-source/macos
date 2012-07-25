/*
	This file provides armv7+neon hand implementation of the following function

	void SHA256_Transform(SHA256_ctx *ctx, char *data, unsigned int num_blocks);

	which is a C function in sha2.c (from xnu).

	sha256 algorithm per block description:

		1. W(0:15) = big-endian (per 4 bytes) loading of input data (64 byte) 
		2. load 8 digests a-h from ctx->state
		3. for r = 0:15
				T1 = h + Sigma1(e) + Ch(e,f,g) + K[r] + W[r];
				d += T1;
				h = T1 + Sigma0(a) + Maj(a,b,c)
				permute a,b,c,d,e,f,g,h into h,a,b,c,d,e,f,g
		4. for r = 16:63
				W[r] = W[r-16] + sigma1(W[r-2]) + W[r-7] + sigma0(W[r-15]);
				T1 = h + Sigma1(e) + Ch(e,f,g) + K[r] + W[r];
				d += T1;
				h = T1 + Sigma0(a) + Maj(a,b,c)
				permute a,b,c,d,e,f,g,h into h,a,b,c,d,e,f,g
				
	In the assembly implementation:	
		- a circular window of message schedule W(r:r+15) is updated and stored in q0-q3
		- its corresponding W+K(r:r+15) is updated and stored in a stack space circular buffer
		- the 8 digests (a-h) will be stored in GPR or memory

	the implementation per block looks like

	----------------------------------------------------------------------------

	load W(0:15) (big-endian per 4 bytes) into q0:q3
	pre_calculate and store W+K(0:15) in stack

	load digests a-h from ctx->state;

	for (r=0;r<48;r+=4) {
		digests a-h update and permute round r:r+3
		update W([r:r+3]%16) and WK([r:r+3]%16) for the next 4th iteration 
	}

	for (r=48;r<64;r+=4) {
		digests a-h update and permute round r:r+3
	}

	ctx->states += digests a-h;

	----------------------------------------------------------------------------

	our implementation (allows multiple blocks per call) pipelines the loading of W/WK of a future block 
	into the last 16 rounds of its previous block:

	----------------------------------------------------------------------------

	load W(0:15) (big-endian per 4 bytes) into q0:q3 
	pre_calculate and store W+K(0:15) in stack

L_loop:

	load digests a-h from ctx->state;

	for (r=0;r<48;r+=4) {
		digests a-h update and permute round r:r+3
		update W([r:r+3]%16) and WK([r:r+3]%16) for the next 4th iteration 
	}

	num_block--;
	if (num_block==0)	jmp L_last_block;

	for (r=48;r<64;r+=4) {
		digests a-h update and permute round r:r+3
		load W([r:r+3]%16) (big-endian per 4 bytes) into q0:q3 
		pre_calculate and store W+K([r:r+3]%16) in stack
	}

	ctx->states += digests a-h;

	jmp	L_loop;

L_last_block:

	for (r=48;r<64;r+=4) {
		digests a-h update and permute round r:r+3
	}

	ctx->states += digests a-h;

	------------------------------------------------------------------------

	Apple CoreOS vector & numerics
	cclee 10-12-10
*/

#include <arm/arch.h>

#if defined(_ARM_ARCH_7)

	// associate variables with registers or memory

	#define	ctx			r0
	#define data		r1
	#define	num_blocks	[sp, #64]

	#define	a			r2
	#define	b			r3
	#define	c			r4
	#define	d			r5
	#define	e			r8
	#define	f			r9
	#define	g			r10
	#define	h			r11

	#define	K			r6

	// 2 local variables
	#define	t	r12
	#define	s	lr

	// a window (16 words) of message scheule
	#define	W0	q0
	#define	W1	q1
	#define	W2	q2
	#define	W3	q3
	#define	zero	q8

	// circular buffer for WK[(r:r+15)%16]
	#define WK(r)   [sp,#(r&15)*4]

// #define Ch(x,y,z)   (((x) & (y)) ^ ((~(x)) & (z)))

	.macro Ch
	mvn		t, $0		// ~x
	and		s, $0, $1	// (x) & (y)
	and		t, t, $2	// (~(x)) & (z)
	eor		t, t, s		// t = Ch(x,y,z) = (((x) & (y)) ^ ((~(x)) & (z)))
	.endm

// #define Maj(x,y,z)  (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

	.macro	Maj
	eor		t, $1, $2		// y^z
	and		s, $1, $2		// y&z
	and		t, t, $0		// x&(y^z)
	eor		t, t, s			// t = Maj(x,y,z) = (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z))) 
	.endm

// #define sigma0_256(x)   (S32(7,  (x)) ^ S32(18, (x)) ^ R(3 ,   (x)))

	// performs sigma0_256 on 4 words on a Q register
	// use q6/q7 as intermediate registers
	.macro	sigma0
	vshr.u32	q6, $0, #7
	vshl.i32	q7, $0, #14
	vshr.u32	$0, $0, #3
	veor		$0, q6
	veor		$0, q7
	vshr.u32	q6, #11
	vshl.i32	q7, #11
	veor		$0, q6
	veor		$0, q7
	.endm

// #define sigma1_256(x)   (S32(17, (x)) ^ S32(19, (x)) ^ R(10,   (x)))

	// performs sigma1_256 on 4 words on a Q register
	// use q6/q7 as intermediate registers
	.macro	sigma1
	vshr.u32	q6, $0, #17
	vshl.i32	q7, $0, #13
	vshr.u32	$0, $0, #10
	veor		$0, q6
	veor		$0, q7
	vshr.u32	q6, #2
	vshl.i32	q7, #2
	veor		$0, q6
	veor		$0, q7
	.endm

// #define Sigma0_256(x)   (S32(2,  (x)) ^ S32(13, (x)) ^ S32(22, (x)))

	.macro	Sigma0
	ror		t, $0, #2		// S32(2,  (x))
	ror		s, $0, #13		// S32(13,  (x))
	eor		t, t, s			// S32(2,  (x)) ^ S32(13, (x))
	ror		s, s, #9		// S32(22,  (x))
	eor		t, t, s			// t = (S32(2,  (x)) ^ S32(13, (x)) ^ S32(22, (x)))
	.endm

// #define Sigma1_256(x)   (S32(6,  (x)) ^ S32(11, (x)) ^ S32(25, (x)))

	.macro	Sigma1
	ror		t, $0, #6		// S32(6,  (x))
	ror		s, $0, #11		// S32(11, (x))
	eor		t, t, s			// S32(6,  (x)) ^ S32(11, (x))
	ror		s, s, #14		// S32(25, (x))	
	eor		t, t, s			// t = (S32(6,  (x)) ^ S32(11, (x)) ^ S32(25, (x)))
	.endm

	// per round digests update
	.macro	round
	// ror		t, $4, #6			// S32(6,  (x))
	eor		t, t, $4, ror #11	// S32(6,  (x)) ^ S32(11, (x))
	eor		t, t, $4, ror #25	// t = (S32(6,  (x)) ^ S32(11, (x)) ^ S32(25, (x)))
	and		s, $4, $5			// (x) & (y)
	add		$7, t				// use h to store h+Sigma1(e)
	bic		t, $6, $4			// (~(x)) & (z)
	eor		t, t, s				// t = Ch(x,y,z) = (((x) & (y)) ^ ((~(x)) & (z)))
	ldr		s, WK($8)			//
	add		$7, t				// t = h+Sigma1(e)+Ch(e,f,g);
	ror		t, $0, #2			// S32(2,  (x))
	add		$7, s				// h = T1
	eor		t, t, $0, ror #13	// S32(2,  (x)) ^ S32(13, (x))
	add		$3, $7				// d += T1;
	eor		t, t, $0, ror #22	// t = (S32(2,  (x)) ^ S32(13, (x)) ^ S32(22, (x)))				// t = Sigma0(a);
	add		$7, t				// h = T1 + Sigma0(a);
	eor		t, $1, $2			// y^z
	and		s, $1, $2			// y&z
	and		t, t, $0			// x&(y^z)
	eor		s, s, t				// t = Maj(x,y,z) = (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
	// add		$7, s				// h = T1 + Sigma0(a) + Maj(a,b,c);			
	.endm

	// per 4 rounds digests update and permutation
	// permutation is absorbed by rotating the roles of digests a-h
	.macro	rounds
	ror		t, $4, #6
	round	$0, $1, $2, $3, $4, $5, $6, $7, 0+$8
	ror		t, $3, #6
	add		$7, s
	round	$7, $0, $1, $2, $3, $4, $5, $6, 1+$8
	ror		t, $2, #6
	add		$6, s
	round	$6, $7, $0, $1, $2, $3, $4, $5, 2+$8
	ror		t, $1, #6
	add		$5, s
	round	$5, $6, $7, $0, $1, $2, $3, $4, 3+$8
	add		$4, s
	.endm

	.macro	rounds_a
	ror		t, e, #6
	round	a, b, c, d, e, f, g, h, 0+$0
	ror		t, d, #6
	add		h, s
	round	h, a, b, c, d, e, f, g, 1+$0
	ror		t, c, #6
	add		g, s
	round	g, h, a, b, c, d, e, f, 2+$0
	ror		t, b, #6
	add		f, s
	round	f, g, h, a, b, c, d, e, 3+$0
	add		e, s
	.endm

	.macro	rounds_e
	ror		t, a, #6
	round	e, f, g, h, a, b, c, d, 0+$0
	ror		t, h, #6
	add		d, s
	round	d, e, f, g, h, a, b, c, 1+$0
	ror		t, g, #6
	add		c, s
	round	c, d, e, f, g, h, a, b, 2+$0
	ror		t, f, #6
	add		b, s
	round	b, c, d, e, f, g, h, a, 3+$0
	add		a, s
	.endm

	// update the message schedule W and W+K (4 rounds) 16 rounds ahead in the future 
	.macro	message_schedule
	vld1.32	{q5},[K,:128]!
	vext.32 q4, $0, $1, #1			// Q4 = w4:w1
	sigma0	q4						// sigma0(w4:w1)
	vadd.s32	$0, q4				// w3:w0 + sigma0(w4:w1)
	vext.32	q6, $2, $3, #1			// Q6 = w12:w9
	vadd.s32	$0, q6				// w3:w0 + sigma0(w4:w1) + w12:w9
	vext.64	q4, $3, zero, #1		// 0 0 w15:w14
	sigma1	q4						// Q4 = sigma1(0 0 w15:w14)
	vadd.s32	$0, q4				// w3:w0 + sigma0(w4:w1) + w12:w9 + sigma1(0 0 w15:w14)
	vext.64	q4, zero, $0, #1		// Q4 = (w17:w16 0 0)
	sigma1	q4						// sigma1(w17:w16 0 0)
	vadd.s32	$0, q4				// w19:w16 = w3:w0 + sigma0(w4:w1) + w12:w9 + sigma1(w17:w14)
	add		t, sp, #(($4&15)*4)
	vadd.s32	q5, $0				// W+K
	vst1.32		{q5},[t,:128]
	.endm

	// this macro is used in the last 16 rounds of a current block
	// it reads the next message (16 4-byte words), load it into 4 words W[r:r+3], computes WK[r:r+3]
	// and save into stack to prepare for next block

	.macro	update_W_WK
	vld1.s32	{$1},[data]!
	vrev32.8	$1, $1
	add		t, sp, #($0*16)
	vld1.s32	{q4},[K,:128]!
	vadd.s32	q4, $1
	vst1.32		{q4},[t]
	.endm

	.macro	Update_Digits
	ldr		t, [ctx]
	ldr		s, [ctx,#4]
	add		a, t
	add		b, s
	strd	a, b, [ctx]

	ldr		t, [ctx,#8]
	ldr		s, [ctx,#12]
	add		c, t
	add		d, s
	strd	c, d, [ctx, #8]

	ldr		t, [ctx,#16]
	ldr		s, [ctx,#20]
	add		e, t
	add		f, s
	strd	e, f, [ctx, #16]

	ldr		t, [ctx,#24]
	ldr		s, [ctx,#28]
	add		g, t
	add		h, s
	strd	g, h, [ctx, #24]
	.endm

	.macro	rounds_a_schedule_update
	eor		t, e, e, ror #5	// S32(6,  (x)) ^ S32(11, (x))
	vld1.32	{q5},[K,:128]!
	eor		t, t, e, ror #19	// t = (S32(6,  (x)) ^ S32(11, (x)) ^ S32(25, (x)))
	vext.32 q4, $1, $2, #1			// Q4 = w4:w1
	and		s, e, f				// (x) & (y)
	add		h, t, ror #6				// use h to store h+Sigma1(e)
	bic		t, g, e				// (~(x)) & (z)
	vshr.u32	q6, q4, #7
	eor		t, t, s				// t = Ch(x,y,z) = (((x) & (y)) ^ ((~(x)) & (z)))
	vshl.i32	q7, q4, #14
	ldr		s, WK($0)			//
	add		h, t				// t = h+Sigma1(e)+Ch(e,f,g);
	eor		t, a, a, ror #11	// S32(2,  (x)) ^ S32(13, (x))
	add		h, s				// h = T1
	eor		t, t, a, ror #20	// t = (S32(2,  (x)) ^ S32(13, (x)) ^ S32(22, (x)))				// t = Sigma0(a);
	add		d, h				// d += T1;
	vshr.u32	q4, q4, #3
	add		h, t, ror #2				// h = T1 + Sigma0(a);
	eor		t, b, c			// y^z
	and		s, b, c			// y&z
	veor		q4, q6
	vshr.u32	q6, #11
	and		t, t, a			// x&(y^z)
	eor		s, s, t				// t = Maj(x,y,z) = (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
	eor		t, d, d, ror #5	// S32(6,  (x)) ^ S32(11, (x))
	veor		q4, q7
	vshl.i32	q7, #11


	add		h, s
	eor		t, t, d, ror #19	// t = (S32(6,  (x)) ^ S32(11, (x)) ^ S32(25, (x)))
	and		s, d, e				// (x) & (y)
	add		g, t, ror #6				// use h to store h+Sigma1(e)

	bic		t, f, d				// (~(x)) & (z)
	veor		q4, q6
	veor		q4, q7
	vext.32	q6, $3, $4, #1			// Q6 = w12:w9
	vadd.s32	$1, q4				// w3:w0 + sigma0(w4:w1)
	eor		t, t, s				// t = Ch(x,y,z) = (((x) & (y)) ^ ((~(x)) & (z)))
	ldr		s, WK(1+$0)			//
	add		g, t				// t = h+Sigma1(e)+Ch(e,f,g);
	eor		t, h, h, ror #11	// S32(2,  (x)) ^ S32(13, (x))
	add		g, s				// h = T1
	eor		t, t, h, ror #20	// t = (S32(2,  (x)) ^ S32(13, (x)) ^ S32(22, (x)))				// t = Sigma0(a);
	vadd.s32	$1, q6				// w3:w0 + sigma0(w4:w1) + w12:w9
	vext.64	q4, $4, zero, #1		// 0 0 w15:w14
	add		c, g				// d += T1;
	add		g, t, ror #2			// h = T1 + Sigma0(a);
	eor		t, a, b				// y^z
	and		s, a, b				// y&z
	and		t, t, h				// x&(y^z)
	vshr.u32	q6, q4, #17
	vshl.i32	q7, q4, #13
	vshr.u32	q4, q4, #10
	eor		s, s, t				// t = Maj(x,y,z) = (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))


	veor		q4, q6
	veor		q4, q7
	vshr.u32	q6, #2
	vshl.i32	q7, #2
	veor		q4, q6
	veor		q4, q7

	eor		t, c, c, ror #5	// S32(6,  (x)) ^ S32(11, (x))
	add		g, s
	eor		t, t, c, ror #19	// t = (S32(6,  (x)) ^ S32(11, (x)) ^ S32(25, (x)))
	vadd.s32	$1, q4				// w3:w0 + sigma0(w4:w1) + w12:w9 + sigma1(0 0 w15:w14)
	and		s, c, d				// (x) & (y)
	add		f, t, ror #6				// use h to store h+Sigma1(e)
	bic		t, e, c				// (~(x)) & (z)
	vext.64	q4, zero, $1, #1		// Q4 = (w17:w16 0 0)
	eor		t, t, s				// t = Ch(x,y,z) = (((x) & (y)) ^ ((~(x)) & (z)))
	ldr		s, WK(2+$0)			//
	add		f, t				// t = h+Sigma1(e)+Ch(e,f,g);
	vshr.u32	q6, q4, #17
	vshl.i32	q7, q4, #13
	vshr.u32	q4, q4, #10
	eor		t, g, g, ror #11	// S32(2,  (x)) ^ S32(13, (x))
	add		f, s				// h = T1
	eor		t, t, g, ror #20	// t = (S32(2,  (x)) ^ S32(13, (x)) ^ S32(22, (x)))				// t = Sigma0(a);
	add		b, f				// d += T1;
	veor		q4, q6
	veor		q4, q7
	add		f, t, ror #2			// h = T1 + Sigma0(a);
	eor		t, h, a				// y^z
	and		s, h, a				// y&z
	and		t, t, g				// x&(y^z)
	vshr.u32	q6, #2
	vshl.i32	q7, #2
	eor		s, s, t				// t = Maj(x,y,z) = (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

	eor		t, b, b, ror #5	// S32(6,  (x)) ^ S32(11, (x))
	add		f, s
	eor		t, t, b, ror #19	// t = (S32(6,  (x)) ^ S32(11, (x)) ^ S32(25, (x)))
	veor		q4, q6
	veor		q4, q7

	vadd.s32	$1, q4				// w19:w16 = w3:w0 + sigma0(w4:w1) + w12:w9 + sigma1(w17:w14)

	and		s, b, c				// (x) & (y)
	add		e, t, ror #6				// use h to store h+Sigma1(e)
	bic		t, d, b				// (~(x)) & (z)
	vadd.s32	q5, $1				// W+K
	eor		t, t, s				// t = Ch(x,y,z) = (((x) & (y)) ^ ((~(x)) & (z)))
	ldr		s, WK(3+$0)			//
	add		e, t				// t = h+Sigma1(e)+Ch(e,f,g);
	eor		t, f, f, ror #11	// S32(2,  (x)) ^ S32(13, (x))
	add		e, s				// h = T1
	eor		t, t, f, ror #20	// t = (S32(2,  (x)) ^ S32(13, (x)) ^ S32(22, (x)))				// t = Sigma0(a);
	add		a, e				// d += T1;
	add		e, t, ror #2				// h = T1 + Sigma0(a);
	eor		t, g, h				// y^z
	and		s, g, h				// y&z
	and		t, t, f				// x&(y^z)
	eor		s, s, t				// t = Maj(x,y,z) = (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

	add		t, sp, #(($0&15)*4)
	add		e, s
	vst1.32		{q5},[t,:128]

	.endm

	.macro	rounds_e_schedule_update
	eor		t, a, a, ror #5			// S32(6,  (x)) ^ S32(11, (x))
	vld1.32	{q5},[K,:128]!
	eor		t, t, a, ror #19	// t = (S32(6,  (x)) ^ S32(11, (x)) ^ S32(25, (x)))
	vext.32 q4, $1, $2, #1			// Q4 = w4:w1
	and		s, a, b				// (x) & (y)
	add		d, t, ror #6				// use h to store h+Sigma1(e)
	bic		t, c, a				// (~(x)) & (z)
	vshr.u32	q6, q4, #7
	eor		t, t, s				// t = Ch(x,y,z) = (((x) & (y)) ^ ((~(x)) & (z)))
	vshl.i32	q7, q4, #14
	ldr		s, WK($0)			//
	add		d, t				// t = h+Sigma1(e)+Ch(e,f,g);
	eor		t, e, e, ror #11	// S32(2,  (x)) ^ S32(13, (x))
	add		d, s				// h = T1
	eor		t, t, e, ror #20	// t = (S32(2,  (x)) ^ S32(13, (x)) ^ S32(22, (x)))				// t = Sigma0(a);
	add		h, d				// d += T1;
	vshr.u32	q4, q4, #3
	add		d, t, ror #2				// h = T1 + Sigma0(a);
	eor		t, f, g				// y^z
	and		s, f, g				// y&z
	veor		q4, q6
	vshr.u32	q6, #11
	and		t, t, e				// x&(y^z)
	eor		s, s, t				// t = Maj(x,y,z) = (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
	eor		t, h, h, ror #5	// S32(6,  (x)) ^ S32(11, (x))
	veor		q4, q7
	vshl.i32	q7, #11


	add		d, s
	eor		t, t, h, ror #19	// t = (S32(6,  (x)) ^ S32(11, (x)) ^ S32(25, (x)))
	and		s, h, a				// (x) & (y)
	add		c, t, ror #6			// use h to store h+Sigma1(e)
	bic		t, b, h				// (~(x)) & (z)

	veor		q4, q6
	veor		q4, q7
	vext.32	q6, $3, $4, #1			// Q6 = w12:w9
	vadd.s32	$1, q4				// w3:w0 + sigma0(w4:w1)
	eor		t, t, s				// t = Ch(x,y,z) = (((x) & (y)) ^ ((~(x)) & (z)))
	ldr		s, WK(1+$0)			//
	add		c, t				// t = h+Sigma1(e)+Ch(e,f,g);
	eor		t, d, d, ror #11	// S32(2,  (x)) ^ S32(13, (x))
	add		c, s				// h = T1
	eor		t, t, d, ror #20	// t = (S32(2,  (x)) ^ S32(13, (x)) ^ S32(22, (x)))				// t = Sigma0(a);
	vadd.s32	$1, q6				// w3:w0 + sigma0(w4:w1) + w12:w9
	vext.64	q4, $4, zero, #1		// 0 0 w15:w14
	add		g, c				// d += T1;
	add		c, t, ror #2			// h = T1 + Sigma0(a);
	eor		t, e, f				// y^z
	and		s, e, f				// y&z
	and		t, t, d				// x&(y^z)
	vshr.u32	q6, q4, #17
	vshl.i32	q7, q4, #13
	vshr.u32	q4, q4, #10
	eor		s, s, t				// t = Maj(x,y,z) = (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

	veor		q4, q6
	veor		q4, q7
	vshr.u32	q6, #2
	vshl.i32	q7, #2
	veor		q4, q6
	veor		q4, q7

	eor		t, g, g, ror #5		// S32(6,  (x)) ^ S32(11, (x))
	add		c, s
	eor		t, t, g, ror #19	// t = (S32(6,  (x)) ^ S32(11, (x)) ^ S32(25, (x)))
	vadd.s32	$1, q4				// w3:w0 + sigma0(w4:w1) + w12:w9 + sigma1(0 0 w15:w14)
	and		s, g, h				// (x) & (y)
	add		b, t, ror #6				// use h to store h+Sigma1(e)
	bic		t, a, g				// (~(x)) & (z)
	vext.64	q4, zero, $1, #1		// Q4 = (w17:w16 0 0)
	eor		t, t, s				// t = Ch(x,y,z) = (((x) & (y)) ^ ((~(x)) & (z)))
	ldr		s, WK(2+$0)			//
	add		b, t				// t = h+Sigma1(e)+Ch(e,f,g);
	vshr.u32	q6, q4, #17
	vshl.i32	q7, q4, #13
	vshr.u32	q4, q4, #10
	eor		t, c, c, ror #11	// S32(2,  (x)) ^ S32(13, (x))
	add		b, s				// h = T1
	eor		t, t, c, ror #20	// t = (S32(2,  (x)) ^ S32(13, (x)) ^ S32(22, (x)))				// t = Sigma0(a);
	add		f, b				// d += T1;
	veor		q4, q6
	veor		q4, q7
	add		b, t, ror #2			// h = T1 + Sigma0(a);
	eor		t, d, e				// y^z
	and		s, d, e				// y&z
	and		t, t, c				// x&(y^z)
	vshr.u32	q6, #2
	vshl.i32	q7, #2
	eor		s, s, t				// t = Maj(x,y,z) = (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

	eor		t, f, f, ror #5	// S32(6,  (x)) ^ S32(11, (x))
	add		b, s
	eor		t, t, f, ror #19	// t = (S32(6,  (x)) ^ S32(11, (x)) ^ S32(25, (x)))
	veor		q4, q6
	veor		q4, q7
	vadd.s32	$1, q4				// w19:w16 = w3:w0 + sigma0(w4:w1) + w12:w9 + sigma1(w17:w14)

	and		s, f, g				// (x) & (y)
	add		a, t, ror #6			// use h to store h+Sigma1(e)
	bic		t, h, f				// (~(x)) & (z)
	vadd.s32	q5, $1				// W+K
	eor		t, t, s				// t = Ch(x,y,z) = (((x) & (y)) ^ ((~(x)) & (z)))
	ldr		s, WK(3+$0)			//
	add		a, t				// t = h+Sigma1(e)+Ch(e,f,g);
	eor		t, b, b, ror #11	// S32(2,  (x)) ^ S32(13, (x))
	add		a, s				// h = T1
	eor		t, t, b, ror #20	// t = (S32(2,  (x)) ^ S32(13, (x)) ^ S32(22, (x)))				// t = Sigma0(a);
	add		e, a				// d += T1;
	add		a, t, ror #2				// h = T1 + Sigma0(a);
	eor		t, c, d				// y^z
	and		s, c, d				// y&z
	and		t, t, b				// x&(y^z)
	eor		s, s, t				// t = Maj(x,y,z) = (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
	add		t, sp, #(($0&15)*4)
	add		a, s

	vst1.32		{q5},[t,:128]
	.endm

	.text

	.align	4
K256:
	.long 	0x428a2f98
	.long 	0x71374491
	.long	0xb5c0fbcf
	.long	0xe9b5dba5
	.long	0x3956c25b
	.long	0x59f111f1
	.long	0x923f82a4
	.long	0xab1c5ed5
    .long	0xd807aa98
	.long	0x12835b01
	.long	0x243185be 
	.long	0x550c7dc3
    .long	0x72be5d74 
	.long	0x80deb1fe 
	.long	0x9bdc06a7 
	.long	0xc19bf174
    .long	0xe49b69c1 
	.long	0xefbe4786 
	.long	0x0fc19dc6 
	.long	0x240ca1cc
    .long	0x2de92c6f 
	.long	0x4a7484aa 
	.long	0x5cb0a9dc 
	.long	0x76f988da
    .long	0x983e5152 
	.long	0xa831c66d 
	.long	0xb00327c8 
	.long	0xbf597fc7
    .long	0xc6e00bf3 
	.long	0xd5a79147 
	.long	0x06ca6351 
	.long	0x14292967
    .long	0x27b70a85 
	.long	0x2e1b2138 
	.long	0x4d2c6dfc 
	.long	0x53380d13
    .long	0x650a7354 
	.long	0x766a0abb 
	.long	0x81c2c92e 
	.long	0x92722c85
    .long	0xa2bfe8a1 
	.long	0xa81a664b 
	.long	0xc24b8b70 
	.long	0xc76c51a3
    .long	0xd192e819 
	.long	0xd6990624 
	.long	0xf40e3585 
	.long	0x106aa070
    .long	0x19a4c116 
	.long	0x1e376c08 
	.long	0x2748774c 
	.long	0x34b0bcb5
    .long	0x391c0cb3 
	.long	0x4ed8aa4a 
	.long	0x5b9cca4f 
	.long	0x682e6ff3
    .long	0x748f82ee 
	.long	0x78a5636f 
	.long	0x84c87814 
	.long	0x8cc70208
    .long	0x90befffa
	.long	0xa4506ceb
	.long	0xbef9a3f7
	.long	0xc67178f2


    .globl _vng_armv7neon_sha256_compress
    .private_extern _vng_armv7neon_sha256_compress
_vng_armv7neon_sha256_compress:

    // due to the change of order in the 2nd and 3rd calling argument,
    // we need to switch r1/r2 to use the original code
    // cclee 1-13-11
    mov     r12, r1
    mov     r1, r2
    mov     r2, r12

	// push callee-saved registers
	push	{r4-r7,lr}
	add		r7, sp, #12			// set up dtrace frame pointer
	push	{r8-r11}


	// align sp to 16-byte boundary
	ands    r12, sp, #15		// bytes to align to 16-byte boundary
	addeq	r12, #16			// if nothing, enforce to insert 16 bytes
	sub     sp, r12
	str     r12, [sp]

#if KERNEL
    vpush   {q8}
#endif
    vpush   {q0-q7}
#define stack_size (16*5)       // circular buffer W0-W3, extra 16 to save num_blocks
    sub     sp, #stack_size

	str		r2, num_blocks 

	veor	zero, zero

	// set up pointer to table K256[]
	adr		K, K256

	// load W[0:15]
	vld1.s32	{W0-W1},[data]!
	vld1.s32	{W2-W3},[data]!

	// load K[0:15] & per word byte swap
	vrev32.8	W0, W0
	vrev32.8	W1, W1
	vld1.s32	{q4-q5}, [K,:128]!
	vrev32.8	W2, W2
	vrev32.8	W3, W3
	vld1.s32	{q6-q7}, [K,:128]!

	// compute WK[0:15] and save in stack

	vadd.s32	q4, q0
	vadd.s32	q5, q1
	vadd.s32	q6, q2
	vadd.s32	q7, q3

	vstmia		sp,{q4-q7}

	// digests a-h = ctx->states;
	ldmia		ctx,{a-d,e-h}

L_loop:

	// rounds 0:47 interleaved with W/WK update for rounds 16:63
#if 1
	rounds_a_schedule_update	 0,W0,W1,W2,W3
	rounds_e_schedule_update	 4,W1,W2,W3,W0
	rounds_a_schedule_update	 8,W2,W3,W0,W1
	rounds_e_schedule_update	12,W3,W0,W1,W2
	rounds_a_schedule_update	16,W0,W1,W2,W3
	rounds_e_schedule_update	20,W1,W2,W3,W0
	rounds_a_schedule_update	24,W2,W3,W0,W1
	rounds_e_schedule_update	28,W3,W0,W1,W2
	rounds_a_schedule_update	32,W0,W1,W2,W3
	rounds_e_schedule_update	36,W1,W2,W3,W0
	rounds_a_schedule_update	40,W2,W3,W0,W1
	rounds_e_schedule_update	44,W3,W0,W1,W2
#else
	rounds_a	0
	message_schedule W0,W1,W2,W3,16
	rounds_e	4 
	message_schedule W1,W2,W3,W0,20
	rounds_a	8
	message_schedule W2,W3,W0,W1,24
	rounds_e	12 
	message_schedule W3,W0,W1,W2,28
	rounds_a	16
	message_schedule W0,W1,W2,W3,32
	rounds_e	20 
	message_schedule W1,W2,W3,W0,36
	rounds_a	24
	message_schedule W2,W3,W0,W1,40
	rounds_e	28 
	message_schedule W3,W0,W1,W2,44
	rounds_a	32
	message_schedule W0,W1,W2,W3,48
	rounds_e	36 
	message_schedule W1,W2,W3,W0,52
	rounds_a	40
	message_schedule W2,W3,W0,W1,56
	rounds_e	44 
	message_schedule W3,W0,W1,W2,60
#endif

	// revert K to the beginning of K256[]
	ldr		t, num_blocks
	sub		K, #256

	subs	t, #1						// num_blocks--
	beq		L_final_block				// if final block, wrap up final rounds
	str		t, num_blocks

	// rounds 48:63 interleaved with W/WK initialization for next block rounds 0:15 
	rounds_a	48
	update_W_WK	0, W0
	rounds_e	52 
	update_W_WK	1, W1
	rounds_a	56
	update_W_WK	2, W2
	rounds_e	60 
	update_W_WK	3, W3

	// ctx->states += digests a-h
	Update_Digits

	// digests a-h = ctx->states;
	ldmia		ctx,{a-d,e-h}

	bal		L_loop				// branch for next block

	// wrap up digest update round 48:63 for final block
L_final_block:
	rounds_a	48
	rounds_e	52 
	rounds_a	56
	rounds_e	60 

	// ctx->states += digests a-h
	Update_Digits

	// free allocated stack memory
	add		sp, #stack_size

	// if kernel, restore q0-q8
	vpop	{q0-q1}
	vpop	{q2-q3}
	vpop	{q4-q5}
	vpop	{q6-q7}
#if	KERNEL
	vpop	{q8}
#endif

	// dealign sp from the 16-byte boundary
    ldr     r12, [sp]
    add     sp, r12

	// restore callee-save registers and return
	pop	{r8-r11}
	pop	{r4-r7,pc}

#endif /* _ARM_ARCH_7 */
