/* A tar (tape archiver) program.

   Copyright (C) 1988, 1992, 1993, 1994, 1995, 1996, 1997, 1999, 2000,
   2001, 2003, 2004 Free Software Foundation, Inc.

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

#include <system.h>

#include <fnmatch.h>
#include <argp.h>

#include <signal.h>
#if ! defined SIGCHLD && defined SIGCLD
# define SIGCHLD SIGCLD
#endif

/* The following causes "common.h" to produce definitions of all the global
   variables, rather than just "extern" declarations of them.  GNU tar does
   depend on the system loader to preset all GLOBAL variables to neutral (or
   zero) values; explicit initialization is usually not done.  */
#define GLOBAL
#include "common.h"

#include <getdate.h>
#include <localedir.h>
#include <rmt.h>
#include <prepargs.h>
#include <quotearg.h>
#include <xstrtol.h>

/* Local declarations.  */

#ifndef DEFAULT_ARCHIVE_FORMAT
# define DEFAULT_ARCHIVE_FORMAT GNU_FORMAT
#endif

#ifndef DEFAULT_ARCHIVE
# define DEFAULT_ARCHIVE "tar.out"
#endif

#ifndef DEFAULT_BLOCKING
# define DEFAULT_BLOCKING 20
#endif


/* Miscellaneous.  */

/* Name of option using stdin.  */
static const char *stdin_used_by;

/* Doesn't return if stdin already requested.  */
void
request_stdin (const char *option)
{
  if (stdin_used_by)
    USAGE_ERROR ((0, 0, _("Options `-%s' and `-%s' both want standard input"),
		  stdin_used_by, option));

  stdin_used_by = option;
}

/* Returns true if and only if the user typed 'y' or 'Y'.  */
int
confirm (const char *message_action, const char *message_name)
{
  static FILE *confirm_file;
  static int confirm_file_EOF;

  if (!confirm_file)
    {
      if (archive == 0 || stdin_used_by)
	{
	  confirm_file = fopen (TTY_NAME, "r");
	  if (! confirm_file)
	    open_fatal (TTY_NAME);
	}
      else
	{
	  request_stdin ("-w");
	  confirm_file = stdin;
	}
    }

  fprintf (stdlis, "%s %s?", message_action, quote (message_name));
  fflush (stdlis);

  {
    int reply = confirm_file_EOF ? EOF : getc (confirm_file);
    int character;

    for (character = reply;
	 character != '\n';
	 character = getc (confirm_file))
      if (character == EOF)
	{
	  confirm_file_EOF = 1;
	  fputc ('\n', stdlis);
	  fflush (stdlis);
	  break;
	}
    return reply == 'y' || reply == 'Y';
  }
}

static struct fmttab {
  char const *name;
  enum archive_format fmt;
} const fmttab[] = {
  { "v7",      V7_FORMAT },
  { "oldgnu",  OLDGNU_FORMAT },
  { "ustar",   USTAR_FORMAT },
  { "posix",   POSIX_FORMAT },
#if 0 /* not fully supported yet */
  { "star",    STAR_FORMAT },
#endif
  { "gnu",     GNU_FORMAT },
  { "pax",     POSIX_FORMAT }, /* An alias for posix */
  { NULL,	 0 }
};

static void
set_archive_format (char const *name)
{
  struct fmttab const *p;

  for (p = fmttab; strcmp (p->name, name) != 0; )
    if (! (++p)->name)
      USAGE_ERROR ((0, 0, _("%s: Invalid archive format"),
		    quotearg_colon (name)));

  archive_format = p->fmt;
}

static const char *
archive_format_string (enum archive_format fmt)
{
  struct fmttab const *p;

  for (p = fmttab; p->name; p++)
    if (p->fmt == fmt)
      return p->name;
  return "unknown?";
}

#define FORMAT_MASK(n) (1<<(n))

static void
assert_format(unsigned fmt_mask)
{
  if ((FORMAT_MASK(archive_format) & fmt_mask) == 0)
    USAGE_ERROR ((0, 0,
		  _("GNU features wanted on incompatible archive format")));
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
  ANCHORED_OPTION = CHAR_MAX + 1,
  ALLOW_NAME_MANGLING_OPTION,
  ATIME_PRESERVE_OPTION,
  BACKUP_OPTION,
  CHECKPOINT_OPTION,
  CHECK_LINKS_OPTION,
  DELETE_OPTION,
  EXCLUDE_OPTION,
  EXCLUDE_CACHES_OPTION,
  FORCE_LOCAL_OPTION,
  GROUP_OPTION,
  IGNORE_CASE_OPTION,
  IGNORE_FAILED_READ_OPTION,
  INDEX_FILE_OPTION,
  KEEP_NEWER_FILES_OPTION,
  LICENSE_OPTION,
  MODE_OPTION,
  NEWER_MTIME_OPTION,
  NO_ANCHORED_OPTION,
  NO_IGNORE_CASE_OPTION,
  NO_OVERWRITE_DIR_OPTION,
  NO_RECURSION_OPTION,
  NO_SAME_OWNER_OPTION,
  NO_SAME_PERMISSIONS_OPTION,
  NO_WILDCARDS_OPTION,
  NO_WILDCARDS_MATCH_SLASH_OPTION,
  NULL_OPTION,
  NUMERIC_OWNER_OPTION,
  OCCURRENCE_OPTION,
  OLD_ARCHIVE_OPTION,
  ONE_FILE_SYSTEM_OPTION,
  OVERWRITE_OPTION,
  OWNER_OPTION,
  PAX_OPTION,
  POSIX_OPTION,
  PRESERVE_OPTION,
  RECORD_SIZE_OPTION,
  RECURSION_OPTION,
  RECURSIVE_UNLINK_OPTION,
  REMOVE_FILES_OPTION,
  RMT_COMMAND_OPTION,
  RSH_COMMAND_OPTION,
  SAME_OWNER_OPTION,
  SHOW_DEFAULTS_OPTION,
  SHOW_OMITTED_DIRS_OPTION,
  STRIP_COMPONENTS_OPTION,
  SUFFIX_OPTION,
  TOTALS_OPTION,
  USAGE_OPTION,
  USE_COMPRESS_PROGRAM_OPTION,
  UTC_OPTION,
  VERSION_OPTION,
  VOLNO_FILE_OPTION,
  WILDCARDS_OPTION,
  WILDCARDS_MATCH_SLASH_OPTION
};

const char *argp_program_version = "tar (" PACKAGE_NAME ") " VERSION;
const char *argp_program_bug_address = "<" PACKAGE_BUGREPORT ">";
static char doc[] = N_("GNU `tar' saves many files together into a single tape or disk archive, and can restore individual files from the archive.\n\
\n\
Examples:\n\
  tar -cf archive.tar foo bar  # Create archive.tar from files foo and bar.\n\
  tar -tvf archive.tar         # List all files in archive.tar verbosely.\n\
  tar -xf archive.tar          # Extract all files from archive.tar.\n\
\vThe backup suffix is `~', unless set with --suffix or SIMPLE_BACKUP_SUFFIX.\n\
The version control may be set with --backup or VERSION_CONTROL, values are:\n\n\
  t, numbered     make numbered backups\n\
  nil, existing   numbered if numbered backups exist, simple otherwise\n\
  never, simple   always make simple backups\n");


