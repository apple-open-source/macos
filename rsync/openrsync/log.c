/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2024, Klara, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <libutil.h>
#include <locale.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#ifdef __APPLE__
#include <usbuf.h>
#else
#include <sys/sbuf.h>
#endif
#include <sys/param.h>
#include <sys/stat.h>

#include "extern.h"

#ifndef LOG_NDELAY
#define	LOG_NDELAY	0
#endif

#define	RSYNCD_SYSLOG_IDENT	"openrsyncd"
#define	RSYNCD_SYSLOG_OPTIONS	(LOG_PID | LOG_NDELAY)

#define LOG_FORMAT_SUCCESS	(1 << 0)
#define LOG_FORMAT_ITEMIZE	(1 << 1)
#define LOG_FORMAT_LATEPRINT	(1 << 2)
#define LOG_FORMAT_OPERATION	(1 << 3)
#define LOG_FORMAT_ITEMIZE_I	(1 << 4)

extern int verbose;

#define	FACILITY(f)	{ #f, LOG_ ##f }
const struct syslog_facility {
	const char	*name;
	int		 value;
} facilities[] = {
	FACILITY(AUTH),
	FACILITY(AUTHPRIV),
#ifdef LOG_CONSOLE
	FACILITY(CONSOLE),
#endif
	FACILITY(CRON),
	FACILITY(DAEMON),
	FACILITY(FTP),
	FACILITY(KERN),
	FACILITY(LPR),
	FACILITY(MAIL),
	FACILITY(NEWS),
#ifdef LOG_NTP
	FACILITY(NTP),
#endif
#ifdef LOG_SECURITY
	FACILITY(SECURITY),
#endif
	FACILITY(USER),
	FACILITY(UUCP),
	FACILITY(LOCAL0),
	FACILITY(LOCAL1),
	FACILITY(LOCAL2),
	FACILITY(LOCAL3),
	FACILITY(LOCAL4),
	FACILITY(LOCAL6),
	FACILITY(LOCAL7),
};

static FILE *log_file;
static struct sess *log_sess;
static int log_facility = LOG_DAEMON;

int
rsync_set_logfacility(const char *facility)
{
	const struct syslog_facility *def;

	for (size_t i = 0; i < nitems(facilities); i++) {
		def = &facilities[i];

		if (strcasecmp(def->name, facility)) {
			log_facility = def->value;
			return 0;
		}
	}

	errno = ENOENT;
	return -1;
}

static void
rsync_logfile_changed(FILE *old_logfile, FILE *new_logfile)
{

	/* We're the last reference to the log file; close it. */
	if (old_logfile != stdout && old_logfile != stderr && old_logfile != NULL)
		fclose(old_logfile);

	if (old_logfile != NULL && new_logfile == NULL) {
		/* <anything> -> syslog */
		openlog(RSYNCD_SYSLOG_IDENT, RSYNCD_SYSLOG_OPTIONS,
		    log_facility);
	} else if (old_logfile == NULL && new_logfile != NULL) {
		closelog();
	}
}

void
rsync_set_logfile(FILE *new_logfile, struct sess *sess)
{
	FILE *prev_logfile;

	/*
	 * Only the server should supply a non-null sess argument,
	 * which causes log_vwritef() to send log messages to
	 * the client via the multiplexed return channel.
	 *
	 * If sess->opts is NULL, then we're in the daemon client handler before
	 * we've figured out the client options and we can assume that things
	 * will work out.
	 */
	if (sess != NULL && sess->opts != NULL && !sess->opts->daemon) {
		assert(new_logfile == stdout);
		assert(sess->opts->server);
		assert(sess->mplex_writes);
	}

	prev_logfile = log_file;
	log_file = new_logfile;
	log_sess = sess;

	rsync_logfile_changed(prev_logfile, new_logfile);
}

static int
log_priority(enum log_type type)
{

	switch (type) {
	case LT_WARNING:
		return LOG_WARNING;
	case LT_ERROR:
		return LOG_ERR;
	case LT_CLIENT:
	case LT_INFO:
	case LT_LOG:
	default:
		return LOG_INFO;
	}
}

