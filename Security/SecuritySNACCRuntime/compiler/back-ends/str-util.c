/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 * compiler/back_ends/c_gen/str_util.c  - bunch of ASN.1/C string utilities
 *
 *
 * Mike Sample
 * 91/08/12
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/compiler/back-ends/str-util.c,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
 * $Log: str-util.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:08  mb
 * Move from private repository to open source repository
 *
 * Revision 1.3  2001/05/05 00:59:27  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.2  2000/05/10 21:36:43  rmurphy
 * changing the suffix for c++ output files to .cpp - requires -DMACOS on the compilation line
 *
 * Revision 1.1.1.1  1999/03/16 18:06:39  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.4  1995/07/25 18:13:31  rj
 * include string(s).h
 *
 * by default, snacc now derives output file names from the .asn1 input file name instead of the module name.
 * the global keepbaseG variable switches between the two behaviours.
 *
 * additional filename generator for idl backend.
 *
 * changed `_' to `-' in file names.
 *
 * Revision 1.3  1994/10/08  03:48:17  rj
 * since i was still irritated by cpp standing for c++ and not the C preprocessor, i renamed them to cxx (which is one known suffix for C++ source files). since the standard #define is __cplusplus, cplusplus would have been the more obvious choice, but it is a little too long.
 *
 * Revision 1.2  1994/09/01  00:25:31  rj
 * snacc_config.h removed; more portable .h file inclusion.
 *
 * Revision 1.1  1994/08/28  09:48:37  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include "asn-incl.h"

#include <ctype.h>
#if HAVE_UNISTD_H
#include <unistd.h>  /* for pathconf (..) */
#endif
#if STDC_HEADERS || HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <stdio.h>

#include "asn1module.h"
#include "mem.h"
#include "define.h"
#include "c-gen/rules.h"
#include "c-gen/type-info.h"
#include "c-gen/kwd.h"
#include "c++-gen/kwd.h"
#include "str-util.h"


#define DIGIT_TO_ASCII( d)	(((d) % 10) + '0')

int	keepbaseG = TRUE;

/*
 * allocates new and returns a copy of the given
 * string with '-'s (dashes) replaced by  '_'s (underscores)
 */
char *
Asn1TypeName2CTypeName PARAMS ((aName),
    char *aName)
{
    char *retVal;
    if (aName == NULL)
        return NULL;

    retVal = Malloc (strlen (aName) + 1);
    strcpy (retVal, aName);
    Dash2Underscore (retVal, strlen (retVal));

    return retVal;
}  /* Asn1TypeName2CTypeName */


/*
 * allocates new str and returns a copy of the given
 * string with '-'s (dashes) replaced by  '_'s (underscores)
 */
char *
Asn1FieldName2CFieldName PARAMS ((aName),
    char *aName)
{
    char *retVal;
    if (aName == NULL)
        return NULL;

    retVal = Malloc (strlen (aName) + 1);
    strcpy (retVal, aName);
    Dash2Underscore (retVal, strlen (retVal));

    return retVal;
}  /* Asn1FieldName2CFieldName */


/*
 * allocates new str and returns a copy of the given
 * string with '-'s (dashes) replaced by  '_'s (underscores)
 */
char *
Asn1ValueName2CValueName PARAMS ((aName),
    char *aName)
{
    char *retVal;
    if (aName == NULL)
        return NULL;

    retVal = Malloc (strlen (aName) + 1);
    strcpy (retVal, aName);
    Dash2Underscore (retVal, strlen (retVal));

    return retVal;
}  /* Asn1FieldName2CFieldName */


/*
 * allocates and returns a string with all of
 * the caps from the given string
 */
char *
GetCaps PARAMS ((str),
    char *str)
{
    int i, j;
    char *retVal;

    if (str == NULL)
        return NULL;

    retVal = Malloc (strlen (str) + 1);

    for (j = 0, i = 0; i < strlen (str); i++)
    {
        if (isupper (str[i]))
            retVal[j++] = str[i];
    }

    retVal[j] = '\0';  /* null terminate */

    return retVal;

}  /* GetCaps */


/*
 * allocates and returns a string with all of
 * the caps and digits from the given string
 */
char *
GetCapsAndDigits PARAMS ((str),
    char *str)
{
    int i, j;
    char *retVal;

    if (str == NULL)
        return NULL;

    retVal = Malloc (strlen (str) + 1);

    for (j = 0, i = 0; i < strlen (str); i++)
    {
        if ((isupper (str[i])) ||  (isdigit (str[i])))
            retVal[j++] = str[i];
    }

    retVal[j] = '\0';  /* null terminate */

    return retVal;

}  /* GetCapsAndDigits */


/*
 * replaces lowercase chars in given str
 * with upper case version
 * NOTE: modifies given str
 */
void
Str2UCase PARAMS ((str, len),
    char *str _AND_
    int len)
{
    int i;
    for (i=0; i < len; i++)
    {
        if (islower (str[i]))
            str[i] = toupper (str[i]);
    }
} /* Str2UCase */


