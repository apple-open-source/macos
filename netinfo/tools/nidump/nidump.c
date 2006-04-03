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

#import <sys/types.h>
#import <stdio.h>
#import <netinet/in.h>
#import <arpa/inet.h>
#import <string.h>
#import <stdlib.h>
#import <netinfo/ni.h>
#import <NetInfo/nilib2.h>

#define MAXLINELEN 72
#define MYNAME "nidump"
#define TAB_STOP 2

void print_string(char *s, FILE *out)
{
	int i, len;

	len = strlen(s);
	for (i = 0; i < len; i++)
	{
		if (s[i] == '"') fprintf(out, "\\\"");
		else putc(s[i], out);
	}
}

void print_nidir(void *ni, ni_id *dir, int indent, FILE *out, char *last)
{
	int	pn, vn;
	ni_idlist il;
	ni_proplist	pl;
	ni_property	prop;
	ni_namelist	values;
	ni_status status;
	int i, len;
	ni_id child;

	NI_INIT(&pl);
	status = ni_read(ni, dir, &pl);
	if (status != NI_OK) return;

	for (i = 0; i < indent; i++) putc(' ', out);
	fprintf(out, "{\n");

	indent += TAB_STOP;
	/* for each property */
	for (pn = 0; pn < pl.ni_proplist_len; pn++) {

		prop = pl.ni_proplist_val[pn];

		/* print the property key */
		for (i = 0; i < indent; i++) putc(' ', out);
		fprintf(out, "\"");
		print_string(prop.nip_name, out);
		fprintf(out, "\" = ( ");
	
		values = prop.nip_val;

		/* for each value in the namelist for this property */
		for (vn = 0; vn < values.ni_namelist_len; vn++) {
			/* print the value */
			fprintf(out, "\"");
			print_string(values.ni_namelist_val[vn], out);
			fprintf(out, "\"");
			if ((vn + 1) < values.ni_namelist_len)
				fprintf(out, ", ");
			else
				fprintf(out, " ");
		}
		fprintf(out, ");\n");
	}

	ni_proplist_free(&pl);

	status = ni_children(ni, dir, &il);
	if (status != NI_OK) return;

	len = il.ni_idlist_len;

	if (len > 0)
	{
		for (i = 0; i < indent; i++) putc(' ', out);
		fprintf(out, "CHILDREN = (\n");

		for (i = 0; i < len; i++)
		{
			child.nii_object = il.ni_idlist_val[i];
			print_nidir(ni, &child, indent+TAB_STOP, stdout,
				((i+1) < len) ? "," : "");
		}

		for (i = 0; i < indent; i++) putc(' ', out);
		fprintf(out, ")\n");
	}

	indent -= TAB_STOP;

	for (i = 0; i < indent; i++) putc(' ', out);
	fprintf(out, "}%s\n", last);

	ni_idlist_free(&il);
}

void dump_raw(void *ni, char *path)
{
	/* XXX */

	ni_id dir;
	ni_status status;

	status = ni_pathsearch(ni, &dir, path);
	if (status != NI_OK) return;
	
	print_nidir(ni, &dir, 0, stdout, "");
}

int check_pl(ni_proplist p, const char *key, int required)
{
	ni_index where;
	ni_namelist *nl;

	where = ni_proplist_match((const ni_proplist)p, key, NULL);
	if (where == NI_INDEX_NULL) return 0;
	nl = &(p.ni_proplist_val[where].nip_val);
	if (nl->ni_namelist_len < required) return 0;
	return 1;
}

int print_pl_first(ni_proplist p, const char *key)
{
	ni_index where;
	ni_namelist *nl;

	where = ni_proplist_match((const ni_proplist)p, key, NULL);
	if (where == NI_INDEX_NULL) return 0;
	nl = &(p.ni_proplist_val[where].nip_val);
	if (nl->ni_namelist_len == 0) return 0;
	printf("%s", nl->ni_namelist_val[0]);
	return 1;
}

