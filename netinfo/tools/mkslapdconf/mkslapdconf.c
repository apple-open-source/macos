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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinfo/ni.h>
#include <netdb.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/dir.h>

#include <NetInfo/system.h>
#include <NetInfo/dsx500dit.h>
#include <NetInfo/network.h>

static const char LDAP_SYSCONFDIR[] = "/etc/openldap";

#ifdef _OS_VERSION_MACOS_X_
static const char LOCALSTATEDIR[] = "/var/run";
static const char NETINFO_DIR[] = "/var/db/netinfo";
#else
#ifdef _OS_VERSION_DARWIN_
static const char LOCALSTATEDIR[] = "/var/run";
static const char NETINFO_DIR[] = "/var/db/netinfo";
#else
static const char LOCALSTATEDIR[] = "/etc";
static const char NETINFO_DIR[] = "/etc/netinfo";
#endif
#endif

static dsdata NAME_NAME = { DataTypeCStr, sizeof("name"), "name", 1 };
static dsdata NAME_MACHINES = { DataTypeCStr, sizeof("machines"), "machines", 1 };
static dsdata NAME_IP_ADDRESS = { DataTypeCStr, sizeof("ip_address"), "ip_address", 1 };
static dsdata NAME_MASTER = { DataTypeCStr, sizeof("master"), "master", 1 };
static dsdata NAME_LOCAL = { DataTypeCStr, sizeof("local"), "local", 1 };
static dsdata NAME_DC_LOCAL = { DataTypeCaseCStr, sizeof("dc=local"), "dc=local", 1 };

typedef struct {
	dsattribute *parent;
	dsdata *suffix;
	dsdata *tag;
	u_int32_t master;
} domaininfo;

static char *suffixes[] = {
	".nidb",
	".move",
	".temp",
	NULL
};

/* from nibindd */
dsstatus isnidir(char *dir, dsdata **tag)
{
	char *s, *p;
	int i;

	s = strchr(dir, '.');
	if (s == NULL)
		return DSStatusFailed;

	for (i = 0; suffixes[i] != NULL; i++)
	{
		if (strcmp(s, suffixes[i]) == 0)
		{
			*tag = cstring_to_dsdata(dir);
			assert(*tag != NULL);
			p = (*tag)->data;
			s = strchr(p, '.');
			assert(s != NULL);
			(*tag)->length = (s - p) + 1;
			*s = 0;
			return DSStatusOK;
		}
	}

	return DSStatusFailed;
}

void *nibind_new(struct in_addr *addr)
{
	struct sockaddr_in sin;
	int sock = RPC_ANYSOCK;

	sin.sin_port = 0;
	sin.sin_family = AF_INET;
	bzero(sin.sin_zero, sizeof(sin.sin_zero));
	sin.sin_addr = *addr;
	return ((void *)clnttcp_create(&sin, NIBIND_PROG, NIBIND_VERS, 
				       &sock, 0, 0));
}

void nibind_free(void *ni)
{
	return clnt_destroy((CLIENT *)ni);
}

void freeinfo(domaininfo *info)
{
	if (info->suffix != NULL)
		dsdata_release(info->suffix);

	if (info->tag != NULL)
		dsdata_release(info->tag);

	if (info->parent != NULL)
		dsattribute_release(info->parent);

	free(info);
}

int infocmp(const void *_info1, const void *_info2)
{
	domaininfo *info1, *info2;
	int len1, len2;

	info1 = *(domaininfo **)_info1;
	info2 = *(domaininfo **)_info2;

	len2 = info2->suffix->length - 1;
	len1 = info1->suffix->length - 1;

	if (len2 > len1)
	{
		return len2 - len1;
	}

	return strcasecmp(dsdata_to_cstring(info1->suffix) + len1 - len2, dsdata_to_cstring(info2->suffix));
}

