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

#import "LUGlobal.h"
#import "LUDictionary.h"
#import "LUArray.h"
#import "FFAgent.h"
#import <NetInfo/nilib2.h>
#import "print.h"
#import "LUDictionaryExtras.h"
#import <sys/types.h>
#import <pwd.h>
#import <libc.h>
#import <stdlib.h>
#import <strings.h>
#import <stdio.h>
#import <netinfo/ni.h>

#define MYNAME "niload"
#define TIMEOUT 30

#define RETAIN_NETINFO 1
#define DELETE_NETINFO 2
#define MERGE_VALUES 3

BOOL verbose;

extern void raw_load();

/* To avoid including libc.h */
extern uid_t getuid(void);

ni_status
check_auth(void *domain, char *user, char *passwd)
{
    ni_status status;
	ni_id dir;
	char str[1024], salt[3];
	ni_namelist nl;

	sprintf(str, "/users/%s", user);
	status = ni_pathsearch(domain, &dir, str);
	if (status == NI_NODIR) return NI_NOUSER;
	if (status != NI_OK) return status;

	status = ni_lookupprop(domain, &dir, "passwd", &nl);
	if (status != NI_OK) return status;

	if (nl.ni_namelist_len == 0) return YES;

	salt[0] = nl.ni_namelist_val[0][0];
	salt[1] = nl.ni_namelist_val[0][1];
	salt[2] = '\0';

	if (strcmp(nl.ni_namelist_val[0], crypt(passwd, salt)))
		return NI_AUTHERROR;

	return NI_OK;
}

ni_index
ni_entrylist_match(const ni_entrylist el, ni_name_const name)
{
	ni_index i, len;
	ni_index where;
	ni_entry *e;
	ni_namelist *nl;

	len = el.ni_entrylist_len;
	if (len == 0) return NI_INDEX_NULL;

	for (i = 0; i < len; i++)
	{
		e = &el.ni_entrylist_val[i];
		if (e == NULL) continue; 

		nl = e->names;
		if (nl == NULL) continue; 

		where = ni_namelist_match(*nl, name);
		if (where != NI_INDEX_NULL) return i;
	}

	return NI_INDEX_NULL;
}


void
usage(void)
{
	fprintf(stderr, "usage: ");
	fprintf(stderr, "%s [opts] format domain [mountmap]\n", MYNAME);
	fprintf(stderr, "opts:\n");
	fprintf(stderr, "\t-d              delete (override) existing entries from NetInfo\n");
	fprintf(stderr, "\t                when the input contains a duplicate name\n\n");
	fprintf(stderr, "\t-m              merge new values into NetInfo\n");
	fprintf(stderr, "\t                when the input contains a duplicate name\n\n");
	fprintf(stderr, "\tNote: only one of -d or -m may be used.\n");
	fprintf(stderr, "\t      If neither is given, existing entries in NetInfo will\n");
	fprintf(stderr, "\t      be unchanged if there are duplicate names in the input\n\n");
	fprintf(stderr, "\t-v              verbose\n");
	fprintf(stderr, "\t-p              prompt for password\n");
	fprintf(stderr, "\t-u <user>       authenticate as another user (implies -p)\n");
	fprintf(stderr, "\t-P <password>   password supplied on command line (overrides -p)\n");
	fprintf(stderr, "\t-T <timeout>    read & write timeout in seconds (default 30)\n");
	fprintf(stderr, "\t-t              domain specified by <host>/<tag>\n");
	exit(1);
}

void
merge_proplist(ni_proplist *new, ni_proplist *old)
{
	int i, len;
	ni_index where;

	len = old->ni_proplist_len;

	for (i = 0; i < len; i++)
	{
		where = ni_proplist_match(
			(const ni_proplist)*new,
			(const ni_name)old->ni_proplist_val[i].nip_name, NULL);

		if (where == NI_INDEX_NULL)
		{
			ni_proplist_insert(new,
				(const ni_property)old->ni_proplist_val[i], where);
		}
	}
}

