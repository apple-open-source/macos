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


// c++_examples/any/genber.C - builds an AnyTestType value and writes BER form
//            of the value to a file called "att.ber"
//
//  Shows how to build internal rep of lists and ANY values.
//
// MS 92
//
// $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c++-examples/any/genber.C,v 1.1.1.1 2001/05/18 23:14:05 mb Exp $
// $Log: genber.C,v $
// Revision 1.1.1.1  2001/05/18 23:14:05  mb
// Move from private repository to open source repository
//
// Revision 1.3  2001/05/05 00:59:17  rmurphy
// Adding darwin license headers
//
// Revision 1.2  2000/06/08 19:58:44  dmitch
// Mods for X port.
//
// Revision 1.1.1.1  1999/03/16 18:05:57  aram
// Originals from SMIME Free Library.
//
// Revision 1.5  1995/07/24 15:33:34  rj
// changed `_' to `-' in file names.
//
// any-test.[hC] becomes any.[hC] due to to snacc's new file name generation scheme.
//
// check return value of new.
//
// Revision 1.4  1995/02/18  13:54:03  rj
// added #define HAVE_VARIABLE_SIZED_AUTOMATIC_ARRAYS since not every C++ compiler provides them.
//
// Revision 1.3  1994/10/08  01:26:22  rj
// several \size_t'
//
// Revision 1.2  1994/08/31  08:56:30  rj
// first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
//


#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <fstream.h>

#include "asn-incl.h"
#include "any.h"

#define APPLE_ANY_HACK		1

main (int argc, char *argv[])
{
    ofstream outputFile;
    AsnBuf outputBuf;
    size_t encodedLen;
    size_t dataSize = 1024;
#if HAVE_VARIABLE_SIZED_AUTOMATIC_ARRAYS
    char data[dataSize];
#else
    char *data = new char[dataSize];
    if (!data)
	return 1;
#endif /* HAVE_VARIABLE_SIZED_AUTOMATIC_ARRAYS */
    AnyTestType att;
    TSeq1 ts1;
    TSeq2 ts2;
    AttrValue1 *atv1ptr;
    AttrValue2 *atv2ptr;
    AsnInt intVal;
    AsnBool boolVal;
    AsnOcts octsVal ("Hi Mom");
    OctsId octsIdVal = octsVal;
    AsnBits bitsVal;
    BitsId bitsIdVal (9);
    AsnReal realVal;

    // READ THIS!!!
    // you must be really careful when setting the
    // "value" field and "id" fields in an
    // ANY/ANY DEFINED BY type because "value" is a
    // "AsnType*" and will accept any
    // pointer value.  It will even encode
    // the wrong value without complaining if you
    // set "value" to the wrong object.

    atv1ptr = att.intMap.Append();
    atv1ptr->id = intId;
    intVal = -99;
	#if	APPLE_ANY_HACK
    atv1ptr->anyDefBy.value = reinterpret_cast<CSM_Buffer *>(&intVal);
	#else
    atv1ptr->anyDefBy.value = &intVal;
	#endif
    atv1ptr = att.intMap.Append();
    atv1ptr->id = boolId;
    boolVal = true;
	#if	APPLE_ANY_HACK
    atv1ptr->anyDefBy.value = reinterpret_cast<CSM_Buffer *>(&boolVal);
	#else
    atv1ptr->anyDefBy.value = &boolVal;
	#endif
	
    atv1ptr = att.intMap.Append();
    atv1ptr->id = octsId;
	#if	APPLE_ANY_HACK
    atv1ptr->anyDefBy.value = reinterpret_cast<CSM_Buffer *>(&octsIdVal);
	#else
    atv1ptr->anyDefBy.value = &octsIdVal;
	#endif
	
    atv1ptr = att.intMap.Append();
    atv1ptr->id = bitsId;
    bitsIdVal.SetBit (0);
    bitsIdVal.ClrBit (1);
    bitsIdVal.SetBit (2);
    bitsIdVal.ClrBit (3);
    bitsIdVal.SetBit (4);
    bitsIdVal.ClrBit (5);
    bitsIdVal.SetBit (6);
    bitsIdVal.ClrBit (7);
    bitsIdVal.SetBit (8);
    bitsIdVal.ClrBit (9);
	#if	APPLE_ANY_HACK
    atv1ptr->anyDefBy.value = reinterpret_cast<CSM_Buffer *>(&bitsIdVal);
	#else
    atv1ptr->anyDefBy.value = &bitsIdVal;
	#endif
	
    atv1ptr = att.intMap.Append();
    atv1ptr->id = realId;
    realVal = 108.3838;
	#if	APPLE_ANY_HACK
    atv1ptr->anyDefBy.value = reinterpret_cast<CSM_Buffer *>(&realVal);
	#else
    atv1ptr->anyDefBy.value = &realVal;
	#endif
	
    // now do TSeq2 with same vals but use OID as identifier
    atv2ptr = att.oidMap.Append();
    atv2ptr->id = intOid;
 	#if	APPLE_ANY_HACK
    atv2ptr->anyDefBy.value = reinterpret_cast<CSM_Buffer *>(&intVal);
	#else
	atv2ptr->anyDefBy.value = &intVal;
	#endif
	
    atv2ptr = att.oidMap.Append();
    atv2ptr->id = boolOid;
 	#if	APPLE_ANY_HACK
    atv2ptr->anyDefBy.value = reinterpret_cast<CSM_Buffer *>(&boolVal);
	#else
    atv2ptr->anyDefBy.value = &boolVal;
	#endif
	
    atv2ptr = att.oidMap.Append();
    atv2ptr->id = octsOid;
 	#if	APPLE_ANY_HACK
    atv2ptr->anyDefBy.value = reinterpret_cast<CSM_Buffer *>(&octsVal);
	#else
    atv2ptr->anyDefBy.value = &octsVal;
	#endif
	
    atv2ptr = att.oidMap.Append();
    atv2ptr->id = bitsOid;
    bitsVal = bitsIdVal; // copy bits
 	#if	APPLE_ANY_HACK
    atv2ptr->anyDefBy.value = reinterpret_cast<CSM_Buffer *>(&bitsVal);
	#else
    atv2ptr->anyDefBy.value = &bitsVal;
	#endif
	
    atv2ptr = att.oidMap.Append();
    atv2ptr->id = realOid;
 	#if	APPLE_ANY_HACK
    atv2ptr->anyDefBy.value = reinterpret_cast<CSM_Buffer *>(&bitsVal);
	#else
    atv2ptr->anyDefBy.value = &bitsVal;
	#endif
	
    outputBuf.Init (data, dataSize);
    outputBuf.ResetInWriteRvsMode();

    if (!att.BEncPdu (outputBuf, encodedLen))
        cout << "failed encoding AnyTestType value" << endl;

    outputFile.open ("att.ber");
    if (!outputFile)
    {
        perror ("ofstream::open");
        exit (1);
    }

    outputBuf.ResetInReadMode();
    for ( ; encodedLen > 0; encodedLen--)
        outputFile.put (outputBuf.GetByte());


    cout << "Wrote the following BER AnyTestType value to att.ber." << endl;
    cout << "Test it with \"def\" and \"indef\"" << endl;
    //cout << att << endl;

    return 0;
}
