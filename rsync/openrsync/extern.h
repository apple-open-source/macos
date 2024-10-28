/*	$OpenBSD: extern.h,v 1.45 2023/04/28 10:24:38 claudio Exp $ */
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
#ifndef EXTERN_H
#define EXTERN_H

#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fts.h>
#include <stdio.h>
#include <stdbool.h>

#include "md4.h"

#ifdef __APPLE__
#include <os/log.h>
#include <errno.h>
#endif

#ifndef __printflike
#define __printflike(fmtarg, firstvararg) \
	__attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#endif

#ifndef nitems
#define nitems(_a)	(sizeof((_a)) / sizeof((_a)[0]))
#endif

#if !HAVE_PLEDGE
# define pledge(x, y) (1)
#endif
#if !HAVE_UNVEIL
# define unveil(x, y) (1)
#endif

/*
 * Mirror the reference rsync here; they don't really entertain path limitations
 * lower than 4096.
 */
#if PATH_MAX <= 4096
#define	BIGPATH_MAX	(4096 + 1024)
#else
#define	BIGPATH_MAX	(PATH_MAX + 1024)
#endif

/*
 * This is the rsync protocol version that we support.
 */
#define	RSYNC_PROTOCOL		(29)
#define	RSYNC_PROTOCOL_MIN	(27)
#define	RSYNC_PROTOCOL_MAX	(40)

#define	protocol_newflist	(sess->protocol >= 28)
#define	protocol_itemize	(sess->protocol >= 29)
#define	protocol_newsort	(sess->protocol >= 29)
#define	protocol_dlrename	(sess->protocol >= 29)
#define	protocol_keepalive	(sess->protocol >= 29)
#define	protocol_fliststats	(sess->protocol >= 29)
#define	protocol_newbatch	(sess->protocol >= 29)
#define	protocol_delrules	(sess->protocol >= 29)

/*
 * itemize-changes flags.
 */
#define	IFLAG_ATIME		(1<<0)
#define	IFLAG_CHECKSUM		(1<<1)
#define	IFLAG_SIZE		(1<<2)
#define	IFLAG_TIME		(1<<3)
#define	IFLAG_PERMS		(1<<4)
#define	IFLAG_OWNER		(1<<5)
#define	IFLAG_GROUP		(1<<6)
#define	IFLAG_ACL		(1<<7)
#define	IFLAG_XATTR		(1<<8)
#define	IFLAG_BASIS_FOLLOWS	(1<<11)
#define	IFLAG_HLINK_FOLLOWS	(1<<12)
#define	IFLAG_NEW		(1<<13)
#define	IFLAG_LOCAL_CHANGE	(1<<14)
#define	IFLAG_TRANSFER		(1<<15)
/*
 * Not for transmission.
 */
#define	IFLAG_MISSING_DATA	(1<<16)	/* used by log_formatted() */
#define	IFLAG_DELETED		(1<<17)	/* used by log_formatted() */
#define	IFLAG_HAD_BASIS		(1<<18) /* had basis, used by sender_get_iflags() */

#define	SIGNIFICANT_IFLAGS	\
	(~(IFLAG_BASIS_FOLLOWS | ITEM_HLINK_FOLLOWS | IFLAG_LOCAL_CHANGE))

/*
 * Defaults from the reference rsync; the max password size is specifically for
 * password files, and not otherwise strictly enforced.
 */
#define	RSYNCD_DEFAULT_USER	"nobody"
#define	RSYNCD_MAX_PASSWORDSZ	511
/* In future versions, this may be higher to support flexible digest choices. */
#define	RSYNCD_CHALLENGE_RESPONSESZ	(MD4_DIGEST_LENGTH * 16)
/* Maximum auth response size. */
#define	RSYNCD_MAXAUTHSZ	2048

/* Some daemon values */
/* In future versions, this may be higher to support flexible digest choices. */
#define	RSYNCD_CHALLENGE_RESPONSESZ	(MD4_DIGEST_LENGTH * 16)

/*
 * Maximum amount of file data sent over the wire at once.
 */
#define MAX_CHUNK	(32 * 1024)

/* Save 2 bytes to fit a 14 bit count and compression flags */
#define	MAX_COMP_CHUNK	16383

/*
 * zlib needs a bit of extra space in the buffer.
 */
#define MAX_CHUNK_BUF	((MAX_CHUNK)*1001/1000+16)

/*
 * This is the minimum size for a block of data not including those in
 * the remainder block.
 */
#define	BLOCK_SIZE_MIN  (700)

/*
 * Maximum number of base directories that can be used.
 */
#define MAX_BASEDIR	20

#define	PHASE_XFER	0
#define	PHASE_REDO	1
#define	PHASE_DLUPDATES 2	/* Protocol 29 */

enum basemode {
	BASE_MODE_COMPARE = 1,	/* Just compare */
	BASE_MODE_COPY,		/* Copy into rootfd */
	BASE_MODE_LINK,		/* Hardlink into rootfd */
};

enum dirmode {
	DIRMODE_OFF = 0,	/* No --dirs */
	DIRMODE_IMPLIED,	/* Implied --dirs */
	DIRMODE_REQUESTED,	/* --dirs */
};

enum dryrun {
	DRY_DISABLED = 0,	/* Full run */
	DRY_XFER,		/* Xfer only */
	DRY_FULL,		/* Full dry-run*/
};

/*
 * The sender and receiver use a two-phase synchronisation process.
 * The first uses two-byte hashes; the second, 16-byte.
 * (The second must hold a full MD4 digest.)
 */