/* NOTE:

   Available option letters are DEIJQY and aeqy. Consider the following
   assignments:

   [For Solaris tar compatibility]
   e  exit immediately with a nonzero exit status if unexpected errors occur
   E  use extended headers (--format=posix)
   [q  alias for --occurrence=1 =/= this would better be used for quiet?]
   [I  same as T =/= will harm star compatibility]
   
   y  per-file gzip compression
   Y  per-block gzip compression */

static struct argp_option options[] = {
  {NULL, 0, NULL, 0,
   N_("Main operation mode:"), 0},
  
  {"list", 't', 0, 0,
   N_("list the contents of an archive"), 10 },
  {"extract", 'x', 0, 0,
   N_("extract files from an archive"), 10 },
  {"get", 0, 0, OPTION_ALIAS, NULL, 0 },
  {"create", 'c', 0, 0,
   N_("create a new archive"), 10 },
  {"diff", 'd', 0, 0,
   N_("find differences between archive and file system"), 10 },
  {"compare", 0, 0, OPTION_ALIAS, NULL, 10},
  {"append", 'r', 0, 0,
   N_("append files to the end of an archive"), 10 },
  {"update", 'u', 0, 0,
   N_("only append files newer than copy in archive"), 10 },
  {"catenate", 'A', 0, 0,
   N_("append tar files to an archive"), 10 },
  {"concatenate", 0, 0, OPTION_ALIAS, NULL, 10},
  {"delete", DELETE_OPTION, 0, 0,
   N_("delete from the archive (not on mag tapes!)"), 10 },

  {NULL, 0, NULL, 0,
   N_("Operation modifiers:"), 20},

  {"verify", 'W', 0, 0,
   N_("attempt to verify the archive after writing it"), 21 },
  {"remove-files", REMOVE_FILES_OPTION, 0, 0,
   N_("remove files after adding them to the archive"), 21 },
  {"keep-old-files", 'k', 0, 0,
   N_("don't replace existing files when extracting"), 21 },
  {"keep-newer-files", KEEP_NEWER_FILES_OPTION, 0, 0,
   N_("don't replace existing files that are newer than their archive copies"), 21 },
  {"no-overwrite-dir", NO_OVERWRITE_DIR_OPTION, 0, 0,
   N_("preserve metadata of existing directories"), 21 },
  {"overwrite", OVERWRITE_OPTION, 0, 0,
   N_("overwrite existing files when extracting"), 21 },
  {"unlink-first", 'U', 0, 0,
   N_("remove each file prior to extracting over it"), 21 },
  {"recursive-unlink", RECURSIVE_UNLINK_OPTION, 0, 0,
   N_("empty hierarchies prior to extracting directory"), 21 },
  {"sparse", 'S', 0, 0,
   N_("handle sparse files efficiently"), 21 },
  {"to-stdout", 'O', 0, 0,
   N_("extract files to standard output"), 21 },
  {"incremental", 'G', 0, 0,
   N_("handle old GNU-format incremental backup"), 21 },
  {"listed-incremental", 'g', N_("FILE"), 0,
   N_("handle new GNU-format incremental backup"), 21 },
  {"ignore-failed-read", IGNORE_FAILED_READ_OPTION, 0, 0,
   N_("do not exit with nonzero on unreadable files"), 21 },
  {"occurrence", OCCURRENCE_OPTION, N_("NUMBER"), OPTION_ARG_OPTIONAL,
   N_("process only the NUMth occurrence of each file in the archive. This option is valid only in conjunction with one of the subcommands --delete, --diff, --extract or --list and when a list of files is given either on the command line or via -T option. NUMBER defaults to 1."), 21 },
  {"seek", 'n', NULL, 0,
   N_("Archive is seekable"), 21 },
    
  {NULL, 0, NULL, 0,
   N_("Handling of file attributes:"), 30 },

  {"owner", OWNER_OPTION, N_("NAME"), 0,
   N_("force NAME as owner for added files"), 31 },
  {"group", GROUP_OPTION, N_("NAME"), 0,
   N_("force NAME as group for added files"), 31 },
  {"mode", MODE_OPTION, N_("CHANGES"), 0,
   N_("force (symbolic) mode CHANGES for added files"), 31 },
  {"atime-preserve", ATIME_PRESERVE_OPTION, 0, 0,
   N_("don't change access times on dumped files"), 31 },
  {"touch", 'm', 0, 0,
   N_("don't extract file modified time"), 31 },
  {"same-owner", SAME_OWNER_OPTION, 0, 0,
   N_("try extracting files with the same ownership"), 31 },
  {"no-same-owner", NO_SAME_OWNER_OPTION, 0, 0,
   N_("extract files as yourself"), 31 },
  {"numeric-owner", NUMERIC_OWNER_OPTION, 0, 0,
   N_("always use numbers for user/group names"), 31 },
  {"preserve-permissions", 'p', 0, 0,
   N_("extract permissions information"), 31 },
  {"same-permissions", 0, 0, OPTION_ALIAS, NULL, 31 },
  {"no-same-permissions", NO_SAME_PERMISSIONS_OPTION, 0, 0,
   N_("do not extract permissions information"), 31 },
  {"preserve-order", 's', 0, 0,
   N_("sort names to extract to match archive"), 31 },
  {"same-order", 0, 0, OPTION_ALIAS, NULL, 31 },
  {"preserve", PRESERVE_OPTION, 0, 0,
   N_("same as both -p and -s"), 31 },

  {NULL, 0, NULL, 0,
   N_("Device selection and switching:"), 40 },
  
  {"file", 'f', N_("ARCHIVE"), 0,
   N_("use archive file or device ARCHIVE"), 41 },
  {"force-local", FORCE_LOCAL_OPTION, 0, 0,
   N_("archive file is local even if has a colon"), 41 },
  {"rmt-command", RMT_COMMAND_OPTION, N_("COMMAND"), 0,
   N_("use given rmt COMMAND instead of rmt"), 41 }, 
  {"rsh-command", RSH_COMMAND_OPTION, N_("COMMAND"), 0,
   N_("use remote COMMAND instead of rsh"), 41 },
#ifdef DEVICE_PREFIX
  {"-[0-7][lmh]", 0, NULL, OPTION_DOC, /* It is OK, since `name' will never be
					  translated */
   N_("specify drive and density"), 41 },
#endif  
  {NULL, '0', NULL, OPTION_HIDDEN, NULL, 41 },
  {NULL, '1', NULL, OPTION_HIDDEN, NULL, 41 },
  {NULL, '2', NULL, OPTION_HIDDEN, NULL, 41 },
  {NULL, '3', NULL, OPTION_HIDDEN, NULL, 41 },
  {NULL, '4', NULL, OPTION_HIDDEN, NULL, 41 },
  {NULL, '5', NULL, OPTION_HIDDEN, NULL, 41 },
  {NULL, '6', NULL, OPTION_HIDDEN, NULL, 41 },
  {NULL, '7', NULL, OPTION_HIDDEN, NULL, 41 },
  {NULL, '8', NULL, OPTION_HIDDEN, NULL, 41 },
  {NULL, '9', NULL, OPTION_HIDDEN, NULL, 41 },
  
