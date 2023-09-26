#if defined(VEC_OPTIMIZE)

#ifdef __arm__

#include <arm/arch.h>
#define KERNEL_SUPPORT_NEON 1   // this is for building dylib, defined so it will build with neon for armv7
#define BASE 65521	    /* largest prime smaller than 65536 */
#define NMAX 5552 		/* NMAX is the largest n such that 255n(n+1)/2 + (n+1)(BASE-1) <= 2^32-1 */

// Note: buf should have been 16-byte aligned in the caller function,

// uLong adler32_vec(unsigned int adler, unsigned int sum2, const Bytef* buf, int len) {
//    unsigned n;
//    while (len >= NMAX) {
//        len -= NMAX;
//        n = NMAX / 16;          /* NMAX is divisible by 16 */
//        do {
//            DO16(buf);          /* 16 sums unrolled */
//            buf += 16;
//        } while (--n);
//        MOD(adler);
//        MOD(sum2);
//    }
//    if (len) {                  /* avoid modulos if none remaining */
//        while (len >= 16) {
//            len -= 16;
//            DO16(buf);
//            buf += 16;
//        }
//        while (len--) {
//            adler += *buf++;
//            sum2 += adler;
//        }
//        MOD(adler);
//        MOD(sum2);
//    }
//    return adler | (sum2 << 16); 		/* return recombined sums */
// }


/* 
	DO16 vectorization:
	given initial unsigned int sum2 and adler, and a new set of 16 input bytes (x[0:15]), it can be shown that
	sum2  += (16*adler + 16*x[0] + 15*x[1] + ... + 1*x[15]);
	adler += (x[0] + x[1] + ... + x[15]);

	therefore, this is what can be done to vectorize the above computation
	1. 16-byte aligned vector load into q2 (x[0:x15])
	2. sum2 += (adler<<4);
	3. vmull.u8 (q9,q8),q2,d2 where d2 = (1,1,1,1...,1), (q9,q8) + 16 16-bit elements x[0:15]
	4. vmull.u8 (q11,q10),q2,q0 where q0 = (1,2,3,4...,16), (q11,q10) + 16 16-bit elements (16:1)*x[0:15]
	5. parallel add (with once expansion to 32-bit) (q9,q8) and (q11,q10) all the way to accumulate to adler and sum2 

	In this revision, whenever possible, 2 DO16 loops are combined into a DO32 loop.
	1. 32-byte aligned vector load into q2,q14 (x[0:x31])
    2. sum2 += (adler<<5);
    3. vmull.u8 (4 q registers),(q2,q14),d2 where d2 = (1,1,1,1...,1), (4 q registers) : 32 16-bit elements x[0:31]
	4. vmull.u8 (4 q registers),(q2,q14),(q0,q15) where q0 = (1,...,32), (4 q regs) : 32 16-bit elements (32:1)*x[0:31]
    5. parallel add (with once expansion to 32-bit) the pair of (4 q regs) all the way to accumulate to adler and sum2 

	This change improves the performance by ~ 0.55 cycle/uncompress byte on ARM Cortex-A8.

*/

/*
	MOD implementation:
	adler%BASE = adler - floor(adler*(1/BASE))*BASE; where (1/BASE) = 0x80078071 in Q47
	1. vmull.u32   q2,(adler,sum2),(1/BASE)		// *(1/BASE) in Q47
    2. vshr.u64    q2,q2,#47					// floor function
    3. vpadd.u32   d4,d4,d5						// merge into a double word in d4
    4. vmls.u32    (adler,sum2),d4,d3[0]        // (adler,sum2) -= floor[(adler,sum2)/BASE]*BASE
	 
*/

#if defined _ARM_ARCH_6			// this file would be used only for armv6 or above


	.text
	.align 2
	.globl _adler32_vec
_adler32_vec:
 