#define	CSUM_LENGTH_PHASE1 (2)
#define	CSUM_LENGTH_PHASE2 (16)

/*
 * Rsync error codes.
 */
#define ERR_SYNTAX	1
#define ERR_PROTOCOL	2
#define ERR_FILEGEN	3
#define ERR_SOCK_IO	10
#define ERR_FILE_IO	11
#define ERR_WIREPROTO	12
#define ERR_IPC		14	/* catchall for any kind of syscall error */
#define ERR_TERMIMATED	16
#define ERR_SIGUSR1	19
#define ERR_SIGNAL	20
#define ERR_WAITPID	21
#define ERR_NOMEM	22
#define ERR_PARTIAL	23
#define ERR_DEL_LIMIT	25

#define TOKEN_END		0x00	/* end of sequence */
#define TOKEN_LONG		0x20	/* Token is 32-bits */
#define TOKEN_LONG_RUN		0x21	/* Token is 32-bits and has 16 bit run count */
#define TOKEN_DEFLATED		0x40	/* Data is deflated */
#define TOKEN_RELATIVE		0x80	/* Token number is relative */
#define TOKEN_RUN_RELATIVE	0xc0	/* Run count is 16-bit */

#define TOKEN_MAX_DATA		MAX_COMP_CHUNK	/* reserve 2 bytes for flags */
#define TOKEN_MAX_BUF		(TOKEN_MAX_DATA + 2)

/* Forward declarations */
struct flist;
struct sess;

#ifdef __APPLE__
extern int syslog_trace;
extern os_log_t syslog_trace_obj;
#endif

/*
 * Use this for --timeout.
 * All poll events will use it and catch time-outs.
 */
extern int poll_timeout;

/*
 * Use this for --contimeout.
 */
extern int poll_contimeout;

/*
 * Operating mode for a client or a server.
 * Sender means we synchronise local files with those from remote.
 * Receiver is the opposite.
 * This is relative to which host we're running on.
 */
enum	fmode {
	FARGS_SENDER,
	FARGS_RECEIVER
};

#define	IOTAG_OFFSET	7

enum	compat_loglvl {
	LOGNONE = 0,
	/* Protocol == * */
	LOGERROR_XFER,
	LOGINFO,
	/* Protocols >= 30 */
	LOGERROR,
	LOGWARNING,
};

struct	iobuf {
	uint8_t		*buffer;
	size_t		 offset;
	size_t		 resid;
	size_t		 size;
};

enum	iotag {
	IT_DATA = 0,

	IT_ERROR_XFER = LOGERROR_XFER,
	IT_INFO = LOGINFO,
	IT_ERROR = LOGERROR,
	IT_WARNING = LOGWARNING,

	IT_SUCCESS = 100,
	IT_DELETED,
	IT_NO_SEND,
};

struct	vstring {
	char		*vstring_buffer;
	size_t		 vstring_offset;
	size_t		 vstring_size;
};

typedef int (io_tag_handler_fn)(void *, const void *, size_t sz);

enum	zlib_state {
	COMPRESS_INIT = 0,
	COMPRESS_READY,
	COMPRESS_SEQUENCE,
	COMPRESS_RUN,
	COMPRESS_DONE,
};

/*
 * Delete modes:
 * - unspecified: transforms into one of the below after option processing,
 *    i.e., --del / --delete.
 * - before: delete files up-front
 * - during: delete files during transfer
 * - delay:  gather deletions and delete after after
 * - after: delete after transfer
 * - excluded: delete excluded files, too
 */
enum	delmode {
	DMODE_NONE,
	DMODE_UNSPECIFIED,
	DMODE_BEFORE,
	DMODE_DURING,
	DMODE_DELAY,
	DMODE_AFTER,
};

/* --numeric-ids mode */
enum	nidsmode {
	NIDS_OFF,	/* No numeric IDs */
	NIDS_STEALTH,	/* Numeric IDs, client side is unaware */
	NIDS_FULL,	/* Numeric IDs, both sides know */
};

/*
 * Super modes; we can either force an attempt of super-user activities, write
 * them into xattrs (fake), or we can disable super-user activities altogether.
 */
enum	smode {
	SMODE_UNSET,
	SMODE_ON,	/* --super */
	SMODE_OFF,	/* --no-super */
};

/*
 * File arguments given on the command line.
 * See struct opts.
 */
struct	fargs {
	char	  *user; /* username or NULL if unspecified or local */
	char	  *host; /* hostname or NULL if local */
	char	 **sources; /* transfer source */
	size_t	   sourcesz; /* number of sources */
	char	  *sink; /* transfer endpoint */
	enum fmode mode; /* mode of operation */
	int	   remote; /* uses rsync:// or :: for remote */
	char	  *module; /* if rsync://, the module */
};

/*
 * The subset of stat(2) information that we need.
 * (There are some parts we don't use yet.)
 */
struct	flstat {
	mode_t		 mode;	 /* mode */
	uid_t		 uid;	 /* user */
	gid_t		 gid;	 /* group */
	dev_t		 rdev;	 /* device type */
	off_t		 size;	 /* size */
	time_t		 mtime;	 /* modification */
	unsigned int	 flags;
	int64_t		 device; /* device number, for hardlink detection */
	int64_t		 inode;  /* inode number, for hardlink detection */
	uint64_t	 nlink;  /* number of links, for hardlink detection */
#define	FLSTAT_TOP_DIR	 0x01	 /* a top-level directory */

/* Platform use */
#define	FLSTAT_PLATFORM_BIT1	0x10000000
#define	FLSTAT_PLATFORM_BIT2	0x20000000
#define	FLSTAT_PLATFORM_BIT3	0x40000000
#define	FLSTAT_PLATFORM_BIT4	0x80000000
#define	FLSTAT_PLATFORM_MASK	0xf0000000
};