dsstatus islocaladdress(dsengine *s, dsdata *name)
{
	dsstatus status;
	u_int32_t dsid;
	dsrecord *host;
	dsattribute *addresses;
	dsdata *d;
	struct in_addr address;
	char *p;

	status = dsengine_match(s, 0, &NAME_NAME, &NAME_MACHINES, &dsid);
	if (status != DSStatusOK)
		return status;

	status = dsengine_match(s, dsid, &NAME_NAME, name, &dsid);
	if (status != DSStatusOK)
		return status;

	status = dsengine_fetch(s, dsid, &host);
	if (status != DSStatusOK)
		return status;

	addresses = dsrecord_attribute(host, &NAME_IP_ADDRESS, SELECT_ATTRIBUTE);
	if (addresses == NULL)
	{
		dsrecord_release(host);
		return DSStatusInvalidKey;
	}

	dsrecord_release(host);

	d = dsattribute_value(addresses, 0);
	if (d == NULL)
	{
		dsattribute_release(addresses);
		return DSStatusInvalidKey;
	}

	dsattribute_release(addresses);

	p = dsdata_to_cstring(d);
	if (p == NULL)
	{
		dsdata_release(d);
		return DSStatusInvalidKey;
	}

	address.s_addr = inet_addr(p);
	status = (sys_is_my_address(&address)) ? DSStatusOK : DSStatusFailed;

	dsdata_release(d);

	return status;
}

dsstatus getmaster(dsengine *s, dsdata **master, dsdata **mastertag)
{
	dsstatus status;
	dsrecord *root;
	dsattribute *a;
	dsdata *d;
	char *v, *p;

	status = dsengine_fetch(s, 0, &root);
	if (status != DSStatusOK)
		return status;

	a = dsrecord_attribute(root, &NAME_MASTER, SELECT_ATTRIBUTE);
	if (a == NULL)
	{
		dsrecord_release(root);
		return DSStatusInvalidKey;
	}

	dsrecord_release(root);

	d = dsattribute_value(a, 0);
	if (d == NULL)
	{
		dsattribute_release(a);
		return DSStatusInvalidKey;
	}

	dsattribute_release(a);

	p = dsdata_to_cstring(d);
	if (p == NULL)
	{
		dsdata_release(d);
		return DSStatusInvalidKey;
	}
	
	v = strdup(p);
	assert(v != NULL);

	dsdata_release(d);

	p = strchr(v, '/');
	if (p == NULL)
	{
		free(v);
		return DSStatusInvalidKey;
	}
	
	*p = '\0';
	p++;

	*master = cstring_to_dsdata(v);
	assert(*master != NULL);

	*mastertag = cstring_to_dsdata(p);
	assert(*mastertag != NULL);

	free(v);

	return DSStatusOK;
}

domaininfo *getservinfo(dsdata *tag, u_int32_t remote)
{
	dsengine *s;
	char *name;
	u_int32_t flags;
	dsx500dit *dit;
	dsstatus status;
	domaininfo *info;
	u_int32_t size;
	dsdata *master, *mastertag;

	info = (domaininfo *)malloc(sizeof(*info));
	assert(info != NULL);

	info->suffix = NULL;
	info->tag = NULL;
	info->parent = NULL;
	info->master = 0;

	size = tag->length - 1;
	if (remote)
		size += sizeof("localhost/");
	else
		size += strlen(NETINFO_DIR) + sizeof("/.nidb");

	name = malloc(size);
	assert(name != NULL);

	if (remote)
		snprintf(name, size, "localhost/%s", dsdata_to_cstring(tag));
	else
		snprintf(name, size, "%s/%s.nidb", NETINFO_DIR, dsdata_to_cstring(tag));

	flags = DSSTORE_FLAGS_ACCESS_READONLY;
	if (remote)
		flags |= DSSTORE_FLAGS_REMOTE_NETINFO | DSSTORE_FLAGS_OPEN_BY_TAG;

	status = dsengine_open(&s, name, flags);
	if (status != DSStatusOK)
	{
		freeinfo(info);
		fprintf(stderr, "mkslapdconf: %s: %s\n", name, dsstatus_message(status));
		free(name);
		return NULL;
	}
	free(name);

	dit = dsx500dit_new(s);
	if (dit == NULL)
	{
		dsengine_close(s);
		freeinfo(info);
		fprintf(stderr, "mkslapdconf: could not retrieve DIT from NetInfo\n");
		return NULL;
	}

	if (!IsStringDataType(dit->local_suffix->type))
	{
		dsx500dit_release(dit);
		dsengine_close(s);
		freeinfo(info);
		fprintf(stderr, "mkslapdconf: could not retrieve naming context from NetInfo\n");
		return NULL;
	}

	info->suffix = dsdata_retain(dit->local_suffix);
	assert(info->suffix != NULL);

	info->parent = dsattribute_retain(dit->parent_referrals);

	dsx500dit_release(dit);

	info->tag = dsdata_retain(tag);

	/* Now find out whether it is the master */
	status = getmaster(s, &master, &mastertag);
	if (status != DSStatusOK)
	{
		dsengine_close(s);
		freeinfo(info);
		fprintf(stderr, "mkslapdconf: %s while reading master property for %s\n", dsstatus_message(status), name);
		return NULL;
	}

	/* "local" is never a clone */
	if (dsdata_equal(info->tag, &NAME_LOCAL) == 0)
	{
		if (islocaladdress(s, master) == DSStatusOK &&
			dsdata_equal(info->tag, mastertag))
		{
			info->master++;
		}
	}
	else
	{
		info->master++;
	}

	dsdata_release(mastertag);
	dsdata_release(master);

	dsengine_close(s);

	return info;
}

