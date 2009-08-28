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
#define INFO_FILE 1
#define INFO_NIS 2
#define INFO_OPEN_DIRECTORY 3
#define INFO_PAM 4

#ifndef __SLICK__
#define _PASSWD_FILE "/etc/master.passwd"
#else
#define _PASSWD_FILE "/etc/passwd"
#endif

#include <stdio.h>
#include <errno.h>
#include <pwd.h>
#include <libc.h>
#include <ctype.h>
#include <string.h>
#include <pwd.h>
#include "stringops.h"

#ifdef __SLICK__
#define _PASSWORD_LEN 8
#endif

const char* progname = "chkpasswd";

static int literal = 0;

extern int file_check_passwd(char *, char *);
extern int nis_check_passwd(char *, char *);
extern int od_check_passwd(char *, char *);
extern int pam_check_passwd(char *);

void
checkpasswd(char *name, char *old_pw)
{
	int isNull;
	char *p;

	printf("Checking password for %s.\n", name);

	p = "";
	isNull = 0;
	if (old_pw == NULL) isNull = 1;
	if ((isNull == 0) && (old_pw[0] == '\0')) isNull = 1;
	if (isNull == 0)
	{
		p = getpass("Password:");
		sleep(1); // make sure this doesn't go too quickly
		if (strcmp(literal ? p : crypt(p, old_pw), old_pw))
		{
			errno = EACCES;
			fprintf(stderr, "Sorry\n");
			exit(1);
		}
	}
	return;
}

void
usage()
{
	fprintf(stderr, "usage: chkpasswd [-i infosystem] [-l location] [-c] [name]\n");
	fprintf(stderr, "  infosystem:\n");
	fprintf(stderr, "    file\n");
	fprintf(stderr, "    NIS\n");
	fprintf(stderr, "    OpenDirectory\n");
	fprintf(stderr, "  location (for infosystem):\n");
	fprintf(stderr, "    file           location is path to file (default is %s)\n", _PASSWD_FILE);
	fprintf(stderr, "    NIS            location is NIS domain name\n");
	fprintf(stderr, "    OpenDirectory  location is directory node name\n");
	fprintf(stderr, "  -c: supplied password is compared verbatim without first\n");
	fprintf(stderr, "      being crypted\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	char* user = NULL;
	char* locn = NULL;
	int infosystem, ch;

	infosystem = INFO_PAM;

	while ((ch = getopt(argc, argv, "ci:l:")) != -1) {
		switch(ch) {
		case 'i':
			if (!strcasecmp(optarg, "file")) {
				infosystem = INFO_FILE;
			} else if (!strcasecmp(optarg, "NIS")) {
				infosystem = INFO_NIS;
			} else if (!strcasecmp(optarg, "YP")) {
				infosystem = INFO_NIS;
			} else if (!strcasecmp(optarg, "opendirectory")) {
				infosystem = INFO_OPEN_DIRECTORY;
			} else if (!strcasecmp(optarg, "PAM")) {
				infosystem = INFO_PAM;
			} else {
				fprintf(stderr, "%s: Unknown info system \'%s\'.\n",
					progname, optarg);
				usage();
			}
			break;
		case 'l':
			locn = optarg;
			break;
		case 'c':
			literal++;
			break;
		case '?':
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;
		
	if (argc > 1) {
		usage();
	} else if (argc == 1) {
		user = argv[0];
	}

	if (user == NULL) {
		struct passwd* pw = getpwuid(getuid());
		if (pw != NULL && pw->pw_name != NULL) {
			user = strdup(pw->pw_name);
		}
		if (user == NULL) {
			fprintf(stderr, "you don't have a login name\n");
			exit(1);
		}
	}
	
	switch (infosystem)
	{
		case INFO_FILE:
			file_check_passwd(user, locn);
			break;
		case INFO_NIS:
			nis_check_passwd(user, locn);
			break;
		case INFO_OPEN_DIRECTORY:
			od_check_passwd(user, locn);
			break;
		case INFO_PAM:
			pam_check_passwd(user);
			break;
	}

	exit(0);
}

