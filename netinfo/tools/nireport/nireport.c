/* 
 * nireport: print a tab-separated report of selected values in all
 * subdirectories of a given directory in a NetInfo domain.
 * usage: nireport domain directory property ...
 *
 * domain is either a hierarchical domain name.
 *   (e.g. "/foo", ".", "../..", "/", etc)
 * or domain is "-t <host>/<tag>", where host is a name or dotted-quad address.
 *   (e.g. "-t zippy/network", "-t 129.18.11.59/local", etc)
 *
 * directory is a NetInfo directory in the given domain.
 *   (e.g. "/", "/users", "/machines", etc)
 *
 * property... is a list of property keys for which values will be printed.
 * Unmatched keys are quietly ignored.
 * 
 * Examples:
 * nireport / /users uid gid name
 * nireport .. /machines serves ip_address
 * nireport -t wizard/network /printers rm rp
 *
 * Written by Marc Majka
 *
 * Copyright 1994, NeXT Computer Inc.
 */

#include <objc/objc.h>
#include <netinfo/ni.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <NetInfo/nilib2.h>

char myname[128];

/* Reflect return value in exit status... */
/*   0     == success
 * 101-198 == ni_status (n <  NI_FAILED)
 *     199 == ni_status (n == NI_FAILED)
 * 200     == ni_connect() error
 * 201-2xx == ni_parse_status (+ 100)
 */
void _EXIT_(int err)
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

void errexit(char *s, ni_status status)
{
	fprintf(stderr, "%s: %s\n", s, ni_error(status));
	_EXIT_(status);
}

int ni_report_dir(dom, dir, keys, n)
void *dom;		// domain
ni_id *dir;		// directory
char *keys[];	// property keys
int n;			// number of keys
{
	int i, j, k, looking;
	ni_status rc;
	ni_namelist n1, n2;

	/* get list of properties */
	NI_INIT(&n1);
	rc = ni_listprops(dom, dir, &n1);
	if (rc) errexit("ni_listprops", rc);

	/* For each key */
	for (k = 0; k < n; k++) {

		/* flag to see if we found it somewhere */
		looking = 1;
		
		/* for each property */
		for (i = 0; i < n1.ni_namelist_len && looking; i++) {
	
			/* are we interested in this property key? */
			if (!strcmp(keys[k], n1.ni_namelist_val[i])) {

				/* remember that we found the key */
				looking = 0;

				/* look up values for this property */
				NI_INIT(&n2);
				rc = ni_lookupprop(dom, dir, n1.ni_namelist_val[i], &n2);
				if (rc) errexit("ni_listprops", rc);

				/* print property values */
				if (n2.ni_namelist_len == 0)
					printf("#NoValue#");

				for (j = 0; j < n2.ni_namelist_len; j++) {
					if (j) printf(",");
					printf("%s", n2.ni_namelist_val[j]);
				}
				printf("\t");

				/* free list of values */
				ni_namelist_free(&n2);
			}
		} // each property

		/* check to see if we ever found the key */
		if (looking) printf("#NoValue#\t");

	} // each key

	printf("\n");

	/* free list of keys */
	ni_namelist_free(&n1);

	return(0);
}

int ni_report(dom, dir, keys, n)
void *dom;		// domain
ni_id *dir;		// directory
char *keys[];	// property keys
int n;			// number of keys
{
	int i, rc;
	ni_idlist children;
	ni_id child;

	/* get a list of all my children */
	NI_INIT(&children);
	rc = ni_children(dom, dir, &children);
	if (rc) errexit("ni_children", rc);

	/* print a report for each child */
	for (i = 0; i < children.ni_idlist_len; i++) {
		child.nii_object = children.ni_idlist_val[i];
		ni_report_dir(dom, &child, keys, n);
	}

	/* free list of clild ids */
	ni_idlist_free(&children);
	return(0);
}

void usage()
{
	fprintf(stderr, "usage: %s [-T timeout] [-t] domain directory property...\n",
		myname);
	exit(1);
}

int main(int argc, char *argv[])
{
	void *dom;
	ni_id dir;
	int i, dp, firstprop, timeout;
	ni_status rc;
	BOOL bytag;
	char *slash;
	
	slash = rindex(argv[0], '/');
	if (slash == NULL) strcpy(myname, argv[0]);
	else strcpy(myname, slash+1);

	if (argc < 3) usage();

	bytag = NO;
	timeout = 30;
	dp = 0;
	firstprop = 3;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-t")) {
			if (bytag) usage();
			bytag = YES;
			dp = i + 1;
			firstprop += 1;
		}
		if (!strcmp(argv[i], "-T")) {
			timeout = atoi(argv[++i]);
			firstprop += 2;
		}
	}

	/* check for no properties in argv */
	if (firstprop >= argc) {
		fprintf(stderr, "no properties\n");
		usage();
	}

	/* open the domain */
	if (dp == 0) dp = firstprop - 2;
	if ((rc = do_open(myname, argv[dp], &dom, bytag, timeout, NULL, NULL))) {
	        _EXIT_(rc);
	}

	/* open the specified directory */
	rc = ni_pathsearch(dom, &dir, argv[firstprop-1]);
	if (rc) errexit("ni_pathsearch", rc);

	/* print property values for selected properties in all subdirs */
	ni_report(dom, &dir, argv+firstprop, argc-firstprop);

	ni_free(dom);
	exit(0);
}

