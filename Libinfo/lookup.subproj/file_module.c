/*
 * Copyright (c) 2008-2009 Apple Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <si_module.h>
#include <paths.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <ils.h>

/* These really should be in netdb.h & etc. */
#define _PATH_RPCS "/etc/rpc"
#define _PATH_ALIASES "/etc/aliases"
#define _PATH_ETHERS "/etc/ethers"

static si_item_t *rootfs = NULL;

static char *
_fsi_copy_string(char *s)
{
	int len;
	char *t;

	if (s == NULL) return NULL;

	len = strlen(s) + 1;
	t = malloc(len);
	bcopy(s, t, len);
	return t;
}

static char **
_fsi_append_string(char *s, char **l)
{
	int i, len;

	if (s == NULL) return l;
	if (l == NULL) 
	{
		l = (char **)malloc(2 * sizeof(char *));
		l[0] = s;
		l[1] = NULL;
		return l;
	}

	for (i = 0; l[i] != NULL; i++);
	len = i + 1; /* count the NULL on the end of the list too! */

	l = (char **)reallocf(l, (len + 1) * sizeof(char *));

	l[len - 1] = s;
	l[len] = NULL;
	return l;
}

__private_extern__ char **
_fsi_tokenize(char *data, const char *sep, int trailing_empty, int *ntokens)
{
	char **tokens;
	int p, i, start, end, more, len, end_on_sep;
	int scanning;

	tokens = NULL;
	end_on_sep = 0;

	if (data == NULL) return NULL;

	if (ntokens != NULL) *ntokens = 0;
	if (sep == NULL)
	{
		tokens = _fsi_append_string(data, tokens);
		if (ntokens != NULL) *ntokens = *ntokens + 1;
		return tokens;
	}

	len = strlen(sep);
	p = 0;

	while (data[p] != '\0')
	{
		end_on_sep = 1;
		/* skip leading white space */
		while ((data[p] == ' ') || (data[p] == '\t') || (data[p] == '\n')) p++;

		/* check for end of line */
		if (data[p] == '\0') break;

		/* scan for separator */
		start = p;
		end = p;
		scanning = 1;
		end_on_sep = 0;

		while (scanning == 1)
		{
			if (data[p] == '\0') break;

			for (i = 0; i < len; i++)
			{
				if (data[p] == sep[i])
				{
					scanning = 0;
					end_on_sep = 1;
					break;
				}
			}

			/* end is last non-whitespace character */
			if ((scanning == 1) && (data[p] != ' ') && (data[p] != '\t') && (data[p] != '\n')) end = p;

			p += scanning;
		}

		/* see if there's data left after p */
		more = 0;
		if (data[p] != '\0') more = 1;

		/* set the character following the token to nul */
		if (start == p) data[p] = '\0';
		else data[end + 1] = '\0';

		tokens = _fsi_append_string(data + start, tokens);
		if (ntokens != NULL) *ntokens = *ntokens + 1;
		p += more;
	}

	if ((end_on_sep == 1) && (trailing_empty != 0))
	{
		/* if the scan ended on an empty token, add a null string */
		tokens = _fsi_append_string(data + p, tokens);
		if (ntokens != NULL) *ntokens = *ntokens + 1;
	}

	return tokens;
}

__private_extern__ char *
_fsi_get_line(FILE *fp)
{
	char s[4096];
	char *out;

	s[0] = '\0';

	fgets(s, sizeof(s), fp);
	if ((s == NULL) || (s[0] == '\0')) return NULL;

	if (s[0] != '#') s[strlen(s) - 1] = '\0';

	out = _fsi_copy_string(s);
	return out;
}

/* USERS */

static si_item_t *
_fsi_parse_user(si_mod_t *si, const char *name, uid_t uid, int which, char *data, int format, uint64_t sec, uint64_t nsec)
{
	char **tokens;
	int ntokens, match;
	time_t change, exsire;
	si_item_t *item;
	uid_t xuid;

	if (data == NULL) return NULL;

	ntokens = 0;
	tokens = _fsi_tokenize(data, ":", 1, &ntokens);
	if (((format == 0) && (ntokens != 10)) || ((format == 1) && (ntokens !=  7)))
	{
		free(tokens);
		return NULL;
	}

	xuid = atoi(tokens[2]);
	match = 0;

	/* XXX MATCH GECOS? XXX*/
	if (which == SEL_ALL) match = 1;
	else if ((which == SEL_NAME) && (string_equal(name, tokens[0]))) match = 1;
	else if ((which == SEL_NUMBER) && (uid == xuid)) match = 1;

	if (match == 0)
	{
		free(tokens);
		return NULL;
	}

	if (format == 0)
	{
		/* master.passwd: name[0] passwd[1] uid[2] gid[3] class[4] change[5] exsire[6] gecos[7] dir[8] shell[9] */
		/* struct pwd: name[0] passwd[1] uid[2] gid[3] change[5] class[4] gecos[7] dir[8] shell[9] exsire[6] */
		change = atoi(tokens[5]);
		exsire = atoi(tokens[6]);
		item = (si_item_t *)LI_ils_create("L4488ss44LssssL", (unsigned long)si, CATEGORY_USER, 1, sec, nsec, tokens[0], tokens[1], xuid, atoi(tokens[3]), change, tokens[4], tokens[7], tokens[8], tokens[9], exsire);
	}
	else
	{
		/* passwd: name[0] passwd[1] uid[2] gid[3] gecos[4] dir[5] shell[6] */
		/* struct pwd: name[0] passwd[1] uid[2] gid[3] change[-] class[-] gecos[4] dir[5] shell[6] exsire[-] */
		item = (si_item_t *)LI_ils_create("L4488ss44LssssL", (unsigned long)si, CATEGORY_USER, 1, sec, nsec, tokens[0], tokens[1], xuid, atoi(tokens[3]), 0, "", tokens[4], tokens[5], tokens[6], 0);
	}

	free(tokens); 
	return item;
}

