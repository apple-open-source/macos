/*
 * niutil: command-line NetInfo interface
 * Written by Marc Majka
 *
 * Copyright 1989-94, NeXT Computer Inc.
 */
#include <NetInfo/config.h>
#include <stdio.h>
#include <netinfo/ni.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <errno.h>
#include <ctype.h>
#include <c.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <NetInfo/nilib2.h>

/* Reflect return value in exit status... */
/*   0     == success
 * 101-198 == ni_status (n <  NI_FAILED)
 *     199 == ni_status (n == NI_FAILED)
 * 200     == ni_connect() error
 * 201-2xx == ni_parse_status (+ 100)
 */
void
_EXIT_(int err)
{
	if (err == 0)
		/* if no error */
		exit(0);
	else if (err < NI_FAILED)
		/* if NetInfo error */
		exit(err + 100);
	else if (err == NI_FAILED)
		/* if NetInfo error && ni_status==NI_FAILED */
		exit(199);
	else if (err == NI_FAILED+1)
		/* if error returned by ni_connect() */
		exit(200);
	else
		/* if error parsing NetInfo tag */
		exit(err + 200);
}

#define SUN_RPC 1

#define TIMEOUT 30

#define NIU_NOOP 0
#define NIU_CREATE 1
#define NIU_DESTROY 2
#define NIU_CREATEPROP 3
#define NIU_APPENDPROP 4
#define NIU_MERGEPROP 5
#define NIU_INSERTVAL 6
#define NIU_DESTROYPROP 7
#define NIU_DESTROYVAL 8
#define NIU_RENAMEPROP 9
#define NIU_READ 10
#define NIU_LIST 11
#define NIU_RPARENT 12
#define NIU_RESYNC 13
#define NIU_STATISTICS 14
#define NIU_READPROP 15
#define NIU_READVAL 16
#define NIU_DOMAINNAME 17

int minargs[] = {
0, /* noop */
2, /* create */
2, /* destroy */
3, /* createprop */
4, /* appendprop */
4, /* mergeprop */
5, /* inservtval */
3, /* destroyprop */
4, /* destroyval */
4, /* renameprop */
2, /* read */
2, /* list */
1, /* rparent */
1, /* resync */
1, /* statistics */
3, /* readprop */
4, /* readval */
1  /* domainname */
};

char auth_user[128];
char auth_passwd[128];
bool opt_user, opt_tag, opt_numeric, opt_retry_busy;

char myname[128];
const char ROOTUSER[] = "root";

int timeout;

void usage()
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, "\t%s -create      [opts] <domain> <path>\n", myname);
	fprintf(stderr, "\t%s -destroy     [opts] <domain> <path>\n", myname);
	fprintf(stderr, "\t%s -createprop  [opts] <domain> <path> <propkey> [<val>...]\n", myname);
	fprintf(stderr, "\t%s -appendprop  [opts] <domain> <path> <propkey> <val>...\n", myname);
	fprintf(stderr, "\t%s -mergeprop   [opts] <domain> <path> <propkey> <val>...\n", myname);
	fprintf(stderr, "\t%s -insertval   [opts] <domain> <path> <propkey> <val> <index>\n", myname);
	fprintf(stderr, "\t%s -destroyprop [opts] <domain> <path> <propkey>...\n", myname);
	fprintf(stderr, "\t%s -destroyval  [opts] <domain> <path> <propkey> <val>...\n", myname);
	fprintf(stderr, "\t%s -renameprop  [opts] <domain> <path> <oldkey> <newkey>\n", myname);
	fprintf(stderr, "\t%s -read        [opts] <domain> <path>\n", myname);
	fprintf(stderr, "\t%s -list        [opts] <domain> <path> [<propkey>]\n", myname);
	fprintf(stderr, "\t%s -readprop    [opts] <domain> <path> <propkey>\n", myname);
	fprintf(stderr, "\t%s -readval     [opts] <domain> <path> <propkey> <index>\n", myname);
	fprintf(stderr, "\t%s -rparent     [opts] <domain>\n", myname);
	fprintf(stderr, "\t%s -resync      [opts] <domain>\n", myname);
	fprintf(stderr, "\t%s -statistics  [opts] <domain>\n", myname);
	fprintf(stderr, "\t%s -domainname  [opts] <domain>\n", myname);
	fprintf(stderr, "opts:\n");
	fprintf(stderr, "\t-t\t\tdomain specified by <hostname>/<tag>\n");
	fprintf(stderr, "\t-p\t\tprompt for password\n");
	fprintf(stderr, "\t-u <user>\tauthenticate as another user (implies -p)\n");
	fprintf(stderr, "\t-P <password>\tpassword supplied on command line (overrides -p)\n");
	fprintf(stderr, "\t-T <timeout>\tread & write timeout in seconds (default 30)\n");
	fprintf(stderr, "\t-n\t\tnumeric output for -rparent\n");
	fprintf(stderr, "\t-R\t\tRetry operation if master is busy\n");
	exit(1);
}