#if (!KERNEL_SUPPORT_NEON) || (!defined _ARM_ARCH_7)	// for armv6 or armv7 without neon support


	#define	adler			r0
	#define	sum2			r1
	#define	buf				r2
	#define	len				r3	
	#define	one_by_base		r4
	#define	base			r5
	#define nmax			r6
	#define	t				r12
	#define	vecs			lr
	#define	x0				r8
	#define	x1				r10
	#define	x2				r11
	#define	x3				r12
	#define	zero			r9

	// this macro performs adler/sum2 update for 4 input bytes

	.macro DO4
	add		sum2, adler, lsl #2				// sum2 += 4*adler;
	ldr		x0,[buf]						// 4 bytes in 1 32-bit word
	usada8	adler, x0, zero, adler			// adler += sum(x0:x3)
	ldrb	x0,[buf], #4					// x0
	ldrb	x2,[buf,#-2]					// x2
	ldrb	x1,[buf,#-3]					// x1
	ldrb	x3,[buf,#-1]					// x3
	add		sum2, x0, lsl #2				// sum2 += 4*x0
	add		x3, x3, x1, lsl #1				// x3+2*x1
	add		sum2, x2, lsl #1				// sum2 += 2*x2
	add		x3, x1							// x3+3*x1
	add		sum2, x3						// sum2 += x3+3*x1
	.endm

	// the following macro cascades 4 DO4 into a adler/sum2 update for 16 bytes
	.macro DO16
	DO4										// adler/sum2 update for 4 input bytes
	DO4										// adler/sum2 update for 4 input bytes
	DO4										// adler/sum2 update for 4 input bytes
	DO4										// adler/sum2 update for 4 input bytes
	.endm

	// the following macro performs adler sum2 modulo BASE
	.macro	modulo_base
	umull	x0,x1,adler,one_by_base			// adler/BASE in Q47
	umull	x2,x3,sum2,one_by_base			// sum2/BASE in Q47
	lsr		x1, #15							// x1 >> 15 = floor(adler/BASE)
	lsr		x3, #15							// x3 >> 15 = floor(sum2/BASE)
	mla		adler, x1, base, adler			// adler %= base;
	mla		sum2, x3, base, sum2			// sum2 %= base;
	.endm

	adr		t, coeffs	
	push	{r4-r6, r8-r11, lr}
	ldmia	t, {one_by_base, base, nmax}	// load up coefficients

	subs        len, nmax                   // pre-subtract len by NMAX
	eor			zero, zero					// a dummy zero register to use usada8 instruction
    blt         len_lessthan_NMAX           // if (len < NMAX) skip the while loop     

while_lengenmax_loop:						// do {
    lsr         vecs, nmax, #4              // vecs = NMAX/16;

len16_loop:									// do {

	DO16

	subs	vecs, #1						// vecs--;
	bgt			len16_loop					// } while (vec>0);	

	modulo_base								// adler sum2 modulo BASE

	subs		len, nmax					// len -= NMAX
	bge			while_lengenmax_loop		// } while (len >= NMAX);

len_lessthan_NMAX:
	adds		len, nmax					// post-subtract len by NMAX

	subs		len, #16					// pre-decrement len by 16
	blt			len_lessthan_16

len16_loop2:

	DO16

	subs		len, #16
	bge			len16_loop2

len_lessthan_16:
	adds		len, #16					// post-increment len by 16
	beq			len_is_zero

remaining_buf:
	ldrb		x0, [buf], #1
	subs		len, #1
	add			adler, x0
	add			sum2, adler
	bgt			remaining_buf

len_is_zero:

	modulo_base 							// adler sum2 modulo BASE

	add		r0, adler, sum2, lsl #16		// to return sum2<<16 | adler 

	pop		{r4-r6, r8-r11, pc}

	.align 2
coeffs:
	.long	-2146992015
	.long	-BASE
	.long	NMAX

#else	// KERNEL_SUPPORT_NEON


#if defined _ARM_ARCH_7 

	#define	adler	r0
	#define	sum2	r1
	#define	buf		r2
	#define	len		r3	
	#define	nmax	r4
	#define	nvec	r6				// vecs = NMAX/16
	#define	n		r5
	#define	t		r12

	#define	adlersum2		d10

	adr			t, vec_table				// address to vec_table[]
	stmfd		sp!, {r4, r5, r6, lr}
	ldr			nmax, [t, #40]				// NMAX

#if	KERNEL
	vpush	{q12-q15}
	vpush	{q8-q9}
	vpush	{q0-q3}
#endif
	vpush	{q4-q5}

	vmov		adlersum2, adler, sum2		// pack up adler/sum2 into a double register 
	cmp			len, nmax					// len vs NMAX
	vld1.32		{q8-q9},[t,:128]!			// loading up coefficients for adler/sum2 computation
	vldr		d8,[t]						// for sum2 computation
	blt			L_len_lessthan_NMAX			// if (len < NMAX) skip the while loop		

	sub			len, len, nmax					// pre-decrement len by NMAX

L_while_len_ge_NMAX_loop: 					// while (len>=NMAX) {

	// DO16
	vld1.32		{q0},[buf,:128]!			// 16 bytes input
	vshl.u64	d11, adlersum2, #(4+32)		// adler*16
	vmull.u8	q12, d0, d18				// 16*x0, 15*x1, ..., 9*x7
	vmull.u8	q13, d1, d19				// 8*x8, 7*x9, ..., 1*x15
	vpaddl.u8	q0, q0						// x0+x1, x2+x3, ... x14+x15
	vadd.i64	adlersum2, adlersum2, d11	// sum2 += adler * 16
	vadd.i16	q1, q12, q13				// 16*x0+8*x8, ..., 9*x7+1*x15
	vpaddl.u16	q0, q0						// x0+x1+x2+x3, ..., x12+x13+x14+x15
	vpaddl.u16	q1, q1						// 16*x0+15*x1+14*x2+13*x3, ... , 4*x12+3*x13+2*x14+x15
	vpaddl.u32	q0, q0						// x0+...+x7,x8+...+x15
	vpaddl.u32	q1, q1						// 16*x0+15*x1+...+9*x7, 8*x8+ ... + 1*x15
	vzip.32		q0, q1						//
	mov			nvec, #173
	vadd.i64	d0, d0, d2					//

0:

	vadd.i64	adlersum2, adlersum2, d0
	vld1.32		{q0-q1},[buf,:128]!			// 32 bytes input
	vshl.u64	d11, adlersum2, #(5+32)		// adler*32
	vmull.u8	q12, d0, d16				// 32*x0, 31*x1, ..., 25*x7
	vmull.u8	q13, d1, d17				// 24*x8, 23*x9, ..., 17*x15
	vpaddl.u8	q0, q0						// x0+x1, x2+x3, ... x14+x15
	vmull.u8	q14, d2, d18				// 16*x16, 15*x17, ..., 9*x23
	vmull.u8	q15, d3, d19				// 8*x24, 7*x25, ..., 1*x31
	vpaddl.u8	q1, q1						// x16+x17, x18+x19, ... x30+x31
	vadd.i64	adlersum2, adlersum2, d11	// sum2 += adler * 32
	vadd.i16    q12, q12, q13
	vadd.i16    q14, q14, q15
	vadd.i16    q0, q0, q1
	vadd.i16    q1, q12, q14
	subs		nvec, nvec, #1
	vpaddl.u16	q0, q0						// x0+x1+x2+x3, ..., x12+x13+x14+x15
	vpaddl.u16	q1, q1						// 16*x0+15*x1+14*x2+13*x3, ... , 4*x12+3*x13+2*x14+x15
	vpaddl.u32	q0, q0						// x0+...+x7,x8+...+x15
	vpaddl.u32	q1, q1						// 16*x0+15*x1+...+9*x7, 8*x8+ ... + 1*x15
	vzip.32		q0, q1						//
	vadd.i64	d0, d0, d2					//
	bgt			0b
	vadd.i64	adlersum2, adlersum2, d0

	// mod(alder,BASE); mod(sum2,BASE);
	vmull.u32	q2,adlersum2,d8[1]			// alder/BASE, sum2/BASE in Q47
	subs		len, len, nmax					// len -= NMAX;
	vshr.u64	q2,q2,#47					// take the integer part
	vpadd.u32	d4,d4,d5					// merge into a double word in d4
	vmls.u32	adlersum2,d4,d8[0]			// (adler,sum2) -= floor[(adler,sum2)/BASE]*BASE

	bge			L_while_len_ge_NMAX_loop		// repeat while len >= NMAX

	adds		len, len, nmax					// post-increment len by NMAX

L_len_lessthan_NMAX:

	subs		len, len, #32					// pre-decrement len by 32
	blt			L_len_lessthan_32				// if len < 32, branch to len16_loop 

L_len32_loop:
	vld1.32		{q0-q1},[buf,:128]!			// 32 bytes input
	vshl.u64	d11, adlersum2, #(5+32)		// adler*32
	vmull.u8	q12, d0, d16				// 32*x0, 31*x1, ..., 25*x7
	vmull.u8	q13, d1, d17				// 24*x8, 23*x9, ..., 17*x15
	vpaddl.u8	q0, q0						// x0+x1, x2+x3, ... x14+x15
	vmull.u8	q14, d2, d18				// 16*x16, 15*x17, ..., 9*x23
	vmull.u8	q15, d3, d19				// 8*x24, 7*x25, ..., 1*x31
	vpaddl.u8	q1, q1						// x16+x17, x18+x19, ... x30+x31
	vadd.i64	adlersum2, adlersum2, d11	// sum2 += adler * 32
	vadd.i16    q12, q12, q13
	vadd.i16    q14, q14, q15
	vadd.i16    q0, q0, q1
	vadd.i16    q1, q12, q14
	subs		len, len, #32					// len -= 32; 
	vpaddl.u16	q0, q0						// x0+x1+x2+x3, ..., x12+x13+x14+x15
	vpaddl.u16	q1, q1						// 16*x0+15*x1+14*x2+13*x3, ... , 4*x12+3*x13+2*x14+x15
	vpaddl.u32	q0, q0						// x0+...+x7,x8+...+x15
	vpaddl.u32	q1, q1						// 16*x0+15*x1+...+9*x7, 8*x8+ ... + 1*x15
	vzip.32		q0, q1						//
	vadd.i64	d0, d0, d2					//
	vadd.i64	adlersum2, adlersum2, d0
	bge			L_len32_loop

L_len_lessthan_32:

	tst         len, #16
	beq			1f
	vld1.32		{q0},[buf,:128]!			// 16 bytes input
	vshl.u64	d11, adlersum2, #(4+32)		// adler*16
	vmull.u8	q12, d0, d18				// 16*x0, 15*x1, ..., 9*x7
	vmull.u8	q13, d1, d19				// 8*x8, 7*x9, ..., 1*x15
	vpaddl.u8	q0, q0						// x0+x1, x2+x3, ... x14+x15
	vadd.i64	adlersum2, adlersum2, d11	// sum2 += adler * 16
	vadd.i16	q1, q12, q13				// 16*x0+8*x8, ..., 9*x7+1*x15
	vpaddl.u16	q0, q0						// x0+x1+x2+x3, ..., x12+x13+x14+x15
	vpaddl.u16	q1, q1						// 16*x0+15*x1+14*x2+13*x3, ... , 4*x12+3*x13+2*x14+x15
	vpaddl.u32	q0, q0						// x0+...+x7,x8+...+x15
	vpaddl.u32	q1, q1						// 16*x0+15*x1+...+9*x7, 8*x8+ ... + 1*x15
	vzip.32		q0, q1						//
	vadd.i64	d0, d0, d2					//
	vadd.i64	adlersum2, adlersum2, d0
1:
	ands		len, len, #15				// post-increment len by 16
	beq			L_len_is_zero_internal		// if len==0, branch to len_is_zero_internal

	// restore adler/sum2 into general registers for remaining (<16) bytes

	vmov		adler, sum2, adlersum2
L_remaining_len_loop:
	ldrb		t, [buf], #1				// *buf++;
	subs		len, #1						// len--;
	add			adler,t						// adler += *buf
	add			sum2,adler					// sum2 += adler
	bgt			L_remaining_len_loop			// break if len<=0

	vmov		adlersum2, adler, sum2		// move to double register for modulo operation

L_len_is_zero_internal:

	// mod(alder,BASE); mod(sum2,BASE);

	vmull.u32	q2,adlersum2,d8[1]			// alder/BASE, sum2/BASE in Q47
	vshr.u64	q2,q2,#47					// take the integer part
	vpadd.u32	d4,d4,d5					// merge into a double word in d4
	vmls.u32	adlersum2,d4,d8[0]			// (adler,sum2) -= floor[(adler,sum2)/BASE]*BASE

	vmov        adler, sum2, adlersum2		// restore adler/sum2 from (s12=sum2, s13=adler)
	add			r0, adler, sum2, lsl #16	// to return adler | (sum2 << 16);

	vpop		{q4-q5}
#if	KERNEL
	vpop		{q0-q1}
	vpop		{q2-q3}
	vpop		{q8-q9}
	vpop		{q12-q13}
	vpop		{q14-q15}
#endif

	ldmfd       sp!, {r4, r5, r6, pc}			// restore registers and return 


	// constants to be loaded into q registers
	.align	4		// 16 byte aligned

vec_table:

	// coefficients for computing sum2
	.long	0x1d1e1f20		// s0
	.long	0x191a1b1c		// s1
	.long	0x15161718		// s2
	.long	0x11121314		// s3
	.long	0x0d0e0f10		// s0
	.long	0x090a0b0c		// s1
	.long	0x05060708		// s2
	.long	0x01020304		// s3

	.long	BASE			// s6 : BASE 
	.long	0x80078071		// s7 : 1/BASE in Q47

NMAX_loc:
	.long	NMAX			// NMAX
	
#endif		// _ARM_ARCH_7

#endif		//  (!KERNEL_SUPPORT_NEON) || (!defined _ARM_ARCH_7)

#endif		// _ARM_ARCH_6

#endif		// __arm__

#endif		// VEC_OPTIMIZE
