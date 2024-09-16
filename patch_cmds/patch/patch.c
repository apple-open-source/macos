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
 * $OpenBSD: patch.c,v 1.54 2014/12/13 10:31:07 tobias Exp $
 * $FreeBSD$
 *
 */

#ifdef __APPLE__
#include <sys/param.h>
#else
#include <sys/types.h>
#endif
#include <sys/stat.h>
#ifdef __APPLE__
#include <sys/time.h>
#endif

#include <assert.h>
#include <ctype.h>
#ifdef __APPLE__
#include <errno.h>
#endif
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef __APPLE__
#include <time.h>
#endif
#include <unistd.h>

#include "common.h"
#include "util.h"
#include "pch.h"
#include "inp.h"
#include "backupfile.h"
#include "pathnames.h"

mode_t		filemode = 0644;

char		*buf;			/* general purpose buffer */
size_t		buf_size;		/* size of the general purpose buffer */

bool		using_plan_a = true;	/* try to keep everything in memory */
bool		out_of_mem = false;	/* ran out of memory in plan a */
bool		nonempty_patchf_seen = false;	/* seen nonempty patch file? */

#define MAXFILEC 2

char		*filearg[MAXFILEC];
bool		ok_to_create_file = false;
char		*outname = NULL;
char		*origprae = NULL;
char		*TMPOUTNAME;
char		*TMPINNAME;
char		*TMPREJNAME;
char		*TMPPATNAME;
bool		toutkeep = false;
bool		trejkeep = false;
bool		warn_on_invalid_line;
bool		last_line_missing_eol;

#ifdef DEBUGGING
int		debug = 0;
#endif

bool		force = false;
bool		batch = false;
bool		verbose = false;
#ifdef __APPLE__
bool		quiet = false;
#endif
bool		reverse = false;
bool		noreverse = false;
bool		skip_rest_of_patch = false;
int		strippath = 957;
bool		canonicalize = false;
bool		check_only = false;
int		diff_type = 0;
char		*revision = NULL;	/* prerequisite revision, if any */
LINENUM		input_lines = 0;	/* how long is input file in lines */
int		posix = 0;		/* strict POSIX mode? */
enum quote_options	quote_opt;

#ifdef __APPLE__
static enum vcsopt parse_vcs_option(const char *optval);
#endif
static void	reinitialize_almost_everything(void);
static void	get_some_switches(void);
static LINENUM	locate_hunk(LINENUM);
static void	abort_context_hunk(void);
static void	rej_line(int, LINENUM);
static void	abort_hunk(void);
#ifdef __APPLE__
static bool	putline(LINENUM line, FILE *fp);
#endif
static void	apply_hunk(LINENUM);
static void	init_output(const char *);
static void	init_reject(const char *);
static void	copy_till(LINENUM, bool);
static bool	spew_output(void);
static void	dump_line(LINENUM, bool);
static bool	patch_match(LINENUM, LINENUM, LINENUM);
static bool	similar(const char *, const char *, int);
#ifdef __APPLE__
static void	usage(FILE *, int);
#else
static void	usage(void);
#endif
static bool	handle_creation(bool, bool *);

/* true if -E was specified on command line.  */
static bool	remove_empty_files = false;

/* true if -R was specified on command line.  */
static bool	reverse_flag_specified = false;

static bool	Vflag = false;

/* buffer holding the name of the rejected patch file. */
static char	rejname[PATH_MAX];

/* how many input lines have been irretractibly output */
static LINENUM	last_frozen_line = 0;

static int	Argc;		/* guess */
static char	**Argv;
static int	Argc_last;	/* for restarting plan_b */
static char	**Argv_last;

static FILE	*ofp = NULL;	/* output file pointer */
static FILE	*rejfp = NULL;	/* reject file pointer */

static int	filec = 0;	/* how many file arguments? */
static LINENUM	last_offset = 0;
static LINENUM	maxfuzz = 2;

#ifdef __APPLE__
enum vcsopt	vcsget = VCS_DEFAULT;

long	settime_gmtoff;
bool	settime;

time_t	mtime_old;
time_t	mtime_new;
#endif

/* patch using ifdef, ifndef, etc. */
static bool		do_defines = false;
/* #ifdef xyzzy */
static char		if_defined[128];
/* #ifndef xyzzy */
static char		not_defined[128];
/* #else */
static const char	else_defined[] = "#else\n";
/* #endif xyzzy */
static char		end_defined[128];

/* Apply a set of diffs as appropriate. */

