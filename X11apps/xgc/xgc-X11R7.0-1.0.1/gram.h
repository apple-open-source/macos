/* A Bison parser, made by GNU Bison 1.875c.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     STRING = 258,
     NUMBER = 259,
     RUN = 260,
     FUNCTION = 261,
     FUNCTIONTYPE = 262,
     TEST = 263,
     TESTTYPE = 264,
     LINESTYLE = 265,
     LINESTYLETYPE = 266,
     CAPSTYLE = 267,
     CAPSTYLETYPE = 268,
     JOINSTYLE = 269,
     JOINSTYLETYPE = 270,
     ROUND = 271,
     SOLID = 272,
     FILLSTYLE = 273,
     FILLSTYLETYPE = 274,
     FILLRULE = 275,
     FILLRULETYPE = 276,
     ARCMODE = 277,
     ARCMODETYPE = 278,
     FOREGROUND = 279,
     BACKGROUND = 280,
     LINEWIDTH = 281,
     PLANEMASK = 282,
     DASHLIST = 283,
     PERCENT = 284,
     FONT = 285
   };
#endif
#define STRING 258
#define NUMBER 259
#define RUN 260
#define FUNCTION 261
#define FUNCTIONTYPE 262
#define TEST 263
#define TESTTYPE 264
#define LINESTYLE 265
#define LINESTYLETYPE 266
#define CAPSTYLE 267
#define CAPSTYLETYPE 268
#define JOINSTYLE 269
#define JOINSTYLETYPE 270
#define ROUND 271
#define SOLID 272
#define FILLSTYLE 273
#define FILLSTYLETYPE 274
#define FILLRULE 275
#define FILLRULETYPE 276
#define ARCMODE 277
#define ARCMODETYPE 278
#define FOREGROUND 279
#define BACKGROUND 280
#define LINEWIDTH 281
#define PLANEMASK 282
#define DASHLIST 283
#define PERCENT 284
#define FONT 285




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 19 "gram.y"
typedef union YYSTYPE {
  int num;
  char *ptr;
} YYSTYPE;
/* Line 1275 of yacc.c.  */
#line 102 "y.tab.h"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;