void print_pl_from(ni_proplist p, const char *key, const char *sep, int start)
{
	ni_index where;
	ni_namelist *nl;
	int i, len;

	where = ni_proplist_match((const ni_proplist)p, key, NULL);
	if (where == NI_INDEX_NULL) return;
	nl = &(p.ni_proplist_val[where].nip_val);
	len = nl->ni_namelist_len;

	for (i = start; i < len; i++)
	{
		printf("%s", nl->ni_namelist_val[i]);
		if (i < (len - 1)) printf("%s", sep);
	}
}

void print_pl_from_prefix(ni_proplist p, const char *key, const char *sep, const char *pre, int start)
{
	ni_index where;
	ni_namelist *nl;
	int i, len;

	where = ni_proplist_match((const ni_proplist)p, key, NULL);
	if (where == NI_INDEX_NULL) return;
	nl = &(p.ni_proplist_val[where].nip_val);
	len = nl->ni_namelist_len;

	for (i = start; i < len; i++)
	{
		printf("%s%s", pre, nl->ni_namelist_val[i]);
		if (i < (len - 1)) printf("%s", sep);
	}
}

void print_alias(ni_proplist p)
{
	ni_index where, w1;
	ni_namelist *nl;
	int i, len, linelen, wordlen;

	w1 = ni_proplist_match((const ni_proplist)p, "members", NULL);
	if (w1 == NI_INDEX_NULL) return;
	nl = &(p.ni_proplist_val[w1].nip_val);
	len = nl->ni_namelist_len;
	if (len < 1) return;

	where = ni_proplist_match((const ni_proplist)p, "name", NULL);
	if (where == NI_INDEX_NULL) return;
	nl = &(p.ni_proplist_val[where].nip_val);
	len = nl->ni_namelist_len;
	if (len < 1) return;

	linelen = strlen(nl->ni_namelist_val[0]) + 2;
	printf("%s: ", nl->ni_namelist_val[0]);

	nl = &(p.ni_proplist_val[w1].nip_val);
	len = nl->ni_namelist_len;

	for (i = 0; i < len; i++)
	{
		wordlen = strlen(nl->ni_namelist_val[i]);
		if (i < (len - 1)) wordlen += 1;
		linelen += wordlen;
		if (linelen > MAXLINELEN)
		{
			printf("\n\t");
			linelen = wordlen + 8;
		}
		printf("%s", nl->ni_namelist_val[i]);
		if (i < (len - 1)) printf(",");
	}
	printf("\n");
}

char *printer_prop(char *key, ni_namelist *nl)
{
	static char prop[1024];

	sprintf(prop, "%s", key);
	if (nl-> ni_namelist_len == 0)
	{
		strcat(prop, ":");
		return prop;
	}

	if (nl->ni_namelist_val[0][0] != '#') strcat(prop, "=");
	strcat(prop, nl->ni_namelist_val[0]);
	strcat(prop, ":");
	return prop;
}

void print_printer(ni_proplist p)
{
	ni_index where;
	ni_namelist *nl;
	int i, len, linelen, wordlen;
	char *word;

	where = ni_proplist_match((const ni_proplist)p, "name", NULL);
	if (where == NI_INDEX_NULL) return;
	nl = &(p.ni_proplist_val[where].nip_val);
	len = nl->ni_namelist_len;
	if (len < 1) return;

	for (i = 0; i < len; i++)
	{
		printf("%s", nl->ni_namelist_val[i]);
		if (i < (len - 1)) printf("|");
	}
	printf(": \\\n\t:");

	linelen = 9;

	len = p.ni_proplist_len;

	for (i = 0; i < len; i++)
	{
		if (!strcmp("name", p.ni_proplist_val[i].nip_name)) continue;

		nl = &(p.ni_proplist_val[i].nip_val);
		word = printer_prop(p.ni_proplist_val[i].nip_name, nl);
		wordlen = strlen(word);
		linelen += wordlen;
		if (linelen > MAXLINELEN)
		{
			printf(" \\\n\t:");
			linelen = wordlen + 9;
		}
		printf("%s", word);
	}

	printf("\n");
}

