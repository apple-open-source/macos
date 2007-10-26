#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "srm.h"
#include "config.h"

int empty_directory(const char *path) {
  DIR *dp;
  struct dirent *de;
  
  dp = opendir(path);
  if (dp == NULL) {
    return -1;
  }
  while ((de = readdir(dp)) != NULL) {
    if (de->d_namlen < 3 && (!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))) {
      continue;
    }
    (void)closedir(dp);
    return -1;
  }
  (void)closedir(dp);
  return 0;
}

int rename_unlink(const char *path) {
  char *new_name, *p, c;
  struct stat statbuf;
  size_t new_name_size = strlen(path) + 15;
  int i = 0;
  
  if ( (new_name = (char *)alloca(new_name_size)) == NULL ) {
    errno = ENOMEM;
    return -1;
  }

  strncpy(new_name, path, new_name_size);

  if ( (p = strrchr(new_name, '/')) != NULL ) {
    p++;
    *p = '\0';
  } else {
    p = new_name;
  }

  do {
    i = 0;

    while (i < 14) {
      c = random_char();
      if (isalnum((int) c)) {
	p[i] = c;
	i++;
      }
    }
    p[i] = '\0';
  } while (lstat(new_name, &statbuf) == 0);

  if (lstat(path, &statbuf) == -1)
    return -1;

  if (S_ISDIR(statbuf.st_mode) && (empty_directory(path) == -1)) {
      /* Directory isn't empty (e.g. because it contains an immutable file).
         Attempting to remove it will fail, so avoid renaming it. */
    errno = ENOTEMPTY;
    return -1;
  }

  if (rename(path, new_name) == -1)
    return -1;

  sync();

  if (lstat(new_name, &statbuf) == -1) {
    /* Bad mojo, we just renamed to new_name and now the path is invalid.
       Die ungracefully and exit before anything worse happens. */
    perror("Fatal error in rename_unlink()");
    exit(EXIT_FAILURE);
  }

  if (S_ISDIR(statbuf.st_mode))
    return rmdir(new_name);

  return unlink(new_name);
}
