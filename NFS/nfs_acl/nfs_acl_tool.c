/*
 * Copyright (c) 2017 Apple Inc. All rights reserved.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <err.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/acl.h>
#include "nfs_acl_lib.h"

int verbose = 1;

struct option longopts[] = {
	{ "list", no_argument, NULL, 'l' },
	{ "verbose", no_argument, NULL, 'v' },
	{ "quiet", no_argument, NULL, 'q' },
	{ "help", no_argument, NULL, 'h' },
	{ "keep", no_argument, NULL, 'k' },
	{ "acl", required_argument, NULL, 'a' },
	{ "set", no_argument, NULL, 's' },
	{ /* name */ NULL, /* has_arg */ 0, /* flag */ NULL, /* val */ 0 }
};

void
free_acl(acl_t *objp)
{
	if (*objp)
		acl_free(*objp);
	*objp = NULL;
}

struct opdesc {
	const char *arg;
	const char *descp;
} opdesc[] = {
	{ /*list*/ "", "List the acl associated with each path" },
	{ /*verbose*/ "", "Inceease verboseness of tool. Default is 1" },
	{ /*quiet*/ "", "Only fatal errors are printed, sucess/failure is determine for exit status" },
	{ /*help*/ "", "Print this help and exit" },
	{ /*keep*/ "", "Don't remove temporary files or directories use to verify inheritance from directories" },
	{ /*acl*/  "acl*", "Acl to set or merge on each path specified" },
	{ /*set*/ "", "Don't merge, just set the acl" }
};

void usage()
{
	struct option *op;
	struct opdesc *opdp;

	fprintf(stderr, "%s: {-l | [-vqhks] [-a acl] path [ path ...]\n", getprogname());
	for (op = longopts, opdp = opdesc; op && op->name; op++, opdp++) {
		printf("-%c, --%-8s %-8s %s\n", op->val, op->name, opdp->arg, opdp->descp);
	}
	fprintf(stderr, "* acls are specified as in chmod execpt multiple entries and be spedified by \n"
		"using multiple acl options or separating them with semicolons with no spaces.\n");
	exit(1);
}

acl_t
get_acl_from_path(const char *path)
{

	return (acl_get_file(path, ACL_TYPE_EXTENDED));
}

int
compare_acl_to_path(acl_t acl, const char *path, int is_dir, int inherited, int pflag)
{
	int status = 0;
	acl_t temp_acl;

	temp_acl = get_acl_from_path(path);
	if (verbose + pflag > 1) {
		printf("%s: Got ACL:\n", path);
		printacl(temp_acl, is_dir);
		printf("acl supplied:\n");
		printacl(acl, is_dir);
	}
	if (!acl_matches(temp_acl, acl, inherited))
		status = 1;

	free_acl(&temp_acl);

	return (status);
}

int
set_acl_on_path(acl_t acl, const char *path)
{

	return (acl_set_file(path, ACL_TYPE_EXTENDED, acl));
}

acl_t
get_inheritable_acl(acl_t aacl, acl_flag_t flag)
{
	int next_entry;
	acl_t i_acl, acl;
	acl_entry_t entry;
	acl_entry_t i_entry;
	acl_flagset_t eflags;

	acl = acl_dup(aacl);
	if (acl == NULL)
		err(1, "get_inheritable_acl");
	i_acl = acl_init(1);
	if (i_acl == NULL)
		err(1, "get_inheritable_acl");

	for (next_entry = ACL_FIRST_ENTRY;
	     acl_get_entry(acl, next_entry, &entry) == 0;
	     next_entry = ACL_NEXT_ENTRY) {
		if (acl_get_flagset_np(entry, &eflags) != 0) {
			err(1, "Unable to obtain entry flagset");
		}
		if (acl_get_flag_np(eflags, flag)) {
			if (acl_create_entry(&i_acl, &i_entry))
				err(1, "acl_create_entry() failed");
			if (acl_copy_entry(i_entry, entry))
				err(1, "acl_copy_entry() failed");
		}
	}

	if (verbose > 1) {
		printf("acl is:\n");
		printacl(i_acl, flag == ACL_ENTRY_FILE_INHERIT ? 0 : 1);
	}

	acl_free(acl);

	return (i_acl);
}

acl_t
merge_acls(acl_t oacl, acl_t modifier, const char *path)
{
	unsigned aindex  = 0;
	acl_entry_t entry = NULL;
	int retval = 0;
	acl_t new_acl;

	if (!modifier) {
		return oacl;
	}
	if (!oacl) {
		return (modifier);
	}

	new_acl = acl_dup(oacl);
	if (new_acl == NULL)
		return (NULL);

	for (aindex = 0; !retval &&
		     acl_get_entry(modifier,
				   (entry == NULL ? ACL_FIRST_ENTRY :
				    ACL_NEXT_ENTRY), &entry) == 0;
	     aindex++) {

		retval += modify_acl(&new_acl, entry, ACL_SET_FLAG, -1, 0, 0, path);
	}
	if (retval) {
		acl_free(new_acl);
		return (NULL);
	}

	return (new_acl);
}


