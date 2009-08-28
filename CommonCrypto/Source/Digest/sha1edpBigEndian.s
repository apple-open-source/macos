/*	sha1edpBigEndian.s -- Core of SHA-1 algorithm, process blocks with
	big-endian data.
*/


#if defined __i386__ || defined __x86_64__


#if defined __x86_64__
	#define	UseRedZone	// x86_64 may use the red zone.  i386 may not.
#endif

#include "sha1edp.h"
#if CC_SHA1_BLOCK_BYTES != 64
	#error "Expected CC_SHA1_BLOCK_BYTES to be 64."
#endif


// Provide a convenient way to conditionalize based on architecture.
#if defined __i386__
	#define	Arch(i386, x86_64)	i386
#elif defined __x86_64__
	#define	Arch(i386, x86_64)	x86_64
#endif


/*	Rename the general registers.  This makes it easier to keep track of them
	and provides names for the "whole register" that are uniform between i386
	and x86_64.
*/
#if defined __i386__
	#define	r0	%eax	// Available for any use.
	#define	r1	%ecx	// Available for any use, some special purposes (loop).
	#define	r2	%edx	// Available for any use.
	#define	r3	%ebx	// Must be preserved by called routine.
	#define	r4	%esp	// Stack pointer.
	#define	r5	%ebp	// Frame pointer, must preserve, no bare indirect.
	#define	r6	%esi	// Must be preserved by called routine.
	#define	r7	%edi	// Must be preserved by called routine.
#elif defined __x86_64__
	#define	r0	%rax	// Available for any use.
	#define	r1	%rcx	// Available for any use.
	#define	r2	%rdx	// Available for any use.
	#define	r3	%rbx	// Must be preserved by called routine.
	#define	r4	%rsp	// Stack pointer.
	#define	r5	%rbp	// Frame pointer.  Must be preserved by called routine.
	#define	r6	%rsi	// Available for any use.
	#define	r7	%rdi	// Available for any use.
	#define	r8	%r8		// Available for any use.
	#define	r9	%r9		// Available for any use.
	#define	r10	%r10	// Available for any use.
	#define	r11	%r11	// Available for any use.
	#define	r12	%r12	// Must be preserved by called routine.
	#define	r13	%r13	// Must be preserved by called routine.
	#define	r14	%r14	// Must be preserved by called routine.
	#define	r15	%r15	// Must be preserved by called routine.
#else
	#error "Unknown architecture."
#endif


// Define names for fixed-size portions of registers.

// 32 bits.
#define	r0d		%eax
#define	r1d		%ecx
#define	r2d		%edx
#define	r3d		%ebx
#define	r4d		%esp
#define	r5d		%ebp
#define	r6d		%esi
#define	r7d		%edi
#define	r8d		%r8d
#define	r9d		%r9d
#define	r10d	%r10d
#define	r11d	%r11d
#define	r12d	%r12d
#define	r13d	%r13d
#define	r14d	%r14d
#define	r15d	%r15d


	.text


/*	Routine:

		_sha1_block_asm_host_order.

	Function:

		Update SHA-1 context from whole blocks provided in big-endian order.

	Input:

		SHA_CTX *Context	// SHA-1 context structure.
		const void *Data	// Data, CC_SHA1_BLOCK_BYTES * Blocks bytes.
		int Blocks			// Number of blocks to process.  Must be positive.

	Output:

		*Context is updated.
*/
	.globl _sha1_block_asm_host_order
	.private_extern	_sha1_block_asm_host_order
_sha1_block_asm_host_order:

	// Push new stack frame.
	push	r5

	// Save registers.
	push	r3
	#if defined __i386__
		push	r6
		push	r7
		#define	SaveSize	(5*4)
	#elif defined __x86_64__
		#define	SaveSize	(3*8)
		// Add pushes of r12 to r15 if used.
	#endif

/*	SaveSize is the number of bytes of data pushed onto the stack so far,
	including the caller's return address.
*/

#if defined UseRedZone
	// No additional bytes are needed above stack for local data.
	#define	LocalsSize	0

	/*	Our local data contains an array named W of starting at offset WOffset
		from the stack pointer.  There is plenty of space in the red zone below
		the stack, so we will put the data there.  It is aligned to a multiple
		of 16 bytes because the big-endian version of this routine may use
		movaps to write to it.
	*/
	#define	WOffset	(- 16*4 - (-SaveSize & 15))
#else
	// Make space for W array in local data.
	#define	LocalsSize	(16*4 + (-SaveSize & 15))

	/*	Our local data contains an array named W of starting at offset WOffset
		from the stack pointer.  It is aligned to a multiple of 16 bytes
		because the big-endian version of this routine may use movaps to write
		to it.
	*/
	#define	WOffset	0
#endif

