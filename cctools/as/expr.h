/* expr.h -> header file for expr.c
   Copyright (C) 1987 Free Software Foundation, Inc.

This file is part of GAS, the GNU Assembler.

GAS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GAS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GAS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#import "struc-symbol.h"
#import "bignum.h"
#import "flonum.h"

/*
 * This table describes the use of segments as EXPRESSION types.
 *
 *	X_seg	X_add_symbol  X_subtract_symbol	X_add_number
 * SEG_NONE						no (legal) expression
 * SEG_BIG					*	> 32 bits const.
 * SEG_ABSOLUTE				     	0
 * SEG_SECT		*		     	0
 * SEG_UNKNOWN		*			0
 * SEG_DIFFSECT		0		*	0
 *
 * The blank fields MUST be 0, and are nugatory.
 * The '0' fields MAY be 0. The '*' fields MAY NOT be 0.
 *
 * SEG_BIG: A floating point number or an integer larger than 32 bits.
 *   For a floating point number:
 *	X_add_number is < 0
 * 	    The result is in the global variable generic_floating_point_number.
 *	    The value in X_add_number is -'c' where c is the character that
 *	    introduced the constant.  e.g. "0f6.9" will have  -'f' as a
 *	    X_add_number value.
 *   For an integer larger than 32 bits:
 *	X_add_number > 0
 *	    The result is in the global variable generic_bignum.
 *	    The value in X_add_number is a count of how many littlenums it
 *	    took to represent the bignum.
 */
typedef enum {
    SEG_ABSOLUTE,	/* absolute */
    SEG_SECT,		/* normal defined section */
    SEG_DIFFSECT,	/* difference between symbols in sections */
    SEG_UNKNOWN,	/* expression involving an undefined symbol */
    SEG_NONE,		/* no expression */
    SEG_BIG		/* bigger than 32 bits constant */
} segT;

extern char *seg_name[];
extern segT N_TYPE_seg[];

/*
 * When an expression is SEG_BIG, it is in these globals (see comments above
 * about SEG_BIG).  This data may be clobbered whenever expr() is called.
 */
extern FLONUM_TYPE    generic_floating_point_number;
extern LITTLENUM_TYPE generic_bignum[];
#define SIZE_OF_LARGE_NUMBER (20)	/* Number of littlenums in above */
					/* generic_bignum which is enough to */
					/* hold most precise flonum. */

/*
 * Abbreviations (mnemonics).
 *
 *	O	operator
 *	Q	quantity,  operand
 *	X	eXpression
 */

/*
 * By popular demand, we define a struct to represent an expression.
 * This will no doubt mutate as expressions become baroque.
 *
 * Currently, we support expressions like "foo-bar+42".
 * In other words we permit a (possibly undefined) minuend, a
 * (possibly undefined) subtrahend and an (absolute) augend.
 * RMS says this is so we can have 1-pass assembly for any compiler
 * emmissions, and a 'case' statement might emit 'undefined1 - undefined2'.
 *
 * To simplify table-driven dispatch, we also have a "segment" for the
 * entire expression. That way we don't require complex reasoning about
 * whether particular components are defined; and we can change component
 * semantics without re-working all the dispatch tables in the assembler.
 * In other words the "type" of an expression is its segment.
 */

typedef struct {
    symbolS *X_add_symbol;	/* foo */
    symbolS *X_subtract_symbol;	/* bar */
    long     X_add_number;	/* 42 (must be signed) */
    segT     X_seg;		/* What segment (expr type) */
} expressionS;

extern segT expression(
    expressionS *resultP);
extern char get_symbol_end(
    void);
extern segT try_to_make_absolute(
    expressionS *expressionP);
