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
 * c_examples/simple/genber.c - builds a PersonnelRecord value and writes BER form
 *            of the value to a file called "pr.ber"
 *
 *
 *  MS 92
 *
 * $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c-examples/simple/genber.c,v 1.1.1.1 2001/05/18 23:14:07 mb Exp $
 * $Log: genber.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:07  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:20  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:08  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.6  1995/07/24 20:45:00  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.5  1995/02/18  15:12:54  rj
 * cosmetic changes
 *
 * Revision 1.4  1995/02/17  16:21:03  rj
 * unnecessary inclusion of <sys/stdtypes.h> removed.
 *
 * Revision 1.3  1994/09/01  01:02:37  rj
 * more portable .h file inclusion.
 *
 * Revision 1.2  1994/08/31  08:59:35  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include "asn-incl.h"

#include <sys/file.h>
#include <sys/stat.h>

#include <stddef.h>
#if STDC_HEADERS
#include <stdlib.h>
#endif
#include <errno.h>
#include <stdio.h>

#include "p-rec.h"


main (int argc, char *argv[])
{
    FILE *outputFile;
    SBuf outputBuf;
    unsigned long int encodedLen;
    int dataSize = 1024;
    int i;
    char data[1024];
    PersonnelRecord pr;
    ChildInformation **childHndl;

    /* used to alloc part of value (Asn1Alloc & AsnListAppend) */
    InitNibbleMem (512,512);

    pr.name = Asn1Alloc (sizeof (Name));
    pr.name->givenName.octs = "John";
    pr.name->givenName.octetLen = strlen (pr.name->givenName.octs);
    pr.name->initial.octs = "E";
    pr.name->initial.octetLen = strlen (pr.name->initial.octs);
    pr.name->familyName.octs = "Smith";
    pr.name->familyName.octetLen = strlen (pr.name->familyName.octs);

    pr.title.octs = "The Big Cheese";
    pr.title.octetLen = strlen (pr.title.octs);

    pr.employeeNumber = 99999;

    pr.dateOfHire.octs = "19820104";
    pr.dateOfHire.octetLen = strlen (pr.dateOfHire.octs);

    pr.nameOfSpouse = (Name*) Asn1Alloc (sizeof (Name));
    pr.nameOfSpouse->givenName.octs = "Mary";
    pr.nameOfSpouse->givenName.octetLen =
        strlen (pr.nameOfSpouse->givenName.octs);
    pr.nameOfSpouse->initial.octs = "L";
    pr.nameOfSpouse->initial.octetLen = strlen (pr.nameOfSpouse->initial.octs);
    pr.nameOfSpouse->familyName.octs = "Smith";
    pr.nameOfSpouse->familyName.octetLen =
        strlen (pr.nameOfSpouse->familyName.octs);

    pr.children = AsnListNew (sizeof (void*));

    childHndl = AsnListAppend (pr.children);
    *childHndl = Asn1Alloc (sizeof (ChildInformation));

    (*childHndl)->dateOfBirth.octs = "19570310";
    (*childHndl)->dateOfBirth.octetLen  = strlen ((*childHndl)->dateOfBirth.octs);
    (*childHndl)->name = (Name*) Asn1Alloc (sizeof (Name));

    (*childHndl)->name->givenName.octs = "James";
    (*childHndl)->name->givenName.octetLen =
        strlen ((*childHndl)->name->givenName.octs);
    (*childHndl)->name->initial.octs = "R";
    (*childHndl)->name->initial.octetLen =
        strlen ((*childHndl)->name->initial.octs);
    (*childHndl)->name->familyName.octs = "Smith";
    (*childHndl)->name->familyName.octetLen =
        strlen ((*childHndl)->name->familyName.octs);

    childHndl = AsnListAppend (pr.children);
    *childHndl = Asn1Alloc (sizeof (ChildInformation));

    (*childHndl)->dateOfBirth.octs = "19610621";
    (*childHndl)->dateOfBirth.octetLen  = strlen ((*childHndl)->dateOfBirth.octs);

    (*childHndl)->name = (Name*) Asn1Alloc (sizeof (Name));

    (*childHndl)->name->givenName.octs = "Lisa";
    (*childHndl)->name->givenName.octetLen =
        strlen ((*childHndl)->name->givenName.octs);
    (*childHndl)->name->initial.octs = "M";
    (*childHndl)->name->initial.octetLen =
        strlen ((*childHndl)->name->initial.octs);
    (*childHndl)->name->familyName.octs = "Smith";
    (*childHndl)->name->familyName.octetLen =
        strlen ((*childHndl)->name->familyName.octs);

    SBufInit (&outputBuf,data, dataSize);
    SBufResetInWriteRvsMode (&outputBuf);

    encodedLen = BEncPersonnelRecord (&outputBuf, &pr);

    /*
     * after encoding a value ALWAYS check for write error
     * in the buffer.  The encode routine do not use longjmp
     * when they enter an error state
     */
    if ((encodedLen <= 0) || (SBufWriteError (&outputBuf)))
    {
        fprintf (stderr, "failed encoding PersonnelRecord value\n");
        exit (1);
    }

    outputFile = fopen ("pr.ber", "w");
    if (!outputFile)
    {
        perror ("fopen:");
        exit (1);
    }

    SBufResetInReadMode (&outputBuf);
    for ( ; encodedLen > 0; encodedLen--)
        fputc (SBufGetByte (&outputBuf), outputFile);


    printf ("Wrote the following BER PersonnelRecord value to pr.ber.\n");
    printf ("Test it with \"def\" and \"indef\"\n");

    PrintPersonnelRecord (stdout, &pr, 0);
    printf ("\n");

    return 0;
}
