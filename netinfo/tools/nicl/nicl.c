/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * nicl: NetInfo command-line interface
 *
 * Written by Marc Majka
 * dsx500 bits by Luke Howard
 * Copyright © 1998, 1999, 2000, Apple Computer.
 */

#include <NetInfo/dsengine.h>
#include <NetInfo/dsindex.h>
#include <NetInfo/dsutil.h>
#include <NetInfo/dsdata.h>
#include <NetInfo/dsrecord.h>
#include <NetInfo/dsx500.h>
#include <NetInfo/dsx500dit.h>
#include <NetInfo/nilib2.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <netinfo/ni.h>
#include <histedit.h>

#define streq(A,B) (strcmp(A,B) == 0)
#define forever for(;;)

#define PROMPT_NONE 0
#define PROMPT_PLAIN 1
#define PROMPT_NI 2
#define PROMPT_X500 3
#define PROMPT_DOT 4

#define VT100_BOLD "\e[1m"
#define VT100_NORM "\e[0m"

static char myname[256];
dsengine *engine = NULL;
static int verbose = 0;
static int interactive = 0;
static u_int32_t current_dir = 0;
static int prompt = PROMPT_NONE;
static int do_bold = 0;
static int do_notify = 0;
static int mode = 0;
static int raw = 0;
static char *term_bold = NULL;
static char *term_norm = NULL;

#define INPUT_LENGTH 4096

#define MODE_NETINFO 0
#define MODE_X500 1

#define ATTR_CREATE 0
#define ATTR_APPEND 1
#define ATTR_MERGE  2

#define META_IS_UNDERSCORE

static int minargs[] =
{
   -1,  /* noop */
	1,  /* create */
	1,  /* delete */
	3,  /* rename */
	1,  /* read */
	1,  /* list */
	3,  /* append */
	3,  /* merge */
	4,  /* insert */
	2,  /* move */
	2,  /* copy */
	4,  /* search */
	1,  /* path */
	2,  /* attach */
	2,  /* detach */
	2,  /* parent */
	1,  /* cd */
	0,  /* pwd */
	0,  /* version history */
	0,  /* stats */
	0,  /* domain name */
	0,  /* rparent */
	0,  /* resync */
	1,  /* authenticate */
	0,  /* refs */
	2,  /* setrdn */
	1,  /* source file */
	0,  /* echo */
	0,  /* flush */
	1   /* load */
};

#define OP_NOOP 0
#define OP_CREATE 1
#define OP_DELETE 2
#define OP_RENAME 3
#define OP_READ 4
#define OP_LIST 5
#define OP_APPEND 6
#define OP_MERGE 7
#define OP_INSERT 8
#define OP_MOVE 9
#define OP_COPY 10
#define OP_SEARCH 11
#define OP_PATH 12
#define OP_ATTACH 13
#define OP_DETACH 14
#define OP_PARENT 15
#define OP_CD 16
#define OP_PWD 17
#define OP_VHISTORY 18
#define OP_STATS 19
#define OP_DOMAINNAME 20
#define OP_RPARENT 21
#define OP_RESYNC 22
#define OP_AUTH 23
#define OP_REFS 24
#define OP_SETRDN 25
#define OP_SOURCE 26
#define OP_ECHO 27
#define OP_FLUSH 28
#define OP_LOAD 29

#define OP_CREATE_DIR 1000
#define OP_WRITE_ATTR 1001
#define OP_DELETE_DIR 2000
#define OP_DELETE_ATTR 2001

int nifty_interactive(FILE *, int);

void usage()
{
	fprintf(stderr, "usage: %s [options] <datasource> [<command>]\n", myname);
	fprintf(stderr, "options:\n");
	fprintf(stderr, "    -c             create a new datasource\n");
	fprintf(stderr, "    -ro            read-only\n");
	fprintf(stderr, "    -p             prompt for password\n");
	fprintf(stderr, "    -u <user>      authenticate as user\n");
	fprintf(stderr, "    -P <password>  authentication password\n");
	fprintf(stderr, "    -raw           datasource is a NetInfo directory\n");
	fprintf(stderr, "    -t             datasource is <host>/<tag>\n");
	fprintf(stderr, "    -q             quiet - no interactive prompt\n");
	fprintf(stderr, "    -x500          X.500 names\n");
	fprintf(stderr, "commands:\n");
	fprintf(stderr, "    -read    <path> [<key>...]\n");
	fprintf(stderr, "    -create [<path> [<key> [<val>...]]]\n");
	fprintf(stderr, "    -delete  <path> [<key> [<val>...]]\n");
	fprintf(stderr, "    -rename  <path> <old_key> <new_key>\n");
	fprintf(stderr, "    -list    <path> [<key>]\n");
	fprintf(stderr, "    -append  <path> <key> <val>...\n");
	fprintf(stderr, "    -merge   <path> <key> <val>...\n");
	fprintf(stderr, "    -insert  <path> <key> <val> <index>\n");
	fprintf(stderr, "    -move    <path> <new_parent>\n");
	fprintf(stderr, "    -copy    <path> <new_parent>\n");
	fprintf(stderr, "    -search  <path> <scopemin> <scopemax> (<key> <val>)...\n"); 
	fprintf(stderr, "    -path    <path>\n");
	fprintf(stderr, "    -load    [<delim> <key> <val>...]...\n");
	fprintf(stderr, "    -setrdn  <path> <key> <val>\n");
	fprintf(stderr, "    -history [<=>] [<version>]\n");
	fprintf(stderr, "    -stats\n");
	fprintf(stderr, "    -domainname\n");
	fprintf(stderr, "    -rparent\n");
	fprintf(stderr, "    -resync\n");
	fprintf(stderr, "    -flush\n");
	fprintf(stderr, "    -auth    <user> [<password>]\n");
	fprintf(stderr, "    -refs\n");
	fprintf(stderr, "    -source  <file>\n");
	fprintf(stderr, "    -echo    <string>\n");

#ifdef META_IS_DASH_M
	fprintf(stderr, "Preface <key> with \"-m\" for meta-attributes\n");
#endif
	fprintf(stderr, "\n");
#ifdef DANGEROUS
	fprintf(stderr, "These command are for emergency repairs only.\n");
	fprintf(stderr, "Use them with extreme caution!\n");
	fprintf(stderr, "    -attach  <path> <id>\n");
	fprintf(stderr, "    -detach  <path> <id>\n");
	fprintf(stderr, "    -parent  <path> <parent_id>\n");
#endif
}

void
nifty_print_dsattribute(dsattribute *a, int meta, FILE *f, int space)
{
	int i;
	char buf[64];
	dsdata *name;

	if (a == NULL)
	{
		
		for (i = 0; i < space; i++) fprintf(f, " ");
		fprintf(f, "-nil-\n");
		return;
	}

	for (i = 0; i < space; i++) fprintf(f, " ");
#ifdef META_IS_UNDERSCORE
	if (meta) fprintf(f, "_");
#endif
	dsdata_print(a->key, f);
	fprintf(f, ":");
	
	for (i = 0; i < a->count; i++)
	{
		fprintf(f, " ");
		if (a->value[i]->type == DataTypeDirectoryID)
		{
			name = dsengine_map_name(engine, a->value[i], NameTypeDirectoryID);
			if (name == NULL)
			{
				sprintf(buf, "DSID=%u", dsdata_to_dsid(a->value[i]));
				name = cstring_to_dsdata(buf);
			}
			dsdata_print(name, f);
			dsdata_release(name);
		}
		else
		{
			dsdata_print(a->value[i], f);
		}
	}
}