static void *
_fsi_get_user(si_mod_t *si, const char *name, uid_t uid, int which)
{
	char *line;
	si_item_t *item;
	int fmt;
	FILE *f;
	si_list_t *all;
	struct stat sb;
	uint64_t sec, nsec;

	if ((which == SEL_NAME) && (name == NULL)) return NULL;

	all = NULL;
	f = NULL;
	fmt = 0;
	sec = 0;
	nsec = 0;

	if (geteuid() == 0)
	{
		f = fopen(_PATH_MASTERPASSWD, "r");
	}
	else
	{
		f = fopen(_PATH_PASSWD, "r");
		fmt = 1;
	}

	if (f == NULL) return NULL;

	memset(&sb, 0, sizeof(struct stat));
	if (fstat(fileno(f), &sb) == 0)
	{
		sec = sb.st_mtimespec.tv_sec;
		nsec = sb.st_mtimespec.tv_nsec;
	}

	forever
	{
		line = _fsi_get_line(f);
		if (line == NULL) break;

		if (line[0] == '#') 
		{
			free(line);
			line = NULL;
			continue;
		}

		item = _fsi_parse_user(si, name, uid, which, line, fmt, sec, nsec);
		free(line);
		line = NULL;

		if (item == NULL) continue;

		if (which == SEL_ALL)
		{
			all = si_list_add(all, item);
			si_item_release(item);
			continue;
		}

		fclose(f);
		return item;
	}
	fclose(f);
	return all;
}

/* GROUPS */

static si_item_t *
_fsi_parse_group(si_mod_t *si, const char *name, gid_t gid, int which, char *data, uint64_t sec, uint64_t nsec)
{
	char **tokens, **members;
	int ntokens, match;
	si_item_t *item;
	gid_t xgid;

	if (data == NULL) return NULL;

	ntokens = 0;
	tokens = _fsi_tokenize(data, ":", 1, &ntokens);
	if (ntokens != 4)
	{
		free(tokens);
		return NULL;
	}

	xgid = atoi(tokens[2]);
	match = 0;

	if (which == SEL_ALL) match = 1;
	else if ((which == SEL_NAME) && (string_equal(name, tokens[0]))) match = 1;
	else if ((which == SEL_NUMBER) && (gid == xgid)) match = 1;

	if (match == 0)
	{
		free(tokens);
		return NULL;
	}

	ntokens = 0;
	members = _fsi_tokenize(tokens[3], ",", 1, &ntokens);

	item = (si_item_t *)LI_ils_create("L4488ss4*", (unsigned long)si, CATEGORY_GROUP, 1, sec, nsec, tokens[0], tokens[1], xgid, members);

	free(tokens); 
	free(members);

	return item;
}

static void *
_fsi_get_group(si_mod_t *si, const char *name, gid_t gid, int which)
{
	char *line;
	si_item_t *item;
	FILE *f;
	si_list_t *all;
	struct stat sb;
	uint64_t sec, nsec;

	if ((which == SEL_NAME) && (name == NULL)) return NULL;

	all = NULL;
	f = NULL;
	sec = 0;
	nsec = 0;

	f = fopen(_PATH_GROUP, "r");
	if (f == NULL) return NULL;

	memset(&sb, 0, sizeof(struct stat));
	if (fstat(fileno(f), &sb) == 0)
	{
		sec = sb.st_mtimespec.tv_sec;
		nsec = sb.st_mtimespec.tv_nsec;
	}

	forever
	{
		line = _fsi_get_line(f);
		if (line == NULL) break;

		if (line[0] == '#') 
		{
			free(line);
			line = NULL;
			continue;
		}

		item = _fsi_parse_group(si, name, gid, which, line, sec, nsec);
		free(line);
		line = NULL;

		if (item == NULL) continue;

		if (which == SEL_ALL)
		{
			all = si_list_add(all, item);
			si_item_release(item);
			continue;
		}

		fclose(f);
		return item;
	}

	fclose(f);
	return all;
}

static void *
_fsi_get_grouplist(si_mod_t *si, const char *user)
{
	char **tokens, **members;
	int ntokens, i, match, gidcount;
	char *line;
	si_item_t *item;
	FILE *f;
	struct stat sb;
	uint64_t sec, nsec;
	int32_t gid, basegid, *gidp;
	char **gidlist;
	struct passwd *pw;

	if (user == NULL) return NULL;

	gidlist = NULL;
	gidcount = 0;
	f = NULL;
	sec = 0;
	nsec = 0;
	basegid = -1;

	item = si->sim_user_byname(si, user);
	if (item != NULL)
	{
		pw = (struct passwd *)((uintptr_t)item + sizeof(si_item_t));
		basegid = pw->pw_gid;
		free(item);
	}

	f = fopen(_PATH_GROUP, "r");
	if (f == NULL) return NULL;

	memset(&sb, 0, sizeof(struct stat));
	if (fstat(fileno(f), &sb) == 0)
	{
		sec = sb.st_mtimespec.tv_sec;
		nsec = sb.st_mtimespec.tv_nsec;
	}

	forever
	{
		line = _fsi_get_line(f);
		if (line == NULL) break;

		if (line[0] == '#') 
		{
			free(line);
			line = NULL;
			continue;
		}

		ntokens = 0;
		tokens = _fsi_tokenize(line, ":", 1, &ntokens);
		if (ntokens != 4)
		{
			free(tokens);
			continue;
		}

		ntokens = 0;
		members = _fsi_tokenize(tokens[3], ",", 1, &ntokens);

		match = 0;
		gid = -2;

		for (i = 0; (i < ntokens) && (match == 0); i++)
		{
			if (string_equal(user, members[i]))
			{
				gid = atoi(tokens[2]);
				match = 1;
			}
		}

		free(tokens); 
		free(members);
		free(line);
		line = NULL;

		if (match == 1)
		{
			if (gidcount == 0) gidlist = (char **)calloc(1, sizeof(char *));
			else gidlist = (char **)reallocf(gidlist, (gidcount + 1) * sizeof(char *));
			gidp = (int32_t *)calloc(1, sizeof(int32_t));

			if (gidlist == NULL)
			{
				gidcount = 0;
				break;
			}

			if (gidp == NULL)
			{
				for (i = 0; i < gidcount; i++) free(gidlist[i]);
				free(gidlist);
				gidcount = 0;
				break;
			}

			*gidp = gid;
			gidlist[gidcount++] = (char *)gidp;
		}
	}

	fclose(f);

	if (gidcount == 0) return NULL;

	gidlist = (char **)reallocf(gidlist, (gidcount + 1) * sizeof(int32_t *));
	if (gidlist == NULL) return NULL;
	gidlist[gidcount] = NULL;

	item = (si_item_t *)LI_ils_create("L4488s44a", (unsigned long)si, CATEGORY_GROUPLIST, 1, sec, nsec, user, basegid, gidcount, gidlist);

	for (i = 0; i <= gidcount; i++) free(gidlist[i]);
	free(gidlist);

	return item;
}

