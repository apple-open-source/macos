/* Call Windows NT 3.x linker.
   Copyright (C) 1994 Free Software Foundation, Inc.
   Contributed by Douglas B. Rupp (drupp@cs.washington.edu).

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "config.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <process.h>
#include <stdarg.h>
#if 0
#include <varargs.h>
#endif
#include <fcntl.h>
#if defined (_WIN32) && defined (NEXT_PDO)
#include <signal.h>
#endif /* defined (_WIN32) && defined (NEXT_PDO) */

#ifdef __GNUC__
#define spawnvp _spawnvp
#define stat _stat
#define strdup _strdup
#endif __GNUC__

static char *progname = NULL;

/* These can be set by command line arguments */
char *linker_path = 0;
int verbose = 0;

int link_arg_max = -1;
char **link_args = (char **) 0;
int link_arg_index = -1;

int lib_arg_max = -1;
char **lib_args = (char **)0;
int lib_arg_index = -1;

char *search_dirs = 0;

#if defined (_WIN32) && defined (NEXT_PDO)
char *standard_framework_dirs[] = {
	"/Local/Library/Frameworks/", 
	"/Library/Frameworks/",
	NULL
};

char **framework_dirs = NULL;
unsigned long nframework_dirs = 0;

char **framework_names = NULL;
unsigned long nframework_names = 0;

static void cleanup (int);
#endif /* defined (_WIN32) && defined (NEXT_PDO) */

static int is_regular_file (char *name);

static void
addarg (str)
     char *str;
{
  int i;

  if (++link_arg_index >= link_arg_max)
    {
      char **new_link_args
	= (char **) calloc (link_arg_max + 1000, sizeof (char *));

      for (i = 0; i <= link_arg_max; i++)
	new_link_args [i] = link_args [i];

      if (link_args)
	free (link_args);

      link_arg_max += 1000;
      link_args = new_link_args;
    }

  if(str != NULL)
    {
      int has_spaces;

      has_spaces = 0;
      for(i = 0; has_spaces == 0 && str[i] != '\0'; i++)
	has_spaces = isspace(str[i]);

      if(has_spaces)
	{
	  char *p;

	    p = (char *)malloc(strlen(str) + 3);
	    strcpy(p, "\"");
	    strcat(p, str);
	    strcat(p, "\"");
	    link_args [link_arg_index] = p;
	}
      else
	link_args [link_arg_index] = str;
    }
  else
    link_args [link_arg_index] = str;
}

/*
 * libargs() is called when when are running "link.exe /LIB" and takes the
 * arguments in link_args and removed the arguments that cause warnings.
 * The new arguments are left in lib_args with lib_arg_index and lib_arg_max
 * set as link_arg_index and link_arg_max are set.
 */
static void
libargs (void)
{
  int i, j;
  char *p;

  lib_arg_max = link_arg_max;
  lib_args = calloc (lib_arg_max, sizeof (char *));

  for (i = 0; i <= link_arg_index; i++)
    {
      if (link_args [i] != NULL)
	{
	  if (strncmp(link_args [i], "-align", strlen("-align")) != 0 &&
	      strncmp(link_args [i], "/BASE",  strlen("/BASE"))  != 0 &&
	      strncmp(link_args [i], "-debug", strlen("-debug")) != 0)
	    {
		lib_args [++lib_arg_index] = link_args [i];
	    }
	}
      else
	{
	  lib_args [++lib_arg_index] = link_args [i];
	  break;
	}
    }
}