static void __printflike(2, 0)
log_vwritef(enum log_type type, const char *fmt, va_list ap)
{
	int pri;

	pri = log_priority(type);

	/*
	 * If logging is configured, we'll send all non-client messages to it.
	 * Note that in various places throughout here, we'll tap out a copy of
	 * the va_list -- there's a good chance we'll be logging to multiple
	 * places, so we want to avoid running off the end of the arg list.
	 */
	if (type != LT_CLIENT && (log_file == NULL || log_file != stdout)) {
		va_list cap;

		va_copy(cap, ap);
		if (log_file == NULL) {
			vsyslog(pri, fmt, cap);
		} else {
			assert(log_file != stdout);
			vfprintf(log_file, fmt, cap);
		}
		va_end(cap);
	}

	if (quiet && pri != LOG_ERR)
		return;

	/*
	 * We shouldn't route log messages to the client.  If write multiplexing
	 * isn't turned on, we may not have a client yet (in the daemon).
	 */
	if (log_sess != NULL && type != LT_LOG && log_sess->mplex_writes) {
		va_list cap;
		char msgbuf[BIGPATH_MAX];
		int32_t tag;
		int n;

		assert(log_sess->opts->server);

		va_copy(cap, ap);
		n = vsnprintf(msgbuf, sizeof(msgbuf), fmt, cap);
		va_end(cap);
		if (n < 1)
			return;

		if ((size_t)n > sizeof(msgbuf))
			n = sizeof(msgbuf);

		tag = (pri == LOG_ERR) ? IT_ERROR_XFER : IT_INFO;

		if (log_sess->wbufp == NULL) {
			int client = STDOUT_FILENO;

			if (log_sess->role != NULL)
				client = log_sess->role->client;

			io_write_buf_tagged(log_sess, client, msgbuf, n, tag);
		} else {
			size_t *wbufszp = log_sess->wbufszp;
			size_t pos = *log_sess->wbufszp;
			void **wbufp = log_sess->wbufp;
			int32_t	tagbuf;

			assert(log_sess->opts->sender);

			if (!io_lowbuffer_alloc(log_sess, wbufp, wbufszp,
			    log_sess->wbufmaxp, n))
				return;

			tagbuf = htole32(((tag + IOTAG_OFFSET) << 24) + n);

			io_buffer_int(*wbufp, &pos, *wbufszp, tagbuf);
			io_buffer_buf(*wbufp, &pos, *wbufszp, msgbuf, n);
		}

		if (type == LT_CLIENT)
			return;
	}

	/*
	 * Log messages stop here, every other type will trickle through and get
	 * routed to stderr/stdout as appropriate.
	 */
	if (type == LT_LOG || log_sess != NULL)
		return;

	switch (pri) {
	case LOG_INFO:
		vfprintf(stdout, fmt, ap);
		break;
	default:
		fflush(stdout);
		vfprintf(stderr, fmt, ap);
		break;
	}
}

static void __printflike(2, 3)
log_writef(enum log_type type, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	log_vwritef(type, fmt, ap);
	va_end(ap);
}

void
rsync_log_tag(enum iotag tag, const char *fmt, ...)
{
	enum log_type type;
	va_list ap;

	type = (tag == IT_ERROR_XFER) ? LT_WARNING : LT_INFO;

	va_start(ap, fmt);
	log_vwritef(type, fmt, ap);
	va_end(ap);
}

/*
 * Log a message at level "level", starting at zero, which corresponds
 * to the current verbosity level opts->verbose (whose verbosity starts
 * at one).
 */
void
rsync_log(int level, const char *fmt, ...)
{
	char	*buf = NULL;
	va_list	 ap;

	if (verbose < level + 1)
		return;

	if (fmt != NULL) {
		va_start(ap, fmt);
		if (vasprintf(&buf, fmt, ap) == -1) {
			va_end(ap);
			return;
		}
		va_end(ap);
	}

	if (level <= 0 && buf != NULL)
		log_writef(LT_INFO, "%s\n", buf);
	else if (level > 0)
		log_writef(LT_INFO, "%s(%d): %s%s\n", getprogname(),
		    getpid(), (buf != NULL) ? ": " : "",
		    (buf != NULL) ? buf : "");
	free(buf);
}

/*
 * This reports an error---not a warning.
 * However, it is not like errx(3) in that it does not exit.
 */
void
rsync_errx(const char *fmt, ...)
{
	char	*buf = NULL;
	va_list	 ap;

	if (fmt != NULL) {
		va_start(ap, fmt);
		if (vasprintf(&buf, fmt, ap) == -1) {
			va_end(ap);
			return;
		}
		va_end(ap);
	}

	log_writef(LT_ERROR, "%s(%d): error%s%s\n", getprogname(),
	   getpid(), (buf != NULL) ? ": " : "",
	   (buf != NULL) ? buf : "");
	free(buf);
}

/*
 * This reports an error---not a warning.
 * However, it is not like err(3) in that it does not exit.
 */
void
rsync_err(const char *fmt, ...)
{
	char	*buf = NULL;
	va_list	 ap;
	int	 er = errno;

	if (fmt != NULL) {
		va_start(ap, fmt);
		if (vasprintf(&buf, fmt, ap) == -1) {
			va_end(ap);
			return;
		}
		va_end(ap);
	}

	log_writef(LT_ERROR, "%s(%d): error%s%s: %s\n", getprogname(),
	   getpid(), (buf != NULL) ? ": " : "",
	   (buf != NULL) ? buf : "", strerror(er));
	free(buf);
}

/*
 * Prints a non-terminal error message, that is, when reporting on the
 * chain of functions from which the actual warning occurred.
 */