  {"multi-volume", 'M', 0, 0,
   N_("create/list/extract multi-volume archive"), 41 },
  {"tape-length", 'L', N_("NUMBER"), 0,
   N_("change tape after writing NUMBER x 1024 bytes"), 41 },
  {"info-script", 'F', N_("NAME"), 0,
   N_("run script at end of each tape (implies -M)"), 41 },
  {"new-volume-script", 0, 0, OPTION_ALIAS, NULL, 41 },
  {"volno-file", VOLNO_FILE_OPTION, N_("FILE"), 0,
   N_("use/update the volume number in FILE"), 41 },

  {NULL, 0, NULL, 0,
   N_("Device blocking:"), 50 },

  {"blocking-factor", 'b', N_("BLOCKS"), 0,
   N_("BLOCKS x 512 bytes per record"), 51 },
  {"record-size", RECORD_SIZE_OPTION, N_("NUMBER"), 0,
   N_("SIZE bytes per record, multiple of 512"), 51 },
  {"ignore-zeros", 'i', 0, 0,
   N_("ignore zeroed blocks in archive (means EOF)"), 51 },
  {"read-full-records", 'B', 0, 0,
   N_("reblock as we read (for 4.2BSD pipes)"), 51 }, 

  {NULL, 0, NULL, 0,
   N_("Archive format selection:"), 60 },
  
  {"format", 'H', N_("FORMAT"), 0,
   N_("create archive of the given format."), 61 },

  {NULL, 0, NULL, 0, N_("FORMAT is one of the following:"), 62 },
  {"  v7", 0, NULL, OPTION_DOC|OPTION_NO_TRANS, N_("old V7 tar format"), 63},
  {"  oldgnu", 0, NULL, OPTION_DOC|OPTION_NO_TRANS,
   N_("GNU format as per tar <= 1.12"), 63},
  {"  gnu", 0, NULL, OPTION_DOC|OPTION_NO_TRANS,
   N_("GNU tar 1.13.x format"), 63},
  {"  ustar", 0, NULL, OPTION_DOC|OPTION_NO_TRANS,
   N_("POSIX 1003.1-1988 (ustar) format"), 63 },
  {"  pax", 0, NULL, OPTION_DOC|OPTION_NO_TRANS,
   N_("POSIX 1003.1-2001 (pax) format"), 63 },
  {"  posix", 0, NULL, OPTION_DOC|OPTION_NO_TRANS, N_("Same as pax"), 63 },
  
  {"old-archive", OLD_ARCHIVE_OPTION, 0, 0, /* FIXME */
   N_("same as --format=v7"), 68 },
  {"portability", 0, 0, OPTION_ALIAS, NULL, 68 },
  {"posix", POSIX_OPTION, 0, 0,
   N_("same as --format=posix"), 68 },
  {"pax-option", PAX_OPTION, N_("keyword[[:]=value][,keyword[[:]=value], ...]"), 0,
   N_("control pax keywords"), 68 },
  {"label", 'V', N_("TEXT"), 0,
   N_("create archive with volume name NAME. At list/extract time, use TEXT as a globbing pattern"), 68 },
  {"bzip2", 'j', 0, 0,
   N_("filter the archive through bzip2"), 68 },
  {"gzip", 'z', 0, 0,
   N_("filter the archive through gzip"), 68 },
  {"gunzip", 0, 0, OPTION_ALIAS, NULL, 68 },
  {"ungzip", 0, 0, OPTION_ALIAS, NULL, 68 },
  {"compress", 'Z', 0, 0,
   N_("filter the archive through compress"), 68 },
  {"uncompress", 0, 0, OPTION_ALIAS, NULL, 68 },
  {"use-compress-program", USE_COMPRESS_PROGRAM_OPTION, N_("PROG"), 0,
   N_("filter through PROG (must accept -d)"), 68 },

  {NULL, 0, NULL, 0,
   N_("Local file selection:"), 70 },

  {"directory", 'C', N_("DIR"), 0,
   N_("change to directory DIR"), 71 },
  {"files-from", 'T', N_("FILE-OF-NAMES"), 0,
   N_("get names to extract or create from file NAME"), 71 },
  {"null", NULL_OPTION, 0, 0,
   N_("-T reads null-terminated names, disable -C"), 71 },
  {"exclude", EXCLUDE_OPTION, N_("PATTERN"), 0,
   N_("exclude files, given as a PATTERN"), 71 },
  {"exclude-from", 'X', N_("FILE"), 0,
   N_("exclude patterns listed in FILE"), 71 },
  {"exclude-caches", EXCLUDE_CACHES_OPTION, 0, 0,
   N_("exclude directories containing a cache tag"), 71 },
  {"ignore-case", IGNORE_CASE_OPTION, 0, 0,
   N_("exclusion ignores case"), 71 },
  {"anchored", ANCHORED_OPTION, 0, 0,
   N_("exclude patterns match file name start"), 71 },
  {"no-anchored", NO_ANCHORED_OPTION, 0, 0,
   N_("exclude patterns match after any / (default)"), 71 },
  {"no-ignore-case", NO_IGNORE_CASE_OPTION, 0, 0,
   N_("exclusion is case sensitive (default)"), 71 },
  {"no-wildcards", NO_WILDCARDS_OPTION, 0, 0,
   N_("exclude patterns are plain strings"), 71 },
  {"no-wildcards-match-slash", NO_WILDCARDS_MATCH_SLASH_OPTION, 0, 0,
   N_("exclude pattern wildcards do not match '/'"), 71 },
  {"no-recursion", NO_RECURSION_OPTION, 0, 0,
   N_("avoid descending automatically in directories"), 71 },
  {"one-file-system", ONE_FILE_SYSTEM_OPTION, 0, 0,
   N_("stay in local file system when creating archive"), 71 },
  {NULL, 'l', 0, OPTION_HIDDEN, "", 71},
  {"recursion", RECURSION_OPTION, 0, 0,
   N_("recurse into directories (default)"), 71 },
  {"absolute-names", 'P', 0, 0,
   N_("don't strip leading `/'s from file names"), 71 },
  {"dereference", 'h', 0, 0,
   N_("dump instead the files symlinks point to"), 71 },
  {"starting-file", 'K', N_("MEMBER-NAME"), 0,
   N_("begin at member MEMBER-NAME in the archive"), 71 },
  {"strip-components", STRIP_COMPONENTS_OPTION, N_("NUMBER"), 0,
   N_("strip NUMBER leading components from file names"), 71 },
  {"newer", 'N', N_("DATE-OR-FILE"), 0,
   N_("only store files newer than DATE-OR-FILE"), 71 },
  {"newer-mtime", NEWER_MTIME_OPTION, N_("DATE"), 0,
   N_("compare date and time when data changed only"), 71 },
  {"after-date", 'N', N_("DATE"), 0,
   N_("same as -N"), 71 },
  {"backup", BACKUP_OPTION, N_("CONTROL"), OPTION_ARG_OPTIONAL,
   N_("backup before removal, choose version CONTROL"), 71 },
  {"suffix", SUFFIX_OPTION, N_("STRING"), 0,
   N_("backup before removal, override usual suffix ('~' unless overridden by environment variable SIMPLE_BACKUP_SUFFIX"), 71 },
  {"wildcards", WILDCARDS_OPTION, 0, 0,
   N_("exclude patterns use wildcards (default)"), 71 },
  {"wildcards-match-slash", WILDCARDS_MATCH_SLASH_OPTION, 0, 0,
   N_("exclude pattern wildcards match '/' (default)"), 71 },

