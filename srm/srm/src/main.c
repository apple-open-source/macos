#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define GLOBALS
#include "config.h"
#include "srm.h"

int seclevel = 0;
int options = 0;
int show_help = 0;
int show_version = 0;
int opt_buffsize = 0;

struct option longopts[] = {
  { "directory", no_argument, NULL, 'd' },
  { "force", no_argument, NULL, 'f' },
  { "interactive", no_argument, NULL, 'i' },
  { "recursive", no_argument, NULL, 'r' },
  { "simple", no_argument, NULL, 's' },
  { "medium", no_argument, NULL, 'm' },
  { "verbose", no_argument, NULL, 'v' },
  { "nounlink", no_argument, NULL, 'n' },
  { "zero", no_argument, NULL, 'z' },
  { "help", no_argument, &show_help, 1 },
  { "version", no_argument, &show_version, 1 },
  { "verify", no_argument, NULL, 'V' },
  { "bsize", required_argument, NULL, 'B' },
  { NULL, no_argument, NULL, 0 }
};

char *program_name;

int main(int argc, char *argv[]) {
  int opt, i, q;
  char *trees[argc];

  if ( (program_name = strrchr(argv[0], '/')) != NULL)
    program_name++;
  else
    program_name = argv[0];

  while ((opt = getopt_long(argc, argv, "dfirRvsnmzVB:", longopts,
			    NULL)) != -1) {
    switch (opt) {
    case ':': 
    case '?': 
      /* getopt() prints an error message for these cases */
      fprintf(stderr, "Try `%s --help' for more information.\n", program_name);
      exit(EXIT_FAILURE);
    case 0: break; 
    case 'd': break;
    case 'f': 
      options |= OPT_F;
      break;
    case 'i':
      options |= OPT_I;
      break;
    case 'n':
      options |= OPT_N;
      break;
    case 'r':
    case 'R':
      options |= OPT_R;
      break;
    case 's':
      seclevel = 1; /* overrides more secure values, if present */
      break;
    case 'm':
	  if (!seclevel) seclevel = 7; /* overrides default security value */
      break;
    case 'v':
      options |= OPT_V;
      break;
    case 'z':
      options |= OPT_ZERO;
      break;
    case 'V':
      options |= OPT_VERIFY;
      break;
    case 'B':
      opt_buffsize = atoi(optarg);
      break;
    default:
      error("unhandled option %c", opt);
    }
  }

  if (show_help) {
    printf(
	   "Usage: %s [OPTION]... [FILE]...\n"
	   "Overwrite and remove (unlink) the files.\n"
	   "\n"
	   "  -d, --directory     ignored (for compatibility with rm(1))\n"
	   "  -f, --force         ignore nonexistent files, never prompt\n"
	   "  -i, --interactive   prompt before any removal\n"
	   "  -s, --simple        only overwrite with single random pass\n"
	   "  -m, --medium        overwrite with 7 US DoD compliant passes\n"
	   "  -z, --zero          after overwriting, zero blocks used by file\n"
	   "  -n, --nounlink      overwrite file, but do not rename or unlink\n"
	   "  -r, -R, --recursive remove the contents of directories\n"
	   "  -v, --verbose       explain what is being done\n"
	   "      --help          display this help and exit\n"
	   "      --version       display version information and exit\n"
       "\n"
       "Note: The -s option overrides the -m option, if both are present.\n"
       "If neither is specified, the 35-pass Gutmann algorithm is used.\n",
	   program_name);
    exit(EXIT_SUCCESS);
  }

  if (show_version) {
    printf("%s (" PACKAGE ") " VERSION "\n", program_name);
    exit(EXIT_SUCCESS);
  }

  if (optind == argc) {
    printf("%s: too few arguments\n", program_name);
    printf("Try `%s --help' for more information.\n", program_name);
    exit(EXIT_FAILURE);
  }

  init_random(getpid());

  for (i = optind, q = 0; i < argc; i++, q++) 
    trees[q] = argv[i];
  trees[q] = NULL;

  exit(tree_walker(trees));
}