void do_domainname(int argc, char *argv[])
{
	/* print absolute domain name */

	void *domain;
	ni_name dname = NULL;
	ni_status ret;

    retry:
	if ((ret = do_open(myname, argv[0], &domain, opt_tag, timeout,
		(opt_user ? auth_user : NULL),
		(opt_user ? auth_passwd : NULL))) != 0) _EXIT_(ret);

	ret = ni_pwdomain(domain, &dname);
	if (ret != NI_OK) {
		fprintf(stderr,"%s: can't get name for domain %s: %s\n",
			myname, argv[0], ni_error(ret));
		ni_free(domain);
		if (ret == NI_MASTERBUSY && opt_retry_busy) {
		    goto retry;
		}
		_EXIT_(ret);
	}
	printf("%s\n", dname);
	ni_name_free(&dname);
	ni_free(domain);
}

void do_create(int argc, char *argv[])
{
	/* create a directory */

	void *domain;
	ni_status ret;

    retry:
	if ((ret = do_open(myname, argv[0], &domain, opt_tag, timeout,
		(opt_user ? auth_user : NULL),
		(opt_user ? auth_passwd : NULL))) != 0) _EXIT_(ret);

	ret = ni2_create(domain, argv[1]);
	if (ret != NI_OK) {
		fprintf(stderr,"%s: can't create directory %s: %s\n",
			myname, argv[1], ni_error(ret));
		ni_free(domain);
		if (ret == NI_MASTERBUSY && opt_retry_busy) {
		    goto retry;
		}
		_EXIT_(ret);
	}
	ni_free(domain);
}

void do_destroy(int argc, char *argv[])
{
	/* destroy a directory */

	void *domain;
	ni_status ret;

    retry:
	if ((ret = do_open(myname, argv[0], &domain, opt_tag, timeout,
		(opt_user ? auth_user : NULL),
		(opt_user ? auth_passwd : NULL))) != 0) _EXIT_(ret);

	ret = ni2_destroy(domain, argv[1]);
	if (ret != NI_OK) {
		fprintf(stderr,"%s: can't destroy directory %s: %s\n",
			 myname, argv[1], ni_error(ret));
		ni_free(domain);
		if (ret == NI_MASTERBUSY && opt_retry_busy) {
		    goto retry;
		}
		_EXIT_(ret);
	}
	ni_free(domain);
}