void
nifty_print_dsrecord(dsrecord *r, FILE *f, u_int32_t detail)
{
	int i, space, doit;

	if (r == NULL)
	{
		fprintf(f, "-nil-\n");
		return;
	}

#ifdef META_IS_DASH_M
	doit = 1;
#else
	doit = 0;
#endif

	space = 0;

	if (detail >= 1)
	{
		doit = 1;
		fprintf(f, "Record: %u   Version: %u   Serial: %u\n",
			r->dsid, r->vers, r->serial);
		fprintf(f, "Parent: %u\n", r->super);
		if (r->sub_count > 0)
		{
			fprintf(f, "Children (%u):", r->sub_count);
			for (i = 0; i < r->sub_count; i++) fprintf(f, " %u", r->sub[i]);
			fprintf(f, "\n");
		}
	}

	if (doit)
	{
		space = 4;
		if (r->count > 0) fprintf(f, "Attributes:\n");
	}

	for (i = 0; i < r->count; i++)
	{
		nifty_print_dsattribute(r->attribute[i], 0, f, space);
		fprintf(f, "\n");
	}

	if (doit)
	{
		if (r->meta_count > 0) fprintf(f, "Meta-attributes:\n");
	}

	for (i = 0; i < r->meta_count; i++)
	{
		nifty_print_dsattribute(r->meta_attribute[i], 1, f, space);
		fprintf(f, "\n");
	}

	if (detail >= 2)
	{
		if (r->index != NULL)
		{
			fprintf(f, "Index:\n");
			dsindex_print(r->index, f);
			fprintf(f, "\n");
		}
	}

	if (r->next != NULL)
	{
		fprintf(f, "\n");
		nifty_print_dsrecord(r->next, f, detail);
	}
}

dsstatus
nifty_path(char *s, u_int32_t start, u_int32_t *dsid)
{
	dsstatus status;

	if (mode == MODE_X500) 
		status = dsengine_x500_string_pathmatch(engine, start, s, dsid);
	else
		status = dsengine_netinfo_string_pathmatch(engine, start, s, dsid);
	
	if (status != DSStatusOK) return status;
	return DSStatusOK;
}

dsstatus
nifty_pathcreate(char *s, u_int32_t *dsid)
{
	dsstatus status;
	dsrecord *r;
	u_int32_t i, numeric;

	numeric = 1;
	for (i = 0; (numeric == 1) && (s[i] != '\0'); i++)
	{
		if ((s[i] < '0') || (s[i] > '9')) numeric = 0;
	}

	if (numeric == 1)
	{
		*dsid = atoi(s);
		if ((*dsid == 0) && (strcmp(s, "0"))) return DSStatusInvalidRecordID;
		return DSStatusOK;
	}

	if (s[0] == '/') *dsid = 0;
	if (mode == MODE_X500)
		r = dsutil_parse_x500_string_path(s);
	else
		r = dsutil_parse_netinfo_string_path(s);
	
	status = dsengine_pathcreate(engine, *dsid, r, dsid);
	dsrecord_release(r);
	return status;
}

dsdata *
nifty_getkey(int argc, char *argv[], int *x, int *meta)
{
	dsdata *k;

	*x = 0;
	*meta = 0;

	if (argc == 0)
	{
		fprintf(stderr, "Error: expecting a key\n");
		return NULL;
	}

#ifdef META_IS_DASH_M
	if (streq(argv[0], "-m"))
	{
		*x = 1;
		*meta = 1;
	}

	if (argc < 1)
	{
		fprintf(stderr, "Error: expecting a meta-key\n");
		return NULL;
	}

	k = cstring_to_dsdata(argv[1]);
	*x = 2;
#else
	*x = 1;

	if (argv[0][0] == '_')
	{
		*meta = 1;
	}

	k = cstring_to_dsdata(argv[0] + *meta);
#endif

	return k;
}
	
dsdata *
nifty_getval(int argc, char *argv[], int *x)
{
	dsdata *v;

	*x = 0;
	if (argc == 0)
	{
		fprintf(stderr, "Error: expecting a value\n");
		return NULL;
	}

	*x += 1;
	v = cstring_to_dsdata(argv[0]);

	return v;
}

dsstatus
nifty_load(u_int32_t dsid, int argc, char *argv[])
{
	/* @key val... @key val... ... */
	/* ! key val... !key val... ... */
	/* # key val...  # key val... ... */
	/* First char defines delimeter */
	/* backslash escapes delimeter */

	dsrecord *r;
	dsattribute *a;
	dsdata *k, *v;
	dsstatus status;
	u_int32_t i, off, meta;
	char delim, *ks;

	if (argc == 0) return DSStatusOK;

	delim = argv[0][0];

	k = NULL;
	ks = NULL;
	a = NULL;
	r = dsrecord_new();

	for (i = 0; i < argc; i++)
	{
		if (argv[i][0] == delim)
		{
			if (a != NULL)
			{
				dsattribute_release(a);
				a = NULL;
			}

			/* a new key */
			ks = argv[i] + 1;

			if (argv[i][1] == '\0')
			{
				i++;
				if (i >= argc) break;
				ks = argv[i];
			}

			meta = SELECT_ATTRIBUTE;
			if (streq(ks, "-m"))
			{
				meta = SELECT_META_ATTRIBUTE;
				i++;
				if (i >= argc) break;
				ks = argv[i];
			}

			if ((ks[0] == '\\') && (ks[1] == delim)) ks++;

			k = cstring_to_dsdata(ks);
			dsrecord_remove_key(r, k, meta);
			a = dsattribute_new(k);
			dsrecord_append_attribute(r, a, meta);
			dsdata_release(k);
		}
		else
		{
			off = 0;
			if ((argv[i][0] == '\\') && (argv[i][1] == delim)) off = 1;
			v = cstring_to_dsdata(argv[i] + off);
			dsattribute_append(a, v);
			dsdata_release(v);
		}
	}

	status = dsengine_create(engine, r, dsid);
	if (a != NULL) dsattribute_release(a);
	dsrecord_release(r);
	if (status != DSStatusOK)
		fprintf(stderr, "%s\n", dsstatus_message(status));
	return status;
}

dsstatus
nifty_attribute(u_int32_t dsid, int flag, int argc, char *argv[])
{
	/* <path> [<key> [<val>...]] */

	dsrecord *r;
	dsattribute *a;
	dsdata *k, *v;
	dsstatus status;
	u_int32_t i, x, meta;

	if (argc == 0) return DSStatusOK;

	status = dsengine_fetch(engine, dsid, &r);
	if (status != DSStatusOK)
	{
		fprintf(stderr, "Fetch record %u failed: %s\n",
			dsid, dsstatus_message(status));
		return status;
	}

	k = nifty_getkey(argc, argv, &x, &meta);
	if (k == NULL)
	{
		fprintf(stderr, "%s\n", dsstatus_message(DSStatusInvalidKey));
		dsrecord_release(r);
		return DSStatusInvalidKey;
	}

	a = dsrecord_attribute(r, k, meta);
	if (a == NULL) flag = ATTR_CREATE;

	if (flag == ATTR_CREATE)
	{
		dsrecord_remove_key(r, k, meta);
		dsattribute_release(a);
		a = dsattribute_new(k);
		dsrecord_append_attribute(r, a, meta);
	}

	dsdata_release(k);

	for (i = x; i < argc; i++)
	{
		v = cstring_to_dsdata(argv[i]);
		switch (flag)
		{
			case ATTR_CREATE:
			case ATTR_APPEND:
				dsattribute_append(a, v);
				break;
			case ATTR_MERGE:
				dsattribute_merge(a, v);
				break;
		}
		dsdata_release(v);
	}
	

	status = dsengine_save_attribute(engine, r, a, meta);
	dsattribute_release(a);
	dsrecord_release(r);
	if (status != DSStatusOK)
		fprintf(stderr, "%s\n", dsstatus_message(status));
	return status;
}