/* ALIASES */

static si_item_t *
_fsi_parse_alias(si_mod_t *si, const char *name, int which, char *data, uint64_t sec, uint64_t nsec)
{
	char **tokens, **members;
	int ntokens, match;
	si_item_t *item;

	if (data == NULL) return NULL;

	ntokens = 0;
	tokens = _fsi_tokenize(data, ":", 1, &ntokens);
	if (ntokens < 2)
	{
		free(tokens);
		return NULL;
	}

	match = 0;

	if (which == SEL_ALL) match = 1;
	else if (string_equal(name, tokens[0])) match = 1;

	if (match == 0)
	{
		free(tokens);
		return NULL;
	}

	ntokens = 0;
	members = _fsi_tokenize(tokens[3], ",", 1, &ntokens);

	item = (si_item_t *)LI_ils_create("L4488s4*4", (unsigned long)si, CATEGORY_ALIAS, 1, sec, nsec, tokens[0], ntokens, members, 1);

	free(tokens); 
	free(members);

	return item;
}

static void *
_fsi_get_alias(si_mod_t *si, const char *name, int which)
{
	char *line;
	si_item_t *item;
	FILE *f;
	si_list_t *all;
	struct stat sb;
	uint64_t sec, nsec;

	if ((which == SEL_NAME) && (name == NULL)) return NULL;

	all = NULL;
	f = NULL;
	sec = 0;
	nsec = 0;

	f = fopen(_PATH_ALIASES, "r");
	if (f == NULL) return NULL;

	memset(&sb, 0, sizeof(struct stat));
	if (fstat(fileno(f), &sb) == 0)
	{
		sec = sb.st_mtimespec.tv_sec;
		nsec = sb.st_mtimespec.tv_nsec;
	}

	forever
	{
		line = _fsi_get_line(f);
		if (line == NULL) break;

		if (line[0] == '#') 
		{
			free(line);
			line = NULL;
			continue;
		}

		item = _fsi_parse_alias(si, name, which, line, sec, nsec);
		free(line);
		line = NULL;

		if (item == NULL) continue;

		if (which == SEL_ALL)
		{
			all = si_list_add(all, item);
			si_item_release(item);
			continue;
		}

		fclose(f);
		return item;
	}

	fclose(f);
	return all;
}

/* ETHERS */

static si_item_t *
_fsi_parse_ether(si_mod_t *si, const char *name, int which, char *data, uint64_t sec, uint64_t nsec)
{
	char **tokens;
	char *cmac;
	int ntokens, match;
	si_item_t *item;

	if (data == NULL) return NULL;

	ntokens = 0;
	tokens = _fsi_tokenize(data, " \t", 1, &ntokens);
	if (ntokens != 2)
	{
		free(tokens);
		return NULL;
	}

	cmac = si_canonical_mac_address(tokens[1]);
	if (cmac == NULL)
	{
		free(tokens);
		return NULL;
	}

	match = 0;
	if (which == SEL_ALL) match = 1;
	else if ((which == SEL_NAME) && (string_equal(name, tokens[0]))) match = 1;
	else if ((which == SEL_NUMBER) && (string_equal(name, cmac))) match = 1;

	if (match == 0)
	{
		free(tokens);
		free(cmac);
		return NULL;
	}

	item = (si_item_t *)LI_ils_create("L4488ss", (unsigned long)si, CATEGORY_MAC, 1, sec, nsec, tokens[0], cmac);

	free(tokens); 
	free(cmac);

	return item;
}

static void *
_fsi_get_ether(si_mod_t *si, const char *name, int which)
{
	char *line, *cmac;
	si_item_t *item;
	FILE *f;
	si_list_t *all;
	struct stat sb;
	uint64_t sec, nsec;

	if ((which != SEL_ALL) && (name == NULL)) return NULL;

	cmac = NULL;
	if (which == SEL_NUMBER)
	{
		cmac = si_canonical_mac_address(name);
		if (cmac == NULL) return NULL;
	}

	all = NULL;
	f = NULL;
	sec = 0;
	nsec = 0;

	f = fopen(_PATH_ETHERS, "r");
	if (f == NULL) return NULL;

	memset(&sb, 0, sizeof(struct stat));
	if (fstat(fileno(f), &sb) == 0)
	{
		sec = sb.st_mtimespec.tv_sec;
		nsec = sb.st_mtimespec.tv_nsec;
	}

	forever
	{
		line = _fsi_get_line(f);
		if (line == NULL) break;

		if (line[0] == '#') 
		{
			free(line);
			line = NULL;
			continue;
		}

		item = NULL;
		if (which == SEL_NUMBER) item = _fsi_parse_ether(si, cmac, which, line, sec, nsec);
		else item = _fsi_parse_ether(si, name, which, line, sec, nsec);

		free(line);
		line = NULL;

		if (item == NULL) continue;

		if (which == SEL_ALL)
		{
			all = si_list_add(all, item);
			si_item_release(item);
			continue;
		}

		fclose(f);
		return item;
	}

	fclose(f);
	return all;
}

