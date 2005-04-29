From bryanh@giraffe.giraffe.netgate.net Sat Nov 16 09:32:59 1996
Received: from giraffe.giraffe.netgate.net by hera.cwi.nl with SMTP
	id <AA24735@cwi.nl>; Sat, 16 Nov 1996 09:32:53 +0100
Received: (from bryanh@localhost) by giraffe.giraffe.netgate.net (8.6.11/8.6.9) id AAA00639; Sat, 16 Nov 1996 00:32:46 -0800
Date: Sat, 16 Nov 1996 00:32:46 -0800
Message-Id: <199611160832.AAA00639@giraffe.giraffe.netgate.net>
From: bryanh@giraffe.netgate.net (Bryan Henderson)
To: Andries.Brouwer@cwi.nl
In-Reply-To: <9611151043.AA01606=aeb@zeus.cwi.nl> (Andries.Brouwer@cwi.nl)
Subject: Re: cross references for Linux man page package
Status: RO

>I hope a shell script or perl script?

Well, no.  Shell scripts are too hard and I don't know perl.  So it's in 
tortured C.  It also needs the shhopt package (from sunsite), which
effortlessly parses a command line, but not many people know about it.
So maybe this isn't up to distributions standards.


Here it is anyway.  You invoke it just like this:

  preformat ls.1

Or for a whole directory,

  preformat *

Or if you keep preformatted man pages elsewhere than /usr/man/preformat/catN,

  preformat --mandir=/usr/local/doc/package_xyz/man *

It makes the target directories where necessary and groffs and gzips the
man pages into them.  If it finds a man page that looks like ".so whatever",
it just does a symbolic link to the base file instead.

--------------------------------------------------------------------------
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <shhopt.h>

#define TRUE 1
#define FALSE 0



void
compute_mkdir_command(const char *installed_path, 
                      char *mkdir_cmd, const int mkdir_cmd_l) {
/*----------------------------------------------------------------------------
  Figure out what, if any, mkdir command we need to create the directories
  in which to put the file whose full pathname is <installed_path>.
----------------------------------------------------------------------------*/
  char *slash_p;  /* pointer to last slash in installed_path. */
  char need_dir[strlen(installed_path)+1];
  /* pathname of directory which must exist so we can install the man
     page into it.  If we're just defaulting to the current directory,
     then this is a null string.
     */

  slash_p = strrchr(installed_path, '/');
  if (slash_p == NULL) need_dir[0] = '\0';
  else {
    int need_dir_l;   /* length for need_dir */
    need_dir_l = slash_p - installed_path + 1;  /* includes slash */
    strncpy(need_dir, installed_path, need_dir_l);
    need_dir[need_dir_l] = '\0';   /* need that string terminator */
  }

  if (need_dir[0] == '\0')
    mkdir_cmd[0] = '\0';
  else {
    struct stat stat_buf;  /* results of a stat system call */
    int rc;  /* return code from stat() */

    rc = stat(need_dir, &stat_buf);
    if (rc == 0)
      mkdir_cmd[0] = '\0';
    else
      sprintf(mkdir_cmd, "umask 002;mkdir --parents %s; ", need_dir);
  }
}



void
extract_dot_so_stmt(const char *man_page_source_path, 
                    char *dot_so_stmt, const int dot_so_stmt_l) {
  
  FILE *source_file;

  source_file = fopen(man_page_source_path, "r");
  if (source_file != NULL) {
    char buffer[200];  /* First line of file */

    fgets(buffer, sizeof(buffer), source_file);
    fclose(source_file);

    if (strncmp(buffer, ".so ", 4) == 0)
      snprintf(dot_so_stmt, dot_so_stmt_l, "%s", buffer);
    else dot_so_stmt[0] = '\0';
  } else dot_so_stmt[0] = '\0';
}



