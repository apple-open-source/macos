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
     BADHEADER = 314,
     ACCEPT = 315,
     REJECT_ = 316,
     PROTO = 317,
     AUTHTYPE = 318,
     STRING = 319,
     NUMBER = 320,
     NO = 321,
     KEEP = 322,
     FLUSH = 323,
     LIMITFLUSH = 324,
     FETCHALL = 325,
     REWRITE = 326,
     FORCECR = 327,
     STRIPCR = 328,
     PASS8BITS = 329,
     DROPSTATUS = 330,
     DROPDELIVERED = 331,
     DNS = 332,
     SERVICE = 333,
     PORT = 334,
     UIDL = 335,
     INTERVAL = 336,
     MIMEDECODE = 337,
     IDLE = 338,
     CHECKALIAS = 339,
     SSL = 340,
     SSLKEY = 341,
     SSLCERT = 342,
     SSLPROTO = 343,
     SSLCERTCK = 344,
     SSLCERTFILE = 345,
     SSLCERTPATH = 346,
     SSLCOMMONNAME = 347,
     SSLFINGERPRINT = 348,
     PRINCIPAL = 349,
     ESMTPNAME = 350,
     ESMTPPASSWORD = 351,
     TRACEPOLLS = 352
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
#define BADHEADER 314
#define ACCEPT 315
#define REJECT_ 316
#define PROTO 317
#define AUTHTYPE 318
#define STRING 319
#define NUMBER 320
#define NO 321
#define KEEP 322
#define FLUSH 323
#define LIMITFLUSH 324
#define FETCHALL 325
#define REWRITE 326
#define FORCECR 327
#define STRIPCR 328
#define PASS8BITS 329
#define DROPSTATUS 330
#define DROPDELIVERED 331
#define DNS 332
#define SERVICE 333
#define PORT 334
#define UIDL 335
#define INTERVAL 336
#define MIMEDECODE 337
#define IDLE 338
#define CHECKALIAS 339
#define SSL 340
#define SSLKEY 341
#define SSLCERT 342
#define SSLPROTO 343
#define SSLCERTCK 344
#define SSLCERTFILE 345
#define SSLCERTPATH 346
#define SSLCOMMONNAME 347
#define SSLFINGERPRINT 348
#define PRINCIPAL 349
#define ESMTPNAME 350
#define ESMTPPASSWORD 351
#define TRACEPOLLS 352




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 58 "rcfile_y.y"
{
  int proto;
  int number;
  char *sval;
}
/* Line 1529 of yacc.c.  */
#line 249 "y.tab.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;

