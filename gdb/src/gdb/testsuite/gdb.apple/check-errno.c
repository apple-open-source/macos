#include <errno.h>
#include <sys/stat.h>

int
main ()
{
  int result;
  struct stat buf;

  errno = 0;

  result = stat ("./dont-make-a-file-with-this-name-in-the-testsuite", &buf); /* Good place to break first */

  return 0; /* Good place to break second */
}
