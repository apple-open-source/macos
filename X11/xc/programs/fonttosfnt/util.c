/*
Copyright (c) 2002-2003 by Juliusz Chroboczek

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
/* $XFree86: xc/programs/fonttosfnt/util.c,v 1.11 2003/12/19 02:16:36 dawes Exp $ */

#include <time.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#ifndef __UNIXOS2__
# include <math.h>
#else
# include <float.h>
#endif
#include <stdarg.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_INTERNAL_OBJECTS_H
#include FT_BDF_H
#include "X11/Xos.h"
#include "fonttosfnt.h"

#ifdef NEED_SNPRINTF
#undef SCOPE
#define SCOPE static
#include "snprintf.c"
#endif

#ifdef __GLIBC__
#define HAVE_TIMEGM
#define HAVE_TM_GMTOFF
#endif

#ifdef BSD
#define HAVE_TM_GMTOFF
#define GMTOFFMEMBER tm_gmtoff
#endif

#ifdef SCO
#define HAVE_TM_GMTOFF
#define GMTOFFMEMBER tm_tzadj
#endif

/* That's in POSIX */
#define HAVE_TZSET

#ifdef NEED_SETENV
extern int setenv(const char *name, const char *value, int overwrite);
extern void unsetenv(const char *name);
#endif

char*
sprintf_reliable(char *f, ...)
{
    char *s;
    va_list args;
    va_start(args, f);
    s = vsprintf_reliable(f, args);
    va_end(args);
    return s;
}    

char*
vsprintf_reliable(char *f, va_list args)
{
    int n, size = 12;
    char *string;
    while(1) {
        if(size > 4096)
            return NULL;
        string = malloc(size);
        if(!string)
            return NULL;
        n = vsnprintf(string, size, f, args);
        if(n >= 0 && n < size)
            return string;
        else if(n >= size)
            size = n + 1;
        else
            size = size * 3 / 2 + 1;
        free(string);
    }
    /* NOTREACHED */
}

/* Build a UTF-16 string from a Latin-1 string.  
   Result is not NUL-terminated. */
char *
makeUTF16(char *string)
{
    int i;
    int n = strlen(string);
    char *value = malloc(2 * n);
    if(!value)
        return NULL;
    for(i = 0; i < n; i++) {
        value[2 * i] = '\0';
        value[2 * i + 1] = string[i];
    }
    return value;
}

unsigned
makeName(char *s)
{
    return s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3];
}

/* Like mktime(3), but UTC rather than local time */
#if defined(HAVE_TIMEGM)
static time_t
mktime_gmt(struct tm *tm)
{
    return timegm(tm);
}
#elif defined(HAVE_TM_GMTOFF)
static time_t
mktime_gmt(struct tm *tm)
{
    time_t t;
    struct tm *ltm;

    t = mktime(tm);
    if(t < 0)
        return -1;
    ltm = localtime(&t);
    if(ltm == NULL)
        return -1;
    return t + ltm->GMTOFFMEMBER;
}
#elif defined(HAVE_TZSET)
/* Taken from the Linux timegm(3) man page */
static time_t
mktime_gmt(struct tm *tm)
{
    time_t t;
    char *tz;

    tz = getenv("TZ");
    setenv("TZ", "", 1);
    tzset();
    t = mktime(tm);
    if(tz)
        setenv("TZ", tz, 1);
    else
        unsetenv("TZ");
    tzset();
    return t;
}
#else
#error no mktime_gmt implementation on this platform
#endif

/* Return the current time as a signed 64-bit number of seconds since
   midnight, 1 January 1904.  This is apparently when the Macintosh
   was designed. */
