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
 * asn_enum.c - routines for the ASN.1 ENUMERATED type
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
 * $Header: /cvs/Darwin/Security/SecuritySNACCRuntime/c-lib/src/asn-enum.c,v 1.1.1.1 2001/05/18 23:14:08 mb Exp $
 * $Log: asn-enum.c,v $
 * Revision 1.1.1.1  2001/05/18 23:14:08  mb
 * Move from private repository to open source repository
 *
 * Revision 1.2  2001/05/05 00:59:25  rmurphy
 * Adding darwin license headers
 *
 * Revision 1.1.1.1  1999/03/16 18:06:30  aram
 * Originals from SMIME Free Library.
 *
 * Revision 1.3  1995/07/24 21:04:50  rj
 * changed `_' to `-' in file names.
 *
 * Revision 1.2  1994/09/01  01:04:38  rj
 * reduce the risk of unwanted surprises with macro expansion by properly separating the C tokens.
 *
 * Revision 1.1  1994/08/28  09:45:52  rj
 * first check-in. for a list of changes to the snacc-1.1 distribution please refer to the ChangeLog.
 *
 */

#include "asn-config.h"
#include "asn-len.h"
#include "asn-tag.h"
#include "asn-int.h"
#include "asn-enum.h"

/*
 * encodes universal TAG LENGTH and Contents of and ASN.1 ENUMERATED
 */
AsnLen
BEncAsnEnum PARAMS ((b, data),
    BUF_TYPE     b _AND_
    AsnEnum *data)
{
    AsnLen len;

    len =  BEncAsnEnumContent (b, data);
    len += BEncDefLen (b, len);
    len += BEncTag1 (b, UNIV, PRIM, ENUM_TAG_CODE);
    return len;
}  /* BEncAsnEnum */

/*
 * decodes universal TAG LENGTH and Contents of and ASN.1 ENUMERATED
 */
void
BDecAsnEnum PARAMS ((b, result, bytesDecoded, env),
    BUF_TYPE   b _AND_
    AsnEnum    *result _AND_
    AsnLen *bytesDecoded _AND_
    jmp_buf env)
{
    AsnTag tag;
    AsnLen elmtLen;

    if ((tag = BDecTag (b, bytesDecoded, env)) != MAKE_TAG_ID (UNIV, PRIM, ENUM_TAG_CODE))
    {
         Asn1Error ("BDecAsnInt: ERROR wrong tag on ENUMERATED.\n");
         longjmp (env, -40);
    }

    elmtLen = BDecLen (b, bytesDecoded, env);
    BDecAsnEnumContent (b, tag, elmtLen, result, bytesDecoded, env);

}  /* BDecAsnEnum */