// W(i) references word i%16 stored in the local data area.
#define	W(i)	WOffset + ((i)%16)*4(r4)

#define	StackFrame	(LocalsSize + SaveSize)
	/*	StackFrame is the number of bytes in our stack frame, from the top of
		stack after we push registers and make space for local data to the top
		of stack immediately before the call to this routine.
	*/

	#if 0 < LocalsSize
		sub		$LocalsSize, r4	// Allocate space on stack.
	#endif

/*	t0 and t1 hold temporary values used in calculation or data motion and
	overlap Context, Data, and Blocks on i386.  A, B, C, D, and E refer to
	values used in the SHA-1 specification.  However, they are used in
	rotation, so each of them successively holds the values the specification
	refers to as the others at different times.

	Note that t0, t1, and A to E are 32-bit registers.  They are used for
	manipulating the 32-bit chunks of the SHA-1 algorithm.  Context, Data, and
	Blocks are full registers, and the first two must be since they hold
	addresses.
*/
#define	t0			r0d				// Overlaps Context.
#define	t1			r1d				// Overlaps Data and Blocks.
#define	A			Arch(r2d, r8d)
#define	B			r3d
#define	C			r5d
#define	D			Arch(r6d, r9d)
#define	E			Arch(r7d, r10d)

#define	Context		Arch(r0, r7)	// Overlaps t0.
#define	Data		Arch(r1, r6)	// Overlaps t1 and Blocks.
#define	Blocks		Arch(r1, r2)	// Overlaps t1 and Data.

#if defined __i386__

	// Define location of argument i.
	#define	Argument(i)	StackFrame+4*(i)(r4)

	#define	ArgContext	Argument(0)
	#define	ArgData		Argument(1)
	#define	ArgBlocks	Argument(2)

#endif


// Constants of the SHA-1 algorithm.
#define	Constant0	0x5a827999
#define	Constant1	0x6ed9eba1
#define	Constant2	0x8f1bbcdc
#define	Constant3	0xca62c1d6


// Define names for macro parameters, since assembler does not support them.
#define	mF	$0
#define	mB	$1
#define	mC	$2
#define	mD	$3

	/*	Calculate the function used in steps 0 to 19 of SHA-1 and store the
		result in F:

			F = (C ^ D) & B ^ D.
	*/
	.macro	F0
		mov		mC, t0
		xor		mD, t0
		and		mB, t0
		xor		mD, t0
		add		t0, mF
	.endmacro


	/*	Calculate the function used in steps 20 to 39 of SHA-1 and store the
		result in F:

			F = B ^ C ^ D.
	*/
	.macro	F1
		mov		mB, t0
		xor		mC, t0
		xor		mD, t0
		add		t0, mF
	.endmacro


	/*	Calculate the function used in steps 40 to 59 of SHA-1 and store the
		result in F:

			F = B & C | B & D | C & D.

		(A bit in F is on iff corresponding bits are on in at least two of B,
		C, and D.)
	*/
	.macro	F2
		mov		mB, t0
		and		mC, t0
		mov		mB, t1
		and		mD, t1
		or		t1, t0
		mov		mC, t1
		and		mD, t1
		or		t1, t0
		add		t0, mF
	.endmacro

	// Steps 60 to 79 use the same function as 20 to 39.
	#define	F3	F1

// Undefine parameter names.
#undef	mF
#undef	mB
#undef	mC
#undef	mD


// Define names for macro parameters, since assembler does not support them.
#define	mA			$0
#define	mB			$1
#define	mC			$2
#define	mD			$3
#define	mE			$4
#define	mFunction	$5
#define	mWord		$6
#define	mConstant	$7
	/*	Step performs most of one step of the SHA-1 algorithm:

			Add to E a word from the message schedule, a constant, A rotated
			left 5 bits, and a function of B, C, and D.

			Rotate B left 30 bits.

			Rotate values (D to E, C to D, B to C, A to B, and the new E to A).
			(This is effected by rotating the registers passed to this macro,
			rather than by actually moving the data.)

		mWord contains the word from the message schedule.  It may be in t0, so
		we need to finish with it before using t0.

		mConstant contains the constant to add.

		mA, mB, mC, mD, and mE are registers with the current values of A, B,
		C, D, and E.

		mFunction is a macro that implements the function of B, C, and D.
	*/
	.macro	Step
		add		$$mConstant, mE
		add		mWord, mE
		mov		mA, t0
		roll	$$5, t0
		add		t0, mE
		mFunction	mE, mB, mC, mD
		roll	$$30, mB
	.endmacro
