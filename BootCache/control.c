/*
 * Block device "smart readahead" cache control utility.
 *
 * Provides the user-space interface to the BootCache cache;
 * loading the playlist, reading and presorting the history list
 * into a new playlist, gathering performance statistics.
 *
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "BootCache.h"

static int	usage(const char *reason);
static int	start_cache(const char *pfname);
static int	stop_cache(const char *pfname, int debugging);
static int	autostop_cache(char *delay);
static int	merge_playlists(const char *pfname, int argc, char *argv[]);
static int	print_statistics(struct BC_statistics *ss);
static void	print_history(struct BC_history_entry *he, int nentries);
static int	print_playlist(const char *pfname, int source);
static int	unprint_playlist(const char *pfname);
static int	truncate_playlist(const char *pfname, char *larg);

static int verbose;
static char *myname;

int
main(int argc, char *argv[])
{
	int ch, cflag;
	char *pfname;

	myname = argv[0];
	pfname = NULL;
	cflag = 0;
	while ((ch = getopt(argc, argv, "b:cvf:")) != -1) {
		switch(ch) {
		case 'b':
			if ((sscanf(optarg, "%d", &BC_blocksize)) != 1)
				usage("bad blocksize specification");
		case 'c':
			cflag++;
			break;
		case 'v':
			verbose++;
			break;
		case 'f':
			pfname = optarg;
			break;
		case '?':
		default:
			return(usage(NULL));
			break;
		}
	}
	argc -= optind;
	argv += optind;
	
	if (argc < 1)
		return(usage("missing command"));
	/* documented interface */
	if (!strcmp(argv[0], "start"))
		return(start_cache(pfname));
	if (!strcmp(argv[0], "stop"))
		return(stop_cache(pfname, 0));
	if (!strcmp(argv[0], "tag"))
		return(BC_tag_history());
	if (!strcmp(argv[0], "statistics"))
		return(print_statistics(NULL));
	if (!strcmp(argv[0], "merge"))
		return(merge_playlists(pfname, argc - 1, argv + 1));
	if (!strcmp(argv[0], "print"))
		return(print_playlist(pfname, cflag));
	if (!strcmp(argv[0], "unprint"))
		return(unprint_playlist(pfname));
	if (!strcmp(argv[0], "truncate")) {
		if (argc < 2) 
			return(usage("missing truncate length"));
		return(truncate_playlist(pfname, argv[1]));
	}

	/* internal interface */
	if (!strcmp(argv[0], "autostop")) {
		if (argc < 2) 
			return(usage("missing autostop delay"));
		return(autostop_cache(argv[1]));
	}
	if (!strcmp(argv[0], "unload")) {
		if (BC_unload()) {
			warnx("could not unload cache");
			return(1);
		} else {
			return(0);
		}
	}
	return(usage("invalid command"));
}

/*
 * Suggest better usage habits.
 */
static int
usage(const char *reason)
{
	if (reason != NULL)
		warnx("%s\n", reason);
	fprintf(stderr, "Usage: %s [-vvv] [-b blocksize] [-f <playlistfile>] start|stop\n", myname);
	fprintf(stderr, "           Start/stop the cache using <playlistfile>.\n");
	fprintf(stderr, "       %s statistics\n", myname);
	fprintf(stderr, "           Print statistics for the currently-active cache.\n");
	fprintf(stderr, "       %s tag\n", myname);
	fprintf(stderr, "           Insert the end-prefetch tag.\n");
	fprintf(stderr, "       %s [-vvv] -f <playlistfile> merge <playlistfile1> [<playlistfile2>...]\n",
	    myname);
	fprintf(stderr, "           Merge <playlistfile1>... into <playlistfile>.\n");
	fprintf(stderr, "       %s [-c] -f <playlistfile> print\n", myname);
	fprintf(stderr, "           Print the contents of <playlistfile>.\n");
	fprintf(stderr, "       %s -f <playlistfile> unprint\n", myname);
	fprintf(stderr, "           Read a playlist from standard input and write to <playlistfile>.\n");
	fprintf(stderr, "       %s -f <playlistfile> truncate <count>\n", myname);
	fprintf(stderr, "           Truncate <playlistfile> to <count> entries.\n");
	fflush(stderr);
	return(1);
}