dsstatus
nifty_create(u_int32_t dsid, int argc, char *argv[])
{
	return nifty_attribute(dsid, ATTR_CREATE, argc, argv);
}

dsstatus
nifty_append(u_int32_t dsid, int argc, char *argv[])
{
	return nifty_attribute(dsid, ATTR_APPEND, argc, argv);
}

dsstatus
nifty_merge(u_int32_t dsid, int argc, char *argv[])
{
	return nifty_attribute(dsid, ATTR_MERGE, argc, argv);
}

dsstatus
nifty_delete(u_int32_t dsid, int argc, char *argv[])
{
	/* <path> [<key> [<val>...]] */
	
	u_int32_t i, x, meta, where, super;
	dsrecord *r;
	dsdata *d;
	dsattribute *a;
	dsstatus status;

	if (argc == 0)
	{
		if (dsid == 0)
		{
			fprintf(stderr, "Can't delete root!\n");
			return DSStatusInvalidRecordID;
		}

		/* remove the whole record */
		status = dsengine_record_super(engine, dsid, &super);
		if (status != DSStatusOK) 
		{
			fprintf(stderr, "Remove: %s\n", dsstatus_message(status));
			return status;
		}

		if (dsid == current_dir) current_dir = super;
	
		status = dsengine_remove(engine, dsid);
		if (status != DSStatusOK) 
			fprintf(stderr, "Remove: %s\n", dsstatus_message(status));
		return status;
	}

	status = dsengine_fetch(engine, dsid, &r);
	if (status != DSStatusOK)
	{
		fprintf(stderr, "Fetch record %u failed: %s\n",
			dsid, dsstatus_message(status));
		return status;
	}

	d = nifty_getkey(argc, argv, &x, &meta);
	if (d == NULL)
	{
		fprintf(stderr, "%s\n", dsstatus_message(DSStatusInvalidKey));
		dsrecord_release(r);
		return DSStatusInvalidKey;
	}

	if (argc == x)
	{
		/* remove an attribute */
		dsrecord_remove_key(r, d, meta);
		dsdata_release(d);

		status = dsengine_save(engine, r);
		if (status != DSStatusOK) 
			fprintf(stderr, "Remove: %s\n", dsstatus_message(status));

		dsrecord_release(r);
		return status;
	}
	
	/* Remove values */
	a = dsrecord_attribute(r, d, meta);
	dsdata_release(d);
	if (a == NULL)
	{
		fprintf(stderr, "No such key: %s\n", argv[x - 1]);
		dsrecord_release(r);
		return DSStatusInvalidKey;
	}

	for (i = x; i < argc; i++)
	{
		d = cstring_to_dsdata(argv[i]);
		where = dsattribute_index(a, d);
		dsdata_release(d);
		if (where == IndexNull) continue;
		dsattribute_remove(a, where);
	}

	status = dsengine_save(engine, r);
	if (status != DSStatusOK) 
	{
		fprintf(stderr, "Remove: %s\n", dsstatus_message(status));
		return status;
	}

	dsattribute_release(a);
	dsrecord_release(r);
	return status;
}

dsstatus
nifty_rename(u_int32_t dsid, int argc, char *argv[])
{
	/* <path> <oldkey> <newkey> */

	dsrecord *r;
	dsdata *k;
	dsattribute *a;
	dsstatus status;
	u_int32_t meta, x;

	status = dsengine_fetch(engine, dsid, &r);
	if (status != DSStatusOK)
	{
		fprintf(stderr, "Fetch record %u failed: %s\n",
			dsid, dsstatus_message(status));
		return status;
	}

	k = nifty_getkey(argc, argv, &x, &meta);
	if (k == NULL)
	{
		fprintf(stderr, "usage: %s -rename path oldkey newkey\n", myname);
		return DSStatusInvalidKey;
	}

	a = dsrecord_attribute(r, k, meta);
	dsdata_release(k);

	if (a == NULL)
	{
		fprintf(stderr, "No such key: %s\n", argv[x - 1]);
		dsrecord_release(r);
		return DSStatusInvalidKey;
	}

	argc -= x;
	argv += x;
	k = nifty_getkey(argc, argv, &x, &meta);
	if (k == NULL)
	{
		fprintf(stderr, "usage: %s -rename path oldkey newkey\n", myname);
		return DSStatusInvalidKey;
	}

	dsattribute_setkey(a, k);
	dsdata_release(k);
	dsattribute_release(a);

	status = dsengine_save(engine, r);
	dsrecord_release(r);
	if (status != DSStatusOK)
		fprintf(stderr, "Rename: %s\n", dsstatus_message(status));
	return status;
}

dsstatus
nifty_read(u_int32_t dsid, int argc, char *argv[])
{
	/* <path> [<key> ...] */

	u_int32_t x, meta;
	dsrecord *r;
	dsstatus status;
	dsdata *k;
	dsattribute *a;

	status = dsengine_fetch(engine, dsid, &r);
	if (status != DSStatusOK)
	{
		fprintf(stderr, "Fetch record %u failed: %s\n",
			dsid, dsstatus_message(status));
		return status;
	}

	if (argc == 0)
	{
		nifty_print_dsrecord(r, stdout, verbose);
		dsrecord_release(r);
		return DSStatusOK;
	}
		
	while (argc > 0)
	{
		k = nifty_getkey(argc, argv, &x, &meta);
		a = dsrecord_attribute(r, k, meta);

		if (a == NULL)
		{
			fprintf(stderr, "No such key: %s\n", argv[x - 1]);
			dsdata_release(k);
			argc -= x;
			argv += x;
			continue;
		}

		dsdata_release(k);

		nifty_print_dsattribute(a, meta, stdout, 0);
		fprintf(stdout, "\n");
		dsattribute_release(a);

		argc -= x;
		argv += x;
	}

	dsrecord_release(r);
	return DSStatusOK;
}

dsstatus
nifty_statistics(u_int32_t dsid, int argc, char *argv[])
{
	dsrecord *r;
	dsattribute *a;
	int i, j;

	r = dsstore_statistics(engine->store);

	for (i = 0; i < r->count; i++)
	{
		a = r->attribute[i];
		dsdata_print(a->key, stdout);
		printf(":");
	
		for (j = 0; j < a->count; j++)
		{
			printf(" ");
			dsdata_print(a->value[j], stdout);
		}
		printf("\n");
	}

	dsrecord_release(r);
	return DSStatusOK;
}