/* HOSTS */

static si_item_t *
_fsi_parse_host(si_mod_t *si, const char *name, const void *addr, int af, int which, char *data, uint64_t sec, uint64_t nsec)
{
	char **tokens, **h_aliases, *null_alias;
	int i, ntokens, match, xaf, h_length;
	struct in_addr a4;
	struct in6_addr a6;
	si_item_t *item;
	char *h_addr_list[2];
	char h_addr_4[4], h_addr_6[16];

	if (data == NULL) return NULL;

	null_alias = NULL;

	ntokens = 0;
	tokens = _fsi_tokenize(data, " 	", 0, &ntokens);
	if (ntokens < 2)
	{
		free(tokens);
		return NULL;
	}

	h_addr_list[1] = NULL;

	xaf = AF_UNSPEC;
	if (inet_pton(AF_INET, tokens[0], &a4) == 1)
	{
		xaf = AF_INET;
		h_length = sizeof(struct in_addr);
		memcpy(h_addr_4, &a4, 4);
		h_addr_list[0] = h_addr_4;
	}
	else if (inet_pton(AF_INET6, tokens[0], &a6) == 1)
	{
		xaf = AF_INET6;
		h_length = sizeof(struct in6_addr);
		memcpy(h_addr_6, &a6, 16);
		h_addr_list[0] = h_addr_6;
	}

	if (xaf == AF_UNSPEC)
	{
		free(tokens);
		return NULL;
	}

	h_aliases = NULL;
	if (ntokens > 2) h_aliases = &(tokens[2]);

	match = 0;

	if (which == SEL_ALL) match = 1;
	else
	{
		if (af == xaf)
		{
			if (which == SEL_NAME)
			{
				if (string_equal(name, tokens[1])) match = 1;
				else if (h_aliases != NULL)
				{
					for (i = 0; (h_aliases[i] != NULL) && (match == 0); i++)
						if (string_equal(name, h_aliases[i])) match = 1;
				}
			}
			else if (which == SEL_NUMBER)
			{
				if (memcmp(addr, h_addr_list[0], h_length) == 0) match = 1;
			}
		}
	}

	if (match == 0)
	{
		free(tokens);
		return NULL;
	}

	item = NULL;

	if (h_aliases == NULL) h_aliases = &null_alias;

	if (af == AF_INET)
	{
		item = (si_item_t *)LI_ils_create("L4488s*44a", (unsigned long)si, CATEGORY_HOST_IPV4, 1, sec, nsec, tokens[1], h_aliases, af, h_length, h_addr_list);
	}
	else
	{
		item = (si_item_t *)LI_ils_create("L4488s*44c", (unsigned long)si, CATEGORY_HOST_IPV6, 1, sec, nsec, tokens[1], h_aliases, af, h_length, h_addr_list);
	}

	free(tokens);

	return item;
}

static void *
_fsi_get_host(si_mod_t *si, const char *name, const void *addr, int af, int which, uint32_t *err)
{
	char *line;
	si_item_t *item;
	FILE *f;
	si_list_t *all;
	struct stat sb;
	uint64_t sec, nsec;

	sec = 0;
	nsec = 0;

	if ((which == SEL_NAME) && (name == NULL))
	{
		if (err != NULL) *err = NO_RECOVERY;
		return NULL;
	}

	if ((which == SEL_NUMBER) && (addr == NULL))
	{
		if (err != NULL) *err = NO_RECOVERY;
		return NULL;
	}

	f = fopen(_PATH_HOSTS, "r");
	if (f == NULL)
	{
		if (err != NULL) *err = NO_RECOVERY;
		return NULL;
	}

	memset(&sb, 0, sizeof(struct stat));
	if (fstat(fileno(f), &sb) == 0)
	{
		sec = sb.st_mtimespec.tv_sec;
		nsec = sb.st_mtimespec.tv_nsec;
	}

	all = NULL;

	forever
	{
		line = _fsi_get_line(f);
		if (line == NULL) break;

		if (line[0] == '#') 
		{
			free(line);
			line = NULL;
			continue;
		}

		item = _fsi_parse_host(si, name, addr, af, which, line, sec, nsec);
		free(line);
		line = NULL;

		if (item == NULL) continue;

		if (which == SEL_ALL)
		{
			all = si_list_add(all, item);
			si_item_release(item);
			continue;
		}

		fclose(f);
		return item;
	}

	fclose(f);
	return all;
}

/* SERVICE */

