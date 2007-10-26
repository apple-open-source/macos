%{
/*-
 * Copyright (c) 1995 Alex Tatmanjants <alex@elvisti.kiev.ua>
 *		at Electronni Visti IA, Kiev, Ukraine.
 *			All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/usr.bin/colldef/parse.y,v 1.31 2002/10/16 12:56:22 charnier Exp $");

#include <arpa/inet.h>
#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <unistd.h>
#include <sysexits.h>
#include <limits.h>
#include "collate.h"
#include "common.h"

#define PRI_UNDEFINED	(-1)
#define PRI_IGNORE	0
#define LINE_NONE	(-1)
#define LINE_NORMAL	0
#define LINE_ELLIPSIS	1
#define LINE_UNDEFINED	2
/* If UNDEFINED is specified with ellipses, we reposition prim_pri to
 * UNDEFINED_PRI, leaving gap for undefined characters. */
#define UNDEFINED_PRI	(COLLATE_MAX_PRIORITY - (COLLATE_MAX_PRIORITY >> 2))

extern FILE *yyin;
void yyerror(const char *fmt, ...) __printflike(1, 2);
int yyparse(void);
int yylex(void);
static void usage(void);
static void collate_print_tables(void);
static struct __collate_st_char_pri *getpri(int32_t);
static struct __collate_st_char_pri *haspri(int32_t);
static struct __collate_st_chain_pri *getchain(const wchar_t *, int);
static struct symbol *getsymbolbychar(wchar_t);
static struct symbol *hassymbolbychar(wchar_t);
static void setsymbolbychar(struct symbol *);
struct symbol *getstring(const wchar_t *);
static void makeforwardref(int, const struct symbol *, const struct symbol *);
static int charpricompar(const void *, const void *);
static int substcompar(const void *, const void *);
static int chainpricompar(const void *, const void *);
static void putsubst(int32_t, int, const wchar_t *);
static int hassubst(int32_t, int);
static const wchar_t *__collate_wcsnchr(const wchar_t *, wchar_t, int);
static int __collate_wcsnlen(const wchar_t *, int);
char *showwcs(const wchar_t *, int);
static char *charname(wchar_t);
static char *charname2(wchar_t);

char map_name[FILENAME_MAX] = ".";
wchar_t curr_chain[STR_LEN + 1];

char __collate_version[STR_LEN];
DB *charmapdb;
static DB *charmapdb2;
static DB *largemapdb;
static int nlargemap = 0;
static DB *substdb[COLL_WEIGHTS_MAX];
static int nsubst[COLL_WEIGHTS_MAX];
static DB *chaindb;
static int nchain = 0;
static DB *stringdb;
static DB *forward_ref[COLL_WEIGHTS_MAX];
static struct symbol *prev_weight_table[COLL_WEIGHTS_MAX];
static struct symbol *prev2_weight_table[COLL_WEIGHTS_MAX];
static struct symbol *weight_table[COLL_WEIGHTS_MAX];
static int prev_line = LINE_NONE;
static struct symbol *prev_elem;
static int weight_index = 0;
static int allow_ellipsis = 0;
static struct symbol sym_ellipsis = {SYMBOL_ELLIPSIS, PRI_UNDEFINED};
static struct symbol sym_ignore = {SYMBOL_IGNORE, PRI_IGNORE};
static struct symbol sym_undefined = {SYMBOL_CHAR, PRI_UNDEFINED};
static int order_pass = 0;

#undef __collate_char_pri_table
struct __collate_st_char_pri __collate_char_pri_table[UCHAR_MAX + 1];
struct __collate_st_chain_pri *__collate_chain_pri_table;
struct __collate_st_subst *__collate_substitute_table[COLL_WEIGHTS_MAX];
struct __collate_st_large_char_pri *__collate_large_char_pri_table;

int prim_pri = 2, sec_pri = 2;
#ifdef COLLATE_DEBUG
int debug;
#endif
struct __collate_st_info info = {{DIRECTIVE_FORWARD, DIRECTIVE_FORWARD}, 0, 0, 0, {PRI_UNDEFINED, PRI_UNDEFINED}};

/* Some of the code expects COLL_WEIGHTS_MAX == 2 */
int directive_count = COLL_WEIGHTS_MAX;

const char *out_file = "LC_COLLATE";
%}
%union {
	int32_t ch;
	wchar_t str[BUFSIZE];
}
%token SUBSTITUTE WITH
%token START_LC_COLLATE END_LC_COLLATE COLLATING_ELEMENT FROM COLLATING_SYMBOL
%token ELLIPSIS IGNORE UNDEFINED
%token ORDER RANGE ORDER_START ORDER_END ORDER_SECOND_PASS
%token <str> STRING
%token <str> DEFN
%token <str> ELEM
%token <ch> CHAR
%token <ch> ORDER_DIRECTIVE
%%
collate : datafile {
	FILE *fp;
	int localedef = (stringdb != NULL);
	int z;

	if (nchain > 0) {
		DBT key, val;
		struct __collate_st_chain_pri *t, *v;
		wchar_t *wp, *tp;
		int flags, i, len;

		if ((__collate_chain_pri_table = (struct __collate_st_chain_pri *)malloc(nchain * sizeof(struct __collate_st_chain_pri))) == NULL)
			err(1, "chain malloc");
		flags = R_FIRST;
		t = __collate_chain_pri_table;
		for(i = 0; i < nchain; i++) {
			if (chaindb->seq(chaindb, &key, &val, flags) != 0)
				err(1, "Can't retrieve chaindb %d", i);
			memcpy(&v, val.data, sizeof(struct __collate_st_chain_pri *));
			*t++ = *v;
			if ((len = __collate_wcsnlen(v->str, STR_LEN)) > info.chain_max_len)
				info.chain_max_len = len;
			flags = R_NEXT;
		}
		if (chaindb->seq(chaindb, &key, &val, flags) == 0)
			err(1, "More in chaindb after retrieving %d", nchain);
		qsort(__collate_chain_pri_table, nchain, sizeof(struct __collate_st_chain_pri), chainpricompar);
	}
	for(z = 0; z < directive_count; z++) {
		if (nsubst[z] > 0) {
			DBT key, val;
			struct __collate_st_subst *t;
			wchar_t *wp, *tp;
			int flags, i, j;
			int32_t cval;

			if ((__collate_substitute_table[z] = (struct __collate_st_subst *)calloc(nsubst[z], sizeof(struct __collate_st_subst))) == NULL)
				err(1, "__collate_substitute_table[%d] calloc", z);
			flags = R_FIRST;
			t = __collate_substitute_table[z];
			for(i = 0; i < nsubst[z]; i++) {
				if (substdb[z]->seq(substdb[z], &key, &val, flags) != 0)
					err(1, "Can't retrieve substdb[%d]", z);
				memcpy(&cval, key.data, sizeof(int32_t));
				/* we don't set the byte order of t->val, since we
				 * need it for sorting */
				t->val = cval;
				for(wp = (wchar_t *)val.data, tp = t->str, j = STR_LEN; *wp && j-- > 0;)
					*tp++ = htonl(*wp++);
				t++;
				flags = R_NEXT;
			}
			if (substdb[z]->seq(substdb[z], &key, &val, flags) == 0)
				err(1, "More in substdb[%d] after retrieving %d", z, nsubst[z]);
			qsort(__collate_substitute_table[z], nsubst[z], sizeof(struct __collate_st_subst), substcompar);
		}
	}
	if (nlargemap > 0) {
		DBT key, val;
		struct __collate_st_large_char_pri *t;
		struct __collate_st_char_pri *p;
		int flags, i, z;
		int32_t cval;

		if ((__collate_large_char_pri_table = (struct __collate_st_large_char_pri *)malloc(nlargemap * sizeof(struct __collate_st_large_char_pri))) == NULL)
			err(1, "nlargemap malloc");
		flags = R_FIRST;
		t = __collate_large_char_pri_table;
		for(i = 0; i < nlargemap; i++) {
			if (largemapdb->seq(largemapdb, &key, &val, flags) != 0)
				err(1, "Can't retrieve largemapdb %d", i);
			memcpy(&cval, key.data, sizeof(int32_t));
			memcpy(&p, val.data, sizeof(struct __collate_st_char_pri *));
			/* we don't set the byte order of t->val, since we
			 * need it for sorting */
			t->val = cval;
			for(z = 0; z < directive_count; z++)
				t->pri.pri[z] = htonl(p->pri[z]);
			t++;
			flags = R_NEXT;
		}
		if (largemapdb->seq(largemapdb, &key, &val, flags) == 0)
			err(1, "More in largemapdb after retrieving %d", nlargemap);
		qsort(__collate_large_char_pri_table, nlargemap, sizeof(struct __collate_st_large_char_pri), charpricompar);
	}

	if (info.undef_pri[0] == PRI_UNDEFINED) {
		int i;
		info.undef_pri[0] = prim_pri;
		for(i = 1; i < directive_count; i++)
			info.undef_pri[i] = -prim_pri;
	}

	if (localedef) {
		int ch, z, ret;
		if (sym_undefined.val == PRI_UNDEFINED) {
			int flags = R_FIRST;
			DBT key, val;
			struct symbol *v;
			while((ret = charmapdb->seq(charmapdb, &key, &val, flags)) == 0) {
				memcpy(&v, val.data, sizeof(struct symbol *));
				switch(v->type) {
				case SYMBOL_CHAR: {
					struct __collate_st_char_pri *p = haspri(v->u.wc);
					if (!p || p->pri[0] == PRI_UNDEFINED)
						warnx("<%s> was not defined", showwcs((const wchar_t *)key.data, key.size / sizeof(wchar_t)));
					break;
				}
				case SYMBOL_CHAIN: {
					struct __collate_st_chain_pri *p = getchain(v->u.str, EXISTS);
					if (p->pri[0] == PRI_UNDEFINED)
						warnx("<%s> was not defined", showwcs((const wchar_t *)key.data, key.size / sizeof(wchar_t)));
					break;
				}
				}
				flags = R_NEXT;
			}
			if (ret < 0)
				err(1, "Error retrieving from charmapdb");
		}
		for (ch = 1; ch < UCHAR_MAX + 1; ch++) {
			for(z = 0; z < directive_count; z++)
				if (__collate_char_pri_table[ch].pri[z] == PRI_UNDEFINED)
					__collate_char_pri_table[ch].pri[z] = (info.undef_pri[z] >= 0) ? info.undef_pri[z] : (ch - info.undef_pri[z]);
		}
		for (ch = 0; ch < nlargemap; ch++) {
			for(z = 0; z < directive_count; z++)
				if (__collate_large_char_pri_table[ch].pri.pri[z] == PRI_UNDEFINED)
					__collate_large_char_pri_table[ch].pri.pri[z] = (info.undef_pri[z] >= 0) ? info.undef_pri[z] : (__collate_large_char_pri_table[ch].val - info.undef_pri[z]);
		}
	} else {
		int ch, substed, ordered;
		int fatal = 0;
		for (ch = 1; ch < UCHAR_MAX + 1; ch++) {
			substed = hassubst(ch, 0);
			ordered = (__collate_char_pri_table[ch].pri[0] != PRI_UNDEFINED);
			if (!ordered && !substed) {
				fatal = 1;
				warnx("%s not found", charname(ch));
			}
			if (substed && ordered) {
				fatal = 1;
				warnx("%s can't be ordered since substituted", charname(ch));
			}
		}
		if (fatal)
			exit(1);
	}

	/* COLLATE_SUBST_DUP depends on COLL_WEIGHTS_MAX == 2 */
	if (localedef) {
		if (nsubst[0] == nsubst[1] && (nsubst[0] == 0 ||
		    memcmp(__collate_substitute_table[0], __collate_substitute_table[1], nsubst[0] * sizeof(struct __collate_st_subst)) == 0)) {
			info.flags |= COLLATE_SUBST_DUP;
			nsubst[1] = 0;
		}
	} else {
		info.flags |= COLLATE_SUBST_DUP;
		nsubst[1] = 0;
	}

	for(z = 0; z < directive_count; z++)
		info.subst_count[z] = nsubst[z];

	info.directive_count = directive_count;
	info.chain_count = nchain;
	info.large_pri_count = nlargemap;

	if ((fp = fopen(out_file, "w")) == NULL)
		err(EX_UNAVAILABLE, "can't open destination file %s",
		    out_file);

	strcpy(__collate_version, COLLATE_VERSION1_1A);
	if (fwrite(__collate_version, sizeof(__collate_version), 1, fp) != 1)
		err(EX_IOERR,
		"IO error writting collate version to destination file %s",
		    out_file);
#if __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
	for(z = 0; z < directive_count; z++) {
		info.undef_pri[z] = htonl(info.undef_pri[z]);
		info.subst_count[z] = htonl(info.subst_count[z]);
	}
	info.chain_count = htonl(info.chain_count);
	info.large_pri_count = htonl(info.large_pri_count);
#endif /* __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN */
	if (fwrite(&info, sizeof(info), 1, fp) != 1)
		err(EX_IOERR,
		"IO error writting collate info to destination file %s",
		    out_file);
#if __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
	{
		int i, z;
		struct __collate_st_char_pri *p = __collate_char_pri_table;

		for(i = UCHAR_MAX + 1; i-- > 0; p++) {
			for(z = 0; z < directive_count; z++)
				p->pri[z] = htonl(p->pri[z]);
		}
	}
#endif /* __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN */
	if (fwrite(__collate_char_pri_table,
		   sizeof(__collate_char_pri_table), 1, fp) != 1)
		err(EX_IOERR,
		"IO error writting char table to destination file %s",
		    out_file);
	for(z = 0; z < directive_count; z++) {
		if (nsubst[z] > 0) {
#if __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
			struct __collate_st_subst *t = __collate_substitute_table[z];
			int i;
			for(i = nsubst[z]; i > 0; i--) {
				t->val = htonl(t->val);
				t++;
			}
#endif /* __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN */
			if (fwrite(__collate_substitute_table[z], sizeof(struct __collate_st_subst), nsubst[z], fp) != nsubst[z])
				err(EX_IOERR,
				"IO error writting large substprim table %d to destination file %s",
				    z, out_file);
		}
	}
	if (nchain > 0) {
#if __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
		int i, j, z;
		struct __collate_st_chain_pri *p = __collate_chain_pri_table;
		wchar_t *w;

		for(i = nchain; i-- > 0; p++) {
			for(j = STR_LEN, w = p->str; *w && j-- > 0; w++)
				*w = htonl(*w);
			for(z = 0; z < directive_count; z++)
				p->pri[z] = htonl(p->pri[z]);
		}
#endif /* __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN */
		if (fwrite(__collate_chain_pri_table,
			   sizeof(*__collate_chain_pri_table), nchain, fp) !=
			   (size_t)nchain)
			err(EX_IOERR,
			"IO error writting chain table to destination file %s",
			    out_file);
	}

	if (nlargemap > 0) {
#if __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN
		struct __collate_st_large_char_pri *t = __collate_large_char_pri_table;
		int i;
		for(i = 0; i < nlargemap; i++) {
			t->val = htonl(t->val);
			t++;
		}
#endif /* __DARWIN_BYTE_ORDER == __DARWIN_LITTLE_ENDIAN */
		if (fwrite(__collate_large_char_pri_table, sizeof(struct __collate_st_large_char_pri), nlargemap, fp) != nlargemap)
			err(EX_IOERR,
			"IO error writting large pri tables to destination file %s",
			    out_file);
	}

	if (fclose(fp) != 0)
		err(EX_IOERR, "IO error closing destination file %s",
		    out_file);

#ifdef COLLATE_DEBUG
	if (debug)
		collate_print_tables();
#endif
	exit(EX_OK);
}
;
datafile : statment_list
	| blank_lines start_localedef localedef_sections blank_lines end_localedef blank_lines