dsstatus
nifty_printpath(u_int32_t dsid, int argc, char *argv[])
{
	/* <path> */

	dsrecord *r;
	dsstatus status;
	u_int32_t pdsid;

	status = dsengine_fetch(engine, dsid, &r);
	if (status != DSStatusOK)
	{
		fprintf(stderr, "Fetch record %u failed: %s\n",
			dsid, dsstatus_message(status));
		return status;
	}

	nifty_print_dsrecord(r, stdout, verbose);
	pdsid = r->super;
	dsrecord_release(r);
	if (dsid != 0)
	{
		printf("\n");
		status = nifty_printpath(pdsid, argc, argv);
	}

	return status;
}

dsstatus
nifty_list(u_int32_t dsid, int argc, char *argv[])
{
	/* <path> [<key>] */
	
	u_int32_t i, j;
	dsrecord *l;
	dsstatus status;
	dsdata *k;
	char str[32];
	if (argc == 0) k = NULL;
	else k = cstring_to_dsdata(argv[0]);

	l = NULL;
	status = dsengine_list(engine, dsid, k, 1, 1, &l);
	dsdata_release(k);
	if (status != DSStatusOK) return status;

	if (l == NULL) return DSStatusOK;

	for (i = 0; i < l->count; i++)
	{
		sprintf(str, "%u          ", dsdata_to_int32(l->attribute[i]->key));
		str[10] = '\0';
		printf("%s", str);
		for (j = 0; j < l->attribute[i]->count; j++)
			printf(" %s", dsdata_to_cstring(l->attribute[i]->value[j]));
		printf("\n");
	}

	dsrecord_release(l);
	return DSStatusOK;
}

dsstatus
nifty_insert(u_int32_t dsid, int argc, char *argv[])
{
	/* <path> <key> <val> <index> */

	dsrecord *r;
	dsattribute *a;
	dsdata *k, *v;
	dsstatus status;
	u_int32_t x, meta, where;

	status = dsengine_fetch(engine, dsid, &r);
	if (status != DSStatusOK)
	{
		fprintf(stderr, "Fetch record %u failed: %s\n",
			dsid, dsstatus_message(status));
		return status;
	}

	k = nifty_getkey(argc, argv, &x, &meta);
	if (k == NULL)
	{
		fprintf(stderr, "%s\n", dsstatus_message(DSStatusInvalidKey));
		return DSStatusInvalidKey;
	}

	a = dsrecord_attribute(r, k, meta);
	if (a == NULL) 
	{
		fprintf(stderr, "Insert failed: %s: %s\n",
			dsdata_to_cstring(k), dsstatus_message(DSStatusInvalidKey));
		dsdata_release(k);
		return DSStatusInvalidKey;
	}
	dsdata_release(k);

	argc -= x;
	argv += x;
	v = nifty_getval(argc, argv, &x);
	if (v == NULL)
	{
		fprintf(stderr, "Insert failed: %s\n", dsstatus_message(DSStatusNoData));
		dsattribute_release(a);
		return DSStatusNoData;
	}

	where = atoi(argv[x]);
	
	dsattribute_insert(a, v, where);

	status = dsengine_save_attribute(engine, r, a, meta);
	dsattribute_release(a);
	dsrecord_release(r);
	if (status != DSStatusOK)
		fprintf(stderr, "Insert failed: %s\n", dsstatus_message(status));
	return status;
}

dsstatus
nifty_cd(u_int32_t dsid, int argc, char *argv[])
{
	u_int32_t status, serial;

	status = dsengine_record_serial(engine,dsid, &serial);
	if (status != DSStatusOK)
	{
		fprintf(stderr, "cd: %s\n", dsstatus_message(status));
		return status;
	}

	current_dir = dsid;
	return DSStatusOK;
}

dsstatus
nifty_pwd(u_int32_t dsid, int argc, char *argv[])
{
	char *s;

	if (mode == MODE_X500)
		s = dsengine_x500_string_path(engine, current_dir);
	else	
		s = dsengine_netinfo_string_path(engine, current_dir);

	printf("%s\n", s);
	free(s);

	return DSStatusOK;
}

dsstatus
nifty_move(u_int32_t dsid, int argc, char *argv[])
{
	/* <path> <new_parent> */

	dsstatus status;
	u_int32_t new_dsid;

	status = nifty_path(argv[0], current_dir, &new_dsid);
	if (status != DSStatusOK)
	{
		fprintf(stderr, "Move failed: %s: %s\n",
			argv[0], dsstatus_message(status));
		return status;
	}

	if (raw == 0)
	{
		/*
		 * dsengine_move() only works in raw mode.
		 * When connected to a server, we copy then remove the original.
		 */
		status = dsengine_copy(engine, dsid, new_dsid);
		if (status != DSStatusOK)
		{
			fprintf(stderr, "Move failed: %s\n", dsstatus_message(status));
			return status;
		}

		status = dsengine_remove(engine, dsid);
		if (status != DSStatusOK)
		{
			fprintf(stderr, "Move failed: %s\n", dsstatus_message(status));
		}

		return status;
	}

	status = dsengine_move(engine, dsid, new_dsid);
	if (status != DSStatusOK)
		fprintf(stderr, "Move failed: %s\n", dsstatus_message(status));

	return status;
}

dsstatus
nifty_copy(u_int32_t dsid, int argc, char *argv[])
{
	/* <path> <new_parent> */

	dsstatus status;
	u_int32_t new_dsid;

	status = nifty_path(argv[0], current_dir, &new_dsid);
	if (status != DSStatusOK)
	{
		fprintf(stderr, "Copy failed: %s: %s\n",
			argv[0], dsstatus_message(status));
		return status;
	}
	
	status = dsengine_copy(engine, dsid, new_dsid);
	if (status != DSStatusOK)
	{
		fprintf(stderr, "Copy failed: %s: %s\n",
			argv[0], dsstatus_message(status));
		return status;
	}
	return status;
}

dsstatus
nifty_search(u_int32_t dsid, int argc, char *argv[])
{
	/* <path> <scopemin> <scopemax> (<key> <val>)... */

	dsstatus status;
	dsrecord *pat, *r;
	dsattribute *a;
	dsdata *k, *v;
	u_int32_t i, len, scopemin, scopemax, *match, meta, x;

	pat = dsrecord_new();
	scopemin = atoi(argv[0]);
	scopemax = atoi(argv[1]);
	if (scopemin > scopemax)
	{
		fprintf(stderr, "Scope minimum (%u) > maximum (%u) precludes search\n",
			scopemin, scopemax);
		return DSStatusOK;
	}

	argc -= 2;
	argv += 2;

	while (argc > 0)
	{
		k = nifty_getkey(argc, argv, &x, &meta);
		if (k == NULL)
		{
			dsrecord_release(pat);
			return DSStatusInvalidKey;
		}
		argc -= x;
		argv += x;
		v = nifty_getval(argc, argv, &x);
		if (v == NULL)
		{
			dsrecord_release(pat);
			return DSStatusNoData;
		}
		argc -= x;
		argv += x;
		a = dsattribute_new(k);
		dsattribute_append(a, v);
		dsdata_release(k);
		dsdata_release(v);

		dsrecord_merge_attribute(pat, a, meta);
		dsattribute_release(a);
	}

	status = dsengine_search_pattern(engine, dsid, pat, scopemin, scopemax, &match, &len);
	dsrecord_release(pat);

	printf("Search found %d match%s\n", len, (len == 1) ? "" : "es");

	for (i = 0; i < len; i++)
	{
		status = dsengine_fetch(engine, match[i], &r);
		if (status != DSStatusOK)
		{
			fprintf(stderr, "\nFetch record %u failed: %s\n",
				match[i], dsstatus_message(status));
			continue;
		}

		printf("\n");
		nifty_print_dsrecord(r, stdout, verbose);
		dsrecord_release(r);
	}

	if (len > 0) free(match);
	return status;
}