ni_status 
update_dir(void *domain, char *parent, char *name,
	LUDictionary *item, int op)
{
	ni_proplist *pl;
	ni_proplist oldpl;
	ni_status status;
	ni_id dir, child_dir;
	char path[1024];
	int i, len, x;

	sprintf(path, "%s/", parent);
	x = strlen(path);
	len = strlen(name);
	for (i = 0; i < len; i++)
	{
		if (name[i] == '/') path[x++] = '\\';
		path[x++] = name[i];
	}
	path[x] = '\0';

	status = ni_pathsearch(domain, &dir, path);
	if (status == NI_NODIR)
	{
		/* doesn't exist */
		/* find parent dir */
		status = ni_pathsearch(domain, &dir, parent);
		if (status == NI_NODIR)
		{
			/* create the parent */
			if (verbose) fprintf(stderr, "creating directory: %s\n", parent);
			status = ni2_createpath(domain, &dir, parent);
		}
		if (status != NI_OK) return status;

		/* create new child directory in netinfo */
		pl = [item niProplist];

		if (verbose) fprintf(stderr, "writing new directory %s\n", path);
		status = ni_create(domain, &dir, *pl, &child_dir, NI_INDEX_NULL);
		ni_proplist_free(pl);
		return status;
	}
	if (status != NI_OK) return status;

	if (op == DELETE_NETINFO)
	{
		/* write input proplist to netinfo */
		pl = [item niProplist];

		if (verbose) fprintf(stderr, "re-writing directory %s\n", path);
		status = ni_write(domain, &dir, *pl);
		ni_proplist_free(pl);
		return status;
	}
	else if (op == MERGE_VALUES)
	{
		/* read from netinfo */
		status = ni_read(domain, &dir, &oldpl);
		if (status != NI_OK) return status;

		pl = [item niProplist];

		/* merge values */
		merge_proplist(pl, &oldpl);
		ni_proplist_free(&oldpl);

		/* write back to netinfo */
		if (verbose) fprintf(stderr, "merging directory %s\n", path);
		status = ni_write(domain, &dir, *pl);
		ni_proplist_free(pl);
		return status;
	}
	else
	{
		/* retain NetInfo - do nothing */
		if (verbose) fprintf(stderr, "retaining directory %s\n", path);
		return NI_OK;
	}

	return NI_OK;
}

