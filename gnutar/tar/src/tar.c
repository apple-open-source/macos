/* A tar (tape archiver) program.
   Copyright (C) 1988, 92,93,94,95,96,97, 1999 Free Software Foundation, Inc.
   Written by John Gilmore, starting 1985-08-25.

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

#include "system.h"

#include <getopt.h>

#include <signal.h>
#if ! defined SIGCHLD && defined SIGCLD
# define SIGCHLD SIGCLD
#endif

/* The following causes "common.h" to produce definitions of all the global
   variables, rather than just "extern" declarations of them.  GNU tar does
   depend on the system loader to preset all GLOBAL variables to neutral (or
   zero) values, explicit initialisation is usually not done.  */
#define GLOBAL
#include "common.h"

#include "xstrtol.h"

time_t get_date ();

/* Local declarations.  */

#ifndef DEFAULT_ARCHIVE
# define DEFAULT_ARCHIVE "tar.out"
#endif

#ifndef DEFAULT_BLOCKING
# define DEFAULT_BLOCKING 20
#endif

static void usage PARAMS ((int));

/* Miscellaneous.  */

/*----------------------------------------------.
| Doesn't return if stdin already requested.    |
`----------------------------------------------*/

/* Name of option using stdin.  */
static const char *stdin_used_by = NULL;

void
request_stdin (const char *option)
{
  if (stdin_used_by)
    USAGE_ERROR ((0, 0, _("Options `-%s' and `-%s' both want standard input"),
		  stdin_used_by, option));

  stdin_used_by = option;
}

/*--------------------------------------------------------.
| Returns true if and only if the user typed 'y' or 'Y'.  |
`--------------------------------------------------------*/

int
confirm (const char *message_action, const char *message_name)
{
  static FILE *confirm_file = NULL;

  if (!confirm_file)
    {
      if (archive == 0 || stdin_used_by)
	confirm_file = fopen (TTY_NAME, "r");
      else
	{
	  request_stdin ("-w");
	  confirm_file = stdin;
	}

      if (!confirm_file)
	FATAL_ERROR ((0, 0, _("Cannot read confirmation from user")));
    }

  fprintf (stdlis, "%s %s?", message_action, message_name);
  fflush (stdlis);

  {
    int reply = getc (confirm_file);
    int character;

    for (character = reply;
	 character != '\n' && character != EOF;
	 character = getc (confirm_file))
      continue;
    return reply == 'y' || reply == 'Y';
  }
}

/* Options.  */

/* For long options that unconditionally set a single flag, we have getopt
   do it.  For the others, we share the code for the equivalent short
   named option, the name of which is stored in the otherwise-unused `val'
   field of the `struct option'; for long options that have no equivalent
   short option, we use non-characters as pseudo short options,
   starting at CHAR_MAX + 1 and going upwards.  */

enum
{
  BACKUP_OPTION = CHAR_MAX + 1,
  DELETE_OPTION,
  EXCLUDE_OPTION,
  GROUP_OPTION,
  MODE_OPTION,
  NEWER_MTIME_OPTION,
  NO_RECURSE_OPTION,
  NULL_OPTION,
  OWNER_OPTION,
  POSIX_OPTION,
  PRESERVE_OPTION,
  RECORD_SIZE_OPTION,
  RSH_COMMAND_OPTION,
  SUFFIX_OPTION,
  USE_COMPRESS_PROGRAM_OPTION,
  VOLNO_FILE_OPTION,

  /* Some cleanup is being made in GNU tar long options.  Using old names is
     allowed for a while, but will also send a warning to stderr.  Take old
     names out in 1.14, or in summer 1997, whichever happens last.  */

  OBSOLETE_ABSOLUTE_NAMES,
  OBSOLETE_BLOCK_COMPRESS,
  OBSOLETE_BLOCKING_FACTOR,
  OBSOLETE_BLOCK_NUMBER,
  OBSOLETE_READ_FULL_RECORDS,
  OBSOLETE_TOUCH,
  OBSOLETE_VERSION_CONTROL
};

/* If nonzero, display usage information and exit.  */
static int show_help = 0;

/* If nonzero, print the version on standard output and exit.  */
static int show_version = 0;