dsstatus 
nifty_attach(u_int32_t dsid, int argc, char *argv[])
{
	/* <path> <dsid> */

	dsstatus status;
	u_int32_t child;
	dsrecord *r;

	if (raw == 0)
	{
		fprintf(stderr, "Can only attach in raw mode\n");
		return DSStatusFailed;
	}

	child = atoi(argv[0]);

	status = dsengine_fetch(engine, dsid, &r);
	if (status != DSStatusOK)
	{
		fprintf(stderr, "Attach failed: %s\n", dsstatus_message(status));
		return status;
	}

	status = dsengine_attach(engine, r, child);
	if (status != DSStatusOK)
		fprintf(stderr, "Attach failed: %s\n", dsstatus_message(status));

	dsrecord_release(r);
	return status;
}

dsstatus
nifty_detach(u_int32_t dsid, int argc, char *argv[])
{
	/* <path> <dsid> */

	dsstatus status;
	u_int32_t child;
	dsrecord *r;

	if (raw == 0)
	{
		fprintf(stderr, "Can only detach in raw mode\n");
		return DSStatusFailed;
	}

	child = atoi(argv[0]);

	status = dsengine_fetch(engine, dsid, &r);
	if (status != DSStatusOK)
	{
		fprintf(stderr, "Detach failed: %s\n", dsstatus_message(status));
		return status;
	}

	status = dsengine_detach(engine, r, child);
	if (status != DSStatusOK)
		fprintf(stderr, "Detach failed: %s\n", dsstatus_message(status));

	dsrecord_release(r);
	return status;
}

dsstatus
nifty_parent(u_int32_t dsid, int argc, char *argv[])
{
	/* <parent_path> <dsid> */

	dsstatus status;
	u_int32_t child;
	dsrecord *r;

	if (raw == 0)
	{
		fprintf(stderr, "Can only set parent in raw mode\n");
		return DSStatusFailed;
	}

	child = atoi(argv[0]);

	status = dsengine_fetch(engine, child, &r);
	if (status != DSStatusOK)
	{
		fprintf(stderr, "Parent failed: %s\n", dsstatus_message(status));
		return status;
	}

	status = dsengine_set_parent(engine, r, dsid);
	if (status != DSStatusOK)
		fprintf(stderr, "Parent failed: %s\n", dsstatus_message(status));

	dsrecord_release(r);
	return status;
}

dsstatus
nifty_version_history(u_int32_t dsid, int argc, char *argv[])
{
	dsstatus status;
	dsrecord *r;
	int op;
	u_int32_t max, i, v, n;
	char str[32];
	
	if (argc == 0)
	{
		status = dsengine_version(engine, &max);
		if (status != DSStatusOK)
		{
			printf("Can't get max version: %s\n", dsstatus_message(status));
			return status;
		}

		status = dsengine_fetch(engine, dsid, &r);
		if (status != DSStatusOK)
		{
			fprintf(stderr, "Fetch record %u failed: %s\n",
			dsid, dsstatus_message(status));
			return status;
		}

		printf("Maximum version: %u\n", max);
		printf("Record: %u   Version: %u   Serial: %u\n", 
			r->dsid, r->vers, r->serial);

		dsrecord_release(r);
		return DSStatusOK;
	}

	op = -2;

	if (streq(argv[0], "<")) op = -1;
	else if (streq(argv[0], "lt")) op = -1;
	else if (streq(argv[0], ">")) op = 1;
	else if (streq(argv[0], "gt")) op = 1;
	else if (streq(argv[0], "=")) op = 0;
	else if (streq(argv[0], "eq")) op = 0;
	
	if (op == -2)
	{
		op = 0;
	}
	else
	{
		argc--;
		argv++;
	}

	v = -1;

	if (argc == 0) 
	{
		status = dsengine_fetch(engine, dsid, &r);
		if (status != DSStatusOK)
		{
			fprintf(stderr, "Fetch record %u failed: %s\n",
			dsid, dsstatus_message(status));
			return status;
		}

		v = r->vers;
		dsrecord_release(r);
	}
	else v = atoi(argv[0]);

	if (op == 0)
	{
		/* Record with version v */
		status = dsengine_version_record(engine, v, &n);
		if (status != DSStatusOK)
		{
			printf("No record with version %u\n", v);
			return status;
		}

		if (v == -1)
		{
			status = dsengine_record_version(engine, n, &v);
			if (status != DSStatusOK)
			{
				printf("Can't determine version of record %u\n", n);
				return status;
			}
		}

		printf("Record: %u   Version: %u\n", n, v);
	}
	else if (op < 0)
	{
		/* List records with version < v */
		if (v == 0) return DSStatusOK;

		for (i = v - 1; i > 0; i--)
		{
			status = dsengine_version_record(engine, i, &n);
			if (status != DSStatusOK) continue;

			sprintf(str, "%u          ", n);
			str[10] = '\0';
			printf("Record: %s   Version: %u\n", str, i);
		}

		status = dsengine_version_record(engine, 0, &n);
		if (status != DSStatusOK) return status;

		sprintf(str, "%u          ", n);
		str[10] = '\0';
		printf("Record: %s   Version: %u\n", str, i);
	}
	else
	{
		/* List records with version > v */

		status = dsengine_version(engine, &max);
		if (status != DSStatusOK)
		{
			printf("Can't get max version: %s\n", dsstatus_message(status));
			return status;
		}

		if (v >= max)
		{
			printf("Maximum version number is %u\n", max);
			return DSStatusOK;
		}

		for (i = v + 1; i <= max; i++)
		{
			status = dsengine_version_record(engine, i, &n);
			if (status != DSStatusOK) continue;
			sprintf(str, "%u          ", n);
			str[10] = '\0';
			printf("Record: %s   Version: %u\n", str, i);
		}
	}

	return DSStatusOK;
}

dsstatus
nifty_domainname(u_int32_t dsid, int argc, char *argv[])
{
	char *dn;
	ni_status nistatus;
	dsx500dit *dit;

	if (mode == MODE_X500)
	{
		dit = dsx500dit_new(engine);
		if (dit == NULL) return DSStatusFailed;

		dsdata_print(dit->local_suffix, stdout);
		printf("\n");
		dsx500dit_release(dit);
	}
	else
	{
		if (raw == 1)
		{
			fprintf(stderr, "Can't get domain name in raw mode\n");
			return DSStatusFailed;
		}

		nistatus = ni_pwdomain((void *)engine->store->index[0], &dn);
		if (nistatus != 0) return DSStatusFailed;

		printf("%s\n", dn);
		free(dn);
	}

	return DSStatusOK;
}

dsstatus
nifty_rparent(u_int32_t dsid, int argc, char *argv[])
{
	struct sockaddr_in addr;
	char *tag;
	ni_status nistatus;

	if (raw == 1)
	{
		fprintf(stderr, "Can't get rparent in raw mode\n");
		return DSStatusFailed;
	}

	nistatus = ni2_rparent((void *)engine->store->index[0], &addr, &tag);
	if (nistatus != 0)
	{
		fprintf(stderr, "%s\n", ni_error(nistatus));
		return DSStatusFailed;
	}

	printf("%s/%s\n", inet_ntoa(addr.sin_addr), tag);
	free(tag);

	return DSStatusOK;
}

