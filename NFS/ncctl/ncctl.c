/*
 * Copyright (c) 2014 Apple Inc. All rights reserved.
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
#define _DARWIN_FEATURE_64_BIT_INODE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include <getopt.h>
#include <sys/attr.h>
#include <sys/errno.h>
#include <sys/mount.h>
#include <System/sys/fsctl.h>
#include <System/gssd/gssd_mach_types.h>
#include <Heimdal/krb5.h>
#include <nfs/nfs_ioctl.h>

typedef int (*command)(const char *, uint32_t, void *);

int verbose = 0;

int
nfs_cred_destroy(const char *path, uint32_t flags, void *arg __unused)
{
	int error;

	if (path == NULL)
		return (EINVAL);

	error = fsctl(path, NFS_IOC_DESTROY_CRED, NULL, flags);
	if (error) {
		switch (errno) {
		case ENOTSUP:
			if (verbose)
				printf("%-48s: NFS mount is not using kerberos.\n", path);
			break;
		case ENOTTY:
			if (verbose)
				printf("%-48s isn't in an NFS file system.\n", path);
			break;
		default:
			warn("Could not unset credentials for %s.", path);
			break;
		}
	} else {
		if (verbose)
			printf("%-48s: Credentials unset.\n", path);
	}

	return (error);
}

int
nfs_principal_set(const char *path, uint32_t flags, void *arg)
{
	int error;
	struct nfs_gss_principal *p = (struct nfs_gss_principal *)arg;

	if (path == NULL)
		return (EINVAL);

	error = fsctl(path, NFS_IOC_SET_CRED, arg, flags);
	if (error) {
		switch (errno) {
		case ENOTSUP:
			if (verbose)
				printf("%-48s: NFS mount is not using Kerberos.\n", path);
			break;
		case ENOTTY:
			if (verbose)
				printf("%-48s: isn't in an NFS file system.\n", path);
			break;
		default:
			warn("Could not set principal %s on %s.", (char *)p->principal, path);
			break;
		}
	} else {
		if (verbose)
			printf("%-48s: Credentials set for %s\n", path, (char *)p->principal);
	}

	return (error);
}

#define NFS_PRINCIPAL_MAX_SIZE 128
uint8_t principal_buffer[NFS_PRINCIPAL_MAX_SIZE];

int
nfs_principal_get(const char *path, uint32_t flags, void *arg __unused)
{
	int error;
	struct nfs_gss_principal p;

	if (path == NULL)
		return (EINVAL);

	p.principal = principal_buffer;
	p.nametype = 0;
	p.princlen = NFS_PRINCIPAL_MAX_SIZE;
	p.flags = 0;

	error = fsctl(path, NFS_IOC_GET_CRED, &p, flags);
	if (error) {
		switch (errno) {
		case ENOTSUP:
			if (verbose)
				printf("%-48s: NFS mount is not using Kerberos.\n", path);
			break;
		case ENOTTY:
			if (verbose)
				printf("%-48s isn't in an NFS file system.\n", path);
			break;
		default:
			warn("Could not get principal on %s.", path);
			break;
		}
	} else {
		const char *kin = (p.flags & NFS_IOC_INVALID_CRED_FLAG) ? "[kinit needed]" : "";
		int pricelen = (int)p.princlen; // Principal max size is 128.

		if (p.nametype) {
			printf("%-48s: %.*s %s\n", path, pricelen, p.principal, kin);
		} else {
			if (p.flags & NFS_IOC_NO_CRED_FLAG)
				printf("%-48s: %s\n", path, "Credentials are not set.");
			else if (p.princlen)
				printf("%-48s: Default credential [from %.*s] %s\n",
				       path, pricelen, p.principal, kin);
			else
				printf("%-48s: Default credential %s\n", path, kin);
		}
		if (pricelen > NFS_PRINCIPAL_MAX_SIZE)
			printf("\t\tPrincipal name is trunctated by %d bytes.\n",
			       pricelen - NFS_PRINCIPAL_MAX_SIZE);
	}

	return (error);
}

int
do_command(int argc, char *argv[], command cmd, uint32_t flags, void *arg)
{
	int count, i;
	int return_status = 0;
	struct statfs *mounts;
	int error;

	for (i = 0; i < argc; i++) {
		if (verbose > 1) {
			printf("Acting on credentials associated with %s.\n", argv[i]);
		}
		error = cmd(argv[i], flags, arg);
		if (error)
			return_status = 1;
	}

	if (i == 0) {
		count = getmntinfo(&mounts, MNT_NOWAIT);
		if (count == 0)
			err(1, "getmntinfo failed");

		for (i = 0; i < count; i++) {
			if (strcmp(mounts[i].f_fstypename, "nfs") == 0) {
				if (verbose > 1)
					printf("Acting on credentials mounted on %s\n", mounts[i].f_mntonname);
				error = cmd(mounts[i].f_mntonname, 0, arg);
				if (error) {
					return_status = 1;
				}
			}
		}
	}

	return (return_status);
}

int
check_principal(const char **principal, uint32_t nt __unused)
{
	krb5_error_code ret;
	krb5_context context;
	krb5_ccache  ccache;
	krb5_principal kprincipal;
	char *retprinc;
	int error = -1;

	ret = krb5_init_context (&context);
	if (verbose) {
		if (ret == KRB5_CONFIG_BADFORMAT)
			warnx("krb5_init_context failed to parse configuration file");
		else if (ret)
			warnx("krb5_init_context failed: %d", ret);
	}
	if (ret) {
		warnx("Is kerberose configured?");
		return (-1);
	}
	ret = krb5_parse_name(context, *principal, &kprincipal);
	if (ret) {
		if (verbose)
			krb5_warn (context, ret, "krb5_parse_name");
		goto done;
	}

	ret = krb5_unparse_name(context, kprincipal, &retprinc);
	if (ret)
		krb5_warn(context, ret, "krb5_unparse_name");
	else {
		if (verbose > 1 && strcmp(retprinc, *principal) != 0)
			warnx("Found kerberos principal %s from %s",
			      retprinc, *principal);
		*principal = retprinc;
	}
	ret = krb5_cc_cache_match(context, kprincipal, &ccache);
	if (!ret) {
		krb5_cc_close(context, ccache);
	} else {
		if (verbose)
			krb5_warn(context, ret, "Could not find cache for principal %s", *principal);
		goto done;
	}

	error = 0;
done:
	krb5_free_principal(context, kprincipal);
	krb5_free_context(context);

	return error;
}

const char *progname;

/* options descriptor */
static struct option ncinitopts[] = {
	{ "help",	no_argument,		NULL,		'h' },
	{ "force",	no_argument,            NULL,           'F' },
	{ "principal",	required_argument,      NULL,           'p' },
	{ "verbose",	no_argument,            NULL,		'v' },
	{ "nofollow",	no_argument,		NULL,		'P' },
	{ NULL,         0,                      NULL,           0 }
};

