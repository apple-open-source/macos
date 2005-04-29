#define __amiga_time_lib_c

/* -----------------------------------------------------------------------------
This source is copyrighted by Norbert Pueschel <pueschel@imsdd.meb.uni-bonn.de>
From 'clockdaemon.readme':
(available from Aminet, main site is ftp.wustl.edu:/pub/aminet/ under
 util/time/clockdaemon.lha)
"The original SAS/C functions gmtime, localtime, mktime and time do not
work correctly. The supplied link library time.lib contains replacement
functions for them."
The time.lib library consists of three parts (time.c, timezone.c and version.c),
all included here. [time.lib 1.2 (1997-04-02)]
Permission is granted to the Info-ZIP group to redistribute the time.lib source.
The use of time.lib functions in own, noncommercial programs is permitted.
It is only required to add the timezone.doc to such a distribution.
Using the time.lib library in commercial software (including Shareware) is only
permitted after prior consultation of the author.
------------------------------------------------------------------------------*/
/* History */
/* 30 Mar 1997, Haidinger Walter, added AVAIL_GETVAR macro to support OS <V36 */
/* 24 May 1997, Haidinger Walter, added NO_MKTIME macro to allow use of Zip's */
/*              mktime.c. NO_MKTIME must be defined in the makefile, though.  */
/* 25 May 1997, Haidinger Walter, moved set_TZ() here from filedate.c         */
/* 20 Jul 1997, Paul Kienitz, adapted for Aztec C, added mkgmtime(),          */
/*              debugged, and made New York settings default, as is common.   */
/* 30 Sep 1997, Paul Kienitz, restored real_timezone_is_set flag              */
/* 19 Oct 1997, Paul Kienitz, corrected 16 bit multiply overflow bug          */
/* 21 Oct 1997, Chr. Spieler, shortened long lines, removed more 16 bit stuff */
/*              (n.b. __stdoffset and __dstoffset now have to be long ints)   */
/* 25 Oct 1997, Paul Kienitz, cleanup, make tzset() not redo work needlessly  */
/* 29 Oct 1997, Chr. Spieler, initialized globals _TZ, real_timezone_is_set   */
/* 31 Dec 1997, Haidinger Walter, created z-time.h to overcome sas/c header   */
/*              dependencies. TZ_ENVVAR macro added. Happy New Year!          */
/* 25 Apr 1998, Chr. Spieler, __timezone must always contain __stdoffset      */
/* 28 Apr 1998, Chr. Spieler, P. Kienitz, changed __daylight to standard usage */

#ifdef __SASC
#  include <proto/dos.h>
#  include <proto/locale.h>
#  include <proto/exec.h>
   /* this setenv() is in amiga/filedate.c */
   extern int setenv(const char *var, const char *value, int overwrite);
#else
#  include <clib/dos_protos.h>
#  include <clib/locale_protos.h>
#  include <clib/exec_protos.h>
#  include <pragmas/exec_lib.h>
#  include <pragmas/dos_lib.h>
#  include <pragmas/locale_lib.h>
/* Info-ZIP accesses these by their standard names: */
#  define __timezone timezone
#  define __daylight daylight
#  define __tzset    tzset
#endif
#define NO_TIME_H
#include "amiga/z-time.h"
#include <exec/execbase.h>
#include <clib/alib_stdio_protos.h>
#include <string.h>
#include <stdlib.h>

extern struct ExecBase *SysBase;
extern char *getenv(const char *var);

typedef unsigned long time_t;
struct tm {
    int tm_sec;      /* seconds after the minute */
    int tm_min;      /* minutes after the hour */
    int tm_hour;     /* hours since midnight */
    int tm_mday;     /* day of the month */
    int tm_mon;      /* months since January */
    int tm_year;     /* years since 1900 */
    int tm_wday;     /* days since Sunday */
    int tm_yday;     /* days since January 1 */
    int tm_isdst;    /* Daylight Savings Time flag */
};
struct dstdate {
  enum { JULIAN0, JULIAN, MWD } dd_type;
  int                           dd_day;
  int                           dd_week;
  int                           dd_month;
  int                           dd_secs;
};
static struct dstdate __dststart;
static struct dstdate __dstend;

