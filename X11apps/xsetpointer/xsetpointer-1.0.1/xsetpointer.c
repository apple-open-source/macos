/* $XFree86: xc/programs/xsetpointer/xsetpointer.c,v 3.7 2003/05/27 22:27:10 tsi Exp $ */

/*
 * Copyright 1995 by Frederic Lepied, France. <fred@sugix.frmug.fr.net>       
 *                                                                            
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is  hereby granted without fee, provided that
 * the  above copyright   notice appear  in   all  copies and  that both  that
 * copyright  notice   and   this  permission   notice  appear  in  supporting
 * documentation, and that   the  name of  Frederic   Lepied not  be  used  in
 * advertising or publicity pertaining to distribution of the software without
 * specific,  written      prior  permission.     Frederic  Lepied   makes  no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.                   
 *                                                                            
 * FREDERIC  LEPIED DISCLAIMS ALL   WARRANTIES WITH REGARD  TO  THIS SOFTWARE,
 * INCLUDING ALL IMPLIED   WARRANTIES OF MERCHANTABILITY  AND   FITNESS, IN NO
 * EVENT  SHALL FREDERIC  LEPIED BE   LIABLE   FOR ANY  SPECIAL, INDIRECT   OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA  OR PROFITS, WHETHER  IN  AN ACTION OF  CONTRACT,  NEGLIGENCE OR OTHER
 * TORTIOUS  ACTION, ARISING    OUT OF OR   IN  CONNECTION  WITH THE USE    OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <X11/Xproto.h>
#include <X11/extensions/XInput.h>

int           event_type;

static int 
StrCaseCmp(char *s1, char *s2)
{
	char c1, c2;

	if (*s1 == 0) {
		if (*s2 == 0)
			return(0);
		else
			return(1);
	}
	c1 = (isupper(*s1) ? tolower(*s1) : *s1);
	c2 = (isupper(*s2) ? tolower(*s2) : *s2);
	while (c1 == c2)
	{
		if (c1 == '\0')
			return(0);
		s1++; s2++;
		c1 = (isupper(*s1) ? tolower(*s1) : *s1);
		c2 = (isupper(*s2) ? tolower(*s2) : *s2);
	}
	return(c1 - c2);
}

int
main(int argc, char * argv[])
{
  int                loop, num_extensions, num_devices;
  char               **extensions;
  XDeviceInfo        *devices;
  XDeviceCoreControl corectl;
  Display            *dpy;
  int		     list = 0, core = 0;
  XDevice            *device;
  
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "usage : %s (-l | -c | +c ) <device name>)\n", argv[0]);
    exit(1);
  }

  if (strcmp(argv[1], "-l") == 0) {
    list = 1;
  }
  else if (strcmp(argv[1], "-c") == 0) {
    core = 1;
  }
  else if (strcmp(argv[1], "+c") == 0) {
    core = 2;
  }
  
  dpy = XOpenDisplay(NULL);

  if (!dpy) {
    printf("unable to connect to X Server try to set the DISPLAY variable\n");
    exit(1);
  }

#ifdef DEBUG
  printf("connected to %s\n", XDisplayString(dpy));
#endif

  extensions = XListExtensions(dpy, &num_extensions);
  for (loop = 0; loop < num_extensions &&
         (strcmp(extensions[loop], "XInputExtension") != 0); loop++);
  XFreeExtensionList(extensions);
  if (loop != num_extensions)
    {
      devices = XListInputDevices(dpy, &num_devices);
      for(loop=0; loop<num_devices; loop++)
        {
	  if (list) {
	      printf("%d: \"%s\"	[", devices[loop].id, devices[loop].name ? devices[loop].name : "<noname>");
	      switch(devices[loop].use) {
	      case IsXPointer:
		  printf("XPointer]\n");
		  break;
	      case IsXKeyboard:
		  printf("XKeyboard]\n");
		  break;
	      case IsXExtensionDevice:
		  printf("XExtensionDevice]\n");
		  break;
              case IsXExtensionKeyboard:
                  printf("XExtensionKeyboard]\n");
                  break;
              case IsXExtensionPointer:
                  printf("XExtensionPointer]\n");
                  break;
	      default:
		  printf("invalid value]\n");
		  break;
	      }
	  }
          else if (core) {
            if (argc == 3 && devices[loop].name &&
                StrCaseCmp(devices[loop].name, argv[2]) == 0) {
#ifdef DEBUG
                fprintf(stderr, "opening device %s at %d\n",
                        devices[loop].name ? devices[loop].name : "<noname>",
                        devices[loop].id);
#endif
              device = XOpenDevice(dpy, devices[loop].id);
              if (device) {
                corectl.status = (core - 1);
                corectl.length = sizeof(corectl);
                corectl.control = DEVICE_CORE;
                XChangeDeviceControl(dpy, device, DEVICE_CORE,
                                     (XDeviceControl *)&corectl);
                exit(0);
              }
              else {
                fprintf(stderr, "error opening device\n");
                exit(1);
              }
            }
          }
	  else {
          if ((argc == 2) && devices[loop].name &&
              (StrCaseCmp(devices[loop].name, argv[1]) == 0))
            if (devices[loop].use == IsXExtensionDevice)
              {
#ifdef DEBUG
                fprintf(stderr, "opening device %s\n",
                        devices[loop].name ? devices[loop].name : "<noname>");
#endif
                device = XOpenDevice(dpy, devices[loop].id);
                if (device)
                  {
                    XChangePointerDevice(dpy, device, 0, 1);
                    exit(0);
                  }
                else
                  {
                    fprintf(stderr, "error opening device\n");
                    exit(1);
                  }
              }
	  }
        }
      XFreeDeviceList(devices);
    }
  else
    {
      fprintf(stderr, "No XInput extension available\n");
      exit(1);
    }
  
  if (list) {
    exit(0);
  }
  else {
    fprintf(stderr, "Extended device %s not found\n", core ? argv[2] :
                                                             argv[1]);
    exit(1);
  }
}