void do_createprop(int argc, char *argv[])
{
	/* create or replace a property */

	void *domain;
	ni_status ret;
	ni_namelist nl;
	int i;

	NI_INIT(&nl);
	for (i = 3; i < argc; i++) {
		ni_namelist_insert(&nl, argv[i], NI_INDEX_NULL);
	}

    retry:
	if ((ret = do_open(myname, argv[0], &domain, opt_tag, timeout,
		(opt_user ? auth_user : NULL),
		(opt_user ? auth_passwd : NULL))) != 0) _EXIT_(ret);

	ret = ni2_createprop(domain, argv[1], argv[2], nl);
	ni_namelist_free(&nl);
	if (ret != NI_OK) {
		fprintf(stderr,"%s: can't modify %s in directory %s: %s\n",
			myname, argv[2], argv[1], ni_error(ret));
		ni_free(domain);
		if (ret == NI_MASTERBUSY && opt_retry_busy) {
		    goto retry;
		}
		_EXIT_(ret);
	}
	ni_free(domain);
}

void do_appendprop(int argc, char *argv[])
{
	/* append values to a property */
	/* create the property if it doesn't exist */

	void *domain;
	ni_status ret;
	ni_namelist nl;
	int i;

	NI_INIT(&nl);
	for (i = 3; i < argc; i++) {
		ni_namelist_insert(&nl, argv[i], NI_INDEX_NULL);
	}

    retry:
	if ((ret = do_open(myname, argv[0], &domain, opt_tag, timeout,
		(opt_user ? auth_user : NULL),
		(opt_user ? auth_passwd : NULL))) != 0) _EXIT_(ret);

	ret = ni2_appendprop(domain, argv[1], argv[2], nl);
	ni_namelist_free(&nl);
	if (ret != NI_OK) {
		fprintf(stderr,"%s: can't modify %s in directory %s: %s\n",
			myname, argv[2], argv[1], ni_error(ret));
		ni_free(domain);
		if (ret == NI_MASTERBUSY) {
		    goto retry;
		}
		_EXIT_(ret);
	}
	ni_free(domain);
}

void do_mergeprop(int argc, char *argv[])
{
	/* merge values into a property (no duplicates are added) */
	/* create the property if it doesn't exist */

	void *domain;
	ni_status ret;
	ni_namelist nl;
	int i;

	NI_INIT(&nl);
	for (i = 3; i < argc; i++) {
		ni_namelist_insert(&nl, argv[i], NI_INDEX_NULL);
	}

    retry:
	if ((ret = do_open(myname, argv[0], &domain, opt_tag, timeout,
		(opt_user ? auth_user : NULL),
		(opt_user ? auth_passwd : NULL))) != 0) _EXIT_(ret);

	ret = ni2_mergeprop(domain, argv[1], argv[2], nl);
	ni_namelist_free(&nl);
	if (ret != NI_OK) {
		fprintf(stderr,"%s: can't modify %s in directory %s: %s\n",
			myname, argv[2], argv[1], ni_error(ret));
		ni_free(domain);
		if (ret == NI_MASTERBUSY && opt_retry_busy) {
		    goto retry;
		}
		_EXIT_(ret);
	}
	ni_free(domain);
}

void do_insertval(int argc, char *argv[])
{
	/* insert a value into a property */
	/* create the property if it doesn't exist */

	void *domain;
	ni_status ret;
	int index;

	index = atoi(argv[4]);
	if (index == 0 && strcmp(argv[4], "0")) {
		fprintf(stderr, "%s: index for -insertval must be an integer\n", myname);
		fprintf(stderr, "found %s\n", argv[4]);
		exit(3);
	}

    retry:
	if ((ret = do_open(myname, argv[0], &domain, opt_tag, timeout,
		(opt_user ? auth_user : NULL),
		(opt_user ? auth_passwd : NULL))) != 0) _EXIT_(ret);

	ret = ni2_insertval(domain, argv[1], argv[2], argv[3], index);
	if (ret != NI_OK) {
		fprintf(stderr,"%s: can't modify %s in directory %s: %s\n",
			myname, argv[2], argv[1], ni_error(ret));
		ni_free(domain);
		if (ret == NI_MASTERBUSY && opt_retry_busy) {
		    goto retry;
		}
		_EXIT_(ret);
	}
	ni_free(domain);
}