static char *
locate_file (file_name, path_val)
     char *file_name;
     char *path_val;
{
  char buf [1000];
  int file_len = strlen (file_name);
  char *end_path = path_val + strlen (path_val);
  char *ptr;

  /* Handle absolute pathnames */
  if (file_name [0] == '/' || file_name [0] == DIR_SEPARATOR
      || isalpha (file_name [0]) && file_name [1] == ':')
    {
      strncpy (buf, file_name, sizeof buf);
      buf[sizeof buf - 1] = '\0';
      if (is_regular_file (buf))
	return strdup (buf);
      else
	return 0;
  }

  if (! path_val)
    return 0;

  for (;;)
    {
      for (; *path_val == PATH_SEPARATOR ; path_val++)
	;
      if (! *path_val)
	return 0;

      for (ptr = buf; *path_val && *path_val != PATH_SEPARATOR; )
	*ptr++ = *path_val++;

      ptr--;
      if (*ptr != '/' && *ptr != DIR_SEPARATOR)
	*++ptr = DIR_SEPARATOR;

      strcpy (++ptr, file_name);

      if (is_regular_file (buf))
	return strdup (buf);
    }

  return 0;
}

static char *
expand_lib (name)
     char *name;
{
  char *lib, *lib_path;

  lib = malloc (strlen (name) + 6);
  strcpy (lib, "lib");
  strcat (lib, name);
  strcat (lib, ".a");


  lib_path = locate_file (lib, search_dirs);
  if (!lib_path)
    {
#if defined (_WIN32) && defined (NEXT_PDO)
      char *lib2;
      lib2 = (char *) malloc (strlen (name) + 5);
      strcpy (lib2, name);
      strcat (lib2, ".lib");
      lib_path = locate_file (lib2, search_dirs);
      if (!lib_path)
	{
	  fprintf (stderr, "%s: couldn't locate library: %s or %s\n", progname,
		   lib, lib2);
	  free (lib2);
	  cleanup (0);
	  exit (FATAL_EXIT_CODE);
	}
      free (lib2);
#else
      fprintf (stderr, "%s: couldn't locate library: %s\n", progname, lib);
      cleanup (0);
      exit (-1);
#endif /* _WIN32 */
    }

  return lib_path;



}

static int
is_regular_file (name)
     char *name;
{
  int ret;
  struct _stat statbuf;

  ret = stat(name, &statbuf);
  return !ret && S_ISREG (statbuf.st_mode);
}

static int
is_directory (name)
     char *name;
{
  int ret;
  struct _stat statbuf;

  ret = stat(name, &statbuf);
  return !ret && S_ISDIR (statbuf.st_mode);
}

static void
process_args (p_argc, argv)
     int *p_argc;
     char *argv[];
{
  int i, j;

  for (i = 1; i < *p_argc; i++)
    {
      /* -v turns on verbose option here and is passed on to gcc */
      if (! strcmp (argv [i], "-v"))
	verbose = 1;
    }
}




/*
 * search_for_framework() takes name and tries to open a file with that name
 * in the -F search directories and in the standard framework directories.  If
 * it is sucessful it returns a pointer to the file name indirectly through
 * file_name and the open file descriptor indirectly through fd.
 */
static
void
search_for_framework(
char *name,
char **file_name,
int *fd)
{
    unsigned long i;
    char *p;

	*fd = -1;
	*file_name = NULL;
	for(i = 0; i < nframework_dirs ; i++){
		*file_name = (char*)malloc (strlen(framework_dirs[i]) + strlen(name) + 6);
		// First try with a .lib on it
		sprintf (*file_name, "%s%s%s.lib", framework_dirs[i], "/", name);
	    if((*fd = open(*file_name, O_RDONLY, 0)) != -1)
		{
			close(*fd);
			break;
		}
		// Then try with nothing.
		sprintf (*file_name, "%s%s%s.", framework_dirs[i], "/", name);
	    if((*fd = open(*file_name, O_RDONLY, 0)) != -1)
		{
			close(*fd);
			break;
		}
	    free(*file_name);
	    *file_name = NULL;
	}
	if(*fd == -1){
	    for(i = 0; standard_framework_dirs[i] != NULL ; i++){
		*file_name = (char*)malloc (strlen(standard_framework_dirs[i]) + strlen(name) + 5);
		// First try with a .lib on it
		sprintf (*file_name, "%s%s.lib", standard_framework_dirs[i], name);
		if((*fd = open(*file_name, O_RDONLY, 0)) != -1)
		{
		    close(*fd);
			break;
		}
		// Then try with nothing.
		sprintf (*file_name, "%s%s.", standard_framework_dirs[i], name);
		if((*fd = open(*file_name, O_RDONLY, 0)) != -1)
		{
		    close(*fd);
			break;
		}
		free(*file_name);
		*file_name = NULL;
	    }
	}
	if(*file_name != NULL){
	    for(p = *file_name; *p != '\0'; p++)
		if(*p == '/')
		    *p = '\\';
	}
}


