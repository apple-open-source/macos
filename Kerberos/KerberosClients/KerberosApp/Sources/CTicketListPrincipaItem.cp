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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/CTicketListPrincipaItem.cp,v 1.10 2003/06/12 15:45:45 smcguire Exp $ */

#include "CTicketListPrincipalItem.h"

PP_Using_Namespace_PowerPlant

const int kPrincipalHorizOffset = 18;

// ---------------------------------------------------------------------------
//		¥ Constructor
// ---------------------------------------------------------------------------
CTicketListPrincipalItem::CTicketListPrincipalItem()
	: CTicketListItem (kPrincipalItem, rTicketItemBold, rTicketItemBoldItalic, kPrincipalHorizOffset,
						cc_credentials_no_version, "\pNo User", "\pNo User", 0, 0)
{
	mIsActive = false;
}

// ---------------------------------------------------------------------------
//		¥ Constructor
// ---------------------------------------------------------------------------
CTicketListPrincipalItem::CTicketListPrincipalItem(
	LStr255 inPrincipalString,
	LStr255 inDisplayString,
	unsigned long inStartTime,
	unsigned long inExpirationTime,
	cc_int32 inKerberosVersion,
	UCCache inCCache,
	short inTicketValidity)
	: CTicketListItem (kPrincipalItem, rTicketItemBold, rTicketItemBoldItalic, kPrincipalHorizOffset,
						inKerberosVersion, inPrincipalString, inDisplayString, inStartTime, inExpirationTime, inTicketValidity),
						mCCache(inCCache)
{
	mIsActive = false;
	mPrincipalIsRenewable = false;
	mRenewAttemptHalflife = (inExpirationTime - inStartTime) / 2;
}

// ---------------------------------------------------------------------------
//		¥ Destructor
// ---------------------------------------------------------------------------
CTicketListPrincipalItem::~CTicketListPrincipalItem()
{
}

// ---------------------------------------------------------------------------
//		SetPrincipalAsActive - doesn't actually make the principal active, just
//			sets the item's properties to display proper
// ---------------------------------------------------------------------------
void CTicketListPrincipalItem::SetPrincipalAsActive()
{
	mIsActive = true;
	mTicketItemTextTrait = rTicketItemBoldUnderline;
	mTicketItemExpiredTextTrait = rTicketItemBoldItalicUnderline;
}

// ---------------------------------------------------------------------------
//		SetPrincipalAsInactive - doesn't actually make the principal inactive, 
//			just sets the item's properties to display proper
// ---------------------------------------------------------------------------
void CTicketListPrincipalItem::SetPrincipalAsInactive()
{
	mIsActive = false;
	mTicketItemTextTrait = rTicketItemBold;
	mTicketItemExpiredTextTrait = rTicketItemBoldItalic;
}

// ---------------------------------------------------------------------------
//		PrincipalIsActive - returns whether the Principal is active or not
// ---------------------------------------------------------------------------
Boolean CTicketListPrincipalItem::PrincipalIsActive()
{
	return mIsActive;
}

// ---------------------------------------------------------------------------
//		SetPrincipalIsRenewable
// ---------------------------------------------------------------------------
void CTicketListPrincipalItem::SetPrincipalIsRenewable(Boolean inIsRenewable)
{
	mPrincipalIsRenewable = inIsRenewable;
}

// ---------------------------------------------------------------------------
//		IsTicketRenewable
// ---------------------------------------------------------------------------
Boolean CTicketListPrincipalItem::IsTicketRenewable()
{
	return mPrincipalIsRenewable;
}

// ---------------------------------------------------------------------------
//		SetRenewAttemptHalflife
// ---------------------------------------------------------------------------
void CTicketListPrincipalItem::SetRenewAttemptHalflife(unsigned long inHalflife)
{
	mRenewAttemptHalflife = inHalflife;
}

// ---------------------------------------------------------------------------
//		GetRenewAttemptHalflife
// ---------------------------------------------------------------------------
unsigned long CTicketListPrincipalItem::GetRenewAttemptHalflife()
{
	return mRenewAttemptHalflife;
}


// ---------------------------------------------------------------------------
//		EquivalentItem
// ---------------------------------------------------------------------------
// doesn't test for full equality, just if their parent UCCache's match
Boolean	CTicketListPrincipalItem::EquivalentItem(CTicketListItem *comparisonItem)
{
	Assert_(comparisonItem != nil);
	
	if (comparisonItem == nil)
		return false;
		
	// check first to make sure these items are the same type, otherwise it doesn't make sense
	// to compare them (and could cause a crash if we did)
		if (this->mTicketItemType != comparisonItem->GetItemType())
		return false;
		
	return (this->mCCache == (dynamic_cast<CTicketListPrincipalItem *>(comparisonItem))->mCCache);
}
