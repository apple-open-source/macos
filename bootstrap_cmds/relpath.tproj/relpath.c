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
 * relpath [-d DIR] START_DIR END_PATH
 *
 * Find a relative path from START_DIR to END_PATH.
 * Prints the relative path on standard out.
 *
 * If -d DIR, then only emit a relative path if both
 * START_DIR and END_PATH are sub-directories of DIR;
 * otherwise, emit an absolute path to END_PATH.
 */
#import	<stdio.h>
#import <sys/types.h>
#import <sys/stat.h>
#import <sys/param.h>
#import	<strings.h>
#import	<libc.h>

static int is_prefix(const char *path1, const char *path2);
static char *abspath(const char *opath, char *absbuf);

const char *progname;

void main(int argc, const char * const *argv)
{
	const char *arg;
	const char *base_dir = NULL;
	char start_path[MAXPATHLEN+1];
	char end_path[MAXPATHLEN+1];
	char base_path[MAXPATHLEN+1];
	struct stat st;
	int i;
	int last_elem;
	int prev_path;

	unsetenv("PWD");

	progname = (arg = rindex(*argv, '/')) != NULL ? arg + 1 : *argv;
	argc -= 1; argv += 1;

	for (; argc > 1 && **argv == '-'; argv += 1, argc -= 1) {

		arg = &(*argv)[1];
		do {
			switch (*arg) {
			case 'd':
				argc -= 1; argv += 1;
				if (argc <= 0) {
					fprintf(stderr, "%s: -d takes "
					  "directory name\n", progname);
					exit(1);
				}
				base_dir = *argv;
				break;
			default:
				fprintf(stderr, "%s: Illegal flag: %c\n",
				  progname, *arg);
				exit(1);
			}
		} while (*++arg);
	}
	if (argc < 2) {
		fprintf(stderr, "Usage: %s [-d DIR] START_PATH END_PATH\n",
		  progname);
		exit(1);
	}
	(void) abspath(argv[0], start_path);
	(void) abspath(argv[1], end_path);
	if (base_dir) {
		(void) abspath(base_dir, base_path);
		if (!is_prefix(base_path, start_path) ||
		    !is_prefix(base_path, end_path)) {
			printf("%s\n", end_path);
			exit(0);
		}
		if (stat(base_path, &st) < 0) {
			fprintf(stderr, "%s: ", progname);
			perror(base_path);
			exit(1);
		}
		if ((st.st_mode & S_IFMT) != S_IFDIR) {
			fprintf(stderr, "%s: -d DIR must be directory\n",
			  progname);
			exit(1);
		}
	}
	if (stat(start_path, &st) < 0) {
		fprintf(stderr, "%s: ", progname);
		perror(start_path);
		exit(1);
	}
	if ((st.st_mode & S_IFMT) != S_IFDIR) {
		fprintf(stderr, "%s: START_PATH must be directory\n",
		   progname);
		exit(1);
	}
	if (start_path[strlen(start_path) - 1] != '/')
		strcat(start_path, "/");
	
	if (stat(end_path, &st) < 0) {
		fprintf(stderr, "%s: ", progname);
		perror(end_path);
		exit(1);
	}
	if ((st.st_mode & S_IFMT) == S_IFDIR
	  && end_path[strlen(end_path) - 1] != '/')
		strcat(end_path, "/");

	/* strip common prefix */
	i = 0;
	last_elem = 0;
	while (start_path[i] && start_path[i] == end_path[i]) {
		if (start_path[i] == '/')
			last_elem = i + 1;
		i += 1;
	}
	prev_path = 0;
	for (i = last_elem; start_path[i]; i += 1) {
		if (start_path[i] == '/') {
			if (prev_path)
				putchar('/');
			printf("%s", "..");
			prev_path = 1;
		}
	}
	if (end_path[last_elem]) {
		if (prev_path)
			putchar('/');
		prev_path = 1;
		while (end_path[strlen(end_path) - 1] == '/')
			end_path[strlen(end_path) - 1] = '\0';
		printf("%s", &end_path[last_elem]);
	}
	if (! prev_path)
		putchar('.');
	putchar('\n');
	exit(0);
}

static int
is_prefix(const char *path1, const char *path2)
{
	while (*path1 && *path1 == *path2) {
		path1 += 1;
		path2 += 1;
	}
	return (*path1 == '\0' && (*path2 == '/' || *path2 == '\0'));
}

static char *
abspath(const char *opath, char *absbuf)
{
	struct stat st;
	char curdir[MAXPATHLEN+1];
	char symlink[MAXPATHLEN+1];
	char path[MAXPATHLEN+1];
	char file[MAXPATHLEN+1];
	char *cp;
	int cc;

	strcpy(path, opath);
	/*
	 * resolve last element of path until we know it's not
	 * a symbolic link
	 */
	while (lstat(path, &st) >= 0
	    && (st.st_mode & S_IFMT) == S_IFLNK
	    && (cc = readlink(path, symlink, sizeof(symlink)-1)) > 0) {
		symlink[cc] = '\0';
		if ((cp = rindex(path, '/')) != NULL && symlink[0] != '/')
			*++cp = '\0';
		else
			path[0] = '\0';
		strcat(path, symlink);
	}
	/*
	 * We cheat a little bit here and let getwd() do the
	 * dirty work of resolving everything before the last
	 * element of the path
	 */
	if (getwd(curdir) == NULL) {
		fprintf(stderr, "%s: %s\n", progname, curdir);
		exit(1);
	}
	if ((st.st_mode & S_IFMT) == S_IFDIR) {
		if (chdir(path) < 0) {
			fprintf(stderr, "%s: ", progname);
			perror(path);
			exit(1);
		}
		if (getwd(absbuf) == NULL) {
			fprintf(stderr, "%s: %s\n", progname, absbuf);
			exit(1);
		}
		if (chdir(curdir) < 0) {
			fprintf(stderr, "%s: ", progname);
			perror(path);
			exit(1);
		}
		return absbuf;
	}
	if ((cp = rindex(path, '/')) == NULL) {
		/*
		 * last element of path is only element and it
		 * now not a symbolic link, so we're done
		 */
		strcpy(absbuf, curdir);
		if (absbuf[strlen(absbuf) - 1] != '/')
			strcat(absbuf, "/");
		return strcat(absbuf, path);
	}
	*cp++ = 0;
	strcpy(file, cp); /* save last element */

	if (chdir(path) < 0) {
		fprintf(stderr, "%s: ", progname);
		perror(path);
		exit(1);
	}
	if (getwd(absbuf) == NULL) {
		fprintf(stderr, "%s: %s\n", progname, absbuf);
		exit(1);
	}

	if (chdir(curdir) < 0) {
		fprintf(stderr, "%s: ", progname);
		perror(path);
		exit(1);
	}
	if (absbuf[strlen(absbuf)-1] != '/')
		strcat(absbuf, "/");
	return strcat(absbuf, file);
}