struct option long_options[] =
{
  {"absolute-names", no_argument, NULL, 'P'},
  {"absolute-paths", no_argument, NULL, OBSOLETE_ABSOLUTE_NAMES},
  {"after-date", required_argument, NULL, 'N'},
  {"append", no_argument, NULL, 'r'},
  {"atime-preserve", no_argument, &atime_preserve_option, 1},
  {"backup", optional_argument, NULL, BACKUP_OPTION},
  {"block-compress", no_argument, NULL, OBSOLETE_BLOCK_COMPRESS},
  {"block-number", no_argument, NULL, 'R'},
  {"block-size", required_argument, NULL, OBSOLETE_BLOCKING_FACTOR},
  {"blocking-factor", required_argument, NULL, 'b'},
  {"catenate", no_argument, NULL, 'A'},
  {"checkpoint", no_argument, &checkpoint_option, 1},
  {"compare", no_argument, NULL, 'd'},
  {"compress", no_argument, NULL, 'Z'},
  {"concatenate", no_argument, NULL, 'A'},
  {"confirmation", no_argument, NULL, 'w'},
  /* FIXME: --selective as a synonym for --confirmation?  */
  {"create", no_argument, NULL, 'c'},
  {"delete", no_argument, NULL, DELETE_OPTION},
  {"dereference", no_argument, NULL, 'h'},
  {"diff", no_argument, NULL, 'd'},
  {"directory", required_argument, NULL, 'C'},
  {"exclude", required_argument, NULL, EXCLUDE_OPTION},
  {"exclude-from", required_argument, NULL, 'X'},
  {"extract", no_argument, NULL, 'x'},
  {"file", required_argument, NULL, 'f'},
  {"files-from", required_argument, NULL, 'T'},
  {"force-local", no_argument, &force_local_option, 1},
  {"get", no_argument, NULL, 'x'},
  {"group", required_argument, NULL, GROUP_OPTION},
  {"gunzip", no_argument, NULL, 'z'},
  {"gzip", no_argument, NULL, 'z'},
  {"help", no_argument, &show_help, 1},
  {"ignore-failed-read", no_argument, &ignore_failed_read_option, 1},
  {"ignore-zeros", no_argument, NULL, 'i'},
  /* FIXME: --ignore-end as a new name for --ignore-zeros?  */
  {"incremental", no_argument, NULL, 'G'},
  {"info-script", required_argument, NULL, 'F'},
  {"interactive", no_argument, NULL, 'w'},
  {"keep-old-files", no_argument, NULL, 'k'},
  {"label", required_argument, NULL, 'V'},
  {"list", no_argument, NULL, 't'},
  {"listed-incremental", required_argument, NULL, 'g'},
  {"mode", required_argument, NULL, MODE_OPTION},
  {"modification-time", no_argument, NULL, OBSOLETE_TOUCH},
  {"multi-volume", no_argument, NULL, 'M'},
  {"new-volume-script", required_argument, NULL, 'F'},
  {"newer", required_argument, NULL, 'N'},
  {"newer-mtime", required_argument, NULL, NEWER_MTIME_OPTION},
  {"null", no_argument, NULL, NULL_OPTION},
  {"no-recursion", no_argument, NULL, NO_RECURSE_OPTION},
  {"numeric-owner", no_argument, &numeric_owner_option, 1},
  {"old-archive", no_argument, NULL, 'o'},
  {"one-file-system", no_argument, NULL, 'l'},
  {"owner", required_argument, NULL, OWNER_OPTION},
  {"portability", no_argument, NULL, 'o'},
  {"posix", no_argument, NULL, POSIX_OPTION},
  {"preserve", no_argument, NULL, PRESERVE_OPTION},
  {"preserve-order", no_argument, NULL, 's'},
  {"preserve-permissions", no_argument, NULL, 'p'},
  {"recursive-unlink", no_argument, &recursive_unlink_option, 1},
  {"read-full-blocks", no_argument, NULL, OBSOLETE_READ_FULL_RECORDS},
  {"read-full-records", no_argument, NULL, 'B'},
  /* FIXME: --partial-blocks might be a synonym for --read-full-records?  */
  {"record-number", no_argument, NULL, OBSOLETE_BLOCK_NUMBER},
  {"record-size", required_argument, NULL, RECORD_SIZE_OPTION},
  {"remove-files", no_argument, &remove_files_option, 1},
  {"rsh-command", required_argument, NULL, RSH_COMMAND_OPTION},
  {"same-order", no_argument, NULL, 's'},
  {"same-owner", no_argument, &same_owner_option, 1},
  {"same-permissions", no_argument, NULL, 'p'},
  {"show-omitted-dirs", no_argument, &show_omitted_dirs_option, 1},
  {"sparse", no_argument, NULL, 'S'},
  {"starting-file", required_argument, NULL, 'K'},
  {"suffix", required_argument, NULL, SUFFIX_OPTION},
  {"tape-length", required_argument, NULL, 'L'},
  {"to-stdout", no_argument, NULL, 'O'},
  {"totals", no_argument, &totals_option, 1},
  {"touch", no_argument, NULL, 'm'},
  {"uncompress", no_argument, NULL, 'Z'},
  {"ungzip", no_argument, NULL, 'z'},
  {"unlink-first", no_argument, NULL, 'U'},
  {"update", no_argument, NULL, 'u'},
  {"use-compress-program", required_argument, NULL, USE_COMPRESS_PROGRAM_OPTION},
  {"verbose", no_argument, NULL, 'v'},
  {"verify", no_argument, NULL, 'W'},
  {"version", no_argument, &show_version, 1},
  {"version-control", required_argument, NULL, OBSOLETE_VERSION_CONTROL},
  {"volno-file", required_argument, NULL, VOLNO_FILE_OPTION},