int
macTime(int *hi, unsigned *lo)
{
    unsigned long diff;		/* Not time_t */
    time_t macEpoch, current;
    struct tm tm;
    tm.tm_sec = 0;
    tm.tm_min = 0;
    tm.tm_hour = 0;
    tm.tm_mday = 1;
    tm.tm_mon = 1;
    tm.tm_year = 4;
    tm.tm_isdst = -1;

    macEpoch = mktime_gmt(&tm);
    if(macEpoch < 0) return -1;

    current = time(NULL);
    if(current < 0)
        return -1;

    if(current < macEpoch) {
        errno = EINVAL;
        return -1;
    }

    diff = current - macEpoch;
#if INT_MAX == LONG_MAX
    *hi = 0;
#else
    *hi = diff >> 32;
#endif
    *lo = diff & 0xFFFFFFFF;
    return 0;
}

unsigned
faceFoundry(FT_Face face)
{
    int rc;
    BDF_PropertyRec prop;

    rc = FT_Get_BDF_Property(face, "FOUNDRY", &prop);
    if(rc == 0 && prop.type == BDF_PROPERTY_TYPE_ATOM) {
        if(strcasecmp(prop.u.atom, "adobe") == 0)
            return makeName("ADBE");
        else if(strcasecmp(prop.u.atom, "agfa") == 0)
            return makeName("AGFA");
        else if(strcasecmp(prop.u.atom, "altsys") == 0)
            return makeName("ALTS");
        else if(strcasecmp(prop.u.atom, "apple") == 0)
            return makeName("APPL");
        else if(strcasecmp(prop.u.atom, "arphic") == 0)
            return makeName("ARPH");
        else if(strcasecmp(prop.u.atom, "alltype") == 0)
            return makeName("ATEC");
        else if(strcasecmp(prop.u.atom, "b&h") == 0)
            return makeName("B&H ");
        else if(strcasecmp(prop.u.atom, "bitstream") == 0)
            return makeName("BITS");
        else if(strcasecmp(prop.u.atom, "dynalab") == 0)
            return makeName("DYNA");
        else if(strcasecmp(prop.u.atom, "ibm") == 0)
            return makeName("IBM ");
        else if(strcasecmp(prop.u.atom, "itc") == 0)
            return makeName("ITC ");
        else if(strcasecmp(prop.u.atom, "interleaf") == 0)
            return makeName("LEAF");
        else if(strcasecmp(prop.u.atom, "impress") == 0)
            return makeName("IMPR");
        else if(strcasecmp(prop.u.atom, "larabiefonts") == 0)
            return makeName("LARA");
        else if(strcasecmp(prop.u.atom, "linotype") == 0)
            return makeName("LINO");
        else if(strcasecmp(prop.u.atom, "monotype") == 0)
            return makeName("MT  ");
        else if(strcasecmp(prop.u.atom, "microsoft") == 0)
            return makeName("MS  ");
        else if(strcasecmp(prop.u.atom, "urw") == 0)
            return makeName("URW ");
        else if(strcasecmp(prop.u.atom, "y&y") == 0)
            return makeName("Y&Y ");
        else
            return makeName("UNKN");
    }
    /* For now */
    return makeName("UNKN");
}
    

int
faceWeight(FT_Face face)
{
    int rc;
    BDF_PropertyRec prop;
    rc = FT_Get_BDF_Property(face, "WEIGHT_NAME", &prop);
    if(rc == 0 && prop.type == BDF_PROPERTY_TYPE_ATOM) {
        if(strcasecmp(prop.u.atom, "thin") == 0)
            return 100;
        else if(strcasecmp(prop.u.atom, "extralight") == 0)
            return 200;
        else if(strcasecmp(prop.u.atom, "light") == 0)
            return 300;
        else if(strcasecmp(prop.u.atom, "medium") == 0)
            return 500;
        else if(strcasecmp(prop.u.atom, "semibold") == 0)
            return 600;
        else if(strcasecmp(prop.u.atom, "bold") == 0)
            return 700;
        else if(strcasecmp(prop.u.atom, "extrabold") == 0)
            return 800;
        else if(strcasecmp(prop.u.atom, "black") == 0)
            return 900;
        else
            return 500;
    } else
        return 500;             /* for now */
}