void do_destroyprop(int argc, char *argv[])
{
	/* destroy all properties in a directory with a given key */

	void *domain;
	ni_status ret;
	ni_namelist nl;
	int i;

	NI_INIT(&nl);
	for (i = 2; i < argc; i++) {
		ni_namelist_insert(&nl, argv[i], NI_INDEX_NULL);
	}

    retry:
	if ((ret = do_open(myname, argv[0], &domain, opt_tag, timeout,
		(opt_user ? auth_user : NULL),
		(opt_user ? auth_passwd : NULL))) != 0) _EXIT_(ret);

	ret = ni2_destroyprop(domain, argv[1], nl);
	ni_namelist_free(&nl);
	if (ret != NI_OK) {
		fprintf(stderr,"%s: can't destroy %s in directory %s: %s\n",
			myname, argv[2], argv[1], ni_error(ret));
		ni_free(domain);
		if (ret == NI_MASTERBUSY && opt_retry_busy) {
		    goto retry;
		}
		_EXIT_(ret);
	}
	ni_free(domain);
}

void do_destroyval(int argc, char *argv[])
{
	/* destroy all matching values from a property */

	void *domain;
	ni_status ret;
	ni_namelist nl;
	int i;

    retry:
	NI_INIT(&nl);
	for (i = 3; i < argc; i++) {
		ni_namelist_insert(&nl, argv[i], NI_INDEX_NULL);
	}

	if ((ret = do_open(myname, argv[0], &domain, opt_tag, timeout,
		(opt_user ? auth_user : NULL),
		(opt_user ? auth_passwd : NULL))) != 0) _EXIT_(ret);

	ret = ni2_destroyval(domain, argv[1], argv[2], nl);
	ni_namelist_free(&nl);
	if (ret != NI_OK) {
		fprintf(stderr,"%s: can't modify %s in directory %s: %s\n",
			myname, argv[2], argv[1], ni_error(ret));
		ni_free(domain);
		if (ret == NI_MASTERBUSY && opt_retry_busy) {
		    goto retry;
		}
		_EXIT_(ret);
	}
	ni_free(domain);
}

void do_renameprop(int argc, char *argv[])
{
	/* rename a property */

	void *domain;
	ni_status ret;

    retry:
	if ((ret = do_open(myname, argv[0], &domain, opt_tag, timeout,
		(opt_user ? auth_user : NULL),
		(opt_user ? auth_passwd : NULL))) != 0) _EXIT_(ret);

	ret = ni2_renameprop(domain, argv[1], argv[2], argv[3]);
	if (ret != NI_OK) {
		fprintf(stderr,"%s: can't rename %s in directory %s: %s\n",
			myname, argv[2], argv[1], ni_error(ret));
		if (ret == NI_MASTERBUSY && opt_retry_busy) {
		    goto retry;
		}
		_EXIT_(ret);
	}
	ni_free(domain);
}

void do_print_proplist(ni_proplist plist)
{
	/* utility routine: print a property list */

	int pn, vn;
	ni_property prop;
	ni_namelist values;

	/* for each property */
	for (pn = 0; pn <  plist.ni_proplist_len; pn++) {

		prop = plist.ni_proplist_val[pn];

		/* print the property key */
		printf("%s:", prop.nip_name);
	
		values = prop.nip_val;

		/* for each value in the namelist for this property */
		for (vn = 0; vn < values.ni_namelist_len; vn++) {
			printf(" %s", values.ni_namelist_val[vn]);
		}
		printf("\n");
	}
}