// Undefine parameters names.
#undef	mA
#undef	mB
#undef	mC
#undef	mD
#undef	mE
#undef	mFunction
#undef	mWord
#undef	mConstant


	/*	Prepare a new word from the message schedule.  Parameter $0 is the
	 	index of the new word in our local table.
	*/
	.macro	PrepareWord
		mov		W($0 +  0), t0
		xor		W($0 +  2), t0
		xor		W($0 +  8), t0
		xor		W($0 + 13), t0
		roll	$$1, t0
		movl	t0, W($0 +  0)
	.endmacro


	#if defined __i386__
		mov		ArgContext, Context
	#endif

	// Load current context.
	mov		Contexth0(Context), A
	mov		Contexth1(Context), B
	mov		Contexth2(Context), C
	mov		Contexth3(Context), D
	mov		Contexth4(Context), E

/*	This loop iterates through the blocks of data.  Each iteration updates the
	SHA-1 context for one block.
*/
1:
	#if defined __i386__
		mov		ArgData, Data
	#endif

	/*	Preprocess user data and store in the local data area.  It is a shame
		we cannot overlap this with later work, but we are out of registers and
		do not want tie up a register with Data later on.

		The bytes are in the desired order within the words, so we do not have
		to reverse them as in the little-endian version of this routine, just
		copy them.
	*/
	movups  0*4(Data), %xmm0;    movaps %xmm0, W( 0)
	movups  4*4(Data), %xmm0;    movaps %xmm0, W( 4)
	movups  8*4(Data), %xmm0;    movaps %xmm0, W( 8)
	movups 12*4(Data), %xmm0;    movaps %xmm0, W(12)

	// Advance pointer to next block.
	add		$CC_SHA1_BLOCK_BYTES, Data
	#if	defined __i386__
		mov		Data, ArgData
	#endif

	// Steps 0 to 15.  Use words from already prepared message schedule.
	movl W( 0), t0;     Step A, B, C, D, E, F0, t0, Constant0
	movl W( 1), t0;     Step E, A, B, C, D, F0, t0, Constant0
	movl W( 2), t0;     Step D, E, A, B, C, F0, t0, Constant0
	movl W( 3), t0;     Step C, D, E, A, B, F0, t0, Constant0
	movl W( 4), t0;     Step B, C, D, E, A, F0, t0, Constant0

	movl W( 5), t0;     Step A, B, C, D, E, F0, t0, Constant0
	movl W( 6), t0;     Step E, A, B, C, D, F0, t0, Constant0
	movl W( 7), t0;     Step D, E, A, B, C, F0, t0, Constant0
	movl W( 8), t0;     Step C, D, E, A, B, F0, t0, Constant0
	movl W( 9), t0;     Step B, C, D, E, A, F0, t0, Constant0

	movl W(10), t0;     Step A, B, C, D, E, F0, t0, Constant0
	movl W(11), t0;     Step E, A, B, C, D, F0, t0, Constant0
	movl W(12), t0;     Step D, E, A, B, C, F0, t0, Constant0
	movl W(13), t0;     Step C, D, E, A, B, F0, t0, Constant0
	movl W(14), t0;     Step B, C, D, E, A, F0, t0, Constant0

	movl W(15), t0;     Step A, B, C, D, E, F0, t0, Constant0

// Steps 16 to 19.  Update words in message schedule as we go.
	PrepareWord  16;    Step E, A, B, C, D, F0, t0, Constant0
	PrepareWord  17;    Step D, E, A, B, C, F0, t0, Constant0
	PrepareWord  18;    Step C, D, E, A, B, F0, t0, Constant0
	PrepareWord  19;    Step B, C, D, E, A, F0, t0, Constant0

// Steps 20 to 39.
	PrepareWord  20;    Step A, B, C, D, E, F1, t0, Constant1
	PrepareWord  21;    Step E, A, B, C, D, F1, t0, Constant1
	PrepareWord  22;    Step D, E, A, B, C, F1, t0, Constant1
	PrepareWord  23;    Step C, D, E, A, B, F1, t0, Constant1
	PrepareWord  24;    Step B, C, D, E, A, F1, t0, Constant1

	PrepareWord  25;    Step A, B, C, D, E, F1, t0, Constant1
	PrepareWord  26;    Step E, A, B, C, D, F1, t0, Constant1
	PrepareWord  27;    Step D, E, A, B, C, F1, t0, Constant1
	PrepareWord  28;    Step C, D, E, A, B, F1, t0, Constant1
	PrepareWord  29;    Step B, C, D, E, A, F1, t0, Constant1

	PrepareWord  30;    Step A, B, C, D, E, F1, t0, Constant1
	PrepareWord  31;    Step E, A, B, C, D, F1, t0, Constant1
	PrepareWord  32;    Step D, E, A, B, C, F1, t0, Constant1
	PrepareWord  33;    Step C, D, E, A, B, F1, t0, Constant1
	PrepareWord  34;    Step B, C, D, E, A, F1, t0, Constant1

	PrepareWord  35;    Step A, B, C, D, E, F1, t0, Constant1
	PrepareWord  36;    Step E, A, B, C, D, F1, t0, Constant1
	PrepareWord  37;    Step D, E, A, B, C, F1, t0, Constant1
	PrepareWord  38;    Step C, D, E, A, B, F1, t0, Constant1
	PrepareWord  39;    Step B, C, D, E, A, F1, t0, Constant1