;
statment_list : statment
	| statment_list '\n' statment
;
statment :
	| charmap
	| substitute
	| order
;
blank_lines :
	| '\n'
	| blank_lines '\n'
;
start_localedef : START_LC_COLLATE '\n' {
	int i;
	if ((stringdb = dbopen(NULL, O_CREAT | O_RDWR, 0600, DB_HASH, NULL)) == NULL)
		err(1, "dbopen stringdb");
	directive_count = 0;
	for(i = 0; i < COLL_WEIGHTS_MAX; i++)
		info.directive[i] = DIRECTIVE_UNDEF;
}
;
end_localedef : END_LC_COLLATE '\n'
;
localedef_sections : localedef_preface localedef_order
;
localedef_preface : localedef_statment '\n'
	| localedef_preface localedef_statment '\n'
;
localedef_statment :
	| charmap
	| collating_element
	| collating_symbol
;
collating_element : COLLATING_ELEMENT ELEM FROM STRING {
	int len;
	struct symbol *s;
	if (wcslen($2) > CHARMAP_SYMBOL_LEN)
		yyerror("collating-element symbol name '%s' is too long", showwcs($2, CHARMAP_SYMBOL_LEN));
	if ((len = wcslen($4)) > STR_LEN)
		yyerror("collating-element string '%s' is too long", showwcs($4, STR_LEN));
	if (len < 2)
		yyerror("collating-element string '%s' must be at least two characters", showwcs($4, STR_LEN));
	s = getsymbol($2, NOTEXISTS);
	s->val = PRI_UNDEFINED;
	s->type = SYMBOL_CHAIN;
	wcsncpy(s->u.str, $4, STR_LEN);
	getchain($4, NOTEXISTS);
}
;
collating_symbol : COLLATING_SYMBOL ELEM {
	struct symbol *s;
	if (wcslen($2) > CHARMAP_SYMBOL_LEN)
		yyerror("collating-element symbol name '%s' is too long", showwcs($2, CHARMAP_SYMBOL_LEN));
	s = getsymbol($2, NOTEXISTS);
	s->val = PRI_UNDEFINED;
	s->type = SYMBOL_SYMBOL;
}
;
localedef_order : order_start order_lines1 order_second_pass order_lines2 order_end
;
order_start: ORDER_START order_start_list '\n'
;
order_second_pass: ORDER_SECOND_PASS {
	prev_line = LINE_NONE;
	prev_elem = NULL;
	order_pass++;
}
;
order_start_list : order_start_list_directives {
	if (directive_count > 0)
		yyerror("Multiple order_start lines not allowed");
	if ((info.directive[0] & DIRECTIVE_DIRECTION_MASK) == 0)
		info.directive[0] |= DIRECTIVE_FORWARD;
	directive_count++;
}
	| order_start_list ';' order_start_list_directives {
	if (directive_count >= COLL_WEIGHTS_MAX)
		yyerror("only COLL_WEIGHTS_MAX weights allowed");
	if ((info.directive[directive_count] & DIRECTIVE_DIRECTION_MASK) == 0)
		info.directive[directive_count] |= DIRECTIVE_FORWARD;
	directive_count++;
}
;
order_start_list_directives : ORDER_DIRECTIVE {
	info.directive[directive_count] = $1;
}
	| order_start_list_directives ',' ORDER_DIRECTIVE {
	int direction = ($3 & DIRECTIVE_DIRECTION_MASK);
	int prev = (info.directive[directive_count] & DIRECTIVE_DIRECTION_MASK);
	if (direction && prev && direction != prev)
		yyerror("The forward and backward directives are mutually exclusive");
	info.directive[directive_count] |= $3;
}
;
order_lines1 : order_line1 '\n'
	| order_lines1 order_line1 '\n'
