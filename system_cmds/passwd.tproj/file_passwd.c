/*
 * Copyright (c) 1999-2010 Apple Inc. All rights reserved.
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
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include "passwd.h"

#define _PASSWD_FILE "/etc/master.passwd"
#define _COMPAT_FILE "/etc/passwd"
#define _PASSWD_FIELDS 10
#define BUFSIZE 8192

void getpasswd(char *, int, int, int, int, char *, char **, char**, char **);

static struct passwd *
parse_user(char *line, size_t len)
{
	static struct passwd pw;
	int i,j;
	char *tokens[_PASSWD_FIELDS];
	char *token = NULL;
	bool comment = true;

	free(pw.pw_name);
	free(pw.pw_passwd);
	free(pw.pw_class);
	free(pw.pw_gecos);
	free(pw.pw_dir);
	free(pw.pw_shell);
	memset(&pw, 0, sizeof(pw));

	if (line == NULL) return NULL;

	memset(&tokens, 0, sizeof(char *) * _PASSWD_FIELDS);

	for (i = 0, j = 0; i < len && j < _PASSWD_FIELDS; ++i) {
		int c = line[i];
		if (!isspace(c) && c != '#') {
			comment = false;
		}
		if (!comment && token == NULL) {
			// start a new token
			token = &line[i];
		} else if (token && (c == ':' || c == '\n')) {
			// end the current token
			// special case for empty token
			while (token[0] == ':' && token < &line[i]) {
				tokens[j++] = strdup("");
				++token;
			}
			tokens[j++] = strndup(token, &line[i] - token);
			token = NULL;
		}
	}

	if (comment || j != _PASSWD_FIELDS) return NULL;

	j = 0;
	pw.pw_name = tokens[j++];
	pw.pw_passwd = tokens[j++];
	pw.pw_uid = atoi(tokens[j]);
	free(tokens[j++]);
	pw.pw_gid = atoi(tokens[j]);
	free(tokens[j++]);
	pw.pw_class = tokens[j++];
	pw.pw_change = atoi(tokens[j]);
	free(tokens[j++]);
	pw.pw_expire = atoi(tokens[j]);
	free(tokens[j++]);
	pw.pw_gecos = tokens[j++];
	pw.pw_dir = tokens[j++];
	pw.pw_shell = tokens[j++];

	return &pw;
}

static struct passwd *
find_user(FILE *fp, char *uname)
{
	size_t len;
	char *line;

	rewind(fp);

	while ((line = fgetln(fp, &len)) != NULL) {
		struct passwd *pw = parse_user(line, len);
		if (pw && strcmp(uname, pw->pw_name) == 0) {
			return pw;
		}
	}
	return NULL;
}

static void
rewrite_file(char *path, FILE *fp, struct passwd *newpw)
{
	int fd;
	char *line;
	size_t len;
	FILE *tfp = NULL;
	char *tempname = NULL; // temporary master.passwd file

	asprintf(&tempname, "%s.XXXXXX", path);
	
	fd = mkstemp(tempname);
	if (fd == -1) {
		err(EXIT_FAILURE, "%s", tempname);
	}
	tfp = fdopen(fd, "w+");
	if (tfp == NULL || fchmod(fd, S_IRUSR | S_IWUSR) != 0) {
		int save = errno;
		unlink(tempname);
		errno = save;
		err(EXIT_FAILURE, "%s", tempname);
	}
	
	while ((line = fgetln(fp, &len)) != NULL) {
		struct passwd *pw = parse_user(line, len);

		// if this is not the entry we're looking for or if parsing
		// failed (likely a comment) then print the entry as is.
		if (pw == NULL || strcmp(newpw->pw_name, pw->pw_name) != 0) {
			fwrite(line, sizeof(char), len, tfp);
		} else {
			fprintf(tfp, "%s:%s:%d:%d:%s:%ld:%ld:%s:%s:%s\n",
				newpw->pw_name,
				newpw->pw_passwd,
				newpw->pw_uid,
				newpw->pw_gid,
				newpw->pw_class,
				newpw->pw_change,
				newpw->pw_expire,
				newpw->pw_gecos,
				newpw->pw_dir,
				newpw->pw_shell);
		}
	}

	// Move the temporary file into place.
	if (fclose(tfp) != 0 || rename(tempname, path) != 0) {
		int save = errno;
		unlink(tempname);
		errno = save;
		err(EXIT_FAILURE, "%s", tempname);
	}

	free(tempname);
}

int
file_passwd(char *uname, char *locn)
{
	char *ne, *oc, *nc;
	int fd;
	FILE *fp;
	uid_t uid;
	char *fname;
	struct passwd *pw;
	struct passwd newpw;
	
	fname = _PASSWD_FILE;
	if (locn != NULL) fname = locn;
	
	fd = open(fname, O_RDONLY | O_EXLOCK);
	if (fd == -1) {
		err(EXIT_FAILURE, "%s", fname);
	}

	fp = fdopen(fd, "r");
	if (fp == NULL) {
		err(EXIT_FAILURE, "%s", fname);
	}

	pw = find_user(fp, uname);
	if (pw == NULL) {
		errx(EXIT_FAILURE, "user %s not found in %s", uname, fname);
	}

	uid = getuid();
	if (uid != 0 && uid != pw->pw_uid) {
		errno = EACCES;
		err(EXIT_FAILURE, "%s", uname);
	}

	// Get the password
	getpasswd(uname, (uid == 0), 5, 0, 0, pw->pw_passwd, &ne, &oc, &nc);

	newpw.pw_name = strdup(pw->pw_name);
	newpw.pw_passwd = strdup(ne);
	newpw.pw_uid = pw->pw_uid;
	newpw.pw_gid = pw->pw_gid;
	newpw.pw_class = strdup(pw->pw_class);
	newpw.pw_change = pw->pw_change;
	newpw.pw_expire = pw->pw_expire;
	newpw.pw_gecos = strdup(pw->pw_gecos);
	newpw.pw_dir = strdup(pw->pw_dir);
	newpw.pw_shell = strdup(pw->pw_shell);

	// Rewrite the file
	rewind(fp);
	rewrite_file(fname, fp, &newpw);

	fclose(fp);

	return 0;
}
