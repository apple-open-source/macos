#ifndef _READ_H_
#define _READ_H_
/* read.h - of read.c
   Copyright (C) 1986 Free Software Foundation, Inc.

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

#import "expr.h"

#define PERMIT_WHITESPACE	/* Define to make whitespace be allowed in */
				/* many syntactically unnecessary places. */
				/* Normally undefined. For compatibility */
				/* with ancient GNU cc. */
#undef PERMIT_WHITESPACE

#ifdef PERMIT_WHITESPACE
#define SKIP_WHITESPACE() {if (* input_line_pointer == ' ') ++ input_line_pointer;}
#else
#define SKIP_WHITESPACE() ASSERT( * input_line_pointer != ' ' )
#endif


#define	LEX_NAME	(1)	/* may continue a name */		      
#define LEX_BEGIN_NAME	(2)	/* may begin a name */			      
						        		      
#define is_name_beginner(c)     ( lex_type[(c) & 0xff] & LEX_BEGIN_NAME )
#define is_part_of_name(c)      ( lex_type[(c) & 0xff] & LEX_NAME       )

/*
 * A pseudo opcode table entry.
 */
typedef struct {
    char *poc_name;	/* assembler mnemonic, lower case, no '.' */
    void (*poc_handler)(int poc_val);	/* Do the work */
    int  poc_val;	/* Value to pass to handler */
} pseudo_typeS;

extern char *input_line_pointer; /* -> char we are parsing now. */
extern char *buffer_limit;	 /* -> 1 + last char in buffer. */
extern
#ifndef PPC
const
#endif /* PPC */
char lex_type[];
extern unsigned long text_nsect;

extern void read_begin(
    void);
#ifdef PPC
extern void ppcasm_read_begin(
    void);
#endif /* PPC */
void read_a_source_file(
    char *buffer);
extern long get_absolute_expression(
    void);
extern void demand_empty_rest_of_line(
    void);
extern void ignore_rest_of_line(
    void);
extern void add_to_macro_definition(
    char *char_pointer);
extern segT get_known_segmented_expression(
    expressionS *expP);
extern void totally_ignore_line(
    void);

/* globally know pseudo-op functions (used by some assemblers in MACHINE.c) */
extern void stringer(
    int append_zero);
extern void s_space(
    int value);
extern void s_abs(
    int value);
extern void float_cons(
    int float_type);
extern void cons(
    int nbytes);
extern void s_globl(
    int value);
extern void s_line(
    int value);
extern void s_macro(
    int value);
extern void s_endmacro(
    int value);
extern void big_cons(
    int nbytes);
#endif /* _READ_H_ */
