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
#import "FFParser.h"
#import <NetInfo/nilib2.h>
#import <NetInfo/dsutil.h>
#import "print.h"
#import <sys/types.h>
#import <pwd.h>
#import <libc.h>
#import <stdlib.h>
#import <strings.h>
#import <stdio.h>
#import <netinfo/ni.h>

#define BUFSIZE 8192

#define MYNAME "niload"
#define TIMEOUT 30

#define RETAIN_NETINFO 1
#define DELETE_NETINFO 2
#define MERGE_VALUES 3

BOOL verbose;

extern void raw_load();

/* To avoid including libc.h */
extern uid_t getuid(void);

char *
getLineFromFile(FILE *fp, LUCategory cat)
{
	char s[BUFSIZE];
	char c;
	char *out;
	BOOL getNextLine;
	int len;

    s[0] = '\0';

    fgets(s, BUFSIZE, fp);
    if (s == NULL || s[0] == '\0') return NULL;

	if (s[0] == '#')
	{
		out = strdup("#");
		return out;
	}

	len = strlen(s) - 1;
	s[len] = '\0';

	out = strdup(s);

	/* only printcap, bootparams, and aliases can continue on multiple lines */
	if ((cat != LUCategoryPrinter) && (cat != LUCategoryBootparam) && (cat != LUCategoryAlias))
	{
		return out;
	}

	if (cat == LUCategoryAlias)
	{
		/* alias continues if next line starts with whitespace */
		c = getc(fp);
		while ((c == ' ') || (c == '\t'))
		{
			fgets(s, BUFSIZE, fp);
			if (s == NULL || s[0] == '\0') return out;

			len = strlen(s) - 1;
			s[len] = '\0';
			out = concatString(out, s);

			c = getc(fp);
		}

		/* hit next line - unread a character */
		if (c != EOF) fseek(fp, -1, SEEK_CUR);

		return out;
	}

	/* printcap and bootparams continue if last char is a backslash */
	getNextLine = out[len - 1] == '\\';
	if (getNextLine) out[--len] = '\0';

	while (getNextLine)
	{
		fgets(s, BUFSIZE, fp);
		if (s == NULL || s[0] == '\0') return out;

		len = strlen(s) - 1;
		s[len] = '\0';
		
		getNextLine = s[len - 1] == '\\';
		if (getNextLine) s[--len] = '\0';

		out = concatString(out, s);
	}

	return out;
}

LUArray *
allItemsWithCategory(LUCategory cat, FILE *fp)
{
	char *line;
	LUDictionary *item;
	LUArray *all;
	FFParser *parser;

	if (fp == NULL) return nil;

	parser = [[FFParser alloc] init];

	/* bootptab entries start after a "%%" line */
	if (cat == LUCategoryBootp)
	{
		while (NULL != (line = getLineFromFile(fp, cat)))
		{
			if (!strncmp(line, "%%", 2)) break;
			free(line);
			line = NULL;
		}

		if (line == NULL)
		{
			[parser free];
			return nil;
		}

		free(line);
		line = NULL;
	}

	all = [[LUArray alloc] init];

	while (NULL != (line = getLineFromFile(fp, cat)))
	{
		if (line[0] == '#')
		{
			free(line);
			line = NULL;
			continue;
		}

		item = [parser parse_A:line category:cat];

		free(line);
		line = NULL;
		if (item == nil) continue;
		[all addObject:item];
		[item release];
	}

	[parser free];
	return all;
}

void
usage(void)
{
	fprintf(stderr, "usage: ");
	fprintf(stderr, "%s [opts] format domain\n", MYNAME);
	fprintf(stderr, "opts:\n");
	fprintf(stderr, "\t-d              delete all existing entries from NetInfo\n");
	fprintf(stderr, "\t                before loading new entries\n\n");
	fprintf(stderr, "\t-m              merge properties already in NetInfo\n");
	fprintf(stderr, "\t                when the input contains a duplicate name\n\n");
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
	int p, v, plen, vlen;
	ni_index pwhere, vwhere;

	plen = old->ni_proplist_len;
	for (p = 0; p < plen; p++)
	{
		pwhere = ni_proplist_match((const ni_proplist)*new, (const ni_name)old->ni_proplist_val[p].nip_name, NULL);
		if (pwhere == NI_INDEX_NULL)
		{
			ni_proplist_insert(new, (const ni_property)old->ni_proplist_val[p], NI_INDEX_NULL);
		}
		else
		{
			vlen = old->ni_proplist_val[p].nip_val.ni_namelist_len;
			for (v = 0; v < vlen; v++)
			{
				vwhere = ni_namelist_match(new->ni_proplist_val[pwhere].nip_val, old->ni_proplist_val[p].nip_val.ni_namelist_val[v]);
				if (vwhere == NI_INDEX_NULL)
				{
					ni_namelist_insert(&(new->ni_proplist_val[pwhere].nip_val), old->ni_proplist_val[p].nip_val.ni_namelist_val[v], NI_INDEX_NULL);
				}
			}
		}
	}
}