  {NULL, 0, NULL, 0,
   N_("Informative output:"), 80 },
  
  {"verbose", 'v', 0, 0,
   N_("verbosely list files processed"), 81 },
  {"checkpoint", CHECKPOINT_OPTION, 0, 0,
   N_("display progress messages every 10th record"), 81 },
  {"check-links", CHECK_LINKS_OPTION, 0, 0,
   N_("print a message if not all links are dumped"), 82 },
  {"totals", TOTALS_OPTION, 0, 0,
   N_("print total bytes written while creating archive"), 82 },
  {"utc", UTC_OPTION, 0, 0,
   N_("print file modification dates in UTC"), 82 },
  {"index-file", INDEX_FILE_OPTION, N_("FILE"), 0,
   N_("send verbose output to FILE"), 82 },
  {"block-number", 'R', 0, 0,
   N_("show block number within archive with each message"), 82 },
  {"interactive", 'w', 0, 0,
   N_("ask for confirmation for every action"), 82 },
  {"confirmation", 0, 0, OPTION_ALIAS, NULL, 82 },
  {"show-defaults", SHOW_DEFAULTS_OPTION, 0, 0,
   N_("Show tar defaults"), 82 },
  {"show-omitted-dirs", SHOW_OMITTED_DIRS_OPTION, 0, 0,
   N_("When listing or extracting, list each directory that does not match search criteria"), 82 },
  
  {NULL, 0, NULL, 0,
   N_("Compatibility options:"), 90 },

  {NULL, 'o', 0, 0,
   N_("when creating, same as --old-archive. When extracting, same as --no-same-owner"), 91 },
  {"allow-name-mangling", ALLOW_NAME_MANGLING_OPTION, 0, 0,
   N_("when creating, allow GNUTYPE_NAMES mangling -- considered dangerous"), 91 },

  {NULL, 0, NULL, 0,
   N_("Other options:"), 100 },

  {"help",  '?', 0, 0,  N_("Give this help list"), -1},
  {"usage", USAGE_OPTION, 0, 0,  N_("Give a short usage message"), -1},
  {"license", LICENSE_OPTION, 0, 0, N_("Print license and exit"), -1},
  {"version", VERSION_OPTION, 0, 0,  N_("Print program version"), -1},
  /* FIXME -V (--label) conflicts with the default short option for
     --version */
  
  {0, 0, 0, 0, 0, 0}
};

struct tar_args {
  char const *textual_date_option;
  int exclude_options;
  bool o_option;
  int pax_option;
  char const *backup_suffix_string;
  char const *version_control_string;
  int input_files;
};

static void
show_default_settings (FILE *stream)
{
  fprintf (stream,
	   "--format=%s -f%s -b%d --rmt-command=%s",
	   archive_format_string (DEFAULT_ARCHIVE_FORMAT),
	   DEFAULT_ARCHIVE, DEFAULT_BLOCKING,
	   DEFAULT_RMT_COMMAND);
#ifdef REMOTE_SHELL
  fprintf (stream, " --rsh-command=%s", REMOTE_SHELL);
#endif
  fprintf (stream, "\n");
}

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
  if (use_compress_program_option
      && strcmp (use_compress_program_option, string) != 0)
    USAGE_ERROR ((0, 0, _("Conflicting compression options")));

  use_compress_program_option = string;
}

void
license ()
{
  printf ("tar (%s) %s\n%s\n", PACKAGE_NAME, PACKAGE_VERSION,
	  "Copyright (C) 2004 Free Software Foundation, Inc.\n");
  puts (_("Modified to support extended attributes.\n"));
  puts (_("Based on the work of John Gilmore and Jay Fenlason. See AUTHORS\n\
for complete list of authors.\n"));
  printf (_("   GNU tar is free software; you can redistribute it and/or modify\n"
    "   it under the terms of the GNU General Public License as published by\n"
    "   the Free Software Foundation; either version 2 of the License, or\n"
    "   (at your option) any later version.\n"
    "\n"
    "   GNU tar is distributed in the hope that it will be useful,\n"
    "   but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
    "   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
    "   GNU General Public License for more details.\n"
    "\n"
    "   You should have received a copy of the GNU General Public License\n"
    "   along with GNU tar; if not, write to the Free Software\n"
    "   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA\n\n"));
  exit (0);
}