// Steps 40 to 59.
	PrepareWord  40;    Step A, B, C, D, E, F2, t0, Constant2
	PrepareWord  41;    Step E, A, B, C, D, F2, t0, Constant2
	PrepareWord  42;    Step D, E, A, B, C, F2, t0, Constant2
	PrepareWord  43;    Step C, D, E, A, B, F2, t0, Constant2
	PrepareWord  44;    Step B, C, D, E, A, F2, t0, Constant2

	PrepareWord  45;    Step A, B, C, D, E, F2, t0, Constant2
	PrepareWord  46;    Step E, A, B, C, D, F2, t0, Constant2
	PrepareWord  47;    Step D, E, A, B, C, F2, t0, Constant2
	PrepareWord  48;    Step C, D, E, A, B, F2, t0, Constant2
	PrepareWord  49;    Step B, C, D, E, A, F2, t0, Constant2

	PrepareWord  50;    Step A, B, C, D, E, F2, t0, Constant2
	PrepareWord  51;    Step E, A, B, C, D, F2, t0, Constant2
	PrepareWord  52;    Step D, E, A, B, C, F2, t0, Constant2
	PrepareWord  53;    Step C, D, E, A, B, F2, t0, Constant2
	PrepareWord  54;    Step B, C, D, E, A, F2, t0, Constant2

	PrepareWord  55;    Step A, B, C, D, E, F2, t0, Constant2
	PrepareWord  56;    Step E, A, B, C, D, F2, t0, Constant2
	PrepareWord  57;    Step D, E, A, B, C, F2, t0, Constant2
	PrepareWord  58;    Step C, D, E, A, B, F2, t0, Constant2
	PrepareWord  59;    Step B, C, D, E, A, F2, t0, Constant2

// Steps 60 to 79.
	PrepareWord  60;    Step A, B, C, D, E, F3, t0, Constant3
	PrepareWord  61;    Step E, A, B, C, D, F3, t0, Constant3
	PrepareWord  62;    Step D, E, A, B, C, F3, t0, Constant3
	PrepareWord  63;    Step C, D, E, A, B, F3, t0, Constant3
	PrepareWord  64;    Step B, C, D, E, A, F3, t0, Constant3

	PrepareWord  65;    Step A, B, C, D, E, F3, t0, Constant3
	PrepareWord  66;    Step E, A, B, C, D, F3, t0, Constant3
	PrepareWord  67;    Step D, E, A, B, C, F3, t0, Constant3
	PrepareWord  68;    Step C, D, E, A, B, F3, t0, Constant3
	PrepareWord  69;    Step B, C, D, E, A, F3, t0, Constant3

	PrepareWord  70;    Step A, B, C, D, E, F3, t0, Constant3
	PrepareWord  71;    Step E, A, B, C, D, F3, t0, Constant3
	PrepareWord  72;    Step D, E, A, B, C, F3, t0, Constant3
	PrepareWord  73;    Step C, D, E, A, B, F3, t0, Constant3
	PrepareWord  74;    Step B, C, D, E, A, F3, t0, Constant3

	PrepareWord  75;    Step A, B, C, D, E, F3, t0, Constant3
	PrepareWord  76;    Step E, A, B, C, D, F3, t0, Constant3
	PrepareWord  77;    Step D, E, A, B, C, F3, t0, Constant3
	PrepareWord  78;    Step C, D, E, A, B, F3, t0, Constant3
	PrepareWord  79;    Step B, C, D, E, A, F3, t0, Constant3

	#if defined __i386__
		mov		ArgContext, Context
	#endif

	// Update SHA-1 context.
	add		Contexth0(Context), A
	add		Contexth1(Context), B
	add		Contexth2(Context), C
	add		Contexth3(Context), D
	add		Contexth4(Context), E

	mov		A, Contexth0(Context)
	mov		B, Contexth1(Context)
	mov		C, Contexth2(Context)
	mov		D, Contexth3(Context)
	mov		E, Contexth4(Context)

	// Decrement and loop if not done.
	#if defined __i386__
		mov		ArgBlocks, Blocks
		add		$-1, Blocks
		mov		Blocks, ArgBlocks
	#else
		add		$-1, Blocks
	#endif
	jg		1b

	// Pop stack and restore registers.
	#if 0 < LocalsSize
		add		$LocalsSize, r4
	#endif
	#if defined __i386__
		pop		r7
		pop		r6
	#endif
	pop		r3
	pop		r5

	ret


#endif	// defined __i386__ || defined __x86_64__
