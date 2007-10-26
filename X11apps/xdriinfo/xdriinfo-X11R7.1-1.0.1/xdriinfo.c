/*
 * Client application for querying drivers' configuration information
 * Copyright (C) 2003 Felix Kuehling
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * FELIX KUEHLING, OR ANY OTHER CONTRIBUTORS BE LIABLE FOR ANY CLAIM, 
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE 
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 */

#define GLX_GLXEXT_PROTOTYPES
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

typedef const char * glXGetScreenDriver_t (Display *dpy, int scrNum);
typedef const char * glXGetDriverConfig_t (const char *driverName);

glXGetScreenDriver_t *GetScreenDriver;
glXGetDriverConfig_t *GetDriverConfig;

enum INFO_FUNC {
    LIST, NSCREENS, DRIVER, OPTIONS
};

void printUsage (void);

void printUsage (void) {
    fprintf (stderr,
"Usage: xdriinfo [-display <dpy>] [command]\n"
"Commands:\n"
"  nscreens               print the number of screens on display\n"
"  driver screen          print the DRI driver name of screen\n"
"  options screen|driver  print configuration information about screen or driver\n"
"If no command is given then the DRI drivers for all screens are listed.\n");
}

int main (int argc, char *argv[]) {
    Display *dpy;
    int nScreens, screenNum, i;
    enum INFO_FUNC func = LIST;
    char *funcArg = NULL;
    char *dpyName = NULL;

    GetScreenDriver = (glXGetScreenDriver_t *)glXGetProcAddressARB ((const GLubyte *)"glXGetScreenDriver");
    GetDriverConfig = (glXGetDriverConfig_t *)glXGetProcAddressARB ((const GLubyte *)"glXGetDriverConfig");
    if (!GetScreenDriver || !GetDriverConfig) {
	fprintf (stderr, "libGL is too old.\n");
	return 1;
    }

  /* parse the command line */
    for (i = 1; i < argc; ++i) {
	char **argPtr = NULL;
	if (!strcmp (argv[i], "-display"))
	    argPtr = &dpyName;
	else if (!strcmp (argv[i], "nscreens"))
	    func = NSCREENS;
	else if (!strcmp (argv[i], "driver")) {
	    func = DRIVER;
	    argPtr = &funcArg;
	} else if (!strcmp (argv[i], "options")) {
	    func = OPTIONS;
	    argPtr = &funcArg;
	} else {
	    printUsage ();
	    return 1;
	}
	if (argPtr) {
	    if (++i == argc) {
		printUsage ();
		return 1;
	    }
	    *argPtr = argv[i];
	}
    }

  /* parse screen number argument */
    if (func == DRIVER || func == OPTIONS) {
	if (sscanf (funcArg, "%i", &screenNum) != 1)
	    screenNum = -1;
	else if (screenNum < 0) {
	    fprintf (stderr, "Negative screen number \"%s\".\n", funcArg);
	    return 1;
	}
    }
  /* if the argument to the options command is a driver name, we can handle
   * it without opening an X connection */
    if (func == OPTIONS && screenNum == -1) {
	const char *options = (*GetDriverConfig) (funcArg);
	if (!options) {
	    fprintf (stderr,
		     "Driver \"%s\" is not installed or does not support configuration.\n",
		     funcArg);
	    return 1;
	}
	printf ("%s", options);
	if (isatty (STDOUT_FILENO))
	    printf ("\n");
	return 0;
    } 
  /* driver command needs a valid screen number */
    else if (func == DRIVER && screenNum == -1) {
	fprintf (stderr, "Invalid screen number \"%s\".\n", funcArg);
	return 1;
    }

  /* open display and count the number of screens */
    if (!(dpy = XOpenDisplay (dpyName))) {
	fprintf (stderr, "Error: Couldn't open display\n");
	return 1;
    }
    nScreens = ScreenCount (dpy);

  /* final check on the screen number argument (if any)*/
    if ((func == DRIVER || func == OPTIONS) && screenNum >= nScreens) {
	fprintf (stderr, "Screen number \"%d\" out of range.\n", screenNum);
	return 1;
    }

    switch (func) {
      case NSCREENS:
	printf ("%d", nScreens);
	if (isatty (STDOUT_FILENO))
	    printf ("\n");
	break;
      case DRIVER: {
	  const char *name = (*GetScreenDriver) (dpy, screenNum);
	  if (!name) {
	      fprintf (stderr, "Screen \"%d\" is not direct rendering capable.\n",
		       screenNum);
	      return 1;
	  }
	  printf ("%s", name);
	  if (isatty (STDOUT_FILENO))
	      printf ("\n");
	  break;
      }
      case OPTIONS: {
	  const char *name = (*GetScreenDriver) (dpy, screenNum), *options;
	  if (!name) {
	      fprintf (stderr, "Screen \"%d\" is not direct rendering capable.\n",
		       screenNum);
	      return 1;
	  }
	  options = (*GetDriverConfig) (name);
	  if (!options) {
	      fprintf (stderr,
		       "Driver \"%s\" is not installed or does not support configuration.\n",
		       name);
	      return 1;
	  }
	  printf ("%s", options);
	  if (isatty (STDOUT_FILENO))
	      printf ("\n");
	  break;
      }
      case LIST:
	for (i = 0; i < nScreens; ++i) {
	    const char *name = (*GetScreenDriver) (dpy, i);
	    if (name)
		printf ("Screen %d: %s\n", i, name);
	    else
		printf ("Screen %d: not direct rendering capable.\n", i);
	}
    }

    return 0;
}
