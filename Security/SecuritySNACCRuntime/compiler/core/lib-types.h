/*
 * compiler/core/lib_types.h
 *
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/root/Security/SecuritySNACCRuntime/compiler/core/Attic/lib-types.h,v 1.1 2001/06/20 21:27:57 dmitch Exp $
 * $Log: lib-types.h,v $
 * Revision 1.1  2001/06/20 21:27:57  dmitch
 * Adding missing snacc compiler files.
 *
 * Revision 1.1.1.1  1999/03/16 18:06:49  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/25 19:41:34  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/10/08  03:48:46  rj
 * since i was still irritated by cpp standing for c++ and not the C preprocessor, i renamed them to cxx (which is one known suffix for C++ source files). since the standard #define is __cplusplus, cplusplus would have been the more obvious choice, but it is a little too long.
 *
 * Revision 1.1  1994/08/28  09:49:15  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */


typedef struct LibType
{
    enum BasicTypeChoiceId typeId;
    BER_UNIV_CODE          univTagCode;
    BER_FORM               tagForm;
    AnyRefList            *anyRefs;  /* these may be filled in do_macros.c*/
}  LibType;


extern LibType libTypesG[];

#define LIBTYPE_GET_UNIV_TAG_CODE( tId)		(libTypesG[tId].univTagCode)
#define LIBTYPE_GET_TAG_FORM( tId)		(libTypesG[tId].tagForm)
#define LIBTYPE_GET_ANY_REFS( tId)		(libTypesG[tId].anyRefs)
#define LIBTYPE_GET_ANY_REFS_HNDL( tId)		(&libTypesG[tId].anyRefs)
#define LIBTYPE_GET_ANY_REFS( tId)		(libTypesG[tId].anyRefs)
