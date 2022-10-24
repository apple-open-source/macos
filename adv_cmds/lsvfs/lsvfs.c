/*
 * lsvfs - list loaded VFSes
 * Garrett A. Wollman, September 1994
 * This file is in the public domain.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/sysctl.h>

#include <err.h>
#ifdef __APPLE__
#include <errno.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
/* Removes the legacy Num field. */
#define FMT "%-32.32s %5d %s\n"
#define HDRFMT "%-32.32s %5.5s %s\n"
#define DASHES "-------------------------------- ----- ---------------\n"
#ifndef nitems
#define	nitems(x)	(sizeof(x) / sizeof((x)[0]))
#endif
#else
#define FMT	"%-32.32s 0x%08x %5d  %s\n"
#define HDRFMT	"%-32.32s %10s %5.5s  %s\n"
#define DASHES	"-------------------------------- "	\
		"---------- -----  ---------------\n"
#endif

static struct flaglist {
	int		flag;
	const char	str[32]; /* must be longer than the longest one. */
} fl[] = {
#ifdef __APPLE__
	{ .flag = MNT_LOCAL, .str = "local", },
	{ .flag = MNT_DOVOLFS, .str = "dovolfs" },
#else
	{ .flag = VFCF_STATIC, .str = "static", },
	{ .flag = VFCF_NETWORK, .str = "network", },
	{ .flag = VFCF_READONLY, .str = "read-only", },
	{ .flag = VFCF_SYNTHETIC, .str = "synthetic", },
	{ .flag = VFCF_LOOPBACK, .str = "loopback", },
	{ .flag = VFCF_UNICODE, .str = "unicode", },
	{ .flag = VFCF_JAIL, .str = "jail", },
	{ .flag = VFCF_DELEGADMIN, .str = "delegated-administration", },
#endif
};

static const char *fmt_flags(int);

int
main(int argc, char **argv)
{
#ifdef __APPLE__
	struct vfsconf vfc;
#else
	struct xvfsconf vfc, *xvfsp;
#endif
	size_t buflen;
#ifdef __APPLE__
	int mib[4], max, rv = 0, x;
#else
	int cnt, i, rv = 0;
#endif

	argc--, argv++;

#ifdef __APPLE__
	printf(HDRFMT, "Filesystem", "Refs", "Flags");
#else
	printf(HDRFMT, "Filesystem", "Num", "Refs", "Flags");
#endif
	fputs(DASHES, stdout);

	if (argc > 0) {
		for (; argc > 0; argc--, argv++) {
			if (getvfsbyname(*argv, &vfc) == 0) {
#ifdef __APPLE__
				printf(FMT, vfc.vfc_name, vfc.vfc_refcount,
				    fmt_flags(vfc.vfc_flags));
#else
				printf(FMT, vfc.vfc_name, vfc.vfc_typenum,
				    vfc.vfc_refcount, fmt_flags(vfc.vfc_flags));
#endif
			} else {
				warnx("VFS %s unknown or not loaded", *argv);
				rv++;
			}
		}
	} else {
#ifdef __APPLE__
		buflen = sizeof(int);
		if (sysctlbyname("vfs.generic.maxtypenum", &max, &buflen,
		    NULL, 0) != 0)
			err(1, "sysctl(vfs.generic.maxtypenum)");

		mib[0] = CTL_VFS;
		mib[1] = VFS_GENERIC;
		mib[2] = VFS_CONF;

		buflen = sizeof(vfc);
		for (x = 0; x < max; x++) {
			mib[3] = x;
			if (sysctl(mib, 4, &vfc, &buflen, NULL, 0) != 0) {
				if (errno != ENOTSUP)
					errx(1, "sysctl");
			} else {
				printf(FMT, vfc.vfc_name, vfc.vfc_refcount,
				    fmt_flags(vfc.vfc_flags));
			}
		}
#else
		if (sysctlbyname("vfs.conflist", NULL, &buflen, NULL, 0) < 0)
			err(1, "sysctl(vfs.conflist)");
		xvfsp = malloc(buflen);
		if (xvfsp == NULL)
			errx(1, "malloc failed");
		if (sysctlbyname("vfs.conflist", xvfsp, &buflen, NULL, 0) < 0)
			err(1, "sysctl(vfs.conflist)");
		cnt = buflen / sizeof(struct xvfsconf);

		for (i = 0; i < cnt; i++) {
			printf(FMT, xvfsp[i].vfc_name, xvfsp[i].vfc_typenum,
			    xvfsp[i].vfc_refcount,
			    fmt_flags(xvfsp[i].vfc_flags));
		}
		free(xvfsp);
#endif
	}

	return (rv);
}

static const char *
fmt_flags(int flags)
{
	static char buf[sizeof(struct flaglist) * sizeof(fl)];
	int i;

	buf[0] = '\0';
	for (i = 0; i < (int)nitems(fl); i++) {
		if ((flags & fl[i].flag) != 0) {
			strlcat(buf, fl[i].str, sizeof(buf));
			strlcat(buf, ", ", sizeof(buf));
		}
	}

	/* Zap the trailing comma + space. */
	if (buf[0] != '\0')
		buf[strlen(buf) - 2] = '\0';
	return (buf);
}
