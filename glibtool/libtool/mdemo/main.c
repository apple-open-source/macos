/* main.c -- mdemo test program
   Copyright (C) 1998-2000 Free Software Foundation, Inc.
   Originally by Thomas Tanner <tanner@ffii.org>
   This file is part of GNU Libtool.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
USA. */

#include "foo.h"
#include "ltdl.h"
#include <stdio.h>

int
test_dl (filename)
  char *filename;
{
  lt_dlhandle handle;	
  const lt_dlinfo *info;
  int (*pfoo1)() = 0;
  int (*pfoo2)() = 0;
  int (*phello)() = 0;
  int *pnothing = 0;
  int ret = 0;

  handle = lt_dlopen(filename);
  if (!handle) {
    fprintf (stderr, "can't open the module %s!\n", filename);
    fprintf (stderr, "error was: %s\n", lt_dlerror());
    return 1;
  }

  info = lt_dlgetinfo(handle);
  if (!info) {
    fprintf (stderr, "can't get module info: %s\n", lt_dlerror());
    return 1;
  }
  if (info->name) {
    printf ("module name: %s\n", info->name);
  } else {
    printf ("module is not a libtool module\n");
  }
  printf ("module filename: %s\n", info->filename);
  printf ("module reference count: %i\n", info->ref_count);
  
  phello = (int(*)())lt_dlsym(handle, "hello");  
  if (phello)
    {
      int value = (*phello) ();
      
      printf ("hello returned: %i\n", value);
      if (value == HELLO_RET)
        printf("hello is ok!\n");
    }
  else
    {
      fprintf (stderr, "did not find the `hello' function\n");
      fprintf (stderr, "error was: %s\n", lt_dlerror());
      ret = 1;
    }

  pnothing = (int*)lt_dlsym(handle, "nothing");  
  /* Try assigning to the nothing variable. */
  if (pnothing)
    *pnothing = 1;
  else
    {
      fprintf (stderr, "did not find the `nothing' variable\n");
      fprintf (stderr, "error was: %s\n", lt_dlerror());
      ret = 1;
    }

  pfoo1 = (int(*)())lt_dlsym(handle, "foo1");  
  /* Just call the functions and check return values. */
  if (pfoo1)
    {
      if ((*pfoo1) () == FOO_RET)
        printf("foo1 is ok!\n");
      else
	ret = 1;
    }
  else {
    pfoo2 = (int(*)())lt_dlsym(handle, "foo2");  
    if (pfoo2)
      {
        if ((*pfoo2) () == FOO_RET)
          printf("foo2 is ok!\n");
        else ret = 1;
      }
    else
      {
        fprintf (stderr, "did not find any of the `foo' functions\n");
        fprintf (stderr, "error was: %s\n", lt_dlerror());
        ret = 1;
      }
  }
  lt_dlclose(handle);
  return ret;
}

int
myfunc ()
{
  return HELLO_RET;
}

int myvar;

int
test_dlself ()
{
  lt_dlhandle handle;	
  int (*pmyfunc)() = 0;
  int *pmyvar = 0;
  int ret = 0;

  handle = lt_dlopen(0);
  if (!handle) {
    fprintf (stderr, "can't dlopen the program!\n");
    fprintf (stderr, "error was: %s\n", lt_dlerror());
    return 1;
  }

  pmyfunc = (int(*)())lt_dlsym(handle, "myfunc");  
  if (pmyfunc)
    {
      int value = (*pmyfunc) ();
      
      printf ("myfunc returned: %i\n", value);
      if (value == HELLO_RET)
        printf("myfunc is ok!\n");
    }
  else
    {
      fprintf (stderr, "did not find the `myfunc' function\n");
      fprintf (stderr, "error was: %s\n", lt_dlerror());
      ret = 1;
    }

  pmyvar = (int*)lt_dlsym(handle, "myvar");  
  /* Try assigning to the variable. */
  if (pmyvar)
    *pmyvar = 1;
  else
    {
      fprintf (stderr, "did not find the `myvar' variable\n");
      fprintf (stderr, "error was: %s\n", lt_dlerror());
      ret = 1;
    }

  lt_dlclose(handle);
  return ret;
}

int
main (argc, argv)
  int argc;
  char **argv;
{
  int i;
  int ret = 0;

  printf ("Welcome to GNU libtool mdemo!\n");

  if (argc < 2) {
    fprintf (stderr, "usage: %s module [module...]\n", argv[0]);
  }

  LTDL_SET_PRELOADED_SYMBOLS();
  if (lt_dlinit() != 0) {
    fprintf (stderr, "error during initialization: %s\n", lt_dlerror());
    return 1;
  }

  for (i = 1; i < argc; i++)
    if (test_dl(argv[i]))
       ret = 1;

  if (test_dlself())
    ret = 1;

  lt_dlexit();
  return ret;
}
