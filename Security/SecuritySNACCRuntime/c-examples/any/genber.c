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
 * c-examples/any/genber.c - builds an AnyTestType value and writes BER form
 *            of the value to a file called "att.ber"
 *
 *  Shows how to build internal rep of lists and ANY values.
 *
 *  MS 92
 *
 * $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c-examples/any/genber.c,v 1.1.1.1 2001/05/18 23:14:07 mb Exp $
 * $Log: genber.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:07  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:19  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:08  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.5  1995/07/24 20:40:50  rj
 * any-test.[hc] becomes any.[hc] due to to snacc's new file name generation scheme.
 *
 * changed `_' to `-' in file names.
 *
 * Revision 1.4  1995/02/18  15:17:36  rj
 * cosmetic changes
 *
 * Revision 1.3  1994/08/31  23:48:06  rj
 * more portable .h file inclusion.
 *
 * Revision 1.2  1994/08/31  08:59:32  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include <sys/types.h> /* this must be before stddef for gcc-2.3.1 */
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <stdio.h>

#include "asn-incl.h"
#include "any.h"


main (int argc, char *argv[])
{
    FILE *outputFile;
    SBuf outputBuf;
    unsigned long int encodedLen;
    int dataSize = 1024;
    int i;
    char data[1024];
    AnyTestType att;
    TSeq1 ts1;
    TSeq2 ts2;
    AttrValue1 **atv1Hndl;
    AttrValue2 **atv2Hndl;
    AsnInt intVal;
    AsnBool boolVal;
    AsnOcts octsVal;
    AsnBits bitsVal;
    AsnReal realVal;

    /* used to alloc part of value (Asn1Alloc & AsnListAppend) */
    InitNibbleMem (512,512);

    /* init id to type ANY hash table */
    InitAnyANY_TEST();

    att.intMap = AsnListNew (sizeof (void*));
    atv1Hndl = (AttrValue1**)AsnListAppend (att.intMap);
    *atv1Hndl = (AttrValue1*) Asn1Alloc (sizeof (AttrValue1));
    (*atv1Hndl)->id = intId;  /* the id's are defined in the generated code */
    intVal = -99;
    (*atv1Hndl)->anyDefBy.value = (void*) &intVal;

    atv1Hndl = (AttrValue1**)AsnListAppend (att.intMap);
    *atv1Hndl = (AttrValue1*) Asn1Alloc (sizeof (AttrValue1));
    (*atv1Hndl)->id = boolId;
    boolVal = TRUE;
    (*atv1Hndl)->anyDefBy.value = (void*)&boolVal;

    atv1Hndl = (AttrValue1**)AsnListAppend (att.intMap);
    *atv1Hndl = (AttrValue1*) Asn1Alloc (sizeof (AttrValue1));
    (*atv1Hndl)->id = octsId;
    octsVal.octs = "Hi Mom";
    octsVal.octetLen = strlen (octsVal.octs);
    (*atv1Hndl)->anyDefBy.value = (void*)&octsVal;

    atv1Hndl = (AttrValue1**)AsnListAppend (att.intMap);
    *atv1Hndl = (AttrValue1*) Asn1Alloc (sizeof (AttrValue1));
    (*atv1Hndl)->id = bitsId;
    bitsVal.bitLen = 10;
    bitsVal.bits = (char*)&i;
    SetAsnBit (&bitsVal, 0);
    ClrAsnBit (&bitsVal, 1);
    SetAsnBit (&bitsVal, 2);
    ClrAsnBit (&bitsVal, 3);
    SetAsnBit (&bitsVal, 4);
    ClrAsnBit (&bitsVal, 5);
    SetAsnBit (&bitsVal, 6);
    ClrAsnBit (&bitsVal, 7);
    SetAsnBit (&bitsVal, 8);
    ClrAsnBit (&bitsVal, 9);
    (*atv1Hndl)->anyDefBy.value = (void*)&bitsVal;

    atv1Hndl = (AttrValue1**)AsnListAppend (att.intMap);
    *atv1Hndl = (AttrValue1*) Asn1Alloc (sizeof (AttrValue1));
    (*atv1Hndl)->id = realId;
    realVal = 108.3838;
    (*atv1Hndl)->anyDefBy.value = (void*)&realVal;

    /* now do TSeq2 with same vals but use OID as identifier */
    att.oidMap = AsnListNew (sizeof (void*));

    atv2Hndl = (AttrValue2**)AsnListAppend (att.oidMap);
    *atv2Hndl = (AttrValue2*) Asn1Alloc (sizeof (AttrValue2));
    (*atv2Hndl)->id = intOid;
    (*atv2Hndl)->anyDefBy.value = (void*)&intVal;

    atv2Hndl = (AttrValue2**)AsnListAppend (att.oidMap);
    *atv2Hndl = (AttrValue2*) Asn1Alloc (sizeof (AttrValue2));
    (*atv2Hndl)->id = boolOid;
    (*atv2Hndl)->anyDefBy.value = (void*)&boolVal;

    atv2Hndl = (AttrValue2**)AsnListAppend (att.oidMap);
    *atv2Hndl = (AttrValue2*) Asn1Alloc (sizeof (AttrValue2));
    (*atv2Hndl)->id = octsOid;
    (*atv2Hndl)->anyDefBy.value = (void*)&octsVal;

    atv2Hndl = (AttrValue2**)AsnListAppend (att.oidMap);
    *atv2Hndl = (AttrValue2*) Asn1Alloc (sizeof (AttrValue2));
    (*atv2Hndl)->id = bitsOid;
    (*atv2Hndl)->anyDefBy.value = (void*)&bitsVal;

    atv2Hndl = (AttrValue2**)AsnListAppend (att.oidMap);
    *atv2Hndl = (AttrValue2*) Asn1Alloc (sizeof (AttrValue2));
    (*atv2Hndl)->id = realOid;
    (*atv2Hndl)->anyDefBy.value = (void*)&realVal;

    SBufInit (&outputBuf,data, dataSize);
    SBufResetInWriteRvsMode (&outputBuf);

    encodedLen = BEncAnyTestType (&outputBuf, &att);
    if ((encodedLen <= 0) || (SBufWriteError (&outputBuf)))
    {
        fprintf (stderr, "failed encoding AnyTestType value\n");
        exit (1);
    }

    outputFile = fopen ("att.ber", "w");
    if (!outputFile)
    {
        perror ("fopen:");
        exit (1);
    }

    SBufResetInReadMode (&outputBuf);
    for ( ; encodedLen > 0; encodedLen--)
        fputc (SBufGetByte (&outputBuf), outputFile);


    printf ("Wrote the following BER AnyTestType value to att.ber.\n");
    printf ("Test it with \"def\" and \"indef\"\n");

    PrintAnyTestType (stdout, &att, 0);
    printf ("\n");

    return 0;
}