void do_read(int argc, char *argv[])
{
	/* print the contents of a directory */

	ni_proplist pl;
	void *domain;
	ni_id dir;	
	ni_status ret;

	if ((ret = do_open(myname, argv[0], &domain, opt_tag, timeout,
		(opt_user ? auth_user : NULL),
		(opt_user ? auth_passwd : NULL))) != 0) _EXIT_(ret);

	/* argv[1] should be a directory specification */
	ret = ni2_pathsearch(domain, &dir, argv[1]);
	if (ret != NI_OK) {
		fprintf(stderr, "%s: can't open directory %s: %s\n",
			myname, argv[1], ni_error(ret));
		ni_free(domain);
		_EXIT_(ret);
	}

	/* get the property list stored in the this directory */
	NI_INIT(&pl);
	ret = ni_read(domain, &dir, &pl);
	if (ret != NI_OK) {
		fprintf(stderr, "%s: can't read directory %s: %s\n",
			myname, argv[1], ni_error(ret));
		ni_free(domain);
		_EXIT_(ret);
	}

	do_print_proplist(pl);
	ni_proplist_free(&pl);
	ni_free(domain);
}

void do_readprop(int argc, char *argv[])
{
	/* print the values of a property */

	ni_namelist nl;
	void *domain;
	ni_id dir;	
	ni_status ret;
	int vn;

	if ((ret = do_open(myname, argv[0], &domain, opt_tag, timeout,
		(opt_user ? auth_user : NULL),
		(opt_user ? auth_passwd : NULL))) != 0) _EXIT_(ret);

	/* argv[1] should be a directory specification */
	ret = ni2_pathsearch(domain, &dir, argv[1]);
	if (ret != NI_OK) {
		fprintf(stderr, "%s: can't open directory %s: %s\n",
			myname, argv[1], ni_error(ret));
		ni_free(domain);
		_EXIT_(ret);
	}

	/* get the property values for argv[2] */
	NI_INIT(&nl);
	ret = ni_lookupprop(domain, &dir, argv[2], &nl);
	if (ret != NI_OK) {
		fprintf(stderr, "%s: can't get property %s in directory %s: %s\n",
			myname, argv[2], argv[1], ni_error(ret));
		ni_free(domain);
		_EXIT_(ret);
	}

	for (vn = 0; vn < nl.ni_namelist_len; vn++) {
		if (vn > 0) printf(" ");
		printf("%s", nl.ni_namelist_val[vn]);
	}
	printf("\n");

	ni_namelist_free(&nl);
	ni_free(domain);
}

void do_readval(int argc, char *argv[])
{
	/* print a value of a property */

	ni_namelist nl;
	void *domain;
	ni_id dir;	
	ni_status ret;
	int vn;

	if ((ret = do_open(myname, argv[0], &domain, opt_tag, timeout,
		(opt_user ? auth_user : NULL),
		(opt_user ? auth_passwd : NULL))) != 0) _EXIT_(ret);

	/* argv[1] should be a directory specification */
	ret = ni2_pathsearch(domain, &dir, argv[1]);
	if (ret != NI_OK) {
		fprintf(stderr, "%s: can't open directory %s: %s\n",
			myname, argv[1], ni_error(ret));
		ni_free(domain);
		_EXIT_(ret);
	}

	/* get the property values for argv[2] */
	NI_INIT(&nl);
	ret = ni_lookupprop(domain, &dir, argv[2], &nl);
	if (ret != NI_OK) {
		fprintf(stderr, "%s: can't get property %s in directory %s: %s\n",
			myname, argv[2], argv[1], ni_error(ret));
		ni_free(domain);
		_EXIT_(ret);
	}

	vn = atoi(argv[3]);
	if (vn >= nl.ni_namelist_len) {
		fprintf(stderr, "%s: property %s in directory %s has no value at index %d\n",
			myname, argv[2], argv[1], vn);
		ni_free(domain);
		_EXIT_(ret);
	}

	printf("%s\n", nl.ni_namelist_val[vn]);

	ni_namelist_free(&nl);
	ni_free(domain);
}