int
main(int argc, char *argv[])
{
	FFAgent *ff;
	LUArray *all;
	LUDictionary *item;
	ni_entrylist el;
	int i, op;
	int j, len, nilen;
	char *lastchar, *p, *t;
	BOOL opt_promptpw, opt_passwd;
	int format_arg, domain_arg, mntmap_arg;
	struct passwd *pw;
	char *format;
	LUCategory cat;
	char nidirname[64];
	void *domain;
	ni_status status;
	ni_id dir;
	char *name;
	BOOL raw;
	char auth_user[128];
	char auth_passwd[128];
	BOOL opt_user, opt_tag, authenticate;
	int timeout;

	if (argc < 3) usage();

	pw = getpwuid(getuid());
	if (pw == NULL) {
		fprintf(stderr,"%s: can't determine who you are!\n", MYNAME);
		exit(1);
	}

	strcpy(auth_user, "root");
	auth_passwd[0] = '\0';
	opt_promptpw = NO;
	opt_user = NO;
	opt_passwd = NO;
	authenticate = NO;
	opt_tag = NO;
	verbose = NO;
	raw = NO;
	op = RETAIN_NETINFO;

	timeout = TIMEOUT;

	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] != '-')
		{
			/* do nothing */
		}
		else if (!strcmp(argv[i], "-r") && !raw)
		{
			raw = YES;
		}
		else if (!strcmp(argv[i], "-p") && !opt_promptpw)
		{
			opt_promptpw = YES;
		}
		else if (!strcmp(argv[i], "-t") && !opt_tag)
		{
			opt_tag = YES;
		}
		else if (!strcmp(argv[i], "-T"))
		{
			if (i == argc - 1)
			{
				fprintf(stderr, "%s: insufficient number of arguments for %s\n",
					MYNAME, argv[i]);
				usage();
			}
			timeout = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "-u") && !opt_user)
		{
			if (i == argc - 1)
			{
				fprintf(stderr, "%s: insufficient number of arguments for %s\n",
					MYNAME, argv[i]);
				usage();
			}
			opt_user = YES;
			i++;
			strcpy(auth_user, argv[i]);

			/* BEGIN UGLY HACK TO MAKE argv[i] DISAPPEAR */
			lastchar = &argv[argc-1][0];
			while(*lastchar != '\0') lastchar++;
			p = &argv[i+1][0];
			t = &argv[i][0];
			len = p - t;

			for (j = i+1; j < argc; j++) *(&argv[j-1]) = argv[j];
			argc--;
			for (; p < lastchar; p++) *(p-len) = *p;
			for (j = i; j < argc; j++) argv[j] -= len;
			p -= len;
			*p = '\0';
			for (p++; p < lastchar; p++)  *p = ' ';
			i--;
			/* END UGLY HACK */
		}
		else if (!strcmp(argv[i], "-P") && !opt_passwd)
		{
			if (i == argc - 1)
			{
				fprintf(stderr, "%s: insufficient number of arguments for %s\n",
					MYNAME, argv[i]);
				usage();
			}
			opt_passwd = YES;
			i++;
			strcpy(auth_passwd, argv[i]);

			/* BEGIN UGLY HACK TO MAKE argv[i] DISAPPEAR */
			lastchar = &argv[argc-1][0];
			while(*lastchar != '\0') lastchar++;
			p = &argv[i+1][0];
			t = &argv[i][0];
			len = p - t;

			for (j = i+1; j < argc; j++) *(&argv[j-1]) = argv[j];
			argc--;
			for (; p < lastchar; p++) *(p-len) = *p;
			for (j = i; j < argc; j++) argv[j] -= len;
			p -= len;
			*p = '\0';
			for (p++; p < lastchar; p++)  *p = ' ';
			i--;
			/* END UGLY HACK */
		}
		else if (!strcmp(argv[i], "-d"))
		{
			if (op == DELETE_NETINFO)
			{
				fprintf(stderr, "%s: multiple occurances of -d option\n", MYNAME);
				exit(1);
			}
			if (op == MERGE_VALUES)
			{
				fprintf(stderr, "%s: can't use both -m and -d options\n", MYNAME);
				exit(1);
			}

			op = DELETE_NETINFO;
		}
		else if (!strcmp(argv[i], "-m"))
		{
			if (op == DELETE_NETINFO)
			{
				fprintf(stderr, "%s: can't use both -m and -d options", MYNAME);
				exit(1);
			}
			if (op == MERGE_VALUES)
			{
				fprintf(stderr, "%s: multiple occurances of -m option\n\n", MYNAME);
				exit(1);
			}

			op = MERGE_VALUES;
		}
		else if (!strcmp(argv[i], "-v")) verbose = YES;
		else {
			fprintf(stderr, "%s unknown option: %s\n", MYNAME, argv[i]);
			usage();
		}
	}

	format_arg = 0;
	domain_arg = 0;
	mntmap_arg = 0;

	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] != '-' && strcmp(argv[i-1], "-T"))
		{
			if (format_arg == 0) format_arg = i;
			else if (domain_arg == 0) domain_arg = i;
			else if (mntmap_arg == 0) 
			{
				if (strcmp(argv[format_arg], "mountmaps"))
				{
					fprintf(stderr, "%s: unknown argument: %s\n", MYNAME, argv[i]);
					usage();
				}
				mntmap_arg = i;
			}
			else
			{
				fprintf(stderr, "%s: unknown argument: %s\n", MYNAME, argv[i]);
				usage();
			}
		}
	}

	if (opt_user)
	{
		authenticate = YES;
		if (!opt_passwd) opt_promptpw = YES;
	}
	else
	{
		if (opt_passwd || opt_promptpw) authenticate = YES;
	}

	if (opt_passwd) opt_promptpw = NO;

	if (opt_promptpw)
	{
		strcpy(auth_passwd, (char *)getpass("Password: "));
	}

	if (do_open(MYNAME, argv[domain_arg], &domain, opt_tag, timeout, auth_user, auth_passwd))
		usage();

	format = argv[format_arg];
	cat = -1;

	if (raw)
	{
		/* format is directory name */
		raw_load(domain, format, NULL);
		ni_free(domain);
		exit(0);
	}
	else if (!strcmp(format, "aliases"))
	{
		cat = LUCategoryAlias;
		strcpy(nidirname, "/aliases");
	}
	else if (!strcmp(format, "bootparams"))
	{
		cat = LUCategoryBootparam;
		strcpy(nidirname, "/machines");
	}
	else if (!strcmp(format, "bootptab"))
	{
		cat = LUCategoryBootp;
		strcpy(nidirname, "/machines");
	}
	else if (!strcmp(format, "fstab"))
	{
		cat = LUCategoryMount;
		strcpy(nidirname, "/mounts");
	}
	else if (!strcmp(format, "group"))
	{
		cat = LUCategoryGroup;
		strcpy(nidirname, "/groups");
	}
	else if (!strcmp(format, "hosts"))
	{
		cat = LUCategoryHost;
		strcpy(nidirname, "/machines");
	}
	else if (!strcmp(format, "networks"))
	{
		cat = LUCategoryNetwork;
		strcpy(nidirname, "/networks");
	}
	else if (!strcmp(format, "passwd"))
	{
		cat = LUCategoryUser;
		strcpy(nidirname, "/users");
	}
	else if (!strcmp(format, "passwd.43"))
	{
		cat = LUCategoryUser_43;
		strcpy(nidirname, "/users");
	}
	else if (!strcmp(format, "printcap"))
	{
		cat = LUCategoryPrinter;
		strcpy(nidirname, "/printers");
	}
	else if (!strcmp(format, "protocols"))
	{
		cat = LUCategoryProtocol;
		strcpy(nidirname, "/protocols");
	}
	else if (!strcmp(format, "rpc"))
	{
		cat = LUCategoryRpc;
		strcpy(nidirname, "/rpcs");
	}
	else if (!strcmp(format, "services"))
	{
		cat = LUCategoryService;
		strcpy(nidirname, "/services");
	}
	else
	{
		printf("%s: unknown format: %s\n", MYNAME, format);
		usage();
	}

	ff = [[FFAgent alloc] init];
	if (ff == nil)
	{
		fprintf(stderr, "%s: can't init Flat File agent (internal error)\n", MYNAME);
		exit(1);
	}

	[ff setDirectory:NULL];

	all = [ff allItemsWithCategory:cat file:NULL];
	if (all == nil)
	{
		if (verbose) fprintf(stderr, "0 items read from input\n");
		[ff release];
		ni_free(domain);
		exit(0);
	}

	len = [all count];
	if (len == 0)
	{
		if (verbose) fprintf(stderr, "0 items read from input\n");
		[ff release];
		ni_free(domain);
		exit(0);
	}

	if (verbose) fprintf(stderr, "%d items read from input\n", len);

	NI_INIT(&el);
	status = ni_pathsearch(domain, &dir, nidirname);
	if (status == NI_NODIR)
	{
		if (verbose)
			fprintf(stderr, "Creating NetInfo directory %s\n", nidirname);

		status = ni2_createpath(domain, &dir, nidirname);
		if (status != NI_OK)
		{
			fprintf(stderr, "%s: can't create NetInfo directory %s: %s\n",
				MYNAME, nidirname, ni_error(status));
			[all release];
			[ff release];
			ni_free(domain);
			exit(1);
		}
	}
	else if (status != NI_OK)
	{
		fprintf(stderr, "%s: can't access NetInfo directory %s: %s\n",
			MYNAME, nidirname, ni_error(status));
		[all release];
		[ff release];
		ni_free(domain);
		exit(1);
	}

	status = ni_list(domain, &dir, "name", &el);
	if (status != NI_OK)
	{
		fprintf(stderr, "%s: can't list NetInfo %s: %s\n",
			MYNAME, nidirname, ni_error(status));
		[all release];
		[ff release];
		ni_free(domain);
		exit(1);
	}

	nilen = el.ni_entrylist_len;
	if (verbose) fprintf(stderr, "Netinfo %s contains %d items\n",
		nidirname, nilen);

	for (i = 0; i < len; i++)
	{
		item = [all objectAtIndex:i];
		if (verbose)
		{
			fprintf(stderr, "\nProcessing input item:\n");
			[item print:stderr];
		}

		name = [item valueForKey:"name"];
		if (name == NULL)
		{
			fprintf(stderr, "input item has no name - ignored\n");
			continue;
		}

		status = update_dir(domain, nidirname, name, item, op);
		if (status != NI_OK)
		{
			fprintf(stderr, "%s: can't update NetInfo %s/%s: %s\n",
				MYNAME, nidirname, name, ni_error(status));
			[all release];
			[ff release];
			ni_free(domain);
			exit(1);
		}
	}

	[all release];
	[ff release];
	ni_free(domain);
	exit(0);
}