struct help_opt {
	const char opt;
	const char *lopt;
	const char *msg;
} opts[] = {
	{ 'v', "verbose", "Verbose output." },
	{ 'P', "nofollow", "If a path argument is given that is a symbolic link, do not follow\n\t\t\t  the link, but use the link itself to determine the mount point." },
	{ '\0', NULL, NULL }
};

struct help_opt init_opts[] = {
	{ 'F', "force", "Set the principal even if it does not exist in the kerberos cache collection." },
	{ 'p', "principal", "Set the given principal --principal[=]<principal>\n\t\t\t  on the mounts specified. This option is required." },
	{ '\0', NULL, NULL }
};

#define MAX_ALIASES 5
struct help_cmd {
	const char *cmds[MAX_ALIASES];
	struct help_opt *opts;
	const char *msg;
} cmd_help[] = {
	{ { "list", "get", NULL }, NULL, "List the principals on the specified mounts for this audit session." },
	{ { "unset", "destroy", NULL }, NULL, "Unset the principals on the specified mounts for this audit session." },
	{ { "set", "init", NULL }, init_opts, "Set the suplied principal on the specified mounts for this audit session." },
	{ { NULL }, NULL }
};

#define CMD_FIELD_WIDTH 24
void
usage(const char *prog)
{
	struct help_opt *hop;
	struct help_cmd *hcp;
	const char **alias;
	int cnt;

	if (strncmp(prog, "ncdestroy", 10) == 0 || strncmp(prog, "nclist", 7) == 0)
		fprintf(stderr, "%s [ {-v | --verbose } { -P | --nofollow } ] [path ...]\n", prog);
	else if (strncmp(prog, "ncinit", 7) == 0)
		fprintf(stderr, "%s [ {-v | --verbose } { -P | --nofollow } ] { -p | --principal[=] } principal [path ...]\n", prog);
	else {
		fprintf(stderr, "%s [ options ] command [ command_options] [ path ...]\n", prog);
		fprintf(stderr, "where options are:\n");
		for (hop = opts; hop->opt; hop++)
			printf("\t-%c, --%-10s: %s\n", hop->opt, hop->lopt, hop->msg);
		fprintf(stderr, "where commands are:\n");
		for (hcp = cmd_help; hcp->cmds[0]; hcp++) {
			cnt = 0;
			for (alias = hcp->cmds; *alias; alias++)
				cnt += fprintf(stderr, "%s%s", alias != hcp->cmds ? " | " : "", *alias);
			cnt = cnt < CMD_FIELD_WIDTH ? CMD_FIELD_WIDTH - cnt : 0;
			fprintf(stderr, "%*c: %s", cnt, ' ', hcp->msg);
			if (hcp->opts) {
				fprintf(stderr, " Options:\n");
				for (hop = hcp->opts; hop->opt; hop++)
					fprintf(stderr, "\t-%c, --%-10s: %s\n", hop->opt, hop->lopt, hop->msg);
				fprintf(stderr, "\n");
			} else {
				fprintf(stderr, " Takes no options.\n");
			}
		}
	}
	fprintf(stderr, "\tIf no paths are specifed, act on all currently mounted NFS file systems.\n");
	exit(1);
}

