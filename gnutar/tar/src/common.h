/* Common declarations for the tar program.
   Copyright (C) 1988, 92, 93, 94, 96, 97, 1999 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any later
   version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
   Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Declare the GNU tar archive format.  */
#include "tar.h"

/* The checksum field is filled with this while the checksum is computed.  */
#define CHKBLANKS	"        "	/* 8 blanks, no null */

/* Some constants from POSIX are given names.  */
#define NAME_FIELD_SIZE   100
#define PREFIX_FIELD_SIZE 155
#define UNAME_FIELD_SIZE   32
#define GNAME_FIELD_SIZE   32

/* Some various global definitions.  */

/* Name of file to use for interacting with user.  */
#if MSDOS
# define TTY_NAME "con"
#else
# define TTY_NAME "/dev/tty"
#endif

/* GLOBAL is defined to empty in `tar.c' only, and left alone in other `*.c'
   modules.  Here, we merely set it to "extern" if it is not already set.
   GNU tar does depend on the system loader to preset all GLOBAL variables to
   neutral (or zero) values, explicit initialisation is usually not done.  */
#ifndef GLOBAL
# define GLOBAL extern
#endif

/* Exit status for GNU tar.  Let's try to keep this list as simple as
   possible.  -d option strongly invites a status different for unequal
   comparison and other errors.  */
GLOBAL int exit_status;

#define TAREXIT_SUCCESS 0
#define TAREXIT_DIFFERS 1
#define TAREXIT_FAILURE 2

/* Both WARN and ERROR write a message on stderr and continue processing,
   however ERROR manages so tar will exit unsuccessfully.  FATAL_ERROR
   writes a message on stderr and aborts immediately, with another message
   line telling so.  USAGE_ERROR works like FATAL_ERROR except that the
   other message line suggests trying --help.  All four macros accept a
   single argument of the form ((0, errno, _("FORMAT"), Args...)).  `errno'
   is `0' when the error is not being detected by the system.  */

#define WARN(Args) \
  error Args
#define ERROR(Args) \
  (error Args, exit_status = TAREXIT_FAILURE)
#define FATAL_ERROR(Args) \
  (error Args, error (TAREXIT_FAILURE, 0, \
		      _("Error is not recoverable: exiting now")), 0)
#define USAGE_ERROR(Args) \
  (error Args, usage (TAREXIT_FAILURE), 0)

/* Information gleaned from the command line.  */

#include "arith.h"
#include "backupfile.h"
#include "basename.h"
#include "exclude.h"
#include "modechange.h"
#include "safe-read.h"

/* Name of this program.  */
GLOBAL const char *program_name;

/* Main command option.  */

enum subcommand
{
  UNKNOWN_SUBCOMMAND,		/* none of the following */
  APPEND_SUBCOMMAND,		/* -r */
  CAT_SUBCOMMAND,		/* -A */
  CREATE_SUBCOMMAND,		/* -c */
  DELETE_SUBCOMMAND,		/* -D */
  DIFF_SUBCOMMAND,		/* -d */
  EXTRACT_SUBCOMMAND,		/* -x */
  LIST_SUBCOMMAND,		/* -t */
  UPDATE_SUBCOMMAND		/* -u */
};

GLOBAL enum subcommand subcommand_option;

/* Selected format for output archive.  */
GLOBAL enum archive_format archive_format;

/* Either NL or NUL, as decided by the --null option.  */
GLOBAL char filename_terminator;

/* Size of each record, once in blocks, once in bytes.  Those two variables
   are always related, the second being BLOCKSIZE times the first.  They do
   not have _option in their name, even if their values is derived from
   option decoding, as these are especially important in tar.  */
GLOBAL int blocking_factor;
GLOBAL size_t record_size;

/* Boolean value.  */
GLOBAL int absolute_names_option;

/* This variable tells how to interpret newer_mtime_option, below.  If zero,
   files get archived if their mtime is not less than newer_mtime_option.
   If nonzero, files get archived if *either* their ctime or mtime is not less
   than newer_mtime_option.  */
GLOBAL int after_date_option;

/* Boolean value.  */
GLOBAL int atime_preserve_option;

/* Boolean value.  */
GLOBAL int backup_option;

/* Type of backups being made.  */
GLOBAL enum backup_type backup_type;