void dump_passwd(void *ni)
{
	ni_id dir;
	ni_status status;
	ni_idlist il;
	ni_proplist pl;
	int i, len;

	status = ni_pathsearch(ni, &dir, "/users");
	if (status != NI_OK) return;

	NI_INIT(&il);
	status = ni_children(ni, &dir, &il);
	if (status != NI_OK) return;

	len = il.ni_idlist_len;
	for (i = 0; i < len; i++)
	{
		dir.nii_object = il.ni_idlist_val[i];

		NI_INIT(&pl);
		status = ni_read(ni, &dir, &pl);
		if (status != NI_OK)
		{
			ni_idlist_free(&il);
			return;
		}

		if (!print_pl_first(pl, "name")) continue;
		printf(":");
		print_pl_first(pl, "passwd");
		printf(":");
		print_pl_first(pl, "uid");
		printf(":");
		print_pl_first(pl, "gid");
		printf(":");
		print_pl_first(pl, "class");
		printf(":");
		if (!print_pl_first(pl, "change")) printf("0");
		printf(":");
		if (!print_pl_first(pl, "expire")) printf("0");
		printf(":");
		print_pl_first(pl, "realname");
		printf(":");
		print_pl_first(pl, "home");
		printf(":");
		print_pl_first(pl, "shell");
		printf("\n");

		ni_proplist_free(&pl);
	}

	ni_idlist_free(&il);
}

void dump_aliases(void *ni)
{
	ni_id dir;
	ni_status status;
	ni_idlist il;
	ni_proplist pl;
	int i, len;

	status = ni_pathsearch(ni, &dir, "/aliases");
	if (status != NI_OK) return;

	status = ni_children(ni, &dir, &il);
	if (status != NI_OK) return;

	len = il.ni_idlist_len;
	for (i = 0; i < len; i++)
	{
		dir.nii_object = il.ni_idlist_val[i];

		NI_INIT(&pl);
		status = ni_read(ni, &dir, &pl);
		if (status != NI_OK)
		{
			ni_idlist_free(&il);
			return;
		}

		print_alias(pl);

		ni_proplist_free(&pl);
	}

	ni_idlist_free(&il);
}

void dump_bootparams(void *ni)
{
	ni_id dir;
	ni_status status;
	ni_idlist il;
	ni_proplist pl;
	int i, len;

	status = ni_pathsearch(ni, &dir, "/machines");
	if (status != NI_OK) return;

	status = ni_children(ni, &dir, &il);
	if (status != NI_OK) return;

	len = il.ni_idlist_len;
	for (i = 0; i < len; i++)
	{
		dir.nii_object = il.ni_idlist_val[i];

		NI_INIT(&pl);
		status = ni_read(ni, &dir, &pl);
		if (status != NI_OK)
		{
			ni_idlist_free(&il);
			return;
		}

		if (!check_pl(pl, "name", 1)) continue;
		if (!check_pl(pl, "bootparams", 1)) continue;

		print_pl_first(pl, "name");
		printf(" ");
		print_pl_from(pl, "bootparams", " \\\n\t", 0);
		printf("\n");

		ni_proplist_free(&pl);
	}

	ni_idlist_free(&il);
}

void dump_bootptab(void *ni)
{
	ni_id dir;
	ni_status status;
	ni_idlist il;
	ni_proplist pl;
	int i, len;

	printf("/private/tftpboot:/\n");
	printf("mach\n");
	printf("%%%%\n");

	status = ni_pathsearch(ni, &dir, "/machines");
	if (status != NI_OK) return;

	status = ni_children(ni, &dir, &il);
	if (status != NI_OK) return;

	len = il.ni_idlist_len;
	for (i = 0; i < len; i++)
	{
		dir.nii_object = il.ni_idlist_val[i];

		NI_INIT(&pl);
		status = ni_read(ni, &dir, &pl);
		if (status != NI_OK)
		{
			ni_idlist_free(&il);
			return;
		}

		if (!print_pl_first(pl, "name")) continue;
		printf("\t");
		printf("1");
		printf("\t");
		if (!print_pl_first(pl, "en_address")) printf("0:0:0:0:0:0");
		printf("\t");
		if (!print_pl_first(pl, "ip_address")) printf("0.0.0.0");
		printf("\t");
		if (!print_pl_first(pl, "bootfile")) printf("mach");
		printf("\n");

		ni_proplist_free(&pl);
	}

	ni_idlist_free(&il);
}

