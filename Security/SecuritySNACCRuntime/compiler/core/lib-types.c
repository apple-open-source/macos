/*
 * compiler/core/lib_types.c - tag form/code and any refs info
 *
 *
 * Copyright (C) 1991, 1992 Michael Sample
 *            and the University of British Columbia
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /cvs/root/Security/SecuritySNACCRuntime/compiler/core/Attic/lib-types.c,v 1.1 2001/06/20 21:27:57 dmitch Exp $
 * $Log: lib-types.c,v $
 * Revision 1.1  2001/06/20 21:27:57  dmitch
 * Adding missing snacc compiler files.
 *
 * Revision 1.1.1.1  1999/03/16 18:06:49  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/25 19:41:33  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/09/01  00:37:51  rj
 * snacc_config.h removed.
 *
 * Revision 1.1  1994/08/28  09:49:14  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include "asn-incl.h"
#include "asn1module.h"  /* for BASICTYPE_... choice ids */
#include "lib-types.h"

/*
 * Warning: this table must be in order of ascending
 * BASICTYPE ids such that
 *    libTypesG[BASICTYPE_X].typeId == BASICTYPE_X
 * is alwas true
 */
LibType libTypesG[ BASICTYPE_MACRODEF + 1] =
{
  { BASICTYPE_UNKNOWN,       NO_TAG_CODE,          NULL_FORM, NULL },
  { BASICTYPE_BOOLEAN,       BOOLEAN_TAG_CODE,     PRIM,      NULL },
  { BASICTYPE_INTEGER,       INTEGER_TAG_CODE,     PRIM,      NULL },
  { BASICTYPE_BITSTRING,     BITSTRING_TAG_CODE,   ANY_FORM,  NULL },
  { BASICTYPE_OCTETSTRING,   OCTETSTRING_TAG_CODE, ANY_FORM,  NULL },
  { BASICTYPE_NULL,          NULLTYPE_TAG_CODE,    PRIM,      NULL },
  { BASICTYPE_OID,           OID_TAG_CODE,         PRIM,      NULL },
  { BASICTYPE_REAL,          REAL_TAG_CODE,        PRIM,      NULL },
  { BASICTYPE_ENUMERATED,    ENUM_TAG_CODE,        PRIM,      NULL },
  { BASICTYPE_SEQUENCE,      SEQ_TAG_CODE,         CONS,      NULL },
  { BASICTYPE_SEQUENCEOF,    SEQ_TAG_CODE,         CONS,      NULL },
  { BASICTYPE_SET,           SET_TAG_CODE,         CONS,      NULL },
  { BASICTYPE_SETOF,         SET_TAG_CODE,         CONS,      NULL },
  { BASICTYPE_CHOICE,        NO_TAG_CODE,          CONS,      NULL },
  { BASICTYPE_SELECTION,     NO_TAG_CODE,          NULL_FORM, NULL },
  { BASICTYPE_COMPONENTSOF,  NO_TAG_CODE,          CONS,      NULL },
  { BASICTYPE_ANY,           NO_TAG_CODE,          CONS,      NULL },
  { BASICTYPE_ANYDEFINEDBY,  NO_TAG_CODE,          CONS,      NULL },
  { BASICTYPE_LOCALTYPEREF,  NO_TAG_CODE,          NULL_FORM, NULL },
  { BASICTYPE_IMPORTTYPEREF, NO_TAG_CODE,          NULL_FORM, NULL },
  { BASICTYPE_MACROTYPE,     NO_TAG_CODE,          NULL_FORM, NULL },
  { BASICTYPE_MACRODEF,      NO_TAG_CODE,          NULL_FORM, NULL }
};