void
rsync_errx1(const char *fmt, ...)
{
	char	*buf = NULL;
	va_list	 ap;

	if (verbose < 1)
		return;

	if (fmt != NULL) {
		va_start(ap, fmt);
		if (vasprintf(&buf, fmt, ap) == -1) {
			va_end(ap);
			return;
		}
		va_end(ap);
	}

	log_writef(LT_ERROR, "%s(%d): error%s%s\n", getprogname(),
	   getpid(), (buf != NULL) ? ": " : "",
	   (buf != NULL) ? buf : "");
	free(buf);
}

/*
 * Prints a warning message if we're running -v.
 */
void
rsync_warnx1(const char *fmt, ...)
{
	char	*buf = NULL;
	va_list	 ap;

	if (verbose < 1)
		return;

	if (fmt != NULL) {
		va_start(ap, fmt);
		if (vasprintf(&buf, fmt, ap) == -1) {
			va_end(ap);
			return;
		}
		va_end(ap);
	}

	log_writef(LT_WARNING, "%s(%d): warning%s%s\n", getprogname(),
	   getpid(), (buf != NULL) ? ": " : "",
	   (buf != NULL) ? buf : "");
	free(buf);
}

/*
 * Prints a warning message.
 */
void
rsync_warnx(const char *fmt, ...)
{
	char	*buf = NULL;
	va_list	 ap;

	if (fmt != NULL) {
		va_start(ap, fmt);
		if (vasprintf(&buf, fmt, ap) == -1) {
			va_end(ap);
			return;
		}
		va_end(ap);
	}

	log_writef(LT_WARNING, "%s(%d): warning%s%s\n", getprogname(),
	   getpid(), (buf != NULL) ? ": " : "",
	   (buf != NULL) ? buf : "");
	free(buf);
}

/*
 * Prints a warning with an errno.
 * It uses a level detector for when to inhibit printing.
 */
void
rsync_warn(int level, const char *fmt, ...)
{
	char	*buf = NULL;
	va_list	 ap;
	int	 er = errno;

	if (verbose < level)
		return;

	if (fmt != NULL) {
		va_start(ap, fmt);
		if (vasprintf(&buf, fmt, ap) == -1) {
			va_end(ap);
			return;
		}
		va_end(ap);
	}

	log_writef(LT_WARNING, "%s(%d): warning%s%s: %s\n", getprogname(),
	   getpid(), (buf != NULL) ? ": " : "",
	   (buf != NULL) ? buf : "", strerror(er));
	free(buf);
}

/*
 * Cut down printf implementation taken from printf(1) in
 * FreeBSD 15-current rev 30189156d325fbcc9d1997d791daedc9fa3bed20
 */

static const char widthchars[] = "'+- 0123456789";

/*
 * Copies string 2 into string 1, which is quaranteed to be at least
 * as longly allocated as string 2, omitting "'".  Returns the number
 * of "'"s.
 */

static int
isit_human(char *s1, const char *s2)
{
	char *p1;
	const char *p2;
	int count = 0;

	for (p1 = s1, p2 = s2; *p2; p2++) {
		if (*p2 == '\'')
			count++;
		else
			*p1++ = *p2;
	}
	*p1 = '\0';

	return count;
}

/*
 * Do the 8-bit escaping as needed for `s`.  If `sbuf` is NULL, then the result
 * will be written to the log file -- otherwise, it'll be stashed in the sbuf
 * passed in as requested.
 *
 * TODO: We used to print the names of items to be updated with a mix of calls
 * to LOG1() and print_7_or_8_bit().  With the former, embedded control
 * characters were not correctly escaped, but all other characters were printed
 * as if --8-bit-output were in effect (hence unicode characters were preserved).
 * Conversely, with the latter, unicode characters outside the portable set were
 * all treated as control characters and hence incorrectly escaped.
 *
 * We now call log_item() to print each item, which ends up calling this
 * function, which now handles control characters correctly and partially
 * deals with some range of unicode characters (e.g. c2xx-dfxx) that we get
 * with the C.UTF-8 locale.
 *
 * The correct fix seems to be to use iconv() where available, and print all
 * chars outside the portable set as if --8-bit-output were in effect.
 */
int __printflike(2, 0)
print_7_or_8_bit(const struct sess *sess, const char *fmt, const char *s,
    struct sbuf *sbuf)
{
	const char *p;
	struct sbuf *innerbuf;

	innerbuf = sbuf_new_auto();
	if (innerbuf == NULL) {
		ERR("sbuf_new_auto");
		return 0;
	}