int
main(int argc, char *argv[])
{
	struct stat statbuf;
#ifdef __APPLE__
	time_t	orig_mtime;
	int	error = 0, hunk, failed, i, fd, mismatch;
#else
	int	error = 0, hunk, failed, i, fd;
#endif
	bool	out_creating, out_existed, patch_seen, remove_file;
	bool	reverse_seen;
	LINENUM	where = 0, newwhere, fuzz, mymaxfuzz;
	const	char *tmpdir;
	char	*v;

	setvbuf(stdout, NULL, _IOLBF, 0);
	setvbuf(stderr, NULL, _IOLBF, 0);
	for (i = 0; i < MAXFILEC; i++)
		filearg[i] = NULL;

	buf_size = INITLINELEN;
	buf = malloc((unsigned)(buf_size));
	if (buf == NULL)
		fatal("out of memory\n");

	/* Cons up the names of the temporary files.  */
	if ((tmpdir = getenv("TMPDIR")) == NULL || *tmpdir == '\0')
		tmpdir = _PATH_TMP;
	for (i = strlen(tmpdir) - 1; i > 0 && tmpdir[i] == '/'; i--)
		;
	i++;
	if (asprintf(&TMPOUTNAME, "%.*s/patchoXXXXXXXXXX", i, tmpdir) == -1)
		fatal("cannot allocate memory");
	if ((fd = mkstemp(TMPOUTNAME)) < 0)
		pfatal("can't create %s", quoted_name(TMPOUTNAME));
	close(fd);

	if (asprintf(&TMPINNAME, "%.*s/patchiXXXXXXXXXX", i, tmpdir) == -1)
		fatal("cannot allocate memory");
	if ((fd = mkstemp(TMPINNAME)) < 0)
		pfatal("can't create %s", quoted_name(TMPINNAME));
	close(fd);

	if (asprintf(&TMPREJNAME, "%.*s/patchrXXXXXXXXXX", i, tmpdir) == -1)
		fatal("cannot allocate memory");
	if ((fd = mkstemp(TMPREJNAME)) < 0)
		pfatal("can't create %s", quoted_name(TMPREJNAME));
	close(fd);

	if (asprintf(&TMPPATNAME, "%.*s/patchpXXXXXXXXXX", i, tmpdir) == -1)
		fatal("cannot allocate memory");
	if ((fd = mkstemp(TMPPATNAME)) < 0)
		pfatal("can't create %s", quoted_name(TMPPATNAME));
	close(fd);

	v = getenv("SIMPLE_BACKUP_SUFFIX");
	if (v)
		simple_backup_suffix = v;
	else
		simple_backup_suffix = ORIGEXT;

#ifdef __APPLE__
	v = getenv("PATCH_VERBOSE");
	if (v != NULL) {
		long  pverbose;
		char *endp;

		errno = 0;
		pverbose = strtol(v, &endp, 10);
		if (errno != 0 || *endp != '\0')
			fatal("bad value for PATCH_VERBOSE: %s\n", v);

		verbose = pverbose != 0;
	}
#endif

	/* parse switches */
	Argc = argc;
	Argv = argv;
	get_some_switches();

	if (!Vflag) {
#ifdef __APPLE__
		/*
		 * In conformance mode, we should disable all implicit backup
		 * behavior unless it's been requested.  The -b,
		 * --backup-if-mismatch, and -V flags will all set backup_type
		 * as they're processed.  We want --posix/POSIXLY_CORRECT to
		 * override an implicit --backup-if-mismatch, but not an
		 * explicit one.
		 *
		 * Note that currently, `posix` does not reflect any value of
		 * COMMAND_MODE that is put in place!  This is bug-for-bug
		 * compatible with the previous patch(1) implementation!  The
		 * default of --backup-if-mismatch did not appear to be
		 * influenced by differing values of COMMAND_MODE that were
		 * tested.
		 *
		 * This would appear to be a gray area in the specification, so
		 * we may need to revisit this at a later date if the tests
		 * start checking that backups aren't created in the fuzz/offset
		 * cases.
		 */
		if (posix && backup_type == none)
			backup_mismatch = false;
#endif
		if ((v = getenv("PATCH_VERSION_CONTROL")) == NULL)
			v = getenv("VERSION_CONTROL");
		if (v != NULL || !posix)
			backup_type = get_version(v);	/* OK to pass NULL. */
	}

#ifdef __APPLE__
	if (vcsget == VCS_DEFAULT) {
		if ((v = getenv("PATCH_GET")) != NULL)
			vcsget = parse_vcs_option(v);
		else
			vcsget = VCS_DISABLED;
	}
#endif

	/* make sure we clean up /tmp in case of disaster */
	set_signals(0);

	patch_seen = false;
	for (open_patch_file(filearg[1]); there_is_another_patch();
	    reinitialize_almost_everything()) {
		/* for each patch in patch file */

		if (source_file != NULL && (diff_type == CONTEXT_DIFF ||
		    diff_type == NEW_CONTEXT_DIFF ||
		    diff_type == UNI_DIFF))
			out_creating = strcmp(source_file, _PATH_DEVNULL) == 0;
		else
			out_creating = false;
		patch_seen = true;

		warn_on_invalid_line = true;

		if (outname == NULL)
			outname = xstrdup(filearg[0]);

		/*
		 * At this point, we know if we're supposed to be creating the
		 * file and we know if we should be trying to handle a conflict
		 * between the patch and the file already existing.  We defer
		 * handling it until hunk processing because we want to swap
		 * the hunk if they opt to reverse it, but we want to make sure
		 * we *can* swap the hunk without running into memory issues
		 * before we offer it.  We also want to be verbose if flags or
		 * user decision cause us to skip -- this is explained a little
		 * more later.
		 */
		out_existed = stat(outname, &statbuf) == 0;

#ifdef __APPLE__
		orig_mtime = out_existed ? statbuf.st_mtimespec.tv_sec : 0;

#define	ALL_WRITE	(S_IWUSR | S_IWGRP | S_IWOTH)
		/*
		 * VCS logic is skipped here entirely if we're writing the
		 * result out to a separate file.  If the output ends up being
		 * read-only, then we'll take exception to that independently.
		 * The --get option generally assumes that we're applying a
		 * patch directly to a repository.
		 */
		if (vcsget != VCS_DISABLED &&
		    strcmp(filearg[0], outname) == 0) {
			int rv;
			bool is_ro;

			is_ro = false;
			if (out_existed)
				is_ro = access(outname, W_OK) < 0;

			/*
			 * We check the mode anyways because of the classic,
			 * it will be writable by root, problem.  If the write
			 * bit has been stripped, we should try to check it out
			 * anyways to be good citizens.
			 */
			if (out_existed && !is_ro &&
			    (statbuf.st_mode & ALL_WRITE) != 0)
				goto skipvcs;

			if (vcs_probe(outname, !out_existed, false) != 0) {
				if (out_existed && !is_ro)
					goto skipvcs;

				skip_rest_of_patch = true;
				say("%s cannot be written...\n", outname);
				goto skipvcs;
			}

			if (!vcs_supported()) {
				skip_rest_of_patch = true;
				say("%s cannot be written, and %s is not supported...\n",
				    outname, vcs_name());
				goto skipvcs;
			}

			if (vcsget == VCS_PROMPT && !vcs_prompt(outname))
				continue;

			if (check_only) {
				if (!out_existed) {
					skip_rest_of_patch = true;
					say("%s does not exist, but was found in %s.  Check it out before attempting a dry-run.\n",
					    outname, vcs_name());
				}

				goto skipvcs;
			}

			rv = vcs_checkout(outname, !out_existed);
			if (rv != 0) {
				skip_rest_of_patch = true;
				say("%s cannot be written, and a checkout from %s failed: %s\n",
				    outname, vcs_name(),
				    rv > 0 ? strerror(rv) : "unknown error");
				goto skipvcs;
			}
		}
skipvcs:
#undef ALL_WRITE
#endif	/* __APPLE__ */

		/* for ed script just up and do it and exit */
		if (diff_type == ED_DIFF) {
			do_ed_script();
			continue;
		}
		/* initialize the patched file */
		if (!skip_rest_of_patch)
			init_output(TMPOUTNAME);

		/* initialize reject file */
		init_reject(TMPREJNAME);

		/* find out where all the lines are */
		if (!skip_rest_of_patch)
			scan_input(filearg[0]);

		/*
		 * from here on, open no standard i/o files, because
		 * malloc might misfire and we can't catch it easily
		 */

		/* apply each hunk of patch */
		hunk = 0;
		failed = 0;
#ifdef __APPLE__
		mismatch = 0;
#endif
		reverse_seen = false;
		out_of_mem = false;
		remove_file = false;
		while (another_hunk()) {
			assert(!out_creating || hunk == 0);
			hunk++;
			fuzz = 0;

			/*
			 * There are only three cases in handle_creation() that
			 * results in us skipping hunk location, in order:
			 *
			 * 1.) Potentially reversed but -f/--force'd,
			 * 2.) Potentially reversed but -N/--forward'd
			 * 3.) Reversed and the user's opted to not apply it.
			 *
			 * In all three cases, we still want to inform the user
			 * that we're ignoring it in the standard way, which is
			 * also tied to this hunk processing loop.
			 */
			if (out_creating)
				reverse_seen = handle_creation(out_existed,
				    &remove_file);

			mymaxfuzz = pch_context();
			if (maxfuzz < mymaxfuzz)
				mymaxfuzz = maxfuzz;
			if (!skip_rest_of_patch) {
				do {
					where = locate_hunk(fuzz);
					if (hunk == 1 && where == 0 && !force && !reverse_seen) {
						/* dwim for reversed patch? */
						if (!pch_swap()) {
							if (fuzz == 0)
								say("Not enough memory to try swapped hunk!  Assuming unswapped.\n");
							continue;
						}
						reverse = !reverse;
						/* try again */
						where = locate_hunk(fuzz);
						if (where == 0) {
							/* didn't find it swapped */
							if (!pch_swap())
								/* put it back to normal */
								fatal("lost hunk on alloc error!\n");
							reverse = !reverse;
						} else if (noreverse) {
							if (!pch_swap())
								/* put it back to normal */
								fatal("lost hunk on alloc error!\n");
							reverse = !reverse;
							say("Ignoring previously applied (or reversed) patch.\n");
							skip_rest_of_patch = true;
						} else if (batch) {
							if (verbose)
								say("%seversed (or previously applied) patch detected!  %s -R.",
								    reverse ? "R" : "Unr",
								    reverse ? "Assuming" : "Ignoring");
						} else {
							ask("%seversed (or previously applied) patch detected!  %s -R? [y] ",
							    reverse ? "R" : "Unr",
							    reverse ? "Assume" : "Ignore");
							if (*buf == 'n') {
								ask("Apply anyway? [n] ");
								if (*buf != 'y')
									skip_rest_of_patch = true;
								else
									reverse_seen = true;
								where = 0;
								reverse = !reverse;
								if (!pch_swap())
									/* put it back to normal */
									fatal("lost hunk on alloc error!\n");
							}
						}
					}
				} while (!skip_rest_of_patch && where == 0 &&
				    ++fuzz <= mymaxfuzz);

				if (skip_rest_of_patch) {	/* just got decided */
					if (ferror(ofp) || fclose(ofp)) {
						say("Error writing %s\n",
						    quoted_name(TMPOUTNAME));
						error = 1;
					}
					ofp = NULL;
				}
			}
			newwhere = pch_newfirst() + last_offset;
			if (skip_rest_of_patch) {
				abort_hunk();
				failed++;
				if (verbose)
					say("Hunk #%d ignored at %ld.\n",
					    hunk, newwhere);
			} else if (where == 0) {
				abort_hunk();
				failed++;
				if (verbose)
					say("Hunk #%d failed at %ld.\n",
					    hunk, newwhere);
			} else {
				apply_hunk(where);

#ifdef __APPLE__
				if (fuzz != 0 || last_offset != 0)
					mismatch++;
#endif
				if (verbose) {
					say("Hunk #%d succeeded at %ld",
					    hunk, newwhere);
					if (fuzz != 0)
						say(" with fuzz %ld", fuzz);
					if (last_offset)
						say(" (offset %ld line%s)",
						    last_offset,
						    last_offset == 1L ? "" : "s");
					say(".\n");
				}
			}
		}

		if (out_of_mem && using_plan_a) {
			Argc = Argc_last;
			Argv = Argv_last;
			say("\n\nRan out of memory using Plan A--trying again...\n\n");
			if (ofp)
				fclose(ofp);
			ofp = NULL;
			if (rejfp)
				fclose(rejfp);
			rejfp = NULL;
			continue;
		}
		if (hunk == 0)
			fatal("Internal error: hunk should not be 0\n");

		/* finish spewing out the new file */
		if (!skip_rest_of_patch && !spew_output()) {
			say("Can't write %s\n", quoted_name(TMPOUTNAME));
			error = 1;
		}

		/* and put the output where desired */
		ignore_signals();
		if (!skip_rest_of_patch) {
			char	*realout = outname;

			if (!check_only) {
#ifdef __APPLE__
				time_t compare_mtime, set_mtime;
				bool do_backup = backup_requested;
				bool file_matched = failed == 0 &&
				    mismatch == 0;

				if (!do_backup && backup_mismatch &&
				    !file_matched)
					do_backup = true;
				if (move_file(TMPOUTNAME, outname,
				     do_backup) < 0) {
#else
				if (move_file(TMPOUTNAME, outname) < 0) {
#endif
					toutkeep = true;
					realout = TMPOUTNAME;
					chmod(TMPOUTNAME, filemode);
				} else
					chmod(outname, filemode);

				/*
				 * remove_file is a per-patch flag indicating
				 * whether it's OK to remove the empty file.
				 * This is specifically set when we're reversing
				 * the creation of a file and it ends up empty.
				 * This is an exception to the global policy
				 * (remove_empty_files) because the user would
				 * likely not expect the reverse of file
				 * creation to leave an empty file laying
				 * around.
				 */
				if ((remove_empty_files || remove_file) &&
				    stat(realout, &statbuf) == 0 &&
				    statbuf.st_size == 0) {
					if (verbose)
						say("Removing %s (empty after patching).\n",
						    quoted_name(realout));
					unlink(realout);
				}
#ifdef __APPLE__
				else if (settime && (force || file_matched)) {
					if (reverse) {
						compare_mtime = mtime_new;
						set_mtime = mtime_old;
					} else {
						compare_mtime = mtime_old;

						/*
						 * If the file didn't previously
						 * exist, we'll give it a pass
						 * and set the timestamp if it
						 * was requested if we were
						 * supposed to be creating it.
						 *
						 * The mtime of /dev/null will
						 * never match.
						 */
						if (out_creating)
							compare_mtime = 0;
						set_mtime = mtime_new;
					}

					if (force ||
					    orig_mtime == compare_mtime) {
						struct timeval times[2] = { 0 };

						times[0].tv_sec = set_mtime;
						times[1].tv_sec = set_mtime;
						utimes(outname, times);
					}
				}
#endif

			}
		}
		if (ferror(rejfp) || fclose(rejfp)) {
			say("Error writing %s\n", quoted_name(rejname));
			error = 1;
		}
		rejfp = NULL;
		if (failed) {
			error = 1;
			if (*rejname == '\0') {
				if (strlcpy(rejname, outname,
				    sizeof(rejname)) >= sizeof(rejname))
					fatal("filename %s is too long\n",
					    quoted_name(outname));
				if (strlcat(rejname, REJEXT,
				    sizeof(rejname)) >= sizeof(rejname))
					fatal("filename %s is too long\n",
					    quoted_name(outname));
			}
			if (!check_only)
				say("%d out of %d hunks %s--saving rejects to %s\n",
				    failed, hunk, skip_rest_of_patch ? "ignored" : "failed",
				    quoted_name(rejname));
			else
				say("%d out of %d hunks %s while patching %s\n",
				    failed, hunk, skip_rest_of_patch ? "ignored" : "failed",
				    quoted_name(filearg[0]));
#ifdef __APPLE__
			if (!check_only && move_file(TMPREJNAME, rejname,
			    true) < 0)
#else
			if (!check_only && move_file(TMPREJNAME, rejname) < 0)
#endif
				trejkeep = true;
		}
		set_signals(1);
	}

	if (!patch_seen && nonempty_patchf_seen)
		error = 2;

	my_exit(error);
	/* NOTREACHED */
}

/* Prepare to find the next patch to do in the patch file. */

static void
reinitialize_almost_everything(void)
{
	re_patch();
	re_input();

	input_lines = 0;
	last_frozen_line = 0;

	filec = 0;
	if (!out_of_mem) {
		free(filearg[0]);
		filearg[0] = NULL;
	}

	free(source_file);
	source_file = NULL;

	free(outname);
	outname = NULL;

	last_offset = 0;
	diff_type = 0;

	free(revision);
	revision = NULL;

	reverse = reverse_flag_specified;
	skip_rest_of_patch = false;

	get_some_switches();
}

#ifdef __APPLE__
enum {
	BINARY_OPT = CHAR_MAX + 1,
	BACKUP_MISMATCH,
	NO_BACKUP_MISMATCH,
	HELP_OPT,
	QUOTE_OPT,
};
#endif

/* Process switches and filenames. */

#ifdef __APPLE__
static enum vcsopt
parse_vcs_option(const char *optval)
{
	char *endp;
	int gopt;

	errno = 0;
	gopt = (int)strtol(optval, &endp, 10);
	if (errno != 0 || *endp != '\0')
		fatal("invalid --get value: %s, expected number", optval);
	if (gopt < 0)
		return (VCS_PROMPT);
	else if (gopt == 0)
		return (VCS_DISABLED);
	return (VCS_ALWAYS);
}

static int
parse_quote_option(const char *opt)
{

	if (opt == NULL)
		opt = getenv("QUOTING_STYLE");
	if (opt == NULL)
		opt = "shell";

	if (strcmp(opt, "shell") == 0)
		quote_opt = QO_SHELL;
	else if (strcmp(opt, "literal") == 0)
		quote_opt = QO_LITERAL;
	else if (strcmp(opt, "shell-always") == 0)
		quote_opt = QO_SHELL_ALWAYS;
	else if (strcmp(opt, "c") == 0)
		quote_opt = QO_C;
	else if (strcmp(opt, "escape") == 0)
		quote_opt = QO_ESCAPE;
	else
		return (EINVAL);

	return (0);
}
#endif

static void
get_some_switches(void)
{
#ifdef __APPLE__
	const char *options = "b::B:cCd:D:eEfF:g:i:lnNo:p:r:RstTuvV:x:Y:z:Z";
#else
	const char *options = "b::B:cCd:D:eEfF:i:lnNo:p:r:RstuvV:x:z:";
#endif
	static struct option longopts[] = {
		{"backup",		no_argument,		0,	'b'},
#ifdef __APPLE__
		{"backup-if-mismatch",	no_argument,		0,
		    BACKUP_MISMATCH},
		{"basename-prefix",	required_argument,	0,	'Y'},
#endif
		{"batch",		no_argument,		0,	't'},
		{"check",		no_argument,		0,	'C'},
		{"context",		no_argument,		0,	'c'},
		{"debug",		required_argument,	0,	'x'},
		{"directory",		required_argument,	0,	'd'},
		{"dry-run",		no_argument,		0,	'C'},
		{"ed",			no_argument,		0,	'e'},
		{"force",		no_argument,		0,	'f'},
		{"forward",		no_argument,		0,	'N'},
		{"fuzz",		required_argument,	0,	'F'},
#ifdef __APPLE__
		{"get",			required_argument,	0,	'g'},
		{"help",		no_argument,		0,
		    HELP_OPT},
#endif
		{"ifdef",		required_argument,	0,	'D'},
		{"input",		required_argument,	0,	'i'},
		{"ignore-whitespace",	no_argument,		0,	'l'},
#ifdef __APPLE__
		{"no-backup-if-mismatch",	no_argument,	0,
		    NO_BACKUP_MISMATCH},
#endif
		{"normal",		no_argument,		0,	'n'},
		{"output",		required_argument,	0,	'o'},
		{"prefix",		required_argument,	0,	'B'},
		{"quiet",		no_argument,		0,	's'},
#ifdef __APPLE__
		{"quoting-style",	required_argument,	0,
		    QUOTE_OPT},
#endif
		{"reject-file",		required_argument,	0,	'r'},
		{"remove-empty-files",	no_argument,		0,	'E'},
		{"reverse",		no_argument,		0,	'R'},
#ifdef __APPLE__
		{"set-time",		no_argument,		0,	'T'},
		{"set-utc",		no_argument,		0,	'Z'},
#endif
		{"silent",		no_argument,		0,	's'},
		{"strip",		required_argument,	0,	'p'},
		{"suffix",		required_argument,	0,	'z'},
		{"unified",		no_argument,		0,	'u'},
		{"version",		no_argument,		0,	'v'},
		{"version-control",	required_argument,	0,	'V'},
		{"posix",		no_argument,		&posix,	1},
#ifdef __APPLE__
		{"binary",		no_argument,		0,	BINARY_OPT},
		{"verbose",		no_argument,		&verbose, 1},
#endif
		{NULL,			0,			0,	0}
	};
#ifdef __APPLE__
	const char *quoting_style;
#endif
	int ch;

	rejname[0] = '\0';
#ifdef __APPLE__
	quoting_style = NULL;
#endif
	Argc_last = Argc;
	Argv_last = Argv;
	if (!Argc)
		return;
	optreset = optind = 1;
	while ((ch = getopt_long(Argc, Argv, options, longopts, NULL)) != -1) {
		switch (ch) {
#ifdef __APPLE__
		case BINARY_OPT:
			/* ignored */
			break;
#endif
		case 'b':
#ifdef __APPLE__
			backup_requested = true;
#endif
			if (backup_type == none)
				backup_type = numbered_existing;
			if (optarg == NULL)
				break;
			if (verbose)
				say("Warning, the ``-b suffix'' option has been"
				    " obsoleted by the -z option.\n");
			/* FALLTHROUGH */
		case 'z':
			/* must directly follow 'b' case for backwards compat */
			simple_backup_suffix = xstrdup(optarg);
			break;
#ifdef __APPLE__
		case BACKUP_MISMATCH:
			if (backup_type == none)
				backup_type = numbered_existing;
			backup_mismatch = true;
			break;
		case NO_BACKUP_MISMATCH:
			backup_mismatch = false;
			break;
#endif
		case 'B':
			origprae = xstrdup(optarg);
			break;
		case 'c':
			diff_type = CONTEXT_DIFF;
			break;
		case 'C':
			check_only = true;
			break;
		case 'd':
			if (chdir(optarg) < 0)
				pfatal("can't cd to %s", optarg);
			break;
		case 'D':
			do_defines = true;
			if (!isalpha((unsigned char)*optarg) && *optarg != '_')
				fatal("argument to -D is not an identifier\n");
			snprintf(if_defined, sizeof if_defined,
			    "#ifdef %s\n", optarg);
			snprintf(not_defined, sizeof not_defined,
			    "#ifndef %s\n", optarg);
			snprintf(end_defined, sizeof end_defined,
			    "#endif /* %s */\n", optarg);
			break;
		case 'e':
			diff_type = ED_DIFF;
			break;
		case 'E':
			remove_empty_files = true;
			break;
		case 'f':
			force = true;
			break;
		case 'F':
			maxfuzz = atoi(optarg);
			break;
#ifdef __APPLE__
		case 'g':
			vcsget = parse_vcs_option(optarg);
			break;
		case HELP_OPT:
			usage(stdout, 0);
			break;
#endif
		case 'i':
			if (++filec == MAXFILEC)
				fatal("too many file arguments\n");
			filearg[filec] = xstrdup(optarg);
			break;
		case 'l':
			canonicalize = true;
			break;
		case 'n':
			diff_type = NORMAL_DIFF;
			break;
		case 'N':
			noreverse = true;
			break;
		case 'o':
			outname = xstrdup(optarg);
			break;
		case 'p':
			strippath = atoi(optarg);
			break;
#ifdef __APPLE__
		case QUOTE_OPT:
			quoting_style = optarg;
			break;
#endif
		case 'r':
			if (strlcpy(rejname, optarg,
			    sizeof(rejname)) >= sizeof(rejname))
				fatal("argument for -r is too long\n");
			break;
		case 'R':
			reverse = true;
			reverse_flag_specified = true;
			break;
		case 's':
			verbose = false;
#ifdef __APPLE__
			quiet = true;
#endif
			break;
		case 't':
			batch = true;
			break;
#ifdef __APPLE__
		case 'T': {
			struct tm *tm;
			time_t now;

			if (settime)
				fatal("-T and -Z are mutually exclusive options\n");
			now = time(NULL);
			tm = localtime(&now);
			settime_gmtoff = tm->tm_gmtoff;
			settime = true;
			break;
		}
#endif
		case 'u':
			diff_type = UNI_DIFF;
			break;
		case 'v':
			version();
			break;
		case 'V':
			backup_type = get_version(optarg);
#ifdef __APPLE__
			backup_requested = true;
#endif
			Vflag = true;
			break;
#ifdef DEBUGGING
		case 'x':
			debug = atoi(optarg);
			break;
#endif
#ifdef __APPLE__
		case 'Y':
			simple_backup_prefix = xstrdup(optarg);
			break;
		case 'Z':
			if (settime)
				fatal("-T and -Z are mutually exclusive options\n");
			settime_gmtoff = 0;
			settime = true;
			break;
#endif
		default:
			if (ch != '\0')
#ifdef __APPLE__
				usage(stderr, EXIT_FAILURE);
#else
				usage();
#endif
			break;
		}
	}
	Argc -= optind;
	Argv += optind;

#ifdef __APPLE__
	/*
	 * --verbose disables --quiet, unsetting it here reduces the patching of
	 * upstream patch(1).
	 */
	if (verbose)
		quiet = false;
#endif

#ifdef __APPLE__
	if (parse_quote_option(quoting_style) != 0)
		fatal("invalid quoting style '%s'", quoting_style);
#endif

	if (Argc > 0) {
		filearg[0] = xstrdup(*Argv++);
		Argc--;
		while (Argc > 0) {
			if (++filec == MAXFILEC)
				fatal("too many file arguments\n");
			filearg[filec] = xstrdup(*Argv++);
			Argc--;
		}
	}

	if (getenv("POSIXLY_CORRECT") != NULL)
		posix = 1;
}

static void
#ifdef __APPLE__
usage(FILE *outfp, int code)
#else
usage(void)
#endif
{
#ifdef __APPLE__
	fprintf(outfp,
#else
	fprintf(stderr,
#endif
"usage: patch [-bCcEeflNnRstuv] [-B backup-prefix] [-D symbol] [-d directory]\n"
#ifdef __APPLE__
"             [-g vcs-option] [-F max-fuzz] [-i patchfile] [-o out-file]\n"
"             [-p strip-count] [-r rej-name] [-T | -Z]\n"
"             [-V t | nil | never | none] [-x number] [-Y prefix]\n"
"             [-z backup-ext] [--quoting-style style] [--posix]\n"
"             [origfile [patchfile]]\n"
#else
"             [-F max-fuzz] [-i patchfile] [-o out-file] [-p strip-count]\n"
"             [-r rej-name] [-V t | nil | never | none] [-x number]\n"
"             [-z backup-ext] [--posix] [origfile [patchfile]]\n"
#endif
"       patch <patchfile\n");
#ifdef __APPLE__
	my_exit(code);
#else
	my_exit(EXIT_FAILURE);
#endif
}

/*
 * Attempt to find the right place to apply this hunk of patch.
 */
static LINENUM
locate_hunk(LINENUM fuzz)
{
	LINENUM	first_guess = pch_first() + last_offset;
	LINENUM	offset;
	LINENUM	pat_lines = pch_ptrn_lines();
	LINENUM	max_pos_offset = input_lines - first_guess - pat_lines + 1;
	LINENUM	max_neg_offset = first_guess - last_frozen_line - 1 + pch_context();

	if (pat_lines == 0) {		/* null range matches always */
		if (verbose && fuzz == 0 && (diff_type == CONTEXT_DIFF
		    || diff_type == NEW_CONTEXT_DIFF
		    || diff_type == UNI_DIFF)) {
			say("Empty context always matches.\n");
		}
		return (first_guess);
	}
	if (max_neg_offset >= first_guess)	/* do not try lines < 0 */
		max_neg_offset = first_guess - 1;
	if (first_guess <= input_lines && patch_match(first_guess, 0, fuzz))
		return first_guess;
	for (offset = 1; ; offset++) {
		bool	check_after = (offset <= max_pos_offset);
		bool	check_before = (offset <= max_neg_offset);

		if (check_after && patch_match(first_guess, offset, fuzz)) {
#ifdef DEBUGGING
			if (debug & 1)
				say("Offset changing from %ld to %ld\n",
				    last_offset, offset);
#endif
			last_offset = offset;
			return first_guess + offset;
		} else if (check_before && patch_match(first_guess, -offset, fuzz)) {
#ifdef DEBUGGING
			if (debug & 1)
				say("Offset changing from %ld to %ld\n",
				    last_offset, -offset);
#endif
			last_offset = -offset;
			return first_guess - offset;
		} else if (!check_before && !check_after)
			return 0;
	}
}

/* We did not find the pattern, dump out the hunk so they can handle it. */

static void
abort_context_hunk(void)
{
	LINENUM	i;
	const LINENUM	pat_end = pch_end();
	/*
	 * add in last_offset to guess the same as the previous successful
	 * hunk
	 */
	const LINENUM	oldfirst = pch_first() + last_offset;
	const LINENUM	newfirst = pch_newfirst() + last_offset;
	const LINENUM	oldlast = oldfirst + pch_ptrn_lines() - 1;
	const LINENUM	newlast = newfirst + pch_repl_lines() - 1;
	const char	*stars = (diff_type >= NEW_CONTEXT_DIFF ? " ****" : "");
	const char	*minuses = (diff_type >= NEW_CONTEXT_DIFF ? " ----" : " -----");

	fprintf(rejfp, "***************\n");
	for (i = 0; i <= pat_end; i++) {
		switch (pch_char(i)) {
		case '*':
			if (oldlast < oldfirst)
				fprintf(rejfp, "*** 0%s\n", stars);
			else if (oldlast == oldfirst)
				fprintf(rejfp, "*** %ld%s\n", oldfirst, stars);
			else
				fprintf(rejfp, "*** %ld,%ld%s\n", oldfirst,
				    oldlast, stars);
			break;
		case '=':
			if (newlast < newfirst)
				fprintf(rejfp, "--- 0%s\n", minuses);
			else if (newlast == newfirst)
				fprintf(rejfp, "--- %ld%s\n", newfirst, minuses);
			else
				fprintf(rejfp, "--- %ld,%ld%s\n", newfirst,
				    newlast, minuses);
			break;
		case '\n':
#ifdef __APPLE__
			putline(i, rejfp);
#else
			fprintf(rejfp, "%s", pfetch(i));
#endif
			break;
		case ' ':
		case '-':
		case '+':
		case '!':
#ifdef __APPLE__
			fprintf(rejfp, "%c ", pch_char(i));
			putline(i, rejfp);
#else
			fprintf(rejfp, "%c %s", pch_char(i), pfetch(i));
#endif
			break;
		default:
			fatal("fatal internal error in abort_context_hunk\n");
		}
	}
}

static void
rej_line(int ch, LINENUM i)
{
	size_t len;
	const char *line = pfetch(i);

#ifdef __APPLE__
	/*
	 * We could use putline() here as well, but we need both the line itself
	 * and the size to track if we had a newline, while none of the other
	 * consumers do.
	 */
	len = pch_line_len(i);

	fputc(ch, rejfp);
	fwrite(line, 1, len, rejfp);
#else
	len = strlen(line);

	fprintf(rejfp, "%c%s", ch, line);
#endif
	if (len == 0 || line[len - 1] != '\n') {
		if (len >= USHRT_MAX)
			fprintf(rejfp, "\n\\ Line too long\n");
		else
			fprintf(rejfp, "\n\\ No newline at end of line\n");
	}
}

static void
abort_hunk(void)
{
	LINENUM		i, j, split;
	int		ch1, ch2;
	const LINENUM	pat_end = pch_end();
	const LINENUM	oldfirst = pch_first() + last_offset;
	const LINENUM	newfirst = pch_newfirst() + last_offset;

	if (diff_type != UNI_DIFF) {
		abort_context_hunk();
		return;
	}
	split = -1;
	for (i = 0; i <= pat_end; i++) {
		if (pch_char(i) == '=') {
			split = i;
			break;
		}
	}
	if (split == -1) {
		fprintf(rejfp, "malformed hunk: no split found\n");
		return;
	}
	i = 0;
	j = split + 1;
	fprintf(rejfp, "@@ -%ld,%ld +%ld,%ld @@\n",
	    pch_ptrn_lines() ? oldfirst : 0,
	    pch_ptrn_lines(), newfirst, pch_repl_lines());
	while (i < split || j <= pat_end) {
		ch1 = i < split ? pch_char(i) : -1;
		ch2 = j <= pat_end ? pch_char(j) : -1;
		if (ch1 == '-') {
			rej_line('-', i);
			i++;
		} else if (ch1 == ' ' && ch2 == ' ') {
			rej_line(' ', i);
			i++;
			j++;
		} else if (ch1 == '!' && ch2 == '!') {
			while (i < split && ch1 == '!') {
				rej_line('-', i);
				i++;
				ch1 = i < split ? pch_char(i) : -1;
			}
			while (j <= pat_end && ch2 == '!') {
				rej_line('+', j);
				j++;
				ch2 = j <= pat_end ? pch_char(j) : -1;
			}
		} else if (ch1 == '*') {
			i++;
		} else if (ch2 == '+' || ch2 == ' ') {
			rej_line(ch2, j);
			j++;
		} else {
			fprintf(rejfp, "internal error on (%ld %ld %ld)\n",
			    i, split, j);
			rej_line(ch1, i);
			rej_line(ch2, j);
			return;
		}
	}
}

#ifdef __APPLE__
/*
 * putline -- write a line from the patch file out to fp.  This avoids assuming
 * the length of the fetched line, explicitly grabbing it and writing it out
 * char-by-char so that we're not breaking any internal NULs (e.g., UTF-16).
 *
 * Returns true on success or false on EOF or write error.
 */
static bool
putline(LINENUM line, FILE *fp)
{
	size_t len = pch_line_len(line);

	return (fwrite(pfetch(line), 1, len, fp) == len);
}
#endif

/* We found where to apply it (we hope), so do it. */

static void
apply_hunk(LINENUM where)
{
	LINENUM		old = 1;
	const LINENUM	lastline = pch_ptrn_lines();
	LINENUM		new = lastline + 1;
#define OUTSIDE 0
#define IN_IFNDEF 1
#define IN_IFDEF 2
#define IN_ELSE 3
	int		def_state = OUTSIDE;
	const LINENUM	pat_end = pch_end();

	where--;
	while (pch_char(new) == '=' || pch_char(new) == '\n')
		new++;

	while (old <= lastline) {
		if (pch_char(old) == '-') {
			copy_till(where + old - 1, false);
			if (do_defines) {
				if (def_state == OUTSIDE) {
					fputs(not_defined, ofp);
					def_state = IN_IFNDEF;
				} else if (def_state == IN_IFDEF) {
					fputs(else_defined, ofp);
					def_state = IN_ELSE;
				}
#ifdef __APPLE__
				putline(old, ofp);
#else
				fputs(pfetch(old), ofp);
#endif
			}
			last_frozen_line++;
			old++;
		} else if (new > pat_end) {
			break;
		} else if (pch_char(new) == '+') {
			copy_till(where + old - 1, false);
			if (do_defines) {
				if (def_state == IN_IFNDEF) {
					fputs(else_defined, ofp);
					def_state = IN_ELSE;
				} else if (def_state == OUTSIDE) {
					fputs(if_defined, ofp);
					def_state = IN_IFDEF;
				}
			}
#ifdef __APPLE__
			putline(new, ofp);
#else
			fputs(pfetch(new), ofp);
#endif
			new++;
		} else if (pch_char(new) != pch_char(old)) {
			say("Out-of-sync patch, lines %ld,%ld--mangled text or line numbers, maybe?\n",
			    pch_hunk_beg() + old,
			    pch_hunk_beg() + new);
#ifdef DEBUGGING
			say("oldchar = '%c', newchar = '%c'\n",
			    pch_char(old), pch_char(new));
#endif
			my_exit(2);
		} else if (pch_char(new) == '!') {
			copy_till(where + old - 1, false);
			if (do_defines) {
				fputs(not_defined, ofp);
				def_state = IN_IFNDEF;
			}
			while (pch_char(old) == '!') {
				if (do_defines) {
#ifdef __APPLE__
					putline(old, ofp);
#else
					fputs(pfetch(old), ofp);
#endif
				}
				last_frozen_line++;
				old++;
			}
			if (do_defines) {
				fputs(else_defined, ofp);
				def_state = IN_ELSE;
			}
			while (pch_char(new) == '!') {
#ifdef __APPLE__
				putline(new, ofp);
#else
				fputs(pfetch(new), ofp);
#endif
				new++;
			}
		} else {
			if (pch_char(new) != ' ')
				fatal("Internal error: expected ' '\n");
			old++;
			new++;
			if (do_defines && def_state != OUTSIDE) {
				fputs(end_defined, ofp);
				def_state = OUTSIDE;
			}
		}
	}
	if (new <= pat_end && pch_char(new) == '+') {
		copy_till(where + old - 1, false);
		if (do_defines) {
			if (def_state == OUTSIDE) {
				fputs(if_defined, ofp);
				def_state = IN_IFDEF;
			} else if (def_state == IN_IFNDEF) {
				fputs(else_defined, ofp);
				def_state = IN_ELSE;
			}
		}
		while (new <= pat_end && pch_char(new) == '+') {
#ifdef __APPLE__
			putline(new, ofp);
#else
			fputs(pfetch(new), ofp);
#endif
			new++;
		}
	}
	if (do_defines && def_state != OUTSIDE) {
		fputs(end_defined, ofp);
	}
}

/*
 * Open the new file.
 */
static void
init_output(const char *name)
{
	ofp = fopen(name, "w");
	if (ofp == NULL)
		pfatal("can't create %s", quoted_name(name));
}

/*
 * Open a file to put hunks we can't locate.
 */
static void
init_reject(const char *name)
{
	rejfp = fopen(name, "w");
	if (rejfp == NULL)
		pfatal("can't create %s", quoted_name(name));
}

/*
 * Copy input file to output, up to wherever hunk is to be applied.
 * If endoffile is true, treat the last line specially since it may
 * lack a newline.
 */
static void
copy_till(LINENUM lastline, bool endoffile)
{
	if (last_frozen_line > lastline)
		fatal("misordered hunks! output would be garbled\n");
	while (last_frozen_line < lastline) {
		if (++last_frozen_line == lastline && endoffile)
			dump_line(last_frozen_line, !last_line_missing_eol);
		else
			dump_line(last_frozen_line, true);
	}
}

/*
 * Finish copying the input file to the output file.
 */
static bool
spew_output(void)
{
	int rv;

#ifdef DEBUGGING
	if (debug & 256)
		say("il=%ld lfl=%ld\n", input_lines, last_frozen_line);
#endif
	if (input_lines)
		copy_till(input_lines, true);	/* dump remainder of file */
	rv = ferror(ofp) == 0 && fclose(ofp) == 0;
	ofp = NULL;
	return rv;
}

/*
 * Copy one line from input to output.
 */
static void
dump_line(LINENUM line, bool write_newline)
{
	char	*s;

	s = ifetch(line, 0);
	if (s == NULL)
		return;
	/* Note: string is not NUL terminated. */
	for (; *s != '\n'; s++)
		putc(*s, ofp);
	if (write_newline)
		putc('\n', ofp);
}

/*
 * Does the patch pattern match at line base+offset?
 */
static bool
patch_match(LINENUM base, LINENUM offset, LINENUM fuzz)
{
	LINENUM		pline = 1 + fuzz;
	LINENUM		iline;
	LINENUM		pat_lines = pch_ptrn_lines() - fuzz;
#ifdef __APPLE__
	LINENUM		pat_leading_ctx = pch_leading_context();
	LINENUM		pat_trailing_ctx = pch_trailing_context();
	LINENUM		pat_ctx = MAX(pat_leading_ctx, pat_trailing_ctx);
#else
#endif
	const char	*ilineptr;
	const char	*plineptr;
	size_t		plinelen;

#ifdef __APPLE__
	/*
	 * If we have trailing context but no leading context, this is an
	 * asymmetry that indicates we're at the beginning of the file.  We
	 * cannot apply fuzz at the beginning of the file.
	 */
	if (pat_ctx != 0 && pat_leading_ctx == 0 && base + offset + fuzz != 1)
		return false;
#endif
	/* Patch does not match if we don't have any more context to use */
	if (pline > pat_lines)
		return false;
	for (iline = base + offset + fuzz; pline <= pat_lines; pline++, iline++) {
		ilineptr = ifetch(iline, offset >= 0);
		if (ilineptr == NULL)
			return false;
		plineptr = pfetch(pline);
		plinelen = pch_line_len(pline);
		if (canonicalize) {
			if (!similar(ilineptr, plineptr, plinelen))
				return false;
#ifdef __APPLE__
		} else if (bstrnNE(ilineptr, plineptr, plinelen))
#else
		} else if (strnNE(ilineptr, plineptr, plinelen))
#endif
			return false;
		if (iline == input_lines) {
			/*
			 * We are looking at the last line of the file.
			 * If the file has no eol, the patch line should
			 * not have one either and vice-versa. Note that
			 * plinelen > 0.
			 */
			if (last_line_missing_eol) {
				if (plineptr[plinelen - 1] == '\n')
					return false;
			} else {
				if (plineptr[plinelen - 1] != '\n')
					return false;
			}
		}
	}
#ifdef __APPLE__
	/*
	 * The other direction: some context but nothing trailing.  We must be
	 * at EOF and, again, we can't fuzz the end of the file.
	 */
	if (pat_ctx != 0 && pat_trailing_ctx == 0 && iline != input_lines + 1)
		return false;
#endif
	return true;
}

/*
 * Do two lines match with canonicalized white space?
 */
static bool
similar(const char *a, const char *b, int len)
{
	while (len) {
		if (isspace((unsigned char)*b)) {	/* whitespace (or \n) to match? */
			if (!isspace((unsigned char)*a))	/* no corresponding whitespace? */
				return false;
			while (len && isspace((unsigned char)*b) && *b != '\n')
				b++, len--;	/* skip pattern whitespace */
			while (isspace((unsigned char)*a) && *a != '\n')
				a++;	/* skip target whitespace */
			if (*a == '\n' || *b == '\n')
				return (*a == *b);	/* should end in sync */
		} else if (*a++ != *b++)	/* match non-whitespace chars */
			return false;
		else
			len--;	/* probably not necessary */
	}
	return true;		/* actually, this is not reached */
	/* since there is always a \n */
}

static bool
handle_creation(bool out_existed, bool *remove)
{
	bool reverse_seen;

	reverse_seen = false;
	if (reverse && out_existed) {
		/*
		 * If the patch creates the file and we're reversing the patch,
		 * then we need to indicate to the patch processor that it's OK
		 * to remove this file.
		 */
		*remove = true;
	} else if (!reverse && out_existed) {
		/*
		 * Otherwise, we need to blow the horn because the patch appears
		 * to be reversed/already applied.  For non-batch jobs, we'll
		 * prompt to figure out what we should be trying to do to raise
		 * awareness of the issue.  batch (-t) processing suppresses the
		 * questions and just assumes that we're reversed if it looks
		 * like we are, which is always the case if we've reached this
		 * branch.
		 */
		if (force) {
			skip_rest_of_patch = true;
			return (false);
		}
		if (noreverse) {
			/* If -N is supplied, however, we bail out/ignore. */
			say("Ignoring previously applied (or reversed) patch.\n");
			skip_rest_of_patch = true;
			return (false);
		}

		/* Unreversed... suspicious if the file existed. */
		if (!pch_swap())
			fatal("lost hunk on alloc error!\n");

		reverse = !reverse;

		if (batch) {
			if (verbose)
				say("Patch creates file that already exists, %s %seversed",
				    reverse ? "Assuming" : "Ignoring",
				    reverse ? "R" : "Unr");
		} else {
			ask("Patch creates file that already exists!  %s -R? [y] ",
			    reverse ? "Assume" : "Ignore");

			if (*buf == 'n') {
				ask("Apply anyway? [n]");
				if (*buf != 'y')
					/* Don't apply; error out. */
					skip_rest_of_patch = true;
				else
					/* Attempt to apply. */
					reverse_seen = true;
				reverse = !reverse;
				if (!pch_swap())
					fatal("lost hunk on alloc error!\n");
			} else {
				/*
				 * They've opted to assume -R; effectively the
				 * same as the first branch in this function,
				 * but the decision is here rather than in a
				 * prior patch/hunk as in that branch.
				 */
				*remove = true;
			}
		}
	}

	/*
	 * The return value indicates if we offered a chance to reverse but the
	 * user declined.  This keeps the main patch processor in the loop since
	 * we've taken this out of the normal flow of hunk processing to
	 * simplify logic a little bit.
	 */
	return (reverse_seen);
}