void do_list(int argc, char *argv[])
{
	/* list subdirectories by name, or by another key if supplied */
	int en, vn;
	ni_entrylist el;
	ni_namelist *values;
	void *domain;
	ni_id dir;	
	ni_status ret;
	char listkey[256];

	if ((ret = do_open(myname, argv[0], &domain, opt_tag, timeout,
		(opt_user ? auth_user : NULL),
		(opt_user ? auth_passwd : NULL))) != 0) _EXIT_(ret);

	/* argv[1] should be a directory specification */
	ret = ni2_pathsearch(domain, &dir, argv[1]);
	if (ret != NI_OK) {
		fprintf(stderr, "%s can't open directory %s: %s\n",
			myname, argv[1], ni_error(ret));
		_EXIT_(ret);
	}

	/* if there's a second argument, it's a property key */
	/* by default, we'll list directories by name */
	if (argc > 2) strcpy(listkey, argv[2]);
	else strcpy(listkey, "name");

	/* get the values of the specified key for all subdirectories */
	NI_INIT(&el);
	ret = ni_list(domain, &dir, listkey, &el);
	if (ret != NI_OK) {
		fprintf(stderr, "%s: can't list directory %s: %s\n",
			myname, argv[1], ni_error(ret));
		ni_free(domain);
		_EXIT_(ret);
	}

	/* for each entry */
	for (en = 0; en <  el.ni_entrylist_len; en++) {

		/* print the directory ID */
		printf("%lu\t", el.ni_entrylist_val[en].id);

		/* entries contain a pointer to a namelist of values */
		if (el.ni_entrylist_val[en].names != NULL) {
			values = el.ni_entrylist_val[en].names;

			/* for each value in the namelist for this property */
			for (vn = 0; vn < values->ni_namelist_len; vn++) {
				/* print the value */
				printf(" %s", values->ni_namelist_val[vn]);
			}
		}
		printf("\n");
	}

	ni_entrylist_free(&el);
	ni_free(domain);
}

void do_resync(int argc, char *argv[])
{
	/* resync a NetInfo server */
	/* resyncs the master if a domain is specified */

	void *domain;
	ni_status ret;

    retry:
	if ((ret = do_open(myname, argv[0], &domain, opt_tag, timeout,
		(opt_user ? auth_user : NULL),
		(opt_user ? auth_passwd : NULL))) != 0) _EXIT_(ret);

	/* if the domain was opened by name, re-sync the master */
	if (!opt_tag) ni_needwrite(domain, 1);

	ret = ni_resync(domain);
	if (ret != NI_OK) {
		fprintf(stderr, "%s can't resync domain %s: %s\n",
			myname, argv[0], ni_error(ret));
		ni_free(domain);
		if (ret == NI_MASTERBUSY && opt_retry_busy) {
		    goto retry;
		}
		_EXIT_(ret);
	}
	ni_free(domain);
}

void do_rparent(int argc, char *argv[])
{
	/* find the parent server for a given server */
	/* anybody's guess who you're talking to if a domain is specified */
	/* rather than a host/tag */

	/* MM I suppose I should print the child's name and tag... */

	void *domain;
	ni_status ret;
	struct sockaddr_in parent_addr;
	char *parent_tag;
	struct hostent *ahost;

	if ((ret = do_open(myname, argv[0], &domain, opt_tag, timeout,
		(opt_user ? auth_user : NULL),
		(opt_user ? auth_passwd : NULL))) != 0) _EXIT_(ret);

	ret = ni2_rparent(domain, &parent_addr, &parent_tag);
	if (ret == NI_NETROOT) {
		printf("root domain: no parent\n");
		ni_free(domain);
		_EXIT_(ret);
	}

	if (ret != NI_OK) {
		fprintf(stderr, "%s: can't find rparent for domain %s: %s\n",
			myname, argv[0], ni_error(ret));
		ni_free(domain);
		_EXIT_(ret);
	}

	if (opt_numeric) {
		ahost = (struct hostent *)NULL;
	}
	else {
		ahost = gethostbyaddr((char *)&(parent_addr.sin_addr),
			sizeof(struct in_addr), AF_INET);
	}

	if (ahost == NULL) {
		printf("%s", inet_ntoa(parent_addr.sin_addr));
	}
	else {
		printf("%s", ahost->h_name);
	}
	printf("/%s\n", parent_tag);

	free(parent_tag);
	ni_free(domain);
}