	for (p = s; *p; p++) {
		unsigned char c = *(unsigned char *)p;

		if (isprint(c) || c == '\t' || c == 0x7f) {
			if (c == '\\' &&
			    *(unsigned char *)(p + 1) == '#' &&
			    isdigit(*(unsigned char *)(p + 2)) &&
			    isdigit(*(unsigned char *)(p + 3)) &&
			    isdigit(*(unsigned char *)(p + 4))) {
				sbuf_printf(innerbuf, "\\#%03o", '\\');
			} else {
				sbuf_putc(innerbuf, c);
			}
		} else if (c < ' ') {
                        sbuf_printf(innerbuf, "\\#%03o", c);
		} else if (sess->opts->bit8) {
                        sbuf_putc(innerbuf, c);
		} else if (c >= 0xc2 && c <= 0xdf) {
			unsigned char c2 = *(unsigned char *)(p + 1);

			/* TODO: Use iconv() */
			if (c2 >= 0x80 && c2 <= 0xdf) {
				sbuf_putc(innerbuf, c);
				sbuf_putc(innerbuf, c2);
				p++;
			} else {
				sbuf_printf(innerbuf, "\\#%03o", c);
			}
		} else {
			sbuf_printf(innerbuf, "\\#%03o", c);
		}
	}

	if (sbuf_finish(innerbuf) != 0) {
		ERR("sbuf_finish");
		sbuf_delete(innerbuf);
		return 0;
	}

	if (sbuf != NULL)
		sbuf_printf(sbuf, fmt, sbuf_data(innerbuf));
	else
		log_writef(LT_INFO, fmt, sbuf_data(innerbuf));
	sbuf_delete(innerbuf);

	return 1;
}

/*
 * rval is filled with whether there is any argument that requires
 * late printing or whether itemization is requested.  See the
 * LOG_FORMAT_* flags.
 *
 * rval is expected to be initialized to zero before the first call.
 */
