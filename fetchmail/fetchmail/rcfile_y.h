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
     DEFAULTS = 258,
     POLL = 259,
     SKIP = 260,
     VIA = 261,
     AKA = 262,
     LOCALDOMAINS = 263,
     PROTOCOL = 264,
     AUTHENTICATE = 265,
     TIMEOUT = 266,
     KPOP = 267,
     SDPS = 268,
     ENVELOPE = 269,
     QVIRTUAL = 270,
     USERNAME = 271,
     PASSWORD = 272,
     FOLDER = 273,
     SMTPHOST = 274,
     FETCHDOMAINS = 275,
     MDA = 276,
     BSMTP = 277,
     LMTP = 278,
     SMTPADDRESS = 279,
     SMTPNAME = 280,
     SPAMRESPONSE = 281,
     PRECONNECT = 282,
     POSTCONNECT = 283,
     LIMIT = 284,
     WARNINGS = 285,
     INTERFACE = 286,
     MONITOR = 287,
     PLUGIN = 288,
     PLUGOUT = 289,
     IS = 290,
     HERE = 291,
     THERE = 292,
     TO = 293,
     MAP = 294,
     WILDCARD = 295,
     BATCHLIMIT = 296,
     FETCHLIMIT = 297,
     FETCHSIZELIMIT = 298,
     FASTUIDL = 299,
     EXPUNGE = 300,
     PROPERTIES = 301,
     SET = 302,
     LOGFILE = 303,
     DAEMON = 304,
     SYSLOG = 305,
     IDFILE = 306,
     PIDFILE = 307,
     INVISIBLE = 308,
     POSTMASTER = 309,
     BOUNCEMAIL = 310,
     SPAMBOUNCE = 311,
     SOFTBOUNCE = 312,
     SHOWDOTS = 313,
     PROTO = 314,
     AUTHTYPE = 315,
     STRING = 316,
     NUMBER = 317,
     NO = 318,
     KEEP = 319,
     FLUSH = 320,
     LIMITFLUSH = 321,
     FETCHALL = 322,
     REWRITE = 323,
     FORCECR = 324,
     STRIPCR = 325,
     PASS8BITS = 326,
     DROPSTATUS = 327,
     DROPDELIVERED = 328,
     DNS = 329,
     SERVICE = 330,
     PORT = 331,
     UIDL = 332,
     INTERVAL = 333,
     MIMEDECODE = 334,
     IDLE = 335,
     CHECKALIAS = 336,
     SSL = 337,
     SSLKEY = 338,
     SSLCERT = 339,
     SSLPROTO = 340,
     SSLCERTCK = 341,
     SSLCERTPATH = 342,
     SSLCOMMONNAME = 343,
     SSLFINGERPRINT = 344,
     PRINCIPAL = 345,
     ESMTPNAME = 346,
     ESMTPPASSWORD = 347,
     TRACEPOLLS = 348
   };
#endif
/* Tokens.  */
#define DEFAULTS 258
#define POLL 259
#define SKIP 260
#define VIA 261
#define AKA 262
#define LOCALDOMAINS 263
#define PROTOCOL 264
#define AUTHENTICATE 265
#define TIMEOUT 266
#define KPOP 267
#define SDPS 268
#define ENVELOPE 269
#define QVIRTUAL 270
#define USERNAME 271
#define PASSWORD 272
#define FOLDER 273
#define SMTPHOST 274
#define FETCHDOMAINS 275
#define MDA 276
#define BSMTP 277
#define LMTP 278
#define SMTPADDRESS 279
#define SMTPNAME 280
#define SPAMRESPONSE 281
#define PRECONNECT 282
#define POSTCONNECT 283
#define LIMIT 284
#define WARNINGS 285
#define INTERFACE 286
#define MONITOR 287
#define PLUGIN 288
#define PLUGOUT 289
#define IS 290
#define HERE 291
#define THERE 292
#define TO 293
#define MAP 294
#define WILDCARD 295
#define BATCHLIMIT 296
#define FETCHLIMIT 297
#define FETCHSIZELIMIT 298
#define FASTUIDL 299
#define EXPUNGE 300
#define PROPERTIES 301
#define SET 302
#define LOGFILE 303
#define DAEMON 304
#define SYSLOG 305
#define IDFILE 306
#define PIDFILE 307
#define INVISIBLE 308
#define POSTMASTER 309
#define BOUNCEMAIL 310
#define SPAMBOUNCE 311
#define SOFTBOUNCE 312
#define SHOWDOTS 313
#define PROTO 314
#define AUTHTYPE 315
#define STRING 316
#define NUMBER 317
#define NO 318
#define KEEP 319
#define FLUSH 320
#define LIMITFLUSH 321
#define FETCHALL 322
#define REWRITE 323
#define FORCECR 324
#define STRIPCR 325
#define PASS8BITS 326
#define DROPSTATUS 327
#define DROPDELIVERED 328
#define DNS 329
#define SERVICE 330
#define PORT 331
#define UIDL 332
#define INTERVAL 333
#define MIMEDECODE 334
#define IDLE 335
#define CHECKALIAS 336
#define SSL 337
#define SSLKEY 338
#define SSLCERT 339
#define SSLPROTO 340
#define SSLCERTCK 341
#define SSLCERTPATH 342
#define SSLCOMMONNAME 343
#define SSLFINGERPRINT 344
#define PRINCIPAL 345
#define ESMTPNAME 346
#define ESMTPPASSWORD 347
#define TRACEPOLLS 348




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 58 "rcfile_y.y"
{
  int proto;
  int number;
  char *sval;
}
/* Line 1489 of yacc.c.  */
#line 241 "rcfile_y.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

