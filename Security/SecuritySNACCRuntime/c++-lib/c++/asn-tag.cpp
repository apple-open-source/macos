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


// file: .../c++-lib/src/asn-tag.C -  ASN.1 tag manipulation routines
//
// MS 92/06/18
// Copyright (C) 1992 Michael Sample and the University of British Columbia
//
// This library is free software; you can redistribute it and/or
// modify it provided that this copyright/license information is retained
// in original form.
//
// If you modify this file, you must clearly indicate your changes.
//
// This source code is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//

#include "asn-config.h"
#include "asn-len.h"
#include "asn-tag.h"

/*
 * Decode a BER Tag from the given buffer.  Error is
 * flagged if the tag is too long or if a read error occurs.
 */
AsnTag
BDecTag (BUF_TYPE  b, AsnLen &bytesDecoded, ENV_TYPE env)
{
    AsnTag tagId;
    AsnTag tmpTagId;
    unsigned i;

    tagId = ((AsnTag) b.GetByte()) << ((sizeof (AsnTag)-1) *8);
    bytesDecoded++;

    /* check if long tag format (ie code > 31) */
    if ((tagId & (((AsnTag) 0x1f) << ((sizeof (AsnTag)-1)*8))) == (((AsnTag)0x1f) << ((sizeof (AsnTag)-1)*8)))
    {
        i = 2;
        do
        {
            tmpTagId = (AsnTag) b.GetByte();
            tagId |= (tmpTagId << ((sizeof (AsnTag)-i)*8));
            bytesDecoded++;
            i++;
        }
        while ((tmpTagId & (AsnTag)0x80) && (i <= sizeof (AsnTag)));

        /*
         * check for too long a tag
         */
        if (i > (sizeof (AsnTag)+1))
        {
            Asn1Error << "BDecTag: ERROR - tag value overflow" << endl;
			#if SNACC_EXCEPTION_ENABLE
			SnaccExcep::throwMe(-21);
			#else
            longjmp (env, -21);
			#endif
        }
    }

    if (b.ReadError())
    {
        Asn1Error << "BDecTag: ERROR - decoded past the end of data" << endl;
		#if SNACC_EXCEPTION_ENABLE
		SnaccExcep::throwMe(-22);
		#else
        longjmp (env, -22);
		#endif
    }

    return tagId;

} /* BDecTag */