static error_t
parse_opt(int key, char *arg, struct argp_state *state)
{
  struct tar_args *args = state->input;
  
  switch (key)
    {
      case 1:
	/* File name or non-parsed option, because of ARGP_IN_ORDER */
	name_add (optarg);
	args->input_files++;
	break;

    case 'A':
      set_subcommand_option (CAT_SUBCOMMAND);
      break;
      
    case 'b':
      {
	uintmax_t u;
	if (! (xstrtoumax (arg, 0, 10, &u, "") == LONGINT_OK
	       && u == (blocking_factor = u)
	       && 0 < blocking_factor
	       && u == (record_size = u * BLOCKSIZE) / BLOCKSIZE))
	  USAGE_ERROR ((0, 0, "%s: %s", quotearg_colon (arg),
			_("Invalid blocking factor")));
      }
      break;

    case 'B':
      /* Try to reblock input records.  For reading 4.2BSD pipes.  */
      
      /* It would surely make sense to exchange -B and -R, but it seems
	 that -B has been used for a long while in Sun tar and most
	 BSD-derived systems.  This is a consequence of the block/record
	 terminology confusion.  */
      
      read_full_records_option = true;
      break;

    case 'c':
      set_subcommand_option (CREATE_SUBCOMMAND);
      break;

    case 'C':
      name_add ("-C");
      name_add (arg);
      break;

    case 'd':
      set_subcommand_option (DIFF_SUBCOMMAND);
      break;

    case 'f':
      if (archive_names == allocated_archive_names)
	{
	  allocated_archive_names *= 2;
	  archive_name_array =
	    xrealloc (archive_name_array,
		      sizeof (const char *) * allocated_archive_names);
	}
      archive_name_array[archive_names++] = arg;
      break;

    case 'F':
      /* Since -F is only useful with -M, make it implied.  Run this
	 script at the end of each tape.  */
      
      info_script_option = arg;
      multi_volume_option = true;
      break;
      
    case 'g':
      listed_incremental_option = arg;
      after_date_option = true;
      /* Fall through.  */
      
    case 'G':
      /* We are making an incremental dump (FIXME: are we?); save
	 directories at the beginning of the archive, and include in each
	 directory its contents.  */
      
      incremental_option = true;
      break;
      
    case 'h':
      /* Follow symbolic links.  */
      dereference_option = true;
      break;
      
    case 'i':
      /* Ignore zero blocks (eofs).  This can't be the default,
	 because Unix tar writes two blocks of zeros, then pads out
	 the record with garbage.  */
      
      ignore_zeros_option = true;
      break;
      
    case 'I':
      USAGE_ERROR ((0, 0,
		    _("Warning: the -I option is not supported;"
		      " perhaps you meant -j or -T?")));
      break;
      
    case 'j':
      set_use_compress_program_option ("bzip2");
      break;
      
    case 'k':
      /* Don't replace existing files.  */
      old_files_option = KEEP_OLD_FILES;
      break;
      
    case 'K':
      starting_file_option = true;
      addname (arg, 0);
      break;
      
    case 'l':
      /* Historically equivalent to --one-file-system. This usage is
	 incompatible with UNIX98 and POSIX specs and therefore is
	 deprecated. The semantics of -l option will be changed in
	 future versions. See TODO.
      */
      WARN ((0, 0,
	     _("Semantics of -l option will change in the future releases.")));
      WARN ((0, 0,
	     _("Please use --one-file-system option instead.")));
      /* FALL THROUGH */
    case ONE_FILE_SYSTEM_OPTION:
      /* When dumping directories, don't dump files/subdirectories
	 that are on other filesystems. */
      one_file_system_option = true;
      break;
      
    case 'L':
      {
	uintmax_t u;
	if (xstrtoumax (arg, 0, 10, &u, "") != LONGINT_OK)
	  USAGE_ERROR ((0, 0, "%s: %s", quotearg_colon (arg),
			_("Invalid tape length")));
	tape_length_option = 1024 * (tarlong) u;
	multi_volume_option = true;
      }
      break;
      
    case 'm':
      touch_option = true;
      break;
      
    case 'M':
      /* Make multivolume archive: when we can't write any more into
	 the archive, re-open it, and continue writing.  */
      
      multi_volume_option = true;
      break;

    case 'n':
      seekable_archive = true;
      break;
      
#if !MSDOS
    case 'N':
      after_date_option = true;
      /* Fall through.  */

    case NEWER_MTIME_OPTION:
      if (NEWER_OPTION_INITIALIZED (newer_mtime_option))
	USAGE_ERROR ((0, 0, _("More than one threshold date")));
      
      if (FILE_SYSTEM_PREFIX_LEN (arg) != 0
	  || ISSLASH (*arg)
	  || *arg == '.')
	{
	  struct stat st;
	  if (deref_stat (dereference_option, arg, &st) != 0)
	    {
	      stat_error (arg);
	      USAGE_ERROR ((0, 0, _("Date file not found")));
	    }
	  newer_mtime_option.tv_sec = st.st_mtime;
	  newer_mtime_option.tv_nsec = TIMESPEC_NS (st.st_mtim);
	}
      else
	{
	  if (! get_date (&newer_mtime_option, arg, NULL))
	    {
	      WARN ((0, 0, _("Substituting %s for unknown date format %s"),
		     tartime (newer_mtime_option.tv_sec), quote (arg)));
	      newer_mtime_option.tv_nsec = 0;
	    }
	  else
	    args->textual_date_option = arg;
	}
      
      break;
#endif /* not MSDOS */
      
    case 'o':
      args->o_option = true;
      break;
      
    case 'O':
      to_stdout_option = true;
      break;

    case 'p':
      same_permissions_option = true;
      break;
      
    case 'P':
      absolute_names_option = true;
      break;
      
    case 'r':
      set_subcommand_option (APPEND_SUBCOMMAND);
      break;
      
    case 'R':
      /* Print block numbers for debugging bad tar archives.  */
      
      /* It would surely make sense to exchange -B and -R, but it seems
	 that -B has been used for a long while in Sun tar ans most
	 BSD-derived systems.  This is a consequence of the block/record
	 terminology confusion.  */
      
      block_number_option = true;
      break;
      
    case 's':
      /* Names to extr are sorted.  */
      
      same_order_option = true;
      break;
      
    case 'S':
      sparse_option = true;
      break;
      
    case 't':
      set_subcommand_option (LIST_SUBCOMMAND);
      verbose_option++;
      break;

    case 'T':
      files_from_option = arg;
      break;
      
    case 'u':
      set_subcommand_option (UPDATE_SUBCOMMAND);
      break;
      
    case 'U':
      old_files_option = UNLINK_FIRST_OLD_FILES;
      break;
      
    case UTC_OPTION:
      utc_option = true;
      break;
      
    case 'v':
      verbose_option++;
      break;
      
    case 'V':
      volume_label_option = arg;
      break;
      
    case 'w':
      interactive_option = true;
      break;
      
    case 'W':
      verify_option = true;
      break;
      
    case 'x':
      set_subcommand_option (EXTRACT_SUBCOMMAND);
      break;
      
    case 'X':
      if (add_exclude_file (add_exclude, excluded, arg,
			    args->exclude_options | recursion_option, '\n')
	  != 0)
	{
	  int e = errno;
	  FATAL_ERROR ((0, e, "%s", quotearg_colon (arg)));
	}
      break;
      
    case 'y':
      USAGE_ERROR ((0, 0,
		    _("Warning: the -y option is not supported;"
		      " perhaps you meant -j?")));
      break;
      
    case 'z':
      set_use_compress_program_option ("gzip");
      break;
      
    case 'Z':
      set_use_compress_program_option ("compress");
      break;

    case ALLOW_NAME_MANGLING_OPTION:
      allow_name_mangling_option = true;
      break;
      
    case ANCHORED_OPTION:
      args->exclude_options |= EXCLUDE_ANCHORED;
      break;
      
    case ATIME_PRESERVE_OPTION:
      atime_preserve_option = true;
      break;
      
    case CHECKPOINT_OPTION:
      checkpoint_option = true;
      break;
      
    case BACKUP_OPTION:
      backup_option = true;
      if (arg)
	args->version_control_string = arg;
      break;
      
    case DELETE_OPTION:
      set_subcommand_option (DELETE_SUBCOMMAND);
      break;
      
    case EXCLUDE_OPTION:
      add_exclude (excluded, arg, args->exclude_options | recursion_option);
      break;
      
    case EXCLUDE_CACHES_OPTION:
      exclude_caches_option = true;
      break;

    case FORCE_LOCAL_OPTION:
      force_local_option = true;
      break;
      
    case 'H':
      set_archive_format (arg);
      break;
      
    case INDEX_FILE_OPTION:
      index_file_name = arg;
      break;
      
    case IGNORE_CASE_OPTION:
      args->exclude_options |= FNM_CASEFOLD;
      break;
      
    case IGNORE_FAILED_READ_OPTION:
      ignore_failed_read_option = true;
      break;
      
    case KEEP_NEWER_FILES_OPTION:
      old_files_option = KEEP_NEWER_FILES;
      break;
      
    case GROUP_OPTION:
      if (! (strlen (arg) < GNAME_FIELD_SIZE
	     && gname_to_gid (arg, &group_option)))
	{
	  uintmax_t g;
	  if (xstrtoumax (arg, 0, 10, &g, "") == LONGINT_OK
	      && g == (gid_t) g)
	    group_option = g;
	  else
	    FATAL_ERROR ((0, 0, "%s: %s", quotearg_colon (arg),
			  _("%s: Invalid group")));
	}
      break;
      
    case MODE_OPTION:
      mode_option
	= mode_compile (arg,
			MODE_MASK_EQUALS | MODE_MASK_PLUS | MODE_MASK_MINUS);
      if (mode_option == MODE_INVALID)
	FATAL_ERROR ((0, 0, _("Invalid mode given on option")));
      if (mode_option == MODE_MEMORY_EXHAUSTED)
	xalloc_die ();
      break;
      
    case NO_ANCHORED_OPTION:
      args->exclude_options &= ~ EXCLUDE_ANCHORED;
      break;
      
    case NO_IGNORE_CASE_OPTION:
      args->exclude_options &= ~ FNM_CASEFOLD;
      break;
      
    case NO_OVERWRITE_DIR_OPTION:
      old_files_option = NO_OVERWRITE_DIR_OLD_FILES;
      break;
      
    case NO_WILDCARDS_OPTION:
      args->exclude_options &= ~ EXCLUDE_WILDCARDS;
      break;
      
    case NO_WILDCARDS_MATCH_SLASH_OPTION:
      args->exclude_options |= FNM_FILE_NAME;
      break;
      
    case NULL_OPTION:
      filename_terminator = '\0';
      break;
      
    case NUMERIC_OWNER_OPTION:
      numeric_owner_option = true;
      break;
      
    case OCCURRENCE_OPTION:
      if (!arg)
	occurrence_option = 1;
      else
	{
	  uintmax_t u;
	  if (xstrtoumax (arg, 0, 10, &u, "") == LONGINT_OK)
	    occurrence_option = u;
	  else
	    FATAL_ERROR ((0, 0, "%s: %s", quotearg_colon (arg),
			  _("Invalid number")));
	}
      break;
      
    case OVERWRITE_OPTION:
      old_files_option = OVERWRITE_OLD_FILES;
      break;

    case OWNER_OPTION:
      if (! (strlen (arg) < UNAME_FIELD_SIZE
	     && uname_to_uid (arg, &owner_option)))
	{
	  uintmax_t u;
	  if (xstrtoumax (arg, 0, 10, &u, "") == LONGINT_OK
	      && u == (uid_t) u)
	    owner_option = u;
	  else
	    FATAL_ERROR ((0, 0, "%s: %s", quotearg_colon (arg),
			  _("Invalid owner")));
	}
      break;
      
    case PAX_OPTION:
      args->pax_option++;
      xheader_set_option (arg);
      break;
      
    case POSIX_OPTION:
      set_archive_format ("posix");
      break;
      
    case PRESERVE_OPTION:
      same_permissions_option = true;
      same_order_option = true;
      break;
      
    case RECORD_SIZE_OPTION:
      {
	uintmax_t u;
	if (! (xstrtoumax (arg, 0, 10, &u, "") == LONGINT_OK
	       && u == (size_t) u))
	  USAGE_ERROR ((0, 0, "%s: %s", quotearg_colon (arg),
			_("Invalid record size")));
	record_size = u;
	if (record_size % BLOCKSIZE != 0)
	  USAGE_ERROR ((0, 0, _("Record size must be a multiple of %d."),
			BLOCKSIZE));
	blocking_factor = record_size / BLOCKSIZE;
      }
      break;
      
    case RECURSIVE_UNLINK_OPTION:
      recursive_unlink_option = true;
      break;
      
    case REMOVE_FILES_OPTION:
      remove_files_option = true;
      break;
      
    case RMT_COMMAND_OPTION:
      rmt_command = arg;
      break;
      
    case RSH_COMMAND_OPTION:
      rsh_command_option = arg;
      break;
      
    case SHOW_DEFAULTS_OPTION:
      show_default_settings (stdout);
      exit(0);
      
    case STRIP_COMPONENTS_OPTION:
      {
	uintmax_t u;
	if (! (xstrtoumax (arg, 0, 10, &u, "") == LONGINT_OK
	       && u == (size_t) u))
	  USAGE_ERROR ((0, 0, "%s: %s", quotearg_colon (arg),
			_("Invalid number of elements")));
	strip_name_components = u;
      }
      break;

    case SHOW_OMITTED_DIRS_OPTION:
      show_omitted_dirs_option = true;
      break;
      
    case SUFFIX_OPTION:
      backup_option = true;
      args->backup_suffix_string = arg;
      break;
      
    case TOTALS_OPTION:
      totals_option = true;
      break;
      
    case USE_COMPRESS_PROGRAM_OPTION:
      set_use_compress_program_option (arg);
      break;
      
    case VOLNO_FILE_OPTION:
      volno_file_option = arg;
      break;
      
    case WILDCARDS_OPTION:
      args->exclude_options |= EXCLUDE_WILDCARDS;
      break;
      
    case WILDCARDS_MATCH_SLASH_OPTION:
      args->exclude_options &= ~ FNM_FILE_NAME;
      break;

    case CHECK_LINKS_OPTION:
      check_links_option = 1;
      break;
      
    case NO_RECURSION_OPTION:
      recursion_option = 0;
      break;

    case NO_SAME_OWNER_OPTION:
      same_owner_option = -1;
      break;

    case NO_SAME_PERMISSIONS_OPTION:
      same_permissions_option = -1;
      break;

    case RECURSION_OPTION:
      recursion_option = FNM_LEADING_DIR;
      break;

    case SAME_OWNER_OPTION:
      same_owner_option = 1;
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
	int device = key - '0';
	int density;
	static char buf[sizeof DEVICE_PREFIX + 10];
	char *cursor;

	if (arg[1])
	  argp_error (state, _("Malformed density argument: '%s'"), arg);
	
	strcpy (buf, DEVICE_PREFIX);
	cursor = buf + strlen (buf);

#ifdef DENSITY_LETTER

	sprintf (cursor, "%d%c", device, arg[0]);
	
#else /* not DENSITY_LETTER */

	switch (arg[0])
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
	    argp_error (state, _("Unknown density: '%c'"), arg[0]);
	  }
	sprintf (cursor, "%d", device);
	
#endif /* not DENSITY_LETTER */

	if (archive_names == allocated_archive_names)
	  {
	    allocated_archive_names *= 2;
	    archive_name_array =
	      xrealloc (archive_name_array,
			sizeof (const char *) * allocated_archive_names);
	  }
	archive_name_array[archive_names++] = strdup (buf);
      }
      break;

