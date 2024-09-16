/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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

#include <sys/stat.h>

#include <assert.h>
#include <ctype.h>
#if HAVE_ERR
# include <err.h>
#endif
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

#define	RSYNC_PATH	"rsync"

const char *
alt_base_mode(int mode)
{
	switch (mode) {
	case BASE_MODE_COMPARE:
		return "--compare-dest";
	case BASE_MODE_COPY:
		return "--copy-dest";
	case BASE_MODE_LINK:
		return "--link-dest";
	default:
		errx(1, "unknown base mode %d", mode);
	}
}

/*
 * Parses the program, adding each word to the current arguments as it goes.
 */
static void
fargs_cmdline_prog(arglist *argsp, const char *prog)
{
	const char *arg, *end;
	char *mprog, *walker;
	char lastquote, quotec;

	mprog = strdup(prog);
	if (mprog == NULL)
		err(ERR_IPC, "strdup");

	end = &mprog[strlen(mprog) + 1];
	quotec = lastquote = '\0';
	for (arg = walker = mprog; *walker != '\0'; walker++) {
		/* Add what we have so far once we hit whitespace. */
		if (isspace(*walker)) {
			lastquote = '\0';
			addargs(argsp, "%.*s", (int)(walker - arg), arg);

			/* Skip entire sequence of whitespace. */
			while (isspace(*(walker + 1)))
				walker++;

			arg = walker + 1;
			continue;
		} else if (*walker == '"' || *walker == '\'') {
			char *search = walker + 1;

			quotec = *walker;

			/*
			 * Compatible with the reference rsync, but not with
			 * traditional shell style: we don't strip off the
			 * the beginning quote of the second quoted part of a
			 * single arg.
			 */
			if (arg == walker || quotec != lastquote) {
				memmove(walker, walker + 1, end - (walker + 1));
				search = walker;
				end--;
			}


			/*
			 * Skip to the closing quote; smb rsync doesn't seem to
			 * even try to deal with escaped quotes.  If we didn't
			 * find a closing quote, we'll bail out and report the
			 * error.
			 */
			walker = strchr(search, quotec);
			if (walker == NULL)
				break;

			/*
			 * We'll move the remainder of the string over and
			 * strip off the quote character, then take a step
			 * backward and let us process whichever quote just
			 * replaced our terminal quote.
			 */
			memmove(walker, walker + 1, end - (walker + 1));
			assert(walker > arg);
			end--;
			walker--;

			lastquote = quotec;
			quotec = '\0';

			continue;
		}
	}

	if (quotec != '\0') {
		free(mprog);
		errx(ERR_SYNTAX,
		    "Missing terminating `%c` in specified remote-shell command",
		    quotec);
	} else if (walker > arg) {
		addargs(argsp, "%.*s", (int)(walker - arg), arg);
	}

	free(mprog);
}

static int
fargs_is_ssh(const char *prog)
{
	const char *base;

	/*
	 * In theory we should have a program to inspect, but the reference
	 * rsync seems to accept an empty --rsh and uses the machine name as the
	 * first argument, for better or worse.
	 */
	if (prog == NULL)
		return 0;

	base = strrchr(prog, '/');
	if (base == NULL)
		base = prog;
	else
		base++;

	return strcmp(base, "ssh") == 0;
}