;
order_line1 :
	| ELEM {
	struct symbol *s = getsymbol($1, EXISTS);
	if (s->val != PRI_UNDEFINED)
		yyerror("<%s> redefined", showwcs($1, CHARMAP_SYMBOL_LEN));
	if (prev_line == LINE_ELLIPSIS) {
		struct symbol *m;
		wchar_t i;
		int v;
		switch (s->type) {
		case SYMBOL_CHAIN:
			yyerror("Chain <%s> can't be endpoints of ellipsis", showwcs($1, CHARMAP_SYMBOL_LEN));
		case SYMBOL_SYMBOL:
			yyerror("Collating symbol <%s> can't be endpoints of ellipsis", showwcs($1, CHARMAP_SYMBOL_LEN));
		}
		if (s->u.wc <= prev_elem->u.wc)
			yyerror("<%s> is before starting point of ellipsis", showwcs($1, CHARMAP_SYMBOL_LEN));
		for(i = prev_elem->u.wc + 1, v = prev_elem->val + 1; i < s->u.wc; i++, v++) {
			m = getsymbolbychar(i);
			if (m->val != PRI_UNDEFINED)
				yyerror("<%s> was previously defined while filling ellipsis symbols", showwcs(m->name, CHARMAP_SYMBOL_LEN));
			m->val = v;
		}
		s->val = v;
	} else
		s->val = prim_pri;
	prim_pri = s->val + 1;
	weight_index = 0;
}				weights {
	int i;
	struct symbol *s = getsymbol($1, EXISTS);
	if (s->type == SYMBOL_SYMBOL) {
		if (weight_index != 0)
			yyerror("Can't specify weights for collating symbol <%s>", showwcs($1, CHARMAP_SYMBOL_LEN));
	} else if (weight_index == 0) {
		for(i = 0; i < directive_count; i++)
			weight_table[i] = s;
	} else if (weight_index != directive_count)
		yyerror("Not enough weights specified");
	memcpy(prev_weight_table, weight_table, sizeof(weight_table));
	prev_line = LINE_NORMAL;
	prev_elem = s;
}
	| ELLIPSIS { weight_index = 0; allow_ellipsis = 1; } weights {
	int i;
	if (prev_line == LINE_ELLIPSIS)
		yyerror("Illegal sequential ellipsis lines");
	if (prev_line == LINE_UNDEFINED)
		yyerror("Ellipsis line can not follow UNDEFINED line");
	if (prev_line == LINE_NONE)
		yyerror("Ellipsis line must follow a collating identifier lines");
	if (weight_index == 0) {
		for(i = 0; i < directive_count; i++)
			weight_table[i] = &sym_ellipsis;
	} else if (weight_index != directive_count)
		yyerror("Not enough weights specified");
	for(i = 0; i < directive_count; i++) {
		if (weight_table[i]->type != SYMBOL_ELLIPSIS)
			continue;
		switch (prev_weight_table[i]->type) {
		case SYMBOL_CHAIN:
			yyerror("Startpoint of ellipsis can't be a collating element");
		case SYMBOL_IGNORE:
			yyerror("Startpoint of ellipsis can't be IGNORE");
		case SYMBOL_SYMBOL:
			yyerror("Startpoint of ellipsis can't be a collating symbol");
		case SYMBOL_STRING:
			yyerror("Startpoint of ellipsis can't be a string");
		}
	}
	memcpy(prev2_weight_table, prev_weight_table, sizeof(prev_weight_table));
	memcpy(prev_weight_table, weight_table, sizeof(weight_table));
	prev_line = LINE_ELLIPSIS;
	allow_ellipsis = 0;
}
	| UNDEFINED {
	if (sym_undefined.val != PRI_UNDEFINED)
		yyerror("Multiple UNDEFINED lines not allowed");
	sym_undefined.val = prim_pri++;
	weight_index = 0;
	allow_ellipsis = 1;
}				 weights {
	int i;
	if (weight_index == 0) {
		weight_table[0] = &sym_undefined;
		for(i = 1; i < directive_count; i++)
			weight_table[i] = &sym_ellipsis;
	} else if (weight_index != directive_count)
		yyerror("Not enough weights specified");
	memcpy(prev_weight_table, weight_table, sizeof(weight_table));
	prev_line = LINE_UNDEFINED;
}
;
order_lines2 : order_line2 '\n'
	| order_lines2 order_line2 '\n'