static si_item_t *
_fsi_parse_service(si_mod_t *si, const char *name, const char *proto, int port, int which, char *data, uint64_t sec, uint64_t nsec)
{
	char **tokens, **s_aliases, *xproto;
	int i, ntokens, match;
	si_item_t *item;
	int xport;

	if (data == NULL) return NULL;

	port = ntohs(port);

	ntokens = 0;
	tokens = _fsi_tokenize(data, " 	", 0, &ntokens);
	if (ntokens < 2)
	{
		free(tokens);
		return NULL;
	}

	s_aliases = NULL;
	if (ntokens > 2) s_aliases = &(tokens[2]);

	xport = atoi(tokens[1]);

	xproto = strchr(tokens[1], '/');

	if (xproto == NULL)
	{
		free(tokens);
		return NULL;
	}

	*xproto++ = '\0';
	if ((proto != NULL) && (string_not_equal(proto, xproto)))
	{
		free(tokens);
		return NULL;
	}

	match = 0;
	if (which == SEL_ALL) match = 1;
	else if (which == SEL_NAME)
	{
		if (string_equal(name, tokens[0])) match = 1;
		else if (s_aliases != NULL)
		{
			for (i = 0; (s_aliases[i] != NULL) && (match == 0); i++)
				if (string_equal(name, s_aliases[i])) match = 1;
		}
	}
	else if ((which == SEL_NUMBER) && (port == xport)) match = 1;

	if (match == 0)
	{
		free(tokens);
		return NULL;
	}

	/* strange but correct */
	xport = htons(xport);

	item = (si_item_t *)LI_ils_create("L4488s*4s", (unsigned long)si, CATEGORY_SERVICE, 1, sec, nsec, tokens[0], s_aliases, xport, xproto);

	free(tokens);

	return item;
}

static void *
_fsi_get_service(si_mod_t *si, const char *name, const char *proto, int port, int which)
{
	char *p, *line;
	si_item_t *item;
	FILE *f;
	si_list_t *all;
	struct stat sb;
	uint64_t sec, nsec;

	sec = 0;
	nsec = 0;

	if ((which == SEL_NAME) && (name == NULL)) return NULL;
	if ((which == SEL_NUMBER) && (port == 0)) return NULL;

	f = fopen(_PATH_SERVICES, "r");
	if (f == NULL) return NULL;

	memset(&sb, 0, sizeof(struct stat));
	if (fstat(fileno(f), &sb) == 0)
	{
		sec = sb.st_mtimespec.tv_sec;
		nsec = sb.st_mtimespec.tv_nsec;
	}

	all = NULL;

	forever
	{
		line = _fsi_get_line(f);
		if (line == NULL) break;

		if (line[0] == '#') 
		{
			free(line);
			line = NULL;
			continue;
		}

		p = strchr(line, '#');
		if (p != NULL) *p = '\0';

		item = _fsi_parse_service(si, name, proto, port, which, line, sec, nsec);
		free(line);
		line = NULL;

		if (item == NULL) continue;

		if (which == SEL_ALL)
		{
			all = si_list_add(all, item);
			si_item_release(item);
			continue;
		}

		fclose(f);
		return item;
	}

	fclose(f);
	return all;
}

/*
 * Generic name/number/aliases lookup
 * Works for protocols, networks, and rpcs
 */

static si_item_t *
_fsi_parse_name_num_aliases(si_mod_t *si, const char *name, int num, int which, char *data, uint64_t sec, uint64_t nsec, int cat)
{
	char **tokens, **aliases;
	int i, ntokens, match, xnum;
	si_item_t *item;

	if (data == NULL) return NULL;

	ntokens = 0;
	tokens = _fsi_tokenize(data, " 	", 0, &ntokens);
	if (ntokens < 2)
	{
		free(tokens);
		return NULL;
	}

	xnum = atoi(tokens[1]);

	aliases = NULL;
	if (ntokens > 2) aliases = &(tokens[2]);

	match = 0;

	if (which == SEL_ALL) match = 1;
	else if (which == SEL_NAME)
	{
		if (string_equal(name, tokens[0])) match = 1;
		else if (aliases != NULL)
		{
			for (i = 0; (aliases[i] != NULL) && (match == 0); i++)
				if (string_equal(name, aliases[i])) match = 1;
		}
	}
	else if ((which == SEL_NUMBER) && (num == xnum)) match = 1;

	if (match == 0)
	{
		free(tokens);
		return NULL;
	}

	item = (si_item_t *)LI_ils_create("L4488s*4", (unsigned long)si, cat, 1, sec, nsec, tokens[0], aliases, xnum);

	free(tokens);

	return item;
}

static void *
_fsi_get_name_number_aliases(si_mod_t *si, const char *name, int num, int which, int cat, const char *path)
{
	char *p, *line;
	si_item_t *item;
	FILE *f;
	si_list_t *all;
	struct stat sb;
	uint64_t sec, nsec;

	sec = 0;
	nsec = 0;

	f = fopen(path, "r");
	if (f == NULL) return NULL;

	memset(&sb, 0, sizeof(struct stat));
	if (fstat(fileno(f), &sb) == 0)
	{
		sec = sb.st_mtimespec.tv_sec;
		nsec = sb.st_mtimespec.tv_nsec;
	}

	all = NULL;

	forever
	{
		line = _fsi_get_line(f);
		if (line == NULL) break;

		if (line[0] == '#') 
		{
			free(line);
			line = NULL;
			continue;
		}

		p = strchr(line, '#');
		if (p != NULL) *p = '\0';

		item = _fsi_parse_name_num_aliases(si, name, num, which, line, sec, nsec, cat);
		free(line);
		line = NULL;

		if (item == NULL) continue;

		if (which == SEL_ALL)
		{
			all = si_list_add(all, item);
			si_item_release(item);
			continue;
		}

		fclose(f);
		return item;
	}

	fclose(f);
	return all;
}

/* MOUNT */