void do_statistics(int argc, char *argv[])
{
	/* print a server's statistics */

	ni_proplist pl;
	void *domain;
	ni_status ret;

	if ((ret = do_open(myname, argv[0], &domain, opt_tag, timeout,
		(opt_user ? auth_user : NULL),
		(opt_user ? auth_passwd : NULL))) != 0) _EXIT_(ret);

	/* get the statistics property list */
	NI_INIT(&pl);
	ret = ni_statistics(domain, &pl);
	if (ret != NI_OK) {
		fprintf(stderr, "%s: can't get statistics for domain %s: %s\n",
			myname, argv[0], ni_error(ret));
		ni_free(domain);
		_EXIT_(ret);
	}

	do_print_proplist(pl);
	ni_proplist_free(&pl);
	ni_free(domain);
}

int main(int argc, char *argv[])
{
	int i, op, firstdata;
	int j, len;
	char *lastchar, *p, *t;
	bool opt_promptpw, opt_passwd;
	char *slash;
	
	slash = rindex(argv[0], '/');
	if (slash == NULL) strcpy(myname, argv[0]);
	else strcpy(myname, slash+1);

	if (argc < 2) usage();

	opt_retry_busy = FALSE;
	opt_promptpw = FALSE;
	opt_user = FALSE;
	opt_passwd = FALSE;
	opt_tag = FALSE;
	opt_numeric = FALSE;
	op = NIU_NOOP;

	timeout = TIMEOUT;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			/* do nothing */
		}
		else if (!strcmp(argv[i], "-p") && !opt_promptpw) {
			opt_promptpw = TRUE;
		}
		else if (!strcmp(argv[i], "-t") && !opt_tag) {
			opt_tag = TRUE;
		}
		else if (!strcmp(argv[i], "-n") && !opt_numeric) {
			opt_numeric = TRUE;
		}
		else if (!strcmp(argv[i], "-R")) {
			opt_retry_busy = TRUE;
		}
		else if (!strcmp(argv[i], "-T")) {
			if (i == argc - 1) {
				fprintf(stderr, "%s: insufficient number of arguments for %s\n", myname, argv[i]);
				usage(argv[0]);
			}
			timeout = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "-u") && !opt_user) {
			if (i == argc - 1) {
				fprintf(stderr, "%s: insufficient number of arguments for %s\n", myname, argv[i]);
				usage(argv[0]);
			}
			opt_user = TRUE;
			i++;
			strcpy(auth_user, argv[i]);

			/* BEGIN UGLY HACK TO MAKE argv[i] DISAPPEAR */
			lastchar = &argv[argc-1][0];
			while(*lastchar != '\0') lastchar++;
			if ((i + 1) == argc) p = lastchar;
			else p = &argv[i+1][0];
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
		else if (!strcmp(argv[i], "-P") && !opt_passwd) {
			if (i == argc - 1) {
				fprintf(stderr, "%s: insufficient number of arguments for %s\n", myname, argv[i]);
				usage(argv[0]);
			}
			opt_passwd = TRUE;
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
		else if (!strcmp(argv[i], "-create"))		op = NIU_CREATE;
		else if (!strcmp(argv[i], "-destroy"))		op = NIU_DESTROY;
		else if (!strcmp(argv[i], "-createprop"))	op = NIU_CREATEPROP;
		else if (!strcmp(argv[i], "-appendprop"))	op = NIU_APPENDPROP;
		else if (!strcmp(argv[i], "-mergeprop"))	op = NIU_MERGEPROP;
		else if (!strcmp(argv[i], "-insertval"))	op = NIU_INSERTVAL;
		else if (!strcmp(argv[i], "-destroyprop"))	op = NIU_DESTROYPROP;
		else if (!strcmp(argv[i], "-destroyval"))	op = NIU_DESTROYVAL;
		else if (!strcmp(argv[i], "-renameprop"))	op = NIU_RENAMEPROP;
		else if (!strcmp(argv[i], "-read"))		op = NIU_READ;
		else if (!strcmp(argv[i], "-list"))		op = NIU_LIST;
		else if (!strcmp(argv[i], "-rparent"))		op = NIU_RPARENT;
		else if (!strcmp(argv[i], "-resync"))		op = NIU_RESYNC;
		else if (!strcmp(argv[i], "-statistics"))	op = NIU_STATISTICS;
		else if (!strcmp(argv[i], "-stat"))		op = NIU_STATISTICS;
		else if (!strcmp(argv[i], "-readprop"))		op = NIU_READPROP;
		else if (!strcmp(argv[i], "-readval"))		op = NIU_READVAL;
		else if (!strcmp(argv[i], "-domainname"))	op = NIU_DOMAINNAME;
		else if (op == NIU_NOOP) usage();
	}

	firstdata = argc;
	for (i = 1; (i < argc) && (firstdata == argc); i++) {
		if (argv[i][0] != '-' && strcmp(argv[i-1], "-T")) firstdata = i;
	}

	if (opt_user) opt_promptpw = TRUE;
	if (opt_passwd) opt_promptpw = FALSE;

	if (!opt_user && (opt_passwd || opt_promptpw)) {
		opt_user = TRUE;
		strcpy(auth_user, ROOTUSER);
	}

	if ((argc - firstdata) < minargs[op]) {
		fprintf(stderr, "%s: insufficient number of arguments for %s\n", myname, argv[1]);
		usage();
	}

	if (opt_promptpw) {
		strcpy(auth_passwd, (char *)getpass("Password: "));
	}

	switch (op) {
		case NIU_NOOP:
			break;
		case NIU_CREATE:
			do_create(argc-firstdata, argv+firstdata);
			break;
		case NIU_DESTROY:
			do_destroy(argc-firstdata, argv+firstdata);
			break;
		case NIU_CREATEPROP:
			do_createprop(argc-firstdata, argv+firstdata);
			break;
		case NIU_APPENDPROP:
			do_appendprop(argc-firstdata, argv+firstdata);
			break;
		case NIU_MERGEPROP:
			do_mergeprop(argc-firstdata, argv+firstdata);
			break;
		case NIU_INSERTVAL:
			do_insertval(argc-firstdata, argv+firstdata);
			break;
		case NIU_DESTROYPROP:
			do_destroyprop(argc-firstdata, argv+firstdata);
			break;
		case NIU_DESTROYVAL:
			do_destroyval(argc-firstdata, argv+firstdata);
			break;
		case NIU_RENAMEPROP:
			do_renameprop(argc-firstdata, argv+firstdata);
			break;
		case NIU_READ:
			do_read(argc-firstdata, argv+firstdata);
			break;
		case NIU_LIST:
			do_list(argc-firstdata, argv+firstdata);
			break;
		case NIU_RESYNC:
			do_resync(argc-firstdata, argv+firstdata);
			break;
		case NIU_RPARENT:
			do_rparent(argc-firstdata, argv+firstdata);
			break;
		case NIU_STATISTICS:
			do_statistics(argc-firstdata, argv+firstdata);
			break;
		case NIU_READPROP:
			do_readprop(argc-firstdata, argv+firstdata);
			break;
		case NIU_READVAL:
			do_readval(argc-firstdata, argv+firstdata);
			break;
		case NIU_DOMAINNAME:
			do_domainname(argc-firstdata, argv+firstdata);
			break;
		default:
			fprintf(stderr, "%s: sorry, %s is not implemented yet!\n", myname, argv[1]);
			break;
	}

	exit(0);
}
