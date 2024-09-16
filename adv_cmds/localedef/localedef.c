/*-
 * Copyright 2018 Nexenta Systems, Inc.
 * Copyright 2015 John Marino <draco@marino.st>
 *
 * This source code is derived from the illumos localedef command, and
 * provided under BSD-style license terms by Nexenta Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * POSIX localedef.
 */
#include <sys/cdefs.h>

#ifdef __APPLE__
#include <machine/endian.h>
#include <sys/fcntl.h>
#else
#include <sys/endian.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>

#ifdef __APPLE__
#include <sys/wait.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <paths.h>	/* _PATH_LOCALE */
#include <spawn.h>
#include <stdbool.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <stddef.h>
#include <unistd.h>
#include <limits.h>
#include <locale.h>
#include <dirent.h>
#include "collate.h"
#include "localedef.h"
#include "parser.h"

#ifndef	TEXT_DOMAIN
#define	TEXT_DOMAIN	"SYS_TEST"
#endif

#ifdef __APPLE__
#include <libkern/OSByteOrder.h>

#define	htobe32(x)	OSSwapHostToBigInt32(x)
#define	htole32(x)	OSSwapHostToLittleInt32(x)
#endif

#ifdef __APPLE__
int bsd = 0;
#else
static int bsd = 0;
#endif
static int byteorder = 0;
int verbose = 0;
int undefok = 0;
int warnok = 0;
static char *locname = NULL;
#ifdef __APPLE__
static char rootpath[PATH_MAX];
#endif
static char locpath[PATH_MAX];
char *version = NULL;

const char *
category_name(void)
{
	switch (get_category()) {
	case T_CHARMAP:
		return ("CHARMAP");
	case T_WIDTH:
		return ("WIDTH");
	case T_COLLATE:
		return ("LC_COLLATE");
	case T_CTYPE:
		return ("LC_CTYPE");
	case T_MESSAGES:
		return ("LC_MESSAGES");
	case T_MONETARY:
		return ("LC_MONETARY");
	case T_NUMERIC:
		return ("LC_NUMERIC");
	case T_TIME:
		return ("LC_TIME");
	default:
		INTERR;
		return (NULL);
	}
}

static char *
category_file(void)
{
	if (bsd)
		(void) snprintf(locpath, sizeof (locpath), "%s.%s",
		    locname, category_name());
	else
#ifdef __APPLE__
	{
		const char *catname = category_name();

		/*
		 * macOS hasn't switched to accepting the immediate LC_MESSAGES
		 * as a file, so continue creating it as a directory.
		 */
		if (strcmp(catname, "LC_MESSAGES") == 0) {
			(void) snprintf(locpath, sizeof (locpath),
			    "%s%s/LC_MESSAGES/%s", rootpath, locname, catname);
		} else {
			(void) snprintf(locpath, sizeof (locpath), "%s%s/%s",
			    rootpath, locname, catname);
		}
	}
#else
		(void) snprintf(locpath, sizeof (locpath), "%s/%s",
		    locname, category_name());
#endif
	return (locpath);
}

FILE *
open_category(void)
{
	FILE *file;

	if (verbose) {
		(void) printf("Writing category %s: ", category_name());
		(void) fflush(stdout);
	}

	/* make the parent directory */
	if (!bsd)
		(void) mkdir(dirname(category_file()), 0755);

	/*
	 * note that we have to regenerate the file name, as dirname
	 * clobbered it.
	 */
	file = fopen(category_file(), "w");
	if (file == NULL) {
		errf("%s", strerror(errno));
		return (NULL);
	}
	return (file);
}

void
close_category(FILE *f)
{
#ifdef __APPLE__
	int serrno;
#endif
	if (fchmod(fileno(f), 0644) < 0) {
#ifdef __APPLE__
		serrno = errno;
#endif
		(void) fclose(f);
		(void) unlink(category_file());
#ifdef __APPLE__
		errf("%s", strerror(serrno));
#else
#endif
		errf("%s", strerror(errno));
	}
	if (fclose(f) < 0) {
#ifdef __APPLE__
		serrno = errno;
#endif
		(void) unlink(category_file());
#ifdef __APPLE__
		errf("%s", strerror(serrno));
#else
		errf("%s", strerror(errno));
#endif
	}
	if (verbose) {
		(void) fprintf(stdout, "done.\n");
		(void) fflush(stdout);
	}
}

#ifdef __APPLE__
static const char *kw_noquotes[] = {
	"grouping",
	"mon_grouping",
};