/*
 * replaces uppercase chars in given str
 * with lower case version
 * NOTE: modifies given str
 */
void
Str2LCase PARAMS ((str, len),
    char *str _AND_
    int len)
{
    int i;
    for (i=0; i < len; i++)
    {
        if (isupper (str[i]))
            str[i] = tolower (str[i]);
    }
} /* Str2LCase */


/*
 * replace dash chars in given str
 * with underscores
 * NOTE: modifies given str
 */
void
Dash2Underscore PARAMS ((str, len),
    char *str _AND_
    int len)
{
    int i;
    for (i=0; i < len; i++)
    {
        if (str[i] == '-')
            str[i] = '_';
    }
} /* Dash2Underscore */


/*
 * tacks on the ascii version of the given digit
 * at the end of the given str.
 * NOTE: make sure the str you give has enough space
 * for the digits
 */
void
AppendDigit PARAMS ((str, digit),
    char *str _AND_
    int digit)
{
    int high = 1000000000;
    int currDigit;
    int value;
    char digitStr[20]; /* arbitrary length > max */

    if (digit < 0)
        digit *= -1;

    currDigit = 0;
    while (high > 0)
    {
        value = digit / high;
        if (value != 0)
            digitStr[currDigit++]= DIGIT_TO_ASCII (value);

        digit = digit % high;
        high  = high/10;
    }

    if (currDigit == 0)
        strcat (str, "0");
    else
    {
        digitStr[currDigit] = '\0';  /* null terminate */
        strcat (str, digitStr);
    }
} /* AppendDigit */




/*
 * given a defined object list containing null termintated strs,
 * a str to be made unique wrt to the list by adding digits to the
 * end, the max number of digits to add and the digit to start
 * at, str is modified to be unique.  It is not added to the
 * defined object list.  The given str must have enough spare,
 * allocated chars after it's null terminator to hold maxDigits
 * more characters.
 * Only appends digits if the string is not unique or is a C keyword.
 *
 * Eg  MakeCStrUnique ({ "Foo", "Bar" }, "Foo\0   ", 3, 1)
 *         modifies the the Str "Foo" to "Foo1"
 */
void
MakeCStrUnique PARAMS ((nameList, str, maxDigits, startingDigit),
    DefinedObj *nameList _AND_
    char *str _AND_
    int maxDigits _AND_
    int startingDigit)
{
    int digit, len, maxDigitVal;

    if (ObjIsDefined (nameList, str, StrObjCmp) || IsCKeyWord (str))
    {
        for (maxDigitVal = 1; maxDigits > 0; maxDigits--)
            maxDigitVal *= 10;

        len = strlen (str);
        digit = startingDigit;
        do
        {
            str[len] = '\0';
            AppendDigit (str, digit++);
        } while (ObjIsDefined (nameList, str, StrObjCmp) &&  (digit < maxDigitVal));
    }
}  /* MakeCStrUnique */


/*
 * same as MakeCStrUnique except checks against C++ keywords
 */
void
MakeCxxStrUnique PARAMS ((nameList, str, maxDigits, startingDigit),
    DefinedObj *nameList _AND_
    char *str _AND_
    int maxDigits _AND_
    int startingDigit)
{
    int digit, len, maxDigitVal;

    if (ObjIsDefined (nameList, str, StrObjCmp) || IsCxxKeyWord (str))
    {
        for (maxDigitVal = 1; maxDigits > 0; maxDigits--)
            maxDigitVal *= 10;

        len = strlen (str);
        digit = startingDigit;
        do
        {
            str[len] = '\0';
            AppendDigit (str, digit++);
        } while (ObjIsDefined (nameList, str, StrObjCmp) &&  (digit < maxDigitVal));
    }
}  /* MakeCxxStrUnique */


/*
 * if (keepbaseG)
 * {
 *   strip leading path and trailing suffix
 * }
 * else
 * {
 *   allocates and returns a base file name generated from
 *   the module's name.  May shorten the name if the
 *   expected length exceed the systems max path component length
 *   (eg to support SYS V 14 char filename len limit)
 * }
 * Base file name is used as the base name for the generated C source files.
 */