/*
 * Start the cache, optionally passing in the playlist file.
 */
static int
start_cache(const char *pfname)
{
	struct BC_playlist_entry *pc;
	int nentries;
	int error;

	/* set up to start recording with no playback */
	nentries = 0;
	pc = NULL;

	/*
	 * If we have a playlist, open it and prepare to load it.
	 */
	error = 0;
	if (pfname != NULL) {
		error = BC_read_playlist(pfname, &pc, &nentries);
	} else if (!access(BC_BOOT_PLAYLIST, R_OK)) {
		/*
		 * This is the "smart" startup mode, without a playlist
		 * specified.
		 */
		error = BC_read_playlist(BC_BOOT_PLAYLIST, &pc, &nentries);
	}

	/*
	 * If the playlist is missing or invalid, ignore it (we'll
	 * overwrite it later.
	 */
	if ((error != 0) && (error != EINVAL) && (error != ENOENT)) {
		warnx("could not read playlist %s : %s", pfname, strerror(error));
		return(error);
	}

	error = BC_start(pc, nentries);
	if (error != 0)
		warnx("could not start cache: %s", strerror(error));
	return(error);
}

/*
 * Stop the cache and fetch the history list.  Post-process the history list
 * and save to the named playlist file.
 */
static int
stop_cache(const char *pfname, int debugging)
{
	struct BC_playlist_entry *pc, *opc;
	struct BC_history_entry *he;
	struct BC_statistics *ss;
	int nentries, onentries, error;

	/*
	 * Stop the cache and fetch the history list.
	 */
	if ((error = BC_stop(&he, &nentries)) != 0)
		err(1, "could not stop cache/fetch history");

	/*
	 * Fetch cache statistics and print if requested.
	 */
	if ((error = BC_fetch_statistics(&ss)) != 0)
		errx(1, "could not fetch cache statistics: %s", strerror(error));

	if (verbose > 0)
		print_statistics(ss);
	if (verbose > 1)
		print_history(he, nentries);

	/* write history and stats to debug logs if debugging */
	if (debugging) {
		BC_print_history(BC_BOOT_HISTFILE, he, nentries);
		if (ss != NULL) {
			BC_print_statistics(BC_BOOT_STATFILE, ss);
		}
	}

	/*
	 * If we have not been asked to update the history list, we are done
	 * here.
	 */
	if (pfname == NULL) {
		free(he);
		return(0);
	}

	/*
	 * Convert the history list to playlist format.
	 */
	if ((error = BC_convert_history(he, &pc, &nentries)) != 0) {
		free(he);
		errx(1, "could not allocate memory to convert in");
	}

	/*
	 * In order to ensure best possible coverage, we try to merge the
	 * existing playlist with our new history (provided that the hit
	 * rate has been good).
	 *
	 * We check the number of initiated reads from our statistics against
	 * the number of entries in the "old" playlist to ensure that the
	 * filename we've been given matches the playlist that was just run.
	 *
	 * XXX we hardcode the "good" threshold here to 85%, should be tunable
	 */
	opc = NULL;
	if (ss == NULL) {
		warnx("no statistics, not merging playlists");
		goto nomerge;	/* no stats, can't make sane decision */
	}
	if (ss->ss_total_extents < 1) {
		warnx("no playlist in kernel, not merging");
		goto nomerge;
	}
	if (BC_read_playlist(pfname, &opc, &onentries) != 0) {
		warnx("no old playlist '%s', not merging", pfname);
		goto nomerge;	/* can't read old playlist, nothing to merge */
	}
	if (onentries != ss->ss_total_extents) {
		warnx("old playlist does not match in-kernel playlist, not merging");
		goto nomerge;	/* old playlist doesn't match in-kernel playlist */
	}
	if (((nentries * 100) / onentries) < 95) {
		warnx("new playlist smaller than old playlist, not merging");
		goto nomerge;	/* new playlist has > 5% fewer extents */
	}
	if (((ss->ss_spurious_blocks * 100) / (ss->ss_read_blocks + 1)) > 10) {
		warnx("old playlist has excess block wastage, not merging");
		goto nomerge;	/* old playlist has > 10% block wastage */
	}
	if (((ss->ss_hit_blocks * 100) / (ss->ss_requested_blocks + 1)) < 85) {
		warnx("old playlist has poor hitrate, not merging");
		goto nomerge;	/* old playlist had < 85% hitrate */
	}
	BC_merge_playlists(&pc, &nentries, opc, onentries);

nomerge:
	if (opc != NULL)
		free(opc);

	/*
	 * Sort the playlist into block order and coalesce into the smallest set
	 * of read operations.
	 */
	BC_sort_playlist(pc, nentries);
	BC_coalesce_playlist(&pc, &nentries);

	/*
	 * Securely overwrite the previous playlist.
	 */
	if ((error = BC_write_playlist(pfname, pc, nentries)) != 0) {
		free(pc);
		errx(1, "could not write playlist");
	}

	return(0);
}