#else /* not DEVICE_PREFIX */

      argp_error (state, 
		  _("Options `-[0-7][lmh]' not supported by *this* tar"));
      
#endif /* not DEVICE_PREFIX */
      
    case '?':
      state->flags |= ARGP_NO_EXIT;
      argp_state_help (state, state->out_stream,
		       ARGP_HELP_STD_HELP & ~ARGP_HELP_BUG_ADDR);
      fprintf (state->out_stream, _("\n*This* tar defaults to:\n"));
      show_default_settings (state->out_stream);
      fprintf (state->out_stream, "\n");
      fprintf (state->out_stream, _("Report bugs to %s.\n"), 
	       argp_program_bug_address);
      exit (0);

    case USAGE_OPTION:
      argp_state_help (state, state->out_stream,
		       ARGP_HELP_USAGE | ARGP_HELP_EXIT_OK);
      break;

    case VERSION_OPTION:
      fprintf (state->out_stream, "%s\n", argp_program_version);
      exit (0);

    case LICENSE_OPTION:
      license ();
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

static struct argp argp = {
  options,
  parse_opt,
  N_("[FILE]..."),
  doc,
  NULL,
  NULL,
  NULL
};

void
usage (int status)
{
  argp_help (&argp, stderr, ARGP_HELP_SEE, (char*) program_name);
  exit (status);
}

