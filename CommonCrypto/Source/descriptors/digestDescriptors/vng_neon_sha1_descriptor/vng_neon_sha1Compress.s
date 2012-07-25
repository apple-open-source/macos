/*

	This file is copied from xnu-1776 (sha1armv7neon.s) and renamed to the current name to replace
	a buggy version (based on an earlier version I sent to murf (and eventually to Fabrice).
	It improves the sha1 performance slightly (8.3 to 7.5 cycles/byte), and also fixes a
	compiled-off vpop/vpush bug (compiled only for kernel code).

	The problem with the earlier code that murf/Fabrice was working on is that 
	the order of the 2nd and 3rd calling arguments are switched. (r1/r2).
	While the internal code uses r1 as an temp register, simply changing the order will have the
	number of blocks corrupted with temp variable.

	This is fixed by staying with the original order (r0/r1/r2) = (out/in/blocks), and
	have r1/r2 switched the 1st thing the subroutine is called.

	I ran the test to verify this fixes the bug.

	cclee	1-13-11 

	sha1armv7neon.s : this file provides optimized armv7+neon implementation of the sha1 function
	CoreOS - vector & numerics group
	cclee	10-10-10

	The implementation is based on the principle described in an Intel online article
    "Improving the Performance of the Secure Hash Algorithm (SHA-1)"
    http://software.intel.com/en-us/articles/improving-the-performance-of-the-secure-hash-algorithm-1/

	Update HASH[] by processing a one 64-byte block in MESSAGE[] can be represented by the following C function

void SHA1( int HASH[], int MESSAGE[] )
{
	int A[81], B[81], C[81], D[81], E[81];
	int W[80];
	int i, FN;

	A[0] = HASH[0]; B[0] = HASH[1]; C[0] = HASH[2]; D[0] = HASH[3]; E[0] = HASH[4];

	for ( i=0; i<80; ++i ) {
		if ( i < 16 )
			W[i] = BIG_ENDIAN_LOAD( MESSAGE[i] );
		else
		 	W[i] = ROTATE_LEFT( W[i-3] ^ W[i-8] ^ W[i-14] ^ W[i-16], 1 ); 

		FN = F( i, B[i], C[i], D[i] );
		A[i+1] = FN + E[i] + ROTATE_LEFT( A[i], 5 ) + W[i] + K(i);
		B[i+1] = A[i];
		C[i+1] = ROTATE_LEFT( B[i], 30 );
		D[i+1] = C[i];
		E[i+1] = D[i];
	}

	HASH[0] += A[80]; HASH[1] += B[80]; HASH[2] += C[80]; HASH[3] += D[80]; HASH[4] += E[80];
} 


	For i=0:15, W[i] is simply big-endian loading of MESSAGE[i]. 
	For i=16:79, W[i] is updated according to W[i] = ROTATE_LEFT( W[i-3] ^ W[i-8] ^ W[i-14] ^ W[i-16], 1 );

    The approach (by Dean Gaudet) can be used to vectorize the computation of W[i] for i=16:79,

    1. update 4 consequtive W[i] (stored in a single 16-byte register)
    W[i  ] = (W[i-3] ^ W[i-8] ^ W[i-14] ^ W[i-16]) rol 1
    W[i+1] = (W[i-2] ^ W[i-7] ^ W[i-13] ^ W[i-15]) rol 1
    W[i+2] = (W[i-1] ^ W[i-6] ^ W[i-12] ^ W[i-14]) rol 1
    W[i+3] = (   0   ^ W[i-5] ^ W[i-11] ^ W[i-13]) rol 1

    2. this additional calculation unfortunately requires many additional operations
    W[i+3] ^= W[i] rol 1

    3. once we have 4 W[i] values in a Q register, we can also add four K values with one instruction
    W[i:i+3] += {K,K,K,K}

    Let W0 = {W[i] W[i+1] W[i+2] W[i+3]} be the current W-vector to be computed, 
		W4 = {W[i-4] W[i-3] W[i-2] W[i-1]} be the previous vector, and so on

    The Dean Gaudet approach can be expressed as

    1. W0 = rotate_left(left_shift(W4,32) ^ W8 ^ left_shift(concatenate(W16,W12),64) ^ W16,1);
    2. W[i+3] ^= W[i] rol 1
    3. W0 += {K,K,K,K}

    For i>=32, the Intel online article suggests that (using a basic identity (X rol 1) rol 1 = X rol 2) 
	the update equation is equivalent to

    1. W0 = rotate_left(left_shift(concatenate(W8,W4),64) ^ W16 ^ W28 ^ W32, 2); 

    Note:
    1. In total, we need 8 16-byte registers or memory for W0,W4,...,W28. W0 and W32 can be the same register or memory.
    2. The registers are used in a circular buffering mode. For example, we start with W28,W24,...,W0 
		(with W0 indicating the most recent 16-byte) 
		i=0, W28,W24,...,W0
        i=4, W24,W20,...,W28
        i=8, W20,W16,...,W24
        .
        .
        and so forth.
    3. once W-vector is computed, W+K is then computed and saved in the stack memory, this will be used later when
		updating the digests A/B/C/D/E 

	the execution flow (for 1 single 64-byte block) looks like

	W_PRECALC_00_15		// big-endian loading of 64-bytes into 4 W-vectors, compute WK=W+K, save WK in the stack memory

	W_PRECALC_16_31		// for each vector, update digests, update W (Gaudet) and WK=W+K, save WK in the stack memory

	W_PRECALC_32_79		// for each vector, update digests, update W (Intel) and WK=W+K, save WK in the stack memory 

	our implementation (allows multiple blocks per call) pipelines the loading of W/WK of a future block 
    into the last 16 rounds of its previous block:

	----------------------------------------------------------------------------------------------------------

	load W(0:15) (big-endian per 4 bytes) into 4 Q registers
    pre_calculate and store WK = W+K(0:15) in 16-byte aligned stack memory

L_loop:

    load digests a-e from ctx->state;

    for (r=0;r<16;r+=4) {
        digests a-e update and permute round r:r+3
        update W([r:r+3]%16) (Gaudet) and WK([r:r+3]%16) for the next 4th iteration 
    }

    for (r=16;r<64;r+=4) {
        digests a-e update and permute round r:r+3
        update W([r:r+3]%16) (Intel) and WK([r:r+3]%16) for the next 4th iteration 
    }

    num_block--;
    if (num_block==0)   jmp L_last_block;

    for (r=64;r<80;r+=4) {
        digests a-e update and permute round r:r+3
        load W([r:r+3]%16) (big-endian per 4 bytes) into 4 Q registers
        pre_calculate and store W+K([r:r+3]%16) in stack
    }

    ctx->states += digests a-e;

    jmp L_loop;

L_last_block:

    for (r=64;r<80;r+=4) {
        digests a-e update and permute round r:r+3
    }

    ctx->states += digests a-e;


	----------------------------------------------------------------------------------------------------------
	
*/

#include <arm/arch.h>

#if defined(_ARM_ARCH_7)

#define	OPTIMIZED	1		// defined OPTIMIZED to use our final optimized assembly code
#define	Multiple_Blocks	1	// defined Multiple_Blocks to allow performing multiple blocks sha1 computation per call
							//	i.e., change to prototype to void SHA1( int HASH[], int MESSAGE[], int num_blocks );

// symbolizing all used variables

#define	ctx			r0			// SHA1_CTX	*ctx;
#define	buf			r1			// char *MESSAGE[];
#define	num_blocks	r2			// int	num_blocks;	this is used only if Multiple_Blocks is defined.

#define K_BASE     	r3			// to point to the tables for K
#define HASH_PTR   	r4			// copy of ctx, to release r0 for other usage
#define BUFFER_PTR 	r5			// copy of buf, to release r1 for other usage

// sha1 digests a-e
#define A r6
#define B r8
#define C r9
#define D r10
#define E r11

// temp variables
#define T1 r0
#define T2 r1

// used Q registers
#define W_TMP  		q12
#define W_TMP2 		q13
#define	ZERO		q14
#define	K_VALUES	q15
#define W0  		q4
#define W4  		q5
#define W8  		q6
#define W12 		q7
#define W16 		q8
#define W20 		q9
#define W24 		q10
#define W28 		q11

