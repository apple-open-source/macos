#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#include <errno.h>
#include <err.h>
#include <sysexits.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "dumpemacs.h"

int main(int argc, char *argv[]) {
  int ret;

  if(!is_emacs_valid(0)) {
    const char *newargs[2];
    newargs[0] = kDumpEmacsPath;
    newargs[1] = NULL;

    ret = runit(newargs, 0);
    if(ret)
      errx(1, "Failed to dump emacs");

  }

  ret = execv(kEmacsArchPath, argv);
  if(ret)
    err(EX_OSERR, "execv(%s) failed", kEmacsArchPath);

  // shouldn't get here
  return EX_OSERR;
}