dsstatus
nifty_resync(u_int32_t dsid, int argc, char *argv[])
{
	ni_status nistatus;

	if (raw == 1)
	{
		fprintf(stderr, "Can't resync in raw mode\n");
		return DSStatusFailed;
	}

	nistatus = ni_resync((void *)engine->store->index[0]);
	if (nistatus != 0) return DSStatusFailed;

	return DSStatusOK;
}

dsstatus
nifty_authenticate(int argc, char *argv[])
{
	dsdata *u, *p;
	int x;
	dsstatus status;

	if (argc == 0)
	{
		u = cstring_to_dsdata("root");
	}
	else
	{
		u = nifty_getval(argc, argv, &x);
		if (u == NULL) return DSStatusFailed;
		argc -= x;
		argv += x;
	}

	if (argc > 1)
	{
		p = nifty_getval(argc, argv, &x);
	}
	else
	{
		p = cstring_to_dsdata(getpass("Password: "));
	}
	
	if (p == NULL)
	{
		dsdata_release(u);
		return DSStatusFailed;
	}

	status = dsengine_authenticate(engine, u, p);

	dsdata_release(u);
	dsdata_release(p);

	return status;
}

dsstatus
nifty_printrefs(int argc, char *argv[])
{
	dsx500dit *dit;
	int i, j;
	dsattribute *a;

	dit = dsx500dit_new(engine);
	if (dit == NULL) return DSStatusFailed;

	if (dit->parent_referrals)
	{
		printf("Parent referrals:\n");
		printf("dn: ");
		dsdata_print(dit->parent_referrals->key, stdout);
		printf("\n");

		for (i = 0; i < dit->parent_referrals->count; i++)
		{
			printf("  ref: ");
			dsdata_print(dit->parent_referrals->value[i], stdout);
			printf("\n");
		}
	}

	if (dit->child_count)
	{
		if (dit->parent_referrals) printf("\n");

		printf("Child referrals:\n");
		for (i = 0; i < dit->child_count; i++)
		{
			a = dit->child_referrals[i];
	
			printf("dn: ");
			dsdata_print(a->key, stdout);
			printf("\n");
			for (j = 0; j < a->count; j++)
			{
				printf("  ref: ");
				dsdata_print(a->value[j], stdout);
				printf("\n");
			}
		}
	}

	dsx500dit_release(dit);
	return DSStatusOK;
}

dsstatus
nifty_setrdn(u_int32_t dsid, int argc, char *argv[])
{
	/* <path> <key> */

	dsstatus status;
	dsrecord *r;
	dsdata *rdnkey, *key;
	dsattribute *a;

	status = dsengine_fetch(engine, dsid, &r);
	if (status != DSStatusOK)
	{
		fprintf(stderr, "Fetch record %u failed: %s\n",
			dsid, dsstatus_message(status));
		return status;
	}

	rdnkey = cstring_to_dsdata("rdn");
	key = cstring_to_dsdata(argv[0]);

	dsrecord_remove_key(r, rdnkey, SELECT_META_ATTRIBUTE);

	if (!streq(argv[0], "name"))
	{
		a = dsattribute_new(rdnkey);
		dsrecord_append_attribute(r, a, SELECT_META_ATTRIBUTE);
		dsattribute_insert(a, key, 0);
		dsattribute_release(a);
	}

	dsdata_release(rdnkey);
	dsdata_release(key);

	status = dsengine_save(engine, r);
	dsrecord_release(r);
	if (status != DSStatusOK) 
	{
		fprintf(stderr, "%s\n", dsstatus_message(status));
	}

	return status;
}

int
nifty_source(int argc, char *argv[])
{
	FILE *f;
	int status;

	f = fopen(argv[0], "r");
	if (f == NULL)
	{
		fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
		return DSStatusFailed;
	}

	status = nifty_interactive(f, PROMPT_NONE);
	fclose(f);
	return status;
}

int
nifty_echo(int argc, char *argv[])
{
	int i;

	for (i = 0; i < argc; i++)
	{
		printf("%s", argv[i]);
		if ((i + 1) < argc) printf(" ");
	}

	printf("\n");

	return DSStatusOK;
}

int
nifty_flush_cache(int argc, char *argv[])
{
	dsengine_flush_cache(engine);
	return DSStatusOK;
}