#define isleapyear(y)    (((y)%4==0&&(!((y)%100==0)||((y)%400==0)))?1:0)
#define yearlen(y)       (isleapyear(y)?366:365)
#define weekday(d)       (((d)+4)%7)
#define jan1ofyear(y)    (((y)-70)*365+((y)-69)/4-((y)-1)/100+((y)+299)/400)
#define wdayofyear(y)    weekday(jan1ofyear(y))
#define AMIGA2UNIX       252460800 /* seconds between 1.1.1970 and 1.1.1978 */
#define CHECK            300       /* min. time between checks of IXGMTOFFSET */
#define GETVAR_REQVERS   36L       /* required OS version for GetVar() */
#define AVAIL_GETVAR     (SysBase->LibNode.lib_Version >= GETVAR_REQVERS)
#ifndef TZ_ENVVAR
#  define TZ_ENVVAR      "TZ"      /* environment variable to parse */
#endif

#ifdef __SASC
  extern int  __daylight;
  extern long __timezone;
  extern char *__tzname[2];
  extern char *_TZ;
#else
  int __daylight;
  long __timezone;
  char *__tzname[2];
  char *_TZ = NULL;
#endif
int real_timezone_is_set = FALSE;   /* globally visible TZ_is_valid signal */
char __tzstn[MAXTIMEZONELEN];
char __tzdtn[MAXTIMEZONELEN];
/* the following 4 variables are only used internally; make them static ? */
int __isdst;
time_t __nextdstchange;
long __stdoffset;
long __dstoffset;
#define TZLEN 64
static char TZ[TZLEN];
static struct tm TM;
static const unsigned short days[2][13] = {
  { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
  { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};
#ifndef NO_MKTIME     /* only used by mktime() */
static const unsigned short monlen[2][12] = {
  { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
  { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};
#endif

/* internal prototypes */
static time_t dst2time(int year,struct dstdate *dst);
static void time2tm(time_t time);
static int checkdst(time_t time);
#ifndef NO_MKTIME
static void normalize(int *i,int *j,int norm);
#endif
static long gettime(char **s);
static void getdstdate(char **s,struct dstdate *dst);
#ifdef __SASC
void set_TZ(long time_zone, int day_light);
#endif

/* prototypes for sc.lib replacement functions */
struct tm *gmtime(const time_t *t);
struct tm *localtime(const time_t *t);
#ifndef NO_MKTIME
time_t mkgmtime(struct tm *tm);
time_t mktime(struct tm *tm);
#endif
time_t time(time_t *tm);
void __tzset(void);


static time_t dst2time(int year,struct dstdate *dst)
{
  int isleapyear,week,mon,mday;
  mon = 0;
  mday = dst->dd_day;
  isleapyear = isleapyear(year);
  switch(dst->dd_type) {
    case JULIAN:
      if(!isleapyear || dst->dd_day <= 59) break;
    default:
      mday++;
      break;
    case MWD:
      mon = dst->dd_month-1;
      week = dst->dd_week;
      if(week == 5) {
        mon++;
        week = 0;
      }
      mday = dst->dd_day - weekday(jan1ofyear(year)+days[isleapyear][mon]);
      if(mday < 0) mday += 7;
      mday += (week - 1) * 7 + 1;
      break;
  }
  return((time_t)(jan1ofyear(year)+days[isleapyear][mon]+mday-1)*(time_t)86400L+
         (time_t)dst->dd_secs);
}

static void time2tm(time_t time)
{
  int isleapyear;
  TM.tm_sec  = time % 60;
  time /= 60;
  TM.tm_min  = time % 60;
  time /= 60;
  TM.tm_hour = time % 24;
  time /= 24;
  TM.tm_year = time/365 + 70; /* guess year */
  while((TM.tm_yday = time - jan1ofyear(TM.tm_year)) < 0) TM.tm_year--;
  isleapyear = isleapyear(TM.tm_year);
  for(TM.tm_mon = 0;
      TM.tm_yday >= days[isleapyear][TM.tm_mon+1];
      TM.tm_mon++);
  TM.tm_mday = TM.tm_yday - days[isleapyear][TM.tm_mon] + 1;
  TM.tm_wday = (time+4)%7;
}

static int checkdst(time_t time)
{
  int year,day;
  time_t t,u;
  day = time / 86400L;
  year = day / 365 + 70; /* guess year */
  while(day - jan1ofyear(year) < 0) year--;
  t = dst2time(year,&__dststart) + __stdoffset;
  u = dst2time(year,&__dstend)   + __dstoffset;
  if(u > t) {
    return((time >= t && time < u)?1:0);
  }
  else {
    return((time < u || time >= t)?1:0);
  }
}

struct tm *gmtime(const time_t *t)
{
  TM.tm_isdst = 0;
  time2tm(*t);
  return(&TM);
}

struct tm *localtime(const time_t *t)
{
  if(!_TZ) __tzset();
  TM.tm_isdst = checkdst(*t);
  time2tm(*t - (TM.tm_isdst ? __dstoffset : __stdoffset));
  return(&TM);
}

#ifndef NO_MKTIME   /* normalize() only used by mktime() */
static void normalize(int *i,int *j,int norm)
{
  while(*i < 0) {
    *i += norm;
    (*j)--;
  }
  while(*i >= norm) {
    *i -= norm;
    (*j)++;
  }
}

time_t mkgmtime(struct tm *tm)
{
  time_t t;
  normalize(&tm->tm_sec,&tm->tm_min,60);
  normalize(&tm->tm_min,&tm->tm_hour,60);
  normalize(&tm->tm_hour,&tm->tm_mday,24);
  normalize(&tm->tm_mon,&tm->tm_year,12);
  while(tm->tm_mday > monlen[isleapyear(tm->tm_year)][tm->tm_mon]) {
    tm->tm_mday -= monlen[isleapyear(tm->tm_year)][tm->tm_mon];
    tm->tm_mon++;
    if(tm->tm_mon == 12) {
      tm->tm_mon = 0;
      tm->tm_year++;
    }
  }
  while(tm->tm_mday < 0) {
    tm->tm_mon--;
    if(tm->tm_mon == -1) {
      tm->tm_mon = 11;
      tm->tm_year--;
    }
    tm->tm_mday += monlen[isleapyear(tm->tm_year)][tm->tm_mon];
  }
  tm->tm_yday = tm->tm_mday + days[isleapyear(tm->tm_year)][tm->tm_mon] - 1;
  t = jan1ofyear(tm->tm_year) + tm->tm_yday;
  tm->tm_wday = weekday(t);
  if(tm->tm_year < 70) return((time_t)0);
  t = t * 86400L + tm->tm_hour * 3600L + tm->tm_min * 60L + (time_t)tm->tm_sec;
  return(t);
}

time_t mktime(struct tm *tm)
{
  time_t t;
  if(!_TZ) __tzset();
  t = mkgmtime(tm);
  if(tm->tm_isdst < 0) tm->tm_isdst = checkdst(t);
  t += tm->tm_isdst ? __dstoffset : __stdoffset;
  return(t);
}
#endif /* !NO_MKTIME */

static long gettime(char **s)
{
  long num,time;
  for(num = 0;**s >= '0' && **s <= '9';(*s)++) {
    num = 10*num + (**s - '0');
  }
  time = 3600L * num;
  if(**s == ':') {
    (*s)++;
    for(num = 0;**s >= '0' && **s <= '9';(*s)++) {
      num = 10*num + (**s - '0');
    }
    time += 60 * num;
    if(**s == ':') {
      (*s)++;
      for(num = 0;**s >= '0' && **s <= '9';(*s)++) {
        num = 10*num + (**s - '0');
      }
      time += num;
    }
  }
  return(time);
}

static void getdstdate(char **s,struct dstdate *dst)
{
  switch(**s) {
    case 'J':
    case 'j':
      (*s)++;
      dst->dd_type = JULIAN;
      for(dst->dd_day = 0;**s >= '0' && **s <= '9';(*s)++) {
        dst->dd_day = 10*dst->dd_day + (**s - '0');
      }
      break;
    case 'M':
    case 'm':
      (*s)++;
      dst->dd_type = MWD;
      for(dst->dd_month = 0;**s >= '0' && **s <= '9';(*s)++) {
        dst->dd_month = 10*dst->dd_month + (**s - '0');
      }
      if(**s != '.') return;
      (*s)++;
      for(dst->dd_week = 0;**s >= '0' && **s <= '9';(*s)++) {
        dst->dd_week = 10*dst->dd_week + (**s - '0');
      }
      if(**s != '.') return;
      (*s)++;
      for(dst->dd_day = 0;**s >= '0' && **s <= '9';(*s)++) {
        dst->dd_day = 10*dst->dd_day + (**s - '0');
      }
      break;
    default:
      dst->dd_type = JULIAN0;
      for(dst->dd_day = 0;**s >= '0' && **s <= '9';(*s)++) {
        dst->dd_day = 10*dst->dd_day + (**s - '0');
      }
      break;
  }
  if(**s == '/') {
    (*s)++;
    dst->dd_secs = gettime(s);
  }
}

void __tzset(void)
{
  char *s,*t;
  int minus = 0;
  time_t tm;
  struct Library *LocaleBase;
  struct Locale *loc = NULL;
  if (real_timezone_is_set)
    return;
  real_timezone_is_set = TRUE;
  __dststart.dd_secs = __dstend.dd_secs = 7200;
  __dststart.dd_type = __dstend.dd_type = MWD;
  __dststart.dd_month = 4;
  __dststart.dd_week = 1;
  __dstend.dd_month = 10;
  __dstend.dd_week = 5;
  __dststart.dd_day = __dstend.dd_day = 0; /* sunday */
  _TZ = NULL;
  if (AVAIL_GETVAR) {                /* GetVar() available? */
    if(GetVar(TZ_ENVVAR,TZ,TZLEN,GVF_GLOBAL_ONLY) > 0)
      _TZ = TZ;
  } else
      _TZ = getenv(TZ_ENVVAR);
  if (_TZ == NULL || !_TZ[0]) {
    static char gmt[MAXTIMEZONELEN] = DEFAULT_TZ_STR;
    LocaleBase = OpenLibrary("locale.library",0);
    if(LocaleBase) {
      loc = OpenLocale(0);  /* cannot return null */
      if (loc->loc_GMTOffset == -300 || loc->loc_GMTOffset == 300) {
        BPTR eh;
        if (eh = Lock("ENV:sys/locale.prefs", ACCESS_READ))
          UnLock(eh);
        else {
          real_timezone_is_set = FALSE;
          loc->loc_GMTOffset = 300; /* Amigados bug: default when locale is */
        }                           /* not initialized can have wrong sign  */
      }
      sprintf(gmt, "GMT%ld:%02ld", loc->loc_GMTOffset / 60L,
              labs(loc->loc_GMTOffset) % 60L);
      CloseLocale(loc);
      CloseLibrary(LocaleBase);
    } else
      real_timezone_is_set = FALSE;
    _TZ = gmt;
  }
  for(s = _TZ,t = __tzstn;*s && *s != '+' && *s != '-' && *s != ',' &&
      (*s < '0' || *s > '9');s++) {
    if(t-__tzstn < MAXTIMEZONELEN-1) *(t++) = *s;
  }
  *t = '\0';
  if(*s == '+') {
    s++;
  }
  else {
    if(*s == '-') {
      minus = 1;
      s++;
    }
  }
  __stdoffset = gettime(&s);
  if(minus) {
    __stdoffset *= -1;
  }
  if(*s) {
    minus = 0;
    for(t = __tzdtn;*s && *s != '+' && *s != '-' && *s != ',' &&
        (*s < '0' || *s > '9');s++) {
      if(t-__tzdtn < MAXTIMEZONELEN-1) *(t++) = *s;
    }
    *t = '\0';
    if(*s == '+') {
      s++;
    }
    else {
      if(*s == '-') {
        minus = 1;
        s++;
      }
    }
    if(*s && *s != ',') {
      __dstoffset = gettime(&s);
      if(minus) {
        __dstoffset *= -1;
      }
    }
    else {
      __dstoffset = __stdoffset - 3600L;
    }
    if(*s == ',') {
      s++;
      getdstdate(&s,&__dststart);
      if(*s == ',') {
        s++;
        getdstdate(&s,&__dstend);
      }
    }
  }
  else {
    __dstoffset = __stdoffset;
  }
  time2tm(time(&tm));
  __isdst = checkdst(tm);
  __daylight = (__dstoffset != __stdoffset);
  __timezone = __stdoffset;
  __nextdstchange = dst2time(TM.tm_year, __isdst ? &__dstend : &__dststart);
  if(tm >= __nextdstchange) {
    __nextdstchange = dst2time(TM.tm_year+1,
                               __isdst ? &__dstend : &__dststart);
  }
  __tzname[0] = __tzstn;
  __tzname[1] = __tzdtn;
#ifdef __SASC
  if (loc)         /* store TZ envvar if data read from locale */
    set_TZ(__timezone, __daylight);
#endif
}

time_t time(time_t *tm)
{
  static time_t last_check = 0;
  static struct _ixgmtoffset {
    LONG  Offset;
    UBYTE DST;
    UBYTE Null;
  } ixgmtoffset;
  static char *envvarstr; /* ptr to environm. string (used if !AVAIL_GETVAR) */
  struct DateStamp ds;
  time_t now;
  DateStamp(&ds);
  now = ds.ds_Days * 86400L + ds.ds_Minute * 60L +
        ds.ds_Tick / TICKS_PER_SECOND;
  if(now - last_check > CHECK) {
    last_check = now;
    if (AVAIL_GETVAR)    /* GetVar() available? */
      if(GetVar("IXGMTOFFSET",(STRPTR)&ixgmtoffset,6,
                GVF_BINARY_VAR|GVF_GLOBAL_ONLY) == -1) {
        __tzset();
        ixgmtoffset.Offset = __isdst ? __dstoffset : __stdoffset;
      }
    else
      if (envvarstr=getenv("IXGMTOFFSET")) {
        ixgmtoffset = *((struct _ixgmtoffset *)envvarstr);  /* copy to struct */
        __tzset();
        ixgmtoffset.Offset = __isdst ? __dstoffset : __stdoffset;
      }
  }
  now += AMIGA2UNIX;
  now += ixgmtoffset.Offset;
  if(tm) *tm = now;
  return(now);
}

#ifdef __SASC

/* Stores data from timezone and daylight to ENV:TZ.                  */
/* ENV:TZ is required to exist by some other SAS/C library functions, */
/* like stat() or fstat().                                            */
void set_TZ(long time_zone, int day_light)
{
  char put_tz[MAXTIMEZONELEN];  /* string for putenv: "TZ=aaabbb:bb:bbccc" */
  int offset;
  void *exists;     /* dummy ptr to see if global envvar TZ already exists */
  if (AVAIL_GETVAR)
     exists = (void *)FindVar(TZ_ENVVAR,GVF_GLOBAL_ONLY);  /* OS V36+ */
  else
     exists = (void *)getenv(TZ_ENVVAR);
  /* see if there is already an envvar TZ_ENVVAR. If not, create it */
  if (exists == NULL) {
    /* create TZ string by pieces: */
    sprintf(put_tz, "GMT%+ld", time_zone / 3600L);
    if (time_zone % 3600L) {
      offset = (int) labs(time_zone % 3600L);
      sprintf(put_tz + strlen(put_tz), ":%02d", offset / 60);
      if (offset % 60)
        sprintf(put_tz + strlen(put_tz), ":%02d", offset % 60);
    }
    if (day_light)
      strcat(put_tz,"DST");
    if (AVAIL_GETVAR)       /* store TZ to ENV:TZ. */
       SetVar(TZ_ENVVAR,put_tz,-1,GVF_GLOBAL_ONLY);     /* OS V36+ */
    else
       setenv(TZ_ENVVAR,put_tz, 1);
  }
}
#endif /* __SASC */