// for round t (0:79), W+K was previously saved in the stack space, and can be referenced by 
#define	WK(t)	[sp, #((t&15)*4)]



	// ----------- macros for initial rounds 0:15 big-endian loading of message[] and saving W+K into stack  ---------

	.macro	W_PRECALC_00_15_0		// default : BUFFER_PTR points to the current message block 
	vld1.f32 {W_TMP},[BUFFER_PTR]!	// loading 16-byte message[] (16-byte aligned) into W_TMP
	.endm

	.macro	W_PRECALC_00_15_1		// input $0 : current 16-bytes in the circular buffer
	vrev32.8 $0, W_TMP				// byte swap W_TMP and save to $0
	.endm

	.macro	W_PRECALC_00_15_2		// $0 = W, K_VALUES = (K,K,K,K)
	vadd.s32	W_TMP, $0, K_VALUES	// W_TMP = W + K
	.endm
	
	.macro	W_PRECALC_00_15_3		// default : lr points to the 16-byte aligned memory to store W+K
	vst1.s32	{W_TMP}, [lr,:128]!	// save W[i]+K in 16-byte aligned stack memory
	.endm

	.macro	INITIAL_W_PRECALC

	vld1.s32	{K_VALUES}, [K_BASE,:128]!		// loading K values into a Q register

	// W28,W24,....,W4,W0 is used as memory in a circular buffer. They are listed in forward order from left to right.
	// The last one, e.g., W0=W[0], is the current 4 .long (4-bytes) register. W4 = W[-4]. W28 = W[-28].
	// After a circular buffer rotation, W28 = W[0], W0 = W[-4], W4 = W[-8] and so forth.

#if !OPTIMIZED

	// proliferate the code using the above 4 macros
	// at this point, lr = sp (16-bytes aligned), K_VALUES dupped with K, NUFFER_PTR points to the current message[]

	// i=0
	// circular buffer : W28,W24,W20,W16,W12,W8,W4,W0
	W_PRECALC_00_15_0						// W_TMP = (BUFFER_PTR)
	W_PRECALC_00_15_1	W0					// convert W_TMP to big-endian, and save W0 = W[0] = W_TMP	
	W_PRECALC_00_15_2	W0					// W_TMP = W[0] + K[0]
	W_PRECALC_00_15_3						// (sp) = W_TMP = W[0] + K[0]

	// i=4
	// circular buffer : W24,W20,W16,W12,W8,W4,W0,W28
	W_PRECALC_00_15_0						// W_TMP = 16(BUFFER_PTR)
	W_PRECALC_00_15_1	W28					// convert W_TMP to big-endian, and save W28 = W[0] = W_TMP	
	W_PRECALC_00_15_2	W28					// W_TMP = W[0] + K[0]
	W_PRECALC_00_15_3						// 16(sp) = W_TMP = W[0] + K[0]

	// i=8
	// circular buffer : W20,W16,W12,W8,W4,W0,W28,W24
	W_PRECALC_00_15_0						// W_TMP = 32(BUFFER_PTR)
	W_PRECALC_00_15_1	W24					// convert W_TMP to big-endian, and save W24 = W[0] = W_TMP
	W_PRECALC_00_15_2	W24					// W_TMP = W[0] + K[0]
	W_PRECALC_00_15_3						// 32(sp) = W_TMP = W[0] + K[0]

	// i=12
	// circular buffer : W16,W12,W8,W4,W0,W28,W24,W20
	W_PRECALC_00_15_0						// W_TMP = 48(BUFFER_PTR)
	W_PRECALC_00_15_1	W20					// convert W_TMP to big-endian, and save W20 = W[0] = W_TMP	
	W_PRECALC_00_15_2	W20					// W_TMP = W[0] + K[0]
	W_PRECALC_00_15_3						// 48(sp) = W_TMP = W[0] + K[0]

#else

	// the following performs the same task, using less instructions (and slightly fewer CPU cycles)
	//the code uses W4 W8 temporarily, with no harm

	vld1.f32 {W_TMP,W_TMP2},[BUFFER_PTR]!
	vld1.f32 {W4,W8},[BUFFER_PTR]!
	vrev32.8 W0, W_TMP
	vrev32.8 W28, W_TMP2
	vrev32.8 W24, W4
	vrev32.8 W20, W8
	vadd.s32	W_TMP, W0, K_VALUES
	vadd.s32	W_TMP2, W28, K_VALUES
	vadd.s32	W4, W24, K_VALUES
	vadd.s32	W8, W20, K_VALUES
	vst1.s32	{W_TMP,W_TMP2}, [lr,:128]!	// save W[i]+K(i) in stack memory
	vst1.s32	{W4,W8}, [lr,:128]!	// save W[i]+K(i) in stack memory

#endif

	.endm		// INITIAL_W_PRECALC

	// ----------------- functions F1/F2/F3/F4 used in updating the sha1 digests -----------------------

	// F1(b,c,d) = ((((c) ^ (d)) & (b)) ^ (d))
	.macro	F1
	eor	T1, $2, $1
	and	T1, $0
	eor	T1, $2
	.endm

	// F2(b,c,d) = ((b) ^ (c) ^ (d))
	.macro	F2
	eor	T1, $1, $2
	eor	T1, $0
	.endm

	// F3(b,c,d) = (((b) & (c)) | (((b) | (c)) & (d)))
	.macro	F3
	orr T1, $0, $1
	and r12, $1, $0
	and T1, $2
	orr	T1, r12
	.endm

	#define	F4	F2					// F4 = F2

	// -------------------------------------------------------------------------------------------------

	// ------  macros for performing sha1 digest update, rotating the roles of digest registers  -------
	/*
		FN = F( i, B[i], C[i], D[i] );
		A[i+1] = FN + E[i] + ROTATE_LEFT( A[i], 5 ) + WK[i];
		B[i+1] = A[i];
		C[i+1] = ROTATE_LEFT( B[i], 30 );
		D[i+1] = C[i];
		E[i+1] = D[i];
	*/

	.macro	RR0
	$0		$2, $3, $4			// T1 = FN = F( i, B[i], C[i], D[i] );
	ldr		r12, WK($6)			// r12 = WK[i]
	ror		$2, #2				// C[i+1] = ROTATE_LEFT( B[i], 30 );
	add		$5, r12				// E[i] + WK[i]
	ldr		r12, WK($6+1)		// r12 = WK[i+1]
	add		$4, r12				// D[i] + WK[i+1]
	add		$5, T1				// E[i] + WK[i] + FN
	.endm

	.macro	RR1
	add		$5, $5, $1, ror #27				// T2 = FN + E[i] + ROTATE_LEFT( A[i], 5 ) + WK[i]
	add		$4, $5, ror #27			// E[i+1] + WK[i+1] + ROTATE_LEFT( A[i+1], 5 )
	$0		$1, $2, $3			// T1 = FN = F(i+1, B[i+1], C[i+1], D[i+1])
	add		$4, T1				// FN + FN + E[i+1] + ROTATE_LEFT( A[i+1], 5 ) + WK[i+1]; 
	ror		$1, #2				// C[i+1] = B[i] = ROTATE_LEFT( B[i], 30 );
	.endm
	// -------------------------------------------------------------------------------------------------

	// RR0/RR1 and F1/F2/F3 are combined (re-organize the instruction schedule to reduce stalls)
	// hence the following 6 combined macros

	.macro	RR0_F1
	ldr		r12, WK($6)
	eor 	T1, $3, $4
	ldr		T2, WK($6+1)
	and		T1, $2
	add		$5, r12
	ror		$2, #2
	eor		T1, $4
	add		$4, T2
	add		$5, T1
	.endm

	.macro	RR0_F2
	ldrd	T1,T2,WK($6)
	eor		r12, $2, $4
	ror		$2, #2
	eor		r12, $3
	add		$5, T1
	add		$4, T2
	add		$5, r12
	.endm

	.macro	RR0_F3
	ldr		r12, WK($6)
	orr		T1, $2, $3
	and		T2, $2, $3 
	ror		$2, #2
	add		$5, r12
	ldr		r12, WK($6+1)
	and		T1, $4
	orr		T1, T2
	add		$4, r12
	add		$5, T1
	.endm

	.macro	RR1_F1
	add		$5, $5, $1, ror #27
    eor 	T1, $3, $2
    and 	T1, $1
    eor 	T1, $3
	add		$4, $5, ror #27
	ror		$1, #2
	add		$4, T1
	.endm

	.macro	RR1_F2
	eor		T1, $1, $2
	add		$5, $5, $1, ror #27
	eor		T1, $3
	add		$4, T1
	add		$4, $5, ror #27
	ror		$1, #2
	.endm

	.macro	RR1_F3
	add		$5, $5, $1, ror #27
	orr		T1, $1, $2
	and		T1, $3
	and		r12, $1, $2
	orr		T1, r12
	add		$4, $5, ror #27
	ror		$1, #2
	add		$4, T1
	.endm


	// ------  rounds 16-31 compute W[0] using the vectorization approach by Dean Gaudet -------
	/*
	W[i  ] = (W[i-3] ^ W[i-8] ^ W[i-14] ^ W[i-16]) rol 1
    W[i+1] = (W[i-2] ^ W[i-7] ^ W[i-13] ^ W[i-15]) rol 1
    W[i+2] = (W[i-1] ^ W[i-6] ^ W[i-12] ^ W[i-14]) rol 1
    W[i+3] = (   0   ^ W[i-5] ^ W[i-11] ^ W[i-13]) rol 1

	W[i+3] ^= W[i] rol 1
	*/

	// -------------- macros for rounds 16-31 compute W (Gaudet's approach) and save W+K --------------

	.macro	W_PRECALC_16_31_0			// W_16,W_12,W_8,W_4,W
	vext.64	$4, $0, $1, #1
	veor	$4, $2						// W_8 ^ W_14	
	vext.32	W_TMP,$3,ZERO,#1
	.endm

	.macro	W_PRECALC_16_31_1	// W_16,W
	veor	W_TMP, $0				// W_3 ^ W_16
	veor	$1, W_TMP				// W_3 ^ W_16 ^ W_8 ^ W_14
	vshl.i32	W_TMP, $1, #1
	vext.32	W_TMP2,ZERO, $1, #1	
	vshr.u32	$1, #31
	.endm

	.macro	W_PRECALC_16_31_2	// W
	vorr	W_TMP, $0
	vshl.i32	$0, W_TMP2, #2
	vshr.u32	W_TMP2, #30
	.endm

	.macro	W_PRECALC_16_31_3	// W, i, K_XMM
	veor	W_TMP, W_TMP2
	veor	W_TMP, $0
	vadd.s32	W_TMP2, W_TMP, K_VALUES
	vorr	$0, W_TMP, W_TMP
	vst1.f32	{W_TMP2}, [lr,:128]!
	.endm

	// -------------- macros for rounds 32-79 compute W (Max's approach) and save W+K --------------

	.macro	W_PRECALC_32_79_0			// W_28,W_8,W_4,W
	veor		$3, $0
	vext.64		W_TMP, $1, $2, #1
	.endm

	.macro	W_PRECALC_32_79_1	// W_16,W
	veor	W_TMP, $0
	veor	W_TMP, $1
	vshr.u32	$1, W_TMP, #30
	vshl.i32	W_TMP, #2
	.endm

	.macro	W_PRECALC_32_79_2	// W
	vorr		W_TMP, $0
	.endm

	.macro	W_PRECALC_32_79_3	// W, i, K_XMM
	vadd.s32	W_TMP2, W_TMP, K_VALUES
	vorr	$0, W_TMP, W_TMP
	vst1.f32	{W_TMP2}, [lr,:128]!
	.endm

	// the main code body in early revisions --- no so optimized