static void
inject_category_line(char *line, int linelen)
{
	const char *check_kw;
	char *kwend;

	kwend = strchr(line, ' ');
	if (kwend == NULL)
		goto enqueue;

	*kwend = '\0';
	for (size_t i = 0; i < nitems(kw_noquotes); i++) {
		check_kw = kw_noquotes[i];

		if (strcmp(line, check_kw) == 0) {
			/* line is still NUL terminated after linelen */
			char *cp = kwend + 1, *ep = &line[linelen - 1];

			while (isspace(*cp) && cp < ep)
				cp++;

			if (*cp == '"')
				*cp = ' ';

			cp = ep;
			while (isspace(*cp) && cp > kwend)
				cp--;

			if (*cp == '"')
				*cp = ' ';
		}
	}

enqueue:
	/*
	 * The above may have transformed our space into a nul byte for quick
	 * and dirty comparisons; send it back.
	 */
	if (kwend != NULL)
		*kwend = ' ';
	scan_enqueue(line, linelen);
}

static int
inject_category_exec(char * const * argv, char * const * envp)
{
	FILE *rpipe;
	char *line;
	posix_spawn_file_actions_t fa;
	posix_spawnattr_t sa;
	size_t linecap;
	ssize_t linelen;
	pid_t pid, wpid;
	int pfd[2], rv, status;

	rpipe = NULL;
	pfd[0] = pfd[1] = -1;
	pid = wpid = -1;

	if (pipe(pfd) == -1)
		return (errno);

	fa = NULL;
	sa = NULL;
	if ((rv = posix_spawn_file_actions_init(&fa)) != 0)
		goto cleanup;

	if ((rv = posix_spawn_file_actions_adddup2(&fa, pfd[1],
	    STDOUT_FILENO)) != 0)
		goto cleanup;

	if ((rv = posix_spawn_file_actions_addopen(&fa, STDIN_FILENO,
	    _PATH_DEVNULL, O_RDWR, 0644)) != 0)
		goto cleanup;

	if ((rv = posix_spawn_file_actions_adddup2(&fa, STDIN_FILENO,
	    STDERR_FILENO)) != 0)
		goto cleanup;

	if ((rv = posix_spawnattr_init(&sa)) != 0)
		goto cleanup;

	if ((rv = posix_spawnattr_setflags(&sa, POSIX_SPAWN_CLOEXEC_DEFAULT)) !=
	   0)
		goto cleanup;

	rv = posix_spawn(&pid, "/usr/bin/locale", &fa, &sa, argv, envp);

cleanup:
	/* First, close the write side */
	close(pfd[1]);
	pfd[1] = -1;
	/* Next, clean up attrs */
	if (sa != NULL)
		posix_spawnattr_destroy(&sa);
	if (fa != NULL)
		posix_spawn_file_actions_destroy(&fa);
	/* Finally, propagate the error. */
	if (rv != 0)
		goto out;

	/* Parent */
	if ((rpipe = fdopen(pfd[0], "r")) == NULL) {
		rv = errno;
		goto out;
	}

	pfd[0] = -1;	/* Now owned by rpipe */

	line = NULL;
	linecap = 0;

	while ((linelen = getline(&line, &linecap, rpipe)) > 0) {
		char *ep = &line[linelen - 1];

		for (char *sp = line; sp < ep; sp++) {
			if (*sp == '=')
				*sp = ' ';
		}

		inject_category_line(line, linelen);
	}

	free(line);
out:
	if (pfd[0] >= 0)
		close(pfd[0]);
	if (rpipe != NULL)
		fclose(rpipe);

	if (pid >= 0) {
		/* Reap the process on our way out. */
		while ((wpid = waitpid(pid, &status, 0)) == -1 &&
		    errno == EINTR) {
			/* Re-enter */
		}

		if (wpid == -1)
			rv = errno;
	}

	return (rv);
}

/* Inject the category from `src` locale to us. */
static int
inject_category(char *src)
{
	char *envvar;
	char *locale_argv[4] = { "locale", "-k", NULL, NULL };
	char *locale_envp[2] = { NULL, NULL };
	FILE *localef;
	int rv;

	/* XXX Need to check that we only allow the usual keywords in this case. */

	rv = EINVAL;
	envvar = NULL;

	/*
	 * Just set LC_ALL in the exec'ed environment, since locale(1) will need
	 * to call setlocale(3) anyways; no sense affecting the current process.
	 */
	if (asprintf(&envvar, "LC_ALL=%s", src) <= 0)
		return (ENOMEM);
	locale_envp[0] = envvar;

	locale_argv[2] = category_name();

	rv = inject_category_exec(locale_argv, locale_envp);
	free(envvar);
	return (rv);
}
#endif