int
faceWidth(FT_Face face)
{
    int rc;
    BDF_PropertyRec prop;
    rc = FT_Get_BDF_Property(face, "SETWIDTH_NAME", &prop);
    if(rc == 0 && prop.type == BDF_PROPERTY_TYPE_ATOM) {
        if(strcasecmp(prop.u.atom, "ultracondensed") == 0)
            return 1;
        else if(strcasecmp(prop.u.atom, "extracondensed") == 0)
            return 2;
        else if(strcasecmp(prop.u.atom, "condensed") == 0)
            return 3;
        else if(strcasecmp(prop.u.atom, "semicondensed") == 0)
            return 4;
        else if(strcasecmp(prop.u.atom, "normal") == 0)
            return 5;
        else if(strcasecmp(prop.u.atom, "semiexpanded") == 0)
            return 6;
        else if(strcasecmp(prop.u.atom, "expanded") == 0)
            return 7;
        else if(strcasecmp(prop.u.atom, "extraexpanded") == 0)
            return 8;
        else if(strcasecmp(prop.u.atom, "ultraexpanded") == 0)
            return 9;
        else
            return 5;
    } else
        return 5;               /* for now */
}

int
faceItalicAngle(FT_Face face)
{
    int rc;
    BDF_PropertyRec prop;

    rc = FT_Get_BDF_Property(face, "ITALIC_ANGLE", &prop);
    if(rc == 0 && prop.type == BDF_PROPERTY_TYPE_INTEGER) {
        return (prop.u.integer - 64 * 90) * (TWO_SIXTEENTH / 64);
    }

    rc = FT_Get_BDF_Property(face, "SLANT", &prop);
    if(rc == 0 && prop.type == BDF_PROPERTY_TYPE_ATOM) {
        if(strcasecmp(prop.u.atom, "i") == 0 ||
           strcasecmp(prop.u.atom, "s") == 0)
            return -30 * TWO_SIXTEENTH;
        else
            return 0;
    } else
        return 0;               /* for now */
}

int
faceFlags(FT_Face face)
{
    int flags = 0;
    BDF_PropertyRec prop;
    int rc;

    if(faceWeight(face) >= 650)
        flags |= FACE_BOLD;
    rc = FT_Get_BDF_Property(face, "SLANT", &prop);
    if(rc == 0 && prop.type == BDF_PROPERTY_TYPE_ATOM) {
        if(strcasecmp(prop.u.atom, "i") == 0 ||
           strcasecmp(prop.u.atom, "s") == 0)
            flags |= FACE_ITALIC;
    }
    return flags;
}

char *
faceEncoding(FT_Face face)
{
    BDF_PropertyRec p1, p2;
    int rc;

    rc = FT_Get_BDF_Property(face, "CHARSET_REGISTRY", &p1);
    if(rc != 0 || p1.type != BDF_PROPERTY_TYPE_ATOM)
        return NULL;
    rc = FT_Get_BDF_Property(face, "CHARSET_ENCODING", &p2);
    if(rc != 0 || p2.type != BDF_PROPERTY_TYPE_ATOM)
        return NULL;

    return sprintf_reliable("%s-%s", p1.u.atom, p2.u.atom);
}
    
int
degreesToFraction(int deg, int *num, int *den)
{
    double n, d;
    double rad, val;
    int i;

    if(deg <= -(60 * TWO_SIXTEENTH) || deg >= (60 * TWO_SIXTEENTH))
        goto fail;

    rad = (((double)deg) / TWO_SIXTEENTH) / 180.0 * M_PI;

    n = sin(-rad);
    d = cos(rad);

    if(d < 0.001)
        goto fail;

    val = atan2(n, d);
    /* There must be a cleaner way */
    for(i = 1; i < 10000; i++) {
        if((int)(d * i) != 0.0 &&
           fabs(atan2(ROUND(n * i), ROUND(d * i)) - val) < 0.05) {
            *num = (int)ROUND(n * i);
            *den = (int)ROUND(d * i);
            return 0;
        }
    }

 fail:
    *den = 1;
    *num = 0;
    return -1;
}

