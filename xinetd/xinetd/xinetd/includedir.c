/*
 * (c) Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms
 * and conditions for redistribution.
 */
#include "config.h"
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "pset.h"
#include "str.h"
#include "includedir.h"
#include "msg.h"
#include "parse.h"
#include "sio.h"

#if !defined(NAME_MAX)
   #ifdef FILENAME_MAX
   #define NAME_MAX FILENAME_MAX
   #else
   #define NAME_MAX 256
   #endif
#endif

static int compfunc( const void *_a, const void *_b )
{
   char **a = (char **)_a;
   char **b = (char **)_b;

   if( a == NULL || a[0] == NULL )
      return -1;
   if( b == NULL || b[0] == NULL )
      return 1;
   return strcmp(a[0], b[0]);
}

void handle_includedir(const char *service_name, struct configuration *confp)
{
   char *filename;
   pset_h dir_list;
   DIR *dirfp;
   struct dirent *direntry;
   char *storename;
   struct stat sb;
   int u, incfd, len_sn;
   const char *func = "handle_includedir";

   if( service_name == NULL )
      return;

   dir_list = pset_create(0, 0);
   if( dir_list == NULL )
      return;

   len_sn = strlen(service_name);
   filename = (char *)malloc(len_sn + NAME_MAX + 2);
   if (! filename) {
      parsemsg( LOG_ERR, func, ES_NOMEM );
      return;
   }
   errno = 0;
   dirfp = opendir(service_name);
   if (! dirfp) {
      parsemsg( LOG_ERR, func, "Unable to read included directory: %s", service_name);
      free(filename);
      return;
   }
   /* Get the list of files in the directory */
   while ((direntry = readdir(dirfp)) != 0) {
      storename = new_string(direntry->d_name);
      if( storename == NULL ) {
         parsemsg( LOG_ERR, func, ES_NOMEM );
         free( filename );
         return;
      }
      pset_add(dir_list, storename);
   }
   closedir(dirfp);

   /* Sort the list using "compfunc" */
   pset_sort(dir_list, compfunc);

   /* Now, traverse the list in alphabetic order 
    * (as determined by strcmp).
    */
   for( u = 0; u < pset_count(dir_list); u++ ) {
      storename = pset_pointer(dir_list, u);

      /* Don't try to parse any files containing a dot ('.')
      * or ending with a tilde ('~'). This catches the case of 
      * '.' and '..', as well as preventing the parsing of 
      * many editor files, temporary files and those saved by RPM
      * package upgrades.
      */
      if ( !storename[0] /* Shouldn't happen */ || 
                      strchr(storename, '.') ||
                      storename[strlen(storename)-1] == '~') {
         pset_remove(dir_list, storename);
         free(storename);
         u--;
         continue;
      }

      strx_sprint(filename, len_sn+NAME_MAX+1, "%s/%s",
         service_name, storename);

      if( stat(filename, &sb) < 0 ) {
         parsemsg( LOG_ERR, func, "Unable to stat includedir file %s", filename);
         pset_remove(dir_list, storename);
         free(storename);
         u--;
         continue;
      }

      /* Only open it if it's a regular file. */
      if( !S_ISREG(sb.st_mode) ) {
          msg( LOG_ERR, func,
         "%s is not a regular file. It is being skipped.",
         filename );
          pset_remove(dir_list, storename);
          free(storename);
          u--;
          continue;
      }
      incfd = open(filename, O_RDONLY);
      if( incfd < 0 ) {
         parsemsg( LOG_ERR, func, "Unable to open included configuration file: %s", filename);
         pset_remove(dir_list, storename);
         free(storename);
         u--;
         continue;
      }
      parsemsg( LOG_DEBUG,func,"Reading included configuration file: %s",filename);
      parse_conf_file(incfd, confp);
      Sclose(incfd);
      pset_remove(dir_list, storename);
      free(storename);
      u--;
   }
   if ( errno != 0) {
      parsemsg( LOG_ERR, func, "Error reading included directory: %s", service_name);
   }
   pset_destroy(dir_list);
   free(filename);
}