void
format_page(const char *installed_path, const char *man_page_source_path,
            const char *mkdir_cmd, int *rc_p) {
/*----------------------------------------------------------------------------
  Format and compress the groff source in file <man_page_source_path>
  and put the output in <installed_path>.  Execute the possible mkdir
  command <mkdir_cmd> too.
-----------------------------------------------------------------------------*/
  char shell_command[100+strlen(installed_path) 
                     + strlen(man_page_source_path) 
                     + strlen(mkdir_cmd)];  
    /* A pipeline we have the shell execute */
  int rc; /* local return code */

  snprintf(shell_command, sizeof(shell_command), 
           "%sgroff -Tlatin1 -mandoc %s | gzip >%s",
           mkdir_cmd, man_page_source_path, installed_path);

  printf("%s\n", shell_command);
  rc = system(shell_command);
  if (rc != 0) {
    fprintf(stderr, "groff pipeline failed, rc = %d\n", rc);
    *rc_p = 10;
  } else {
    *rc_p = 0;
    chmod(installed_path, 
          S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH
          );
  }
}



void
create_symlink(const char *installed_path, const char *dot_so_stmt,
               const char *mkdir_cmd, int *rc_p) {
/*----------------------------------------------------------------------------
  Create a symlink from <installed_path> to the installed name of the man
  page identified by <dot_so_stmt>.

  We make some large assumptions about the .so statement, so this may return
  gibberish. 

  Execute the possible mkdir command <mkdir_cmd> too.
-----------------------------------------------------------------------------*/
  char shell_command[100+strlen(mkdir_cmd) +
                     strlen(installed_path) + 
                     strlen(dot_so_stmt)];  
    /* A pipeline we have the shell execute */
  int rc; /* local return code */
  char *slash_p;  /* pointer to last slash in .so statement, or NULL */

  slash_p = strrchr(dot_so_stmt, '/');
  if (slash_p == NULL) {
    fprintf(stderr, "Cannot find the base filename "
            "in the .so statement '%s'.  There is no slash ('/').\n",
            dot_so_stmt);
    *rc_p = 15;
  } else if (*(slash_p+1) == '\0') {
    fprintf(stderr, "Cannot find the base filename "
            "in the .so statement '%s'.  There is nothing after the "
            "last slash ('/').",
            dot_so_stmt);
    *rc_p = 13;
  } else {
    char link_contents[200];

    strncpy(link_contents, slash_p+1, sizeof(link_contents)-10);
    if (link_contents[strlen(link_contents)-1] == '\n')
      link_contents[strlen(link_contents)-1] = '\0';
    strcat(link_contents, ".gz");

    sprintf(shell_command, "%sln --symbolic %s %s",
            mkdir_cmd, link_contents, installed_path);

    printf("%s\n", shell_command);
    rc = system(shell_command);
    if (rc != 0) {
      fprintf(stderr, "ln pipeline failed, rc = %d\n", rc);
      *rc_p = 10;
    } else *rc_p = 0;
  }
}



void 
install_it(char *installed_path, char *man_page_source_path, int *rc_p){
/*----------------------------------------------------------------------------
  Take the man page groff source in file <man_page_source_path>, format
  it, compress it, and place it in file <installed_path>.

  Special case:  If the file appears to be just a groff .so statement, 
  don't format it; instead, create a symbolic link that will do the same
  thing as formatting the .so.  A .so statement looks like:

     .so man3/basepage.3

  and means to include all the groff source from the file man3/basepage.3.
  So we just create a symbolic link to cat3/basepage.3.gz and save some
  redundancy.


  Make any directories necessary to create file <installed_path>.

-----------------------------------------------------------------------------*/
  char mkdir_cmd[30 + strlen(installed_path)];
    /* A mkdir shell command to create the necessary directories.  Null
       string if no directory needs creating.
       */
  char dot_so_stmt[200];
    /* The .so statement from the man page source, if the man page appears
       to be one that consists solely of a .so statement.  If it doesn't
       appear so, this is an empty string.
       */

  /* We have to remove the file first, because it may be a symbolic link
     for the purposes of having the same man page come up for multiple
     commands.  If we just overwrite, we will be replacing the base file,
     which we don't want to do.
     */
  unlink(installed_path);  

  compute_mkdir_command(installed_path, mkdir_cmd, sizeof(mkdir_cmd));

  extract_dot_so_stmt(man_page_source_path, dot_so_stmt, sizeof(dot_so_stmt));

  if (*dot_so_stmt != '\0')
    create_symlink(installed_path, dot_so_stmt, mkdir_cmd, rc_p);
  else
    format_page(installed_path, man_page_source_path, mkdir_cmd, rc_p);
}



