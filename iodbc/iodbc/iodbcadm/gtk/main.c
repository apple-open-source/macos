/*
 *  main.c
 *
 *  $Id: main.c,v 1.9 2007/02/02 11:58:14 source Exp $
 *
 *  Main program
 *
 *  The iODBC driver manager.
 *
 *  Copyright (C) 1996-2006 by OpenLink Software <iodbc@openlinksw.com>
 *  All Rights Reserved.
 *
 *  This software is released under the terms of either of the following
 *  licenses:
 *
 *      - GNU Library General Public License (see LICENSE.LGPL)
 *      - The BSD License (see LICENSE.BSD).
 *
 *  Note that the only valid version of the LGPL license as far as this
 *  project is concerned is the original GNU Library General Public License
 *  Version 2, dated June 1991.
 *
 *  While not mandated by the BSD license, any patches you make to the
 *  iODBC source code may be contributed back into the iODBC project
 *  at your discretion. Contributions will benefit the Open Source and
 *  Data Access community as a whole. Submissions may be made at:
 *
 *      http://www.iodbc.org
 *
 *
 *  GNU Library Generic Public License Version 2
 *  ============================================
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; only
 *  Version 2 of the License dated June 1991.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *
 *  The BSD License
 *  ===============
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *  3. Neither the name of OpenLink Software Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL OPENLINK OR
 *  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <iodbc.h>
#include <isql.h>
#include <odbcinst.h>
#include <unistd.h>
#include <stdlib.h>

#include "gui.h"


int
gtk_gui (int *argc, char **argv[])
{
  GtkWidget *mainwnd;
#if GTK_CHECK_VERSION(2,0,0)
  gtk_set_locale();
#endif
  gtk_init (argc, argv);
  mainwnd = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  return SQLManageDataSources (mainwnd);
}


int
kde_gui (int *argc, char **argv[])
{
  return -1;
}


void
display_help (void)
{
  printf ("-help\t\t\tDisplay the list of options.\n\r");
  printf
      ("-odbc filename\t\tSet the location of the user ODBC.INI file.\n\r");
  printf
      ("-odbcinst filename\tSet the location of the user ODBCINST.INI file.\n\r");
  printf
      ("-sysodbc filename\tSet the location of the system ODBC.INI file.\n\r");
  printf
      ("-sysodbcinst filename\tSet the location of the system ODBCINST.INI file.\n\r");
  printf ("-gui guitype\t\tSet the GUI type : GTK, KDE.\n\r");
  printf ("-debug\t\t\tThe error messages are displayed on the console.\n\r");
  printf
      ("-admin odbcinstfile\tUsed to administrate the system odbcinst.ini file.\n\r\n\r");
  _exit (1);
}


#if !defined(HAVE_SETENV)
static int
setenv (const char *name, const char *value, int overwrite)
{
  int rc;
  char *entry;

  /*
   *  Allocate some space for new environment variable
   */
  if ((entry = (char *) malloc (strlen (name) + strlen (value) + 2)) == NULL)
    return -1;
  strcpy (entry, name);
  strcat (entry, "=");
  strcat (entry, value);

  /*
   *  Check if variable already exists in current environment and whether
   *  we want to overwrite it with a new value if it exists.
   */
  if (getenv (name) != NULL && !overwrite)
    {
      free (entry);
      return 0;
    }

  /*
   *  Add the variable to the environment.
   */
  rc = putenv (entry);
  free (entry);
  return (rc == 0) ? 0 : -1;
}
#endif /* HAVE_SETENV */


int
main (int argc, char *argv[])
{
  BOOL debug = FALSE;
  char path[4096];
  char *gui = NULL;
  int i = 1;

  printf ("iODBC Administrator (GTK)\n");
  printf ("%s\n", PACKAGE_STRING);
  printf ("Copyright (C) 2000-2006 OpenLink Software\n");
  printf ("Please report all bugs to <%s>\n\n", PACKAGE_BUGREPORT);

  /* Check options commands */
  if (argc > 1)
    {
      for (; i < argc; i++)
	{
	  if (!strcasecmp (argv[i], "-help"))
	    display_help ();

	  if (!strcasecmp (argv[i], "-debug"))
	    debug = TRUE;

	  if (!strcasecmp (argv[i], "-odbc"))
	    {
	      if (i + 1 >= argc)
		display_help ();
	      setenv ("ODBCINI", argv[++i], TRUE);
	    }

	  if (!strcasecmp (argv[i], "-admin"))
	    {
	      if (i + 1 >= argc)
		display_help ();
	      setenv ("ODBCINSTINI", argv[++i], TRUE);
	      setenv ("SYSODBCINSTINI", argv[i], TRUE);
	    }

	  if (!strcasecmp (argv[i], "-odbcinst"))
	    {
	      if (i + 1 >= argc)
		display_help ();
	      setenv ("ODBCINSTINI", argv[++i], TRUE);
	    }

	  if (!strcasecmp (argv[i], "-sysodbc"))
	    {
	      if (i + 1 >= argc)
		display_help ();
	      setenv ("SYSODBCINI", argv[++i], TRUE);
	    }

	  if (!strcasecmp (argv[i], "-sysodbcinst"))
	    {
	      if (i + 1 >= argc)
		display_help ();
	      setenv ("SYSODBCINSTINI", argv[++i], TRUE);
	    }

	  if (!strcasecmp (argv[i], "-gui"))
	    {
	      if (i + 2 >= argc)
		display_help ();
	      gui = argv[++i];
	    }
	}
    }

  if (!getenv ("ODBCINI") && getenv ("HOME"))
    {
      STRCPY (path, getenv ("HOME"));
      STRCAT (path, "/.odbc.ini");
      setenv ("ODBCINI", path, TRUE);
    }

  if (!debug)
    {
      close (STDOUT_FILENO);
      close (STDERR_FILENO);
    }

  if (gui && !strcasecmp (gui, "KDE"))
    return kde_gui (&argc, &argv);

  return gtk_gui (&argc, &argv);
}