void printhead(FILE *fp)
{
	time_t t;
	char *c;

	t = time(NULL);
	c = ctime(&t);

	fprintf(fp, "######################################################################\n");
	fprintf(fp, "# @(#)slapd.conf generated by mkslapdconf on %s", c);
	fprintf(fp, "######################################################################\n");

	fprintf(fp, "include\t\t%s/schema/core.schema\n", LDAP_SYSCONFDIR);
	fprintf(fp, "include\t\t%s/schema/cosine.schema\n", LDAP_SYSCONFDIR);
	fprintf(fp, "include\t\t%s/schema/nis.schema\n", LDAP_SYSCONFDIR);
	fprintf(fp, "include\t\t%s/schema/inetorgperson.schema\n", LDAP_SYSCONFDIR);
	fprintf(fp, "include\t\t%s/schema/misc.schema\n", LDAP_SYSCONFDIR);
	fprintf(fp, "include\t\t%s/schema/apple.schema\n", LDAP_SYSCONFDIR);
	fprintf(fp, "pidfile\t\t%s/slapd.pid\n", LOCALSTATEDIR);
	fprintf(fp, "argsfile\t%s/slapd.args\n", LOCALSTATEDIR);
	fprintf(fp, "allows\t\tbind_v2\n");
}

void printdefaultsearchbase(FILE *fp, domaininfo *info)
{
	/*
	 * generate defaultSearchBase line to alias NULL
	 * DN search base to local NetInfo database.
	 */
	if (info->suffix->length > 1)
	{
		fprintf(fp, "defaultsearchbase\t\"");
		dsdata_print(info->suffix, fp);
		fprintf(fp, "\"\n");
	}
	fprintf(fp, "\n");
}

void printreferrals(FILE *fp, domaininfo *info)
{
	int i;

	/*
	 * generate referral line in slapd.conf to point to parent server.
	 * we used to do let the server generate this dynamically but this
	 * is risky because, if the server becomes misconfigured,
	 * and is responsible for the NULL DN, a referral loop may
	 * result.
	 */
	if (info->parent != NULL)
	{
		for (i = 0; i < info->parent->count; i++)
		{
			fprintf(fp, "referral\t");
			dsdata_print(info->parent->value[i], fp);
			fprintf(fp, "\n");
		}
		fprintf(fp, "\n");
	}
}

void printdomainconf(FILE *fp, u_int32_t remote, u_int32_t isDefaultSearchBase, domaininfo *info)
{	
	assert(info != NULL);
	assert(info->suffix != NULL);
	assert(info->tag != NULL);

	fprintf(fp, "######################################################################\n");
	fprintf(fp, "# NetInfo database for ");
	dsdata_print(info->suffix, fp);
	fprintf(fp, "\n");
	fprintf(fp, "######################################################################\n");
	fprintf(fp, "database\tnetinfo\n");

	fprintf(fp, "suffix\t\t\"");
	dsdata_print(info->suffix, fp);
	fprintf(fp, "\"\n");

	/*
	 * Allow "dc=local" to be an alias for the local
	 * NetInfo domain.
	 */
	if (isDefaultSearchBase)
	{
		fprintf(fp, "suffix\t\t\"dc=local\"\n");
	}

	fprintf(fp, "flags\t\tDSENGINE_FLAGS_NATIVE_AUTHORIZATION");

	if (remote)
	{
		fprintf(fp, " DSSTORE_FLAGS_REMOTE_NETINFO DSSTORE_FLAGS_OPEN_BY_TAG");
	}

	if (info->master == 0)
		fprintf(fp, " DSSTORE_FLAGS_ACCESS_READONLY");
	else
		fprintf(fp, " DSSTORE_FLAGS_ACCESS_READWRITE");
	fprintf(fp, "\n");

	if (remote)
	{
		fprintf(fp, "datasource\tlocalhost/");
		dsdata_print(info->tag, fp);
		fprintf(fp, "\n");
	}
	else
	{
		fprintf(fp, "datasource\t%s/", NETINFO_DIR);
		dsdata_print(info->tag, fp);
		fprintf(fp, ".nidb\n");
	}

	fprintf(fp, "include\t\t%s/schema/netinfo.schema\n", LDAP_SYSCONFDIR);
	fprintf(fp, "\n");
}