static si_item_t *
_fsi_parse_fs(si_mod_t *si, const char *name, int which, char *data, uint64_t sec, uint64_t nsec)
{
	char **tokens, *tmp, **opts, *fstype;
	int ntokens, match, i, freq, passno;
	si_item_t *item;

	if (data == NULL) return NULL;

	freq = 0;
	passno = 0;
	fstype = NULL;

	ntokens = 0;
	tokens = _fsi_tokenize(data, " 	", 0, &ntokens);
	if ((ntokens < 4) || (ntokens > 6))
	{
		free(tokens);
		return NULL;
	}

	if (ntokens >= 5) freq = atoi(tokens[4]);
	if (ntokens == 6) passno = atoi(tokens[5]);

	tmp = strdup(tokens[3]);
	if (tmp == NULL)
	{
		free(tokens);
		return NULL;
	}

	ntokens = 0;
	opts = _fsi_tokenize(tmp, ",", 0, &ntokens);

	if (opts == NULL)
	{
		free(tokens); 
		free(tmp);
		return NULL;
	}

	for (i = 0; i < ntokens; i++)
	{
		if ((string_equal(opts[i], "rw")) || (string_equal(opts[i], "ro")) || (string_equal(opts[i], "sw")) || (string_equal(opts[i], "xx")))
		{
			fstype = opts[i];
			break;
		}
	}

	match = 0;

	if (which == SEL_ALL) match = 1;
	else if ((which == SEL_NAME) && (string_equal(name, tokens[0]))) match = 1;
	else if ((which == SEL_NUMBER) && (string_equal(name, tokens[1]))) match = 1;

	if (match == 0)
	{
		free(tokens);
		return NULL;
	}

	item = (si_item_t *)LI_ils_create("L4488sssss44", (unsigned long)si, CATEGORY_FS, 1, sec, nsec, tokens[0], tokens[1], tokens[2], tokens[3], (fstype == NULL) ? "rw" : fstype, freq, passno);

	free(tokens); 
	free(opts); 
	free(tmp);

	return item;
}

static char *
_fsi_get_device_path(dev_t target_dev)
{
	char *result;
    char dev[PATH_MAX];
    char *name;
	char namebuf[PATH_MAX];

	result = NULL;

    strlcpy(dev, _PATH_DEV, sizeof(dev));

    /* The root device in fstab should always be a block special device */
    name = devname_r(target_dev, S_IFBLK, namebuf, sizeof(namebuf));
    if (name == NULL)
	{
		DIR *dirp;
		struct stat devst;
		struct dirent *ent, entbuf;

       /* No _PATH_DEVDB. We have to search for it the slow way */
        dirp = opendir(_PATH_DEV);
        if (dirp == NULL) return NULL;

        while (readdir_r(dirp, &entbuf, &ent) == 0 && ent != NULL)
		{
            /* Look for a block special device */
            if (ent->d_type == DT_BLK)
			{
                strlcat(dev, ent->d_name, sizeof(dev));
                if (stat(dev, &devst) == 0)
				{
                    if (devst.st_rdev == target_dev) {
						result = strdup(dev);
						break;
					}
                }
            }

            /* reset dev to _PATH_DEV and try again */
            dev[sizeof(_PATH_DEV) - 1] = '\0';
        }
		
		if (dirp) closedir(dirp);
    }
	else
	{
        /* We found the _PATH_DEVDB entry */
		strlcat(dev, name, sizeof(dev));
		result = strdup(dev);
	}

    return result;
}

static si_item_t *
_fsi_fs_root(si_mod_t *si)
{
	struct stat rootstat;
	struct statfs rootfsinfo;
	char *root_spec;
	const char *root_path;

	if (rootfs != NULL) return si_item_retain(rootfs);

	root_path = "/";

	if (stat(root_path, &rootstat) < 0) return NULL;
 	if (statfs(root_path, &rootfsinfo) < 0) return NULL;

	/* Check to make sure we're not looking at a synthetic root: */
	if (string_equal(rootfsinfo.f_fstypename, "synthfs"))
	{
		root_path = "/root";
        if (stat(root_path, &rootstat) < 0) return NULL;
		if (statfs(root_path, &rootfsinfo) < 0) return NULL;
	}

	root_spec = _fsi_get_device_path(rootstat.st_dev);

	rootfs = (si_item_t *)LI_ils_create("L4488sssss44", (unsigned long)si, CATEGORY_FS, 1, 0LL, 0LL, root_spec, root_path, rootfsinfo.f_fstypename, FSTAB_RW, FSTAB_RW, 0, 1);
	return rootfs;
}


static void *
_fsi_get_fs(si_mod_t *si, const char *name, int which)
{
	char *line;
	si_item_t *item;
	FILE *f;
	si_list_t *all;
	struct stat sb;
	uint64_t sec, nsec;
	int synthesize_root;
	struct fstab *rfs;

	if ((which != SEL_ALL) && (name == NULL)) return NULL;

	all = NULL;
	f = NULL;
	sec = 0;
	nsec = 0;
#ifdef SYNTH_ROOTFS
	synthesize_root = 1;
#else
	synthesize_root = 0;
#endif

	f = fopen(_PATH_FSTAB, "r");
	if ((f == NULL) || (synthesize_root == 1))
	{
		item = _fsi_fs_root(si);

		rfs = NULL;
		if (item != NULL) rfs = (struct fstab *)((uintptr_t)item + sizeof(si_item_t));

		switch (which)
		{
			case SEL_NAME:
			{
				if ((rfs != NULL) && (string_equal(name, rfs->fs_spec)))
				{
					if (f != NULL) fclose(f);
					return item;
				}

				break;
			}

			case SEL_NUMBER:
			{
				if ((rfs != NULL) && (string_equal(name, rfs->fs_file)))
				{
					if (f != NULL) fclose(f);
					return item;
				}

				break;
			}

			case SEL_ALL:
			{
				all = si_list_add(all, item);
				si_item_release(item);
				break;
			}
		}
	}

	if (f == NULL) return all;

	memset(&sb, 0, sizeof(struct stat));
	if (fstat(fileno(f), &sb) == 0)
	{
		sec = sb.st_mtimespec.tv_sec;
		nsec = sb.st_mtimespec.tv_nsec;
	}

	forever
	{
		line = _fsi_get_line(f);
		if (line == NULL) break;

		if (line[0] == '#') 
		{
			free(line);
			line = NULL;
			continue;
		}

		item = _fsi_parse_fs(si, name, which, line, sec, nsec);
		free(line);
		line = NULL;

		if (item == NULL) continue;

		if (which == SEL_ALL)
		{
			all = si_list_add(all, item);
			si_item_release(item);
			continue;
		}

		fclose(f);
		return item;
	}

	fclose(f);
	return all;
}

