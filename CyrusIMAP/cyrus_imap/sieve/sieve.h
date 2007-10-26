/* A Bison parser, made by GNU Bison 2.1.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.

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
     NUMBER = 258,
     STRING = 259,
     IF = 260,
     ELSIF = 261,
     ELSE = 262,
     REJCT = 263,
     FILEINTO = 264,
     REDIRECT = 265,
     KEEP = 266,
     STOP = 267,
     DISCARD = 268,
     VACATION = 269,
     REQUIRE = 270,
     SETFLAG = 271,
     ADDFLAG = 272,
     REMOVEFLAG = 273,
     MARK = 274,
     UNMARK = 275,
     NOTIFY = 276,
     DENOTIFY = 277,
     ANYOF = 278,
     ALLOF = 279,
     EXISTS = 280,
     SFALSE = 281,
     STRUE = 282,
     HEADER = 283,
     NOT = 284,
     SIZE = 285,
     ADDRESS = 286,
     ENVELOPE = 287,
     BODY = 288,
     COMPARATOR = 289,
     IS = 290,
     CONTAINS = 291,
     MATCHES = 292,
     REGEX = 293,
     COUNT = 294,
     VALUE = 295,
     OVER = 296,
     UNDER = 297,
     GT = 298,
     GE = 299,
     LT = 300,
     LE = 301,
     EQ = 302,
     NE = 303,
     ALL = 304,
     LOCALPART = 305,
     DOMAIN = 306,
     USER = 307,
     DETAIL = 308,
     RAW = 309,
     TEXT = 310,
     CONTENT = 311,
     DAYS = 312,
     ADDRESSES = 313,
     SUBJECT = 314,
     FROM = 315,
     HANDLE = 316,
     MIME = 317,
     METHOD = 318,
     ID = 319,
     OPTIONS = 320,
     LOW = 321,
     NORMAL = 322,
     HIGH = 323,
     ANY = 324,
     MESSAGE = 325,
     INCLUDE = 326,
     PERSONAL = 327,
     GLOBAL = 328,
     RETURN = 329,
     COPY = 330
   };
#endif
/* Tokens.  */
#define NUMBER 258
#define STRING 259
#define IF 260
#define ELSIF 261
#define ELSE 262
#define REJCT 263
#define FILEINTO 264
#define REDIRECT 265
#define KEEP 266
#define STOP 267
#define DISCARD 268
#define VACATION 269
#define REQUIRE 270
#define SETFLAG 271
#define ADDFLAG 272
#define REMOVEFLAG 273
#define MARK 274
#define UNMARK 275
#define NOTIFY 276
#define DENOTIFY 277
#define ANYOF 278
#define ALLOF 279
#define EXISTS 280
#define SFALSE 281
#define STRUE 282
#define HEADER 283
#define NOT 284
#define SIZE 285
#define ADDRESS 286
#define ENVELOPE 287
#define BODY 288
#define COMPARATOR 289
#define IS 290
#define CONTAINS 291
#define MATCHES 292
#define REGEX 293
#define COUNT 294
#define VALUE 295
#define OVER 296
#define UNDER 297
#define GT 298
#define GE 299
#define LT 300
#define LE 301
#define EQ 302
#define NE 303
#define ALL 304
#define LOCALPART 305
#define DOMAIN 306
#define USER 307
#define DETAIL 308
#define RAW 309
#define TEXT 310
#define CONTENT 311
#define DAYS 312
#define ADDRESSES 313
#define SUBJECT 314
#define FROM 315
#define HANDLE 316
#define MIME 317
#define METHOD 318
#define ID 319
#define OPTIONS 320
#define LOW 321
#define NORMAL 322
#define HIGH 323
#define ANY 324
#define MESSAGE 325
#define INCLUDE 326
#define PERSONAL 327
#define GLOBAL 328
#define RETURN 329
#define COPY 330




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 149 "sieve.y"
typedef union YYSTYPE {
    int nval;
    char *sval;
    stringlist_t *sl;
    test_t *test;
    testlist_t *testl;
    commandlist_t *cl;
    struct vtags *vtag;
    struct aetags *aetag;
    struct htags *htag;
    struct btags *btag;
    struct ntags *ntag;
    struct dtags *dtag;
} YYSTYPE;
/* Line 1447 of yacc.c.  */
#line 203 "y.tab.h"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;



