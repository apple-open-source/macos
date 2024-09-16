/* tmbstr.c -- Tidy string utility functions

  (c) 1998-2006 (W3C) MIT, ERCIM, Keio University
  See tidy.h for the copyright notice.

  CVS Info :

    $Author$ 
    $Date$ 
    $Revision$ 

*/

#include "forward.h"
#include "tmbstr.h"
#include "lexer.h"

/* like strdup but using MemAlloc */
tmbstr TY_(tmbstrdup)( ctmbstr str )
{
    tmbstr s = NULL;
    if ( str )
    {
        size_t len = TY_(tmbstrlen)( str );
        tmbstr cp = s = (tmbstr) MemAlloc( 1+len );
        while ( 0 != (*cp++ = *str++) )
            /**/;
    }
    return s;
}

/* like strndup but using MemAlloc */
tmbstr TY_(tmbstrndup)( ctmbstr str, size_t len )
{
    tmbstr s = NULL;
    if ( str && len > 0 )
    {
        tmbstr cp = s = (tmbstr) MemAlloc( 1+len );
        while ( len-- > 0 &&  (*cp++ = *str++) )
          /**/;
        *cp = 0;
    }
    return s;
}

/* exactly same as strncpy */
size_t TY_(tmbstrncpy)( tmbstr s1, ctmbstr s2, size_t size )
{
    if ( s1 != NULL && s2 != NULL && size > 0 )
    {
        tmbstr cp = s1;
        while ( *s2 && --size )  /* Predecrement: reserve byte */
            *cp++ = *s2++;       /* for NULL terminator. */
        *cp = 0;
    }
    return size;
}

/* Allows expressions like:  cp += tmbstrcpy( cp, "joebob" );
*/
size_t TY_(tmbstrcpy)( tmbstr s1, ctmbstr s2 )
{
    size_t ncpy = 0;
    while (0 != (*s1++ = *s2++) )
        ++ncpy;
    return ncpy;
}

/* Allows expressions like:  cp += tmbstrcat( cp, "joebob" );
*/
size_t TY_(tmbstrcat)( tmbstr s1, ctmbstr s2 )
{
    size_t ncpy = 0;
    while ( *s1 )
        ++s1;

    while (0 != (*s1++ = *s2++) )
        ++ncpy;
    return ncpy;
}

/* exactly same as strcmp */
int TY_(tmbstrcmp)( ctmbstr s1, ctmbstr s2 )
{
    int c;
    while ((c = *s1) == *s2)
    {
        if (c == '\0')
            return 0;

        ++s1;
        ++s2;
    }

    return (*s1 > *s2 ? 1 : -1);
}

/* returns byte count, not char count */
size_t TY_(tmbstrlen)( ctmbstr str )
{
    size_t len = 0;
    if ( str ) 
    {
        while ( *str++ )
            ++len;
    }
    return len;
}

/*
 MS C 4.2 doesn't include strcasecmp.
 Note that tolower and toupper won't
 work on chars > 127.

 Neither does ToLower()!
*/
int TY_(tmbstrcasecmp)( ctmbstr s1, ctmbstr s2 )
{
    uint c;

    while (c = (uint)(*s1), TY_(ToLower)(c) == TY_(ToLower)((uint)(*s2)))
    {
        if (c == '\0')
            return 0;

        ++s1;
        ++s2;
    }

    return (*s1 > *s2 ? 1 : -1);
}

int TY_(tmbstrncmp)( ctmbstr s1, ctmbstr s2, size_t n )
{
    uint c;

    while ((c = (byte)*s1) == (byte)*s2)
    {
        if (c == '\0')
            return 0;

        if (n == 0)
            return 0;

        ++s1;
        ++s2;
        --n;
    }

    if (n == 0)
        return 0;

    return (*s1 > *s2 ? 1 : -1);
}

int TY_(tmbstrncasecmp)( ctmbstr s1, ctmbstr s2, size_t n )
{
    uint c;

    while (c = (uint)(*s1), TY_(ToLower)(c) == TY_(ToLower)((uint)(*s2)))
    {
        if (c == '\0')
            return 0;

        if (n == 0)
            return 0;

        ++s1;
        ++s2;
        --n;
    }

    if (n == 0)
        return 0;

    return (*s1 > *s2 ? 1 : -1);
}