;
order_line2 :
	| ELEM { weight_index = 0; } weights {
	int i;
	struct symbol *s = getsymbol($1, EXISTS);
	if (s->val == PRI_UNDEFINED)
		yyerror("<%s> undefined", showwcs($1, CHARMAP_SYMBOL_LEN));
	if (s->type == SYMBOL_SYMBOL) {
		if (weight_index != 0)
			yyerror("Can't specify weights for collating symbol <%s>", showwcs($1, CHARMAP_SYMBOL_LEN));
	} else if (weight_index == 0) {
		for(i = 0; i < directive_count; i++)
			weight_table[i] = s;
	} else if (weight_index != directive_count)
		yyerror("Not enough weights specified");
	if (prev_line == LINE_ELLIPSIS) {
		int w, x;
		for(i = 0; i < directive_count; i++) {
			switch (prev_weight_table[i]->type) {
			case SYMBOL_CHAR:
			case SYMBOL_CHAIN:
			case SYMBOL_IGNORE:
			case SYMBOL_SYMBOL:
				for (w = prev_elem->u.wc + 1; w < s->u.wc; w++) {
					struct __collate_st_char_pri *p = getpri(w);
					if (p->pri[i] != PRI_UNDEFINED)
						yyerror("Char 0x02x previously defined", w);
					p->pri[i] = prev_weight_table[i]->val;
				}
				break;
			case SYMBOL_ELLIPSIS:

				switch (weight_table[i]->type) {
				case SYMBOL_STRING:
					yyerror("Strings can't be endpoints of ellipsis");
				case SYMBOL_CHAIN:
					yyerror("Chains can't be endpoints of ellipsis");
				case SYMBOL_IGNORE:
					yyerror("IGNORE can't be endpoints of ellipsis");
				case SYMBOL_SYMBOL:
					yyerror("Collation symbols can't be endpoints of ellipsis");
				}
				if (s->val - prev_elem->val != weight_table[i]->val - prev2_weight_table[i]->val)
					yyerror("Range mismatch in weight %d", i);
				x = prev2_weight_table[i]->val + 1;
				for (w = prev_elem->u.wc + 1; w < s->u.wc; w++) {
					struct __collate_st_char_pri *p = getpri(w);
					if (p->pri[i] != PRI_UNDEFINED)
						yyerror("Char 0x02x previously defined", w);
					p->pri[i] = x++;
				}
				break;
			case SYMBOL_STRING:
				for (w = prev_elem->u.wc + 1; w < s->u.wc; w++) {
					struct __collate_st_char_pri *p = getpri(w);
					if (p->pri[i] != PRI_UNDEFINED)
						yyerror("Char 0x02x previously defined", w);
					putsubst(w, i, prev_weight_table[i]->u.str);
					p->pri[i] = prev_weight_table[i]->val;
				}
				break;
			}
		}
	}
	switch(s->type) {
	case SYMBOL_CHAR: {
		struct __collate_st_char_pri *p = getpri(s->u.wc);
		for(i = 0; i < directive_count; i++) {
			switch (weight_table[i]->type) {
			case SYMBOL_CHAR:
			case SYMBOL_CHAIN:
			case SYMBOL_IGNORE:
			case SYMBOL_SYMBOL:
				if (p->pri[i] != PRI_UNDEFINED)
					yyerror("Char 0x02x previously defined", s->u.wc);
				p->pri[i] = weight_table[i]->val;
				break;
			case SYMBOL_STRING:
				if (p->pri[i] != PRI_UNDEFINED)
					yyerror("Char 0x02x previously defined", s->u.wc);
				putsubst(s->u.wc, i, weight_table[i]->u.str);
				p->pri[i] = weight_table[i]->val;
				break;
			}
		}
		break;
	}
	case SYMBOL_CHAIN: {
		struct __collate_st_chain_pri *p = getchain(s->u.str, EXISTS);
		for(i = 0; i < directive_count; i++) {
			switch (weight_table[i]->type) {
			case SYMBOL_CHAR:
			case SYMBOL_CHAIN:
			case SYMBOL_IGNORE:
			case SYMBOL_SYMBOL:
				if (p->pri[i] != PRI_UNDEFINED)
					yyerror("Chain %s previously defined", showwcs(s->u.str, STR_LEN));
				p->pri[i] = weight_table[i]->val;
				break;
			case SYMBOL_STRING :
				if (wcsncmp(s->u.str, weight_table[i]->u.str, STR_LEN) != 0)
					yyerror("Chain/string mismatch");
				if (p->pri[i] != PRI_UNDEFINED)
					yyerror("Chain %s previously defined", showwcs(s->u.str, STR_LEN));
				/* negative value mean don't substitute
				 * the chain, but it is in an
				 * equivalence class */
				p->pri[i] = -weight_table[i]->val;
			}
		}
		break;
	}
	}
	memcpy(prev_weight_table, weight_table, sizeof(weight_table));
	prev_line = LINE_NORMAL;
	prev_elem = s;
}
	| ELLIPSIS { weight_index = 0; allow_ellipsis = 1; } weights {
	int i;
	if (prev_line == LINE_ELLIPSIS)
		yyerror("Illegal sequential ellipsis lines");
	if (prev_line == LINE_UNDEFINED)
		yyerror("Ellipsis line can not follow UNDEFINED line");
	if (prev_line == LINE_NONE)
		yyerror("Ellipsis line must follow a collating identifier lines");
	if (weight_index == 0) {
		for(i = 0; i < directive_count; i++)
			weight_table[i] = &sym_ellipsis;
	} else if (weight_index != directive_count)
		yyerror("Not enough weights specified");
	for(i = 0; i < directive_count; i++) {
		if (weight_table[i]->type != SYMBOL_ELLIPSIS)
			continue;
		switch (prev_weight_table[i]->type) {
		case SYMBOL_CHAIN:
			yyerror("Startpoint of ellipsis can't be a collating element");
		case SYMBOL_IGNORE:
			yyerror("Startpoint of ellipsis can't be IGNORE");
		case SYMBOL_SYMBOL:
			yyerror("Startpoint of ellipsis can't be a collating symbol");
		case SYMBOL_STRING:
			yyerror("Startpoint of ellipsis can't be a string");
		}
	}
	memcpy(prev2_weight_table, prev_weight_table, sizeof(prev_weight_table));
	memcpy(prev_weight_table, weight_table, sizeof(weight_table));
	prev_line = LINE_ELLIPSIS;
	allow_ellipsis = 0;
}
	| UNDEFINED { weight_index = 0; allow_ellipsis = 1; } weights {
	int i;

	if (weight_index == 0) {
		weight_table[0] = &sym_undefined;
		for(i = 1; i < directive_count; i++)
			weight_table[i] = &sym_ellipsis;
	} else if (weight_index != directive_count)
		yyerror("Not enough weights specified");
	for(i = 0; i < directive_count; i++) {
		switch (weight_table[i]->type) {
		case SYMBOL_CHAR:
		case SYMBOL_CHAIN:
		case SYMBOL_IGNORE:
		case SYMBOL_SYMBOL:
			info.undef_pri[i] = weight_table[i]->val;
			break;
		case SYMBOL_ELLIPSIS :
			/* Negative values mean that the priority is
			 * relative to the lexical value */
			info.undef_pri[i] = -sym_undefined.val;
			prim_pri = UNDEFINED_PRI;
			break;
		case SYMBOL_STRING :
			yyerror("Strings can't be used with UNDEFINED");
		}
	}
	memcpy(prev_weight_table, weight_table, sizeof(weight_table));
	prev_line = LINE_UNDEFINED;
}
;
weights :
	| weight
	| weights ';' weight