acl_t
parse_acl_argument(acl_t acl, char *acl_rep)
{
	acl_t new_acl = parse_acl_entries(acl_rep);
	acl_t ret_acl;

	if (new_acl == NULL)
		return (acl);

	ret_acl = merge_acls(acl, new_acl, "command line argument");
	if (ret_acl)
		acl_free(acl);

	return (ret_acl);
}

void
print_acl_from_path(const char *path, int isdir)
{
	acl_t acl;

	acl =  acl_get_file(path, ACL_TYPE_EXTENDED);
	if (acl == NULL) {
		if (verbose > 1)
			warn("%s: acl_get_file", path);
		return;
	}

	printf("%s\n", path);
	printacl(acl, isdir);
	acl_free(acl);
}

int
main(int argc, char *argv[])
{
	int opt;
	int lindex;
	int set_only = 0;
	int keep = 0;
	int list = 0;
	const char *path;
	acl_t orig_acl = NULL;
	acl_t set_acl = NULL;
	acl_t merge_acl;
	struct stat sbuf;

	while ((opt = getopt_long(argc, argv, "a:skvqhl", longopts, &lindex)) != -1) {
		switch (opt) {
		case 'a': set_acl = parse_acl_argument(set_acl, optarg);
			break;
		case 's': set_only = 1;
			break;
		case 'k': keep = 1;
			break;
		case 'v': verbose++;
			break;
		case 'q': verbose = 0;
			break;
		case 'l': list = 1;
			break;
		case 'h':
		default:
			usage();
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (!list && (set_acl == NULL && set_only))
		errx(1, "No acl to use");

	/* Get a file object path */
	for (path = *argv; path; argv++, argc--, path = *argv) {
		int isdir;
		int status = 0;

		if (lstat(path, &sbuf) == -1) {
			warn("%s", path);
			continue;
		}
		if (!S_ISDIR(sbuf.st_mode) && !S_ISREG(sbuf.st_mode)) {
			warnx("path is not a direcory or regular file, skipping");
			continue;
		}
		isdir = S_ISDIR(sbuf.st_mode);
		if (list) {
			print_acl_from_path(path, isdir);
			continue;
		}

		if (!set_only) {
			orig_acl = get_acl_from_path(path);
			if (orig_acl == NULL && set_acl == NULL) {
				warn("No acl to set. skipping");
				continue;
			}
			if (verbose) {
				printf("%s: Got ACL\n", path);
				printacl(orig_acl, isdir);
			}
			merge_acl = merge_acls(orig_acl, set_acl, path);
			if (merge_acl == NULL) {
				errx(1, "Could not merge acls");
			}
			if (merge_acl != orig_acl)
				free_acl(&orig_acl);
		} else {
			merge_acl = set_acl;
		}

		if (verbose) {
			printf("%s: Setting ACL\n", path);
			printacl(merge_acl, isdir);
		}
		if (set_acl_on_path(merge_acl, path)) {
			err(1, "Could not set acl");
		}

		status = compare_acl_to_path(merge_acl, path, isdir, 0, 0);
		if (status) {
			errx(1, "Acls did not compare");
		}

		if (isdir) {
			char *objname;
			acl_t file_acl;
			acl_t dir_acl;

			file_acl = get_inheritable_acl(merge_acl, ACL_ENTRY_FILE_INHERIT);
			if (file_acl) {
				int fd;

				asprintf(&objname, "%s/temp.XXXXXX", path);
				if (objname == NULL)
					err(1, "asprintf");
				mktemp(objname);
				if (verbose)
					printf("Creating file %s\n", objname);
				fd = open(objname, O_WRONLY|O_CREAT);
				if (fd == -1)
					err(1, "Could not create %s", objname);
				close(fd);
				status = compare_acl_to_path(file_acl, objname, 0, 1, 1);
				if (!keep)
					unlink(objname);
				free_acl(&file_acl);
				free(objname);
				if (status)
					errx(1, "Acls don't compare");
			}

			dir_acl = get_inheritable_acl(merge_acl, ACL_ENTRY_DIRECTORY_INHERIT);
			if (dir_acl) {
				asprintf(&objname, "%s/temp_d.XXXXXX", path);
				if (objname == NULL)
					err(1, "asprintf");
				mktemp(objname);
				if (verbose)
					printf("Creating directory %s\n", objname);
				if (mkdir(objname, 0777))
					err(1, "mkdir %s", objname);
				status = compare_acl_to_path(dir_acl, objname, 1, 1, 1);
				if (!keep)
					rmdir(objname);
				if (status)
					errx(1, "Acls did not compare");
				free_acl(&dir_acl);
				free(objname);
			}
		}
		if (merge_acl != set_acl)
			free_acl(&merge_acl);
	}
}
