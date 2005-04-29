/*
  Copyright (c) 1990-1999 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 1999-Oct-05 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, both of these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.cdrom.com/pub/infozip/license.html
*/
#include "acorn/riscos.h"

#define RISCOS
#define NO_SYMLINK
#define NO_FCNTL_H
#define NO_UNISTD_H
#define NO_MKTEMP

#define PROCNAME(n) (action == ADD || action == UPDATE ? wild(n) : \
                     procname(n, 1))

#define isatty(a) 1
#define fseek(f,o,t) riscos_fseek((f),(o),(t))

#define localtime riscos_localtime
#define gmtime riscos_gmtime

#ifdef ZCRYPT_INTERNAL
#  define ZCR_SEED2     (unsigned)3141592654L   /* use PI as seed pattern */
#endif
