#include "make.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <mach/mach.h>
#ifdef MACOSX
#include <servers/bootstrap.h>
#else
#include <servers/netname.h>
#endif
#include <sys/param.h>
#ifdef _WIN32
#include <direct.h>
#endif

#ifdef MACOSX
#define port_t mach_port_t
#ifndef PORT_NULL
#define PORT_NULL MACH_PORT_NULL
#endif
#endif

void
v_make_support (type, name, file, line, msg, ap)
      int type;
      const char *name;
      const char *file;
      int line;
      const char *msg;
      va_list ap;
{
  static port_t port = PORT_NULL;
  static int already_tried = 0;
  static char directory[MAXPATHLEN];
  char message[1000];
  
  if (port == PORT_NULL && already_tried == 0)
    {
      char *port_name = getenv ("MAKEPORT");
#ifndef MACOSX
      char *host_name = getenv ("MAKEHOST");
#endif
      
      already_tried = 1;
      
      if (port_name == NULL)
        return;
      
#ifdef MACOSX
      bootstrap_look_up (bootstrap_port, port_name, &port);
#else
      if (host_name == NULL)
        host_name = "";

      netname_look_up (name_server_port, host_name, port_name, &port);
#endif
      
      if (port == PORT_NULL)
	return;

#ifdef _WIN32
      getcwd (directory, MAXPATHLEN);
#else
      getwd (directory);
#endif
    }
  
  if (name == NULL)
    name = "";
  
  if (file == NULL)
    file = "";
  
  if (msg == NULL)
    message[0] = '\0';
  else
    vsprintf (message, msg, ap);

  /* MAKE_ALERT doesn't take const params, hence the explicit casting
     away of the const-ness.  */
  
  make_alert (port,
	      type,
	      (char *)name, strlen (name) + 1,
	      (char *)file, strlen (file) + 1,
	      directory, strlen (directory) + 1,
	      line,
	      (char *)message, strlen (message) + 1);
}

void
make_support (int type, const char *name, const char *file, int line, const char *msg, ...)
{
  va_list ap;
  va_start (ap, msg);
  v_make_support (type, name, file, line, msg, ap);
  va_end (ap);
}