/*
 * Background ourselves and wait for (delay) seconds before calling stop_cache.
 */
static int
autostop_cache(char *delay)
{
	int	delaysec, debugging, result;
	char	*cp;
	time_t	t;
	
	/* determine whether we are going to produce debug output */
	debugging = !access(BC_BOOT_FLAGFILE, F_OK);

	/* open the logfile and attach to standard output/error */
	if (debugging) {
		freopen(BC_BOOT_LOGFILE, "w", stdout);
		freopen(BC_BOOT_LOGFILE, "w", stderr);
	}

	/* we must run as root */
	if (geteuid() != 0) {
		usage("autostop must run as root");
		return(EPERM);
	}
	
	/* parse the delay */
	delaysec = (int)strtol(delay, &cp, 0);
	if ((cp == delay) || (*cp != 0) || (delaysec < 1) || (delaysec > 3600)) {
		warnx("delay value '%s' can't be parsed", delay);
		usage("bad delay value");
		return(EINVAL);
	}

	/* detach and background ourselves */
	if (daemon(1/* nochdir */, 0 /* closeall */)) {
		warn("daemon failed");
		return(errno);
	}

	/* reopen the logfile and attach to standard out/error if requested */
	if (debugging) {
		freopen(BC_BOOT_LOGFILE, "a", stdout);
		setlinebuf(stdout);
		freopen(BC_BOOT_LOGFILE, "a", stderr);
		setlinebuf(stderr);
	}
	
	/* now sleep for the specified delay */
	time(&t);
	warnx("sleeping for %d seconds at %s", delaysec, ctime(&t));
	while (delaysec > 0) {
		delaysec = sleep(delaysec);
	}

	/* and save */
	result = stop_cache(BC_BOOT_PLAYLIST, debugging);

	/* unload kext */
	if ((result == 0) && BC_unload())
		warnx("could not unload kext");
	
	time(&t);
	warnx("autostop finished at %s", ctime(&t));
	return(result);
}

/*
 * Merge multiple playlist files.
 */
static int
merge_playlists(const char *pfname, int argc, char *argv[])
{
	struct BC_playlist_entry *pc, *npc;
	int i, nentries, nnentries;

	pc = NULL;
	nentries = 0;

	/*
	 * Read playlists into memory.
	 */
	for (i = 0; i < argc; i++) {

		/*
		 * Read the next playlist and merge with the current set.
		 */
		if (BC_read_playlist(argv[i], &npc, &nnentries) != 0)
			return(1);
		if (BC_merge_playlists(&pc, &nentries, npc, nnentries))
			return(1);
		free(npc);
	}

	/*
	 * Sort, coalesce and emit the consolidated playlist.
	 */
	BC_sort_playlist(pc, nentries);
	BC_coalesce_playlist(&pc, &nentries);
	return(BC_write_playlist(pfname, pc, nentries));
}

/*
 * Print statistics.
 */
static int
print_statistics(struct BC_statistics *ss)
{
	int error;
	
	if (ss == NULL) {
		if ((error = BC_fetch_statistics(&ss)) != 0) {
			errx(1, "could not fetch statistics: %s", strerror(errno));
		}
	}
	
	return(BC_print_statistics(NULL, ss));
}

/*
 * Print history entries.
 */