void dump_exports(void *ni)
{
	ni_id dir;
	ni_status status;
	ni_idlist il;
	ni_proplist pl;
	int i, len;

	status = ni_pathsearch(ni, &dir, "/exports");
	if (status != NI_OK) return;

	status = ni_children(ni, &dir, &il);
	if (status != NI_OK) return;

	len = il.ni_idlist_len;
	for (i = 0; i < len; i++)
	{
		dir.nii_object = il.ni_idlist_val[i];

		NI_INIT(&pl);
		status = ni_read(ni, &dir, &pl);
		if (status != NI_OK)
		{
			ni_idlist_free(&il);
			return;
		}

		if (!print_pl_first(pl, "name")) continue;
		printf(" ");
		print_pl_from(pl, "name", " ", 1);
		printf(" ");
		print_pl_from_prefix(pl, "opts", " ", "-", 0);
		printf(" ");
		print_pl_from(pl, "clients", " ", 0);
		printf("\n");

		ni_proplist_free(&pl);
	}

	ni_idlist_free(&il);
}

void dump_fstab(void *ni)
{
	ni_id dir;
	ni_status status;
	ni_idlist il;
	ni_proplist pl;
	int i, len;
	ni_index where;

	status = ni_pathsearch(ni, &dir, "/mounts");
	if (status != NI_OK) return;

	status = ni_children(ni, &dir, &il);
	if (status != NI_OK) return;

	len = il.ni_idlist_len;
	for (i = 0; i < len; i++)
	{
		dir.nii_object = il.ni_idlist_val[i];

		NI_INIT(&pl);
		status = ni_read(ni, &dir, &pl);
		if (status != NI_OK)
		{
			ni_idlist_free(&il);
			return;
		}

		if (!print_pl_first(pl, "name")) continue;
		printf(" ");
		print_pl_first(pl, "dir");
		printf(" ");
		if (!print_pl_first(pl, "vfstype")) printf("nfs");
		printf(" ");
		where = ni_proplist_match(pl, "opts", NULL);
		if (where == NI_INDEX_NULL)
		{
			printf("rw");
		}
		else if (pl.ni_proplist_val[where].nip_val.ni_namelist_len == 0)
		{
			printf("rw");
		}
		else
		{
			print_pl_from(pl, "opts", ",", 0);
		}
		printf(" ");
		if (!print_pl_first(pl, "freq")) printf("0");
		printf(" ");
		if (!print_pl_first(pl, "passno")) printf("0");
		printf("\n");

		ni_proplist_free(&pl);
	}

	ni_idlist_free(&il);
}

void dump_printcap(void *ni)
{
	ni_id dir;
	ni_status status;
	ni_idlist il;
	ni_proplist pl;
	int i, len;

	status = ni_pathsearch(ni, &dir, "/printers");
	if (status != NI_OK) return;

	status = ni_children(ni, &dir, &il);
	if (status != NI_OK) return;

	len = il.ni_idlist_len;
	for (i = 0; i < len; i++)
	{
		dir.nii_object = il.ni_idlist_val[i];

		NI_INIT(&pl);
		status = ni_read(ni, &dir, &pl);
		if (status != NI_OK)
		{
			ni_idlist_free(&il);
			return;
		}

		print_printer(pl);

		ni_proplist_free(&pl);
	}

	ni_idlist_free(&il);
}