enum name_basis {
	BASIS_DIR_LOW = 0,
	BASIS_DIR_HIGH = 0x7F,
	BASIS_NORMAL,
	BASIS_PARTIAL_DIR,
	BASIS_BACKUP,
	BASIS_FUZZY,
};

/*
 * A list of files with their statistics.
 * Note that the md[] field is only used by the --checksum option, and
 * hence could be eliminated (when not needed) by making struct flist
 * a variably-sized structure and handling its variability in both the
 * sender and receiver.
 */
typedef int platform_open(const struct sess *, const struct flist *, int);
typedef int platform_flist_sent(struct sess *, int, const struct flist *);

struct	flist {
	char		*path; /* path relative to root */
	int		 pdfd; /* dirfd for partial */
	const char	*wpath; /* "working" path for receiver */
	struct flstat	 st; /* file information */
	char		*link; /* symlink target, hlink name, or NULL */
	unsigned char    md[MD4_DIGEST_LENGTH]; /* MD4 hash for --checksum */
	int		 flstate; /* flagged for redo, or complete? */
	int32_t		 iflags; /* Itemize flags */
	enum name_basis	 basis; /* name basis */
	platform_open	*open; /* special open() for this entry */
	platform_flist_sent	*sent; /* notify the platform an entry was sent */
	int		 sendidx; /* Sender index */
};

#define	FLIST_COMPLETE		0x01	/* Finished */
#define	FLIST_REDO		0x02	/* Finished, but go again */
#define	FLIST_SUCCESS		0x04	/* Finished and in place */
#define	FLIST_FAILED		0x08	/* Failed */
#define	FLIST_SUCCESS_ACKED	0x10	/* Sent success message */

#define	FLIST_DONE_MASK		(FLIST_SUCCESS | FLIST_REDO | FLIST_FAILED)

/*
 * Holds many struct flist and takes care of memory management.
 */
struct fl {
	struct flist *flp;
	size_t sz;   /* Actual entries */
	size_t max;  /* Allocated size */
};
void fl_init(struct fl *);
long fl_new_index(struct fl *); /* Returns index of new element */
struct flist *fl_new(struct fl *); /* Returns pointer to new element */
struct flist *fl_atindex(struct fl *, size_t idx);
long fl_curridx(struct fl *); /* Current size */
void fl_print(const char *id, struct fl *fl);
void fl_pop(struct fl *);

/*
 * Options passed into the command line.
 * See struct fargs.
 */
struct	opts {
	int		 sender;		/* --sender */
	int		 server;		/* --server */
	int		 protocol;		/* --protocol */
	int		 append;		/* --append */
	int		 checksum;		/* -c --checksum */
	int		 checksum_seed;		/* --checksum-seed */
	const char	*chmod;			/* --chmod */
	int		 recursive;		/* -r */
	enum dryrun	 dry_run;		/* -n */
	int		 inplace;		/* --inplace */
	int		 partial;		/* --partial */
	char		*partial_dir;		/* --partial-dir */
	int		 preserve_times;	/* -t */
	int		 preserve_perms;	/* -p */
	int		 copy_links;		/* -L */
	int		 copy_unsafe_links;	/* --copy-unsafe-links */
	int		 safe_links;		/* --safe-links */
	int		 copy_dirlinks;		/* -k */
	int		 keep_dirlinks;		/* -K */
	int		 preserve_links;	/* -l */
	int		 preserve_gids;		/* -g */
	int		 preserve_uids;		/* -u */
	enum delmode	 del;			/* --delete */
	int		 del_excl;		/* --delete-excluded */
	int		 devices;		/* --devices */
	int		 specials;		/* --specials */
	int		 no_cache;		/* --no-cache */
	int		 no_motd;		/* --no-motd */
	enum nidsmode	 numeric_ids;		/* --numeric-ids */
	int		 one_file_system;	/* -x */
	int		 omit_dir_times;	/* -O */
	int		 ignore_times;		/* -I --ignore-times */
	int              progress;              /* --progress */
	int		 alt_base_mode;
	int		 sparse;		/* -S --sparse */
	int		 update;		/* -u --update */
	int		 backup;		/* --backup */
	char		*backup_dir;		/* --backup-dir */
	char		*backup_suffix;		/* --suffix */
	int		 human_readable;	/* --human-readable */
	int		 ign_exist;		/* --ignore-existing */
	int		 ign_non_exist;		/* --ignore-nonexisting */
	int		 relative;		/* --relative */
	enum dirmode	 dirs;			/* -d --dirs */
	int		 dlupdates;             /* --delay-updates */
	int		 hard_links;		/* -H --hard-links */
	int		 remove_source;		/* --remove-source-files */
	int		 supermode;		/* --{no-,}super */
	int		 bit8;		        /* -8 --8-bit-output */
	int		 stats;			/* --stats */
	off_t		 max_size;		/* --max-size */
	off_t		 min_size;		/* --min-size */
	char		*rsync_path;		/* --rsync-path */
	char		*ssh_prog;		/* --rsh or -e */
	const char	*port;			/* --port */
	const char	*address;		/* --address */
	char		*basedir[MAX_BASEDIR];
	char            *filesfrom;             /* --files-from */
	int		 from0;			/* -0 */
	char            *outformat;             /* --out-format */
	FILE            *outfile;               /* --out-format and -v */
	const char	*sockopts;		/* --sockopts */
	off_t		 bwlimit;		/* --bwlimit */
	int		 size_only;		/* --size-only */
	long		 block_size;		/* --block-size */
	char            *filesfrom_host;        /* --files-from */
	char            *filesfrom_path;        /* --files-from */
	int		 whole_file;		/* --whole-file */
	const char	*read_batch;		/* --read-batch */
	const char	*write_batch;		/* --write-batch */
	const char	*password_file;		/* --password-file */
	char		*temp_dir;		/* --temp-dir */
	int		 compress;		/* -z --compress */
	int		 compression_level;	/* --compress-level */
#if 0
	char		*syncfile;		/* --sync-file */
#endif
	int		 ipf;			/* -4 / -6 */
	int		 force_delete;		/* --force */
	int		 ignore_errors;		/* --ignore-errors */
	int		 preserve_executability;	/* --executability */
	int		 modwin;		/* --modify-windows=sec */
	int		 fuzzy_basis;		/* -y */
	int		 quiet;			/* -q, --quiet */
	long		 max_delete;		/* --max-delete */
#ifdef __APPLE__
	int		 extended_attributes;	/* --extended-attributes */
#endif
	int		 ignore_nonreadable;	/* daemon: ignore nonreadable */
};