;
weight : ELEM {
	struct symbol *s;
	if (weight_index >= directive_count)
		yyerror("More weights than specified by order_start");
	s = getsymbol($1, EXISTS);
	if (order_pass && s->val == PRI_UNDEFINED)
		yyerror("<%s> is undefined", showwcs($1, CHARMAP_SYMBOL_LEN));
	weight_table[weight_index++] = s;
}
	| ELLIPSIS {
	if (weight_index >= directive_count)
		yyerror("More weights than specified by order_start");
	if (!allow_ellipsis)
		yyerror("Ellipsis weight not allowed");
	weight_table[weight_index++] = &sym_ellipsis;
}
	| IGNORE {
	if (weight_index >= directive_count)
		yyerror("More weights than specified by order_start");
	weight_table[weight_index++] = &sym_ignore;
}
	| STRING {
	if (weight_index >= directive_count)
		yyerror("More weights than specified by order_start");
	if (wcslen($1) > STR_LEN)
		yyerror("String '%s' is too long", showwcs($1, STR_LEN));
	weight_table[weight_index++] = getstring($1);
}
;
order_end : ORDER_END '\n'
;
charmap : DEFN CHAR {
	int len = wcslen($1);
	struct symbol *s;
	if (len > CHARMAP_SYMBOL_LEN)
		yyerror("Charmap symbol name '%s' is too long", showwcs($1, CHARMAP_SYMBOL_LEN));
	s = getsymbol($1, NOTEXISTS);
	s->type = SYMBOL_CHAR;
	s->val = PRI_UNDEFINED;
	s->u.wc = $2;
	setsymbolbychar(s);
}
;
substitute : SUBSTITUTE CHAR WITH STRING {
	if (wcslen($4) + 1 > STR_LEN)
		yyerror("%s substitution is too long", charname($2));
	putsubst($2, 0, $4);
}
;
order : ORDER order_list
;
order_list : item
	| order_list ';' item
