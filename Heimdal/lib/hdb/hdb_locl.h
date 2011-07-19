/*
 * Copyright (c) 1997-2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* $Id$ */

#ifndef __HDB_LOCL_H__
#define __HDB_LOCL_H__

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <roken.h>

#include "crypto-headers.h"
#include <krb5.h>
#include <hdb.h>
#include <hdb-private.h>

krb5_error_code hdb_od_create(krb5_context, HDB **, const char *);
krb5_error_code hdb_keytab_create(krb5_context, HDB **, const char *);
krb5_error_code _hdb_keytab2hdb_entry(krb5_context, const krb5_keytab_entry *, hdb_entry_ex *);

#ifdef __APPLE__
#define HDB_DEFAULT_DB "od:/Local/Default"
#else
#define HDB_DEFAULT_DB HDB_DB_DIR "/heimdal"
#endif

#define HDB_DB_FORMAT_ENTRY "hdb/db-format"

#define KRB5_KDB_DISALLOW_POSTDATED	0x00000001
#define KRB5_KDB_DISALLOW_FORWARDABLE	0x00000002
#define KRB5_KDB_DISALLOW_TGT_BASED	0x00000004
#define KRB5_KDB_DISALLOW_RENEWABLE	0x00000008
#define KRB5_KDB_DISALLOW_PROXIABLE	0x00000010
#define KRB5_KDB_DISALLOW_DUP_SKEY	0x00000020
#define KRB5_KDB_DISALLOW_ALL_TIX	0x00000040
#define KRB5_KDB_REQUIRES_PRE_AUTH	0x00000080
#define KRB5_KDB_REQUIRES_HW_AUTH	0x00000100
#define KRB5_KDB_REQUIRES_PWCHANGE	0x00000200
#define KRB5_KDB_DISALLOW_SVR		0x00001000
#define KRB5_KDB_PWCHANGE_SERVICE	0x00002000
#define KRB5_KDB_SUPPORT_DESMD5		0x00004000
#define KRB5_KDB_NEW_PRINC		0x00008000

#undef ALLOC
#define ALLOC(X) ((X) = calloc(1, sizeof(*(X))))
#undef ALLOC_SEQ
#define ALLOC_SEQ(X, N) do { (X)->len = (N); \
(X)->val = calloc((X)->len, sizeof(*(X)->val)); } while(0)

#endif /* __HDB_LOCL_H__ */