enum rule_type {
	RULE_NONE,
	RULE_EXCLUDE,
	RULE_INCLUDE,
	RULE_CLEAR,
	RULE_MERGE,
	RULE_DIR_MERGE,
	RULE_SHOW,
	RULE_HIDE,
	RULE_PROTECT,
	RULE_RISK,
};

/*
 * An individual block description for a file.
 * See struct blkset.
 */
struct	blk {
	off_t		 offs; /* offset in file */
	size_t		 idx; /* block index */
	size_t		 len; /* bytes in block */
	uint32_t	 chksum_short; /* fast checksum */
	unsigned char	 chksum_long[CSUM_LENGTH_PHASE2]; /* slow checksum */
};

enum	blkstatst {
	BLKSTAT_NONE = 0,
	BLKSTAT_NEXT,
	BLKSTAT_DATA,
	BLKSTAT_TOK,
	BLKSTAT_HASH,
	BLKSTAT_DONE,
	BLKSTAT_PHASE,
	BLKSTAT_FLUSH,
};

/*
 * Information for the sender updating receiver blocks reentrantly.
 */
struct	blkstat {
	off_t		 offs; /* position in sender file */
	off_t		 total; /* total amount processed */
	off_t		 dirty; /* total amount sent */
	size_t		 hint; /* optimisation: next probable match */
	void		*map; /* mapped file or MAP_FAILED otherwise */
	size_t		 mapsz; /* size of file or zero */
	int		 fd; /* descriptor girding the map */
	enum blkstatst	 curst; /* FSM for sending file blocks */
	off_t		 curpos; /* sending: position in file to send */
	off_t		 curlen; /* sending: length of send */
	int32_t		 curtok; /* sending: next matching token or zero */
	struct blktab	*blktab; /* hashtable of blocks */
	uint32_t	 s1; /* partial sum for computing fast hash */
	uint32_t	 s2; /* partial sum for computing fast hash */
};

/*
 * When transferring file contents, we break the file down into blocks
 * and work with those.
 */
struct	blkset {
	off_t		 size; /* file size */
	size_t		 rem; /* terminal block length if non-zero */
	size_t		 len; /* block length */
	size_t		 csum; /* checksum length */
	struct blk	*blks; /* all blocks */
	size_t		 blksz; /* number of blks */
};

enum	send_dl_state {
	SDL_META = 0,
	SDL_IFLAGS,
	SDL_BLOCKS,
	SDL_DONE,
	SDL_SKIP,
};

/*
 * Context for the role (sender/receiver).  The role may embed this into their
 * own context struct.
 */
struct	role {
	int		 append;		/* Append mode active */

	/*
	 * We basically track two different forms of phase: the metadata phase,
	 * and the transfer phase.  The metadata phase may be advanced from the
	 * transfer phase, as we'll immediately move on to checking for or
	 * processing redo entries when the first phase's file requests are
	 * done.
	 *
	 * Each role has its own way of tracking these, so we just have them
	 * drop a pointer here to avoid having to keep things in sync.  `append`
	 * above will be reset as the transfer phase progresses, so parts
	 * dealing with block metadata should check `*phase` *and* `append` to
	 * determine if they need to send/receive block checksums, and anything
	 * part of the data transfer process should just be checking `append`.
	 */
	const int	*phase;			/* Metadata phase */
};

/*
 * The filter should return 0 on success, or -1 on failure.  If *outlink is
 * populated on a successful return, then it should be used in place of the
 * symlink we were given.  If it is NULL on a successful return, then the filter
 * is declining to act and the caller should proceed with the original link.
 */
typedef int (symlink_filter)(const char *, char **, enum fmode);

/*
 * Values required during a communication session.
 */