#if	!OPTIMIZED
	.macro	INTERNAL
	// i=16
	// circular buffer : W12,W8,W4,W0,W28,W24,W20,W16
	W_PRECALC_16_31_0	W0,W28,W24,W20,W16
	RR0_F1				F1,A,B,C,D,E,0
	W_PRECALC_16_31_1	W0,W16
	RR1_F1				F1,A,B,C,D,E,0
	W_PRECALC_16_31_2	W16
	RR0_F1				F1,D,E,A,B,C,2
	W_PRECALC_16_31_3	W16, 2, 0
	RR1_F1				F1,D,E,A,B,C,2

	vld1.s32	{K_VALUES}, [K_BASE,:128]!

	// i=20,
	// W8,W4,W0,W28,W24,W20,W16,W12
	W_PRECALC_16_31_0	W28,W24,W20,W16,W12
	RR0_F1				F1,B,C,D,E,A,4
	W_PRECALC_16_31_1	W28,W12
	RR1_F1				F1,B,C,D,E,A,4
	W_PRECALC_16_31_2	W12
	RR0_F1				F1,E,A,B,C,D,6
	W_PRECALC_16_31_3	W12, 6, 16
	RR1_F1				F1,E,A,B,C,D,6

	// i=24,
	// W4,W0,W28,W24,W20,W16,W12,W8
	W_PRECALC_16_31_0	W24,W20,W16,W12,W8
	RR0_F1				F1,C,D,E,A,B,8
	W_PRECALC_16_31_1	W24,W8
	RR1_F1				F1,C,D,E,A,B,8
	W_PRECALC_16_31_2	W8
	RR0_F1				F1,A,B,C,D,E,10
	W_PRECALC_16_31_3	W8,10,16
	RR1_F1				F1,A,B,C,D,E,10

	// i=28
	// W0,W28,W24,W20,W16,W12,W8,W4
	W_PRECALC_16_31_0	W20,W16,W12,W8,W4
	RR0_F1				F1,D,E,A,B,C,12
	W_PRECALC_16_31_1	W20,W4
	RR1_F1				F1,D,E,A,B,C,12
	W_PRECALC_16_31_2	W4
	RR0_F1				F1,B,C,D,E,A,14
	W_PRECALC_16_31_3	W4,14,16
	RR1_F1				F1,B,C,D,E,A,14

	sub		lr, #0x40

	//i=32
	// W28,W24,W20,W16,W12,W8,W4,W0
	W_PRECALC_32_79_0	W28,W8,W4,W0
	RR0_F1				F1,E,A,B,C,D,16
	W_PRECALC_32_79_1	W16,W0
	RR1_F1				F1,E,A,B,C,D,16
	W_PRECALC_32_79_2	W0
	RR0_F1				F1,C,D,E,A,B,18
	W_PRECALC_32_79_3	W0,18,16
	RR1_F1				F1,C,D,E,A,B,18

	//i=36
	// W24,W20,W16,W12,W8,W4,W0,W28
	W_PRECALC_32_79_0	W24,W4,W0,W28
	RR0_F2				F2,A,B,C,D,E,20
	W_PRECALC_32_79_1	W12,W28
	RR1_F2				F2,A,B,C,D,E,20
	W_PRECALC_32_79_2	W28
	RR0_F2				F2,D,E,A,B,C,22
	W_PRECALC_32_79_3	W28,22,16
	RR1_F2				F2,D,E,A,B,C,22

	vld1.s32    {K_VALUES}, [K_BASE,:128]!

	//i=40
	#undef  K_XMM
    #define K_XMM   32
	// W20,W16,W12,W8,W4,W0,W28,W24	
	W_PRECALC_32_79_0	W20,W0,W28,W24
	RR0_F2				F2,B,C,D,E,A,24
	W_PRECALC_32_79_1	W8,W24
	RR1_F2				F2,B,C,D,E,A,24
	W_PRECALC_32_79_2	W24
	RR0_F2				F2,E,A,B,C,D,26
	W_PRECALC_32_79_3	W24,26,K_XMM
	RR1_F2				F2,E,A,B,C,D,26

	//i=44
	// W16,W12,W8,W4,W0,W28,W24,W20
	W_PRECALC_32_79_0	W16,W28,W24,W20
	RR0_F2				F2,C,D,E,A,B,28
	W_PRECALC_32_79_1	W4,W20
	RR1_F2				F2,C,D,E,A,B,28
	W_PRECALC_32_79_2	W20
	RR0_F2				F2,A,B,C,D,E,30
	W_PRECALC_32_79_3	W20,30,K_XMM
	RR1_F2				F2,A,B,C,D,E,30

	sub		lr, #0x40

	//i=48
	// W12,W8,W4,W0,W28,W24,W20,W16
	W_PRECALC_32_79_0	W12,W24,W20,W16
	RR0_F2				F2,D,E,A,B,C,32
	W_PRECALC_32_79_1	W0,W16
	RR1_F2				F2,D,E,A,B,C,32
	W_PRECALC_32_79_2	W16
	RR0_F2				F2,B,C,D,E,A,34
	W_PRECALC_32_79_3	W16,34,K_XMM
	RR1_F2				F2,B,C,D,E,A,34

	//i=52
	// W8,W4,W0,W28,W24,W20,W16,W12
	W_PRECALC_32_79_0	W8,W20,W16,W12
	RR0_F2				F2,E,A,B,C,D,36
	W_PRECALC_32_79_1	W28,W12
	RR1_F2				F2,E,A,B,C,D,36
	W_PRECALC_32_79_2	W12
	RR0_F2				F2,C,D,E,A,B,38
	W_PRECALC_32_79_3	W12,38,K_XMM
	RR1_F2				F2,C,D,E,A,B,38

	//i=56
	// W4,W0,W28,W24,W20,W16,W12,W8
	W_PRECALC_32_79_0	W4,W16,W12,W8
	RR0_F3				F3,A,B,C,D,E,40
	W_PRECALC_32_79_1	W24,W8
	RR1					F3,A,B,C,D,E,40
	W_PRECALC_32_79_2	W8
	RR0_F3				F3,D,E,A,B,C,42
	W_PRECALC_32_79_3	W8,42,K_XMM
	RR1_F3				F3,D,E,A,B,C,42

	vld1.s32    {K_VALUES}, [K_BASE,:128]!

	//i=60
	#undef	K_XMM
	#define	K_XMM	48
	// W0,W28,W24,W20,W16,W12,W8,W4
	W_PRECALC_32_79_0	W0,W12,W8,W4
	RR0_F3				F3,B,C,D,E,A,44
	W_PRECALC_32_79_1	W20,W4
	RR1_F3				F3,B,C,D,E,A,44
	W_PRECALC_32_79_2	W4
	RR0_F3				F3,E,A,B,C,D,46
	W_PRECALC_32_79_3	W4,46,K_XMM
	RR1_F3				F3,E,A,B,C,D,46

	sub		lr, #0x40

	//i=64
	// W28,W24,W20,W16,W12,W8,W4,W0
	W_PRECALC_32_79_0	W28,W8,W4,W0
	RR0_F3				F3,C,D,E,A,B,48
	W_PRECALC_32_79_1	W16,W0
	RR1_F3				F3,C,D,E,A,B,48
	W_PRECALC_32_79_2	W0
	RR0_F3				F3,A,B,C,D,E,50
	W_PRECALC_32_79_3	W0,50,K_XMM
	RR1_F3				F3,A,B,C,D,E,50

	//i=68
	// W24,W20,W16,W12,W8,W4,W0,W28
	W_PRECALC_32_79_0	W24,W4,W0,W28
	RR0_F3				F3,D,E,A,B,C,52
	W_PRECALC_32_79_1	W12,W28
	RR1_F3				F3,D,E,A,B,C,52
	W_PRECALC_32_79_2	W28
	RR0_F3				F3,B,C,D,E,A,54
	W_PRECALC_32_79_3	W28,54,K_XMM
	RR1_F3				F3,B,C,D,E,A,54

	//i=72
	// W20,W16,W12,W8,W4,W0,W28,W24
	W_PRECALC_32_79_0	W20,W0,W28,W24
	RR0_F3				F3,E,A,B,C,D,56
	W_PRECALC_32_79_1	W8,W24
	RR1_F3				F3,E,A,B,C,D,56
	W_PRECALC_32_79_2	W24
	RR0_F3				F3,C,D,E,A,B,58
	W_PRECALC_32_79_3	W24,58,K_XMM
	RR1_F3				F3,C,D,E,A,B,58

	// starting using F4	

	//i=76
	// W16,W12,W8,W4,W0,W28,W24,W20
	W_PRECALC_32_79_0	W16,W28,W24,W20
	RR0_F2				F4,A,B,C,D,E,60
	W_PRECALC_32_79_1	W4,W20
	RR1_F2				F4,A,B,C,D,E,60
	W_PRECALC_32_79_2	W20
	RR0_F2				F4,D,E,A,B,C,62
	W_PRECALC_32_79_3	W20,62,K_XMM
	RR1_F2				F4,D,E,A,B,C,62

	sub		K_BASE, #64
	sub		lr, #0x40

	.endm