static void
print_history(struct BC_history_entry *he, int nentries)
{
	while (nentries--) {
		printf("%-10llu %-5llu %s%s%s\n",
		    he->he_offset, he->he_length,
		    he->he_flags == BC_HE_HIT  ? "hit"  : "",
		    he->he_flags == BC_HE_MISS ? "miss" : "",
		    he->he_flags == BC_HE_TAG  ? "tag"  : "");
		he++;
	}
}

/*
 * Print a playlist from a file.
 */
static int
print_playlist(const char *pfname, int source)
{
	struct BC_playlist_entry *pc, *opc;
	int nentries, i;
	long size;

	if (pfname == NULL)
		errx(1, "must specify a playlist file to print");

	/*
	 * Suck in the playlist.
	 */
	if (BC_read_playlist(pfname, &pc, &nentries))
		errx(1, "could not read playlist");
	opc = pc;

	if (source) {
		printf("static struct BC_playlist_entry BC_data[] = {\n");
	} else {
		printf("%d-byte blocks\n", BC_blocksize);
	}

	/*
	 * Print entries in source or "human-readable" format.
	 */
	size = 0;
	for (i = 0; nentries-- > 0; i++, pc++) {
		if (source) {
			printf("    {0x%llx, 0x%llx, 0x%x}%s\n",
			    pc->pce_offset, pc->pce_length, pc->pce_flags,
			    (nentries > 0) ? "," : "");
		} else {
			printf("%-10llu %-5llu %s\n",
			    pc->pce_offset, pc->pce_length,
			    pc->pce_flags & PCE_PREFETCH ? "prefetch" : "");
		}
		size += pc->pce_length;
	}
	if (source) {
		printf("};\n");
		printf("static int BC_playlist_blocksize = %d;\n", BC_blocksize);
	} else {
		printf("%ld blocks\n", size);
	}
	free(opc);
	
	return(0);
}

/*
 * Read a playlist and write to a file.
 */
static int
unprint_playlist(const char *pfname)
{
	struct BC_playlist_entry *pc;
	char flags[256];
	int nentries, alloced, error, got;

	pc = NULL;
	alloced = 0;
	nentries = 0;

	if (pfname == NULL)
		errx(1, "must specify a playlist file to create");

	if (BC_blocksize == 0) {
		warnx("assuming 512-byte blocks");
		BC_blocksize = 512;
	}

	for (;;) {
		/* make sure we have space for the next entry */
		if (nentries >= alloced) {
			if (alloced == 0) {
				alloced = 100;
			} else {
				alloced = alloced * 2;
			}
			if ((pc = realloc(pc, alloced * sizeof(*pc))) == NULL)
				errx(1, "could not allocate memory for %d entries", alloced);
		}

		/* read input */
		got = fscanf(stdin, "%llu %llu %s",
		    &(pc + nentries)->pce_offset,
		    &(pc + nentries)->pce_length,
		    flags);
		(pc + nentries)->pce_flags = 0;
		if (got < 2)
			break;
		if (got == 3) {
			if (!strcmp(flags, "prefetch"))
				(pc + nentries)->pce_flags |= PCE_PREFETCH;
		}

		/* grow */
		nentries++;
	}

	if (nentries < 1)
		errx(1, "no input");

	if ((error = BC_write_playlist(pfname, pc, nentries)) != 0)
		errx(1, "could not create playlist");
	
	return(0);
}


/*
 * Truncate a playlist to a given number of entries.
 */
static int
truncate_playlist(const char *pfname, char *larg)
{
	struct BC_playlist_entry *pc;
	int nentries;
	char *cp;
	int length, error;
	
	if (pfname == NULL)
		errx(1, "must specify a playlist file to truncate");

	length = strtol(larg, &cp, 0);
	if ((*cp != 0) || (length < 1))
		err(1, "bad truncate length '%s'", larg);
	
	/*
	 * Suck in the playlist.
	 */
	if (BC_read_playlist(pfname, &pc, &nentries))
		errx(1, "could not read playlist to truncate");

	/*
	 * Make sure the new length is shorter.
	 */
	if (length > nentries) {
		warnx("playlist is shorter than specified truncate length");
	} else {
	
		/*
		 * Write a shortened version of the same playlist.
		 */
		if ((error = BC_write_playlist(pfname, pc, length)) != 0)
			errx(1, "could not truncate playlist");
	}
	return(0);
}