struct	sess {
	const struct opts *opts; /* system options */
	enum fmode	   mode; /* sender or receiver */
	int32_t		   seed; /* checksum seed */
	int32_t		   lver; /* local version */
	int32_t		   rver; /* remote version */
	uint64_t	   total_read; /* non-logging wire/reads */
	uint64_t	   total_read_lf; /* reads at time of last file */
	uint64_t	   total_size; /* total file size */
	uint64_t	   total_write; /* non-logging wire/writes */
	uint64_t	   total_write_lf; /* writes at last file*/
	uint64_t	   total_files; /* file count */
	uint64_t	   total_files_xfer; /* files transferred */
	uint64_t	   total_xfer_size; /* total file size transferred */
	uint64_t	   total_unmatched; /* data we transferred */
	uint64_t	   total_matched; /* data we recreated */
	uint64_t	   flist_size; /* items on the flist */
	uint64_t	   flist_build; /* time to build flist */
	uint64_t	   flist_xfer; /* time to transfer flist */
	int		   mplex_reads; /* multiplexing reads? */
	size_t		   mplex_read_remain; /* remaining bytes */
	int		   mplex_writes; /* multiplexing writes? */
	double             last_time; /* last time printed --progress */  
	int                itemize; /* --itemize or %i in --output-format */
	int                lateprint; /* Does output format contain a flag requiring late print? */
	char             **filesfrom; /* Contents of files-from */
	size_t             filesfrom_n; /* Number of lines for filesfrom */
	int		   filesfrom_fd; /* --files-from */
	int		   wbatch_fd; /* --write-batch */
	struct dlrename    *dlrename; /* Deferred renames for --delay-update */
	struct role	  *role; /* Role context */
	mode_t		   chmod_dir_AND;
	mode_t		   chmod_dir_OR;
	mode_t		   chmod_dir_X;
	mode_t		   chmod_file_AND;
	mode_t		   chmod_file_OR;
	mode_t		   chmod_file_X;
	double             start_time; /* Time of first transfer */
	symlink_filter	  *symlink_filter;
	uint64_t           total_errors; /* Total non-fatal errors */
	long		   total_deleted; /* Total files deleted */
	bool		   err_del_limit; /* --max-delete limit exceeded */
	int		   protocol; /* negotiated protocol version */
};

/*
 * Combination of name and numeric id for groups and users.
 */
struct	ident {
	int32_t	 id; /* the gid_t or uid_t */
	int32_t	 mapped; /* if receiving, the mapped gid */
	char	*name; /* resolved name */
};

typedef struct arglist arglist;
struct arglist {
	char	**list;
	u_int	num;
	u_int	nalloc;
};
void	addargs(arglist *, const char *, ...)
	    __attribute__((format(printf, 2, 3)));
const char	*getarg(arglist *, size_t);
void	freeargs(arglist *);

struct cleanup_ctx;

struct	download;
struct	upload;

struct hardlinks;
const struct flist *find_hl(const struct flist *this,
			    const struct hardlinks *hl);

extern const char rsync_shopts[];
extern const struct option rsync_lopts[];
extern int verbose;

#define	TMPDIR_FD	(sess->opts->temp_dir ? p->tempfd : p->rootfd)
#define	IS_TMPDIR	(sess->opts->temp_dir != NULL)

#define MINIMUM(a, b) (((a) < (b)) ? (a) : (b))

#ifdef __APPLE__
#define	do_os_log(type, ...) do {				\
	int rserrno = errno;					\
	os_log_ ## type(syslog_trace_obj, __VA_ARGS__);		\
	errno = rserrno;					\
} while(0)

/*
 * ERR() gets mapped to an error, but ERRX1() gets more chatty so we map those
 * to debug messages -- we'll very roughly end up emitting one for each stack
 * frame, but not consistently.
 */
#define	syslog_trace_error(errlevel, ...) do {		\
	if ((errlevel) >= 0)				\
		do_os_log(error, __VA_ARGS__);		\
	else						\
		do_os_log(debug, __VA_ARGS__);		\
} while(0)

/*
 * Warnings we'll likely be able to observe everything we need in the console,
 * so just map them to `info` -- no need to persist these to disk.
 */