#if defined (_WIN32) && defined (NEXT_PDO)
/*
 * The names of temporary @ files to expand -filelists argument with ,dirname
 * options into and count of them (needed to clean them up).
 */
static char **tmp_at_filenames = NULL;
static int ntmp_at_filenames = 0;

static
void
cleanup(
int sig)
{
  int i;

	for(i = 0; i < ntmp_at_filenames; i++)
	    (void)remove(tmp_at_filenames[i]);
}

/*
 * add_to_tmp_at_file() takes a pointer to the string which is the -filelist
 * argument.  This argument may look like "listfile,dirname" where listfile is
 * the name of a file containing object file names one to a line and dirname is
 * an optional directory name to be prepend to each object file name.  This
 * routine writes the object file names (with the dirname) into a temporary
 * file.  The temporary file name with an @ symbol prepened to is then returned
 * so it can be added to argument list to link as @tmpfile.
 */
static
char *
add_to_tmp_at_file(
char *filelist_arg)
{
    char *comma, *dirname, *tmppath, filename[FILENAME_MAX + 2], *p;
    int dirsize, filesize;
    int offset;
    FILE *stream, *tmp_at_file;
    char *tmp_at_filename;

	dirsize = 0;
	comma = strrchr(filelist_arg, ',');
	if(comma != NULL){
	    *comma = '\0';
	    dirname = comma + 1;
	    dirsize = strlen(dirname);
	    if(dirsize + 2 > FILENAME_MAX){
		*comma = ',';
		fprintf(stderr, "%s: directory name too long for -filelist "
			"argument: %s\n", progname, filelist_arg);
		cleanup(0);
		exit(FATAL_EXIT_CODE);
	    } 
	    strcpy(filename, dirname);
	    if(filename[dirsize] != '/' ||
	       filename[dirsize] != DIR_SEPARATOR){
	       filename[dirsize] = DIR_SEPARATOR;
	       filename[++dirsize] = '\0';
	       
	    }
	}

	stream = fopen(filelist_arg, "r");
	if(stream == NULL){
	    fprintf(stderr, "%s: can't open -filelist argument: %s\n",
		    progname, filelist_arg);
	    cleanup(0);
	    exit(FATAL_EXIT_CODE);
	}

	if(comma != NULL)
	    *comma = ',';

	tmp_at_filename = (char *)malloc(L_tmpnam + 1);
	tmp_at_filename[0] = '@';
	tmpnam(tmp_at_filename+1);
	tmp_at_file = fopen(tmp_at_filename+1, "w");
	if(tmp_at_file == NULL){
	    fprintf(stderr, "%s: can't create temporary file: %s to expand "
		    "-filelist %s argument\n", progname, tmp_at_filename+1,
		    filelist_arg);
	    cleanup(0);
	    exit(FATAL_EXIT_CODE);
	}

	tmp_at_filenames = (char **)realloc(tmp_at_filenames,
				    sizeof(char *) * (ntmp_at_filenames+1));
	tmp_at_filenames[ntmp_at_filenames++] = tmp_at_filename+1;

	while(fgets(filename+dirsize, FILENAME_MAX+2-dirsize, stream) != NULL){
	    filesize = strlen(filename);
	    if(filesize > FILENAME_MAX &&
	      (filesize != FILENAME_MAX+1 || filename[filesize-1] != '\n')){
		fprintf(stderr, "%s: file name constructed from -filelist %s "
			"argument too long (%s)\n", progname, filelist_arg,
			filename);
		cleanup(0);
		exit(FATAL_EXIT_CODE);
	    }
	    for(p = filename; *p != '\0'; p++)
		if(*p == '/')
		    *p = DIR_SEPARATOR;
	    if(fputs(filename, tmp_at_file) == EOF){
		fprintf(stderr, "%s: can't write to temporary file: %s to "
			"expand -filelist %s argument\n", progname,
			tmp_at_filename+1,
			filelist_arg);
		cleanup(0);
		exit(FATAL_EXIT_CODE);
	    }
	}

	if(fclose (tmp_at_file) == EOF){
	    fprintf(stderr, "%s: can't close temporary file: %s to expand "
		    "-filelist %s argument\n", progname, tmp_at_filename+1,
		    filelist_arg);
	    cleanup(0);
	    exit(FATAL_EXIT_CODE);
	}
	return(tmp_at_filename);
}
#endif /* defined (_WIN32) && defined (NEXT_PDO) */