  {0, 0, 0, 0}
};

/*---------------------------------------------.
| Print a usage message and exit with STATUS.  |
`---------------------------------------------*/

static void
usage (int status)
{
  if (status != TAREXIT_SUCCESS)
    fprintf (stderr, _("Try `%s --help' for more information.\n"),
	     program_name);
  else
    {
      fputs (_("\
GNU `tar' saves many files together into a single tape or disk archive, and\n\
can restore individual files from the archive.\n"),
	     stdout);
      printf (_("\nUsage: %s [OPTION]... [FILE]...\n"), program_name);
      fputs (_("\
\n\
If a long option shows an argument as mandatory, then it is mandatory\n\
for the equivalent short option also.  Similarly for optional arguments.\n"),
	     stdout);
      fputs(_("\
\n\
Main operation mode:\n\
  -t, --list              list the contents of an archive\n\
  -x, --extract, --get    extract files from an archive\n\
  -c, --create            create a new archive\n\
  -d, --diff, --compare   find differences between archive and file system\n\
  -r, --append            append files to the end of an archive\n\
  -u, --update            only append files newer than copy in archive\n\
  -A, --catenate          append tar files to an archive\n\
      --concatenate       same as -A\n\
      --delete            delete from the archive (not on mag tapes!)\n"),
	    stdout);
      fputs (_("\
\n\
Operation modifiers:\n\
  -W, --verify               attempt to verify the archive after writing it\n\
      --remove-files         remove files after adding them to the archive\n\
  -k, --keep-old-files       don't overwrite existing files when extracting\n\
  -U, --unlink-first         remove each file prior to extracting over it\n\
      --recursive-unlink     empty hierarchies prior to extracting directory\n\
  -S, --sparse               handle sparse files efficiently\n\
  -O, --to-stdout            extract files to standard output\n\
  -G, --incremental          handle old GNU-format incremental backup\n\
  -g, --listed-incremental   handle new GNU-format incremental backup\n\
      --ignore-failed-read   do not exit with nonzero on unreadable files\n"),
	     stdout);
      fputs (_("\
\n\
Handling of file attributes:\n\
      --owner=NAME             force NAME as owner for added files\n\
      --group=NAME             force NAME as group for added files\n\
      --mode=CHANGES           force (symbolic) mode CHANGES for added files\n\
      --atime-preserve         don't change access times on dumped files\n\
  -m, --modification-time      don't extract file modified time\n\
      --same-owner             try extracting files with the same ownership\n\
      --numeric-owner          always use numbers for user/group names\n\
  -p, --same-permissions       extract all protection information\n\
      --preserve-permissions   same as -p\n\
  -s, --same-order             sort names to extract to match archive\n\
      --preserve-order         same as -s\n\
      --preserve               same as both -p and -s\n"),
	     stdout);
      fputs (_("\
\n\
Device selection and switching:\n\
  -f, --file=ARCHIVE             use archive file or device ARCHIVE\n\
      --force-local              archive file is local even if has a colon\n\
      --rsh-command=COMMAND      use remote COMMAND instead of rsh\n\
  -[0-7][lmh]                    specify drive and density\n\
  -M, --multi-volume             create/list/extract multi-volume archive\n\
  -L, --tape-length=NUM          change tape after writing NUM x 1024 bytes\n\
  -F, --info-script=FILE         run script at end of each tape (implies -M)\n\
      --new-volume-script=FILE   same as -F FILE\n\
      --volno-file=FILE          use/update the volume number in FILE\n"),
	     stdout);
      fputs (_("\
\n\
Device blocking:\n\
  -b, --blocking-factor=BLOCKS   BLOCKS x 512 bytes per record\n\
      --record-size=SIZE         SIZE bytes per record, multiple of 512\n\
  -i, --ignore-zeros             ignore zeroed blocks in archive (means EOF)\n\
  -B, --read-full-records        reblock as we read (for 4.2BSD pipes)\n"),
	     stdout);
      fputs (_("\
\n\
Archive format selection:\n\
  -V, --label=NAME                   create archive with volume name NAME\n\
              PATTERN                at list/extract time, a globbing PATTERN\n\
  -o, --old-archive, --portability   write a V7 format archive\n\
      --posix                        write a POSIX conformant archive\n\
  -z, --gzip, --ungzip               filter the archive through gzip\n\
  -Z, --compress, --uncompress       filter the archive through compress\n\
      --use-compress-program=PROG    filter through PROG (must accept -d)\n"),
	     stdout);
      fputs (_("\
\n\
Local file selection:\n\
  -C, --directory=DIR          change to directory DIR\n\
  -T, --files-from=NAME        get names to extract or create from file NAME\n\
      --null                   -T reads null-terminated names, disable -C\n\
      --exclude=PATTERN        exclude files, given as a globbing PATTERN\n\
  -X, --exclude-from=FILE      exclude globbing patterns listed in FILE\n\
  -P, --absolute-names         don't strip leading `/'s from file names\n\
  -h, --dereference            dump instead the files symlinks point to\n\
      --no-recursion           avoid descending automatically in directories\n\
  -l, --one-file-system        stay in local file system when creating archive\n\
  -K, --starting-file=NAME     begin at file NAME in the archive\n"),
	     stdout);
#if !MSDOS
      fputs (_("\
  -N, --newer=DATE             only store files newer than DATE\n\
      --newer-mtime            compare date and time when data changed only\n\
      --after-date=DATE        same as -N\n"),
	     stdout);
#endif
      fputs (_("\
      --backup[=CONTROL]       backup before removal, choose version control\n\
      --suffix=SUFFIX          backup before removel, override usual suffix\n"),
	     stdout);
      fputs (_("\
\n\
Informative output:\n\
      --help            print this help, then exit\n\
      --version         print tar program version number, then exit\n\
  -v, --verbose         verbosely list files processed\n\
      --checkpoint      print directory names while reading the archive\n\
      --totals          print total bytes written while creating archive\n\
  -R, --block-number    show block number within archive with each message\n\
  -w, --interactive     ask for confirmation for every action\n\
      --confirmation    same as -w\n"),
	     stdout);
      fputs (_("\
\n\
The backup suffix is `~', unless set with --suffix or SIMPLE_BACKUP_SUFFIX.\n\
The version control may be set with --backup or VERSION_CONTROL, values are:\n\
\n\
  t, numbered     make numbered backups\n\
  nil, existing   numbered if numbered backups exist, simple otherwise\n\
  never, simple   always make simple backups\n"),
	     stdout);
      printf (_("\
\n\
GNU tar cannot read nor produce `--posix' archives.  If POSIXLY_CORRECT\n\
is set in the environment, GNU extensions are disallowed with `--posix'.\n\
Support for POSIX is only partially implemented, don't count on it yet.\n\
ARCHIVE may be FILE, HOST:FILE or USER@HOST:FILE; and FILE may be a file\n\
or a device.  *This* `tar' defaults to `-f%s -b%d'.\n"),
	      DEFAULT_ARCHIVE, DEFAULT_BLOCKING);
      fputs (_("\
\n\
Report bugs to <tar-bugs@gnu.org>.\n"),
	       stdout);
    }
  exit (status);
}

