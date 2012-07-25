#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#include <err.h>

#include "dumpemacs.h"

int decreasepriv(int debugflag);

int runit(const char * const argv[], int dropprivs)
{
  pid_t child;
  int ret;

  child = fork();
  if(child == 0) {
    if(dropprivs && geteuid() == 0) {
      ret = decreasepriv(0);
      if(ret)
	err(1, "failed to decrease privileges");

    }

    ret = execve(argv[0], (char * const *) argv, 0);
    if(ret)
      err(1, "execve(%s) failed", argv[0]);
  } else {
    do {
      int status = 0;
      pid_t rch = waitpid(child, &status, 0);
      if(rch == -1)
	err(1, "waitpid(%d)", child);

      if(WIFSTOPPED(status))
	continue;

      if(WIFSIGNALED(status))
	errx(1, "child exited on signal %d", WTERMSIG(status));

      if(WIFEXITED(status)) {
	if(WEXITSTATUS(status) == 0)
	  break; // success
	else
	  errx(1, "child exited with status %d", WEXITSTATUS(status));
      }

    } while(1);
  }

  return 0;
}


int decreasepriv(int debugflag)
{
  struct passwd *nobody = NULL;
  int ret;

  nobody = getpwnam("nobody");
  if(nobody == NULL)
    err(1, "getpwnam(nobody) failed");


  ret = initgroups(nobody->pw_name, nobody->pw_gid);
  if(ret)
    err(1, "initgroups() failed");

  ret = setgid(nobody->pw_gid);
  if(ret)
    err(1, "setgid() failed");

  ret = setuid(nobody->pw_uid);
  if(ret)
    err(1, "setuid() failed");


  // system("id");

  return 0;
}
