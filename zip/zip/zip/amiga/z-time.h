/*
  Copyright (c) 1990-1999 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 1999-Oct-05 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, both of these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.cdrom.com/pub/infozip/license.html
*/
#ifndef __amiga_z_time_h
#define __amiga_z_time_h

/* A <time.h> replacement for use with time_lib.c */
/* Usage: * Define (or Undefine) USE_TIME_LIB below             */
/*        * Replace any <time.h> includes by "amiga/z-time.h"   */

/* First of all: Select whether to use time_lib functions or not */
#if 1
#  ifndef USE_TIME_LIB
#  define USE_TIME_LIB
#  endif
#else
#  ifdef USE_TIME_LIB
#  undef USE_TIME_LIB
#  endif
#endif

#ifdef USE_TIME_LIB
   /* constants needed everywhere */
#  define MAXTIMEZONELEN 16
#  ifndef DEFAULT_TZ_STR
#    define DEFAULT_TZ_STR "EST5EDT" /* US East Coast is the usual default */
#  endif

   /* define time_t where needed (everywhere but amiga/time_lib.c) */
#  if defined(__SASC) && defined(NO_TIME_H) && !defined(__amiga_time_lib_c)
     typedef unsigned long time_t;  /* override sas/c's time_t */
#    define _TIME_T        1        /* mark it as already defined */
#    define _COMMTIME_H             /* do not include sys/commtime.h */
#  endif

#  ifndef NO_TIME_H
#    include <time.h>               /* time_lib.c uses NO_TIME_H */
#  endif

   /* adjust included time.h */
#  ifdef __SASC
     /* tz[sd]tn arrays have different length now: need different names */
#    define __tzstn         tzstn
#    define __tzdtn         tzdtn
     /* prevent other possible name conflicts */
#    define __nextdstchange nextdstchange
#    define __stdoffset     stdoffset
#    define __dstoffset     dstoffset

#    ifndef __amiga_time_lib_c
#      ifdef TZ
#        undef TZ                       /* defined in sas/c time.h */
#      endif TZ
#      define TZ  DEFAULT_TZ_STR        /* redefine TZ to default timezone */
       extern char __tzstn[MAXTIMEZONELEN];
       extern char __tzdtn[MAXTIMEZONELEN];
#    endif
#  endif /* __SASC */

#  ifdef AZTEC_C
     void tzset(void);
#  endif

#else /* ?USE_TIME_LIB */

#  ifndef NO_TIME_H
#    include <time.h>
#  endif
#endif /* !USE_TIME_LIB */

#endif /* __amiga_z_time_h */
