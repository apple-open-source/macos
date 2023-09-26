#if defined(VEC_OPTIMIZE) && defined(__arm64__)

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

	similarly, 2 DO16 = DO32 vectorization:
	given initial unsigned int sum2 and adler, and a new set of 32 input bytes (x[0:31]), it can be shown that
	sum2  += (32*adler + 32*x[0] + 31*x[1] + ... + 1*x[31]);
	adler += (x[0] + x[1] + ... + x[31]);

	NMAX = 5552 
	NMAX/16 = 347 = 173*2 + 1

	therefore, for a block of 5552 bytes

		n = 173;
		do {
			DO32(buf); buf+=32;
		} while (n--);

		DO16(buf); buf+=16;
		MOD(adler);
		MOD(sum2);

	for a residual remaining len bytes,
		while (len >= 32) {
			DO32(buf); buf += 32; len -= 32;
        }
		if (len>=16) {
			DO16(buf); buf += 16; len -= 16;
		}
        while (len--) {
            adler += *buf++;
            sum2 += adler;
        }
        MOD(adler);
        MOD(sum2);

		


	DO32:
	pack sum2:adler in a 64-bit register

	0. sum2 += adler*32;	

		sum2:adler += (sum2:adler) << (32+5);

	1. (32, 31, ..., 1) * ( x0, x1, ..., x31)	(v2, v3) * (v0, v1)
		umull.8h	v4, v2, v0
		umull2.8h	v4, v2, v0
		umull.8h	v4, v3, v1
		umull2.8h	v4, v3, v1
		uaddlv		s4, v4.8h

		add		sum2, sum2, s4

	2.	adler += (x0 + x1 + x2 + ... + x31)

		uaddlp		v0.8h, v0.16b 
		uaddlp		v1.8h, v1.16b 
		uaddlv		s0, v0.8h
		uaddlv		s1, v1.8h
		add			s0, s0, s1		

		add		adler, adler, s0
	

	therefore, this is what can be done to vectorize the above computation
	1. 16-byte aligned vector load into q2 (x[0:x15])
	2. sum2 += (adler<<32);				// pack adler/sum2 into a 64-bit word
	3. vmull.u8 (q9,q8),q2,d2 where d2 = (1,1,1,1...,1), (q9,q8) + 16 16-bit elements x[0:15]
	4. vmull.u8 (q11,q10),q2,q0 where q0 = (1,2,3,4...,16), (q11,q10) + 16 16-bit elements (16:1)*x[0:15]
	5. parallel add (with once expansion to 32-bit) (q9,q8) and (q11,q10) all the way to accumulate to adler and sum2 

	In this revision, whenever possible, 2 DO16 loops are combined into a DO32 loop.
	1. 32-byte aligned vector load into q2,q14 (x[0:x31])
    2. sum2 += (adler<<32);
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


	.text
	.align 2
	.globl _adler32_vec
_adler32_vec:
 


	#define	adler	w0
	#define	sum2	w1
	#define	buf		x2
	#define	len		w3	
	#define	nmax	w4
	#define	nvec	w6				// vecs = NMAX/16

	#define	t		x7

	#define	vadlersum2		v5			// only lower part 
	#define	adlersum2		d5			// only lower part 

	add		x0, x0, x1, lsl #32
	adrp    t, vec_table@PAGE
	mov			nmax, #NMAX					// NMAX
    add     t, t, vec_table@PAGEOFF
	ins		vadlersum2.d[0], x0


#if	KERNEL
	sub		x6, sp, #8*16	
	sub		sp, sp, #8*16	
	st1.4s	{v0,v1,v2,v3},[x6],#4*16
	st1.4s	{v4,v5,v6,v7},[x6],#4*16
#endif

	cmp			len, nmax					// len vs NMAX
	ld1.4s		{v0, v1}, [t], #2*16 		// loading up coefficients for adler/sum2 computation
	ldr			d7, [t]						// for MOD operation
	b.lt		L_len_lessthan_NMAX			// if (len < NMAX) skip the while loop		

	sub			len, len, nmax				// pre-decrement len by NMAX