/*
 * This function is used when copying the category from another
 * locale.  Note that the copy is actually performed using a hard
 * link for efficiency.
 */
void
copy_category(char *src)
{
	char	srcpath[PATH_MAX];
	int	rv;

	(void) snprintf(srcpath, sizeof (srcpath), "%s/%s",
	    src, category_name());
	rv = access(srcpath, R_OK);
	if ((rv != 0) && (strchr(srcpath, '/') == NULL)) {
		/* Maybe we should try the system locale */
#ifdef __APPLE__
		(void) snprintf(srcpath, sizeof (srcpath),
		    "%s/%s/%s", _PATH_LOCALE, src, category_name());
#else
		(void) snprintf(srcpath, sizeof (srcpath),
		    "/usr/lib/locale/%s/%s", src, category_name());
#endif
		rv = access(srcpath, R_OK);
	}

	if (rv != 0) {
#ifdef __APPLE__
		bool exists;

		exists = false;
		if (strcmp(src, "C") == 0 || strcmp(src, "POSIX") == 0 ||
		    strncmp(src, "C.", 2) == 0) {
			exists = true;
		} else {
			(void) snprintf(srcpath, sizeof (srcpath),
			    "%s/%s", _PATH_LOCALE, src);

			exists = access(srcpath, R_OK) == 0;
		}

		if (exists) {
			/*
			 * The locale exists, it's simply missing a component.
			 * We'll shell out to locale(1) to get the definition to
			 * inject.
			 */

			if (inject_category(src) == 0)
				return;
		}

#endif
		fprintf(stderr,"source locale data unavailable: %s\n", src);
		return;
	}

	if (verbose > 1) {
		(void) printf("Copying category %s from %s: ",
		    category_name(), src);
		(void) fflush(stdout);
	}

	/* make the parent directory */
	if (!bsd)
		(void) mkdir(dirname(category_file()), 0755);

	if (link(srcpath, category_file()) != 0) {
		fprintf(stderr,"unable to copy locale data: %s\n",
			strerror(errno));
		return;
	}
	if (verbose > 1) {
		(void) printf("done.\n");
	}
}

int
putl_category(const char *s, FILE *f)
{
#ifdef __APPLE__
	int serrno;
#endif
	if (s && fputs(s, f) == EOF) {
#ifdef __APPLE__
		serrno = errno;
#endif
		(void) fclose(f);
		(void) unlink(category_file());
#ifdef __APPLE__
		errf("%s", strerror(serrno));
#else
		errf("%s", strerror(errno));
#endif
		return (EOF);
	}
	if (fputc('\n', f) == EOF) {
#ifdef __APPLE__
		serrno = errno;
#endif
		(void) fclose(f);
		(void) unlink(category_file());
#ifdef __APPLE__
		errf("%s", strerror(serrno));
#else
		errf("%s", strerror(errno));
#endif
		return (EOF);
	}
	return (0);
}

int
wr_category(void *buf, size_t sz, FILE *f)
{
	if (!sz) {
		return (0);
	}
	if (fwrite(buf, sz, 1, f) < 1) {
#ifdef __APPLE__
		int serrno = errno;
#endif
		(void) fclose(f);
		(void) unlink(category_file());
#ifdef __APPLE__
		errf("%s", strerror(serrno));
#else
		errf("%s", strerror(errno));
#endif
		return (EOF);
	}
	return (0);
}

uint32_t
htote(uint32_t arg)
{

	if (byteorder == 4321)
		return (htobe32(arg));
	else if (byteorder == 1234)
		return (htole32(arg));
	else
		return (arg);
}

int yyparse(void);

static void
usage(void)
{
	(void) fprintf(stderr, "Usage: localedef [options] localename\n");
	(void) fprintf(stderr, "[options] are:\n");
	(void) fprintf(stderr, "  -D          : BSD-style output\n");
	(void) fprintf(stderr, "  -b          : big-endian output\n");
	(void) fprintf(stderr, "  -c          : ignore warnings\n");
	(void) fprintf(stderr, "  -l          : little-endian output\n");
	(void) fprintf(stderr, "  -v          : verbose output\n");
	(void) fprintf(stderr, "  -U          : ignore undefined symbols\n");
	(void) fprintf(stderr, "  -f charmap  : use given charmap file\n");
	(void) fprintf(stderr, "  -u encoding : assume encoding\n");
	(void) fprintf(stderr, "  -w widths   : use screen widths file\n");
	(void) fprintf(stderr, "  -i locsrc   : source file for locale\n");
	(void) fprintf(stderr, "  -V version  : version string for locale\n");
	exit(4);
}

