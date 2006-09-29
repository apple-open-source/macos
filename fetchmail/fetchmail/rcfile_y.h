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
     SHOWDOTS = 312,
     PROTO = 313,
     AUTHTYPE = 314,
     STRING = 315,
     NUMBER = 316,
     NO = 317,
     KEEP = 318,
     FLUSH = 319,
     LIMITFLUSH = 320,
     FETCHALL = 321,
     REWRITE = 322,
     FORCECR = 323,
     STRIPCR = 324,
     PASS8BITS = 325,
     DROPSTATUS = 326,
     DROPDELIVERED = 327,
     DNS = 328,
     SERVICE = 329,
     PORT = 330,
     UIDL = 331,
     INTERVAL = 332,
     MIMEDECODE = 333,
     IDLE = 334,
     CHECKALIAS = 335,
     SSL = 336,
     SSLKEY = 337,
     SSLCERT = 338,
     SSLPROTO = 339,
     SSLCERTCK = 340,
     SSLCERTPATH = 341,
     SSLFINGERPRINT = 342,
     PRINCIPAL = 343,
     ESMTPNAME = 344,
     ESMTPPASSWORD = 345,
     TRACEPOLLS = 346
   };
#endif
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
#define SHOWDOTS 312
#define PROTO 313
#define AUTHTYPE 314
#define STRING 315
#define NUMBER 316
#define NO 317
#define KEEP 318
#define FLUSH 319
#define LIMITFLUSH 320
#define FETCHALL 321
#define REWRITE 322
#define FORCECR 323
#define STRIPCR 324
#define PASS8BITS 325
#define DROPSTATUS 326
#define DROPDELIVERED 327
#define DNS 328
#define SERVICE 329
#define PORT 330
#define UIDL 331
#define INTERVAL 332
#define MIMEDECODE 333
#define IDLE 334
#define CHECKALIAS 335
#define SSL 336
#define SSLKEY 337
#define SSLCERT 338
#define SSLPROTO 339
#define SSLCERTCK 340
#define SSLCERTPATH 341
#define SSLFINGERPRINT 342
#define PRINCIPAL 343
#define ESMTPNAME 344
#define ESMTPPASSWORD 345
#define TRACEPOLLS 346




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 58 "../rcfile_y.y"
typedef union YYSTYPE {
  int proto;
  int number;
  char *sval;
} YYSTYPE;
/* Line 1249 of yacc.c.  */
#line 224 "rcfile_y.h"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;



