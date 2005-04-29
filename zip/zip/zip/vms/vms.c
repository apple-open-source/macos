/*
  Copyright (c) 1990-1999 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 1999-Oct-05 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, both of these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.cdrom.com/pub/infozip/license.html
*/
/*
 *  vms.c (zip) by Igor Mandrichenko    Version 2.2-2
 *
 *  Revision history:
 *  ...
 *  2.2-2       18-jan-1993     I.Mandrichenko
 *      vms_stat() added - version of stat() that handles special
 *      case when end-of-file-block == 0
 */

#ifdef VMS                      /* For VMS only ! */

#include "zip.h"

#ifndef __STARLET_LOADED
#include <starlet.h>
#endif
#if !defined(_RMS_H) && !defined(__RMS_LOADED)
#include <rms.h>
#endif

#ifndef UTIL

/* Include the `VMS attributes' preserving file-io code. We distinguish
   between two incompatible flavours of storing VMS attributes in the
   Zip archive:
   a) The "PKware" style follows the extra field specification for
      PKware's VMS Zip.
   b) The "IM (Info-ZIP)" flavour was defined from scratch by
      Igor Mandrichenko. This version has be used in official Info-ZIP
      releases for several years and is known to work well.
 */

#if defined(VMS_PK_EXTRA)
#include "vms_pk.c"
#else
#include "vms_im.c"
#endif

#endif /* !UTIL */

#ifndef ERR
#define ERR(x) (((x)&1)==0)
#endif

#ifndef NULL
#define NULL (void*)(0L)
#endif

int vms_stat(file,s)
char *file;
stat_t *s;
{
    int status;
    int staterr;
    struct FAB fab;
    struct XABFHC fhc;

    /*
     *  In simplest case when stat() returns "ok" and file size is
     *  nonzero or this is directory, finish with this
     */

    if( (staterr=stat(file,s)) == 0
        && ( (int)(s->st_size) >= 0               /* Size - ok */
             || (s->st_mode & S_IFREG) == 0       /* Not a plain file */
           )
    ) return staterr;

    /*
     *  Get here to handle the special case when stat() returns
     *  invalid file size. Use RMS to compute the size.
     *  When EOF block is zero, set file size to its physical size.
     *  One more case to get here is when this is remote file accessed
     *  via DECnet.
     */

    fab = cc$rms_fab;
    fhc = cc$rms_xabfhc;
    fab.fab$l_fna = file;
    fab.fab$b_fns = strlen(file);
    fab.fab$l_xab = (char*)(&fhc);

    fab.fab$b_fac = FAB$M_GET;

    status = sys$open(&fab);
    fab.fab$l_xab = (char*)0L;
    sys$close(&fab);

    if( !ERR(status) )
    {
        if( fhc.xab$l_ebk > 0 )
            s->st_size = ( fhc.xab$l_ebk-1 ) * 512 + fhc.xab$w_ffb;
        else if( fab.fab$b_org == FAB$C_IDX
                 || fab.fab$b_org == FAB$C_REL
                 || fab.fab$b_org == FAB$C_HSH )
                /* Special case, when ebk=0: save entire allocated space */
                    s->st_size = fhc.xab$l_hbk * 512;
        else
            s->st_size = fhc.xab$w_ffb;
        return 0; /* stat() success code */
    }
    else
        return status;
}

void vms_exit(e)
   int e;
{
/*---------------------------------------------------------------------------
    Return an intelligent status/severity level if RETURN_SEVERITY defined:

    $STATUS          $SEVERITY = $STATUS & 7
    31 .. 16 15 .. 3   2 1 0
                       -----
    VMS                0 0 0  0    Warning
    FACILITY           0 0 1  1    Success
    Number             0 1 0  2    Error
             MESSAGE   0 1 1  3    Information
             Number    1 0 0  4    Severe (fatal) error

    0x7FFF0000 was chosen (by experimentation) to be outside the range of
    VMS FACILITYs that have dedicated message numbers.  Hopefully this will
    always result in silent exits--it does on VMS 5.4.  Note that the C li-
    brary translates exit arguments of zero to a $STATUS value of 1 (i.e.,
    exit is both silent and has a $SEVERITY of "success").
  ---------------------------------------------------------------------------*/
  {
    int severity;

    switch (e) {                        /* $SEVERITY: */
      case ZE_NONE:
          severity = 0; break;          /*   warning  */
      case ZE_FORM:
      case ZE_BIG:
      case ZE_NOTE:
      case ZE_ABORT:
      case ZE_NAME:
      case ZE_PARMS:
      case ZE_OPEN:
          severity = 2; break;          /*   error    */
      default:
          severity = 4; break;          /*   fatal    */
    }

    exit(                                       /* $SEVERITY:              */
         (e == ZE_OK) ? 1 :                     /*   success               */
         (0x7FFF0000 | (e << 4) | severity)     /*   warning, error, fatal */
        );
   }
}


/******************************/
/*  Function version_local()  */
/******************************/

void version_local()
{
    static ZCONST char CompiledWith[] = "Compiled with %s%s for %s%s%s%s.\n\n";
#ifdef VMS_VERSION
    char buf[40];
#endif
#ifdef __DECC_VER
    char buf2[40];
    int  vtyp;
#endif

/*  DEC C in ANSI mode does not like "#ifdef MACRO" inside another
    macro when MACRO is equated to a value (by "#define MACRO 1").   */

    printf(CompiledWith,

#ifdef __GNUC__
      "gcc ", __VERSION__,
#else
#  if defined(DECC) || defined(__DECC) || defined (__DECC__)
      "DEC C",
#    ifdef __DECC_VER
      (sprintf(buf2, " %c%d.%d-%03d",
               ((vtyp = (__DECC_VER / 10000) % 10) == 6 ? 'T' :
                (vtyp == 8 ? 'S' : 'V')),
               __DECC_VER / 10000000,
               (__DECC_VER % 10000000) / 100000, __DECC_VER % 1000), buf2),
#    else
      "",
#    endif
#  else
#  ifdef VAXC
      "VAX C", "",
#  else
      "unknown compiler", "",
#  endif
#  endif
#endif

#ifdef VMS_VERSION
#  if defined(__alpha)
      "OpenVMS",   /* version has trailing spaces ("V6.1   "), so truncate: */
      (sprintf(buf, " (%.4s for Alpha)", VMS_VERSION), buf),
#  else /* VAX */
      (VMS_VERSION[1] >= '6')? "OpenVMS" : "VMS",
      (sprintf(buf, " (%.4s for VAX)", VMS_VERSION), buf),
#  endif
#else
      "VMS",
      "",
#endif /* ?VMS_VERSION */

#ifdef __DATE__
      " on ", __DATE__
#else
      "", ""
#endif
      );

} /* end function version_local() */

#endif /* VMS */
