/* A Bison parser, made by GNU Bison 1.875.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002 Free Software Foundation, Inc.

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
     tINT = 258,
     tDOUBLE = 259,
     tSTRING = 260,
     CYAN = 261,
     L_CYAN = 262,
     MAGENTA = 263,
     L_MAGENTA = 264,
     YELLOW = 265,
     D_YELLOW = 266,
     BLACK = 267,
     L_BLACK = 268,
     GAMMA = 269,
     LEVEL = 270,
     STEPS = 271,
     INK_LIMIT = 272,
     PRINTER = 273,
     PARAMETER = 274,
     PARAMETER_INT = 275,
     PARAMETER_FLOAT = 276,
     PARAMETER_CURVE = 277,
     DENSITY = 278,
     TOP = 279,
     LEFT = 280,
     HSIZE = 281,
     VSIZE = 282,
     BLACKLINE = 283,
     PATTERN = 284,
     XPATTERN = 285,
     EXTENDED = 286,
     IMAGE = 287,
     GRID = 288,
     SEMI = 289,
     CHANNEL = 290,
     CMYK = 291,
     KCMY = 292,
     RGB = 293,
     CMY = 294,
     GRAY = 295,
     WHITE = 296,
     RAW = 297,
     MODE = 298,
     PAGESIZE = 299,
     MESSAGE = 300,
     END = 301
   };
#endif
#define tINT 258
#define tDOUBLE 259
#define tSTRING 260
#define CYAN 261
#define L_CYAN 262
#define MAGENTA 263
#define L_MAGENTA 264
#define YELLOW 265
#define D_YELLOW 266
#define BLACK 267
#define L_BLACK 268
#define GAMMA 269
#define LEVEL 270
#define STEPS 271
#define INK_LIMIT 272
#define PRINTER 273
#define PARAMETER 274
#define PARAMETER_INT 275
#define PARAMETER_FLOAT 276
#define PARAMETER_CURVE 277
#define DENSITY 278
#define TOP 279
#define LEFT 280
#define HSIZE 281
#define VSIZE 282
#define BLACKLINE 283
#define PATTERN 284
#define XPATTERN 285
#define EXTENDED 286
#define IMAGE 287
#define GRID 288
#define SEMI 289
#define CHANNEL 290
#define CMYK 291
#define KCMY 292
#define RGB 293
#define CMY 294
#define GRAY 295
#define WHITE 296
#define RAW 297
#define MODE 298
#define PAGESIZE 299
#define MESSAGE 300
#define END 301




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
typedef int YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;



