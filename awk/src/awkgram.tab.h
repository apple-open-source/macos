/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     FIRSTTOKEN = 258,
     PROGRAM = 259,
     PASTAT = 260,
     PASTAT2 = 261,
     XBEGIN = 262,
     XEND = 263,
     NL = 264,
     ARRAY = 265,
     MATCH = 266,
     NOTMATCH = 267,
     MATCHOP = 268,
     FINAL = 269,
     DOT = 270,
     ALL = 271,
     CCL = 272,
     NCCL = 273,
     CHAR = 274,
     OR = 275,
     STAR = 276,
     QUEST = 277,
     PLUS = 278,
     EMPTYRE = 279,
     ZERO = 280,
     IGNORE_PRIOR_ATOM = 281,
     AND = 282,
     BOR = 283,
     APPEND = 284,
     EQ = 285,
     GE = 286,
     GT = 287,
     LE = 288,
     LT = 289,
     NE = 290,
     IN = 291,
     ARG = 292,
     BLTIN = 293,
     BREAK = 294,
     CLOSE = 295,
     CONTINUE = 296,
     DELETE = 297,
     DO = 298,
     EXIT = 299,
     FOR = 300,
     FUNC = 301,
     SUB = 302,
     GSUB = 303,
     IF = 304,
     INDEX = 305,
     LSUBSTR = 306,
     MATCHFCN = 307,
     NEXT = 308,
     NEXTFILE = 309,
     ADD = 310,
     MINUS = 311,
     MULT = 312,
     DIVIDE = 313,
     MOD = 314,
     ASSIGN = 315,
     ASGNOP = 316,
     ADDEQ = 317,
     SUBEQ = 318,
     MULTEQ = 319,
     DIVEQ = 320,
     MODEQ = 321,
     POWEQ = 322,
     PRINT = 323,
     PRINTF = 324,
     SPRINTF = 325,
     ELSE = 326,
     INTEST = 327,
     CONDEXPR = 328,
     POSTINCR = 329,
     PREINCR = 330,
     POSTDECR = 331,
     PREDECR = 332,
     VAR = 333,
     IVAR = 334,
     VARNF = 335,
     CALL = 336,
     NUMBER = 337,
     STRING = 338,
     REGEXPR = 339,
     GETLINE = 340,
     SUBSTR = 341,
     SPLIT = 342,
     RETURN = 343,
     WHILE = 344,
     CAT = 345,
     UPLUS = 346,
     UMINUS = 347,
     NOT = 348,
     POWER = 349,
     INCR = 350,
     DECR = 351,
     INDIRECT = 352,
     LASTTOKEN = 353
   };
#endif
/* Tokens.  */
#define FIRSTTOKEN 258
#define PROGRAM 259
#define PASTAT 260
#define PASTAT2 261
#define XBEGIN 262
#define XEND 263
#define NL 264
#define ARRAY 265
#define MATCH 266
#define NOTMATCH 267
#define MATCHOP 268
#define FINAL 269
#define DOT 270
#define ALL 271
#define CCL 272
#define NCCL 273
#define CHAR 274
#define OR 275
#define STAR 276
#define QUEST 277
#define PLUS 278
#define EMPTYRE 279
#define ZERO 280
#define IGNORE_PRIOR_ATOM 281
#define AND 282
#define BOR 283
#define APPEND 284
#define EQ 285
#define GE 286
#define GT 287
#define LE 288
#define LT 289
#define NE 290
#define IN 291
#define ARG 292
#define BLTIN 293
#define BREAK 294
#define CLOSE 295
#define CONTINUE 296
#define DELETE 297
#define DO 298
#define EXIT 299
#define FOR 300
#define FUNC 301
#define SUB 302
#define GSUB 303
#define IF 304
#define INDEX 305
#define LSUBSTR 306
#define MATCHFCN 307
#define NEXT 308
#define NEXTFILE 309
#define ADD 310
#define MINUS 311
#define MULT 312
#define DIVIDE 313
#define MOD 314
#define ASSIGN 315
#define ASGNOP 316
#define ADDEQ 317
#define SUBEQ 318
#define MULTEQ 319
#define DIVEQ 320
#define MODEQ 321
#define POWEQ 322
#define PRINT 323
#define PRINTF 324
#define SPRINTF 325
#define ELSE 326
#define INTEST 327
#define CONDEXPR 328
#define POSTINCR 329
#define PREINCR 330
#define POSTDECR 331
#define PREDECR 332
#define VAR 333
#define IVAR 334
#define VARNF 335
#define CALL 336
#define NUMBER 337
#define STRING 338
#define REGEXPR 339
#define GETLINE 340
#define SUBSTR 341
#define SPLIT 342
#define RETURN 343
#define WHILE 344
#define CAT 345
#define UPLUS 346
#define UMINUS 347
#define NOT 348
#define POWER 349
#define INCR 350
#define DECR 351
#define INDIRECT 352
#define LASTTOKEN 353




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 41 "awkgram.y"
{
	Node	*p;
	Cell	*cp;
	int	i;
	char	*s;
}
/* Line 1529 of yacc.c.  */
#line 252 "awkgram.tab.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