int
nifty_cmd(int cc, char *cv[])
{
	int status, i, op, dsid, do_path;
	char *cmd;

	dsid = 0;
	op = OP_NOOP;
	do_path = 1;
	i = 0;
	cmd = cv[0];
	if (cmd[0] == '-') cmd++;

	if (streq(cmd, "help"))
	{
		usage();
		return DSStatusOK;
	}

	else if (streq(cmd, "create"))	op = OP_CREATE;
	else if (streq(cmd, "createprop")) op = OP_CREATE;
	else if (streq(cmd, "delete"))	op = OP_DELETE;
	else if (streq(cmd, "destroy"))	op = OP_DELETE;
	else if (streq(cmd, "destroyprop"))	op = OP_DELETE;
	else if (streq(cmd, "rename"))	op = OP_RENAME;
	else if (streq(cmd, "read"))	op = OP_READ;
	else if (streq(cmd, "list"))	op = OP_LIST;
	else if (streq(cmd, "append"))	op = OP_APPEND;
	else if (streq(cmd, "merge"))	op = OP_MERGE;
	else if (streq(cmd, "insert"))	op = OP_INSERT;
	else if (streq(cmd, "move"))	op = OP_MOVE;
	else if (streq(cmd, "copy"))	op = OP_COPY;
	else if (streq(cmd, "search"))	op = OP_SEARCH;
	else if (streq(cmd, "path"))	op = OP_PATH;

	else if (streq(cmd, "attach"))	op = OP_ATTACH;
	else if (streq(cmd, "detach"))	op = OP_DETACH;
	else if (streq(cmd, "parent"))	op = OP_PARENT;
	
	else if (streq(cmd, "cd"))		op = OP_CD;
	else if (streq(cmd, "pwd"))		op = OP_PWD;

	else if (streq(cmd, "stats"))		op = OP_STATS;
	else if (streq(cmd, "statistics"))	op = OP_STATS;

	else if (streq(cmd, "domainname"))	op = OP_DOMAINNAME;
	else if (streq(cmd, "name"))		op = OP_DOMAINNAME;

	else if (streq(cmd, "rparent"))		op = OP_RPARENT;
	else if (streq(cmd, "resync"))		op = OP_RESYNC;
	else if (streq(cmd, "refs"))		op = OP_REFS;

	else if (streq(cmd, "mk"))		op = OP_CREATE;
	else if (streq(cmd, "rm"))		op = OP_DELETE;
	else if (streq(cmd, "cat"))		op = OP_READ;
	else if (streq(cmd, "."))		op = OP_READ;
	else if (streq(cmd, "ls"))		op = OP_LIST;
	else if (streq(cmd, "cp"))		op = OP_COPY;
	else if (streq(cmd, "mv"))		op = OP_MOVE;

	else if (streq(cmd, "setrdn"))		op = OP_SETRDN;

	else if (streq(cmd, "hist") || streq(cmd, "history"))
	{
		op = OP_VHISTORY;
		do_path = 0;
	}

	else if (streq(cmd, "auth") || streq(cmd, "su"))
	{
		op = OP_AUTH;
		do_path = 0;
	}

	else if (streq(cmd, "source") || streq(cmd, "<"))
	{
		op = OP_SOURCE;
		do_path = 0;
	}

	else if (streq(cmd, "echo"))
	{
		op = OP_ECHO;
		do_path = 0;
	}

	else if (streq(cmd, "flush"))
	{
		op = OP_FLUSH;
		do_path = 0;
	}
	
	else if (streq(cmd, "load"))
	{
		op = OP_LOAD;
		do_path = 0;
	}

	else
	{
		usage();
		return DSStatusOK;
	}

	cc--;
	cv++;

	if ((interactive == 1) && (cc == 0) && (minargs[op] == 1))
	{
		/* default path arg to current directory */
	}
	else if (cc < minargs[op])
	{
		fprintf(stderr, "Too few parameters for %s operation\n", cmd);
		usage();
		return DSStatusOK;
	}

	status = DSStatusOK;
	dsid = current_dir;

	if (op == OP_CREATE)
	{
		if (cc > 0) status = nifty_pathcreate(cv[0], &dsid);
		if (cc == 1) dsid = current_dir;
	}
	else if ((do_path != 0) && (cc > 0))
	{
		status = nifty_path(cv[0], current_dir, &dsid);
	}

	if (status != DSStatusOK)
	{
		fprintf(stderr, "%s: %s\n", cmd, dsstatus_message(status));
		return status;
	}

	if (dsid == (u_int32_t)-1)
	{
		fprintf(stderr, "%s: %s\n",
			cv[0], dsstatus_message(DSStatusInvalidRecordID));
		return DSStatusInvalidRecordID;
	}

	if ((do_path != 0) && (cc > 0))
	{
		cc--;
		cv++;
	}

	switch (op)
	{
		case OP_NOOP:
			status = DSStatusOK;
			break;
		case OP_CREATE:
			status = nifty_create(dsid, cc, cv);
			break;
		case OP_LOAD:
			status = nifty_load(dsid, cc, cv);
			break;
		case OP_DELETE:
			status = nifty_delete(dsid, cc, cv);
			break;
		case OP_RENAME:
			status = nifty_rename(dsid, cc, cv);
			break;
		case OP_READ:
			status = nifty_read(dsid, cc, cv);
			break;
		case OP_LIST:
			status = nifty_list(dsid, cc, cv);
			break;
		case OP_APPEND:
			status = nifty_append(dsid, cc, cv);
			break;
		case OP_MERGE:
			status = nifty_merge(dsid, cc, cv);
			break;
		case OP_INSERT:
			status = nifty_insert(dsid, cc, cv);
			break;
		case OP_MOVE:
			status = nifty_move(dsid, cc, cv);
			break;
		case OP_COPY:
			status = nifty_copy(dsid, cc, cv);
			break;
		case OP_SEARCH:
			status = nifty_search(dsid, cc, cv);
			break;
		case OP_PATH:
			status = nifty_printpath(dsid, cc, cv);
			break;
		case OP_ATTACH:
			status = nifty_attach(dsid, cc, cv);
			break;
		case OP_DETACH:
			status = nifty_detach(dsid, cc, cv);
			break;
		case OP_PARENT:
			status = nifty_parent(dsid, cc, cv);
			break;
		case OP_CD:
			status = nifty_cd(dsid, cc, cv);
			break;
		case OP_PWD:
			status = nifty_pwd(dsid, cc, cv);
			break;
		case OP_VHISTORY:
			status = nifty_version_history(dsid, cc, cv);
			break;
		case OP_STATS:
			status = nifty_statistics(dsid, cc, cv);
			break;
		case OP_DOMAINNAME:
			status = nifty_domainname(dsid, cc, cv);
			break;
		case OP_RPARENT:
			status = nifty_rparent(dsid, cc, cv);
			break;
		case OP_RESYNC:
			status = nifty_resync(dsid, cc, cv);
			break;
		case OP_AUTH:
			status = nifty_authenticate(cc, cv);
			break;
		case OP_REFS:
			status = nifty_printrefs(cc, cv);
			break;
		case OP_SETRDN:
			status = nifty_setrdn(dsid, cc, cv);
			break;
		case OP_SOURCE:
			status = nifty_source(cc, cv);
			break;
		case OP_ECHO:
			status = nifty_echo(cc, cv);
			break;
		case OP_FLUSH:
			status = nifty_flush_cache(cc, cv);
			break;
	}
	
	return status;
}

char *
nifty_get_string(char **s)
{
	char *p, *x;
	int i, esc, quote;

	if (*s == NULL) return NULL;
	if (**s == '\0') return NULL;

	/* Skip leading white space */
	while ((**s == ' ') || (**s == '\t')) *s += 1;

	if (**s == '\0') return NULL;

	x = *s;
	i = 0;
	esc = 0;
	quote = 0;
	
	if (*x == '\"')
	{
		quote = 1;
		*s += 1;
		x = *s;
	}
	
	forever
	{
		if (x[i] == '\0') break;
		if ((quote == 1) && (x[i] == '\"')) break;
		if ((quote == 0) && (x[i] == ' ' )) break;
		if ((quote == 0) && (x[i] == '\t')) break;
		
		if (x[i] == '\\')
		{
			if (esc == 0) esc = i;
			i++;
			if (x[i] == '\0') break;
		}
		i++;
	}
	
	p = malloc(i+1);
	memmove(p, x, i);
	p[i] = 0;

	if (quote == 1) i++;
	*s += i;

	while (esc != 0)
	{
		i = esc + 1;
		if (p[i] == '\\') i++;
		esc = 0;
		for (; p[i] != '\0'; i++)
		{
			p[i-1] = p[i];
			if ((p[i] == '\\') && (esc == 0)) esc = i - 1;
		}
		p[i - 1] = '\0';
	}

	return p;
}

char *
nifty_prompt(EditLine *el)
{
	static char *ps = NULL;
	char *p;

	if (ps != NULL) free(ps);

	switch (prompt)
	{
		case PROMPT_NONE:
			ps = calloc(1, 1);
			break;
		case PROMPT_DOT:
			ps = strdup(".");
			break;
		case PROMPT_PLAIN:
			ps = strdup("> ");
			break;
		case PROMPT_NI:
			p = dsengine_netinfo_string_path(engine, current_dir);
			asprintf(&ps, "%s%s%s > ", do_bold ? term_bold : "", p, do_bold ? term_norm : "");
			free(p);
			break;
		case PROMPT_X500:
			p = dsengine_x500_string_path(engine, current_dir);
			if (p == NULL || p[0] == '\0') p = copyString("rootDSE");
			asprintf(&ps, "%s%s%s > ", do_bold ? term_bold : "", p, do_bold ? term_norm : "");
			free(p);
			break;
		default:
			ps = calloc(1, 1);
			break;
	}

	return ps;
}