/* Boolean value.  */
GLOBAL int block_number_option;

/* Boolean value.  */
GLOBAL int checkpoint_option;

/* Specified name of compression program, or "gzip" as implied by -z.  */
GLOBAL const char *use_compress_program_option;

/* Boolean value.  */
GLOBAL int dereference_option;

/* Patterns that match file names to be excluded.  */
GLOBAL struct exclude *excluded;

/* Specified file containing names to work on.  */
GLOBAL const char *files_from_option;

/* Boolean value.  */
GLOBAL int force_local_option;

/* Specified value to be put into tar file in place of stat () results, or
   just -1 if such an override should not take place.  */
GLOBAL gid_t group_option;

/* Boolean value.  */
GLOBAL int ignore_failed_read_option;

/* Boolean value.  */
GLOBAL int ignore_zeros_option;

/* Boolean value.  */
GLOBAL int incremental_option;

/* Specified name of script to run at end of each tape change.  */
GLOBAL const char *info_script_option;

/* Boolean value.  */
GLOBAL int interactive_option;

/* Boolean value.  */
GLOBAL int keep_old_files_option;

/* Specified file name for incremental list.  */
GLOBAL const char *listed_incremental_option;

/* Specified mode change string.  */
GLOBAL struct mode_change *mode_option;

/* Boolean value.  */
GLOBAL int multi_volume_option;

/* The same variable hold the time, whether mtime or ctime.  Just fake a
   non-existing option, for making the code clearer, elsewhere.  */
#define newer_ctime_option newer_mtime_option

/* Specified threshold date and time.  Files having a more recent timestamp
   get archived (also see after_date_option above).  If left to zero, it may
   be interpreted as very low threshold, just usable as such.  */
GLOBAL time_t newer_mtime_option;

/* Boolean value.  */
GLOBAL int no_recurse_option;

/* Boolean value.  */
GLOBAL int numeric_owner_option;

/* Boolean value.  */
GLOBAL int one_file_system_option;

/* Specified value to be put into tar file in place of stat () results, or
   just -1 if such an override should not take place.  */
GLOBAL uid_t owner_option;

/* Boolean value.  */
GLOBAL int recursive_unlink_option;

/* Boolean value.  */
GLOBAL int read_full_records_option;

/* Boolean value.  */
GLOBAL int remove_files_option;

/* Specified remote shell command.  */
GLOBAL const char *rsh_command_option;

/* Boolean value.  */
GLOBAL int same_order_option;

/* Boolean value.  */
GLOBAL int same_owner_option;

/* Boolean value.  */
GLOBAL int same_permissions_option;

/* Boolean value.  */
GLOBAL int show_omitted_dirs_option;

/* Boolean value.  */
GLOBAL int sparse_option;

/* Boolean value.  */
GLOBAL int starting_file_option;

/* Specified maximum byte length of each tape volume (multiple of 1024).  */
GLOBAL tarlong tape_length_option;

/* Boolean value.  */
GLOBAL int to_stdout_option;

/* Boolean value.  */
GLOBAL int totals_option;

/* Boolean value.  */
GLOBAL int touch_option;

/* Boolean value.  */
GLOBAL int unlink_first_option;

/* Count how many times the option has been set, multiple setting yields
   more verbose behavior.  Value 0 means no verbosity, 1 means file name
   only, 2 means file name and all attributes.  More than 2 is just like 2.  */
GLOBAL int verbose_option;

/* Boolean value.  */
GLOBAL int verify_option;

/* Specified name of file containing the volume number.  */
GLOBAL const char *volno_file_option;

/* Specified value or pattern.  */
GLOBAL const char *volume_label_option;

/* Other global variables.  */

/* File descriptor for archive file.  */
GLOBAL int archive;

/* Nonzero when outputting to /dev/null.  */
GLOBAL int dev_null_output;

/* Name of file for the current archive entry.  */
GLOBAL char *current_file_name;

/* Name of link for the current archive entry.  */
GLOBAL char *current_link_name;

/* List of tape drive names, number of such tape drives, allocated number,
   and current cursor in list.  */
GLOBAL const char **archive_name_array;
GLOBAL int archive_names;
GLOBAL int allocated_archive_names;
GLOBAL const char **archive_name_cursor;

