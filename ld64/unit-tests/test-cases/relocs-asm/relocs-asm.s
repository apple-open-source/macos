/*
 * Copyright (c) 2005-2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#if __ppc__ || __ppc64__

	.text
	.align 2
		
	.globl _test_loads
_test_loads:
	stmw r30,-8(r1)
	stwu r1,-48(r1)
Lpicbase:

	; PIC load of a 
	addis r2,r10,ha16(_a-Lpicbase)
	lwz r2,lo16(_a-Lpicbase)(r2)

	; PIC load of c 
	addis r2,r10,ha16(_c-Lpicbase)
	lwz r2,lo16(_c-Lpicbase)(r2)

	; absolute load of a
	lis r2,ha16(_a)
	lwz r2,lo16(_a)(r2)

	; absolute load of c
	lis r2,ha16(_c)
	lwz r2,lo16(_c)(r2)

	; absolute load of external
	lis r2,ha16(_ax)
	lwz r2,lo16(_ax)(r2)

	; absolute lea of external
	lis r2,hi16(_ax)
	ori r2,r2,lo16(_ax)


	; PIC load of a + addend
	addis r2,r10,ha16(_a+0x19000-Lpicbase)
	lwz r2,lo16(_a+0x19000-Lpicbase)(r2)

	; absolute load of a + addend
	lis r2,ha16(_a+0x19000)
	lwz r2,lo16(_a+0x19000)(r2)

	; absolute load of external + addend
	lis r2,ha16(_ax+0x19000)
	lwz r2,lo16(_ax+0x19000)(r2)

	; absolute lea of external + addend
	lis r2,hi16(_ax+0x19000)
	ori r2,r2,lo16(_ax+0x19000)


	; PIC load of a + addend
	addis r2,r10,ha16(_a+0x09000-Lpicbase)
	lwz r2,lo16(_a+0x09000-Lpicbase)(r2)

	; absolute load of a + addend
	lis r2,ha16(_a+0x09000)
	lwz r2,lo16(_a+0x09000)(r2)

	; absolute load of external + addend
	lis r2,ha16(_ax+0x09000)
	lwz r2,lo16(_ax+0x09000)(r2)

	; absolute lea of external + addend
	lis r2,hi16(_ax+0x09000)
	ori r2,r2,lo16(_ax+0x09000)

	blr


_test_calls:
	; call internal
	bl	_test_branches
	
	; call internal + addend
	bl	_test_branches+0x19000

	; call external
	bl	_external
	
	; call external + addend
	bl	_external+0x19000
	

_test_branches:
	; call internal
	bne	_test_calls
	
	; call internal + addend
	bne	_test_calls+16

	; call external
	bne	_external
	
	; call external + addend
	bne	_external+16
#endif



#if __i386__
	.text
	.align 2
	
	.globl _test_loads
_test_loads:
	pushl	%ebp
Lpicbase:

	# PIC load of a 
	movl	_a-Lpicbase(%ebx), %eax
	
	# absolute load of a
	movl	_a, %eax

	# absolute load of external
	movl	_ax, %eax

	# absolute lea of external
	leal	_ax, %eax


	# PIC load of a + addend
	movl	_a-Lpicbase+0x19000(%ebx), %eax

	# absolute load of a + addend
	movl	_a+0x19000(%ebx), %eax

	# absolute load of external + addend
	movl	_ax+0x19000(%ebx), %eax

	# absolute lea of external + addend
	leal	_ax+0x1900, %eax

	ret


_test_calls:
	# call internal
	call	_test_branches
	
	# call internal + addend
	call	_test_branches+0x19000

	# call external
	call	_external
	
	# call external + addend
	call	_external+0x19000
	

_test_branches:
	# call internal
	jne	_test_calls
	
	# call internal + addend
	jne	_test_calls+16

	# call external
	jne	_external
	
	# call external + addend
	jne	_external+16
	
_pointer_diffs:
	nop
	call	_get_ret_eax	
1:	movl _foo-1b(%eax),%esi
	movl _foo+10-1b(%eax),%esi
	movl _test_branches-1b(%eax),%esi
	movl _test_branches+3-1b(%eax),%esi
	
_word_relocs:
	callw	_pointer_diffs

#endif



#if __x86_64__
	.text
	.align 2
	
	.globl _test_loads
_test_loads:

	# PIC load of a 
	movl	_a(%rip), %eax
	
	# PIC load of a + addend
	movl	_a+0x1234(%rip), %eax

	# PIC lea
	leaq	_a(%rip), %rax

	# PIC lea through GOT
	movq	_a@GOTPCREL(%rip), %rax
	
	# PIC access of GOT
  	pushq	_a@GOTPCREL(%rip)

	# PIC lea external through GOT
	movq	_ax@GOTPCREL(%rip), %rax
	
	# PIC external access of GOT
  	pushq	_ax@GOTPCREL(%rip)

	# 1-byte store
  	movb  $0x12, _a(%rip)
  	movb  $0x12, _a+2(%rip)
  	movb  $0x12, L0(%rip)

	# 4-byte store
  	movl  $0x12345678, _a(%rip)
  	movl  $0x12345678, _a+4(%rip)
  	movl  $0x12345678, L0(%rip)
	
	# test local labels
	lea L1(%rip), %rax		
  	movl L0(%rip), %eax		

	ret


_test_calls:
	# call internal
	call	_test_branches
	
	# call internal + addend
	call	_test_branches+0x19000

	# call external
	call	_external
	
	# call external + addend
	call	_external+0x19000
	

_test_branches:
	# call internal
	jne	_test_calls
	
	# call internal + addend
	jne	_test_calls+16

	# call external
	jne	_external
	
	# call external + addend
	jne	_external+16
#endif



	# test that pointer-diff relocs are preserved
	.text
_test_diffs:
	.align 2
Llocal2:
	.long 0
	.long Llocal2-_test_branches
	.long . - _test_branches
	.long . - _test_branches + 8
	.long _test_branches - .
	.long _test_branches - . + 8
	.long _test_branches - . - 8
#if __ppc64__
	.quad Llocal2-_test_branches
#endif

_foo: nop

	.align 2	
_distance_from_foo:
	.long	0
	.long	. - _foo
	.long	. - 8 - _foo
	
	
_distance_to_foo:
	.long	_foo - .
	.long	_foo - . + 4
	

_distance_to_here:	
	.long	_foo - _distance_to_here
	.long	_foo - _distance_to_here - 4 
	.long	_foo - _distance_to_here - 12 
	.long	0


#if __x86_64__
	.data
L0:  .quad _test_branches
_prev:
	.quad _test_branches+4
L1:	.quad _test_branches - _test_diffs
  	.quad _test_branches - _test_diffs + 4
  	.long _test_branches - _test_diffs
#	.long LCL0-.				### assembler bug: should SUB/UNSIGNED with content= LCL0-24, or single pc-rel SIGNED reloc with content = LCL0-.+4
  	.quad L1
  	.quad L0					
  	.quad _test_branches - .
  	.quad _test_branches - L1
  	.quad L1 - _prev			

# the following generates: _foo cannot be undefined in a subtraction expression
# but it should be ok (it will be a linker error if _foo and _bar are not in same linkage unit)
#	.quad _foo - _bar	### assembler bug

	.section __DATA,__data2
LCL0: .long 2


#endif


	.data
_a:	
	.long	0

_b:
#if __ppc__ || __i386__
	.long	_test_calls
	.long	_test_calls+16
	.long	_external
	.long	_external+16
#elif __ppc64__ || __x86_64__
	.quad	_test_calls
	.quad	_test_calls+16
	.quad	_external
	.quad	_external+16
#endif

	# test that reloc sizes are the same
Llocal3:
	.long	0
	
Llocal4:
	.long 0
	
	.long Llocal4-Llocal3
	
Lfiller:
	.space	0x9000
_c:
	.long	0