;
chain : CHAR CHAR {
	curr_chain[0] = $1;
	curr_chain[1] = $2;
	if (curr_chain[0] == '\0' || curr_chain[1] == '\0')
		yyerror("\\0 can't be chained");
	curr_chain[2] = '\0';
}
	| chain CHAR {
	static wchar_t tb[2];
	tb[0] = $2;
	if (tb[0] == '\0')
		yyerror("\\0 can't be chained");
	if (wcslen(curr_chain) + 1 > STR_LEN)
		yyerror("Chain '%s' grows too long", curr_chain);
	(void)wcscat(curr_chain, tb);
}
;
item :  CHAR {
	struct __collate_st_char_pri *p = getpri($1);
	if (p->pri[0] >= 0)
		yyerror("%s duplicated", charname($1));
	p->pri[0] = p->pri[1] = prim_pri;
	sec_pri = ++prim_pri;
}
	| chain {
	struct __collate_st_chain_pri *c = getchain(curr_chain, NOTEXISTS);
	c->pri[0] = c->pri[1] = prim_pri;
	sec_pri = ++prim_pri;
}
	| CHAR RANGE CHAR {
	u_int i;
	struct __collate_st_char_pri *p;

	if ($3 <= $1)
		yyerror("Illegal range %s -- %s", charname($1), charname2($3));

	for (i = $1; i <= $3; i++) {
		p = getpri(i);
		if (p->pri[0] >= 0)
			yyerror("%s duplicated", charname(i));
		p->pri[0] = p->pri[1] = prim_pri++;
	}
	sec_pri = prim_pri;
}
	| '{' mixed_order_list '}' {
	prim_pri = sec_pri;
}
	| '(' sec_order_list ')' {
	prim_pri = sec_pri;
}
;
mixed_order_list : mixed_sub_list {
	sec_pri++;
}
	| mixed_order_list ';' mixed_sub_list {
	sec_pri++;
}
;
mixed_sub_list : mixed_sub_item
	| mixed_sub_list ',' mixed_sub_item 
;
sec_order_list : sec_sub_item
	| sec_order_list ',' sec_sub_item 