/*----------------------------.
| Parse the options for tar.  |
`----------------------------*/

/* Available option letters are DEHIJQY and aejnqy.  Some are reserved:

   y  per-file gzip compression
   Y  per-block gzip compression */

#define OPTION_STRING \
  "-01234567ABC:F:GK:L:MN:OPRST:UV:WX:Zb:cdf:g:hiklmoprstuvwxz"

static void
set_subcommand_option (enum subcommand subcommand)
{
  if (subcommand_option != UNKNOWN_SUBCOMMAND
      && subcommand_option != subcommand)
    USAGE_ERROR ((0, 0,
		  _("You may not specify more than one `-Acdtrux' option")));

  subcommand_option = subcommand;
}

static void
set_use_compress_program_option (const char *string)
{
  if (use_compress_program_option && strcmp (use_compress_program_option, string) != 0)
    USAGE_ERROR ((0, 0, _("Conflicting compression options")));

  use_compress_program_option = string;
}

static void
decode_options (int argc, char *const *argv)
{
  int optchar;			/* option letter */
  int input_files;		/* number of input files */
  const char *backup_suffix_string;
  const char *version_control_string = NULL;

  /* Set some default option values.  */

  subcommand_option = UNKNOWN_SUBCOMMAND;
  archive_format = DEFAULT_FORMAT;
  blocking_factor = DEFAULT_BLOCKING;
  record_size = DEFAULT_BLOCKING * BLOCKSIZE;
  excluded = new_exclude ();

  owner_option = -1;
  group_option = -1;

  backup_suffix_string = getenv ("SIMPLE_BACKUP_SUFFIX");

  /* Convert old-style tar call by exploding option element and rearranging
     options accordingly.  */

  if (argc > 1 && argv[1][0] != '-')
    {
      int new_argc;		/* argc value for rearranged arguments */
      char **new_argv;		/* argv value for rearranged arguments */
      char *const *in;		/* cursor into original argv */
      char **out;		/* cursor into rearranged argv */
      const char *letter;	/* cursor into old option letters */
      char buffer[3];		/* constructed option buffer */
      const char *cursor;	/* cursor in OPTION_STRING */

      /* Initialize a constructed option.  */

      buffer[0] = '-';
      buffer[2] = '\0';

      /* Allocate a new argument array, and copy program name in it.  */

      new_argc = argc - 1 + strlen (argv[1]);
      new_argv = (char **) xmalloc (new_argc * sizeof (char *));
      in = argv;
      out = new_argv;
      *out++ = *in++;

      /* Copy each old letter option as a separate option, and have the
	 corresponding argument moved next to it.  */

      for (letter = *in++; *letter; letter++)
	{
	  buffer[1] = *letter;
	  *out++ = xstrdup (buffer);
	  cursor = strchr (OPTION_STRING, *letter);
	  if (cursor && cursor[1] == ':')
	    {
	      if (in < argv + argc)
		*out++ = *in++;
	      else
		USAGE_ERROR ((0, 0, _("Old option `%c' requires an argument."),
			      *letter));
	    }
	}

      /* Copy all remaining options.  */

      while (in < argv + argc)
	*out++ = *in++;

      /* Replace the old option list by the new one.  */

      argc = new_argc;
      argv = new_argv;
    }

  /* Parse all options and non-options as they appear.  */

  input_files = 0;

  while (optchar = getopt_long (argc, argv, OPTION_STRING, long_options, NULL),
	 optchar != EOF)
    switch (optchar)
      {
      case '?':
	usage (TAREXIT_FAILURE);

      case 0:
	break;

      case 1:
	/* File name or non-parsed option, because of RETURN_IN_ORDER
	   ordering triggerred by the leading dash in OPTION_STRING.  */

	name_add (optarg);
	input_files++;
	break;

      case 'A':
	set_subcommand_option (CAT_SUBCOMMAND);
	break;

      case OBSOLETE_BLOCK_COMPRESS:
	WARN ((0, 0, _("Obsolete option, now implied by --blocking-factor")));
	break;

      case OBSOLETE_BLOCKING_FACTOR:
	WARN ((0, 0, _("Obsolete option name replaced by --blocking-factor")));
	/* Fall through.  */

      case 'b':
	{
	  long l;
	  if (! (xstrtol (optarg, (char **) 0, 10, &l, "") == LONGINT_OK
		 && l == (blocking_factor = l)
		 && 0 < blocking_factor
		 && l == (record_size = l * (size_t) BLOCKSIZE) / BLOCKSIZE))
	    USAGE_ERROR ((0, 0, _("Invalid blocking factor")));
	}
	break;

      case OBSOLETE_READ_FULL_RECORDS:
	WARN ((0, 0,
	       _("Obsolete option name replaced by --read-full-records")));
	/* Fall through.  */

      case 'B':
	/* Try to reblock input records.  For reading 4.2BSD pipes.  */

	/* It would surely make sense to exchange -B and -R, but it seems
	   that -B has been used for a long while in Sun tar ans most
	   BSD-derived systems.  This is a consequence of the block/record
	   terminology confusion.  */

	read_full_records_option = 1;
	break;

      case 'c':
	set_subcommand_option (CREATE_SUBCOMMAND);
	break;

      case 'C':
	name_add ("-C");
	name_add (optarg);
	break;

      case 'd':
	set_subcommand_option (DIFF_SUBCOMMAND);
	break;

      case 'f':
	if (archive_names == allocated_archive_names)
	  {
	    allocated_archive_names *= 2;
	    archive_name_array = (const char **)
	      xrealloc (archive_name_array,
			sizeof (const char *) * allocated_archive_names);
	  }
	archive_name_array[archive_names++] = optarg;
	break;

      case 'F':
	/* Since -F is only useful with -M, make it implied.  Run this
	   script at the end of each tape.  */

	info_script_option = optarg;
	multi_volume_option = 1;
	break;

      case 'g':
	listed_incremental_option = optarg;
	/* Fall through.  */

      case 'G':
	/* We are making an incremental dump (FIXME: are we?); save
	   directories at the beginning of the archive, and include in each
	   directory its contents.  */

	incremental_option = 1;
	break;

      case 'h':
	/* Follow symbolic links.  */

	dereference_option = 1;
	break;

      case 'i':
	/* Ignore zero blocks (eofs).  This can't be the default,
	   because Unix tar writes two blocks of zeros, then pads out
	   the record with garbage.  */

	ignore_zeros_option = 1;
	break;

      case 'k':
	/* Don't overwrite existing files.  */

	keep_old_files_option = 1;
	break;

      case 'K':
	starting_file_option = 1;
	addname (optarg);
	break;

      case 'l':
	/* When dumping directories, don't dump files/subdirectories
	   that are on other filesystems.  */

	one_file_system_option = 1;
	break;

      case 'L':
	{
	  unsigned long u;
	  if (xstrtoul (optarg, (char **) 0, 10, &u, "") != LONG_MAX)
	    USAGE_ERROR ((0, 0, _("Invalid tape length")));
	  clear_tarlong (tape_length_option);
	  add_to_tarlong (tape_length_option, u);
	  mult_tarlong (tape_length_option, 1024);
	  multi_volume_option = 1;
	}
	break;

      case OBSOLETE_TOUCH:
	WARN ((0, 0, _("Obsolete option name replaced by --touch")));
	/* Fall through.  */

      case 'm':
	touch_option = 1;
	break;

      case 'M':
	/* Make multivolume archive: when we can't write any more into
	   the archive, re-open it, and continue writing.  */

	multi_volume_option = 1;
	break;

#if !MSDOS
      case 'N':
	after_date_option = 1;
	/* Fall through.  */

      case NEWER_MTIME_OPTION:
	if (newer_mtime_option)
	  USAGE_ERROR ((0, 0, _("More than one threshold date")));

	newer_mtime_option = get_date (optarg, (voidstar) 0);
	if (newer_mtime_option == (time_t) -1)
	  USAGE_ERROR ((0, 0, _("Invalid date format `%s'"), optarg));

	break;
#endif /* not MSDOS */

      case 'o':
	if (archive_format == DEFAULT_FORMAT)
	  archive_format = V7_FORMAT;
	else if (archive_format != V7_FORMAT)
	  USAGE_ERROR ((0, 0, _("Conflicting archive format options")));
	break;

      case 'O':
	to_stdout_option = 1;
	break;

      case 'p':
	same_permissions_option = 1;
	break;

      case OBSOLETE_ABSOLUTE_NAMES:
	WARN ((0, 0, _("Obsolete option name replaced by --absolute-names")));
	/* Fall through.  */

      case 'P':
	absolute_names_option = 1;
	break;

      case 'r':
	set_subcommand_option (APPEND_SUBCOMMAND);
	break;

      case OBSOLETE_BLOCK_NUMBER:
	WARN ((0, 0, _("Obsolete option name replaced by --block-number")));
	/* Fall through.  */

      case 'R':
	/* Print block numbers for debugging bad tar archives.  */

	/* It would surely make sense to exchange -B and -R, but it seems
	   that -B has been used for a long while in Sun tar ans most
	   BSD-derived systems.  This is a consequence of the block/record
	   terminology confusion.  */

	block_number_option = 1;
	break;

      case 's':
	/* Names to extr are sorted.  */

	same_order_option = 1;
	break;

      case 'S':
	sparse_option = 1;
	break;

      case 't':
	set_subcommand_option (LIST_SUBCOMMAND);
	verbose_option++;
	break;

      case 'T':
	files_from_option = optarg;
	break;

      case 'u':
	set_subcommand_option (UPDATE_SUBCOMMAND);
	break;

      case 'U':
	unlink_first_option = 1;
	break;

      case 'v':
	verbose_option++;
	break;

      case 'V':
	volume_label_option = optarg;
	break;

      case 'w':
	interactive_option = 1;
	break;

      case 'W':
	verify_option = 1;
	break;

      case 'x':
	set_subcommand_option (EXTRACT_SUBCOMMAND);
	break;

      case 'X':
	if (add_exclude_file (excluded, optarg, '\n') != 0)
	  FATAL_ERROR ((0, errno, "%s", optarg));
	break;

      case 'z':
	set_use_compress_program_option ("gzip");
	break;

      case 'Z':
	set_use_compress_program_option ("compress");
	break;

      case OBSOLETE_VERSION_CONTROL:
	WARN ((0, 0, _("Obsolete option name replaced by --backup")));
	/* Fall through.  */

      case BACKUP_OPTION:
	backup_option = 1;
	if (optarg)
	  version_control_string = optarg;
	break;

      case DELETE_OPTION:
	set_subcommand_option (DELETE_SUBCOMMAND);
	break;

      case EXCLUDE_OPTION:
	add_exclude (excluded, optarg);
	break;

      case GROUP_OPTION:
	if (! (strlen (optarg) < GNAME_FIELD_SIZE
	       && gname_to_gid (optarg, &group_option)))
	  {
	    uintmax_t g;
	    if (xstrtoumax (optarg, (char **) 0, 10, &g, "") == LONGINT_OK
		&& g == (gid_t) g)
	      group_option = g;
	    else
	      ERROR ((TAREXIT_FAILURE, 0, _("Invalid group given on option")));
	  }
	break;

      case MODE_OPTION:
	mode_option
	  = mode_compile (optarg,
			  MODE_MASK_EQUALS | MODE_MASK_PLUS | MODE_MASK_MINUS);
	if (mode_option == MODE_INVALID)
	  ERROR ((TAREXIT_FAILURE, 0, _("Invalid mode given on option")));
	if (mode_option == MODE_MEMORY_EXHAUSTED)
	  ERROR ((TAREXIT_FAILURE, 0, _("Memory exhausted")));
	break;

      case NO_RECURSE_OPTION:
	no_recurse_option = 1;
	break;

      case NULL_OPTION:
	filename_terminator = '\0';
	break;

      case OWNER_OPTION:
	if (! (strlen (optarg) < UNAME_FIELD_SIZE
	       && uname_to_uid (optarg, &owner_option)))
	  {
	    uintmax_t u;
	    if (xstrtoumax (optarg, (char **) 0, 10, &u, "") == LONGINT_OK
		&& u == (uid_t) u)
	      owner_option = u;
	    else
	      ERROR ((TAREXIT_FAILURE, 0, _("Invalid owner given on option")));
	  }
	break;

      case POSIX_OPTION:
#if OLDGNU_COMPATIBILITY
	if (archive_format == DEFAULT_FORMAT)
	  archive_format = GNU_FORMAT;
	else if (archive_format != GNU_FORMAT)
	  USAGE_ERROR ((0, 0, _("Conflicting archive format options")));
#else
	if (archive_format == DEFAULT_FORMAT)
	  archive_format = POSIX_FORMAT;
	else if (archive_format != POSIX_FORMAT)
	  USAGE_ERROR ((0, 0, _("Conflicting archive format options")));
#endif
	break;

      case PRESERVE_OPTION:
	same_permissions_option = 1;
	same_order_option = 1;
	break;

      case RECORD_SIZE_OPTION:
	{
	  uintmax_t u;
	  if (! (xstrtoumax (optarg, (char **) 0, 10, &u, "") == LONG_MAX
		 && u == (size_t) u))
	    USAGE_ERROR ((0, 0, _("Invalid record size")));
	  record_size = u;
	  if (record_size % BLOCKSIZE != 0)
	    USAGE_ERROR ((0, 0, _("Record size must be a multiple of %d."),
			  BLOCKSIZE));
	  blocking_factor = record_size / BLOCKSIZE;
	}
	break;

      case RSH_COMMAND_OPTION:
	rsh_command_option = optarg;
	break;

      case SUFFIX_OPTION:
	backup_option = 1;
	backup_suffix_string = optarg;
	break;

      case VOLNO_FILE_OPTION:
	volno_file_option = optarg;
	break;

      case USE_COMPRESS_PROGRAM_OPTION:
	set_use_compress_program_option (optarg);
	break;

      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':

#ifdef DEVICE_PREFIX
	{
	  int device = optchar - '0';
	  int density;
	  static char buf[sizeof DEVICE_PREFIX + 10];
	  char *cursor;

	  density = getopt_long (argc, argv, "lmh", NULL, NULL);
	  strcpy (buf, DEVICE_PREFIX);
	  cursor = buf + strlen (buf);

#ifdef DENSITY_LETTER

	  sprintf (cursor, "%d%c", device, density);

#else /* not DENSITY_LETTER */

	  switch (density)
	    {
	    case 'l':
#ifdef LOW_NUM
	      device += LOW_NUM;
#endif
	      break;

	    case 'm':
#ifdef MID_NUM
	      device += MID_NUM;
#else
	      device += 8;
#endif
	      break;

	    case 'h':
#ifdef HGH_NUM
	      device += HGH_NUM;
#else
	      device += 16;
#endif
	      break;

	    default:
	      usage (TAREXIT_FAILURE);
	    }
	  sprintf (cursor, "%d", device);

#endif /* not DENSITY_LETTER */

	  if (archive_names == allocated_archive_names)
	    {
	      allocated_archive_names *= 2;
	      archive_name_array = (const char **)
		xrealloc (archive_name_array,
			  sizeof (const char *) * allocated_archive_names);
	    }
	  archive_name_array[archive_names++] = buf;

	  /* FIXME: How comes this works for many archives when buf is
	     not xstrdup'ed?  */
	}
	break;

#else /* not DEVICE_PREFIX */

	USAGE_ERROR ((0, 0,
		      _("Options `-[0-7][lmh]' not supported by *this* tar")));

#endif /* not DEVICE_PREFIX */
      }

  /* Process trivial options.  */

  if (show_version)
    {
      printf ("tar (GNU %s) %s\n", PACKAGE, VERSION);
      fputs (_("\
\n\
Copyright (C) 1988, 92,93,94,95,96,97,98, 1999 Free Software Foundation, Inc.\n"),
	     stdout);
      fputs (_("\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"),
	     stdout);
      fputs (_("\
\n\
Written by John Gilmore and Jay Fenlason.\n"),
	     stdout);
      exit (TAREXIT_SUCCESS);
    }

  if (show_help)
    usage (TAREXIT_SUCCESS);

  /* Derive option values and check option consistency.  */

  if (archive_format == DEFAULT_FORMAT)
    {
#if OLDGNU_COMPATIBILITY
      archive_format = OLDGNU_FORMAT;
#else
      archive_format = GNU_FORMAT;
#endif
    }

  if (archive_format == GNU_FORMAT && getenv ("POSIXLY_CORRECT"))
    archive_format = POSIX_FORMAT;

  if ((volume_label_option != NULL
       || incremental_option || multi_volume_option || sparse_option)
      && archive_format != OLDGNU_FORMAT && archive_format != GNU_FORMAT)
    USAGE_ERROR ((0, 0,
		  _("GNU features wanted on incompatible archive format")));

  if (archive_names == 0)
    {
      /* If no archive file name given, try TAPE from the environment, or
	 else, DEFAULT_ARCHIVE from the configuration process.  */

      archive_names = 1;
      archive_name_array[0] = getenv ("TAPE");
      if (archive_name_array[0] == NULL)
	archive_name_array[0] = DEFAULT_ARCHIVE;
    }

  /* Allow multiple archives only with `-M'.  */

  if (archive_names > 1 && !multi_volume_option)
    USAGE_ERROR ((0, 0,
		  _("Multiple archive files requires `-M' option")));

  /* If ready to unlink hierarchies, so we are for simpler files.  */
  if (recursive_unlink_option)
    unlink_first_option = 1;

  /* Forbid using -c with no input files whatsoever.  Check that `-f -',
     explicit or implied, is used correctly.  */

  switch (subcommand_option)
    {
    case CREATE_SUBCOMMAND:
      if (input_files == 0 && !files_from_option)
	USAGE_ERROR ((0, 0,
		      _("Cowardly refusing to create an empty archive")));
      break;

    case EXTRACT_SUBCOMMAND:
    case LIST_SUBCOMMAND:
    case DIFF_SUBCOMMAND:
      for (archive_name_cursor = archive_name_array;
	   archive_name_cursor < archive_name_array + archive_names;
	   archive_name_cursor++)
	if (!strcmp (*archive_name_cursor, "-"))
	  request_stdin ("-f");
      break;

    case CAT_SUBCOMMAND:
    case UPDATE_SUBCOMMAND:
    case APPEND_SUBCOMMAND:
      for (archive_name_cursor = archive_name_array;
	   archive_name_cursor < archive_name_array + archive_names;
	   archive_name_cursor++)
	if (!strcmp (*archive_name_cursor, "-"))
	  USAGE_ERROR ((0, 0,
			_("Options `-Aru' are incompatible with `-f -'")));

    default:
      break;
    }

  archive_name_cursor = archive_name_array;

  /* Prepare for generating backup names.  */

  if (backup_suffix_string)
    simple_backup_suffix = xstrdup (backup_suffix_string);

  if (backup_option)
    backup_type = xget_version ("--backup", version_control_string);
}

