/* $Copyright:
 *
 * Copyright 1998-2002 by the Massachusetts Institute of Technology.
 * 
 * All rights reserved.
 * 
 * Export of this software from the United States of America may require a
 * specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and distribute
 * this software and its documentation for any purpose and without fee is
 * hereby granted, provided that the above copyright notice appear in all
 * copies and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of M.I.T. not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  Furthermore if you
 * modify this software you must label your software as modified software
 * and not distribute it in such a fashion that it might be confused with
 * the original MIT software. M.I.T. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Individual source code files are copyright MIT, Cygnus Support,
 * OpenVision, Oracle, Sun Soft, FundsXpress, and others.
 * 
 * Project Athena, Athena, Athena MUSE, Discuss, Hesiod, Kerberos, Moira,
 * and Zephyr are trademarks of the Massachusetts Institute of Technology
 * (MIT).  No commercial use of these trademarks may be made without prior
 * written permission of MIT.
 * 
 * "Commercial use" means use of a name in a product or other for-profit
 * manner.  It does NOT prevent a commercial firm from referring to the MIT
 * trademarks in order to convey information (although in doing so,
 * recognition of their trademark status should be given).
 * $
 */

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/CTicketListSubPrincipaItem.cp,v 1.9 2003/06/10 21:12:05 smcguire Exp $ */

#include "CTicketListSubPrincipalItem.h"

PP_Using_Namespace_PowerPlant

const int kSubPrincipalHorizOffset = 28;

// ---------------------------------------------------------------------------
//		¥ Constructor
// ---------------------------------------------------------------------------
CTicketListSubPrincipalItem::CTicketListSubPrincipalItem()
	: CTicketListItem (kSubPrincipalItem, rTicketItemBold, rTicketItemBoldItalic, kSubPrincipalHorizOffset,
						cc_credentials_no_version, "\pNo User", "\pNo User", 0, 0)
{
	mParentPrincItem = nil;
}

// ---------------------------------------------------------------------------
//		¥ Constructor
// ---------------------------------------------------------------------------
CTicketListSubPrincipalItem::CTicketListSubPrincipalItem(
	LStr255 inPrincipalString,
	LStr255 inDisplayString,
	unsigned long inStartTime,
	unsigned long inExpirationTime,
	cc_int32 inKerberosVersion,
	CTicketListPrincipalItem *inParentPrincItem,
	short inItemValidity)
	: CTicketListItem (kSubPrincipalItem, rTicketItemBold, rTicketItemBoldItalic, kSubPrincipalHorizOffset,
						inKerberosVersion, inPrincipalString, inDisplayString, inStartTime, inExpirationTime, inItemValidity)
{
	mParentPrincItem = inParentPrincItem;
}

// ---------------------------------------------------------------------------
//		¥ Destructor
// ---------------------------------------------------------------------------
CTicketListSubPrincipalItem::~CTicketListSubPrincipalItem()
{
}

// ---------------------------------------------------------------------------
//		EquivalentItem
// ---------------------------------------------------------------------------
// doesn't test for full equality, just if their principal strings parent item's UCCache's match
Boolean	CTicketListSubPrincipalItem::EquivalentItem(CTicketListItem *comparisonItem)
{
	Assert_(comparisonItem != nil);
	
	if (comparisonItem == nil)
		return false;
		
	// check first to make sure these items are the same type, otherwise it doesn't make sense
	// to compare them (and could cause a crash if we did)
	if (this->mTicketItemType != comparisonItem->GetItemType())
		return false;
	
	// next check to see if versions are the same (the principal check below will match for
	// different versions if there's no instance, so we need this extra check)
	if (this->mKerberosVersion != comparisonItem->GetKerberosVersion())
		return false;
		
	Boolean princStringsAreEqual, parentItemsAreEquivalent;
	
	princStringsAreEqual = (this->mTicketItemPrincipalString == (dynamic_cast<CTicketListSubPrincipalItem *>(comparisonItem))->mTicketItemPrincipalString);
	parentItemsAreEquivalent = mParentPrincItem->EquivalentItem((dynamic_cast<CTicketListSubPrincipalItem *>(comparisonItem))->mParentPrincItem);

	return (princStringsAreEqual && parentItemsAreEquivalent);
}