#define	syslog_trace_warn(errlevel, _fmt, ...)	\
	do_os_log(info, (_fmt), ##__VA_ARGS__)

/*
 * LOG0() are always output, so we omit those entirely (they're errlevel == -1).
 * LOG1 and up get progressively chattier but provide helpful context, so we'll
 * log those as default, info, and debug respectively, then drop
 * LOG4 entirely because it's logging incredibly detailed transfer information.
 */
#define	syslog_trace_log(errlevel, _fmt, ...) do {		\
	if ((errlevel) == 0) {					\
		int rserrno = errno;				\
		os_log(syslog_trace_obj, (_fmt), ##__VA_ARGS__);	\
		errno = rserrno;				\
	} else if ((errlevel) == 1)				\
		do_os_log(info, (_fmt), ##__VA_ARGS__);		\
	else if ((errlevel) == 2)				\
		do_os_log(debug, (_fmt), ##__VA_ARGS__);	\
	/* 4 intentionally discarded, too chatty. */		\
} while(0)
#else
/* XXX Unimplemented */
#define	syslog_trace_error(errlevel, _fmt, ...)
#define	syslog_trace_warn(errlevel, _fmt, ...)
#define	syslog_trace_log(errlevel, _fmt, ...)
#endif

#define LOG0(_fmt, ...) do {					\
	if (syslog_trace)					\
		syslog_trace_log(-1, (_fmt), ##__VA_ARGS__);	\
	rsync_log( -1, (_fmt), ##__VA_ARGS__);			\
} while(0)
#define LOG1(_fmt, ...) do {					\
	if (syslog_trace)					\
		syslog_trace_log(0, (_fmt), ##__VA_ARGS__);	\
	rsync_log( 0, (_fmt), ##__VA_ARGS__);			\
} while(0)
#define LOG2(_fmt, ...) do {					\
	if (syslog_trace)					\
		syslog_trace_log(1, (_fmt), ##__VA_ARGS__);	\
	rsync_log( 1, (_fmt), ##__VA_ARGS__);			\
} while(0)
#define LOG3(_fmt, ...) do {					\
	if (syslog_trace)					\
		syslog_trace_log(2, (_fmt), ##__VA_ARGS__);	\
	rsync_log( 2, (_fmt), ##__VA_ARGS__);			\
} while(0)
#define LOG4(_fmt, ...) do {					\
	if (syslog_trace)					\
		syslog_trace_log(3, (_fmt), ##__VA_ARGS__);	\
	rsync_log( 3, (_fmt), ##__VA_ARGS__);			\
} while(0)
#define ERRX1(_fmt, ...) do {					\
	if (syslog_trace)					\
		syslog_trace_error(1, (_fmt), ##__VA_ARGS__);	\
	rsync_errx1( (_fmt), ##__VA_ARGS__);			\
} while(0)
#define WARNX(_fmt, ...) do {					\
	if (syslog_trace)					\
		syslog_trace_warn(0, (_fmt), ##__VA_ARGS__);	\
	rsync_warnx( (_fmt), ##__VA_ARGS__);			\
} while(0)
#define WARNX1(_fmt, ...) do {					\
	if (syslog_trace)					\
		syslog_trace_warn(1, (_fmt), ##__VA_ARGS__);	\
	rsync_warnx1( (_fmt), ##__VA_ARGS__);			\
} while(0)
#define WARN(_fmt, ...) do {					\
	if (syslog_trace)					\
		syslog_trace_warn(0, (_fmt), ##__VA_ARGS__);	\
	rsync_warn(0,  (_fmt), ##__VA_ARGS__);			\
} while(0)
#define WARN1(_fmt, ...) do {					\
	if (syslog_trace)					\
		syslog_trace_warn(1, (_fmt), ##__VA_ARGS__);	\
	rsync_warn(1,  (_fmt), ##__VA_ARGS__);			\
} while(0)
#define WARN2(_fmt, ...) do {					\
	if (syslog_trace)					\
		syslog_trace_warn(2, (_fmt), ##__VA_ARGS__);	\
	rsync_warn(2,  (_fmt), ##__VA_ARGS__);			\
} while(0)

#if defined(__sun) && defined(ERR)
# undef ERR
#endif
#define ERR(_fmt, ...) do {					\
	if (syslog_trace)					\
		syslog_trace_error(0, (_fmt), ##__VA_ARGS__);	\
	rsync_err( (_fmt), ##__VA_ARGS__);			\
} while(0)
#define ERRX(_fmt, ...) do {					\
	if (syslog_trace)					\
		syslog_trace_error(0, (_fmt), ##__VA_ARGS__);	\
	rsync_errx( (_fmt), ##__VA_ARGS__);			\
} while(0)

int	rsync_set_logfacility(const char *);
void	rsync_set_logfile(FILE *);
void	rsync_log(int, const char *, ...)
			__attribute__((format(printf, 2, 3)));
void	rsync_warnx1(const char *, ...)
			__attribute__((format(printf, 1, 2)));
void	rsync_warn(int, const char *, ...)
			__attribute__((format(printf, 2, 3)));
void	rsync_warnx(const char *, ...)
			__attribute__((format(printf, 1, 2)));
void	rsync_err(const char *, ...)
			__attribute__((format(printf, 1, 2)));
void	rsync_errx(const char *, ...)
			__attribute__((format(printf, 1, 2)));
void	rsync_errx1(const char *, ...)
			__attribute__((format(printf, 1, 2)));

/* Opaque */
struct daemon_cfg	*cfg_parse(const struct sess *, const char *,
	    int module);
typedef int cfg_module_iter(struct daemon_cfg *, const char *, void *);
void	 cfg_free(struct daemon_cfg *);
int	 cfg_foreach_module(struct daemon_cfg *, cfg_module_iter *, void *);
int	 cfg_is_valid_module(struct daemon_cfg *, const char *);
int	 cfg_param_bool(struct daemon_cfg *, const char *, const char *,
	    int *);
int	 cfg_param_int(struct daemon_cfg *, const char *, const char *,
	    int *);
int	 cfg_param_long(struct daemon_cfg *, const char *, const char *,
	    long *);
int	 cfg_param_str(struct daemon_cfg *, const char *, const char *,
	    const char **);
int	 cfg_has_param(struct daemon_cfg *, const char *, const char *);

int	flist_dir_cmp(const void *, const void *);
int	flist_fts_check(struct sess *, FTSENT *, enum fmode);
int	flist_del(struct sess *, int, const struct flist *, size_t);
int	flist_gen(struct sess *, size_t, char **, struct fl *);
void	flist_free(struct flist *, size_t);
int	flist_recv(struct sess *, int, int, struct flist **, size_t *);
int	flist_send(struct sess *, int, int, const struct flist *, size_t);
int	flist_gen_dels(struct sess *, const char *, struct flist **, size_t *,
	    const struct flist *, size_t);
int	flist_add_del(struct sess *, const char *, size_t, struct flist **,
	    size_t *, size_t *, const struct stat *st);

const char	 *alt_base_mode(int);
char		**fargs_cmdline(struct sess *, const struct fargs *, size_t *);

int	batch_open(struct sess *);
void	batch_close(struct sess *, const struct fargs *, int);
int	check_file_mode(const char *, int);

void	cleanup_hold(struct cleanup_ctx *);
void	cleanup_release(struct cleanup_ctx *);
void	cleanup_init(struct cleanup_ctx *);
void	cleanup_run(int code);
void	cleanup_set_args(struct cleanup_ctx *, struct fargs *);
void	cleanup_set_child(struct cleanup_ctx *, pid_t);
void	cleanup_set_session(struct cleanup_ctx *, struct sess *);
void	cleanup_set_download(struct cleanup_ctx *, struct download *);

int	io_register_handler(enum iotag, io_tag_handler_fn *, void *);
int	io_read_line(struct sess *, int, char *, size_t *);
int	io_read_buf(struct sess *, int, void *, size_t);
int	io_read_byte(struct sess *, int, uint8_t *);
int	io_read_check(const struct sess *, int);
int	io_read_close(struct sess *, int);
int	io_read_flush(struct sess *, int);
int	io_read_ushort(struct sess *, int, uint32_t *);
int	io_read_short(struct sess *, int, int32_t *);
int	io_read_int(struct sess *, int, int32_t *);
int	io_read_uint(struct sess *, int, uint32_t *);
int	io_read_long(struct sess *, int, int64_t *);
int	io_read_size(struct sess *, int, size_t *);
int	io_read_ulong(struct sess *, int, uint64_t *);
int	io_read_vstring(struct sess *, int, char *, size_t);
int	io_write_buf_tagged(struct sess *, int, const void *, size_t,
	    enum iotag);
int	io_write_buf(struct sess *, int, const void *, size_t);
int	io_write_byte(struct sess *, int, uint8_t);
int	io_write_int_tagged(struct sess *, int, int32_t, enum iotag);
int	io_write_int(struct sess *, int, int32_t);
int	io_write_uint(struct sess *, int, uint32_t);
int	io_write_short(struct sess *, int, int32_t);
int	io_write_ushort(struct sess *, int, uint32_t);
int	io_write_line(struct sess *, int, const char *);
int	io_write_long(struct sess *, int, int64_t);
int	io_write_ulong(struct sess *, int, uint64_t);
int	io_write_vstring(struct sess *, int, char *, size_t);

int	io_data_written(struct sess *, int, const void *, size_t);

int	io_lowbuffer_alloc(struct sess *, void **, size_t *, size_t *, size_t);
void	io_lowbuffer_int(struct sess *, void *, size_t *, size_t, int32_t);
void	io_lowbuffer_short(struct sess *, void *, size_t *, size_t, int32_t);
void	io_lowbuffer_buf(struct sess *, void *, size_t *, size_t, const void *,
	    size_t);
void	io_lowbuffer_byte(struct sess *sess, void *buf, size_t *bufpos,
	    size_t buflen, int8_t val);
void	io_lowbuffer_vstring(struct sess *sess, void *buf, size_t *bufpos,
	    size_t buflen, char *str, size_t sz);

void	io_buffer_buf(void *, size_t *, size_t, const void *, size_t);
void	io_buffer_byte(void *, size_t *, size_t, int8_t);
void	io_buffer_int(void *, size_t *, size_t, int32_t);
void	io_buffer_short(void *, size_t *, size_t, int32_t);
void	io_buffer_vstring(void *, size_t *, size_t, char *, size_t);

void	io_unbuffer_int(const void *, size_t *, size_t, int32_t *);
int	io_unbuffer_size(const void *, size_t *, size_t, size_t *);
void	io_unbuffer_buf(const void *, size_t *, size_t, void *, size_t);

int	iobuf_alloc(struct sess *, struct iobuf *, size_t);
size_t	iobuf_get_readsz(const struct iobuf *);
int	iobuf_fill(struct sess *, struct iobuf *, int);
void	iobuf_read_buf(struct iobuf *, void *, size_t);
void	iobuf_read_byte(struct iobuf *, uint8_t *);
int32_t	iobuf_peek_int(struct iobuf *);
void	iobuf_read_int(struct iobuf *, int32_t *);
void	iobuf_read_ushort(struct iobuf *, uint32_t *);
void	iobuf_read_short(struct iobuf *, int32_t *);
int	iobuf_read_size(struct iobuf *, size_t *);
int	iobuf_read_vstring(struct iobuf *, struct vstring *);
void	iobuf_free(struct iobuf *);

/* accept(2) callback */
struct sockaddr_storage;
typedef int (rsync_client_handler)(struct sess *, int,
	    struct sockaddr_storage *, socklen_t);

/*
* The option filter should return 1 to allow an option to be processed, 0 to
* stop processing, or -1 to simply skip it.
*/
struct option;
typedef int (rsync_option_filter)(struct sess *, int, const struct option *);

struct opts	*rsync_getopt(int, char *[], rsync_option_filter *,
		    struct sess *);

int	send_iflags(struct sess *, void **, size_t *, size_t *,
	    size_t *, struct flist *, int32_t);

int	rsync_receiver(struct sess *, struct cleanup_ctx *, int, int,
	    const char *);
int	rsync_sender(struct sess *, int, int, size_t, char **);
int	rsync_batch(struct cleanup_ctx *, struct opts *, const struct fargs *);
int	rsync_client(struct cleanup_ctx *, const struct opts *, int,
	    const struct fargs *);
int	rsync_daemon(int, char *[], struct opts *);
int	rsync_connect(const struct opts *, int *, const struct fargs *);
int	rsync_listen(struct sess *, rsync_client_handler *);
int	rsync_setsockopts(int, const char *);
int	rsync_socket(struct cleanup_ctx *, const struct opts *, int,
	    const struct fargs *);

int	rsync_is_socket(int);
int	rsync_password_hash(const char *, const char *, char *, size_t);
int	rsync_server(struct cleanup_ctx *, const struct opts *, size_t,
	    char *[]);
int	rsync_downloader(struct download *, struct sess *, int *, size_t,
	    const struct hardlinks *);
int	rsync_set_metadata(struct sess *, int, int, const struct flist *,
	    const char *);
int	rsync_set_metadata_at(struct sess *, int, int, const struct flist *,
	    const char *);
int	rsync_uploader(struct upload *, int *, struct sess *, int *,
		       const struct hardlinks *);
int	rsync_uploader_tail(struct upload *, struct sess *);

struct download	*download_alloc(struct sess *, int, struct flist *, size_t,
		    int, int);
size_t		 download_needs_redo(struct download *);
const char	*download_partial_path(struct sess *, const struct flist *,
		    char *, size_t);
const char	*download_partial_filepath(const struct flist *);
void		 download_interrupted(struct sess *, struct download *);
void		 download_free(struct sess *, struct download *);
struct upload	*upload_alloc(const char *, int, int, int, size_t,
		   struct flist *, size_t, mode_t);
void		upload_next_phase(struct upload *, struct sess *, int);
void		upload_ack_complete(struct upload *, struct sess *, int);
void		upload_free(struct upload *);
int		upload_del(struct upload *, struct sess *);

struct blktab	*blkhash_alloc(void);
int		 blkhash_set(struct blktab *, const struct blkset *);
void		 blkhash_free(struct blktab *);

struct blkset	*blk_recv(struct sess *, int, struct iobuf *, const char *,
		    struct blkset *, size_t *, enum send_dl_state *);
void		 blk_recv_ack(char [16], const struct blkset *, int32_t);
void		 blk_match(struct sess *, const struct blkset *,
		    const char *, struct blkstat *);
int		 blk_send(struct sess *, int, size_t, const struct blkset *,
		    const char *);
int		 blk_send_ack(struct sess *, int, struct blkset *);

uint32_t	 hash_fast(const void *, size_t);
void		 hash_slow(const void *, size_t, unsigned char *,
		    const struct sess *);
void		 hash_file(const void *, size_t, unsigned char *,
		    const struct sess *);
int		 hash_file_by_path(int, const char *, size_t, unsigned char *);

/*
 * Use of move_file should ideally be limited to copy.c and platform.c -- in
 * recent versions of openrsync, there's a platform_move_file that may implement
 * it slightly differently for a given platform, or it may call back into the
 * generic move_file() that should handle things well enough for the majority of
 * platforms.
 */
int		 move_file(int, const char *, int, const char *, int);
void		 copy_file(int, const char *, const struct flist *);
int		 backup_to_dir(struct sess *, int, const struct flist *,
		    const char *, mode_t);

int		 is_unsafe_link(const char *, const char *, const char *);
char		*make_safe_link(const char *);

int		 mkpath(char *, mode_t);
int		 mkpathat(int fd, char *, mode_t);
int		 mksock(const char *, char *);

int		 mkstempat(int, char *);
char		*mkstemplinkat(char*, int, char *);
char		*mkstempfifoat(int, char *);
char		*mkstempnodat(int, char *, mode_t, dev_t);
char		*mkstempsock(const char *, char *);
int		 mktemplate(char **, const char *, int, int);

int		 platform_flist_modify(const struct sess *, struct fl *);
int		 platform_flist_entry_received(struct sess *, int,
		    struct flist *);
void		 platform_flist_received(struct sess *, struct flist *, size_t);
int		 platform_move_file(const struct sess *, struct flist *,
		    int, const char *, int, const char *, int);
int		 platform_finish_transfer(const struct sess *, struct flist *,
		    int, const char *);

int		 parse_rule_words(const char *line, enum rule_type, int);
int		 parse_rule(const char *line, enum rule_type, int);
void		 parse_file(const char *, enum rule_type, int);
void		 send_rules(struct sess *, int);
void		 recv_rules(struct sess *, int);
char		**rules_export(struct sess *);
int		 rules_match(const char *, int, enum fmode, int);
void		 rules_dir_push(const char *, size_t, int);
void		 rules_dir_pop(const char *, size_t);
void		 rules_base(const char *);

int		 rmatch(const char *, const char *, int);

char		*symlink_read(const char *);
char		*symlinkat_read(int, const char *);

int		 sess_stats_send(struct sess *, int);
int		 sess_stats_recv(struct sess *, int);

int		 idents_add(int, struct ident **, size_t *, int32_t);
void		 idents_assign_gid(struct sess *, struct flist *, size_t,
		    const struct ident *, size_t);
void		 idents_assign_uid(struct sess *, struct flist *, size_t,
		    const struct ident *, size_t);
void		 idents_free(struct ident *, size_t);
int		 idents_recv(struct sess *, int, struct ident **, size_t *);
void		 idents_remap(struct sess *, int, struct ident *, size_t);
int		 idents_send(struct sess *, int, const struct ident *, size_t);

int		 iszerobuf(const void *b, size_t len);

int              read_filesfrom(struct sess *sess, const char *basedir);
void		 cleanup_filesfrom(struct sess *sess);
void             print_filesfrom(char *const *, int, const char *);
void		 delayed_renames(struct sess *sess);

int		 chmod_parse(const char *arg, struct sess *sess);

int		 rsync_humanize(struct sess *, char *, size_t, int64_t);

int		 scan_scaled_def(char *, long long *, char);

static inline int
sess_is_inplace(struct sess *sess)
{

	return sess->opts->inplace || sess->opts->append;
}

#ifndef S_ISTXT
#define S_ISTXT S_ISVTX
#endif

int output(struct sess *sess, const struct flist *fl, int do_print);
void our_strmode(mode_t mode, char *p);
void print_7_or_8_bit(const struct sess *sess, const char *fmt, const char *s);

#endif /*!EXTERN_H*/
