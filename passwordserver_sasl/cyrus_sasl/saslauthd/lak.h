/* COPYRIGHT
 * Copyright (c) 2002-2002 Igor Brezac
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY IGOR BREZAC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL IGOR BREZAC OR
 * ITS EMPLOYEES OR AGENTS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 * END COPYRIGHT */

#ifndef _LAK_H
#define _LAK_H

#include <ldap.h>
#include <lber.h>

#define LAK_OK 0
#define LAK_FAIL -1
#define LAK_NOENT -2

typedef struct lak {
    char   *servers;
    char   *bind_dn;
    char   *bind_pw;
    struct  timeval timeout;
    int     sizelimit;
    int     timelimit;
    int     deref;
    int     referrals;
    long    cache_expiry;
    long    cache_size;
    int     scope;
    char   *search_base;
    char   *filter;
    int     debug;
    int     verbose;
} LAK;

typedef struct lak_conn {
    char    bound;
    LDAP   *ld;
} LAK_CONN;

typedef struct lak_result {
    char              *attribute;
    char              *value;
    size_t             len;
    struct lak_result *next;
} LAK_RESULT;

int lak_lookup(LAK *, char *, char *, char **, LAK_RESULT **);
int lak_authenticate(LAK *, char *, char *, char *);
void lak_close(LAK *, LAK_CONN *);
void lak_free(LAK_RESULT *);

#endif  /* _LAK_H */
