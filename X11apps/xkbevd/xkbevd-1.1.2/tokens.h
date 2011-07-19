/************************************************************
 Copyright (c) 1994 by Silicon Graphics Computer Systems, Inc.

 Permission to use, copy, modify, and distribute this
 software and its documentation for any purpose and without
 fee is hereby granted, provided that the above copyright
 notice appear in all copies and that both that copyright
 notice and this permission notice appear in supporting
 documentation, and that the name of Silicon Graphics not be
 used in advertising or publicity pertaining to distribution
 of the software without specific prior written permission.
 Silicon Graphics makes no representation about the suitability
 of this software for any purpose. It is provided "as is"
 without any express or implied warranty.

 SILICON GRAPHICS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SILICON
 GRAPHICS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
 THE USE OR PERFORMANCE OF THIS SOFTWARE.

 ********************************************************/
#ifndef TOKENS_H
#define	TOKENS_H 1

#define	END_OF_FILE	0
#define	ERROR		255

#define	BELL		1
#define	ACCESSX		2
#define	MESSAGE		3

#define	NONE		20
#define	IGNORE		21
#define	ECHO		22
#define	PRINT_EV	23
#define	SHELL		24
#define	SOUND		25

#define	EQUALS		40
#define	PLUS		41
#define	MINUS		42
#define	DIVIDE		43
#define	TIMES		44
#define	OBRACE		45
#define	CBRACE		46
#define	OPAREN		47
#define	CPAREN		48
#define	OBRACKET	49
#define	CBRACKET	50
#define	DOT		51
#define	COMMA		52
#define	SEMI		53
#define	EXCLAM		54
#define	INVERT		55

#define	STRING		60
#define	INTEGER		61
#define	FLOAT		62
#define	IDENT		63
#define	KEYNAME		64

#endif