static const char * __printflike(1, 0)
printf_doformat(const char *fmt, int *rval, struct sess *sess,
    const struct flist *fl, struct sbuf *sbuf)
{
	static const char skip1[] = "'-+ 0";
	const char *fmt_orig = fmt;
	char convch;
	size_t l;
	char widthstring[8192];
	int humanlevel = 0;
	char buf[8192];

	fmt++;

	widthstring[0] = '%';
	l = strspn(fmt, widthchars);
	/* We need a reserve of 4 chars for substitutions below, plus lead */
	if (l + 5u > sizeof(widthstring)) {
		ERRX("Insufficient buffer for width format");
		return NULL;
	}
	strlcpy(widthstring + 1, fmt, l + 1);

	if (strchr(widthstring, '\'')) {
		char *cooked = malloc(strlen(widthstring));

		if (cooked == NULL) {
			ERR("malloc");
			return NULL;
		}
		humanlevel = isit_human(cooked, widthstring);
		strlcpy(widthstring, cooked, l + 1);
		l -= humanlevel;
		free(cooked);
	}

	/* skip to field width */
	while (*fmt && strchr(skip1, *fmt) != NULL) {
		fmt++;
	}
	if (*fmt == '\0') {
		if (sbuf != NULL) {
			sbuf_putc(sbuf, fmt_orig[0]);
			fmt = fmt_orig + 1;
		}
		return fmt_orig + 1;
	}
	while (isdigit(*fmt)) {
		fmt++;
	}

	convch = *fmt;
	fmt++;

	switch (convch) {
	case 'a':	/* Remote address (daemon) */
	case 'h': {	/* Remote host (daemon) */
		if (!sess->opts->daemon)
			break;	/* Nop in non-daemon mode. */
		/* FALLTHROUGH */
	}
	case 'm':	/* Module */
	case 'P':	/* Module path */
	case 'u': {	/* Auth username */
		const char *rolestr = NULL;

		/*
		 * These are also effectively daemon-only, but we'll still
		 * render a blank string for clients.  All of them are delegated
		 * to the role.
		 */
		if (sess->role->role_fetch_outfmt != NULL) {
			rolestr = sess->role->role_fetch_outfmt(sess,
			    sess->role->role_fetch_outfmt_cookie, convch);
		}
		if (rolestr == NULL)
			rolestr = "";

		if (sbuf != NULL) {
			widthstring[l + 1] = 's';
			widthstring[l + 2] = '\0';
			sbuf_printf(sbuf, widthstring, rolestr);
		}

		break;
	}
	case 'b': {
		uint64_t bytes_transferred = 0;

		*rval |= LOG_FORMAT_LATEPRINT;

		if (sbuf == NULL)
			break;

		if (!sess->opts->dry_run) {
			bytes_transferred = sess->total_read - sess->total_read_lf;
			bytes_transferred += sess->total_write - sess->total_write_lf;
		}

		switch (humanlevel) {
		case 0:
			widthstring[l + 1] = 'l';
			widthstring[l + 2] = 'd';
			widthstring[l + 3] = '\0';
			sbuf_printf(sbuf, widthstring,
				    bytes_transferred);
			break;
		case 1:
			widthstring[l + 1] = 'l';
			widthstring[l + 2] = 'd';
			widthstring[l + 3] = '\0';
			sbuf_printf(sbuf, widthstring,
				    bytes_transferred);
			break;
		case 2:
			humanize_number(buf, 5, bytes_transferred,
					"", HN_AUTOSCALE, HN_DECIMAL|HN_NOSPACE);
			widthstring[l + 1] = 's';
			widthstring[l + 2] = '\0';
			sbuf_printf(sbuf, widthstring, buf);
			break;
		case 3:
			humanize_number(buf, 5, bytes_transferred, "",
					HN_AUTOSCALE,
					HN_DECIMAL|HN_NOSPACE|HN_DIVISOR_1000);
			widthstring[l + 1] = 's';
			widthstring[l + 2] = '\0';
			sbuf_printf(sbuf, widthstring, buf);
			break;
		}
		break;
	}
	case 'B': {
		/* Print mode human-readable */

		if (sbuf != NULL) {
			our_strmode(fl->st.mode, buf);
			widthstring[l + 1] = 's';
			widthstring[l + 2] = '\0';
			sbuf_printf(sbuf, widthstring, buf);
		}
		break;
	}
	case 'c': {
		/* "%c the total size of the block checksums received for the
		   basis file (only when sending)" */
		/*
		 * I don't think smb rsync implements what it says in the
		 * manpage.
		 */
		*rval |= LOG_FORMAT_LATEPRINT;
		break;
	}
#if 0
	case 'C': {

		/* This is a rsync 3.x feature */

		/* the full-file checksum if it is known for the file.
		 * For older rsync protocols/versions, the checksum
		 * was salted, and is thus not a useful value (and is
		 * not dis- played when that is the case). For the
		 * checksum to output for a file, either the
		 * --checksum option must be in-ef- fect or the file
		 * must have been transferred without a salted
		 * checksum being used.  See the --checksum-choice
		 * option for a way to choose the algorithm.
		*/

		break;
	}
#endif
	case 'f': {
		/*
		 * "the filename (long form on sender; no trailing "/")"
		 */
		if (sbuf != NULL) {
			const char *path = fl->path;

			if (sess->opts->relative)
				path = fl->wpath;

			while (*path == '/')
				path++;

			widthstring[l + 1] = 's';
			widthstring[l + 2] = '\0';
			if (!print_7_or_8_bit(sess, widthstring, path, sbuf)) {
				ERRX("print_7_or_8_bit");
				return NULL;
			}
		}
		break;
	}
	case 'G': {
		/* FIXME this is incorrect since gid 0 is also root */
		if (sbuf != NULL) {
			if (fl->st.gid) {
				widthstring[l + 1] = 'd';
				widthstring[l + 2] = '\0';
				sbuf_printf(sbuf, widthstring, fl->st.gid);
			} else {
				widthstring[l + 1] = 's';
				widthstring[l + 2] = '\0';
				sbuf_printf(sbuf, widthstring, "DEFAULT");
			}
		}
		break;
	}
	case 'I':
		*rval |= LOG_FORMAT_ITEMIZE_I;
		break;
	case 'i': {
		/* itemize string YXcstpogz */
		int32_t ifl;

		*rval |= LOG_FORMAT_ITEMIZE;
		if (sbuf != NULL) {
			ifl = fl->iflags;
			if (ifl & IFLAG_DELETED) {
				/* Handled by flist_gen_dels() */
				break;
			}

			/*
			 * We only use 10 bytes from buf[], but buf is very
			 * large so only zero the first few bytes.
			 */
			assert(sizeof(buf) >= 16);
			bzero(buf, 16);

			buf[0] = '.';
			if (ifl & IFLAG_LOCAL_CHANGE) {
				buf[0] = (ifl & IFLAG_HLINK_FOLLOWS) ? 'h' : 'c';
			} else if (ifl & IFLAG_TRANSFER) {
				buf[0] = sess->lreceiver ? '>' : '<';
			}

			if (S_ISDIR(fl->st.mode))
				buf[1] = 'd';
			if (S_ISLNK(fl->st.mode))
				buf[1] = 'L';
			if (S_ISSOCK(fl->st.mode) || S_ISFIFO(fl->st.mode))
				buf[1] = 'S';
			if (S_ISBLK(fl->st.mode) || S_ISCHR(fl->st.mode))
				buf[1] = 'D';
			if (buf[1] == '\0')
				buf[1] = 'f';

			if (ifl & IFLAG_CHECKSUM)
				buf[2] = 'c';
			else
				buf[2] = '.';

			if (ifl & IFLAG_SIZE)
				buf[3] = 's';
			else
				buf[3] = '.';

			buf[4] = '.';
			if (ifl & IFLAG_TIME) {
				if (!sess->opts->preserve_times ||
				    S_ISLNK(fl->st.mode)) {
					buf[4] = 'T';
				} else {
					buf[4] = 't';
				}
			}

			if (ifl & IFLAG_PERMS)
				buf[5] = 'p';
			else
				buf[5] = '.';

			if (ifl & IFLAG_OWNER)
				buf[6] = 'o';
			else
				buf[6] = '.';

			if (ifl & IFLAG_GROUP)
				buf[7] = 'g';
			else
				buf[7] = '.';

			buf[8] = '.';

			if (ifl & IFLAG_MISSING_DATA || ifl & IFLAG_NEW) {
				char c;

				if (ifl & IFLAG_NEW)
					c = '+';
				else
					c = '?';
				buf[2] = c; buf[3] = c; buf[4] = c;
				buf[5] = c; buf[6] = c; buf[7] = c;
				buf[8] = c;
			} else {
				int i;

				if (buf[0] == '.' || buf[0] == 'h' ||
				    (buf[0] == 'c' && buf[1] == 'f')) {
					for (i = 2; buf[i]; ++i) {
						if (buf[i] != '.')
							break;
					}
					if (buf[i] == '\0') {
						for (i = 2; buf[i]; ++i)
							buf[i] = ' ';
					}
				}
			}

			widthstring[l + 1] = 's';
			widthstring[l + 2] = '\0';
			sbuf_printf(sbuf, widthstring, buf);
		}
		break;
	}
	case 'l': {
		/* File length */

		if (sbuf != NULL) {
			switch (humanlevel) {
			case 0:
				widthstring[l + 1] = 'l';
				widthstring[l + 2] = 'd';
				widthstring[l + 3] = '\0';
				sbuf_printf(sbuf, widthstring, fl->st.size);
				break;
			case 1:
				/* TODO for 3.x: use a printf with "'" */
				widthstring[l + 1] = '\'';
				widthstring[l + 2] = 'l';
				widthstring[l + 3] = 'd';
				widthstring[l + 4] = '\0';
				sbuf_printf(sbuf, widthstring, fl->st.size);
				break;
			case 2:
				humanize_number(buf, 5, fl->st.size,
				    "", HN_AUTOSCALE, HN_DECIMAL|HN_NOSPACE);
				widthstring[l + 1] = 's';
				widthstring[l + 2] = '\0';
				sbuf_printf(sbuf, widthstring, buf);
				break;
			case 3:
				humanize_number(buf, 5, fl->st.size, "", HN_AUTOSCALE,
				    HN_DECIMAL|HN_NOSPACE|HN_DIVISOR_1000);
				widthstring[l + 1] = 's';
				widthstring[l + 2] = '\0';
				sbuf_printf(sbuf, widthstring, buf);
				break;
			}
		}
		break;
	}
	case 'L': {
#if 0
		/*
		 * Use "late print" here.  Theoretically late print is
		 * only needed when hardlink printing is requested.
		 * But with just the format string we can't tell
		 * whether there will ever be hardlinks.
		 */
		*rval |= LOG_FORMAT_LATEPRINT;
#endif

		if (sbuf != NULL) {
			if (fl->link != NULL &&
			    (fl->iflags & IFLAG_BASIS_FOLLOWS) == 0) {
				const char *fmt = " -> %s";

				if ((fl->iflags & IFLAG_HLINK_FOLLOWS) != 0)
					fmt = " => %s";

				snprintf(buf, sizeof(buf), fmt, fl->link);
				widthstring[l + 1] = 's';
				widthstring[l + 2] = '\0';
				if (!print_7_or_8_bit(sess, widthstring, buf,
				    sbuf)) {
					ERRX("print_7_or_8_bit");
					return NULL;
				}
			}
		}
		break;
	}
	case 'M': {
		/* Modification time of item */

		if (sbuf != NULL) {
			/* 2024/01/30-16:23:29 */
			strftime(buf, sizeof(buf), "%Y/%m/%d-%H:%M:%S",
			    localtime(&fl->st.mtime));
			widthstring[l + 1] = 's';
			widthstring[l + 2] = '\0';
			sbuf_printf(sbuf, widthstring, buf);
		}
		break;
	}
	case 'n': {
		/* Alternate file name print */

		if (sbuf != NULL) {
			const char *path = fl->wpath;

			if (sess->opts->relative)
				path = fl->path;

			widthstring[l + 1] = 's';
			widthstring[l + 2] = '\0';
			/* "(short form; trailing "/" on dir)" */
			if (S_ISDIR(fl->st.mode)) {
				snprintf(buf, sizeof(buf), "%s/", path);
				path = buf;
			}
			if (!print_7_or_8_bit(sess, widthstring, path, sbuf)) {
				ERRX("print_7_or_8_bit");
				return NULL;
			}
		}
		break;
	}
	case 'o': {
		*rval |= LOG_FORMAT_OPERATION;

		/*
		 * "the operation, which is "send", "recv", or "del." (the
		 * latter includes the trailing period)"
		 */
		if (sbuf != NULL) {
			widthstring[l + 1] = 's';
			widthstring[l + 2] = '\0';
			if (!print_7_or_8_bit(sess, widthstring,
			    sess->opts->sender ? "send" : "recv", sbuf)) {
				ERRX("print_7_or_8_bit");
				return NULL;
			}
		}
		break;
	}
	case 'p': {
		/* PID as a number */
		if (sbuf != NULL) {
			widthstring[l + 1] = 'd';
			widthstring[l + 2] = '\0';
			/* TODO: capture top-level pid in main() */
			sbuf_printf(sbuf, widthstring, getpid());
		}
		break;
	}
	case 't': {
		/* Current machine time */
		time_t now;

		if (sbuf != NULL) {
			time(&now);
			strftime(buf, sizeof(buf), "%Y/%m/%d-%H:%M:%S",
			    localtime(&now));
			widthstring[l + 1] = 's';
			widthstring[l + 2] = '\0';
			sbuf_printf(sbuf, widthstring, buf);
		}
		break;
	}
	case 'U': {
		/* FIXME this is incorrect since uid 0 is also root */
		if (sbuf != NULL) {
			if (fl->st.uid) {
				widthstring[l + 1] = 'd';
				widthstring[l + 2] = '\0';
				sbuf_printf(sbuf, widthstring, fl->st.uid);
			} else {
				widthstring[l + 1] = 's';
				widthstring[l + 2] = '\0';
				sbuf_printf(sbuf, widthstring, "DEFAULT");
			}
		}
		break;
	}
	default:
		if (sbuf != NULL) {
			sbuf_putc(sbuf, fmt_orig[0]);
			fmt = fmt_orig + 1;
		}
		break;
	}
	return fmt;
}