/* Structure for keeping track of filenames and lists thereof.  */
struct name
  {
    struct name *next;
    size_t length;		/* cached strlen(name) */
    char found;			/* a matching file has been found */
    char firstch;		/* first char is literally matched */
    char regexp;		/* this name is a regexp, not literal */
    char *change_dir;		/* set with the -C option */
    const char *dir_contents;	/* for incremental_option */
    char fake;			/* dummy entry */
    char name[1];
  };
GLOBAL struct name *namelist;	/* points to first name in list */
GLOBAL struct name *namelast;	/* points to last name in list */

/* Pointer to the start of the scratch space.  */
struct sp_array
  {
    off_t offset;
    size_t numbytes;
  };
GLOBAL struct sp_array *sparsearray;

/* Initial size of the sparsearray.  */
GLOBAL int sp_array_size;

/* Declarations for each module.  */

/* FIXME: compare.c should not directly handle the following variable,
   instead, this should be done in buffer.c only.  */

enum access_mode
{
  ACCESS_READ,
  ACCESS_WRITE,
  ACCESS_UPDATE
};
extern enum access_mode access_mode;

/* Module buffer.c.  */

extern FILE *stdlis;
extern char *save_name;
extern off_t save_sizeleft;
extern off_t save_totsize;
extern int write_archive_to_stdout;

size_t available_space_after PARAMS ((union block *));
off_t current_block_ordinal PARAMS ((void));
void close_archive PARAMS ((void));
void closeout_volume_number PARAMS ((void));
union block *find_next_block PARAMS ((void));
void flush_read PARAMS ((void));
void flush_write PARAMS ((void));
void flush_archive PARAMS ((void));
void init_total_written PARAMS ((void));
void init_volume_number PARAMS ((void));
void open_archive PARAMS ((enum access_mode));
void print_total_written PARAMS ((void));
void reset_eof PARAMS ((void));
void set_next_block_after PARAMS ((union block *));

/* Module create.c.  */

void create_archive PARAMS ((void));
void dump_file PARAMS ((char *, dev_t, int));
void finish_header PARAMS ((union block *));
void write_eot PARAMS ((void));

#define GID_TO_OCT(val, where) gid_to_oct (val, where, sizeof (where))
#define MAJOR_TO_OCT(val, where) major_to_oct (val, where, sizeof (where))
#define MINOR_TO_OCT(val, where) minor_to_oct (val, where, sizeof (where))
#define MODE_TO_OCT(val, where) mode_to_oct (val, where, sizeof (where))
#define OFF_TO_OCT(val, where) off_to_oct (val, where, sizeof (where))
#define SIZE_TO_OCT(val, where) size_to_oct (val, where, sizeof (where))
#define TIME_TO_OCT(val, where) time_to_oct (val, where, sizeof (where))
#define UID_TO_OCT(val, where) uid_to_oct (val, where, sizeof (where))
#define UINTMAX_TO_OCT(val, where) uintmax_to_oct (val, where, sizeof (where))

void gid_to_oct PARAMS ((gid_t, char *, size_t));
void major_to_oct PARAMS ((major_t, char *, size_t));
void minor_to_oct PARAMS ((minor_t, char *, size_t));
void mode_to_oct PARAMS ((mode_t, char *, size_t));
void off_to_oct PARAMS ((off_t, char *, size_t));
void size_to_oct PARAMS ((size_t, char *, size_t));
void time_to_oct PARAMS ((time_t, char *, size_t));
void uid_to_oct PARAMS ((uid_t, char *, size_t));
void uintmax_to_oct PARAMS ((uintmax_t, char *, size_t));

/* Module diffarch.c.  */

extern int now_verifying;

void diff_archive PARAMS ((void));
void diff_init PARAMS ((void));
void verify_volume PARAMS ((void));

/* Module extract.c.  */

void extr_init PARAMS ((void));
void extract_archive PARAMS ((void));
void apply_delayed_set_stat PARAMS ((void));

/* Module delete.c.  */

void delete_archive_members PARAMS ((void));

/* Module incremen.c.  */

void collect_and_sort_names PARAMS ((void));
char *get_directory_contents PARAMS ((char *, dev_t));
void write_dir_file PARAMS ((void));
void gnu_restore PARAMS ((int));
void write_directory_file PARAMS ((void));

/* Module list.c.  */