__private_extern__ int
file_is_valid(si_mod_t *si, si_item_t *item)
{
	struct stat sb;
	uint64_t sec, nsec;
	const char *path;
	si_mod_t *src;

	if (si == NULL) return 0;
	if (item == NULL) return 0;
	if (si->name == NULL) return 0;
	if (item->src == NULL) return 0;

	src = (si_mod_t *)item->src;

	if (src->name == NULL) return 0;
	if (string_not_equal(si->name, src->name)) return 0;

	if (item == rootfs) return 1;

	path = NULL;
	memset(&sb, 0, sizeof(struct stat));
	sec = item->validation_a;
	nsec = item->validation_b;

	if (item->type == CATEGORY_USER)
	{
		if (geteuid() == 0) path = _PATH_MASTERPASSWD;
		else path = _PATH_PASSWD;
	}
	else if (item->type == CATEGORY_GROUP) path = _PATH_GROUP;
	else if (item->type == CATEGORY_HOST_IPV4) path = _PATH_HOSTS;
	else if (item->type == CATEGORY_HOST_IPV6) path = _PATH_HOSTS;
	else if (item->type == CATEGORY_NETWORK) path = _PATH_NETWORKS;
	else if (item->type == CATEGORY_SERVICE) path = _PATH_SERVICES;
	else if (item->type == CATEGORY_PROTOCOL) path = _PATH_PROTOCOLS;
	else if (item->type == CATEGORY_RPC) path = _PATH_RPCS;
	else if (item->type == CATEGORY_FS) path = _PATH_FSTAB;

	if (path == NULL) return 0;
	if (stat(path, &sb) != 0) return 0;
	if (sec != sb.st_mtimespec.tv_sec) return 0;
	if (nsec != sb.st_mtimespec.tv_nsec) return 0;

	return 1;
}

__private_extern__ si_item_t *
file_user_byname(si_mod_t *si, const char *name)
{
	return _fsi_get_user(si, name, 0, SEL_NAME);
}

__private_extern__ si_item_t *
file_user_byuid(si_mod_t *si, uid_t uid)
{
	return _fsi_get_user(si, NULL, uid, SEL_NUMBER);
}

__private_extern__ si_list_t *
file_user_all(si_mod_t *si)
{
	return _fsi_get_user(si, NULL, 0, SEL_ALL);
}

__private_extern__ si_item_t *
file_group_byname(si_mod_t *si, const char *name)
{
	return _fsi_get_group(si, name, 0, SEL_NAME);
}

__private_extern__ si_item_t *
file_group_bygid(si_mod_t *si, gid_t gid)
{
	return _fsi_get_group(si, NULL, gid, SEL_NUMBER);
}

__private_extern__ si_list_t *
file_group_all(si_mod_t *si)
{
	return _fsi_get_group(si, NULL, 0, SEL_ALL);
}

__private_extern__ si_item_t *
file_grouplist(si_mod_t *si, const char *name)
{
	return _fsi_get_grouplist(si, name);
}

__private_extern__ si_item_t *
file_host_byname(si_mod_t *si, const char *name, int af, const char *ignored, uint32_t *err)
{
	si_item_t *item;

	if (err != NULL) *err = SI_STATUS_NO_ERROR;

	item = _fsi_get_host(si, name, NULL, af, SEL_NAME, err);
	if ((item == NULL) && (err != NULL) && (*err == 0)) *err = SI_STATUS_H_ERRNO_HOST_NOT_FOUND;

	return item;
}

__private_extern__ si_item_t *
file_host_byaddr(si_mod_t *si, const void *addr, int af, const char *ignored, uint32_t *err)
{
	si_item_t *item;

	if (err != NULL) *err = SI_STATUS_NO_ERROR;

	item = _fsi_get_host(si, NULL, addr, af, SEL_NUMBER, err);
	if ((item == NULL) && (err != NULL) && (*err == 0)) *err = SI_STATUS_H_ERRNO_HOST_NOT_FOUND;

	return item;
}

__private_extern__ si_list_t *
file_host_all(si_mod_t *si)
{
	return _fsi_get_host(si, NULL, NULL, 0, SEL_ALL, NULL);
}

__private_extern__ si_item_t *
file_network_byname(si_mod_t *si, const char *name)
{
	if (name == NULL) return NULL;
	return _fsi_get_name_number_aliases(si, name, 0, SEL_NAME, CATEGORY_NETWORK, _PATH_NETWORKS);
}

__private_extern__ si_item_t *
file_network_byaddr(si_mod_t *si, uint32_t addr)
{
	return _fsi_get_name_number_aliases(si, NULL, (int)addr, SEL_NUMBER, CATEGORY_NETWORK, _PATH_NETWORKS);
}

__private_extern__ si_list_t *
file_network_all(si_mod_t *si)
{
	return _fsi_get_name_number_aliases(si, NULL, 0, SEL_ALL, CATEGORY_NETWORK, _PATH_NETWORKS);
}

__private_extern__ si_item_t *
file_service_byname(si_mod_t *si, const char *name, const char *proto)
{
	return _fsi_get_service(si, name, proto, 0, SEL_NAME);
}

__private_extern__ si_item_t *
file_service_byport(si_mod_t *si, int port, const char *proto)
{
	return _fsi_get_service(si, NULL, proto, port, SEL_NUMBER);
}

