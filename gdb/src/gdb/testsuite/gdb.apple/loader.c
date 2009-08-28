#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <unistd.h>

int
main ()
{
  char curDir[1024];
  char tst_name[1024];
  char dummy_name[1024];
  void *tst_cookie, *dummy_cookie;
  int (*tst_function) (void);

  getcwd (curDir, 1023);
  
  sprintf (tst_name, "%s/gdb.apple/loaded.bundle", curDir);
  sprintf (dummy_name, "%s/gdb.apple/dummy.bundle", curDir);

  tst_cookie = dlopen (tst_name, RTLD_NOW);
  if (tst_cookie == NULL)
    {
      printf ("Could not load bundle \"%s\".\n", tst_name);
      return 0;
    }

  tst_function = (int (*) ()) dlsym (tst_cookie, "tst_function");
  printf ("First time around, got: %d\n", tst_function ());
  dlclose (tst_cookie);

  dummy_cookie = dlopen (dummy_name, RTLD_NOW);
  tst_function = (int (*) ()) dlsym (dummy_cookie, "dummy_function");
  printf ("Calling dummy function: %d.\n", tst_function ());

  tst_cookie = dlopen (tst_name, RTLD_NOW);

  tst_function = (int (*) ()) dlsym (tst_cookie, "tst_function");
  printf ("Second time around, got: %d\n", tst_function ());
  dlclose (tst_cookie);

  return 0;
}