enum tristate {
    DEFAULT,
    ON,
    OFF
};

/*
 * set_gvalue() takes a pointer to a -g flag and a pointer into that flag
 * where the number part would start and the pointer to the corresponding
 * gvalue.  It parses out the number part and sets the gvalue.  If there is no
 * number the default value is 2.  It prints an error and exits for invalid
 * arguments or a previously conflicting flag.
 */
static
void
set_gvalue(
char *gflag,
char *number_string,
enum tristate *gvalue)
{
    unsigned int number;
    char *endp;

	if(*number_string == '\0'){
	    number = 2;
	}
	else{
	    number = strtoul(number_string, &endp, 10);
	    if(*endp != '\0'){
		fprintf(stderr, "%s: invalid %s argument\n", progname, gflag);
		cleanup(0);
		exit(FATAL_EXIT_CODE);
	    }
	}
	if(*gvalue == DEFAULT){
	    if(number == 0)
		*gvalue = OFF;
	    else
		*gvalue = ON;
	}
	else{
	    if((*gvalue == ON && number == 0) ||
	       (*gvalue == OFF && number != 0)){
		fprintf(stderr, "%s: conflicting -g[gdb,codeview,stab][number] "
			"arguments specified\n", progname);
		cleanup(0);
		exit(FATAL_EXIT_CODE);
	    }
	}
}