__private_extern__ si_list_t *
file_service_all(si_mod_t *si)
{
	return _fsi_get_service(si, NULL, NULL, 0, SEL_ALL);
}

__private_extern__ si_item_t *
file_protocol_byname(si_mod_t *si, const char *name)
{
	if (name == NULL) return NULL;
	return _fsi_get_name_number_aliases(si, name, 0, SEL_NAME, CATEGORY_PROTOCOL, _PATH_PROTOCOLS);
}

__private_extern__ si_item_t *
file_protocol_bynumber(si_mod_t *si, int number)
{
	return _fsi_get_name_number_aliases(si, NULL, number, SEL_NUMBER, CATEGORY_PROTOCOL, _PATH_PROTOCOLS);
}

__private_extern__ si_list_t *
file_protocol_all(si_mod_t *si)
{
	return _fsi_get_name_number_aliases(si, NULL, 0, SEL_ALL, CATEGORY_PROTOCOL, _PATH_PROTOCOLS);
}

__private_extern__ si_item_t *
file_rpc_byname(si_mod_t *si, const char *name)
{
	if (name == NULL) return NULL;
	return _fsi_get_name_number_aliases(si, name, 0, SEL_NAME, CATEGORY_RPC, _PATH_RPCS);
}

__private_extern__ si_item_t *
file_rpc_bynumber(si_mod_t *si, int number)
{
	return _fsi_get_name_number_aliases(si, NULL, number, SEL_NUMBER, CATEGORY_RPC, _PATH_RPCS);
}

__private_extern__ si_list_t *
file_rpc_all(si_mod_t *si)
{
	return _fsi_get_name_number_aliases(si, NULL, 0, SEL_ALL, CATEGORY_RPC, _PATH_RPCS);
}

__private_extern__ si_item_t *
file_fs_byspec(si_mod_t *si, const char *spec)
{
	return _fsi_get_fs(si, spec, SEL_NAME);
}

__private_extern__ si_item_t *
file_fs_byfile(si_mod_t *si, const char *file)
{
	return _fsi_get_fs(si, file, SEL_NUMBER);
}

__private_extern__ si_list_t *
file_fs_all(si_mod_t *si)
{
	return _fsi_get_fs(si, NULL, SEL_ALL);
}

__private_extern__ si_item_t *
file_alias_byname(si_mod_t *si, const char *name)
{
	return _fsi_get_alias(si, name, SEL_NAME);
}

__private_extern__ si_list_t *
file_alias_all(si_mod_t *si)
{
	return _fsi_get_alias(si, NULL, SEL_ALL);
}

__private_extern__ si_item_t *
file_mac_byname(si_mod_t *si, const char *name)
{
	return _fsi_get_ether(si, name, SEL_NAME);
}

__private_extern__ si_item_t *
file_mac_bymac(si_mod_t *si, const char *mac)
{
	return _fsi_get_ether(si, mac, SEL_NUMBER);
}

__private_extern__ si_list_t *
file_mac_all(si_mod_t *si)
{
	return _fsi_get_ether(si, NULL, SEL_ALL);
}

static si_list_t *
file_addrinfo(si_mod_t *si, const void *node, const void *serv, uint32_t family, uint32_t socktype, uint32_t proto, uint32_t flags, const char *interface, uint32_t *err)
{
	if (err != NULL) *err = SI_STATUS_NO_ERROR;
	return _gai_simple(si, node, serv, family, socktype, proto, flags, interface, err);
}

__private_extern__  si_mod_t *
si_module_static_file()
{
	si_mod_t *out;
	char *outname;

	out = (si_mod_t *)calloc(1, sizeof(si_mod_t));
	outname = strdup("file");

	if ((out == NULL) || (outname == NULL))
	{
		if (out != NULL) free(out);
		if (outname != NULL) free(outname);

		errno = ENOMEM;
		return NULL;
	}

	out->name = outname;
	out->vers = 1;
	out->refcount = 1;

	out->sim_is_valid = file_is_valid;

	out->sim_user_byname = file_user_byname;
	out->sim_user_byuid = file_user_byuid;
	out->sim_user_all = file_user_all;

	out->sim_group_byname = file_group_byname;
	out->sim_group_bygid = file_group_bygid;
	out->sim_group_all = file_group_all;

	out->sim_grouplist = file_grouplist;

	/* NETGROUP SUPPORT NOT IMPLEMENTED */
	out->sim_netgroup_byname = NULL;
	out->sim_in_netgroup = NULL;

	out->sim_alias_byname = file_alias_byname;
	out->sim_alias_all = file_alias_all;

	out->sim_host_byname = file_host_byname;
	out->sim_host_byaddr = file_host_byaddr;
	out->sim_host_all = file_host_all;

	out->sim_network_byname = file_network_byname;
	out->sim_network_byaddr = file_network_byaddr;
	out->sim_network_all = file_network_all;

	out->sim_service_byname = file_service_byname;
	out->sim_service_byport = file_service_byport;
	out->sim_service_all = file_service_all;

	out->sim_protocol_byname = file_protocol_byname;
	out->sim_protocol_bynumber = file_protocol_bynumber;
	out->sim_protocol_all = file_protocol_all;

	out->sim_rpc_byname = file_rpc_byname;
	out->sim_rpc_bynumber = file_rpc_bynumber;
	out->sim_rpc_all = file_rpc_all;

	out->sim_fs_byspec = file_fs_byspec;
	out->sim_fs_byfile = file_fs_byfile;
	out->sim_fs_all = file_fs_all;

	out->sim_mac_byname = file_mac_byname;
	out->sim_mac_bymac = file_mac_bymac;
	out->sim_mac_all = file_mac_all;

	out->sim_wants_addrinfo = NULL;
	out->sim_addrinfo = file_addrinfo;

	/* no nameinfo support */
	out->sim_nameinfo = NULL;

	return out;
}