ni_status 
update_dir(void *domain, char *parent, char *name, LUDictionary *item, BOOL merge)
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

	pl = [item niProplist];

	if (merge)
	{
		/* read from netinfo */
		status = ni_read(domain, &dir, &oldpl);
		if (status != NI_OK)
		{
			ni_proplist_free(pl);
			return status;
		}

		/* merge values */
		merge_proplist(pl, &oldpl);
		ni_proplist_free(&oldpl);
	}
	
	if (verbose) fprintf(stderr, "%s directory %s\n", merge ? "merging" : "writing", path);
	status = ni_write(domain, &dir, *pl);
	ni_proplist_free(pl);
	return status;
}

int
main(int argc, char *argv[])
{
	LUArray *all;
	LUDictionary *item;
	ni_entrylist el;
	int i;
	int j, len, nilen;
	char *lastchar, *p, *t;
	BOOL opt_promptpw, opt_passwd;
	BOOL merge, delete;
	int format_arg, domain_arg;
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
	if (pw == NULL)
	{
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
	merge = NO;
	delete = NO;

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
				fprintf(stderr, "%s: insufficient number of arguments for %s\n", MYNAME, argv[i]);
				usage();
			}
			timeout = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "-u") && !opt_user)
		{
			if (i == argc - 1)
			{
				fprintf(stderr, "%s: insufficient number of arguments for %s\n", MYNAME, argv[i]);
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
			delete = YES;
		}
		else if (!strcmp(argv[i], "-m"))
		{
			merge = YES;
		}
		else if (!strcmp(argv[i], "-v")) verbose = YES;
		else
		{
			fprintf(stderr, "%s unknown option: %s\n", MYNAME, argv[i]);
			usage();
		}
	}

	format_arg = 0;
	domain_arg = 0;

	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] != '-' && strcmp(argv[i-1], "-T"))
		{
			if (format_arg == 0) format_arg = i;
			else if (domain_arg == 0) domain_arg = i;
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

	all = allItemsWithCategory(cat, stdin);
	if (all == nil)
	{
		if (verbose) fprintf(stderr, "0 items read from input\n");
		ni_free(domain);
		exit(0);
	}

	len = [all count];
	if (len == 0)
	{
		if (verbose) fprintf(stderr, "0 items read from input\n");
		ni_free(domain);
		exit(0);
	}

	if (verbose) fprintf(stderr, "%d items read from input\n", len);

	if (delete)
	{
		if (verbose) fprintf(stderr, "Deleting exiting NetInfo directory %s\n", nidirname);
		status = ni2_destroy(domain, nidirname);
		if (status != NI_OK)
		{
			fprintf(stderr, "%s: can't delete NetInfo directory %s: %s\n", MYNAME, nidirname, ni_error(status));
				[all release];
				ni_free(domain);
			exit(1);
		}
	}

	NI_INIT(&el);
	status = ni_pathsearch(domain, &dir, nidirname);
	if (status == NI_NODIR)
	{
		if (verbose) fprintf(stderr, "Creating NetInfo directory %s\n", nidirname);

		status = ni2_createpath(domain, &dir, nidirname);
		if (status != NI_OK)
		{
			fprintf(stderr, "%s: can't create NetInfo directory %s: %s\n", MYNAME, nidirname, ni_error(status));
			[all release];
			ni_free(domain);
			exit(1);
		}
	}
	else if (status != NI_OK)
	{
		fprintf(stderr, "%s: can't access NetInfo directory %s: %s\n", MYNAME, nidirname, ni_error(status));
		[all release];
		ni_free(domain);
		exit(1);
	}
	
	status = ni_list(domain, &dir, "name", &el);
	if (status != NI_OK)
	{
		fprintf(stderr, "%s: can't list NetInfo %s: %s\n", MYNAME, nidirname, ni_error(status));
		[all release];
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

		status = update_dir(domain, nidirname, name, item, merge);
		if (status != NI_OK)
		{
			fprintf(stderr, "%s: can't update NetInfo %s/%s: %s\n", MYNAME, nidirname, name, ni_error(status));
			[all release];
			ni_free(domain);
			exit(1);
		}
	}

	[all release];
	ni_free(domain);
	exit(0);
}