main (argc, argv)
     int argc;
     char *argv[];
{
  int i;
  int done_an_ali = 0;
  int file_name_index;
  enum tristate gdb = DEFAULT, codeview = DEFAULT;
  int debug_flags_specified = 0;
  int undefined_flag = 0; /* records the value of the -undefined switch */
  char *pathval = getenv ("PATH");
  char *spawn_args [5];
  char *tmppathval = malloc (strlen (pathval) + 3);

#if defined (_WIN32) && defined (NEXT_PDO)
  char *next_root = getenv( "NEXT_ROOT" );
  int lib_seen = 0;
  int dll_seen = 0;
  int dll_arg_index = -1;
  int out_arg_index = -1;
  int def_arg_index = -1;

  (void)signal(SIGINT, cleanup);
  (void)signal(SIGTERM, cleanup);

  if (next_root && *next_root)
    {
      if (next_root[strlen(next_root)-1] == '/')
	next_root[strlen(next_root)-1] = '\0';
      for (i = 0; standard_framework_dirs[i]; i++)
	{
	  char* new_fname =
	    (char *) malloc (strlen (next_root)
			     + strlen (standard_framework_dirs[i]) + 1);
	  sprintf (new_fname, "%s%s", next_root, standard_framework_dirs[i]);
	  if (new_fname && *new_fname)
	    standard_framework_dirs[i] = new_fname;
	}
    }
#endif

  progname = argv[0];

  /* Set up the search dirs variable */
  search_dirs = (char*)malloc( strlen( getenv ("LIB") ) + 1 );
  strcpy( search_dirs, getenv ("LIB") );
  strcat( search_dirs, "\0" );

  strcpy (tmppathval, ".;");
  pathval = strcat (tmppathval, pathval);

  process_args (&argc , argv);

  linker_path = locate_file ("link32.exe", pathval);
  if (!linker_path)
    {
      linker_path = locate_file ("link.exe", pathval);
      if (!linker_path)
	{
	  fprintf (stderr, "%s: couldn't locate link32 or link\n", progname);
	  cleanup(0);
	  exit (FATAL_EXIT_CODE);
	}
    }

  addarg (linker_path);

#if defined (_WIN32) && defined (NEXT_PDO)
  /*
   * To make sure we don't alter the position of object files (including
   * libraries and frameworks) we must make two passes through the argument
   * list.  On the first pass we build up the list of -L's and -F's.
   */
  for (i = 1; i < argc; i++)
    {
      int arg_len = strlen (argv [i]);

      if (arg_len > 2 && !strncmp (argv [i], "-L", 2))
	{
	  char *nbuff, *sdbuff;
	  int j, new_len, search_dirs_len;

	  new_len = strlen (&argv[i][2]);
	  search_dirs_len = strlen (search_dirs);

	  nbuff = malloc (new_len + 1);
	  strcpy (nbuff, &argv[i][2]);

	  for (j = 0; j < new_len; j++)
	    if (nbuff[j] == '/')
	      nbuff[j] = DIR_SEPARATOR;

	  sdbuff = malloc (search_dirs_len + new_len + 2);
	  strcpy (sdbuff, search_dirs);
	  sdbuff[search_dirs_len] = PATH_SEPARATOR;
	  sdbuff[search_dirs_len+1] = 0;
	  strcat (sdbuff, nbuff);

	  if (search_dirs)
	    free (search_dirs);

	  search_dirs = sdbuff;
	}
      else if (!strncmp (argv [i], "-F", 2))
	{
	  if (argv[i][2] == '\0')
	    {
	      fprintf (stderr, "%s: error: -F: directory name missing\n",
		       progname);
	      cleanup (0);
	      exit (FATAL_EXIT_CODE);
	    }
  
	  framework_dirs = (char**)realloc (framework_dirs,
			   (nframework_dirs + 1) * sizeof (char*));
	  framework_dirs[nframework_dirs++] = &(argv[i][2]);
	  if (!is_directory (&(argv[i][2])))
	    {
	      fprintf (stderr, "%s: warning: -F: directory name (%s) does not "
		       "exist\n", progname, &(argv[i][2]));
	    }
	}
      else if (arg_len >= 2 && !strncmp (argv [i], "-g", 2))
        {
	  /*
	   * The possible -g flags are as follows:
	   *
	   * -ggdb[number]
	   * -gcodeview[number]
	   * -gstabs[number] (treated the same as -ggdb)
	   * -g[number] (treated the same as -ggdb)
	   *
	   * If no number is present it value is 2.  For this implementation
	   * the values of [number] fall into two classes zero (turn it off)
	   * and non-zero (turn it on).  By default gdb is on and codeview is
	   * off.  set_value() handles errors in [number] and if a value is
	   * specified as both on and off.
	   */
	  if (arg_len > 2)
	    {
	      if (!strncmp (argv[i]+2, "gdb", 3))
		set_gvalue(argv[i], argv[i]+5, &gdb);
	      else if (!strncmp (argv[i]+2, "codeview", 8))
		set_gvalue(argv[i], argv[i]+10, &codeview);
	      else if (!strncmp (argv[i]+2, "stab", 4))
		set_gvalue(argv[i], argv[i]+6, &gdb);
	      else
		set_gvalue(argv[i], argv[i]+2, &gdb);
	    }
	  else
	    set_gvalue(argv[i], argv[i]+2, &gdb);
	}
      else if (arg_len >= 5 && !strncmp (argv [i], "-debug", 6))
	{
	  /*
	   * If any -debug or -debugtype flags are specified then we will
	   * honor them and never add any of our own for -g flags.
	   */
	  debug_flags_specified = 1;
	}
      else if (!strcmp (argv [i], "-o") ||
	       !strcmp (argv [i], "-framework") ||
	       !strcmp (argv [i], "-undefined") ||
	       !strcmp (argv [i], "-image_base") ||
	       !strcmp (argv [i], "-filelist"))
	{
	  if (i + 1 >= argc)
	    {
	      fprintf (stderr, "%s: error: %s: argument missing\n", progname,
		       argv [i]);
	      cleanup (0);
	      exit (FATAL_EXIT_CODE);
	    }
	  i++;
	}
      else if (!strcmp (argv [i], "-lib") ||
	       !strcmp (argv [i], "/LIB"))
	{
	  lib_seen = 1;
	}
      else if (!strcmp (argv [i], "-dll") ||
	       !strcmp (argv [i], "/DLL"))
	{
	  dll_seen = 1;
	}
      else if (!strncmp (argv [i], "-def:", strlen("-def:")) ||
               !strncmp (argv [i], "-DEF:", strlen("-DEF:")))
	{
	  def_arg_index = link_arg_index;
	}
    }

  /* This must the the first argument if seen */
  if (lib_seen == 1)
    {
      addarg ("/LIB");
      dll_seen = 0;
    }
  else if (dll_seen && def_arg_index != -1)
    {
      addarg ("/LIB");
      dll_arg_index = link_arg_index;
    }

#endif /* defined (_WIN32) && defined (NEXT_PDO) */

  for (i = 1; i < argc; i++)
    {
      int arg_len = strlen (argv [i]);

      if (!strcmp (argv [i], "-o"))
	{
	  char *buff, *ptr;
	  int out_len;

	  i++;
	  out_len = strlen (argv[i]) + 10;
	  buff = malloc (out_len);
	  strcpy (buff, "-out:");
	  strcat (buff, argv[i]);
	  ptr = strstr (buff, ".exe");
	  if( ptr == NULL )
	  	ptr = strstr( buff, ".dll" );
	  if (ptr == NULL || strlen (ptr) != 4)
	    strcat (buff, ".exe");
	  addarg (buff);
#if defined (_WIN32) && defined (NEXT_PDO)
	  if (ptr == NULL || !strcmp (ptr, ".exe"))
	    /* Note that this only increases the stack size if the
	       name of the executable is specified on the command line.  */
	    addarg ("-stack:4194304");
	  if (dll_seen && def_arg_index != -1)
	    {
	      if (ptr != NULL && strcmp(ptr, ".dll") == 0)
		{
		  strcpy(ptr, ".lib");
		  out_arg_index = link_arg_index;
		}
	      else
		{
		  link_args[dll_arg_index] = "/DLL";
		  dll_seen = 0;
		}
	    }
#endif /* defined (_WIN32) && defined (NEXT_PDO) */
	}
      else if (arg_len >= 2 && !strncmp (argv [i], "-g", 2))
        {
	  /* Picked up this argument in the first pass */
        }
      else if (arg_len > 2 && !strncmp (argv [i], "-L", 2))
	{
	  /* Picked up this argument in the first pass */
	}
      else if (arg_len > 2 && !strncmp (argv [i], "-l", 2))
	{
	  addarg (expand_lib (&argv[i][2]));
	}
#if defined (_WIN32) && defined (NEXT_PDO)
      else if (!strncmp (argv [i], "-def:", strlen("-def:")) ||
               !strncmp (argv [i], "-DEF:", strlen("-DEF:")))
	{
	  addarg (argv [i]);
	  def_arg_index = link_arg_index;
	}
      else if (!strcmp (argv [i], "-lib") ||
	       !strcmp (argv [i], "/LIB"))
	{
	  /* Picked up this argument in the first pass */
	}
      else if (!strcmp (argv [i], "-dll") ||
	       !strcmp (argv [i], "/DLL"))
	{
	  /* Picked up this argument in the first pass,
	     but pass it on through if we didn't encounter a -def */
	  if (def_arg_index == -1)
	    addarg (argv [i]);
	}
      else if (!strcmp (argv [i], "-image_base"))
	{
	  char *p;

	    i++;
	    p = (char*)malloc (strlen(argv[i]) + 7);
	    strcpy (p, "/BASE:");
	    strcat (p, argv[i]);
	    addarg (p);
	}
      else if (!strcmp (argv [i], "-framework"))
	{
	  char *type;
	  char *p;
	  char *file_name;
	  int count;
	  int fd = -1;			// not used, only for return value;

	  i++;
	  fd = -1;
	  type = strrchr(argv[i], ',');
	  if (type != NULL && type[1] != '\0')
	    {
	      *type = '\0';
	      type++;
	      p = (char*)malloc ((strlen(argv[i])*2) + 11 + strlen(type) + 2);
	      sprintf (p, "%s%s%s%s", argv[i], ".framework/", argv[i], type);
	      search_for_framework(p, &file_name, &fd);
	      if (fd == -1)
		{
		  fprintf(stderr, "%s: can't locate framework link library for:"
			  " -framework %s,%s using suffix %s\n", progname,
			  argv[i], type, type);
		}
	      else
		addarg (file_name);
	    }
	  else
	    type = NULL;
	  if (fd == -1)
	    {
	      p = (char*)malloc ((strlen(argv[i])*2) + 11 + 2);
	      sprintf (p, "%s%s%s", argv[i], ".framework/", argv[i]);
	      search_for_framework (p, &file_name, &fd);
	    }
	  if (fd == -1)
	    {
	      if (type != NULL)
		{
		  fprintf (stderr, "%s: can't locate framework link library "
			   "for: -framework %s,%s\n", progname, argv[i], type);
		  cleanup (0);
		  exit(FATAL_EXIT_CODE);
		}
	      else
		{
		  fprintf (stderr, "%s: can't locate framework link library "
			   "for: -framework %s\n", progname, argv[i]);
		  cleanup (0);
		  exit(FATAL_EXIT_CODE);
		}
	    }
	  else
	    addarg (file_name);
	}
      else if (!strncmp (argv [i], "-F", 2))
	{
	  /* Picked up this argument in the first pass */
	}
      else if (!strcmp (argv[i], "-undefined"))
	{
	  if (++i >= argc)
	    {
	      fprintf (stderr, "%s: error: Argument to -undefined missing\n",
		       progname);
	      cleanup (0);
	      exit (FATAL_EXIT_CODE);
	    }
	  if (!strcmp (argv[i], "warning") || !strcmp (argv[i], "suppress"))
	    {
	      if (!undefined_flag)
		{
		  undefined_flag = 1;
		  addarg ("-force:unresolved");
		}
	      else if (undefined_flag != 1)
		{
		  fprintf (stderr, "%s: error: Multiple conflicting -undefined "
			   "flags\n", progname);
		  cleanup (0);
		  exit (FATAL_EXIT_CODE);
		}
	    }
	  else if (!strcmp (argv[i], "error"))
	    {
	      if (undefined_flag > 0)
		{
		  fprintf (stderr, "%s: error: Multiple conflicting -undefined "
			   "flags\n", progname);
		  cleanup (0);
		  exit (FATAL_EXIT_CODE);
		}
	      undefined_flag = -1;
	    }
	  else
	    {
	      fprintf (stderr, "%s: error: unrecognized option \"-undefined "
		       "%s\"\n", progname, argv[i]);
	      cleanup (0);
	      exit (FATAL_EXIT_CODE);
	    }
	}
      else if (!strcmp (argv[i], "-filelist"))
	{
	  if (++i >= argc)
	    {
	      fprintf (stderr, "%s: error: Argument to -filelist missing\n",
		       progname);
	      cleanup (0);
	      exit (FATAL_EXIT_CODE);
	    }
	  if (strrchr(argv[i], ',') == NULL)
	    {
		char *at_arg = (char *) malloc (strlen (argv[i]) + 2);
		strcpy (at_arg, "@");
		strcat (at_arg, argv[i]);
		addarg (at_arg);
	    }
	  else
	    {
		addarg (add_to_tmp_at_file (argv [i]));
	    }
	}
#endif
    else if (!strcmp (argv [i], "-v")) 
      ;
    else
      addarg (argv [i]);
    }

  if (debug_flags_specified == 0)
    {
	if (gdb == DEFAULT)
	  gdb = ON;
	if (codeview == DEFAULT)
	  codeview = OFF;
	if (gdb == ON)
	  {
	    if (codeview == ON)
	      {
		addarg ("-debug");
		addarg ("-debugtype:both");
		addarg ("-pdb:none");
	      }
	    else
	      {
		addarg ("-debug");
		addarg ("-debugtype:coff");
	      }
	  }
	else
	  {
	    if (codeview == ON)
	      {
		addarg ("-debug");
		addarg ("-debugtype:both");
		addarg ("-pdb:none");
	      }
	    else
	      {
		; /* no flags */
	      }
	  }
    }

  addarg (NULL);

#if defined (_WIN32) && defined (NEXT_PDO)
      if (dll_seen && def_arg_index != -1)
	libargs();
#endif /* defined (_WIN32) && defined (NEXT_PDO) */

  if (verbose)
    {
      int i;
#if defined (_WIN32) && defined (NEXT_PDO)
      if (dll_seen && def_arg_index != -1)
	{
	  for (i = 0; i < lib_arg_index; i++)
	    printf ("%s ", lib_args [i]);
	  putchar ('\n');
	}
      else
#endif /* defined (_WIN32) && defined (NEXT_PDO) */
      for (i = 0; i < link_arg_index; i++)
	printf ("%s ", link_args [i]);
      putchar ('\n');
    }

#if defined (_WIN32) && defined (NEXT_PDO)
  if (dll_seen && def_arg_index != -1)
    {
      if (spawnvp (P_WAIT, linker_path, (const char * const *)lib_args) != 0)
        {
	  cleanup(0);
	  exit (FATAL_EXIT_CODE);
        }
    }
  else
#endif /* defined (_WIN32) && defined (NEXT_PDO) */

  if (spawnvp (P_WAIT, linker_path, (const char * const *)link_args) != 0)
    {
//    fprintf (stderr, "%s: error executing %s\n", progname, link_args[0]);
#if defined (_WIN32) && defined (NEXT_PDO)
      cleanup(0);
      exit (FATAL_EXIT_CODE);
#else
      exit (-1);
#endif /* defined (_WIN32) && defined (NEXT_PDO) */
    }

#if defined (_WIN32) && defined (NEXT_PDO)
  if (dll_seen && def_arg_index != -1)
    {
      char *buff, *ptr;
      int out_len;

      link_args[dll_arg_index] = "/DLL";

      ptr = strstr (link_args[out_arg_index], ".lib");
      strcpy (ptr, ".dll");

      out_len = strlen (link_args[out_arg_index]);
      buff = malloc (out_len);
      strcpy (buff, link_args[out_arg_index] + strlen ("-out:"));
      ptr = strstr (buff, ".dll" );
      strcpy (ptr, ".exp");
      if (def_arg_index != -1)
	link_args[def_arg_index] = buff;
      else
	addarg (buff);

    if (verbose)
      {
	int i;

	for (i = 0; i < link_arg_index; i++)
	  printf ("%s ", link_args [i]);
	putchar ('\n');
      }

    if (spawnvp (P_WAIT, linker_path, (const char * const *)link_args) != 0)
      {
	fprintf (stderr, "%s: error executing %s\n", progname, link_args[0]);
	cleanup(0);
	exit (FATAL_EXIT_CODE);
      }
    }
  cleanup(0);
#endif /* defined (_WIN32) && defined (NEXT_PDO) */

  exit (0);
}
