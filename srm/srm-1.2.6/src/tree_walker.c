#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if HAVE_FTS_OPEN
#include <fts.h>
#endif

#include "srm.h"

#ifndef FTS_D
#define FTS_D      0
#define FTS_DC     1
#define FTS_DNR    2
#define FTS_DOT    3
#define FTS_DP     4
#define FTS_ERR    5
#define FTS_F      6
#define FTS_SL     7
#define FTS_SLNONE 8
#define FTS_NS     9 
#endif /* ndef FTS_D */

int prompt_user(const char *string) {
  char inbuf[8];

  printf("%s", string);
  fgets(inbuf, 4, stdin);
  return strncmp(inbuf, "y", 1) == 0;
}

int check_perms(const char *path) {
  int fd;

  if ( ((fd = open(path, O_WRONLY)) == -1) && (errno == EACCES) ) {
    if ( chmod(path, S_IRUSR | S_IWUSR) == -1 ) {
      errorp("Unable to reset %s to writable (probably not owner) "
	     "... skipping", path);
      return 0;
    }
  }

  close(fd);
  return 1;
}

int prompt_file(const char *path, int fts_flag) {
  int fd, return_value = 1;
  size_t bufsize = strlen(path) + 80;
  char *buf;

  if ( (buf = (char *)malloc(bufsize)) == NULL ) {
    errorp("Out of memory at line %d in prompt_file()", __LINE__);
    return 0;
  }

  if (options & OPT_F) {
    if (options & OPT_V) {
      printf("removing %s\n", path);
      fflush(stdout);
    }
    return check_perms(path);
  }

  if ( (fts_flag != FTS_SL) && ((fd = open(path, O_WRONLY)) == -1) &&
       (errno == EACCES) )
  {
    /* Not a symlink, not writable */
    snprintf(buf, bufsize, "Remove write protected file %s? ", path);
    if ( (return_value = prompt_user(buf)) == 1 ) 
      return_value = check_perms(path);
  } else {
    /* Writable file or symlink */
    if (options & OPT_I) {
      snprintf(buf, bufsize, "Remove %s? ", path);
      return_value = prompt_user(buf);
    }
  }

  if ((options & OPT_V) && return_value)
    printf("removing %s\n", path);

  free(buf);
  close(fd); /* close if open succeeded, or silently fail */

  return return_value;
}

int process_file(char *path, int flag) {
  while (path[strlen(path) - 1] == '/') 
    path[strlen(path)- 1] = '\0';

  switch (flag) {
  case FTS_D: break;
  case FTS_DC:
    error("cyclic directory entry %s", path);
    break;
  case FTS_DNR:
    error("%s: permission denied", path);
    break;
  case FTS_DOT: break;
  case FTS_DP:
    if (options & OPT_R) {
      if ( prompt_file(path, flag) && (rename_unlink(path) == -1) )
        errorp("unable to remove %s", path);
    } else {
      error("%s is a directory", path);
    }
    break;
  case FTS_ERR:
    error("fts error on %s", path);
    break;
  case FTS_F:
  case FTS_SL:
  case FTS_SLNONE:
    if ( prompt_file(path, flag) && (sunlink(path) == -1) ) {
      if (errno == EMLINK) 
        error("%s has multiple links, this one has been removed but not "
              "overwritten", path);
      else
        errorp("unable to remove %s", path);
    }
    break;
  case FTS_NS:
    if ( !(options & OPT_F) ) /* Ignore nonexistant files with -f */
      error("unable to stat %s", path);
  default:
    break;
  }
  return 0;
}

#if HAVE_FTS_OPEN

int tree_walker(char **trees) {
  FTSENT *current_file;
  FTS *stream;
  int i = 0;
  
  while (trees[i] != NULL) {
    while (trees[i][strlen(trees[i]) - 1] == '/')
      trees[i][strlen(trees[i]) -1] = '\0';
    i++;
  }

  if ( (stream = fts_open(trees, FTS_PHYSICAL | FTS_NOCHDIR, NULL)) == NULL )
    errorp("fts_open() returned NULL");
  while ( (current_file = fts_read(stream)) != NULL) {
    process_file(current_file->fts_path, current_file->fts_info);
    if ( !(options & OPT_R) )
      fts_set(stream, current_file, FTS_SKIP);
  }
  return 0;
}

#elif HAVE_NFTW

#if defined(__digital__) && defined(__unix__)
/* Shut up tru64's cc(1) */
#define _XOPEN_SOURCE_EXTENDED
#endif
#include <ftw.h>

int ftw_process_path(const char *opath, const struct stat *statbuf, int flag,
		 struct FTW *dummy)
{
  size_t path_size = strlen(opath) + 1;
  char *path = (char *)alloca(path_size);

  if (path == NULL) {
    errno = ENOMEM;
    return -1;
  }
  strncpy(path, opath, path_size);

  switch (flag) {
  case FTW_F:
    process_file(path, FTS_F);
    break;
  case FTW_SL:
    process_file(path, FTS_SL);
    break;
  case FTW_D:
    process_file(path, FTS_D);
    break;
  case FTW_DP:
    process_file(path, FTS_DP);
    break;
  case FTW_DNR:
    process_file(path, FTS_DNR);
    break;
  case FTW_NS:
    process_file(path, FTS_NS);
    break;
  }

  if (options & OPT_R)
    return 0;
  return 1;
}

int tree_walker(char **trees) {
  int i = 0;

  while (trees[i] != NULL) { 
    while (trees[i][strlen(trees[i]) - 1] == '/')
      trees[i][strlen(trees[i]) -1] = '\0';
    if (options & OPT_R)
      nftw(trees[i], ftw_process_path, 10, FTW_DEPTH);
    else
      nftw(trees[i], ftw_process_path, 10, 0);
    i++;
  }
  return 0;
}

#else
#error No tree traversal function found
#endif