enum read_header
{
  HEADER_STILL_UNREAD,		/* for when read_header has not been called */
  HEADER_SUCCESS,		/* header successfully read and checksummed */
  HEADER_ZERO_BLOCK,		/* zero block where header expected */
  HEADER_END_OF_FILE,		/* true end of file while header expected */
  HEADER_FAILURE		/* ill-formed header, or bad checksum */
};

extern union block *current_header;
extern struct stat current_stat;
extern enum archive_format current_format;

void decode_header PARAMS ((union block *, struct stat *,
			    enum archive_format *, int));
#define STRINGIFY_BIGINT(i, b) \
  stringify_uintmax_t_backwards ((uintmax_t) (i), (b) + sizeof (b))
char *stringify_uintmax_t_backwards PARAMS ((uintmax_t, char *));

#define GID_FROM_OCT(where) gid_from_oct (where, sizeof (where))
#define MAJOR_FROM_OCT(where) major_from_oct (where, sizeof (where))
#define MINOR_FROM_OCT(where) minor_from_oct (where, sizeof (where))
#define MODE_FROM_OCT(where) mode_from_oct (where, sizeof (where))
#define OFF_FROM_OCT(where) off_from_oct (where, sizeof (where))
#define SIZE_FROM_OCT(where) size_from_oct (where, sizeof (where))
#define TIME_FROM_OCT(where) time_from_oct (where, sizeof (where))
#define UID_FROM_OCT(where) uid_from_oct (where, sizeof (where))
#define UINTMAX_FROM_OCT(where) uintmax_from_oct (where, sizeof (where))

gid_t gid_from_oct PARAMS ((const char *, size_t));
major_t major_from_oct PARAMS ((const char *, size_t));
minor_t minor_from_oct PARAMS ((const char *, size_t));
mode_t mode_from_oct PARAMS ((const char *, size_t));
off_t off_from_oct PARAMS ((const char *, size_t));
size_t size_from_oct PARAMS ((const char *, size_t));
time_t time_from_oct PARAMS ((const char *, size_t));
uid_t uid_from_oct PARAMS ((const char *, size_t));
uintmax_t uintmax_from_oct PARAMS ((const char *, size_t));

void list_archive PARAMS ((void));
void print_for_mkdir PARAMS ((char *, int, mode_t));
void print_header PARAMS ((void));
void read_and PARAMS ((void (*do_) ()));
enum read_header read_header PARAMS ((void));
void skip_extended_headers PARAMS ((void));
void skip_file PARAMS ((off_t));

/* Module mangle.c.  */

void extract_mangle PARAMS ((void));

/* Module misc.c.  */

void assign_string PARAMS ((char **, const char *));
char *quote_copy_string PARAMS ((const char *));
int unquote_string PARAMS ((char *));

char *merge_sort PARAMS ((char *, int, int, int (*) (char *, char *)));

int is_dot_or_dotdot PARAMS ((const char *));
int remove_any_file PARAMS ((const char *, int));
int maybe_backup_file PARAMS ((const char *, int));
void undo_last_backup PARAMS ((void));

/* Module names.c.  */

void gid_to_gname PARAMS ((gid_t, char gname[GNAME_FIELD_SIZE]));
int gname_to_gid PARAMS ((char gname[GNAME_FIELD_SIZE], gid_t *));
void uid_to_uname PARAMS ((uid_t, char uname[UNAME_FIELD_SIZE]));
int uname_to_uid PARAMS ((char uname[UNAME_FIELD_SIZE], uid_t *));

void init_names PARAMS ((void));
void name_add PARAMS ((const char *));
void name_init PARAMS ((int, char *const *));
void name_term PARAMS ((void));
char *name_next PARAMS ((int change_));
void name_close PARAMS ((void));
void name_gather PARAMS ((void));
void addname PARAMS ((const char *));
int name_match PARAMS ((const char *));
void names_notfound PARAMS ((void));
void name_expand PARAMS ((void));
struct name *name_scan PARAMS ((const char *));
char *name_from_list PARAMS ((void));
void blank_name_list PARAMS ((void));
char *new_name PARAMS ((const char *, const char *));

/* Module tar.c.  */

int confirm PARAMS ((const char *, const char *));
void request_stdin PARAMS ((const char *));

/* Module update.c.  */

extern char *output_start;

void update_archive PARAMS ((void));