static int
log_format_type(enum log_type type, struct sess *sess, const char *format,
    const struct flist *fl)
{
	const bool do_print = (fl != NULL);
	size_t len;
	int end, rval = 0;
	const char *start;
	const char *fmt;
	struct sbuf *sbuf;

	if (format == NULL)
		return 0;

	sbuf = NULL;
	if (do_print) {
		sbuf = sbuf_new_auto();
		if (sbuf == NULL) {
			ERR("sbuf_new_auto");
			return 0;
		}
	}

	fmt = format;
	len = strlen(fmt);
	rval = end = 0;

	for (; *fmt;) {
		start = fmt;
		while (fmt < format + len) {
			if (fmt[0] == '%') {
				if (do_print)
					sbuf_bcat(sbuf, start, fmt - start);
				if (fmt[1] == '%') {
					/* %% prints a % */
					if (do_print)
						sbuf_putc(sbuf, '%');
					fmt += 2;
				} else {
					fmt = printf_doformat(fmt, &rval, sess,
					    fl, sbuf);
					if (fmt == NULL || *fmt == '\0')
						goto out;
					end = 0;
				}
				start = fmt;
			} else
				fmt++;
		}
		if (end == 1) {
			ERRX("missing format character");
			if (sbuf != NULL)
				sbuf_delete(sbuf);
			return 0;
		}
		if (do_print)
			sbuf_bcat(sbuf, start, fmt - start);
	}

out:
	if (do_print) {
		sbuf_putc(sbuf, '\n');

		if (sbuf_finish(sbuf) != 0) {
			ERR("sbuf_finish");
			sbuf_delete(sbuf);
			return 0;
		}

		log_writef(type, "%s", sbuf_data(sbuf));
		sbuf_delete(sbuf);
	} else {
		assert(sbuf == NULL);
	}

	return rval | LOG_FORMAT_SUCCESS;
}