char **
fargs_cmdline(struct sess *sess, const struct fargs *f, size_t *skip)
{
	arglist		 args;
	size_t		 j;
	char		*rsync_path;

	memset(&args, 0, sizeof args);

	assert(f != NULL);
	assert(f->sourcesz > 0 || f->mode != FARGS_RECEIVER);

	if ((rsync_path = sess->opts->rsync_path) == NULL)
		rsync_path = (char *)RSYNC_PATH;

	if (f->host != NULL) {
		const char *rsh_prog;

		rsh_prog = sess->opts->ssh_prog;
		if (rsh_prog == NULL)
			rsh_prog = getenv("RSYNC_RSH");

		/*
		 * Splice arguments from -e "foo bar baz" into array
		 * elements required for execve(2).
		 * This doesn't do anything fancy: it splits along
		 * whitespace into the array.
		 */

		if (rsh_prog != NULL)
			fargs_cmdline_prog(&args, rsh_prog);
		else
			addargs(&args, "ssh");

		if (sess->opts->ipf > 0 && fargs_is_ssh(getarg(&args, 0)))
			addargs(&args, "-%d", sess->opts->ipf);

		addargs(&args, "%s", f->host);
		fargs_cmdline_prog(&args, rsync_path);
		if (skip)
			*skip = args.num;
		addargs(&args, "--server");
		if (f->mode == FARGS_RECEIVER)
			addargs(&args, "--sender");
	} else {
		fargs_cmdline_prog(&args, rsync_path);
		addargs(&args, "--server");
	}

	/* Shared arguments. */

	if (sess->opts->del) {
		switch (sess->opts->del) {
		case DMODE_UNSPECIFIED:
			addargs(&args, "--delete");
			break;
		case DMODE_BEFORE:
			addargs(&args, "--delete-before");
			break;
		case DMODE_DURING:
			addargs(&args, "--delete-during");
			break;
		case DMODE_DELAY:
			addargs(&args, "--delete-delay");
			break;
		case DMODE_AFTER:
			addargs(&args, "--delete-after");
			break;
		default:
			errx(1, "bogus delete mode %d\n", sess->opts->del);
		}
	}
	if (sess->opts->append)
		addargs(&args, "--append");
	if (sess->opts->checksum)
		addargs(&args, "-c");
	if (sess->seed != 0)
		addargs(&args, "--checksum-seed=%d", sess->seed);
	if (sess->opts->del_excl)
		addargs(&args, "--delete-excluded");
	if (sess->opts->numeric_ids == NIDS_FULL)
		addargs(&args, "--numeric-ids");
	if (sess->opts->preserve_gids)
		addargs(&args, "-g");
	if (sess->opts->preserve_links)
		addargs(&args, "-l");
	if (sess->opts->dry_run == DRY_FULL)
		addargs(&args, "-n");
	if (sess->opts->inplace)
		addargs(&args, "--inplace");

	if (sess->opts->partial_dir != NULL && f->mode == FARGS_SENDER) {
		/* Implied --partial for brevity. */
		addargs(&args, "--partial-dir");
		addargs(&args, "%s", sess->opts->partial_dir);
	} else if (sess->opts->partial && f->mode == FARGS_SENDER) {
		/* Explicit --partial since we have no --partial-dir. */
		addargs(&args, "--partial");
	}

	if (sess->opts->preserve_uids)
		addargs(&args, "-o");
	if (sess->opts->preserve_perms)
		addargs(&args, "-p");
	if (sess->opts->devices)
		addargs(&args, "-D");
	if (sess->opts->recursive)
		addargs(&args, "-r");
	if (sess->opts->preserve_times)
		addargs(&args, "-t");
	if (sess->opts->omit_dir_times)
		addargs(&args, "-O");
	if (sess->opts->sparse)
		addargs(&args, "-S");
	if (sess->opts->hard_links)
		addargs(&args, "-H");
	if (sess->opts->update)
		addargs(&args, "-u");
	if (verbose > 3)
		addargs(&args, "-v");
	if (verbose > 2)
		addargs(&args, "-v");
	if (verbose > 1)
		addargs(&args, "-v");
	if (verbose > 0)
		addargs(&args, "-v");
	if (sess->opts->human_readable > 1)
		addargs(&args, "-h");
	if (sess->opts->human_readable > 0)
		addargs(&args, "-h");
	if (sess->opts->whole_file > 0 && !sess->opts->append)
		addargs(&args, "-W");
	if (sess->opts->progress > 0)
		addargs(&args, "--progress");
	if (sess->opts->backup > 0)
		addargs(&args, "--backup");
	if (sess->opts->backup_dir != NULL) {
		addargs(&args, "--backup-dir");
		addargs(&args, "%s", sess->opts->backup_dir);
	}
	if (sess->opts->backup_suffix != NULL &&
	    strcmp(sess->opts->backup_suffix, "~") != 0 &&
	    *sess->opts->backup_suffix != '\0') {
		addargs(&args, "--suffix");
		addargs(&args, "%s", sess->opts->backup_suffix);
	}
	if (sess->opts->ign_exist > 0)
		addargs(&args, "--ignore-existing");
	if (sess->opts->ign_non_exist > 0)
		addargs(&args, "--ignore-non-existing");
	if (sess->opts->one_file_system > 1)
		addargs(&args, "-x");
	if (sess->opts->one_file_system > 0)
		addargs(&args, "-x");
	if (sess->opts->compress)
		addargs(&args, "-z");
	if (sess->opts->compress &&
	    sess->opts->compression_level != -1)
		addargs(&args, "--compress-level=%d", sess->opts->compression_level);
	if (sess->opts->specials && !sess->opts->devices)
		addargs(&args, "--specials");
	if (!sess->opts->specials && sess->opts->devices)
		/* --devices is sent as -D --no-specials */
		addargs(&args, "--no-specials");
	if (sess->opts->max_size >= 0)
		addargs(&args, "--max-size=%lld", (long long)sess->opts->max_size);
	if (sess->opts->min_size >= 0)
		addargs(&args, "--min-size=%lld", (long long)sess->opts->min_size);
	if (sess->opts->relative > 0)
		addargs(&args, "--relative");
	if (sess->opts->dirs > 0)
		addargs(&args, "--dirs");
	if (sess->opts->dlupdates > 0)
		addargs(&args, "--delay-updates");
	if (sess->opts->copy_links)
		addargs(&args, "-L");
	if (sess->opts->copy_unsafe_links)
		addargs(&args, "--copy-unsafe-links");
	if (sess->opts->safe_links)
		addargs(&args, "--safe-links");
	if (sess->opts->copy_dirlinks)
		addargs(&args, "-k");
	if (sess->opts->keep_dirlinks)
		addargs(&args, "-K");
	if (sess->opts->remove_source)
		addargs(&args, "--remove-source-files");
#ifdef __APPLE__
	if (sess->opts->extended_attributes)
		addargs(&args, "--extended-attributes");
#endif
	if (f->mode == FARGS_SENDER && sess->opts->ignore_times > 0)
		addargs(&args, "--ignore-times");
	if (f->mode == FARGS_SENDER && sess->opts->fuzzy_basis)
		addargs(&args, "--fuzzy");
	if (sess->opts->outformat != NULL)
		addargs(&args, "--out-format=%s", sess->opts->outformat);
	if (sess->opts->bit8 > 0)
		addargs(&args, "-8");
	if (sess->opts->bwlimit >= 1024) {
		addargs(&args, "--bwlimit=%lld",
		    (long long)(sess->opts->bwlimit / 1024));
	}
	if (sess->opts->modwin > 0)
		addargs(&args, "--modify-window=%d", sess->opts->modwin);
	if (f->mode == FARGS_SENDER && sess->opts->temp_dir) {
		addargs(&args, "--temp-dir");
		addargs(&args, "%s", sess->opts->temp_dir);
	}
	if (f->mode == FARGS_SENDER && sess->opts->block_size > 0)
		addargs(&args, "-B%ld", sess->opts->block_size);
	if (sess->opts->force_delete)
		addargs(&args, "--force");
	if (sess->opts->ignore_errors)
		addargs(&args, "--ignore-errors");
	if (sess->opts->preserve_executability)
		addargs(&args, "--executability");
	if (sess->opts->quiet)
		addargs(&args, "-q");
	if (sess->opts->max_delete)
		addargs(&args, "--max-delete=%ld", sess->opts->max_delete);

#ifdef __APPLE__
	if (sess->opts->no_cache == 0) {
		addargs(&args, "--cache");
	}
#else
	if (sess->opts->no_cache == 1) {
		addargs(&args, "--no-cache");
	}
#endif

	if (sess->opts->supermode != SMODE_UNSET) {
		switch (sess->opts->supermode) {
		case SMODE_ON:
			addargs(&args, "--super");
			break;
		case SMODE_OFF:
			addargs(&args, "--no-super");
			break;
		default:
			/* UNREACHABLE */
			assert(0 && "Invalid value for supermode");
			break;
		}
	}

	/* Send this only if we are actually using a remote file for filesfrom */
	if (sess->opts->filesfrom != NULL && sess->opts->filesfrom_host != NULL) {
		/* Must not transmit hostname as part of this */
		addargs(&args, "--files-from");
		addargs(&args, "%s", sess->opts->filesfrom_path);
		if (sess->opts->relative == 0)
			addargs(&args, "--no-relative");
		if (sess->opts->dirs == 0)
			addargs(&args, "--no-dirs");
		if (sess->opts->recursive > 0)
			addargs(&args, "--recursive");
		if (sess->opts->from0) {
			addargs(&args, "--from0");
		}
	}

	/* extra options for the receiver (local is sender) */
	if (f->mode == FARGS_SENDER) {
		if (sess->opts->write_batch != NULL &&
		    sess->opts->dry_run == DRY_XFER) {
			addargs(&args, "--only-write-batch=%s",
			    sess->opts->write_batch);
		}
		if (sess->opts->size_only)
			addargs(&args, "--size-only");

		/* only add --compare-dest, etc if this is the sender */
		if (sess->opts->alt_base_mode != 0) {
			for (j = 0; j < MAX_BASEDIR; j++) {
				if (sess->opts->basedir[j] == NULL)
					break;
				addargs(&args, "%s=%s",
				    alt_base_mode(sess->opts->alt_base_mode),
				    sess->opts->basedir[j]);
			}
		}
	}

	/* Terminate with a full-stop for reasons unknown. */

	addargs(&args, ".");

	/*
	 * In both cases, we could end up with an empty source/sink after
	 * stripping a host: specification.  We should send across an explicit
	 * '.' to use whatever directory ssh puts us in.
	 */
	if (f->mode == FARGS_RECEIVER) {
		for (j = 0; j < f->sourcesz; j++) {
			if (f->sources[j][0] == '\0')
				addargs(&args, ".");
			else
				addargs(&args, "%s", f->sources[j]);
		}
	} else {
		if (f->sink[0] == '\0')
			addargs(&args, ".");
		else
			addargs(&args, "%s", f->sink);
	}

	return args.list;
}