void dump_group(void *ni)
{
	ni_id dir;
	ni_status status;
	ni_idlist il;
	ni_proplist pl;
	int i, len;

	status = ni_pathsearch(ni, &dir, "/groups");
	if (status != NI_OK) return;

	status = ni_children(ni, &dir, &il);
	if (status != NI_OK) return;

	len = il.ni_idlist_len;
	for (i = 0; i < len; i++)
	{
		dir.nii_object = il.ni_idlist_val[i];

		NI_INIT(&pl);
		status = ni_read(ni, &dir, &pl);
		if (status != NI_OK)
		{
			ni_idlist_free(&il);
			return;
		}

		if (!print_pl_first(pl, "name")) continue;
		printf(":");
		print_pl_first(pl, "passwd");
		printf(":");
		print_pl_first(pl, "gid");
		printf(":");
		print_pl_from(pl, "users", ",", 0);
		printf("\n");

		ni_proplist_free(&pl);
	}

	ni_idlist_free(&il);
}

void dump_hosts(void *ni)
{
	ni_id dir;
	ni_status status;
	ni_idlist il;
	ni_proplist pl;
	int i, j, len, naddrs;
	ni_index where;

	status = ni_pathsearch(ni, &dir, "/machines");
	if (status != NI_OK) return;

	status = ni_children(ni, &dir, &il);
	if (status != NI_OK) return;

	len = il.ni_idlist_len;
	for (i = 0; i < len; i++)
	{
		dir.nii_object = il.ni_idlist_val[i];

		NI_INIT(&pl);
		status = ni_read(ni, &dir, &pl);
		if (status != NI_OK)
		{
			ni_idlist_free(&il);
			return;
		}

		where = ni_proplist_match(pl, "ip_address", NULL);
		if (where == NI_INDEX_NULL)
		{
			ni_proplist_free(&pl);
			continue;
		}

		naddrs = pl.ni_proplist_val[where].nip_val.ni_namelist_len;
		for (j = 0; j < naddrs; j++)
		{
			printf("%s", pl.ni_proplist_val[where].nip_val.ni_namelist_val[j]);
			printf("\t");
			print_pl_from(pl, "name", " ", 0);
			printf("\n");
		}

		ni_proplist_free(&pl);
	}

	ni_idlist_free(&il);
}

void dump_ethers(void *ni)
{
	ni_id dir;
	ni_status status;
	ni_idlist il;
	ni_proplist pl;
	int i, len;

	status = ni_pathsearch(ni, &dir, "/machines");
	if (status != NI_OK) return;

	status = ni_children(ni, &dir, &il);
	if (status != NI_OK) return;

	len = il.ni_idlist_len;
	for (i = 0; i < len; i++)
	{
		dir.nii_object = il.ni_idlist_val[i];

		NI_INIT(&pl);
		status = ni_read(ni, &dir, &pl);
		if (status != NI_OK)
		{
			ni_idlist_free(&il);
			return;
		}

		if (!print_pl_first(pl, "en_address")) continue;
		printf("\t");
		print_pl_from(pl, "name", " ", 0);
		printf("\n");

		ni_proplist_free(&pl);
	}

	ni_idlist_free(&il);
}

void dump_networks(void *ni)
{
	ni_id dir;
	ni_status status;
	ni_idlist il;
	ni_proplist pl;
	int i, len;

	status = ni_pathsearch(ni, &dir, "/networks");
	if (status != NI_OK) return;

	status = ni_children(ni, &dir, &il);
	if (status != NI_OK) return;

	len = il.ni_idlist_len;
	for (i = 0; i < len; i++)
	{
		dir.nii_object = il.ni_idlist_val[i];

		NI_INIT(&pl);
		status = ni_read(ni, &dir, &pl);
		if (status != NI_OK)
		{
			ni_idlist_free(&il);
			return;
		}

		if (!print_pl_first(pl, "name")) continue;
		printf("\t");
		print_pl_first(pl, "address");
		printf("\t");
		print_pl_from(pl, "name", "\t", 1);
		printf("\n");

		ni_proplist_free(&pl);
	}

	ni_idlist_free(&il);
}