static int
log_format(struct sess *sess, const char *format, const struct flist *fl)
{
	return log_format_type(LT_INFO, sess, format, fl);
}

void
log_format_init(struct sess *sess)
{
	int flags = log_format(sess, sess->opts->outformat, NULL);
	int logflags = log_format(sess, sess->opts->logformat, NULL);

	if ((flags & LOG_FORMAT_SUCCESS) != 0) {
		bool itemize_I;

		sess->itemize_i = (flags & LOG_FORMAT_ITEMIZE) != 0;

		sess->itemize_o = (flags & LOG_FORMAT_OPERATION) != 0;
		sess->lateprint = (flags & LOG_FORMAT_LATEPRINT) != 0;

		itemize_I = (flags & LOG_FORMAT_ITEMIZE_I) != 0;

		sess->itemize = sess->itemize_i + itemize_I;
		if (sess->itemize == 1) {
			if (sess->opts->itemize > 1 || verbose > 1)
				sess->itemize++;
		}
	}

	if ((logflags & LOG_FORMAT_SUCCESS) != 0) {
		sess->logfile_itemize_i = (logflags & LOG_FORMAT_ITEMIZE) != 0;
		sess->logfile_itemize_o = (logflags & LOG_FORMAT_OPERATION) != 0;
	}

	if (sess->opts->server || sess->opts->daemon)
		sess->lateprint = 1;
}