void usage()
{
	fprintf(stderr, "usage: mkslapdconf [-r]\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	int i, count;
	u_int32_t remote = 0, dcLocal = 0;
	domaininfo **infov;
	dsattribute *tags;

	if (argc < 2)
	{
		remote = 0;
	}
	else if (argc == 2)
	{
		if (strcmp(argv[1], "-r") == 0) 
			remote = 1;
		else 
			usage();
	}
	else
	{
		usage();
	}

	tags = dsattribute_alloc();
	tags->key = NULL;
	tags->count = 0;
	tags->value = NULL;
	tags->retain = 1;

	if (remote) /* contact nibindd */
	{
		struct in_addr addr;
		nibind_registration *regvec;
		unsigned reglen;
		nibind_listreg_res *res;
		void *nb;

		addr.s_addr = htonl(INADDR_LOOPBACK);
		nb = nibind_new(&addr);
		if (nb == NULL)
		{
			fprintf(stderr, "mkslapdconf: could not contact nibindd\n");
			dsattribute_release(tags);
			nibind_free(nb);
			exit(1);
		}

		res = nibind_listreg_1(NULL, nb);
		if (res == NULL || res->status != NI_OK)
		{
			fprintf(stderr, "mkslapdconf: could not obtain registration information from nibindd\n");
			dsattribute_release(tags);
			nibind_free(nb);
			exit(1);
		}

		regvec = res->nibind_listreg_res_u.regs.regs_val;
		reglen = res->nibind_listreg_res_u.regs.regs_len;

		for (i = 0; i < reglen; i++)
		{
			dsattribute_append(tags, cstring_to_dsdata(regvec[i].tag));
		}

		nibind_free(nb);
	}
	else /* read /var/db/netinfo */
	{
		DIR *dp;
		struct direct *d;
		dsdata *tag;

		if (geteuid() != 0)
		{
			fprintf(stderr, "mkslapdconf: warning: not running as root, may not be able to read datastore\n");
		}

		dp = opendir(NETINFO_DIR);
		if (dp == NULL)
		{
			fprintf(stderr, "mkslapdconf: could not open %s\n", NETINFO_DIR);
			dsattribute_release(tags);
			exit(1);
		}

		while ((d = readdir(dp)) != NULL)
		{
			if (isnidir(d->d_name, &tag) == DSStatusOK)
			{
				dsattribute_append(tags, tag);
				dsdata_release(tag);
			}
		}
		closedir(dp);
	}

	infov = (domaininfo **)calloc(tags->count, sizeof(domaininfo *));
	assert(infov != NULL);
	count = 0;

	for (i = 0; i < tags->count; i++)
	{
		domaininfo *info;

		info = getservinfo(tags->value[i], remote);
		if (info == NULL)
		{
			dsattribute_release(tags);
			free(infov);
			exit(1);
		}
		if (dsdata_equal(info->suffix, &NAME_DC_LOCAL))
			dcLocal++;
		infov[count++] = info;
	}

	dsattribute_release(tags);

	printhead(stdout);

	if (count > 0)
	{
		qsort(infov, count, sizeof(domaininfo *), infocmp);

		printdefaultsearchbase(stdout, infov[0]);
	
		printreferrals(stdout, infov[count - 1]);

		for (i = 0; i < count; i++)
		{
			u_int32_t needDcLocalAlias;

			/*
			 * Careful to avoid stamping on a domain which is really
			 * called "dc=local", as this will stop slapd from
			 * starting. This can happen when a local domain is
			 * actually configured as "dc=local".
			 */
			needDcLocalAlias = (dcLocal > 0) ? 0 : (i == 0);
			printdomainconf(stdout, remote, needDcLocalAlias, infov[i]);
			freeinfo(infov[i]);
		}
	}

	free(infov);

	exit(0);
}