void dump_services(void *ni)
{
	ni_id dir;
	ni_status status;
	ni_idlist il;
	ni_proplist pl;
	int i, len;

	status = ni_pathsearch(ni, &dir, "/services");
	if (status != NI_OK) return;

	status = ni_children(ni, &dir, &il);
	if (status != NI_OK) return;

	len = il.ni_idlist_len;
	for (i = 0; i < len; i++)
	{
		dir.nii_object = il.ni_idlist_val[i];

		NI_INIT(&pl);
		status = ni_read(ni, &dir, &pl);
		if (status != NI_OK)
		{
			ni_idlist_free(&il);
			return;
		}

		if (!print_pl_first(pl, "name")) continue;
		printf("\t");
		print_pl_first(pl, "port");
		printf("/");
		print_pl_first(pl, "protocol");
		printf("\t");
		print_pl_from(pl, "name", "\t", 1);
		printf("\n");

		ni_proplist_free(&pl);
	}

	ni_idlist_free(&il);
}

void dump_protocols(void *ni)
{
	ni_id dir;
	ni_status status;
	ni_idlist il;
	ni_proplist pl;
	int i, len;

	status = ni_pathsearch(ni, &dir, "/protocols");
	if (status != NI_OK) return;

	status = ni_children(ni, &dir, &il);
	if (status != NI_OK) return;

	len = il.ni_idlist_len;
	for (i = 0; i < len; i++)
	{
		dir.nii_object = il.ni_idlist_val[i];

		NI_INIT(&pl);
		status = ni_read(ni, &dir, &pl);
		if (status != NI_OK)
		{
			ni_idlist_free(&il);
			return;
		}

		if (!print_pl_first(pl, "name")) continue;
		printf("\t");
		print_pl_first(pl, "number");
		printf("\t");
		print_pl_from(pl, "name", "\t", 1);
		printf("\n");

		ni_proplist_free(&pl);
	}

	ni_idlist_free(&il);
}

void dump_resolv(void *ni)
{
	ni_id dir;
	ni_status status;
	ni_proplist pl;
	ni_namelist *nl;
	int i, len;
	ni_index where;

	status = ni_pathsearch(ni, &dir, "/locations/resolver");
	if (status != NI_OK) return;

	NI_INIT(&pl);
	status = ni_read(ni, &dir, &pl);
	if (status != NI_OK) return;

	if (!check_pl(pl, "domain", 1)) return;
	where = ni_proplist_match((const ni_proplist)pl, "nameserver", NULL);
	if (where == NI_INDEX_NULL) return;
	nl = &(pl.ni_proplist_val[where].nip_val);
	len = nl->ni_namelist_len;
	if (len < 1) return;

	printf("domain ");
	print_pl_first(pl, "domain");
	printf("\n");

	if (check_pl(pl, "search", 1))
	{
		printf("search ");
		print_pl_from(pl, "search", " ", 0);
		printf("\n");
	}

	for (i = 0; i < len; i++)
	{
		printf("nameserver %s\n", nl->ni_namelist_val[i]);
	}

	ni_proplist_free(&pl);
}

void dump_rpc(void *ni)
{
	ni_id dir;
	ni_status status;
	ni_idlist il;
	ni_proplist pl;
	int i, len;

	status = ni_pathsearch(ni, &dir, "/rpcs");
	if (status != NI_OK) return;

	status = ni_children(ni, &dir, &il);
	if (status != NI_OK) return;

	len = il.ni_idlist_len;
	for (i = 0; i < len; i++)
	{
		dir.nii_object = il.ni_idlist_val[i];

		NI_INIT(&pl);
		status = ni_read(ni, &dir, &pl);
		if (status != NI_OK)
		{
			ni_idlist_free(&il);
			return;
		}

		if (!print_pl_first(pl, "name")) continue;
		printf("\t");
		print_pl_first(pl, "number");
		printf("\t");
		print_pl_from(pl, "name", "\t", 1);
		printf("\n");

		ni_proplist_free(&pl);
	}

	ni_idlist_free(&il);
}