char *
just_filename(const char *full_path) {
/*----------------------------------------------------------------------------
  Return pointer into <full_path> of start of filename part.
  Return NULL if pathname ends with a slash (i.e. it's a directory).
-----------------------------------------------------------------------------*/
  char *slash_p;  /* Pointer to last slash in <full_path> */
  char *filename; /* Our eventual result */

  slash_p = strrchr(full_path, '/');
  if (slash_p == NULL) filename = (char *) full_path;
  else if (*(slash_p+1) == '\0') {
    filename = NULL;
  } else filename = slash_p+1;
  return(filename);
}
      


int main(int argc, char *argv[]) {
  char *mandir;
  /* The directory in which the formatted man pages are to go.  This is
     the parent directory of the cat1, cat2, etc. directories.  
     */
  char default_mandir[] = "/usr/man/preformat";  
    /* default value for mandir, if user doesn't give --mandir option */
  int error;   /* boolean: we've encountered an error */
  int i; /* local for loop index */

  const optStruct option_def[] = {
    { 0, (char *) "mandir", OPT_STRING, &mandir, 0},
    { 0, 0, OPT_END, 0, 0}
  };
  int argc_parse;       /* argc, except we modify it as we parse */
  char **argv_parse;    /* argv, except we modify it as we parse */

  mandir = default_mandir;  /* initial assumption - default */
  argc_parse = argc; argv_parse = argv;
  optParseOptions(&argc_parse, argv_parse, option_def, 0);
  /* uses and sets argc_parse, argv_parse.  */
  /* sets mandir (via option_def) */

  error = FALSE;   /* no error yet */

  for (i=1;i <= argc_parse-1 && !error; i++) {
    /* Do one of the man pages specified in the program arguments */
    char *man_page_source_path;  
      /* string: pathname of man page source file we're supposed to install 
       */
    char *man_page_source_fn;   /* pointer within pathname to filename */
    char *dot_p;   /* pointer within filename to last dot */

    char man_section; /* man section number to which this page belongs */
    char installed_path[100]; /* full pathname for installed man page file */

    man_page_source_path = argv_parse[i];  
  
    man_page_source_fn = just_filename(man_page_source_path);
    if (man_page_source_fn == NULL)
      fprintf(stderr, "Need filename at the end of pathname: %s\n",
              man_page_source_path);
    else {
      dot_p = strrchr(man_page_source_fn, '.');
      if (dot_p == NULL) {
        fprintf(stderr, "Invalid source file -- contains no period: %s\n",
                man_page_source_fn);
      } else if (*(dot_p+1) == '\0') {
        fprintf(stderr, "Invalid source file -- need at least one character "
                "after the last period: %s\n", man_page_source_fn);
      } else {
        int rc; /* local return code */
        /* Filename has a dot with at least one character after it.
           Manual section number is the character right after that dot.
           */
        man_section = *(dot_p+1);
      
        sprintf(installed_path, "%s/cat%c/%s.gz",
                mandir, man_section, man_page_source_fn);

        install_it(installed_path, man_page_source_path, &rc);
        if (rc != 0) error = TRUE;
      }
    }
  } 
  return(error);
}