#if 0
/* return offset of cc from beginning of s1,
** -1 if not found.
*/
int TY_(tmbstrnchr)( ctmbstr s1, size_t maxlen, tmbchar cc )
{
    ctmbstr cp = s1;

    for ( size_t i = 0; i < maxlen; ++i, ++cp )
    {
        if ( *cp == cc )
            return (i > INT_MAX) ? INT_MAX : (int)i;
    }

    return -1;
}
#endif

ctmbstr TY_(tmbsubstrn)( ctmbstr s1, size_t len1, ctmbstr s2 )
{
    size_t len2 = TY_(tmbstrlen)(s2);
    long diff = len1 - len2;

    if ( len1 < len2 )
        return NULL;

    for ( size_t ix = 0; ix <= (size_t)diff; ++ix )
    {
        if ( TY_(tmbstrncmp)(s1+ix, s2, len2) == 0 )
            return (ctmbstr) s1+ix;
    }
    return NULL;
}

#if 0
ctmbstr TY_(tmbsubstrncase)( ctmbstr s1, size_t len1, ctmbstr s2 )
{
    size_t len2 = TY_(tmbstrlen)(s2);
    long diff = len1 - len2;

    if ( len1 < len2 )
        return NULL;

    for ( size_t ix = 0; ix <= (size_t)diff; ++ix )
    {
        if ( TY_(tmbstrncasecmp)(s1+ix, s2, len2) == 0 )
            return (ctmbstr) s1+ix;
    }
    return NULL;
}
#endif

ctmbstr TY_(tmbsubstr)( ctmbstr s1, ctmbstr s2 )
{
    size_t len1 = TY_(tmbstrlen)(s1), len2 = TY_(tmbstrlen)(s2);
    long diff = len1 - len2;

    if ( len1 < len2 )
        return NULL;

    for ( size_t ix = 0; ix <= (size_t)diff; ++ix )
    {
        if ( TY_(tmbstrncasecmp)(s1+ix, s2, len2) == 0 )
            return (ctmbstr) s1+ix;
    }
    return NULL;
}

/* Transform ASCII chars in string to lower case */
tmbstr TY_(tmbstrtolower)( tmbstr s )
{
    for ( tmbstr cp=s; *cp; ++cp )
        *cp = (tmbchar) TY_(ToLower)( *cp );
    return s;
}

/* Transform ASCII chars in string to upper case */
tmbstr TY_(tmbstrtoupper)(tmbstr s)
{
    tmbstr cp;

    for (cp = s; *cp; ++cp)
        *cp = (tmbchar)TY_(ToUpper)(*cp);

    return s;
}

#if 0
Bool TY_(tmbsamefile)( ctmbstr filename1, ctmbstr filename2 )
{
#if FILENAMES_CASE_SENSITIVE
    return ( TY_(tmbstrcmp)( filename1, filename2 ) == 0 );
#else
    return ( TY_(tmbstrcasecmp)( filename1, filename2 ) == 0 );
#endif
}
#endif

int TY_(tmbvsnprintf)(tmbstr buffer, size_t count, ctmbstr format, va_list args)
{
    int retval;
#if HAS_VSNPRINTF
    retval = vsnprintf(buffer, count - 1, format, args);
    /* todo: conditionally null-terminate the string? */
    buffer[count - 1] = 0;
#else
    retval = vsprintf(buffer, format, args);
#endif /* HAS_VSNPRINTF */
    return retval;
}

int TY_(tmbsnprintf)(tmbstr buffer, size_t count, ctmbstr format, ...)
{
    int retval;
    va_list args;
    va_start(args, format);
#if HAS_VSNPRINTF
    retval = vsnprintf(buffer, count - 1, format, args);
    /* todo: conditionally null-terminate the string? */
    buffer[count - 1] = 0;
#else
    retval = vsprintf(buffer, format, args);
#endif /* HAS_VSNPRINTF */
    va_end(args);
    return retval;
}

/*
 * local variables:
 * mode: c
 * indent-tabs-mode: nil
 * c-basic-offset: 4
 * eval: (c-set-offset 'substatement-open 0)
 * end:
 */
