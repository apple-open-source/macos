/* $Id: algorithm.h,v 1.8 2004/11/18 15:14:44 ludvigm Exp $ */

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _ALGORITHM_H
#define _ALGORITHM_H

#include "Algorithm_types.h"


struct hmac_algorithm {
	char *name;
	int type;
	int doi;
	caddr_t (*init) (vchar_t *);
	void (*update) (caddr_t, vchar_t *);
	vchar_t *(*final) (caddr_t);
	int (*hashlen) (void);
	vchar_t *(*one) (vchar_t *, vchar_t *);
};

struct hash_algorithm {
	char *name;
	int type;
	int doi;
	caddr_t (*init) (void);
	void (*update) (caddr_t, vchar_t *);
	vchar_t *(*final) (caddr_t);
	int (*hashlen) (void);
	vchar_t *(*one) (vchar_t *);
};

struct enc_algorithm {
	char *name;
	int type;
	int doi;
	int blocklen;
	vchar_t *(*encrypt) (vchar_t *, vchar_t *, vchar_t *);
	vchar_t *(*decrypt) (vchar_t *, vchar_t *, vchar_t *);
	int (*weakkey) (vchar_t *);
	int (*keylen) (int);
};

/* dh group */
struct dh_algorithm {
	char *name;
	int type;
	int doi;
	struct dhgroup *dhgroup;
};

/* ipcomp, auth meth, dh group */
struct misc_algorithm {
	char *name;
	int type;
	int doi;
};

extern int alg_oakley_hashdef_ok (int);
extern int alg_oakley_hashdef_doi (int);
extern int alg_oakley_hashdef_hashlen (int);
extern vchar_t *alg_oakley_hashdef_one (int, vchar_t *);

extern int alg_oakley_hmacdef_doi (int);
extern vchar_t *alg_oakley_hmacdef_one (int, vchar_t *, vchar_t *);

extern int alg_oakley_encdef_ok (int);
extern int alg_oakley_encdef_doi (int);
extern int alg_oakley_encdef_keylen (int, int);
extern int alg_oakley_encdef_blocklen (int);
extern vchar_t *alg_oakley_encdef_decrypt (int, vchar_t *, vchar_t *, vchar_t *);
extern vchar_t *alg_oakley_encdef_encrypt (int, vchar_t *, vchar_t *, vchar_t *);

extern int alg_ipsec_encdef_doi (int);
extern int alg_ipsec_encdef_keylen (int, int);

extern int alg_ipsec_hmacdef_doi (int);
extern int alg_ipsec_hmacdef_hashlen (int);

extern int alg_ipsec_compdef_doi (int);

extern int alg_oakley_dhdef_doi (int);
extern int alg_oakley_dhdef_ok (int);
extern struct dhgroup *alg_oakley_dhdef_group (int);

extern int alg_oakley_authdef_doi (int);

extern int default_keylen (int, int);
extern int check_keylen (int, int, int);
extern int algtype2doi (int, int);
extern int algclass2doi (int);

extern const char *alg_oakley_encdef_name (int);
extern const char *alg_oakley_hashdef_name (int);
extern const char *alg_oakley_dhdef_name (int);
extern const char *alg_oakley_authdef_name (int);

#endif /* _ALGORITHM_H */