/* Tar proper.  */

/*-----------------------.
| Main routine for tar.	 |
`-----------------------*/

int
main (int argc, char *const *argv)
{
  program_name = argv[0];
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  exit_status = TAREXIT_SUCCESS;
  filename_terminator = '\n';

  /* Pre-allocate a few structures.  */

  allocated_archive_names = 10;
  archive_name_array = (const char **)
    xmalloc (sizeof (const char *) * allocated_archive_names);
  archive_names = 0;

#ifdef SIGCHLD
  /* System V fork+wait does not work if SIGCHLD is ignored.  */
  signal (SIGCHLD, SIG_DFL);
#endif

  init_names ();

  /* Decode options.  */

  decode_options (argc, argv);
  name_init (argc, argv);

  /* Main command execution.  */

  if (volno_file_option)
    init_volume_number ();

  switch (subcommand_option)
    {
    case UNKNOWN_SUBCOMMAND:
      USAGE_ERROR ((0, 0,
		    _("You must specify one of the `-Acdtrux' options")));

    case CAT_SUBCOMMAND:
    case UPDATE_SUBCOMMAND:
    case APPEND_SUBCOMMAND:
      update_archive ();
      break;

    case DELETE_SUBCOMMAND:
      delete_archive_members ();
      break;

    case CREATE_SUBCOMMAND:
      if (totals_option)
	init_total_written ();

      create_archive ();
      name_close ();

      if (totals_option)
	print_total_written ();
      break;

    case EXTRACT_SUBCOMMAND:
      extr_init ();
      read_and (extract_archive);
      break;

    case LIST_SUBCOMMAND:
      read_and (list_archive);
      break;

    case DIFF_SUBCOMMAND:
      diff_init ();
      read_and (diff_archive);
      break;
    }

  if (volno_file_option)
    closeout_volume_number ();

  /* Dispose of allocated memory, and return.  */

  free (archive_name_array);
  name_term ();

  if (exit_status == TAREXIT_FAILURE)
    error (0, 0, _("Error exit delayed from previous errors"));
  exit (exit_status);
}