;
mixed_sub_item : CHAR {
	struct __collate_st_char_pri *p = getpri($1);
	if (p->pri[0] >= 0)
		yyerror("%s duplicated", charname($1));
	p->pri[0] = prim_pri;
	p->pri[1] = sec_pri;
}
	| CHAR RANGE CHAR {
	u_int i;
	struct __collate_st_char_pri *p;

	if ($3 <= $1)
		yyerror("Illegal range %s -- %s",
			charname($1), charname2($3));

	for (i = $1; i <= $3; i++) {
		p = getpri(i);
		if (p->pri[0] >= 0)
			yyerror("%s duplicated", charname(i));
		p->pri[0] = prim_pri;
		p->pri[1] = sec_pri;
	}
}
	| chain {
	struct __collate_st_chain_pri *c = getchain(curr_chain, NOTEXISTS);
	c->pri[0] = prim_pri;
	c->pri[1] = sec_pri;
}
sec_sub_item : CHAR {
	struct __collate_st_char_pri *p = getpri($1);
	if (p->pri[0] >= 0)
		yyerror("%s duplicated", charname($1));
	p->pri[0] = prim_pri;
	p->pri[1] = sec_pri++;
}
	| CHAR RANGE CHAR {
	u_int i;
	struct __collate_st_char_pri *p;

	if ($3 <= $1)
		yyerror("Illegal range %s -- %s",
			charname($1), charname2($3));

	for (i = $1; i <= $3; i++) {
		p = getpri(i);
		if (p->pri[0] >= 0)
			yyerror("%s duplicated", charname(i));
		p->pri[0] = prim_pri;
		p->pri[1] = sec_pri++;
	}
}
	| chain {
	struct __collate_st_chain_pri *c = getchain(curr_chain, NOTEXISTS);
	c->pri[0] = prim_pri;
	c->pri[1] = sec_pri++;
}
;
%%
int
main(int ac, char **av)
{
	int ch, z;

	if ((charmapdb = dbopen(NULL, O_CREAT | O_RDWR, 0600, DB_HASH, NULL)) == NULL)
		err(1, "dbopen charmapdb");
	if ((charmapdb2 = dbopen(NULL, O_CREAT | O_RDWR, 0600, DB_HASH, NULL)) == NULL)
		err(1, "dbopen charmapdb");
	if ((largemapdb = dbopen(NULL, O_CREAT | O_RDWR, 0600, DB_HASH, NULL)) == NULL)
		err(1, "dbopen largemapdb");
	if ((substdb[0] = dbopen(NULL, O_CREAT | O_RDWR, 0600, DB_HASH, NULL)) == NULL)
		err(1, "dbopen substdb[0]");
	if ((chaindb = dbopen(NULL, O_CREAT | O_RDWR, 0600, DB_HASH, NULL)) == NULL)
		err(1, "dbopen chaindb");
	/* -1 means an undefined priority, which we adjust after parsing */
	for (ch = 0; ch <= UCHAR_MAX; ch++)
		for(z = 0; z < COLL_WEIGHTS_MAX; z++)
			__collate_char_pri_table[ch].pri[z] = PRI_UNDEFINED;
#ifdef COLLATE_DEBUG
	while((ch = getopt(ac, av, ":do:I:")) != -1) {
#else
	while((ch = getopt(ac, av, ":o:I:")) != -1) {
#endif
		switch (ch)
		{
#ifdef COLLATE_DEBUG
		  case 'd':
			debug++;
			break;
#endif
		  case 'o':
			out_file = optarg;
			break;

		  case 'I':
			strlcpy(map_name, optarg, sizeof(map_name));
			break;

		  default:
			usage();
		}
	}
	ac -= optind;
	av += optind;
	if (ac > 0) {
		if ((yyin = fopen(*av, "r")) == NULL)
			err(EX_UNAVAILABLE, "can't open source file %s", *av);
	}
	yyparse();
	return 0;
}

static struct __collate_st_char_pri *
getpri(int32_t c)
{
	DBT key, val;
	struct __collate_st_char_pri *p;
	int ret;

	if (c <= UCHAR_MAX)
		return &__collate_char_pri_table[c];
	key.data = &c;
	key.size = sizeof(int32_t);
	if ((ret = largemapdb->get(largemapdb, &key, &val, 0)) < 0)
		err(1, "getpri: Error getting %s", charname(c));
	if (ret != 0) {
		struct __collate_st_char_pri *pn;
		int z;
		if ((pn = (struct __collate_st_char_pri *)malloc(sizeof(struct __collate_st_char_pri))) == NULL)
			err(1, "getpri: malloc");
		for(z = 0; z < COLL_WEIGHTS_MAX; z++)
			pn->pri[z] = PRI_UNDEFINED;
		val.data = &pn;
		val.size = sizeof(struct __collate_st_char_pri *);
		if (largemapdb->put(largemapdb, &key, &val, 0) < 0)
			err(1, "getpri: Error storing %s", charname(c));
		nlargemap++;
	}
	memcpy(&p, val.data, sizeof(struct __collate_st_char_pri *));
	return p;
}

static struct __collate_st_char_pri *
haspri(int32_t c)
{
	DBT key, val;
	struct __collate_st_char_pri *p;
	int ret;

	if (c <= UCHAR_MAX)
		return &__collate_char_pri_table[c];
	key.data = &c;
	key.size = sizeof(int32_t);
	if ((ret = largemapdb->get(largemapdb, &key, &val, 0)) < 0)
		err(1, "haspri: Error getting %s", charname(c));
	if (ret != 0)
		return NULL;
	memcpy(&p, val.data, sizeof(struct __collate_st_char_pri *));
	return p;
}

static struct __collate_st_chain_pri *
getchain(const wchar_t *wcs, int exists)
{
	DBT key, val;
	struct __collate_st_chain_pri *p;
	int ret;

	key.data = (void *)wcs;
	key.size = __collate_wcsnlen(wcs, STR_LEN) * sizeof(wchar_t);
	if ((ret = chaindb->get(chaindb, &key, &val, 0)) < 0)
		err(1, "getchain: Error getting \"%s\"", showwcs(wcs, STR_LEN));
	if (ret != 0) {
		struct __collate_st_chain_pri *pn;
		int z;
		if (exists > 0)
			errx(1, "getchain: \"%s\" is not defined", showwcs(wcs, STR_LEN));
		if ((pn = (struct __collate_st_chain_pri *)malloc(sizeof(struct __collate_st_chain_pri))) == NULL)
			err(1, "getchain: malloc");
		for(z = 0; z < COLL_WEIGHTS_MAX; z++)
			pn->pri[z] = PRI_UNDEFINED;
		bzero(pn->str, sizeof(pn->str));
		wcsncpy(pn->str, wcs, STR_LEN);
		val.data = &pn;
		val.size = sizeof(struct __collate_st_chain_pri *);
		if (chaindb->put(chaindb, &key, &val, 0) < 0)
			err(1, "getchain: Error storing \"%s\"", showwcs(wcs, STR_LEN));
		nchain++;
	} else if (exists == 0)
		errx(1, "getchain: \"%s\" already exists", showwcs(wcs, STR_LEN));
	memcpy(&p, val.data, sizeof(struct __collate_st_chain_pri *));
	return p;
}

struct symbol *
getsymbol(const wchar_t *wcs, int exists)
{
	DBT key, val;
	struct symbol *p;
	int ret;

	key.data = (void *)wcs;
	key.size = wcslen(wcs) * sizeof(wchar_t);
	if ((ret = charmapdb->get(charmapdb, &key, &val, 0)) < 0)
		err(1, "getsymbol: Error getting \"%s\"", showwcs(wcs, CHARMAP_SYMBOL_LEN));
	if (ret != 0) {
		struct symbol *pn;
		if (exists > 0)
			errx(1, "getsymbol: \"%s\" is not defined", showwcs(wcs, CHARMAP_SYMBOL_LEN));
		if ((pn = (struct symbol *)malloc(sizeof(struct symbol))) == NULL)
			err(1, "getsymbol: malloc");
		pn->val = PRI_UNDEFINED;
		wcsncpy(pn->name, wcs, CHARMAP_SYMBOL_LEN);
		val.data = &pn;
		val.size = sizeof(struct symbol *);
		if (charmapdb->put(charmapdb, &key, &val, 0) < 0)
			err(1, "getsymbol: Error storing \"%s\"", showwcs(wcs, CHARMAP_SYMBOL_LEN));
	} else if (exists == 0)
		errx(1, "getsymbol: \"%s\" already exists", showwcs(wcs, CHARMAP_SYMBOL_LEN));
	memcpy(&p, val.data, sizeof(struct symbol *));
	return p;
}

static struct symbol *
getsymbolbychar(wchar_t wc)
{
	DBT key, val;
	struct symbol *p;
	int ret;

	key.data = &wc;
	key.size = sizeof(wchar_t);
	if ((ret = charmapdb2->get(charmapdb2, &key, &val, 0)) < 0)
		err(1, "getsymbolbychar: Error getting Char 0x%02x", wc);
	if (ret != 0)
		errx(1, "getsymbolbychar: Char 0x%02x is not defined", wc);
	memcpy(&p, val.data, sizeof(struct symbol *));
	return p;
}

static struct symbol *
hassymbolbychar(wchar_t wc)
{
	DBT key, val;
	struct symbol *p;
	int ret;

	key.data = &wc;
	key.size = sizeof(wchar_t);
	if ((ret = charmapdb2->get(charmapdb2, &key, &val, 0)) < 0)
		err(1, "hassymbolbychar: Error getting Char 0x%02x", wc);
	if (ret != 0)
		return NULL;
	memcpy(&p, val.data, sizeof(struct symbol *));
	return p;
}

static void
setsymbolbychar(struct symbol *s)
{
	DBT key, val;
	struct symbol *p;
	int ret;

	key.data = &s->u.wc;
	key.size = sizeof(wchar_t);
	val.data = &s;
	val.size = sizeof(struct symbol *);
	if (charmapdb2->put(charmapdb2, &key, &val, 0) < 0)
		err(1, "setsymbolbychar: Error storing <%s>", showwcs(s->name, CHARMAP_SYMBOL_LEN));
}

struct symbol *
getstring(const wchar_t *wcs)
{
	DBT key, val;
	struct symbol *p;
	int ret;

	key.data = (void *)wcs;
	key.size = wcslen(wcs) * sizeof(wchar_t);
	if ((ret = stringdb->get(stringdb, &key, &val, 0)) < 0)
		err(1, "getstring: Error getting \"%s\"", showwcs(wcs, STR_LEN));
	if (ret != 0) {
		struct symbol *pn;
		if ((pn = (struct symbol *)malloc(sizeof(struct symbol))) == NULL)
			err(1, "getstring: malloc");
		pn->type = SYMBOL_STRING;
		pn->val = prim_pri++;
		wcsncpy(pn->u.str, wcs, STR_LEN);
		val.data = &pn;
		val.size = sizeof(struct symbol *);
		if (stringdb->put(stringdb, &key, &val, 0) < 0)
			err(1, "getstring: Error storing \"%s\"", showwcs(wcs, STR_LEN));
	}
	memcpy(&p, val.data, sizeof(struct symbol *));
	return p;
}

static void
makeforwardref(int i, const struct symbol *from, const struct symbol * to)
{
}

static void
putsubst(int32_t c, int i, const wchar_t *str)
{
	DBT key, val;
	int ret;
	wchar_t clean[STR_LEN];

	if (!substdb[i])
		if ((substdb[i] = dbopen(NULL, O_CREAT | O_RDWR, 0600, DB_HASH, NULL)) == NULL)
			err(1, "dbopen substdb[%d]", i);
	key.data = &c;
	key.size = sizeof(int32_t);
	bzero(clean, sizeof(clean));
	wcsncpy(clean, str, STR_LEN);
	val.data = clean;
	val.size = sizeof(clean);
	if ((ret = substdb[i]->put(substdb[i], &key, &val, R_NOOVERWRITE)) < 0)
		err(1, "putsubst: Error on %s", charname(c));
	if (ret != 0)
		errx(1, "putsubst: Duplicate substitution of %s", charname(c));
	nsubst[i]++;
}

static int
hassubst(int32_t c, int i)
{
	DBT key, val;
	int ret;

	if (!substdb[i])
		return 0;
	key.data = &c;
	key.size = sizeof(int32_t);
	if ((ret = substdb[i]->get(substdb[i], &key, &val, 0)) < 0)
		err(1, "hassubst: Error getting %s", charname(c));
	return (ret == 0);
}

static int
chainpricompar(const void *a, const void *b)
{
	return wcsncmp(((struct __collate_st_chain_pri *)a)->str, ((struct __collate_st_chain_pri *)b)->str, STR_LEN);
}

static int
charpricompar(const void *a, const void *b)
{
	return ((struct __collate_st_large_char_pri *)a)->val - ((struct __collate_st_large_char_pri *)b)->val;
}

static int
substcompar(const void *a, const void *b)
{
	return ((struct __collate_st_subst *)a)->val - ((struct __collate_st_subst *)b)->val;
}

static const wchar_t *
__collate_wcsnchr(const wchar_t *s, wchar_t c, int len)
{
	while (*s && len > 0) {
		if (*s == c)
			return s;
		s++;
		len--;
	}
	return NULL;
}

static int
__collate_wcsnlen(const wchar_t *s, int len)
{
	int n = 0;
	while (*s && n < len) {
		s++;
		n++;
	}
	return n;
}

static void
usage(void)
{
	fprintf(stderr, "usage: colldef [-o out_file] [-I map_dir] [filename]\n");
	exit(EX_USAGE);
}

void
yyerror(const char *fmt, ...)
{
	va_list ap;
	char msg[128];

	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
	errx(EX_UNAVAILABLE, "%s, near line %d", msg, line_no);
}

char *
showwcs(const wchar_t *t, int len)
{
	static char buf[8* CHARMAP_SYMBOL_LEN];
	char *cp = buf;

	for(; *t && len > 0; len--, t++) {
		if (*t >=32 && *t <= 126)
			*cp++ = *t;
		else {
			sprintf(cp, "\\x{%02x}", *t);
			cp += strlen(cp);
		}
	}
	*cp = 0;
	return buf;
}

static char *
charname(wchar_t wc)
{
	static char buf[CHARMAP_SYMBOL_LEN + 1];
	struct symbol *s = hassymbolbychar(wc);

	if (s)
		strcpy(buf, showwcs(s->name, CHARMAP_SYMBOL_LEN));
	else
		sprintf(buf, "Char 0x%02x", wc);
	return buf;
}

static char *
charname2(wchar_t wc)
{
	static char buf[CHARMAP_SYMBOL_LEN + 1];
	struct symbol *s = hassymbolbychar(wc);

	if (s)
		strcpy(buf, showwcs(s->name, CHARMAP_SYMBOL_LEN));
	else
		sprintf(buf, "Char 0x%02x", wc);
	return buf;
}

#ifdef COLLATE_DEBUG
static char *
show(int c)
{
	static char buf[5];

	if (c >=32 && c <= 126)
		sprintf(buf, "'%c' ", c);
	else
		sprintf(buf, "\\x{%02x}", c);
	return buf;
}

static void
collate_print_tables(void)
{
	int i, z;

	printf("Info: p=%d s=%d f=0x%02x m=%d dc=%d up=%d us=%d pc=%d sc=%d cc=%d lc=%d\n",
	    info.directive[0], info.directive[1],
	    info.flags, info.chain_max_len,
	    info.directive_count,
	    info.undef_pri[0], info.undef_pri[1],
	    info.subst_count[0], info.subst_count[1],
	    info.chain_count, info.large_pri_count);
	for(z = 0; z < info.directive_count; z++) {
		if (info.subst_count[z] > 0) {
			struct __collate_st_subst *p2 = __collate_substitute_table[z];
			if (z == 0 && (info.flags & COLLATE_SUBST_DUP))
				printf("Both substitute tables:\n");
			else
				printf("Substitute table %d:\n", z);
			for (i = info.subst_count[z]; i-- > 0; p2++)
				printf("\t%s --> \"%s\"\n",
					show(p2->val),
					showwcs(p2->str, STR_LEN));
		}
	}
	if (info.chain_count > 0) {
		printf("Chain priority table:\n");
		struct __collate_st_chain_pri *p2 = __collate_chain_pri_table;
		for (i = info.chain_count; i-- > 0; p2++) {
			printf("\t\"%s\" :", showwcs(p2->str, STR_LEN));
			for(z = 0; z < info.directive_count; z++)
				printf(" %d", p2->pri[z]);
			putchar('\n');
		}
	}
	printf("Char priority table:\n");
	{
		struct __collate_st_char_pri *p2 = __collate_char_pri_table;
		for (i = 0; i < UCHAR_MAX + 1; i++, p2++) {
			printf("\t%s :", show(i));
			for(z = 0; z < info.directive_count; z++)
				printf(" %d", p2->pri[z]);
			putchar('\n');
		}
	}
	if (info.large_pri_count > 0) {
		struct __collate_st_large_char_pri *p2 = __collate_large_char_pri_table;
		printf("Large priority table:\n");
		for (i = info.large_pri_count; i-- > 0; p2++) {
			printf("\t%s :", show(p2->val));
			for(z = 0; z < info.directive_count; z++)
				printf(" %d", p2->pri.pri[z]);
			putchar('\n');
		}
	}
}
#endif
