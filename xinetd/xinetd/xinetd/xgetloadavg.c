/*
 * (c) Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms
 * and conditions for redistribution.
 */
/* This file contains OS dependant implementations of xgetloadavg().
 * xgetloadavg takes no arguments and simply returns the 1 minute
 * load average of the machine as a double.
 * Feel free to add implementations here, please update configure.in
 * to define HAVE_LOADAVG.  Defining this macro enables the option
 * in the rest of the code.
 *    --Rob
 */

#include "config.h"
#ifdef HAVE_LOADAVG

#ifdef linux
#include <stdio.h>
#include "xgetloadavg.h"

#define LFILE "/proc/loadavg"
#define MAX 32

double xgetloadavg(void)
{
   FILE *fd;
   double ret = 0;

   fd = fopen(LFILE, "r");
   if( fd == NULL ) {
      return -1;
   }

   if( fscanf(fd, "%lf", &ret) != 1 ) {
      perror("fscanf");
      return -1;
   }

   fclose(fd);

   return ret;
}
#endif /* linux */


#ifdef solaris
#ifdef HAVE_KSTAT_H
#include <kstat.h>

double xgetloadavg(void)
{
   kstat_ctl_t *kc = NULL;
   kstat_t     *ks = NULL;
   kstat_named_t *kn = NULL;

   kc = kstat_open();
   if( kc == NULL ) {
      return -1;
   }

   ks = kstat_lookup(kc, "unix", 0, "system_misc");
   if( ks == NULL ) {
      return -1;
   }

   if( kstat_read(kc, ks, 0) == -1 ) {
      return -1;
   }

   kn = kstat_data_lookup(ks, "avenrun_1min");
   if( kn == NULL ) {
      return -1;
   }

   if( ks->ks_type == KSTAT_TYPE_NAMED ) {
      kn = ks->ks_data;
      if( kn == NULL ) {
         return -1;
      }

      return (double)(kn->value.ui32)/100;
   }

   kstat_close(kc);
}
#endif /* HAVE_KSTAT */
#endif /* solaris */
#ifdef __FreeBSD__

#include <stdio.h>
#include <sys/param.h>
#include <sys/fcntl.h>

double xgetloadavg ()
{
	double ret[3];

	getloadavg (ret, sizeof (ret) / sizeof (ret[0]));

	return ret[0];
}
#endif /* __FreeBSD__ */


#if defined(bsdi) || defined(__APPLE__)
#include <stdlib.h>

double xgetloadavg(void)
{                  
   double loadavg[3];

   if (getloadavg(loadavg, 1) == -1)
   {
      return -1;
   }
   else
   {
      return loadavg[0];
   }
}
#endif /* bsdi || __APPLE__ */


#ifdef __osf__
#include <sys/table.h>

double xgetloadavg(void)
{
  struct tbl_loadavg labuf;

  if (table(TBL_LOADAVG, 0, &labuf, 1, sizeof(labuf)) < 0) {
    perror("TBL_LOADAVG");
    return (-1);
  }

  if (labuf.tl_lscale) {
    return ((double)labuf.tl_avenrun.l[2] /
       (double)labuf.tl_lscale);
  }

  return (labuf.tl_avenrun.d[2]);
}
#endif /* __osf__ */

#endif /* HAVE_LOADAVG */      