/*
 * Print a number into the provided buffer depending on the current
 * --human-readable level.
 * Returns 0 on success, -1 if the buffer is too small.
 */
int
rsync_humanize(struct sess *sess, char *buf, size_t len, int64_t val)
{
	size_t res = 0;
	char tbuf[32];

	switch (sess->opts->human_readable) {
	case 0:
		humanize_number(tbuf, sizeof(tbuf), val, "B", 0, 0);
		res = snprintf(buf, len, "%s", tbuf);
		break;
	case 1:
		humanize_number(tbuf, 9, val, "B",
		    HN_AUTOSCALE, HN_DECIMAL|HN_DIVISOR_1000);
		res = snprintf(buf, len, "%s", tbuf);
		break;
	case 2:
		humanize_number(tbuf, 10, val, "B",
		    HN_AUTOSCALE, HN_DECIMAL|HN_IEC_PREFIXES);
		res = snprintf(buf, len, "%s", tbuf);
		break;
	}

	if (res >= len) {
		return -1;
	}

	return 0;
}

int
log_item_impl(enum log_type type, struct sess *sess, const struct flist *f)
{
	const char *outformat = sess->opts->outformat;
	const char *logformat = sess->opts->logformat;
	int ok = 1;

	if (outformat == NULL && (verbose > 0 || sess->opts->progress))
		outformat = "%n";
	if (type != LT_LOG && outformat != NULL && !sess->opts->server &&
	    !log_format_type(LT_CLIENT, sess, outformat, f))
		ok = 0;
	if (type != LT_CLIENT && logformat != NULL &&
	    !log_format_type(LT_LOG, sess, logformat, f))
		ok = 0;
	return ok;
}

int
log_item(struct sess *sess, const struct flist *f)
{
	bool visible = false;
	bool sig = (f->iflags & SIGNIFICANT_IFLAGS) != 0;
	bool local = (f->iflags & IFLAG_LOCAL_CHANGE) != 0 && sig;
	bool link = (f->iflags & IFLAG_HLINK_FOLLOWS) != 0;
	enum log_type type = (sess->opts->server ? LT_LOG : LT_INFO);

	if (!sess->itemize && verbose > 1 && f->iflags == 0 &&
	    sess->mode == FARGS_RECEIVER) {
		if (S_ISDIR(f->st.mode))
			return 1;

		return print_7_or_8_bit(sess, "%s is uptodate\n", f->wpath, NULL);
	}

	if (sess->itemize) {
		/*
		 * We capture more with %I present, but we'll also expand our
		 * horizons if we have a highly-verbose %i.
		 */
		visible = sig || sess->itemize > 1 || link ||
		    (verbose > 1 && sess->itemize_i);
	}

	/*
	 * We don't generally log if we are the server, but there are
	 * exceptions.  If a custom outformat is set, then we should
	 * generate logs, except if the outformat is being overridden
	 * by using itemize that sets the outformat to include %i or %o.
	 */
	if (sess->opts->server) {
		if (sess->itemize ||
		    (!sess->opts->outformat || !*sess->opts->outformat)) {
			if (log_file == stdout || sess->opts->dry_run)
				return 1;

			if (!(sess->itemize && (sig || verbose > 1)))
				return 1;

			type = LT_LOG;
		} else {
			return 1;
		}
	} else {
		bool filtered = true;

		if (visible || local)
			filtered = false;
		if (S_ISDIR(f->st.mode) && sig)
			filtered = false;
		if (link)
			filtered = false;

		/*
		 * This is technically wrong and should be fixed.  Some filtered
		 * subset will go to both the client and the log, while the more
		 * complete set may go to just the client.  For now, we send the
		 * filtered subset to both and only restrict insignificant stuff
		 * to client-only when -i hasn't been requested in the log file.
		 */
		if (filtered) {
			return 1;
		} else if (!sess->logfile_itemize_i && !sig) {
			type = LT_CLIENT;
		}
	}

	return log_item_impl(type, sess, f);
}

const char *
iflags_decode(uint32_t iflags)
{
	static const char *namev[] = {
		"atime", "cksum", "size", "time",
		"perms", "owner", "group", "acl",
		"xattr", "bad9", "bad10", "basis",
		"hlink", "new", "local", "transfer",
		"missing", "deleted", "hadbasis"
	};
	static char buf[256];
	char *bufp = buf;

	bufp += snprintf(buf, sizeof(buf), "0x%x", iflags);

	if (iflags) {
		assert((iflags & ((1u << (sizeof(namev) / sizeof((namev)[0]))) - 1)));

		for (size_t i = 0; iflags; ++i) {
			if (iflags & 1) {
				bufp += strlcat(bufp, ",", sizeof(buf) - (bufp - buf));
				bufp += strlcat(bufp, namev[i], sizeof(buf) - (bufp - buf));
			}
			iflags >>= 1;
		}
	}

	return buf;
}
