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
 * asn_any.c - BER encode, decode, print, free, type set up and installation
 *             routines for the ASN.1 ANY and ANY DEFINED BY types.
 *
 * MS 92
 * Copyright (C) 1992 Michael Sample and the University of British Columbia
 *
 * This library is free software; you can redistribute it and/or
 * modify it provided that this copyright/license information is retained
 * in original form.
 *
 * If you modify this file, you must clearly indicate your changes.
 *
 * This source code is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Header: /cvs/Darwin/src/live/Security/SecuritySNACCRuntime/c-lib/src/asn-any.c,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
 * $Log: asn-any.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:08  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:25  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:30  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1997/02/28 13:39:49  wan
 * Modifications collected for new version 1.3: Bug fixes, tk4.2.
 *
 * Revision 1.2  1995/07/24 21:04:48  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.1  1994/08/28  09:45:49  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include "asn-config.h"
#include "asn-len.h"
#include "asn-tag.h"
#include "asn-oid.h"
#include "asn-int.h"
#include "asn-any.h"

/*
 * 2 hash tables. 1 for INTEGER to type mappings the other
 * for OBJECT IDENTIFER to type mappings.
 */
Table *anyOidHashTblG = NULL;
Table *anyIntHashTblG = NULL;

/*
 * given an ANY type value and a integer hash key, this defines
 * this any values type (gets ptr to hash tbl entry from int key).
 * The hash table entry contains ptrs to the encode/decode etc. routines.
 */
void
SetAnyTypeByInt PARAMS ((v, id),
    AsnAny *v _AND_
    AsnInt id)
{
    Hash hash;
    void *anyInfo;

    /* use int as hash string */
    hash = MakeHash ((char*)&id, sizeof (id));
    if (CheckForAndReturnValue (anyIntHashTblG, hash, &anyInfo))
        v->ai = (AnyInfo*) anyInfo;
    else
        v->ai = NULL; /* indicates failure */

} /* SetAnyTypeByInt */


/*
 * Same as SetAnyTypeByInt except that the hash key is an OBJECT IDENTIFER.
 */
void SetAnyTypeByOid PARAMS ((v, id),
    AsnAny *v _AND_
    AsnOid *id)
{
    Hash hash;
    void *anyInfo;

    /* use encoded oid as hash string */
    hash = MakeHash (id->octs, id->octetLen);
    if (CheckForAndReturnValue (anyOidHashTblG, hash, &anyInfo))
        v->ai = (AnyInfo*) anyInfo;
    else
        v->ai = NULL; /* indicates failure */

} /* SetAnyTypeByOid */


/*
 * Creates an entry in the hash table that contains the
 * type's size, encode, decode, free, and print routines and anyId.
 * The given intId is used as the hash key so future calls to
 * SetAnyTypeByInt with that intId as the id will reference this entry.
 * The anyId is stored in the hash tbl entry as well so the user can
 * figure out the type with a simple integer comparison.
 *
 * This routine is usually called from the AnyInit routine that
 * the compiler generates from MACRO info.  Call this routine
 * once for each possible ANY type to set up the hash table.
 * Future calls to SetAnyTypeByInt/Oid will reference this table.
 */
void
InstallAnyByInt PARAMS ((anyId, intId, size, Encode, Decode, Free, Print),
    int anyId _AND_
    AsnInt intId _AND_
    unsigned int size _AND_
    EncodeFcn Encode _AND_
    DecodeFcn Decode _AND_
    FreeFcn Free _AND_
    PrintFcn Print)
{
    AnyInfo *a;
    Hash h;

    a = (AnyInfo*) malloc (sizeof (AnyInfo));
    a->anyId = anyId;
    a->oid.octs = NULL;
    a->oid.octetLen = 0;
    a->intId = intId;
    a->size = size;
    a->Encode = Encode;
    a->Decode = Decode;
    a->Free = Free;
    a->Print = Print;

    if (anyIntHashTblG == NULL)
        anyIntHashTblG = InitHash();

    h = MakeHash ((char*)&intId, sizeof (intId));
    Insert (anyIntHashTblG, a, h);

}  /* InstallAnyByOid */


/*
 * Same as InstallAnyByInt except the oid is used as the hash key
 */
void
InstallAnyByOid PARAMS ((anyId, oid, size, Encode, Decode, Free, Print),
    int anyId _AND_
    AsnOid *oid _AND_
    unsigned int size _AND_
    EncodeFcn Encode _AND_
    DecodeFcn Decode _AND_
    FreeFcn Free _AND_
    PrintFcn Print)
{
    AnyInfo *a;
    Hash h;

    a = (AnyInfo*) malloc (sizeof (AnyInfo));
    a->anyId = anyId;
    a->oid.octs = oid->octs;
    a->oid.octetLen = oid->octetLen;
    a->size = size;
    a->Encode = Encode;
    a->Decode = Decode;
    a->Free = Free;
    a->Print = Print;

    h = MakeHash (oid->octs, oid->octetLen);

    if (anyOidHashTblG == NULL)
        anyOidHashTblG = InitHash();

    Insert (anyOidHashTblG, a, h);

}  /* InstallAnyByOid */


/*
 * Calls the free routine in this type's any info.
 * If the routine ptr is NULL, nothing is done
 * (This is the case for INTEGERs, BOOLEANs and other simple
 * values)
 */
void
FreeAsnAny PARAMS ((v),
    AsnAny *v)
{
    if ((v->ai != NULL) && (v->ai->Free != NULL))
        v->ai->Free (v->value);
} /* FreeAsnAny */


/*
 * Calls the Encode routine pointed to in the given type's
 * Any Info.  If the routine ptr is NULL nothing is encoded
 * (This should set some type of error).
 * Note: this calls the BEncFoo not BEncFooContent routine form
 * since the tags are needed too.
 */
AsnLen
BEncAsnAny PARAMS ((b, v),
    BUF_TYPE b _AND_
    AsnAny *v)
{
    if ((v->ai != NULL) && (v->ai->Encode != NULL))
        return v->ai->Encode (b, v->value);
    else
        return 0;
} /* BEncAsnAny */


/*
 * Calls the Decode routine pointed to in the given type's
 * Any Info.  If the routine ptr is NULL any error is flagged.
 * Note: this calls the BDecFoo not BDecFooContent routine form
 * since the tags are needed too.
 */
void BDecAsnAny PARAMS ((b, result, bytesDecoded, env),
    BUF_TYPE b _AND_
    AsnAny  *result _AND_
    AsnLen *bytesDecoded _AND_
    ENV_TYPE env)
{
    if ((result->ai != NULL) && (result->ai->Decode != NULL))
    {
        result->value = (void*) Asn1Alloc (result->ai->size);
        result->ai->Decode (b, result->value, bytesDecoded, env);
    }
    else
    {
        Asn1Error ("ERROR - ANY Decode routine is NULL\n");
        longjmp (env, -44);
    }
}

/*
 * Calls the print routine pointed to from the given type's
 * Any Info.  Prints an error if the type does not have
 * any 'AnyInfo' or if the AnyInfo has a NULL Print routine ptr.
 */
void PrintAsnAny PARAMS ((f, v, indent),
    FILE *f _AND_
    AsnAny *v _AND_
    unsigned short indent)
{
    if ((v->ai != NULL) && (v->ai->Print != NULL))
        v->ai->Print (f, v->value);
    else
        fprintf (f," -- ERROR: malformed ANY value --");
}
