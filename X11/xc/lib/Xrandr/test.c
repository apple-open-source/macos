/* $XFree86: xc/lib/Xrandr/test.c,v 1.2 2001/08/06 21:46:03 dawes Exp $ */

#include <stdio.h>
#include <X11/Xlib.h>
#include "Xrandr.h"

main (int argc, char **argv)

{
  char *display_name = ":0";
  Display *display;
  int major, minor, status;
  
  if ((display = XOpenDisplay (display_name)) == NULL) {
    fprintf(stderr, "Can't open display!\n");
  }
  status = XRRQueryVersion (display, &major, &minor);
  fprintf(stderr, "status = %d, major = %d, minor = %d\n, 
	status, major, minor");

}