void usage(char *name)
{
	fprintf(stderr, "usage: %s [-r] [-T timeout] {directory | format} [-t] domain\n",
		name);
	fprintf(stderr, "known formats:\n");
	fprintf(stderr, "\taliases\n");
	fprintf(stderr, "\tbootptab\n");
	fprintf(stderr, "\tbootparams\n");
	fprintf(stderr, "\tethers\n");
	fprintf(stderr, "\texports\n");
	fprintf(stderr, "\tfstab\n");
	fprintf(stderr, "\tgroup\n");
	fprintf(stderr, "\thosts\n");
	fprintf(stderr, "\tnetworks\n");
	fprintf(stderr, "\tpasswd\n");
	fprintf(stderr, "\tprintcap\n");
	fprintf(stderr, "\tprotocols\n");
	fprintf(stderr, "\tresolv.conf\n");
	fprintf(stderr, "\trpc\n");
	fprintf(stderr, "\tservices\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	void *ni;
	int i, timeout, optcount;
	int dirarg, domarg, mntmaparg;
	int bytag, raw, timeopt;
	char *dumpit;

	if (argc < 3) usage(MYNAME);

	optcount = 0;

	bytag = 0;
	timeopt = 0;
	raw = 0;

	dirarg = 0;
	domarg = 0;
	mntmaparg = 0;

	timeout = 30;

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-t"))
		{
			if (bytag) usage(MYNAME);
			if (domarg > 0) usage(MYNAME);
			if (i == (argc - 1)) usage(MYNAME);
			bytag = 1;
			domarg = ++i;
			optcount++;
		}
		else if (!strcmp(argv[i], "-r"))
		{
			if (raw) usage(MYNAME);
			raw = 1;
			optcount++;
		}
		else if (!strcmp(argv[i], "-T"))
		{
			if (bytag) usage(MYNAME);
			if (i == (argc - 1)) usage(MYNAME);
			timeout = atoi(argv[++i]);
			optcount++;
		}
		else if (dirarg == 0) dirarg = i;
		else if (domarg == 0) domarg = i;
		else if (mntmaparg == 0) mntmaparg = i;
		else usage(MYNAME);
	}

	if (dirarg == 0) usage(MYNAME);
	if (domarg == 0) usage(MYNAME);

	dumpit = argv[dirarg];

	if (do_open(MYNAME, argv[domarg], &ni, bytag, timeout, NULL, NULL)) usage(MYNAME);

	if (ni == NULL)
	{
		fprintf(stderr, "can't connect to domain %s\n", argv[domarg]);
		exit(1);
	}

	if (raw)
	{
		dump_raw(ni, dumpit);
	}

	else if (!strcmp(dumpit, "aliases")) dump_aliases(ni);
	else if (!strcmp(dumpit, "bootptab")) dump_bootptab(ni);
	else if (!strcmp(dumpit, "bootparams")) dump_bootparams(ni);
	else if (!strcmp(dumpit, "ethers")) dump_ethers(ni);
	else if (!strcmp(dumpit, "exports")) dump_exports(ni);
	else if (!strcmp(dumpit, "fstab")) dump_fstab(ni);
	else if (!strcmp(dumpit, "group")) dump_group(ni);
	else if (!strcmp(dumpit, "hosts")) dump_hosts(ni);
	else if (!strcmp(dumpit, "networks")) dump_networks(ni);
	else if (!strcmp(dumpit, "passwd")) dump_passwd(ni);
	else if (!strcmp(dumpit, "printcap")) dump_printcap(ni);
	else if (!strcmp(dumpit, "protocols")) dump_protocols(ni);
	else if (!strcmp(dumpit, "resolv.conf")) dump_resolv(ni);
	else if (!strcmp(dumpit, "rpc")) dump_rpc(ni);
	else if (!strcmp(dumpit, "services")) dump_services(ni);
	else usage(MYNAME);

	exit(0);
}