L_while_len_ge_NMAX_loop: 					// while (len>=NMAX) {


	// 5552/16 = 173*2 + 1, need 1 DO16 + 173 DO32

	.macro	DO16
	ld1.4s	{v2},[buf]	 					// 16 bytes input
	shl		d6, adlersum2, #(4+32)			// adler * 16 
	umull.8h    v4, v2, v1					// 16*x0, 15*x1, ..., 9*x7 
	add		adlersum2, adlersum2, d6		// sum2 += adler * 16
    umlal2.8h   v4, v2, v1					// 8*x8, 7*x9, ..., 1*x15 
    uaddlv      h2, v2.16b					// x0+x1+x2+...+x15
    uaddlv      s4, v4.8h					// 16*x0 + 15*x1 + ... + 1*x15
	zip1.2s		v4, v2, v4
	add			buf, buf, #16
	add.2s		vadlersum2, vadlersum2, v4
	.endm

	.macro	DO32
	ld1.4s		{v2,v3},[buf]					// 32 bytes input
	shl			d6, adlersum2, #(5+32)			// adler * 32 
	umull.8h    v4, v2, v0						// 32*x0, 31*x1, ..., 25*x7 
	add			adlersum2, adlersum2, d6		// sum2 += adler * 32 
    umlal2.8h   v4, v2, v0						// accumulate 24*x8, 23*x9, ..., 17*x15 
    uaddlv      h2, v2.16b						// x0+x1+x2+...+x15 and extend from byte to short
	umlal.8h    v4, v3, v1						// accumulate 16*x16, 15*x17, ..., 9*x23 
    uaddlv      h6, v3.16b						// x16+x17+x18+...+x31 and extend from byte to short
    umlal2.8h   v4, v3, v1						// accumulate 8*x24, 7*x25, ..., 1*x31 
	uaddl.4s	v2, v2, v6						// x0+x1+...+x31 and extend from short to int
	uaddlv		s4, v4.8h						// 32*x0 + 31*x1 + ... + 1*x31
	zip1.2s		v4, v2, v4
	add			buf, buf, #32
	add.2s		vadlersum2, vadlersum2, v4
	.endm

	.macro	MOD_BASE
	umull.2d	v2,vadlersum2,v7[1]
	ushr.2d		v2, v2, #47
	ins			v2.s[1], v2.s[2]
	mls.2s		vadlersum2,v2,v7[0]
	.endm

	DO16
	mov			nvec, #173						// number of DO32 loops

0:
	DO32

	subs		nvec, nvec, #1	
	b.gt		0b

	MOD_BASE	 // MOD(adler,sum2);
	
	subs		len, len, nmax				// pre-decrement len by NMAX
	b.ge		L_while_len_ge_NMAX_loop	// repeat while len >= NMAX

	adds		len, len, nmax				// post-increment len by NMAX

L_len_lessthan_NMAX:

	subs		len, len, #32					// pre-decrement len by 32
	b.lt		L_len_lessthan_32				// if len < 32, branch to len16_loop 

L_len32_loop:

	DO32

	subs		len, len, #32					// len -= 32; 
	b.ge		L_len32_loop

L_len_lessthan_32:

	tst			len, #16
	b.eq		1f

	DO16

1:
	ands		len, len, #15
	b.eq		L_len_is_zero


L_remaining_len_loop:
	ldrb		w0, [buf], #1				// *buf++;
	sub			len, len, #1
	ins			v2.d[0], x0
	
	add			adlersum2, adlersum2, d2	// adler += *buf
	shl			d6, adlersum2, #32			// shift adler up to sum2 position 
	add			adlersum2, adlersum2, d6	// sum2 += adler
	cbnz		len, L_remaining_len_loop	// break if len<=0


L_len_is_zero:


	MOD_BASE 	// mod(alder,BASE); mod(sum2,BASE);

	umov		w0, vadlersum2.s[0]			// adler
	umov		w1, vadlersum2.s[1]			// sum2
	add			w0, w0, w1, lsl #16	// to return adler | (sum2 << 16);

#if	KERNEL
	ld1.4s	{v0,v1,v2,v3},[sp],#4*16
	ld1.4s	{v4,v5,v6,v7},[sp],#4*16
#endif

	ret			lr

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

	

#endif		// VEC_OPTIMIZE && __arm64__
