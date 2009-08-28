/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <sys/param.h>
#include <sys/stat.h>

static int only_headers = 0;
static int verbose = 0;
static int silent = 0;
static int allow_nonexistence = 0;

/*
** returns 0 if file1 is newer than file2
**
** if file2 is a directory, returns 0 if
** file1 is newer than every file in file2
*/

void fail (const char* filename)
{
   fprintf (stderr, "newer: error: cannot stat file \"%s\"\n", filename);
   exit (2);
}

int newer (const char* file1, long file1_mtime, const char* file2)
{
  int header = 0;
  int makefile = 0;
  static char path[MAXPATHLEN];
  struct stat beta;

#ifdef DEBUG
  printf ("comparing %s to %s\n", file1, file2);
#endif

  if (only_headers)
    {
    const char* dot = strrchr (file2, '.');
    if (dot && dot[1]=='h' && dot[2]=='\0') header = 1;
    if (!strncmp (file2, "Makefile", 8)) makefile = 1;
    }

  if (stat (file2, &beta))
    {
    if (!silent) fprintf (stderr, "%s does not exist\n", file2);
    return 0;
    }
  else if (beta.st_mode & S_IFDIR)
    {
    int file2_length = strlen (file2);
    int flag = 1;
    DIR* dir;
#ifdef __APPLE__
    struct direct* entry;
#else
    struct dirent* entry;
#endif

    memcpy (path, file2, file2_length);
    path[file2_length] = '/';
    path[file2_length+1] = '\0';
#ifdef DEBUG
    printf ("recursively processing %s\n", path);
#endif

    if (!((dir = opendir (file2)))) fail (file2);
    while (flag && (entry = readdir (dir)))
      {
      if (entry->d_name[0] == '.') continue;
      strcpy (path + file2_length + 1, entry->d_name);
      flag = newer (file1, file1_mtime, path);
      }
    closedir (dir);
    return flag;
    }
  else if (only_headers && (!makefile && !header))
    {
    return 1;
    }
  else if (file1_mtime <= beta.st_mtime && !(beta.st_mode & S_IFDIR))
    {
    if (verbose) fprintf (stderr, "%s is not newer than %s\n", file1, file2);
    return 0;
    }
  else
    {
    return 1;
    }
}

int main (int argc, char* argv[])
{
  struct stat alpha;
  int result;
  const char* hyphen;
  int i;
  
  while (argv[1] && argv[1][0]=='-') switch (argv[1][1])
    {
  case 'h':
    only_headers = 1;
    argv++; argc--;
    break;
  case 'v':
    verbose = 1;
    argv++; argc--;
    break;
  case 's':
    silent = 1;
    argv++; argc--;
    break;
  case 'n':
    allow_nonexistence = 1;
    argv++; argc--;
    break;
  default:
  error:
    fprintf (stderr, "usage:  newer [options] test-file [file1] ...\n");
    fprintf (stderr, "action:\n");
    fprintf (stderr, "  Succeeds if test-file exists and is newer than all listed comparison files.\n");
    fprintf (stderr, "  If a comparison file is a directory, succeeds only if the test-file is\n");
    fprintf (stderr, "  more recent than all files in the directory as well as the direcory itself.\n");
    fprintf (stderr, "options:\n");
    fprintf (stderr, "  -h  only compare with header files and makefiles\n");
    fprintf (stderr, "      (if file1 matches \"*-headers\" then -h is assumed)\n");
    fprintf (stderr, "  -v  verbose (report file which is newer)\n");
    fprintf (stderr, "  -s  silent (don't report nonexistent files)\n");
    fprintf (stderr, "  -n  succeed if no comparison files listed (even if test file\n");
    fprintf (stderr, "      does not exist)\n");
    exit (2);
    }

  if (allow_nonexistence && argc == 2) return 0;
  if (argc < 2) goto error;
    
  hyphen = strrchr (argv[1], '-');
  if (hyphen && !strcmp (hyphen, "-headers")) only_headers = 1;

  if (stat (argv[1], &alpha))
    {
    if (!silent) fprintf (stderr, "%s does not exist\n", argv[1]);
    result = 0;
    }
  else for (i=2, result=1; result && argv[i]; i++)
    result = newer (argv[1], alpha.st_mtime, argv[i]);
  if (verbose && result) printf ("%s is newer than %s\n", argv[1], argv[2]);
  return !result;
}