int force = 0;
const char *pname = NULL;

int
main_ncinit(int argc, char *argv[], uint32_t flags)
{
	struct nfs_gss_principal gprincipal;
	const char *type = "Kerberos";

	gprincipal.nametype = GSSD_KRB5_PRINCIPAL;
	if (pname == NULL)
		errx(1, "You must specify a principal to set");
	if (!force)
		if (check_principal(&pname, gprincipal.nametype))
			errx(1, "Principal %s is not known. kinit or use --force to override.\n", pname);
	if (verbose)
		printf("Setting %s type principal %s\n", type, pname);

	gprincipal.principal = (uint8_t *)pname;
	gprincipal.princlen = pname ? (uint32_t) strlen(pname) : 0;
	gprincipal.flags = 0;
	if (gprincipal.princlen == 0 || gprincipal.princlen > MAXPATHLEN)
		errx(1, "Principal is invalid");

	return (do_command(argc, argv, nfs_principal_set, flags, &gprincipal));
}

struct cmdent {
	const char *name;
	command cmd;
} cmdtbl[] = {
	{ "list",	nfs_principal_get },
	{ "destroy",	nfs_cred_destroy  },
	{ "unset",	nfs_cred_destroy  },
	{ "set",	nfs_principal_set },
	{ "init",	nfs_principal_set },
	{ NULL,		NULL }
};

command
find_command(const char* cname)
{
	struct cmdent *cp;

	if (strncmp(cname, "nc", 2) == 0)
		cname += 2;
	for (cp = cmdtbl; cp->name; cp++)
		if (strcmp(cp->name, cname) == 0)
			return (cp->cmd);
	return (NULL);
}

int
main(int argc, char *argv[])
{
	command cmd = (command)NULL;
	int opt;
	uint32_t flags = 0;

	progname = getprogname();

	while ((opt = getopt_long(argc, argv, "hFPvp:", ncinitopts, NULL)) != -1) {
		switch (opt) {
		case 'F':
			force = 1;
			break;
		case 'P':
			flags = FSOPT_NOFOLLOW;
			break;
		case 'v':
			verbose++;
			break;
		case 'p':
			pname = optarg;
			break;
		case 'h':
		default:
			usage(progname);
		}
	}
	argc -= optind;
	argv += optind;

	if (argv[0]) {
		cmd = find_command(argv[0]);
		if (cmd) {
			argv++;
			argc--;
		}
	}

	if (cmd == (command)NULL)
		cmd = find_command(progname);
	if (cmd == (command)NULL) {
		struct statfs fsbuf;
		if (argv[0]  && statfs(argv[0], &fsbuf))
			warnx("%s bad command.", argv[0]);
		else
			warnx("No command specified.");
		usage(progname);
	}

	if (cmd == nfs_cred_destroy || cmd == nfs_principal_get) {
		if (pname || force)
			usage(progname);
		return (do_command(argc, argv, cmd, flags, NULL));
	} else if (cmd == nfs_principal_set) {
		return (main_ncinit(argc, argv, flags));
	}

	usage(progname);
}