/* Parse the options for tar.  */

static struct argp_option *
find_argp_option (struct argp_option *options, int letter)
{
  for (;
       !(options->name == NULL
	 && options->key == 0
	 && options->arg == 0
	 && options->flags == 0
	 && options->doc == NULL); options++)
    if (options->key == letter)
      return options;
  return NULL;
}

static void
decode_options (int argc, char **argv)
{
  int index;
  struct tar_args args;
  
  /* Set some default option values.  */
  args.textual_date_option = NULL;
  args.exclude_options = EXCLUDE_WILDCARDS;
  args.o_option = 0;
  args.pax_option = 0;
  args.backup_suffix_string = getenv ("SIMPLE_BACKUP_SUFFIX");
  args.version_control_string = 0;
  args.input_files = 0;

  subcommand_option = UNKNOWN_SUBCOMMAND;
  archive_format = DEFAULT_FORMAT;
  blocking_factor = DEFAULT_BLOCKING;
  record_size = DEFAULT_BLOCKING * BLOCKSIZE;
  excluded = new_exclude ();
  newer_mtime_option.tv_sec = TYPE_MINIMUM (time_t);
  newer_mtime_option.tv_nsec = -1;
  recursion_option = FNM_LEADING_DIR;

  owner_option = -1;
  group_option = -1;

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

      /* Initialize a constructed option.  */

      buffer[0] = '-';
      buffer[2] = '\0';

      /* Allocate a new argument array, and copy program name in it.  */

      new_argc = argc - 1 + strlen (argv[1]);
      new_argv = xmalloc ((new_argc + 1) * sizeof (char *));
      in = argv;
      out = new_argv;
      *out++ = *in++;

      /* Copy each old letter option as a separate option, and have the
	 corresponding argument moved next to it.  */

      for (letter = *in++; *letter; letter++)
	{
	  struct argp_option *opt;
	  
	  buffer[1] = *letter;
	  *out++ = xstrdup (buffer);
	  opt = find_argp_option (options, *letter);
	  if (opt && opt->arg)
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
      *out = 0;

      /* Replace the old option list by the new one.  */

      argc = new_argc;
      argv = new_argv;
    }

  /* Parse all options and non-options as they appear.  */

  prepend_default_options (getenv ("TAR_OPTIONS"), &argc, &argv);

  if (argp_parse (&argp, argc, argv, ARGP_IN_ORDER|ARGP_NO_HELP,
		  &index, &args))
    exit (1);
      

  /* Special handling for 'o' option:

     GNU tar used to say "output old format".
     UNIX98 tar says don't chown files after extracting (we use
     "--no-same-owner" for this).

     The old GNU tar semantics is retained when used with --create
     option, otherwise UNIX98 semantics is assumed */

  if (args.o_option)
    {
      if (subcommand_option == CREATE_SUBCOMMAND)
	{
	  /* GNU Tar <= 1.13 compatibility */
	  set_archive_format ("v7");
	}
      else
	{
	  /* UNIX98 compatibility */
	  same_owner_option = -1;
	}
    }

  /* Handle operands after any "--" argument.  */
  for (; index < argc; index++)
    {
      name_add (argv[index]);
      args.input_files++;
    }

  /* Derive option values and check option consistency.  */

  if (archive_format == DEFAULT_FORMAT)
    {
      if (args.pax_option)
	archive_format = POSIX_FORMAT;
      else
	archive_format = DEFAULT_ARCHIVE_FORMAT;
    }

  if (volume_label_option && subcommand_option == CREATE_SUBCOMMAND)
    assert_format (FORMAT_MASK (OLDGNU_FORMAT)
		   | FORMAT_MASK (GNU_FORMAT));


  if (incremental_option || multi_volume_option)
    assert_format (FORMAT_MASK (OLDGNU_FORMAT) | FORMAT_MASK (GNU_FORMAT));

  if (sparse_option)
    assert_format (FORMAT_MASK (OLDGNU_FORMAT)
		   | FORMAT_MASK (GNU_FORMAT)
		   | FORMAT_MASK (POSIX_FORMAT));

  if (occurrence_option)
    {
      if (!args.input_files && !files_from_option)
	USAGE_ERROR ((0, 0,
		      _("--occurrence is meaningless without a file list")));
      if (subcommand_option != DELETE_SUBCOMMAND
	  && subcommand_option != DIFF_SUBCOMMAND
	  && subcommand_option != EXTRACT_SUBCOMMAND
	  && subcommand_option != LIST_SUBCOMMAND)
	    USAGE_ERROR ((0, 0,
			  _("--occurrence cannot be used in the requested operation mode")));
    }

  if (seekable_archive && subcommand_option == DELETE_SUBCOMMAND)
    {
      /* The current code in delete.c is based on the assumption that
	 skip_member() reads all data from the archive. So, we should
	 make sure it won't use seeks. On the other hand, the same code
	 depends on the ability to backspace a record in the archive,
	 so setting seekable_archive to false is technically incorrect.
         However, it is tested only in skip_member(), so it's not a
	 problem. */
      seekable_archive = false;
    }
  
  if (archive_names == 0)
    {
      /* If no archive file name given, try TAPE from the environment, or
	 else, DEFAULT_ARCHIVE from the configuration process.  */

      archive_names = 1;
      archive_name_array[0] = getenv ("TAPE");
      if (! archive_name_array[0])
	archive_name_array[0] = DEFAULT_ARCHIVE;
    }

  /* Allow multiple archives only with `-M'.  */

  if (archive_names > 1 && !multi_volume_option)
    USAGE_ERROR ((0, 0,
		  _("Multiple archive files require `-M' option")));

  if (listed_incremental_option
      && NEWER_OPTION_INITIALIZED (newer_mtime_option))
    USAGE_ERROR ((0, 0,
		  _("Cannot combine --listed-incremental with --newer")));

  if (volume_label_option)
    {
      size_t volume_label_max_len =
	(sizeof current_header->header.name
	 - 1 /* for trailing '\0' */
	 - (multi_volume_option
	    ? (sizeof " Volume "
	       - 1 /* for null at end of " Volume " */
	       + INT_STRLEN_BOUND (int) /* for volume number */
	       - 1 /* for sign, as 0 <= volno */)
	    : 0));
      if (volume_label_max_len < strlen (volume_label_option))
	USAGE_ERROR ((0, 0,
		      ngettext ("%s: Volume label is too long (limit is %lu byte)",
				"%s: Volume label is too long (limit is %lu bytes)",
				volume_label_max_len),
		      quotearg_colon (volume_label_option),
		      (unsigned long) volume_label_max_len));
    }

  if (verify_option)
    {
      if (multi_volume_option)
	USAGE_ERROR ((0, 0, _("Cannot verify multi-volume archives")));
      if (use_compress_program_option)
	USAGE_ERROR ((0, 0, _("Cannot verify compressed archives")));
    }

  if (use_compress_program_option)
    {
      if (multi_volume_option)
	USAGE_ERROR ((0, 0, _("Cannot use multi-volume compressed archives")));
      if (subcommand_option == UPDATE_SUBCOMMAND)
	USAGE_ERROR ((0, 0, _("Cannot update compressed archives")));
    }

  /* It is no harm to use --pax-option on non-pax archives in archive
     reading mode. It may even be useful, since it allows to override
     file attributes from tar headers. Therefore I allow such usage.
     --gray */
  if (args.pax_option
      && archive_format != POSIX_FORMAT
      && (subcommand_option != EXTRACT_SUBCOMMAND
	  || subcommand_option != DIFF_SUBCOMMAND
	  || subcommand_option != LIST_SUBCOMMAND))
    USAGE_ERROR ((0, 0, _("--pax-option can be used only on POSIX archives")));

  /* If ready to unlink hierarchies, so we are for simpler files.  */
  if (recursive_unlink_option)
    old_files_option = UNLINK_FIRST_OLD_FILES;

  if (utc_option)
    verbose_option = 2;

  /* Forbid using -c with no input files whatsoever.  Check that `-f -',
     explicit or implied, is used correctly.  */

  switch (subcommand_option)
    {
    case CREATE_SUBCOMMAND:
      if (args.input_files == 0 && !files_from_option)
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

  if (args.backup_suffix_string)
    simple_backup_suffix = xstrdup (args.backup_suffix_string);

  if (backup_option)
    backup_type = xget_version ("--backup", args.version_control_string);

  if (verbose_option && args.textual_date_option)
    {
      /* FIXME: tartime should support nanoseconds, too, so that this
	 comparison doesn't complain about lost nanoseconds.  */
      char const *treated_as = tartime (newer_mtime_option.tv_sec);
      if (strcmp (args.textual_date_option, treated_as) != 0)
	WARN ((0, 0,
	       ngettext ("Treating date `%s' as %s + %ld nanosecond",
			 "Treating date `%s' as %s + %ld nanoseconds",
			 newer_mtime_option.tv_nsec),
	       args.textual_date_option, treated_as,
	       newer_mtime_option.tv_nsec));
    }
}


/* Tar proper.  */

/* Main routine for tar.  */
int
main (int argc, char **argv)
{
  set_start_time ();
  program_name = argv[0];

#ifdef HAVE_SETLOCALE
  setlocale (LC_ALL, "");
#endif
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  exit_status = TAREXIT_SUCCESS;
  filename_terminator = '\n';
  set_quoting_style (0, escape_quoting_style);

  /* Pre-allocate a few structures.  */

  allocated_archive_names = 10;
  archive_name_array =
    xmalloc (sizeof (const char *) * allocated_archive_names);
  archive_names = 0;

#ifdef SIGCHLD
  /* System V fork+wait does not work if SIGCHLD is ignored.  */
  signal (SIGCHLD, SIG_DFL);
#endif

  init_names ();

  /* Decode options.  */

  decode_options (argc, argv);
  name_init ();

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
      create_archive ();
      name_close ();

      if (totals_option)
	print_total_written ();
      break;

    case EXTRACT_SUBCOMMAND:
      extr_init ();
      read_and (extract_archive);

      /* FIXME: should extract_finish () even if an ordinary signal is
	 received.  */
      extract_finish ();

      break;

    case LIST_SUBCOMMAND:
      read_and (list_archive);
      break;

    case DIFF_SUBCOMMAND:
      diff_init ();
      read_and (diff_archive);
      break;
    }

  if (check_links_option)
    check_links ();

  if (volno_file_option)
    closeout_volume_number ();

  /* Dispose of allocated memory, and return.  */

  free (archive_name_array);
  name_term ();

  if (stdlis != stderr && (ferror (stdlis) || fclose (stdlis) != 0))
    FATAL_ERROR ((0, 0, _("Error in writing to standard output")));
  if (exit_status == TAREXIT_FAILURE)
    error (0, 0, _("Error exit delayed from previous errors"));
  if (ferror (stderr) || fclose (stderr) != 0)
    exit_status = TAREXIT_FAILURE;
  return exit_status;
}

void
tar_stat_init (struct tar_stat_info *st)
{
  memset (st, 0, sizeof (*st));
}

void
tar_stat_destroy (struct tar_stat_info *st)
{
  free (st->orig_file_name);
  free (st->file_name);
  free (st->link_name);
  free (st->uname);
  free (st->gname);
  free (st->sparse_map);
  memset (st, 0, sizeof (*st));
}
