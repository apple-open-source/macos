/*
Copyright (c) 2002 Francis James Franklin <fjf@alinameridon.com>

Maintained by Peter O'Gorman <ogorman@users.sourceforge.net>

Bug Reports and other queries should go to <ogorman@users.sourceforge.net>

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <string.h>
#define __BSD_VISIBLE 1
#include "dlfcn.h"

#define FLAGS (RTLD_NOW|RTLD_GLOBAL)

typedef void (*dlModuleTestFn) (int number);

int test_one ();
int test_two ();
int test_three ();

int main (int argc, char ** argv)
{
  if (argc == 1) return test_one ();

  if (strcmp (argv[1], "-3") == 0)
    return test_three ();

  if (strcmp (argv[1], "-2") == 0)
    return test_two ();

  return test_one ();
}

int test_one ()
{
  dlModuleTestFn test = 0;
  void * handle[4] = { 0, 0, 0, 0 };
  void * handle2[4] = { 0, 0, 0, 0 };
  int i;

  fprintf (stderr, "\n");

  fprintf (stderr, "==================\n");
  fprintf (stderr, "Test #1: existence\n");
  fprintf (stderr, "==================\n");

  handle[0] = dlopen ("dlone.so", FLAGS);
  if (handle[0] == 0)
    {
      fprintf (stderr, "dlopen(\"dlone.so\",FLAGS) failed: %s\n", dlerror ());
      return 1;
    }
  test = (dlModuleTestFn) dlsym (handle[0], "test");
  if(test == 0)
    {
      fprintf (stderr, "dlsym(handle[0],\"test\"): symbol not found\n");
      return 1;
    }
  test (0);
  dlclose (handle[0]);

  handle[0] = dlopen ("dltwo.so", FLAGS);
  if (handle[0] == 0)
    {
      fprintf (stderr, "dlopen(\"dltwo.so\",FLAGS) failed: %s\n", dlerror ());
      return 1;
    }
  test = (dlModuleTestFn) dlsym (handle[0], "test");
  if(test == 0)
    {
      fprintf (stderr, "dlsym(handle[0],\"test\"): symbol not found\n");
      return 1;
    }
  test (0);
  dlclose (handle[0]);

  fprintf (stderr, "\n");

  fprintf (stderr, "================\n");
  fprintf (stderr, "Test #2: re-open\n");
  fprintf (stderr, "================\n");

  handle[0] = dlopen ("dlone.so", FLAGS);
  if (handle[0] == 0)
    {
      fprintf (stderr, "dlopen(\"dlone.so\",FLAGS) failed: %s\n", dlerror ());
      return 1;
    }
  test = (dlModuleTestFn) dlsym (handle[0], "test");
  if(test == 0)
    {
      fprintf (stderr, "dlsym(handle[0],\"test\"): symbol not found\n");
      return 1;
    }
  test (0);
  dlclose (handle[0]);

  handle[0] = dlopen ("dltwo.so", FLAGS);
  if (handle[0] == 0)
    {
      fprintf (stderr, "dlopen(\"dltwo.so\",FLAGS) failed: %s\n", dlerror ());
      return 1;
    }
  test = (dlModuleTestFn) dlsym (handle[0], "test");
  if(test == 0)
    {
      fprintf (stderr, "dlsym(handle[0],\"test\"): symbol not found\n");
      return 1;
    }
  test (0);
  dlclose (handle[0]);

  fprintf (stderr, "\n");

  fprintf (stderr, "===================\n");
  fprintf (stderr, "Test #3: multi-open\n");
  fprintf (stderr, "===================\n");

  for (i = 0; i < 4; i++)
    {
      handle[i] = dlopen ("dlone.so", FLAGS);
      if (handle[i] == 0)
	{
	  fprintf (stderr, "dlopen(\"dlone.so\",FLAGS) failed: %s\n", dlerror ());
	  return 1;
	}
      test = (dlModuleTestFn) dlsym (handle[i], "test");
      if(test == 0)
	{
	  fprintf (stderr, "dlsym(handle[i],\"test\"): symbol not found\n");
	  return 1;
	}
      test (i);

      handle2[i] = dlopen ("dltwo.so", FLAGS);
      if (handle[i] == 0)
	{
	  fprintf (stderr, "dlopen(\"dltwo.so\",FLAGS) failed: %s\n", dlerror ());
	  return 1;
	}
      test = (dlModuleTestFn) dlsym (handle2[i], "test");
      if(test == 0)
	{
	  fprintf (stderr, "dlsym(handle2[i],\"test\"): symbol not found\n");
	  return 1;
	}
      test (i);
    }
  for (i = 0; i < 4; i++)
    {
      dlclose (handle[i]);
      dlclose (handle2[i]);
    }

  fprintf (stderr, "\n");

  fprintf (stderr, "===================\n");
  fprintf (stderr, "Test #4: dylib-open\n");
  fprintf (stderr, "===================\n");

  for (i = 0; i < 4; i++)
    {
      handle[i] = dlopen ("dllib.dylib", FLAGS);
      if (handle[i] == 0)
	{
	  fprintf (stderr, "dlopen(\"dllib.dylib\",FLAGS) failed: %s\n", dlerror ());
	  return 1;
	}
      test = (dlModuleTestFn) dlsym (handle[i], "test");
      if(test == 0)
	{
	  fprintf (stderr, "dlsym(handle[i],\"test\"): symbol not found\n");
	  return 1;
	}
      test (i);
    }
  for (i = 0; i < 4; i++)
    {
      dlclose (handle[i]);
    }

  fprintf (stderr, "\n");

  fprintf (stderr, "======================\n");
  fprintf (stderr, "Test #5: dylib-re-open\n");
  fprintf (stderr, "======================\n");

  handle[0] = dlopen ("dllib.dylib", FLAGS);
  if (handle[0] == 0)
    {
      fprintf (stderr, "dlopen(\"dllib.dylib\",FLAGS) failed: %s\n", dlerror ());
      return 1;
    }
  test = (dlModuleTestFn) dlsym (handle[0], "test");
  if(test == 0)
    {
      fprintf (stderr, "dlsym(handle[0],\"test\"): symbol not found\n");
      return 1;
    }
  test (0);
  dlclose (handle[0]);

  fprintf (stderr, "\n");

  return 0;
}

int test_two ()
{
  dlModuleTestFn test = 0;
  void * handle[4] = { 0, 0, 0, 0 };

  fprintf (stderr, "\n");

  fprintf (stderr, "=====================\n");
  fprintf (stderr, "Test #2.1: open dylib\n");
  fprintf (stderr, "=====================\n");

  handle[0] = dlopen ("dllib.dylib", FLAGS);
  if (handle[0] == 0)
    {
      fprintf (stderr, "dlopen(\"dllib.dylib\",FLAGS) failed: %s\n", dlerror ());
      return 1;
    }
  test = (dlModuleTestFn) dlsym (handle[0], "test");
  if(test == 0)
    {
      fprintf (stderr, "dlsym(handle[0],\"test\"): symbol not found\n");
      return 1;
    }
  test (0);

  fprintf (stderr, "\n");

  fprintf (stderr, "======================\n");
  fprintf (stderr, "Test #2.2: open module\n");
  fprintf (stderr, "======================\n");

  handle[1] = dlopen ("dlone.so", FLAGS);
  if (handle[1] == 0)
    {
      fprintf (stderr, "dlopen(\"dlone.so\",FLAGS) failed: %s\n", dlerror ());
      return 1;
    }
  test = (dlModuleTestFn) dlsym (handle[1], "test");
  if(test == 0)
    {
      fprintf (stderr, "dlsym(handle[1],\"test\"): symbol not found\n");
      return 1;
    }
  test (0);

  fprintf (stderr, "\n");
  fprintf (stderr, "(closing)\n");
  fprintf (stderr, "\n");

  dlclose (handle[1]);
  dlclose (handle[0]);

  fprintf (stderr, "\n");

  fprintf (stderr, "=====================\n");
  fprintf (stderr, "Test #2.3: open dylib\n");
  fprintf (stderr, "=====================\n");

  handle[0] = dlopen ("dllib.dylib", FLAGS);
  if (handle[0] == 0)
    {
      fprintf (stderr, "dlopen(\"dllib.dylib\",FLAGS) failed: %s\n", dlerror ());
      return 1;
    }
  test = (dlModuleTestFn) dlsym (handle[0], "test");
  if(test == 0)
    {
      fprintf (stderr, "dlsym(handle[0],\"test\"): symbol not found\n");
      return 1;
    }
  test (0);

  fprintf (stderr, "\n");

  fprintf (stderr, "======================\n");
  fprintf (stderr, "Test #2.4: open module\n");
  fprintf (stderr, "======================\n");

  handle[1] = dlopen ("dltwo.so", FLAGS);
  if (handle[1] == 0)
    {
      fprintf (stderr, "dlopen(\"dltwo.so\",FLAGS) failed: %s\n", dlerror ());
      return 1;
    }
  test = (dlModuleTestFn) dlsym (handle[1], "test");
  if(test == 0)
    {
      fprintf (stderr, "dlsym(handle[1],\"test\"): symbol not found\n");
      return 1;
    }
  test (0);

  fprintf (stderr, "\n");
  fprintf (stderr, "(closing)\n");
  fprintf (stderr, "\n");

  dlclose (handle[1]);
  dlclose (handle[0]);

  fprintf (stderr, "\n");

  return 0;
}

void * try_open (const char * f1, const char * f2, const char * f3)
{
  void * handle = 0;

  fprintf (stderr, "- loading `%s'... ", f1);
  if ((handle = dlopen (f1, FLAGS)) == 0)
    {
      fprintf (stderr, "%s\n", dlerror ());
      fprintf (stderr, "- loading `%s'...\n", f2);
      if ((handle = dlopen (f2, FLAGS)) == 0)
	{
	  fprintf (stderr, "%s\n", dlerror ());
	  fprintf (stderr, "- loading `%s'...\n", f3);
	  if ((handle = dlopen (f3, FLAGS)) == 0)
	    {
	      fprintf (stderr, "%s\n", dlerror ());
	    }
	}
    }
  fprintf (stderr, "  handle = %p\n", handle);

  return handle;
}

int test_three ()
{
  dlModuleTestFn test = 0;
  void * handle[4] = { 0, 0, 0, 0 };

  fprintf (stderr, "\n");

  fprintf (stderr, "=====================\n");
  fprintf (stderr, "Test #3.1: open dylib\n");
  fprintf (stderr, "=====================\n");

  handle[0] = try_open ("dllib.a", "libs/dllib.dylib", "dllib.dylib");
  if (handle[0] == 0)
    {
      fprintf (stderr, "dlopen(\"dllib.dylib\",FLAGS) failed: %s\n", dlerror ());
      return 1;
    }
  test = (dlModuleTestFn) dlsym (handle[0], "test");
  if(test == 0)
    {
      fprintf (stderr, "dlsym(handle[0],\"test\"): symbol not found\n");
      return 1;
    }
  test (0);

  fprintf (stderr, "\n");

  fprintf (stderr, "======================\n");
  fprintf (stderr, "Test #3.2: open module\n");
  fprintf (stderr, "======================\n");

  handle[1] = try_open ("dlone.a", "libs/dlone.so", "dlone.so");
  if (handle[1] == 0)
    {
      fprintf (stderr, "dlopen(\"dlone.so\",FLAGS) failed: %s\n", dlerror ());
      return 1;
    }
  test = (dlModuleTestFn) dlsym (handle[1], "test");
  if(test == 0)
    {
      fprintf (stderr, "dlsym(handle[1],\"test\"): symbol not found\n");
      return 1;
    }
  test (0);

  fprintf (stderr, "\n");
  fprintf (stderr, "(closing)\n");
  fprintf (stderr, "\n");

  dlclose (handle[1]);
  dlclose (handle[0]);

  fprintf (stderr, "\n");

  fprintf (stderr, "=====================\n");
  fprintf (stderr, "Test #3.3: open dylib\n");
  fprintf (stderr, "=====================\n");

  handle[0] = try_open ("dllib.a", "libs/dllib.dylib", "dllib.dylib");
  if (handle[0] == 0)
    {
      fprintf (stderr, "dlopen(\"dllib.dylib\",FLAGS) failed: %s\n", dlerror ());
      return 1;
    }
  test = (dlModuleTestFn) dlsym (handle[0], "test");
  if(test == 0)
    {
      fprintf (stderr, "dlsym(handle[0],\"test\"): symbol not found\n");
      return 1;
    }
  test (0);

  fprintf (stderr, "\n");

  fprintf (stderr, "======================\n");
  fprintf (stderr, "Test #3.4: open module\n");
  fprintf (stderr, "======================\n");

  handle[1] = try_open ("dltwo.a", "libs/dltwo.so", "dltwo.so");
  if (handle[1] == 0)
    {
      fprintf (stderr, "dlopen(\"dltwo.so\",FLAGS) failed: %s\n", dlerror ());
      return 1;
    }
  test = (dlModuleTestFn) dlsym (handle[1], "test");
  if(test == 0)
    {
      fprintf (stderr, "dlsym(handle[1],\"test\"): symbol not found\n");
      return 1;
    }
  test (0);

  fprintf (stderr, "\n");
  fprintf (stderr, "(closing)\n");
  fprintf (stderr, "\n");

  dlclose (handle[1]);
  dlclose (handle[0]);

  fprintf (stderr, "\n");

  return 0;
}