int
main(int argc, char **argv)
{
	int c;
	char *lfname = NULL;
	char *cfname = NULL;
	char *wfname = NULL;
	DIR *dir;

	init_charmap();
	init_collate();
	init_ctype();
	init_messages();
	init_monetary();
	init_numeric();
	init_time();

#if YYDEBUG
	yydebug = 0;
#endif

	(void) setlocale(LC_ALL, "");

	while ((c = getopt(argc, argv, "blw:i:cf:u:vUDV:")) != -1) {
		switch (c) {
		case 'D':
			bsd = 1;
			break;
		case 'b':
		case 'l':
			if (byteorder != 0)
				usage();
			byteorder = c == 'b' ? 4321 : 1234;
			break;
		case 'v':
			verbose++;
			break;
		case 'i':
			lfname = optarg;
			break;
		case 'u':
			set_wide_encoding(optarg);
			break;
		case 'f':
			cfname = optarg;
			break;
		case 'U':
			undefok++;
			break;
		case 'c':
			warnok++;
			break;
		case 'w':
			wfname = optarg;
			break;
		case '?':
			usage();
			break;
		case 'V':
			version = optarg;
			break;
		}
	}

	if ((argc - 1) != (optind)) {
		usage();
	}
	locname = argv[argc - 1];
	if (verbose) {
		(void) printf("Processing locale %s.\n", locname);
	}

	if (version && strlen(version) >= XLOCALE_DEF_VERSION_LEN) {
		(void) fprintf(stderr, "Version string too long.\n");
		exit(1);
	}

	if (cfname) {
		if (verbose)
			(void) printf("Loading charmap %s.\n", cfname);
		reset_scanner(cfname);
		(void) yyparse();
	}

	if (wfname) {
		if (verbose)
			(void) printf("Loading widths %s.\n", wfname);
		reset_scanner(wfname);
		(void) yyparse();
	}

	if (verbose) {
		(void) printf("Loading POSIX portable characters.\n");
	}
	add_charmap_posix();

	if (lfname) {
		reset_scanner(lfname);
	} else {
		reset_scanner(NULL);
	}

	/* make the directory for the locale if not already present */
	if (!bsd) {
#ifdef __APPLE__
		char *cp;

		/*
		 * If there's not a slash in the locale name, it must be
		 * interpreted as a public locale.  We'll leave rootpath alone
		 * if locname was specified as a path, otherwise it'll be
		 * _PATH_LOCALE or some such path.
		 */
		cp = strchr(locname, '/');
		if (cp == NULL) {
			size_t sz;

			sz = snprintf(rootpath, sizeof(rootpath), "%s/",
			    _PATH_LOCALE);

			/* _PATH_LOCALE *must* fit within the limits. */
			assert(sz < sizeof(rootpath));
#ifdef NDEBUG
			(void)sz;
#endif

			if (snprintf(locpath, sizeof(locpath), "%s/%s",
			    rootpath, locname) >= sizeof(locpath))
				errf("locale name too long: %s", locname);

			while ((dir = opendir(locpath)) == NULL) {
				if ((errno != ENOENT) ||
				    (mkdir(locpath, 0755) <  0)) {
					errf("%s", strerror(errno));
				}
			}

			goto created;
		}
#endif

		while ((dir = opendir(locname)) == NULL) {
			if ((errno != ENOENT) ||
			    (mkdir(locname, 0755) <  0)) {
				errf("%s", strerror(errno));
			}
		}
#ifdef __APPLE__
created:
#endif
		(void) closedir(dir);

#ifndef __APPLE__
		/*
		 * It's unclear what this mkdir(dirname(category_file))
		 * below aims to do; the category doesn't seem like it should be
		 * set yet, and we created the locale dir above it.
		 */
		(void) mkdir(dirname(category_file()), 0755);
#endif
	}

	(void) yyparse();
#ifdef __APPLE__
	scan_done();
#endif
	if (verbose) {
		(void) printf("All done.\n");
	}
	return (warnings ? 1 : 0);
}