#endif		// !OPTIMIZED
	


	// macros that further combined W_PRECALC_16_31_0 with RR0 to reduce pipeline stalls
	
	.macro	W_PRECALC_16_31_0_RR0_0		// W_16,W_12,W_8,W_4,W
	vext.64	$4, $0, $1, #1
	ldr		r12, WK(0)
	eor 	T1, $7, $8
	ldr		T2, WK(1)
	vext.32	W_TMP,$3,ZERO,#1
	and		T1, $6
	add		$9, r12
	ror		$6, #2
	veor	$4, $2						// W_8 ^ W_14	
	eor		T1, $8
	add		$8, T2
	add		$9, T1
	ror		T2, $5, #27
	.endm

	.macro	W_PRECALC_16_31_0_RR0_4		// W_16,W_12,W_8,W_4,W
	vext.64	$4, $0, $1, #1
	ldr		r12, WK(4)
	eor 	T1, $7, $8
	ldr		T2, WK(5)
	vext.32	W_TMP,$3,ZERO,#1
	and		T1, $6
	add		$9, r12
	ror		$6, #2
	veor	$4, $2						// W_8 ^ W_14	
	eor		T1, $8
	add		$8, T2
	add		$9, T1
	ror		T2, $5, #27
	.endm

	.macro	W_PRECALC_16_31_0_RR0_8		// W_16,W_12,W_8,W_4,W
	vext.64	$4, $0, $1, #1
	ldr		r12, WK(8)
	eor 	T1, $7, $8
	ldr		T2, WK(9)
	vext.32	W_TMP,$3,ZERO,#1
	and		T1, $6
	add		$9, r12
	ror		$6, #2
	veor	$4, $2						// W_8 ^ W_14	
	eor		T1, $8
	add		$8, T2
	add		$9, T1
	ror		T2, $5, #27
	.endm

	.macro	W_PRECALC_16_31_0_RR0_12		// W_16,W_12,W_8,W_4,W
	vext.64	$4, $0, $1, #1
	ldr		r12, WK(12)
	eor 	T1, $7, $8
	ldr		T2, WK(13)
	vext.32	W_TMP,$3,ZERO,#1
	and		T1, $6
	add		$9, r12
	ror		$6, #2
	veor	$4, $2						// W_8 ^ W_14	
	eor		T1, $8
	add		$8, T2
	add		$9, T1
	ror		T2, $5, #27
	.endm

	// macros that further combined W_PRECALC_16_31_1 with RR1 to reduce pipeline stalls
	.macro	W_PRECALC_16_31_1_RR1	// W_16,W
	veor	W_TMP, $0				// W_3 ^ W_16
    eor 	T1, $4, $3
    and 	T1, $2
	add		T2, $6
    eor 	T1, $4
	veor	$1, W_TMP				// W_3 ^ W_16 ^ W_8 ^ W_14
	orr		$6, T2, T2
	// ror		T2, #27
	vshl.i32	W_TMP, $1, #1
	ror		$2, #2
	vext.32	W_TMP2,ZERO, $1, #1	
	add		$5, T1
	vshr.u32	$1, #31
	add		$5, T2, ror #27
	.endm

	// macros that further combined W_PRECALC_16_31_2 with RR0 to reduce pipeline stalls
	.macro	W_PRECALC_16_31_2_RR0	// W
	ldr		r12, WK($6)
	vorr	W_TMP, $0
	eor 	T1, $3, $4
	ldr		T2, WK($6+1)
	and		T1, $2
	vshl.i32	$0, W_TMP2, #2
	add		$5, r12
	ror		$2, #2
	eor		T1, $4
	add		$4, T2
	vshr.u32	W_TMP2, #30
	ror		T2, $1, #27
	add		$5, T1
	.endm

	// macros that further combined W_PRECALC_16_31_3 with RR1 to reduce pipeline stalls
	.macro	W_PRECALC_16_31_3_RR1	// W, i, K_XMM
	veor	W_TMP, W_TMP2
    eor 	T1, $3, $2
	add		T2, $5
    and 	T1, $1
	ror		$1, #2
	veor	W_TMP, $0
	orr		$5, T2, T2
	// ror		T2, #27
	vadd.s32	W_TMP2, W_TMP, K_VALUES
    eor 	T1, $3
	vorr	$0, W_TMP, W_TMP
	add		$4, T2, ror #27
	vst1.f32	{W_TMP2}, [lr,:128]!
	add		$4, T1
	.endm

	// a super macro that combines 4 macro proliferations to further reduce pipeline stalls
	// 	W_PRECALC_16_31_0_RR0_0	W0,W28,W24,W20,W16,A,B,C,D,E
	// 	W_PRECALC_16_31_1_RR1	W0,W16,A,B,C,D,E
	// 	W_PRECALC_16_31_2_RR0	W16,D,E,A,B,C,2 
	// 	W_PRECALC_16_31_3_RR1	W16,D,E,A,B,C

	.macro	SUPER_W_PRECALC_16_31_0		// W_16,W_12,W_8,W_4,W
	vext.64	$4, $0, $1, #1
	ldrd	T1, T2, WK(0)
	vext.32	W_TMP,$3,ZERO,#1
	eor 	r12, $7, $8
	veor	$4, $2						// W_8 ^ W_14	
	and		r12, $6
	veor	W_TMP, $0				// W_3 ^ W_16
	add		$9, T1
	ror		$6, #2
	eor		r12, $8
	add		$8, T2
	veor	$4, W_TMP				// W_3 ^ W_16 ^ W_8 ^ W_14
	add		$9, r12
    eor 	r12, $7, $6
	vext.32	W_TMP2,ZERO, $4, #1	
	add		$9, $9, $5, ror #27
	vshl.i32	W_TMP, $4, #1
    and 	r12, $5
	vshr.u32	$4, #31
    eor 	r12, $7
	vorr	W_TMP, $4
	ror		$5, #2
	vshl.i32	$4, W_TMP2, #2
	add		$8, r12
	vshr.u32	W_TMP2, #30
	add		$8, $9, ror #27
	ldrd	T1, T2, WK(2)
	eor 	r12, $5, $6
	veor	W_TMP, W_TMP2
	add		$7, T1
	and		r12, $9
	ror		$9, #2
	veor	W_TMP, $4
	eor		r12, $6
	add		$6, T2
	vadd.s32	W_TMP2, W_TMP, K_VALUES
	add		$7, r12
	vorr		$4, W_TMP, W_TMP
    eor 	r12, $5, $9
	add		$7, $7, $8, ror #27
    and 	r12, $8
	ror		$8, #2
    eor 	r12, $5
	add		$6, $7, ror #27
	vst1.f32	{W_TMP2}, [lr,:128]!
	add		$6, r12
	.endm

	// a super macro that combines 4 macro proliferations to further reduce pipeline stalls
	//	W_PRECALC_16_31_0_RR0_4	W28,W24,W20,W16,W12,B,C,D,E,A
	//	W_PRECALC_16_31_1_RR1	W28,W12,B,C,D,E,A
	//	W_PRECALC_16_31_2_RR0	W12,E,A,B,C,D,6
	//	W_PRECALC_16_31_3_RR1	W12,E,A,B,C,D
	.macro	SUPER_W_PRECALC_16_31_4		// W_16,W_12,W_8,W_4,W
	vext.64	$4, $0, $1, #1
	ldrd	T1, T2, WK(4)
	vext.32	W_TMP,$3,ZERO,#1
	eor 	r12, $7, $8
	veor	$4, $2						// W_8 ^ W_14	
	and		r12, $6
	veor	W_TMP, $0				// W_3 ^ W_16
	add		$9, T1
	ror		$6, #2
	eor		r12, $8
	add		$8, T2
	veor	$4, W_TMP				// W_3 ^ W_16 ^ W_8 ^ W_14
	add		$9, r12
    eor 	r12, $7, $6
	vext.32	W_TMP2,ZERO, $4, #1	
	add		$9, $9, $5, ror #27
	vshl.i32	W_TMP, $4, #1
    and 	r12, $5
	vshr.u32	$4, #31
    eor 	r12, $7
	vorr	W_TMP, $4
	ror		$5, #2
	vshl.i32	$4, W_TMP2, #2
	add		$8, r12
	vshr.u32	W_TMP2, #30
	add		$8, $9, ror #27
	ldrd	T1, T2, WK(6)
	eor 	r12, $5, $6
	veor	W_TMP, W_TMP2
	add		$7, T1
	and		r12, $9
	ror		$9, #2
	veor	W_TMP, $4
	eor		r12, $6
	add		$6, T2
	vadd.s32	W_TMP2, W_TMP, K_VALUES
	add		$7, r12
	vorr	$4, W_TMP, W_TMP
    eor 	r12, $5, $9
	add		$7, $7, $8, ror #27
    and 	r12, $8
	ror		$8, #2
    eor 	r12, $5
	add		$6, $7, ror #27
	vst1.f32	{W_TMP2}, [lr,:128]!
	add		$6, r12
	.endm

	// a super macro that combines 4 macro proliferations to further reduce pipeline stalls
	//	W_PRECALC_16_31_0_RR0_8	W24,W20,W16,W12,W8,C,D,E,A,B
	//	W_PRECALC_16_31_1_RR1	W24,W8,C,D,E,A,B
	//	W_PRECALC_16_31_2_RR0	W8,A,B,C,D,E,10
	//	W_PRECALC_16_31_3_RR1	W8,A,B,C,D,E
	.macro	SUPER_W_PRECALC_16_31_8		// W_16,W_12,W_8,W_4,W
	vext.64	$4, $0, $1, #1
	ldrd	T1, T2, WK(8)
	vext.32	W_TMP,$3,ZERO,#1
	eor 	r12, $7, $8
	veor	$4, $2						// W_8 ^ W_14	
	and		r12, $6
	veor	W_TMP, $0				// W_3 ^ W_16
	add		$9, T1
	ror		$6, #2
	eor		r12, $8
	add		$8, T2
	veor	$4, W_TMP				// W_3 ^ W_16 ^ W_8 ^ W_14
	add		$9, r12
	//ror		T2, $5, #27
    eor 	r12, $7, $6
	vext.32	W_TMP2,ZERO, $4, #1	
	add		$9, $9, $5, ror #27
	vshl.i32	W_TMP, $4, #1
    and 	r12, $5
	vshr.u32	$4, #31
    eor 	r12, $7
	vorr	W_TMP, $4
	ror		$5, #2
	vshl.i32	$4, W_TMP2, #2
	add		$8, r12
	vshr.u32	W_TMP2, #30
	add		$8, $9, ror #27
	ldrd	T1, T2, WK(10)
	eor 	r12, $5, $6
	veor	W_TMP, W_TMP2
	add		$7, T1
	and		r12, $9
	ror		$9, #2
	veor	W_TMP, $4
	eor		r12, $6
	add		$6, T2
	vadd.s32	W_TMP2, W_TMP, K_VALUES
	add		$7, r12
	vorr	$4, W_TMP, W_TMP
    eor 	r12, $5, $9
	add		$7, $7, $8, ror #27
    and 	r12, $8
	ror		$8, #2
    eor 	r12, $5
	add		$6, $7, ror #27
	vst1.f32	{W_TMP2}, [lr,:128]!
	add		$6, r12
	.endm

	// a super macro that combines 4 macro proliferations to further reduce pipeline stalls
	//	W_PRECALC_16_31_0_RR0_12	W20,W16,W12,W8,W4,D,E,A,B,C
	//	W_PRECALC_16_31_1_RR1	W20,W4,D,E,A,B,C
	//	W_PRECALC_16_31_2_RR0	W4,B,C,D,E,A,14
	//	W_PRECALC_16_31_3_RR1	W4,B,C,D,E,A
	.macro	SUPER_W_PRECALC_16_31_12		// W_16,W_12,W_8,W_4,W
	vext.64	$4, $0, $1, #1
	ldrd	T1, T2, WK(12)
	vext.32	W_TMP,$3,ZERO,#1
	eor 	r12, $7, $8
	veor	$4, $2						// W_8 ^ W_14	
	and		r12, $6
	veor	W_TMP, $0				// W_3 ^ W_16
	add		$9, T1
	ror		$6, #2
	eor		r12, $8
	add		$8, T2
	veor	$4, W_TMP				// W_3 ^ W_16 ^ W_8 ^ W_14
	add		$9, r12
	//ror		T2, $5, #27
    eor 	r12, $7, $6
	vext.32	W_TMP2,ZERO, $4, #1	
	add		$9, $9, $5, ror #27
	vshl.i32	W_TMP, $4, #1
    and 	r12, $5
	vshr.u32	$4, #31
    eor 	r12, $7
	vorr	W_TMP, $4
	ror		$5, #2
	vshl.i32	$4, W_TMP2, #2
	add		$8, r12
	vshr.u32	W_TMP2, #30
	add		$8, $9, ror #27
	ldrd	T1, T2, WK(14)
	eor 	r12, $5, $6
	veor	W_TMP, W_TMP2
	add		$7, T1
	and		r12, $9
	ror		$9, #2
	veor	W_TMP, $4
	eor		r12, $6
	add		$6, T2
	vadd.s32	W_TMP2, W_TMP, K_VALUES
	add		$7, r12
	vorr	$4, W_TMP, W_TMP
    eor 	r12, $5, $9
	add		$7, $7, $8, ror #27
    and 	r12, $8
	ror		$8, #2
    eor 	r12, $5
	add		$6, $7, ror #27
	vst1.f32	{W_TMP2}, [lr,:128]!
	add		$6, r12
	.endm

	// macros that combine W_PRECALC_32_79 with RR0/RR1 and F1/F2/F3 macros
	.macro	W_PRECALC_32_79_0_RR0_F1			// W_28,W_8,W_4,W
	ldr		r12, WK($9)
	eor 	T1, $6, $7
	ldr		T2, WK($9+1)
	veor		$3, $0
	and		T1, $5
	add		$8, r12
	ror		$5, #2
	eor		T1, $7
	add		$7, T2
	vext.64		W_TMP, $1, $2, #1
	add		$8, T1
	//ror		T2, $4, #27
	.endm

	.macro	W_PRECALC_32_79_0_RR0_F2	// W_28,W_8,W_4,W
	ldr		r12, WK($9)
	veor		$3, $0
	eor		T1, $5, $7
	ror		$5, #2
	add		$8, r12
	ldr		r12, WK($9+1)
	eor		T1, $6
	vext.64		W_TMP, $1, $2, #1
	add		$8, T1
	add		$7, r12
	.endm

	.macro	W_PRECALC_32_79_0_RR0_F3			// W_28,W_8,W_4,W
	ldr		r12, WK($9)
	veor		$3, $0
	orr		T1, $5, $6
	and		T2, $5, $6 
	add		$8, r12
	ldr		r12, WK($9+1)
	ror		$5, #2
	vext.64		W_TMP, $1, $2, #1
	and		T1, $7
	add		$7, r12
	orr		T1, T2
	add		$8, T1
	.endm

	.macro	W_PRECALC_32_79_1_RR1	// combined W_PRECALC_32_79_1 and RR1 to absorb some stalls
	veor	W_TMP, $0
	add		$7, $7, $3, ror #27
	veor	W_TMP, $1
	vshr.u32	$1, W_TMP, #30
	add		$6, $7, ror #27
	vshl.i32	W_TMP, #2
	$2		$3, $4, $5
	add		$6, T1
	ror		$3, #2
	.endm

	.macro	W_PRECALC_32_79_1_RR1_F1	// combined W_PRECALC_32_79_1 and RR1 to absorb some stalls
	veor	W_TMP, $0
	eor 	T1, $5, $4
	add		$7, $7, $3, ror #27
    and		T1, $3
	veor	W_TMP, $1
    eor		T1, $5
	vshr.u32	$1, W_TMP, #30
	add		$6, T1
	vshl.i32	W_TMP, #2
	add		$6, $7, ror #27
	ror		$3, #2
	.endm

	.macro	W_PRECALC_32_79_1_RR1_F2	// combined W_PRECALC_32_79_1 and RR1 to absorb some stalls
	veor	W_TMP, $0
	veor	W_TMP, $1
	eor 	T1, $3, $4
	add		$7, $7, $3, ror #27
    eor 	T1, $5
	ror		$3, #2
	add		$6, T1
	vshr.u32	$1, W_TMP, #30
	vshl.i32	W_TMP, #2
	add		$6, $7, ror #27
	.endm

	.macro	W_PRECALC_32_79_1_RR1_F3	// combined W_PRECALC_32_79_1 and RR1 to absorb some stalls
	veor	W_TMP, $0
	veor	W_TMP, $1
	add		$7, $7, $3, ror #27
	orr		T1, $3, $4
	and		r12, $4, $3
	and 	T1, $5
	add		$6, $7, ror #27
	orr		T1, r12
	vshr.u32	$1, W_TMP, #30
	vshl.i32	W_TMP, #2
	ror		$3, #2
	add		$6, T1
	.endm

	.macro	W_PRECALC_32_79_2_RR0_F1	// W
	ldr		r12, WK($6)
	eor 	T1, $3, $4
	ldr		T2, WK($6+1)
	and		T1, $2
	add		$5, r12
	ror		$2, #2
	eor		T1, $4
	add		$4, T2
	vorr		W_TMP, $0
	add		$5, T1
	.endm

	.macro	W_PRECALC_32_79_2_RR0_F2	// W
	ldr		r12, WK($6)
	eor		T1, $2, $4
	ror		$2, #2
	eor		T1, $3
	add		$5, r12
	ldr		r12, WK($6+1)
	vorr		W_TMP, $0
	add		$5, T1
	add		$4, r12
	.endm

	.macro	W_PRECALC_32_79_2_RR0_F3	// W
	ldr		r12, WK($6)
	orr		T1, $2, $3
	and		T2, $2, $3 
	ror		$2, #2
	add		$5, r12
	ldr		r12, WK($6+1)
	and		T1, $4
	vorr		W_TMP, $0
	orr		T1, T2
	add		$4, r12
	add		$5, T1
	.endm

	.macro	W_PRECALC_32_79_3_RR1_F1	// W, i, K_XMM
	vadd.s32	W_TMP2, W_TMP, K_VALUES
	add		$5, $5, $1, ror #27
    eor 	T1, $3, $2
	vorr	$0, W_TMP, W_TMP
    and 	T1, $1
	ror		$1, #2
	vst1.f32	{W_TMP2}, [lr,:128]!
	add		$4, $5, ror #27
    eor 	T1, $3
	add		$4, T1
	.endm

	.macro	W_PRECALC_32_79_3_RR1_F2	// W, i, K_XMM
	vadd.s32	W_TMP2, W_TMP, K_VALUES
	vorr	$0, W_TMP, W_TMP
	add		$5, $5, $1, ror #27
	eor		T1, $1, $2
	ror		$1, #2
	eor		T1, $3
	vst1.f32	{W_TMP2}, [lr,:128]!
	add		$4, T1
	add		$4, $5, ror #27
	.endm

	.macro	W_PRECALC_32_79_3_RR1_F3	// W, i, K_XMM
	vadd.s32	W_TMP2, W_TMP, K_VALUES
	vorr	$0, W_TMP, W_TMP
	orr		T1, $1, $2
	add		$5, $5, $1, ror #27
	and		T1, $3
	and		r12, $1, $2
	add		$4, $5, ror #27
	orr		T1, r12
	vst1.f32	{W_TMP2}, [lr,:128]!
	ror		$1, #2
	add		$4, T1
	.endm


	.macro	LOAD_HASH
	ldr			A, [HASH_PTR, #0]
	ldrd		B,C, [HASH_PTR, #4]
	ldrd		D,E, [HASH_PTR, #12]
	.endm



	// the main code body --- the final optimized version
#if OPTIMIZED

	// rounds 16-31 use the approach by Dean Gaudet
	.macro	INTERNAL
	// i=16
	// circular buffer : W12,W8,W4,W0,W28,W24,W20,W16
#if 1
	SUPER_W_PRECALC_16_31_0 W0,W28,W24,W20,W16,A,B,C,D,E
#else
	W_PRECALC_16_31_0_RR0_0	W0,W28,W24,W20,W16,A,B,C,D,E
	W_PRECALC_16_31_1_RR1	W0,W16,A,B,C,D,E
	W_PRECALC_16_31_2_RR0	W16,D,E,A,B,C,2 
	W_PRECALC_16_31_3_RR1	W16,D,E,A,B,C
#endif

	vld1.s32	{K_VALUES}, [K_BASE,:128]!

	// i=20,
	// W8,W4,W0,W28,W24,W20,W16,W12
#if 1
	SUPER_W_PRECALC_16_31_4	W28,W24,W20,W16,W12,B,C,D,E,A
#else
	W_PRECALC_16_31_0_RR0_4	W28,W24,W20,W16,W12,B,C,D,E,A
	W_PRECALC_16_31_1_RR1	W28,W12,B,C,D,E,A
	W_PRECALC_16_31_2_RR0	W12,E,A,B,C,D,6
	W_PRECALC_16_31_3_RR1	W12,E,A,B,C,D
#endif

	// i=24,
	// W4,W0,W28,W24,W20,W16,W12,W8
#if 1
	SUPER_W_PRECALC_16_31_8	W24,W20,W16,W12,W8,C,D,E,A,B
#else
	W_PRECALC_16_31_0_RR0_8	W24,W20,W16,W12,W8,C,D,E,A,B
	W_PRECALC_16_31_1_RR1	W24,W8,C,D,E,A,B
	W_PRECALC_16_31_2_RR0	W8,A,B,C,D,E,10
	W_PRECALC_16_31_3_RR1	W8,A,B,C,D,E
#endif

	// i=28
	// W0,W28,W24,W20,W16,W12,W8,W4
#if 1
	SUPER_W_PRECALC_16_31_12	W20,W16,W12,W8,W4,D,E,A,B,C
#else
	W_PRECALC_16_31_0_RR0_12	W20,W16,W12,W8,W4,D,E,A,B,C
	W_PRECALC_16_31_1_RR1	W20,W4,D,E,A,B,C
	W_PRECALC_16_31_2_RR0	W4,B,C,D,E,A,14
	W_PRECALC_16_31_3_RR1	W4,B,C,D,E,A
#endif

	sub		lr, #0x40

	//i=32
	// W28,W24,W20,W16,W12,W8,W4,W0
	W_PRECALC_32_79_0_RR0_F1	W28,W8,W4,W0,E,A,B,C,D,16
	W_PRECALC_32_79_1_RR1_F1	W16,W0,F1,E,A,B,C,D,16
	W_PRECALC_32_79_2_RR0_F1	W0,C,D,E,A,B,18
	W_PRECALC_32_79_3_RR1_F1	W0,C,D,E,A,B

	//i=36
	// W24,W20,W16,W12,W8,W4,W0,W28
	W_PRECALC_32_79_0_RR0_F2	W24,W4,W0,W28,A,B,C,D,E,20
	W_PRECALC_32_79_1_RR1_F2	W12,W28,F2,A,B,C,D,E,20
	W_PRECALC_32_79_2_RR0_F2	W28,D,E,A,B,C,22
	W_PRECALC_32_79_3_RR1_F2	W28,D,E,A,B,C

	vld1.s32    {K_VALUES}, [K_BASE,:128]!

	//i=40
	// W20,W16,W12,W8,W4,W0,W28,W24	
	W_PRECALC_32_79_0_RR0_F2	W20,W0,W28,W24,B,C,D,E,A,24
	W_PRECALC_32_79_1_RR1_F2	W8,W24,F2,B,C,D,E,A,24
	W_PRECALC_32_79_2_RR0_F2	W24,E,A,B,C,D,26
	W_PRECALC_32_79_3_RR1_F2	W24,E,A,B,C,D

	//i=44
	// W16,W12,W8,W4,W0,W28,W24,W20
	W_PRECALC_32_79_0_RR0_F2	W16,W28,W24,W20,C,D,E,A,B,28
	W_PRECALC_32_79_1_RR1_F2	W4,W20,F2,C,D,E,A,B,28
	W_PRECALC_32_79_2_RR0_F2	W20,A,B,C,D,E,30
	W_PRECALC_32_79_3_RR1_F2	W20,A,B,C,D,E

	sub		lr, #0x40

	//i=48
	// W12,W8,W4,W0,W28,W24,W20,W16
	W_PRECALC_32_79_0_RR0_F2	W12,W24,W20,W16,D,E,A,B,C,32
	W_PRECALC_32_79_1_RR1_F2	W0,W16,F2,D,E,A,B,C,32
	W_PRECALC_32_79_2_RR0_F2	W16,B,C,D,E,A,34
	W_PRECALC_32_79_3_RR1_F2	W16,B,C,D,E,A

	//i=52
	// W8,W4,W0,W28,W24,W20,W16,W12
	W_PRECALC_32_79_0_RR0_F2	W8,W20,W16,W12,E,A,B,C,D,36
	W_PRECALC_32_79_1_RR1_F2	W28,W12,F2,E,A,B,C,D,36
	W_PRECALC_32_79_2_RR0_F2	W12,C,D,E,A,B,38
	W_PRECALC_32_79_3_RR1_F2	W12,C,D,E,A,B

	//i=56
	// W4,W0,W28,W24,W20,W16,W12,W8
	W_PRECALC_32_79_0_RR0_F3	W4,W16,W12,W8,A,B,C,D,E,40
	W_PRECALC_32_79_1_RR1_F3	W24,W8,F3,A,B,C,D,E,40
	W_PRECALC_32_79_2_RR0_F3	W8,D,E,A,B,C,42
	W_PRECALC_32_79_3_RR1_F3	W8,D,E,A,B,C

	vld1.s32    {K_VALUES}, [K_BASE,:128]!

	//i=60
	// W0,W28,W24,W20,W16,W12,W8,W4
	W_PRECALC_32_79_0_RR0_F3	W0,W12,W8,W4,B,C,D,E,A,44
	W_PRECALC_32_79_1_RR1_F3	W20,W4,F3,B,C,D,E,A,44
	W_PRECALC_32_79_2_RR0_F3	W4,E,A,B,C,D,46
	W_PRECALC_32_79_3_RR1_F3	W4,E,A,B,C,D

	sub		lr, #0x40

	//i=64
	// W28,W24,W20,W16,W12,W8,W4,W0
	W_PRECALC_32_79_0_RR0_F3	W28,W8,W4,W0,C,D,E,A,B,48
	W_PRECALC_32_79_1_RR1_F3	W16,W0,F3,C,D,E,A,B,48
	W_PRECALC_32_79_2_RR0_F3	W0,A,B,C,D,E,50
	W_PRECALC_32_79_3_RR1_F3	W0,A,B,C,D,E

	//i=68
	// W24,W20,W16,W12,W8,W4,W0,W28
	W_PRECALC_32_79_0_RR0_F3	W24,W4,W0,W28,D,E,A,B,C,52
	W_PRECALC_32_79_1_RR1_F3	W12,W28,F3,D,E,A,B,C,52
	W_PRECALC_32_79_2_RR0_F3	W28,B,C,D,E,A,54
	W_PRECALC_32_79_3_RR1_F3	W28,B,C,D,E,A

	//i=72
	// W20,W16,W12,W8,W4,W0,W28,W24
	W_PRECALC_32_79_0_RR0_F3	W20,W0,W28,W24,E,A,B,C,D,56
	W_PRECALC_32_79_1_RR1_F3	W8,W24,F3,E,A,B,C,D,56
	W_PRECALC_32_79_2_RR0_F3	W24,C,D,E,A,B,58
	W_PRECALC_32_79_3_RR1_F3	W24,C,D,E,A,B

	// starting using F4	

	//i=76
	// W16,W12,W8,W4,W0,W28,W24,W20
	W_PRECALC_32_79_0_RR0_F2	W16,W28,W24,W20,A,B,C,D,E,60
	W_PRECALC_32_79_1_RR1_F2	W4,W20,F4,A,B,C,D,E,60
	W_PRECALC_32_79_2_RR0_F2	W20,D,E,A,B,C,62
	W_PRECALC_32_79_3_RR1_F2	W20,D,E,A,B,C

	sub		K_BASE, #64
	sub		lr, #0x40
	.endm

#endif		// OPTIMIZED

	.macro	SOFTWARE_PIPELINING

	vld1.s32    {K_VALUES}, [K_BASE,:128]!

#if !OPTIMIZED
	// i=0
	// circular buffer : W28,W24,W20,W16,W12,W8,W4,W0
	W_PRECALC_00_15_0	0					// W_TMP = (BUFFER_PTR)
	RR0_F2				F4,B,C,D,E,A,64
	W_PRECALC_00_15_1	W0					// convert W_TMP to big-endian, and save W0 = W[0] = W_TMP	
	RR1_F2				F4,B,C,D,E,A,64
	W_PRECALC_00_15_2	W0					// W_TMP = W[0] + K[0]
	RR0_F2				F4,E,A,B,C,D,66
	W_PRECALC_00_15_3						// (sp) = W_TMP = W[0] + K[0]
	RR1_F2				F4,E,A,B,C,D,66

	// i=4
	// circular buffer : W24,W20,W16,W12,W8,W4,W0,W28
	W_PRECALC_00_15_0	4					// W_TMP = 16(BUFFER_PTR)
	RR0_F2				F4,C,D,E,A,B,68
	W_PRECALC_00_15_1	W28					// convert W_TMP to big-endian, and save W28 = W[0] = W_TMP	
	RR1_F2				F4,C,D,E,A,B,68
	W_PRECALC_00_15_2	W28					// W_TMP = W[0] + K[0]
	RR0_F2				F4,A,B,C,D,E,70
	W_PRECALC_00_15_3						// 16(sp) = W_TMP = W[0] + K[0]
	RR1_F2				F4,A,B,C,D,E,70

	// i=8
	// circular buffer : W20,W16,W12,W8,W4,W0,W28,W24
	W_PRECALC_00_15_0	8					// W_TMP = 32(BUFFER_PTR)
	RR0_F2				F4,D,E,A,B,C,72
	W_PRECALC_00_15_1	W24					// convert W_TMP to big-endian, and save W24 = W[0] = W_TMP
	RR1_F2				F4,D,E,A,B,C,72
	W_PRECALC_00_15_2	W24					// W_TMP = W[0] + K[0]
	RR0_F2				F4,B,C,D,E,A,74
	W_PRECALC_00_15_3						// 32(sp) = W_TMP = W[0] + K[0]
	RR1_F2				F4,B,C,D,E,A,74

	// i=12
	// circular buffer : W16,W12,W8,W4,W0,W28,W24,W20
	W_PRECALC_00_15_0	12					// W_TMP = 48(BUFFER_PTR)
	RR0_F2				F4,E,A,B,C,D,76
	W_PRECALC_00_15_1	W20					// convert W_TMP to big-endian, and save W20 = W[0] = W_TMP	
	RR1_F2				F4,E,A,B,C,D,76
	W_PRECALC_00_15_2	W20					// W_TMP = W[0] + K[0]
	RR0_F2				F4,C,D,E,A,B,78
	W_PRECALC_00_15_3						// 48(sp) = W_TMP = W[0] + K[0]
	RR1_F2				F4,C,D,E,A,B,78
#else
	// i=0
	// circular buffer : W28,W24,W20,W16,W12,W8,W4,W0

	RR0_F2				F4,B,C,D,E,A,64
	vld1.f32 {W_TMP,W_TMP2},[BUFFER_PTR]!
	RR1_F2				F4,B,C,D,E,A,64
	vld1.f32 {W4,W8},[BUFFER_PTR]!
	RR0_F2				F4,E,A,B,C,D,66
	vrev32.8 W0, W_TMP
	RR1_F2				F4,E,A,B,C,D,66

	// i=4
	// circular buffer : W24,W20,W16,W12,W8,W4,W0,W28
	RR0_F2				F4,C,D,E,A,B,68
	vrev32.8 W28, W_TMP2
	RR1_F2				F4,C,D,E,A,B,68
	vrev32.8 W24, W4
	RR0_F2				F4,A,B,C,D,E,70
	vrev32.8 W20, W8
	RR1_F2				F4,A,B,C,D,E,70

	// i=8
	// circular buffer : W20,W16,W12,W8,W4,W0,W28,W24
	RR0_F2				F4,D,E,A,B,C,72
	vadd.s32	W_TMP, W0, K_VALUES
	RR1_F2				F4,D,E,A,B,C,72
	vadd.s32	W_TMP2, W28, K_VALUES
	RR0_F2				F4,B,C,D,E,A,74
	vadd.s32	W4, W24, K_VALUES
	RR1_F2				F4,B,C,D,E,A,74

	// i=12
	// circular buffer : W16,W12,W8,W4,W0,W28,W24,W20
	RR0_F2				F4,E,A,B,C,D,76
	vadd.s32	W8, W20, K_VALUES
	RR1_F2				F4,E,A,B,C,D,76
	vst1.s32	{W_TMP,W_TMP2}, [lr,:128]!	// save W[i]+K(i) in stack memory
	RR0_F2				F4,C,D,E,A,B,78
	vst1.s32	{W4,W8}, [lr,:128]!	// save W[i]+K(i) in stack memory
	RR1_F2				F4,C,D,E,A,B,78
#endif

	sub		lr, #0x40
	.endm

	.macro	ENDING
	//i=80
	RR0_F2				F4,B,C,D,E,A,64
	RR1_F2				F4,B,C,D,E,A,64
	RR0_F2				F4,E,A,B,C,D,66
	RR1_F2				F4,E,A,B,C,D,66

	//i=84
	RR0_F2				F4,C,D,E,A,B,68
	RR1_F2				F4,C,D,E,A,B,68
	RR0_F2				F4,A,B,C,D,E,70
	RR1_F2				F4,A,B,C,D,E,70

	//i=88
	RR0_F2				F4,D,E,A,B,C,72
	RR1_F2				F4,D,E,A,B,C,72
	RR0_F2				F4,B,C,D,E,A,74
	RR1_F2				F4,B,C,D,E,A,74

	//i=92
	RR0_F2				F4,E,A,B,C,D,76
	RR1_F2				F4,E,A,B,C,D,76
	RR0_F2				F4,C,D,E,A,B,78
	RR1_F2				F4,C,D,E,A,B,78
	.endm

	.macro UPDATE_ALL_HASH	
	ldrd				T1, T2, [HASH_PTR,#0]
	add					A, T1
	add					B, T2
	str					A, [HASH_PTR,#0]
	ldrd				T1, T2, [HASH_PTR,#8]
	add					C, T1
	add					D, T2
	strd				B, C, [HASH_PTR,#4]
	ldr					T1, [HASH_PTR,#16]
	add					E, T1
	strd				D, E, [HASH_PTR,#12]
	.endm

	.macro  SHA1_PIPELINED_MAIN_BODY
	LOAD_HASH						// load initial hashes into A,B,C,D,E
	orr		lr, sp, sp
	INITIAL_W_PRECALC
	sub		lr, #0x40
0:
	INTERNAL
#if Multiple_Blocks
	subs num_blocks, #1					// pre-decrement num_blocks by 1
	ble	1f							// if num_blocks <= 0, branch to finish off
	SOFTWARE_PIPELINING
	UPDATE_ALL_HASH
	b	0b
1:
#endif
	ENDING
	UPDATE_ALL_HASH
	.endm

	.text

	.p2align	4

#define K1 0x5a827999
#define K2 0x6ed9eba1
#define K3 0x8f1bbcdc
#define K4 0xca62c1d6

K_XMM_AR:
    .long	K1
	.long	K1
	.long	K1
	.long	K1
    .long	K2
	.long	K2
	.long	K2
	.long	K2
    .long	K3
	.long	K3
	.long	K3
	.long	K3
    .long	K4
	.long	K4
	.long	K4
	.long	K4


	.globl _sha1_vng_armv7neon_compress
	.private_extern _sha1_vng_armv7neon_compress	
_sha1_vng_armv7neon_compress:

	// due to the change of order in the 2nd and 3rd calling argument, 
	// we need to switch r1/r2 to use the original code 
	// cclee 1-13-11
	mov		r12, r1
	mov		r1, r2
	mov		r2, r12

	push	{r4-r7,lr}
	nop
	add		r7, sp, #12		// set up base pointer for debug tracing
	push	{r8-r11}

	// align sp to 16-byte boundary
	ands	r12, sp, #15	// number of bytes to align to 16-byte boundary
	addeq	r12, #16		// in case it's already 16-byte aligned and hence no where to store num_aligned_bytes
	sub		sp, r12
	str		r12, [sp]

#if	KERNEL
	vpush	{q8-q15}
#endif
	vpush	{q4-q7}
#define	stack_size (16*4)		// circular buffer W0-W3
	sub		sp, #stack_size

	veor	ZERO, ZERO

	orr		HASH_PTR, ctx, ctx
	orr		BUFFER_PTR, buf, buf
	adr		K_BASE, K_XMM_AR

	SHA1_PIPELINED_MAIN_BODY

	
	add		sp, #stack_size
	vpop	{q4-q5}
	vpop	{q6-q7}
#if	KERNEL
	vpop	{q8-q9}
	vpop	{q10-q11}
	vpop	{q12-q13}
	vpop	{q14-q15}
#endif

	// restore sp to its original alignment
	ldr		r12, [sp]
	add		sp, r12

	pop		{r8-r11}
	pop		{r4-r7,pc}

#endif	/* _ARM_ARCH_7 */

