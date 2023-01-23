/*-
 * Copyright 1986, Larry Wall
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following condition is met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this condition and the following disclaimer.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * patch - a program to apply diffs to original files
 *
 * -C option added in 1998, original code by Marc Espie, based on FreeBSD
 * behaviour
 *
 * $OpenBSD: util.c,v 1.35 2010/07/24 01:10:12 ray Exp $
 * $FreeBSD$
 */

#include <sys/stat.h>

#ifdef __APPLE__
#include <assert.h>
#endif
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <paths.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "util.h"
#include "backupfile.h"
#include "pathnames.h"

/* Rename a file, copying it if necessary. */

int
#ifdef __APPLE__
move_file(const char *from, const char *to, bool do_backup)
#else
move_file(const char *from, const char *to)
#endif
{
	int	fromfd;
	ssize_t	i;

	/* to stdout? */

	if (strEQ(to, "-")) {
#ifdef DEBUGGING
		if (debug & 4)
			say("Moving %s to stdout.\n", from);
#endif
		fromfd = open(from, O_RDONLY);
		if (fromfd < 0)
			pfatal("internal error, can't reopen %s",
			    quoted_name(from));
		while ((i = read(fromfd, buf, buf_size)) > 0)
			if (write(STDOUT_FILENO, buf, i) != i)
				pfatal("write failed");
		close(fromfd);
		return 0;
	}
#ifdef __APPLE__
	if (do_backup && backup_file(to) < 0) {
#else
	if (backup_file(to) < 0) {
#endif
		int serrno = errno;	/* __APPLE__ */
		say("Can't backup %s, output is in %s: %s\n",
		    quoted_name(to), quoted_name(from), strerror(serrno));
		return -1;
	}
#ifdef DEBUGGING
	if (debug & 4)
		say("Moving %s to %s.\n", from, to);
#endif
	if (rename(from, to) < 0) {
		if (errno != EXDEV || copy_file(from, to) < 0) {
			int serrno = errno;	/* __APPLE__ */
			say("Can't create %s, output is in %s: %s\n",
			    quoted_name(to), quoted_name(from),
			    strerror(serrno));
			return -1;
		}
	}
	return 0;
}

/* Backup the original file.  */

int
backup_file(const char *orig)
{
	struct stat	filestat;
	char		bakname[PATH_MAX], *s, *simplename;
	dev_t		orig_device;
	ino_t		orig_inode;

	if (backup_type == none || stat(orig, &filestat) != 0)
		return 0;			/* nothing to do */
	/*
	 * If the user used zero prefixes or suffixes, then
	 * he doesn't want backups.  Yet we have to remove
	 * orig to break possible hardlinks.
	 */
	if ((origprae && *origprae == 0) || *simple_backup_suffix == 0) {
		unlink(orig);
		return 0;
	}
	orig_device = filestat.st_dev;
	orig_inode = filestat.st_ino;

	if (origprae) {
		if (strlcpy(bakname, origprae, sizeof(bakname)) >= sizeof(bakname) ||
		    strlcat(bakname, orig, sizeof(bakname)) >= sizeof(bakname))
			fatal("filename %s too long for buffer\n",
			    quoted_name(origprae));
	} else {
		if ((s = find_backup_file_name(orig)) == NULL)
			fatal("out of memory\n");
		if (strlcpy(bakname, s, sizeof(bakname)) >= sizeof(bakname))
			fatal("filename %s too long for buffer\n",
			    quoted_name(s));
		free(s);
	}

	if ((simplename = strrchr(bakname, '/')) != NULL)
		simplename = simplename + 1;
	else
		simplename = bakname;

	/*
	 * Find a backup name that is not the same file. Change the
	 * first lowercase char into uppercase; if that isn't
	 * sufficient, chop off the first char and try again.
	 */
	while (stat(bakname, &filestat) == 0 &&
	    orig_device == filestat.st_dev && orig_inode == filestat.st_ino) {
		/* Skip initial non-lowercase chars.  */
		for (s = simplename; *s && !islower((unsigned char)*s); s++)
			;
		if (*s)
			*s = toupper((unsigned char)*s);
		else
			memmove(simplename, simplename + 1,
			    strlen(simplename + 1) + 1);
	}
#ifdef DEBUGGING
	if (debug & 4)
		say("Moving %s to %s.\n", orig, bakname);
#endif
	if (rename(orig, bakname) < 0) {
		if (errno != EXDEV || copy_file(orig, bakname) < 0)
			return -1;
	}
	return 0;
}

/*
 * Copy a file.
 */
int
copy_file(const char *from, const char *to)
{
	int	tofd, fromfd;
	ssize_t	i;

	tofd = open(to, O_CREAT|O_TRUNC|O_WRONLY, 0666);
	if (tofd < 0)
		return -1;
	fromfd = open(from, O_RDONLY, 0);
	if (fromfd < 0)
		pfatal("internal error, can't reopen %s", quoted_name(from));
	while ((i = read(fromfd, buf, buf_size)) > 0)
		if (write(tofd, buf, i) != i)
			pfatal("write to %s failed", quoted_name(to));
	close(fromfd);
	close(tofd);
	return 0;
}

/*
 * Allocate a unique area for a string.
 */
char *
savestr(const char *s)
{
	char	*rv;

	if (!s)
		s = "Oops";
	rv = strdup(s);
	if (rv == NULL) {
		if (using_plan_a)
			out_of_mem = true;
		else
			fatal("out of memory\n");
	}
	return rv;
}

#ifdef __APPLE__
/*
 * Save a line into a new buffer, including the terminal newline.  This is like
 * savestr(), but used in contexts where we don't want to assume that the buffer
 * won't have internal NULs for other reasons (e.g., patch files).
 */
char *
saveline(const char *line, size_t linesz, size_t *sz)
{
	const char *s, *end;
	char *rv;

	if (!line)
		return savestr(NULL);

	end = line + linesz;
	for (s = line; s != end && *s != '\n'; s++)
	    ;

	/*
	 * If we reached the end of the buffer, do a quick rewind by one and
	 * consider it the whole line.  There's a great chance this is
	 * malformed since diff(1) inserts a newline and a notice about no
	 * newline at end of file at EOL as needed, so the caller will probably
	 * just toss this out anyways (but it deserves a chance).
	 */
	if (s == end)
		s--;
	*sz = (s - line) + 1;
	if ((rv = malloc(*sz)) == NULL) {
		if (using_plan_a)
			out_of_mem = true;
		else
			fatal("out of memory\n");
	}

	memcpy(rv, line, *sz);
	return (rv);
}
#endif

/*
 * Allocate a unique area for a string.  Call fatal if out of memory.
 */
char *
xstrdup(const char *s)
{
	char	*rv;

	if (!s)
		s = "Oops";
	rv = strdup(s);
	if (rv == NULL)
		fatal("out of memory\n");
	return rv;
}

/*
 * Vanilla terminal output (buffered).
 */
void
say(const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
	fflush(stdout);
}

/*
 * Terminal output, pun intended.
 */
void
fatal(const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	fprintf(stderr, "patch: **** ");
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	my_exit(2);
}

/*
 * Say something from patch, something from the system, then silence . . .
 */
void
pfatal(const char *fmt, ...)
{
	va_list	ap;
	int	errnum = errno;

	fprintf(stderr, "patch: **** ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, ": %s\n", strerror(errnum));
	my_exit(2);
}

/*
 * Get a response from the user via /dev/tty
 */
void
ask(const char *fmt, ...)
{
	va_list	ap;
	ssize_t	nr = 0;
	static	int ttyfd = -1;

	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
	fflush(stdout);
	if (ttyfd < 0)
		ttyfd = open(_PATH_TTY, O_RDONLY);
	if (ttyfd >= 0) {
		if ((nr = read(ttyfd, buf, buf_size)) > 0 &&
		    buf[nr - 1] == '\n')
			buf[nr - 1] = '\0';
	}
	if (ttyfd < 0 || nr <= 0) {
		/* no tty or error reading, pretend user entered 'return' */
		putchar('\n');
		buf[0] = '\0';
	}
}

/*
 * How to handle certain events when not in a critical region.
 */
void
set_signals(int reset)
{
	static sig_t	hupval, intval;

	if (!reset) {
		hupval = signal(SIGHUP, SIG_IGN);
		if (hupval != SIG_IGN)
			hupval = my_exit;
		intval = signal(SIGINT, SIG_IGN);
		if (intval != SIG_IGN)
			intval = my_exit;
	}
	signal(SIGHUP, hupval);
	signal(SIGINT, intval);
}

/*
 * How to handle certain events when in a critical region.
 */
void
ignore_signals(void)
{
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
}

/*
 * Make sure we'll have the directories to create a file. If `striplast' is
 * true, ignore the last element of `filename'.
 */

void
makedirs(const char *filename, bool striplast)
{
	char	*tmpbuf;

	if ((tmpbuf = strdup(filename)) == NULL)
		fatal("out of memory\n");

	if (striplast) {
		char	*s = strrchr(tmpbuf, '/');
		if (s == NULL) {
			free(tmpbuf);
			return;	/* nothing to be done */
		}
		*s = '\0';
	}
	if (mkpath(tmpbuf) != 0)
		pfatal("creation of %s failed", quoted_name(tmpbuf));
	free(tmpbuf);
}

/*
 * Make filenames more reasonable.
 */
char *
#ifdef __APPLE__
fetchname(const char *at, bool *exists, int strip_leading, const char **endp)
#else
fetchname(const char *at, bool *exists, int strip_leading)
#endif
{
	char		*fullname, *name, *t;
	int		sleading, tab;
	struct stat	filestat;

	if (at == NULL || *at == '\0')
		return NULL;
	while (isspace((unsigned char)*at))
		at++;
#ifdef DEBUGGING
	if (debug & 128)
		say("fetchname %s %d\n", at, strip_leading);
#endif
	/* So files can be created by diffing against /dev/null.  */
	if (strnEQ(at, _PATH_DEVNULL, sizeof(_PATH_DEVNULL) - 1)) {
		*exists = true;
#ifdef __APPLE__
		if (endp != NULL)
			*endp = at + (sizeof(_PATH_DEVNULL) - 1);
#endif
		return NULL;
	}
	name = fullname = t = savestr(at);

	tab = strchr(t, '\t') != NULL;
	/* Strip off up to `strip_leading' path components and NUL terminate. */
	for (sleading = strip_leading; *t != '\0' && ((tab && *t != '\t') ||
	    !isspace((unsigned char)*t)); t++) {
		if (t[0] == '/' && t[1] != '/' && t[1] != '\0')
			if (--sleading >= 0)
				name = t + 1;
	}
	*t = '\0';

	/*
	 * If no -p option was given (957 is the default value!), we were
	 * given a relative pathname, and the leading directories that we
	 * just stripped off all exist, put them back on.
	 */
	if (strip_leading == 957 && name != fullname && *fullname != '/') {
		name[-1] = '\0';
		if (stat(fullname, &filestat) == 0 && S_ISDIR(filestat.st_mode)) {
			name[-1] = '/';
			name = fullname;
		}
	}
#ifdef __APPLE__
	if (endp != NULL)
		*endp = at + ((name + strlen(name)) - fullname);
#endif
	name = savestr(name);
	free(fullname);

	*exists = stat(name, &filestat) == 0;
	return name;
}

void
version(void)
{
	printf("patch 2.0-12u11-Apple\n");
	my_exit(EXIT_SUCCESS);
}

/*
 * Exit with cleanup.
 */
void
my_exit(int status)
{
	unlink(TMPINNAME);
	if (!toutkeep)
		unlink(TMPOUTNAME);
	if (!trejkeep)
		unlink(TMPREJNAME);
	unlink(TMPPATNAME);
	exit(status);
}

#ifdef __APPLE__
#ifdef NDEBUG
#define	CHECK_SPACE(n)	do {			\
	if ((pos) + (n) >= qsize)		\
		return (NULL);			\
} while (0)
#else
#define CHECK_SPACE(n)	assert((pos) + (n) < qsize)
#endif
static const char *
quoted_name_shell(const char *filename, size_t flen, char *qbuf, size_t qsize,
    bool always)
{
	size_t pos;
	char c;
	int quoted;	/* None, Single, Double */

	pos = 0;
	quoted = 0;
	if (always)
		quoted++;

	/*
	 * We still scan, even if we're always quoting, because we may prefer
	 * double quotes to single quotes if there's a single quote appearing
	 * in the string.
	 */
	for (size_t i = 0; i < flen; i++) {
		c = filename[i];

		/*
		 * We err on the side of caution here.  Any remotely
		 * non-trivial name will get quoted.
		 */
		if (isalnum(c) || c == '-' || c == '_' || c == '.')
			continue;

		/*
		 * Single quote will make us double quote, special
		 * characters can more or less be single quoted.  We
		 * don't treat double quotes differently because they're
		 * less ugly to escape.
		 */
		if (quoted == 0)
			quoted++;
		if (c == '\'')
			quoted++;
		if (quoted == 2)
			break;
	}

	if (quoted != 0) {
		CHECK_SPACE(1);
		qbuf[pos++] = quoted == 1 ? '\'' : '"';
	}

	for (size_t i = 0; i < flen; i++) {
		c = filename[i];

		switch (c) {
		case '\'':
			/* Should have double quoted. */
			assert(quoted == 2);
			CHECK_SPACE(1);
			qbuf[pos++] = c;

			break;
		case '$':
		case '`':
		case '\\':
		case '"':
			/*
			 * Characters that only need escaped under double
			 * quotes.
			 */
			assert(quoted > 0);
			CHECK_SPACE(quoted);
			if (quoted == 2)
				qbuf[pos++] = '\'';
			qbuf[pos++] = c;

			break;
		case '\n':
		case '\r':
		case '\t':
			/*
			 * Characters that we'll decompose and use $'' expansion
			 * for.
			 */
			assert(quoted > 0);
			CHECK_SPACE(3);
			qbuf[pos++] = qbuf[0];
			qbuf[pos++] = '$';
			qbuf[pos++] = '\'';

			/*
			 * Collapse into a single quoted string to waste less.
			 */
			for (char cp = filename[i];
			    (cp == '\n' || cp == '\r' || cp == '\t') && i < flen;
			    cp = filename[++i]) {
				CHECK_SPACE(2);
				qbuf[pos++] = '\\';
				switch (cp) {
				case '\n':
					qbuf[pos++] = 'n';

					break;
				case '\r':
					qbuf[pos++] = 'r';

					break;
				case '\t':
					qbuf[pos++] = 't';

					break;
				}
			}

			/*
			 * Back up one, we didn't munch that last one.
			 */
			i--;
			CHECK_SPACE(2);
			qbuf[pos++] = '\'';
			qbuf[pos++] = qbuf[0];

			break;
		default:
			CHECK_SPACE(1);
			qbuf[pos++] = c;

			break;
		}
	}

	CHECK_SPACE(1 + !!quoted);
	if (quoted != 0)
		qbuf[pos++] = qbuf[0];
	qbuf[pos++] = '\0';
	return (qbuf);
}

static const char *
quoted_name_c(const char *filename, size_t flen, char *qbuf, size_t qsize,
    bool quoted)
{
	size_t pos;
	char c;

	pos = 0;
	if (quoted) {
		CHECK_SPACE(1);
		qbuf[pos++] = '"';
	}

	for (size_t i = 0; i < flen; i++) {
		c = filename[i];

		switch (c) {
		case '\t':
		case '\n':
		case '\r':
			/* Characters that we'll decompose. */
			CHECK_SPACE(2);
			qbuf[pos++] = '\\';
			switch (c) {
			case '\n':
				qbuf[pos++] = 'n';
				break;
			case '\r':
				qbuf[pos++] = 'r';
				break;
			case '\t':
				qbuf[pos++] = 't';
				break;
			}

			break;
		case '\\':
		case '"':
			/* Characters that we'll escape. */
			CHECK_SPACE(2);
			qbuf[pos++] = '\\';
			qbuf[pos++] = c;

			break;
		default:
			if (c < 0x20 || c == 0x7f) {
				CHECK_SPACE(4);
				sprintf(&qbuf[pos], "\\x%.02x", c);
				pos += 4;

				/*
				 * Keep hex'ifying hex digits after this to
				 * avoid a parser mishap.
				 */
				while (++i < flen) {
					c = filename[i];
					if ((c >= 'a' && c <= 'f') ||
					    (c >= 'A' && c <= 'F') ||
					    isdigit(c)) {
						CHECK_SPACE(4);
						sprintf(&qbuf[pos], "\\x%.02x",
						    c);
						pos += 4;

						continue;
					}

					break;
				}

				/*
				 * Back up one, we didn't munch that last one.
				 */
				i--;
			} else {
				/* Trivial character, pass it 1:1. */
				CHECK_SPACE(1);
				qbuf[pos++] = c;
			}

			break;
		}
	}

	CHECK_SPACE(1 + !!quoted);
	if (quoted)
		qbuf[pos++] = '"';
	qbuf[pos++] = '\0';
	return (qbuf);
}
#undef CHECK_SPACE

const char *
quoted_name(const char *filename)
{
	/*
	 * Large enough for a PATH_MAX buffer full of newlines, enclosed in
	 * quotes, and NUL terminated -- ~8K?  Move it onto the heap at the
	 * first sign of trouble.
	 */
	static char qbuf[(PATH_MAX * 8) + 3];
	size_t flen;

	if (quote_opt == QO_LITERAL)
		return (filename);

	flen = strlen(filename);
	if (quote_opt == QO_SHELL || quote_opt == QO_SHELL_ALWAYS)
		return (quoted_name_shell(filename, flen, qbuf, sizeof(qbuf),
		    quote_opt == QO_SHELL_ALWAYS));

	assert(quote_opt == QO_C || quote_opt == QO_ESCAPE);
	return (quoted_name_c(filename, flen, qbuf, sizeof(qbuf),
	    quote_opt == QO_C));
}
#endif