static char *
nifty_input_line(FILE *in, EditLine *el, History *h)
{
	HistEvent hev;
	const char *eline;
	char *out, fline[INPUT_LENGTH];
	int count, len;
	
	if (el == NULL)
	{
		count = fscanf(in, "%[^\n]%*c", fline);
		if (count < 0) return NULL;
		if (count == 0)
		{
			fscanf(in, "%*c");
			out = calloc(1, 1);
			return out;
		}
		len = strlen(fline);
		out = malloc(len + 1);
		memmove(out, fline, len);
		out[len] = '\0';
		return out;
	}

	eline = el_gets(el, &count);
	if (eline == NULL) return NULL;
	if (count <= 0) return NULL;

	len = count - 1;
	out = malloc(len);
	memmove(out, eline, len);
	out[len] = '\0';
	if (len > 0) history(h, &hev, H_ENTER, out);

	return out;
}



int
nifty_interactive(FILE *in, int pmt)
{
	char *s, *p, **iargv, *line;
	int i, iargc, quote, esc;
	EditLine *el;
	History *h;
	HistEvent hev;

	iargv = NULL;
	interactive = 1;

	el = NULL;
	h = NULL;

	if (pmt != PROMPT_NONE)
	{
		el = el_init("nicl", in, stdout, stderr);
		h = history_init();

		el_set(el, EL_HIST, history, h);
		el_set(el, EL_PROMPT, nifty_prompt);
		el_set(el, EL_EDITOR, "emacs");
		el_set(el, EL_SIGNAL, 1);
		el_set(el, EL_EDITMODE, 1);

		history(h, &hev, H_SETSIZE, 100000);
	}

	forever
	{
		line = nifty_input_line(in, el, h);

		if (line == NULL) break;
		if (line[0] == '\0')
		{
			free(line);
			continue;
		}
		if (streq(line, "quit")) break;
		if (streq(line, "exit")) break;
		if (streq(line, "q")) break;

		p = line;
		quote = 0;
		esc = 0;
		iargc = 0;

		forever
		{
			s = nifty_get_string(&p);
			if (s == NULL) break;
			if (iargc == 0)
				iargv = (char **)malloc(sizeof(char *));
			else
				iargv = (char **)realloc(iargv, (iargc + 1) * sizeof(char *));
	
			iargv[iargc++] = s;
		}

		nifty_cmd(iargc, iargv);

		for (i = 0; i < iargc; i++) free(iargv[i]);
		free(iargv);
		free(line);
	}

	return DSStatusOK;
}

int
main(int argc, char *argv[])
{
	int i, opt_tag, opt_promptpw, opt_user, opt_password, opt_create;
	u_int32_t flags;
	char *slash, *term;
	dsstatus status;
	dsengine *e;
	dsdata *auth_user, *auth_password;

	slash = rindex(argv[0], '/');
	if (slash == NULL) strcpy(myname, argv[0]);
	else strcpy(myname, slash+1);

	if (argc < 2)
	{
		usage();
		exit(0);
	}

	auth_user = NULL;
	auth_password = NULL;

	mode = MODE_NETINFO;
	prompt = PROMPT_NI;
	raw = 0;
	opt_tag = 0;
	opt_promptpw = 0;
	opt_user = 0;
	opt_password = 0;
	opt_create = 0;
	interactive = 0;
	do_notify = 1;
	flags = 0;

	flags |= DSSTORE_FLAGS_SERVER_MASTER;
	flags |= DSSTORE_FLAGS_ACCESS_READWRITE;

	term = getenv("TERM");
	if ((term != NULL) && (!strcasecmp(term, "vt100")))
	{
		do_bold = 1;
		term_bold = VT100_BOLD;
		term_norm = VT100_NORM;
	}

	for (i = 1; i < argc; i++)
	{
		if (streq(argv[i], "-ro"))
		{
			flags &= ~DSSTORE_FLAGS_ACCESS_MASK;
			flags |= DSSTORE_FLAGS_ACCESS_READONLY;
		}
		else if (streq(argv[i], "-v")) verbose = 1;
		else if (streq(argv[i], "-vv")) verbose = 2;
		else if (streq(argv[i], "-q")) prompt = PROMPT_NONE;
		else if (streq(argv[i], "-nobold")) do_bold = 0;
		else if (streq(argv[i], "-nonotify")) do_notify = 0;
		else if (streq(argv[i], "-t")) opt_tag = 1;
		else if (streq(argv[i], "-raw")) raw = 1;
		else if (streq(argv[i], "-p")) opt_promptpw = 1;
		else if (streq(argv[i], "-c")) opt_create = 1;
		else if (streq(argv[i], "-x500"))
		{
			prompt = PROMPT_X500;
			mode = MODE_X500;
		}
		else if (streq(argv[i], "-P"))
		{
			i++;
			opt_password = i;
		}
		else if (streq(argv[i], "-u"))
		{
			i++;
			opt_user = i;
		}
		else break;
	}

	if (raw == 0)
	{
		flags = DSSTORE_FLAGS_REMOTE_NETINFO;	
		if (opt_tag == 1) flags |= DSSTORE_FLAGS_OPEN_BY_TAG;
	}

	if (mode == MODE_NETINFO)
		flags |= DSENGINE_FLAGS_NETINFO_NAMING;
	else
		flags |= DSENGINE_FLAGS_X500_NAMING;

	if (do_notify != 0) flags |= DSSTORE_FLAGS_NOTIFY_CHANGES;

	if (opt_user) opt_promptpw = 1;
	if (opt_password) opt_promptpw = 0;

	if ((opt_user == 0) && ((opt_password == 1) || (opt_promptpw == 1)))
	{
		auth_user = cstring_to_dsdata("root");
	}

	if (opt_user != 0)
		auth_user = cstring_to_dsdata(argv[opt_user]);

	if (opt_password != 0)
		auth_password = cstring_to_dsdata(argv[opt_password]);
	else if (opt_promptpw == 1)
		auth_password = cstring_to_dsdata(getpass("Password: "));

	if ((argc > 2) && (streq(argv[argc - 1], "-create") || streq(argv[argc - 1], "create")))
	{
		if (raw == 0)
		{
			fprintf(stderr, "Can't create a NetInfo domain\n");
			exit(1);
		}

		status = dsengine_new(&e, argv[i], flags);
		if (status != DSStatusOK)
		{
			fprintf(stderr, "create(%s): %s\n",
				argv[i], dsstatus_message(status));
			exit(status);
		}

		dsengine_close(e);
		exit(DSStatusOK);
	}

	if (	opt_create == 1)
	{
		status = dsengine_new(&engine, argv[i], flags);
		if (status != DSStatusOK)
		{
			fprintf(stderr, "create(%s): %s\n",
				argv[i], dsstatus_message(status));
			exit(status);
		}
	}
	else
	{
		status = dsengine_open(&engine, argv[i], flags);
		if (status != DSStatusOK)
		{
			fprintf(stderr, "open(%s): %s\n", argv[i], dsstatus_message(status));
			exit(status);
		}
	}

	if (auth_user != NULL)
	{
		dsengine_authenticate(engine, auth_user, auth_password);
		dsdata_release(auth_user);
		dsdata_release(auth_password);
	}

	i++;
	if (i >= argc)
	{
		if (isatty(fileno(stdin)) == 0) prompt = PROMPT_NONE;

		status = nifty_interactive(stdin, prompt);
		if (prompt != PROMPT_NONE) printf("Goodbye\n");
	}
	else status = nifty_cmd(argc - i, argv + i);

	dsengine_close(engine);
	exit(status);
}
