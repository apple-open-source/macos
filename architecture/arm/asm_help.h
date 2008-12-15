/*
 * Copyright (c) 2000-2007 Apple Inc. All rights reserved.
 */

#ifndef	_ARCH_ARM_ASM_HELP_H_
#define	_ARCH_ARM_ASM_HELP_H_

#include	<architecture/arm/reg_help.h>

#ifdef	__ASSEMBLER__

#define ALIGN						\
	.align	2

#define	ROUND_TO_STACK(len)				\
	(((len) + STACK_INCR - 1) / STACK_INCR * STACK_INCR)

#define CALL_MCOUNT
#define NESTED_FUNCTION_PROLOGUE(localvarsize)
#define LEAF_FUNCTION_PROLOGUE(localvarsize)
#define FUNCTION_EPILOGUE

/*
 * Macros for declaring procedures
 *
 * Use of these macros allows ctags to have a predictable way
 * to find various types of declarations.  They also simplify
 * inserting appropriate symbol table information.
 *
 * NOTE: these simple stubs will be replaced with more
 * complicated versions once we know what the linker and gdb
 * will require as far as register use masks and frame declarations.
 * These macros may also be ifdef'ed in the future to contain profiling
 * code.
 *
 */

/*
 * TEXT -- declare start of text segment
 */
#define	TEXT						\
	.text

/*
 * DATA -- declare start of data segment
 */
#define DATA						\
	.data

/*
 * LEAF -- declare global leaf procedure
 * NOTE: Control SHOULD NOT FLOW into a LEAF!  A LEAF should only
 * be jumped to.  (A leaf may do an align.)  Use a LABEL() if you
 * need control to flow into the label.
 */
#define	LEAF(name, localvarsize)			\
	.globl	name					;\
	ALIGN						;\
name:							;\
	LEAF_FUNCTION_PROLOGUE(localvarsize)

/*
 * X_LEAF -- declare alternate global label for leaf
 */
#define	X_LEAF(name, value)				\
	.globl	name					;\
	.set	name,value

/*
 * P_LEAF -- declare private leaf procedure
 */
#define	P_LEAF(name, localvarsize)			\
	ALIGN						;\
name:							;\
	LEAF_FUNCTION_PROLOGUE(localvarsize)

/*
 * LABEL -- declare a global code label
 * MUST be used (rather than LEAF, NESTED, etc) if control
 * "flows into" the label.
 */
#define	LABEL(name)					\
	.globl	name					;\
name:

/*
 * NESTED -- declare procedure that invokes other procedures
 */
#define	NESTED(name, localvarsize)			\
	.globl	name					;\
	ALIGN						;\
name:							;\
	NESTED_FUNCTION_PROLOGUE(localvarsize)

/*
 * X_NESTED -- declare alternate global label for nested proc
 */
#define	X_NESTED(name, value)				\
	.globl	name					;\
	.set	name,value

/*
 * P_NESTED -- declare private nested procedure
 */
#define	P_NESTED(name, localvarsize)			\
	ALIGN						;\
name:							;\
	NESTED_FUNCTION_PROLOGUE(localvarsize)

/*
 * END -- mark end of procedure
 */
#define	END(name)					\
	FUNCTION_EPILOGUE

/*
 * Storage definition macros
 * The main purpose of these is to allow an easy handle for ctags
 */

/*
 * IMPORT -- import symbol
 */
#define	IMPORT(name)					\
	.reference	name

/*
 * ABS -- declare global absolute symbol
 */
#define	ABS(name, value)				\
	.globl	name					;\
	.set	name,value

/*
 * P_ABS -- declare private absolute symbol
 */
#define	P_ABS(name, value)				\
	.set	name,value

/*
 * EXPORT -- declare global label for data
 */
#define	EXPORT(name)					\
	.globl	name					;\
name:

/*
 * BSS -- declare global zero'ed storage
 */
#define	BSS(name,size)					\
	.comm	name,size

/*
 * P_BSS -- declare private zero'ed storage
 */
#define	P_BSS(name,size)				\
	.lcomm	name,size

#endif	/* __ASSEMBLER__ */

#endif	/* _ARCH_ARM_ASM_HELP_H_ */