char *
MakeBaseFileName PARAMS ((refName),
    const char *refName)
{
  if (keepbaseG)
  {
    char	*base, *dot;
    int		stublen;
    char	*stub;

    if (base = strrchr (refName, '/'))
      base++;
    else
      base = refName;

    if (dot = strrchr (base, '.'))
      stublen = dot - base;
    else
      stublen = strlen (base);

    stub = Malloc (stublen+1);
    memcpy (stub, base, stublen);
    stub[stublen] = '\0';

    return stub;
  }
  else
  {
    int fNameLen;
    int cpyLen;
    char *retVal;
    int maxPathComponentLen;
    char pathName[1024];
#   define MAX_SUFFIX_LEN 2 /* .c, .h, .C */
    extern int maxFileNameLenG; /* declared in snacc.c */

    /*
     * if the user has not given the max file name len
     * via the -mf option,
     * find the max filename len (ala POSIX method)
     * if possible.  Otherwise hardwire it to 14
     * to support underpowered OSes
     */
    if (maxFileNameLenG > 2)
        maxPathComponentLen = maxFileNameLenG;
    else
#ifdef _PC_NAME_MAX
        maxPathComponentLen = pathconf (getcwd (pathName, 1024), _PC_NAME_MAX);
#else
        maxPathComponentLen = 14;
#endif

    retVal = (char *)Malloc (strlen (refName) +1);
    fNameLen = strlen (refName) + MAX_SUFFIX_LEN;
    if ((fNameLen > maxPathComponentLen) && (maxPathComponentLen != -1))
    {
        cpyLen = maxPathComponentLen - MAX_SUFFIX_LEN;

        /* don't allow trailing dash */
        if (refName[cpyLen-1] == '-')
            cpyLen--;

        strncpy (retVal, refName, cpyLen);
        retVal[cpyLen] = '\0';
    }
    else
        strcpy (retVal, refName);

    return retVal;
  }
} /* MakeBaseFileName */




/*
 * given a module name and a suffix, the
 * suffix is appended to the module name
 * and the whole string is put into lower case
 * and underscores are inserted in likely places
 * (ie MTSAbstractSvc.h -> mts_abstract_svc.h)
 */
char *
MakeFileName PARAMS ((refName, suffix),
    const char *refName _AND_
    const char *suffix)
{
  if (keepbaseG)
  {
    size_t	baselen = strlen (refName),
		sufflen = strlen (suffix);
    char *filename = Malloc (baselen + sufflen + 1);

    memcpy (filename, refName, baselen);
    memcpy (filename+baselen, suffix, sufflen);
    filename[baselen+sufflen] = '\0';

    return filename;
  }
  else
  {
    int i, cpyIndex, len;
    char *hdrCpy;
    int fNameLen;
    char *fName;
#define MAX_UNDERSCORE 10

    fName = Malloc (strlen (refName) + strlen (suffix) + 1);
    strcpy (fName, refName);
    strcat (fName, suffix);


    fNameLen = strlen (fName);

    /*
     * convert dashes to underscores, add spaces
     */
    Dash2Underscore (fName, fNameLen);


    /*
     * remove the next two lines if you uncomment the
     * following underscore inserter
     */
    Str2LCase (fName, fNameLen - strlen (suffix));
    return fName;

    /*
     *  NO LONGER DONE - LET THE USER MODIFY THE ASN.1 IF DESIRED
     *  add underscore between Lcase/Ucase of UCase/UcaseLcasce
     *  eg MTSAbstractSvc -> MTS_Abstract_Svc
     *  (if enough space)
    len = strlen (fName) + MAX_UNDERSCORE + 1;
    hdrCpy = (char *) Malloc (len);

    hdrCpy[0] = fName[0];
    for (i = 1, cpyIndex = 1; (cpyIndex < len) && (i < fNameLen); i++)
    {
        if (((islower (fName[i-1])) && (isupper (fName[i]))) ||
             ((isupper (fName[i-1])) && (isupper (fName[i])) &&
                            ((i < (fNameLen-1)) && (islower (fName[i+1])))))
        {
            hdrCpy[cpyIndex++] = '_';
            hdrCpy[cpyIndex++] = fName[i];
        }
        else
            hdrCpy[cpyIndex++] = fName[i];
    }
    hdrCpy[cpyIndex++] = '\0';

    Str2LCase (hdrCpy, cpyIndex - strlen (suffix));

    Free (fName);
    return hdrCpy;
    */
  }
}  /* MakeFileName */


char *
MakeCHdrFileName PARAMS ((refName),
    const char *refName)
{
    return MakeFileName (refName, ".h");
}

char *
MakeCSrcFileName PARAMS ((refName),
    const char *refName)
{
    return MakeFileName (refName, ".c");
}

char *
MakeCxxHdrFileName PARAMS ((refName),
    const char *refName)
{
    return MakeFileName (refName, ".h");
}

char *
MakeCxxSrcFileName PARAMS ((refName),
    const char *refName)
{
#ifndef MACOS
    return MakeFileName (refName, ".C");
#else
    return MakeFileName (refName, ".cpp");      /* ignore cpp rant */
#endif
}

#ifdef _IBM_ENC_
char *                                           /* 19.8.93 IBM-ENC */
MakedbHdrFileName PARAMS ((refName),
    const char *refName)
{
    return MakeFileName (refName, "db.h");
}

char *                                           /* 19.8.93 IBM-ENC */
MakedbSrcFileName PARAMS ((refName),
    const char *refName)
{
    return MakeFileName (refName, "db.C");
}
#endif /* _IBM_ENC_ */

#if IDL
char *
MakeIDLFileName PARAMS ((refName),
    const char *refName)
{
    return MakeFileName (refName, ".idl");
}
#endif
