#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>

#include "dumpemacs.h"

int is_emacs_valid(int debugflag)
{
  struct stat sb;
  int ret;
  time_t lastMod;
  
  ret = lstat(kEmacsArchPath, &sb);
  if(ret == -1) {
    if(errno == ENOENT)
      return 0; // regen it
    else
      return -1; // error
  }

  if(S_ISDIR(sb.st_mode)) {
    errno = EISDIR;
    return -1;
  } else if(!S_ISREG(sb.st_mode)) {
    // if it's not a filem nuke it
    return 0;
  }

  // it's a file. check the times
  lastMod = sb.st_mtime;
  
  // validate that the per-arch executable is
  // newer than the wrapper or dumpemacs
  ret = lstat(kEmacsWrapperPath, &sb);
  if(ret == -1)
    return -1; // error

  if(sb.st_mtime > lastMod) {
    // if the wrapper is more recent than the per-arch
    return 0; // regen
  }
 
  ret = lstat(kDumpEmacsPath, &sb);
  if(ret == -1)
    return -1; // error

  if(sb.st_mtime > lastMod) {
    // if the dumpemacs is more recent than the per-arch
    return 0; // regen
  }


  // per-arch emacs is present and newer than the
  // infrastructure tools, so let it be
  return 1;
}
