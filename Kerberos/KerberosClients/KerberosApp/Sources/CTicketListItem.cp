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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/CTicketListItem.cp,v 1.8 2003/06/10 21:12:01 smcguire Exp $ */

#include "CTicketListItem.h"

PP_Using_Namespace_PowerPlant

// ---------------------------------------------------------------------------
//		¥ Constructor with parameters (mostly for subclasses)
// ---------------------------------------------------------------------------
CTicketListItem::CTicketListItem(
	short inItemType,
	ResIDT inTextTrait,
	ResIDT inExpiredTextTrait,
	short inDrawOffset,
	cc_int32 inKerberosVersion,
	LStr255 inPrincipalString,
	LStr255 inDisplayString,
	unsigned long inStartTime,
	unsigned long inExpirationTime,
	short inItemValidity)
{
	mTicketItemType = inItemType;
	mTicketItemTextTrait = inTextTrait;
	mTicketItemExpiredTextTrait = inExpiredTextTrait;
	mTicketItemDrawOffset = inDrawOffset;

	mKerberosVersion = inKerberosVersion;
	mTicketItemPrincipalString.Assign(inPrincipalString);
	mTicketItemDisplayString.Assign(inDisplayString);
	mTicketItemStartTime = inStartTime;
	mTicketItemExpirationTime = inExpirationTime;
	mTicketItemValidity = inItemValidity;
	
	mItemIsExpanded = false;
	mItemIsSelected = false;
}

// ---------------------------------------------------------------------------
//		¥ Destructor
// ---------------------------------------------------------------------------
CTicketListItem::~CTicketListItem()
{
}

// ---------------------------------------------------------------------------
//		GetItemPrincipalString - puts a Pascal String in parameter
// ---------------------------------------------------------------------------
void CTicketListItem::GetItemPrincipalString (Str255 *outPrincipalString)
{
	LString::CopyPStr(mTicketItemPrincipalString, *outPrincipalString, sizeof(Str255));
}

// ---------------------------------------------------------------------------
//		SetItemPrincipalString - takes Pascal String as input
// ---------------------------------------------------------------------------
void CTicketListItem::SetItemPrincipalString (LStr255 inPrincipalString)
{
	mTicketItemPrincipalString.Assign(inPrincipalString);
}
		
// ---------------------------------------------------------------------------
//		GetItemDisplayString - puts a Pascal String in parameter
// ---------------------------------------------------------------------------
void CTicketListItem::GetItemDisplayString (Str255 *outDisplayString)
{
	LString::CopyPStr(mTicketItemDisplayString, *outDisplayString, sizeof(Str255));
}

// ---------------------------------------------------------------------------
//		SetItemDisplayString - takes Pascal String as input
// ---------------------------------------------------------------------------
void CTicketListItem::SetItemDisplayString (LStr255 inDisplayString)
{
	mTicketItemDisplayString.Assign(inDisplayString);
}
		
// ---------------------------------------------------------------------------
//		GetItemStartTime
// ---------------------------------------------------------------------------
unsigned long CTicketListItem::GetItemStartTime()
{
	return mTicketItemStartTime;
}

// ---------------------------------------------------------------------------
//		SetItemStartTime
// ---------------------------------------------------------------------------
void CTicketListItem::SetItemStartTime(unsigned long inStartTime)
{
	mTicketItemStartTime = inStartTime;
}

// ---------------------------------------------------------------------------
//		GetItemExpirationTime
// ---------------------------------------------------------------------------
unsigned long CTicketListItem::GetItemExpirationTime()
{
	return mTicketItemExpirationTime;
}

// ---------------------------------------------------------------------------
//		SetItemExpirationTime
// ---------------------------------------------------------------------------
void CTicketListItem::SetItemExpirationTime(unsigned long inExpirationTime)
{
	mTicketItemExpirationTime = inExpirationTime;
}

// ---------------------------------------------------------------------------
//		GetItemValidity
// ---------------------------------------------------------------------------
short CTicketListItem::GetItemValidity()
{
	return mTicketItemValidity;
}

// ---------------------------------------------------------------------------
//		SetItemValidity
// ---------------------------------------------------------------------------
void CTicketListItem::SetItemValidity(short inItemValidity)
{
	mTicketItemValidity = inItemValidity;
}

// ---------------------------------------------------------------------------
//		¥ GetKerberosVersion - return Kerberos versions of this principal's credentials
// ---------------------------------------------------------------------------
cc_int32 CTicketListItem::GetKerberosVersion()
{
	return mKerberosVersion;
}

// ---------------------------------------------------------------------------
//		¥ SetKerberosVersion - set Kerberos versions of this principal's credentials
// ---------------------------------------------------------------------------
void CTicketListItem::SetKerberosVersion(cc_int32 inVersion)
{
	mKerberosVersion = inVersion;
}

// ---------------------------------------------------------------------------
//		¥ GetItemIsExpanded
// ---------------------------------------------------------------------------
Boolean CTicketListItem::GetItemIsExpanded()
{
	return mItemIsExpanded;
}

// ---------------------------------------------------------------------------
//		¥ SetItemIsExpanded
// ---------------------------------------------------------------------------
void	CTicketListItem::SetItemIsExpanded(const Boolean &inIsExpanded)
{
	mItemIsExpanded = inIsExpanded;
}

// ---------------------------------------------------------------------------
//		¥ GetItemIsSelected
// ---------------------------------------------------------------------------
Boolean	CTicketListItem::GetItemIsSelected()
{
	return mItemIsSelected;
}

// ---------------------------------------------------------------------------
//		¥ SetItemIsSelected
// ---------------------------------------------------------------------------
void	CTicketListItem::SetItemIsSelected(const Boolean &inIsSelected)
{
	mItemIsSelected = inIsSelected;
}

// ---------------------------------------------------------------------------
//		IsTicketForwardable
// ---------------------------------------------------------------------------
Boolean CTicketListItem::IsTicketForwardable()
{
	return false;
}

// ---------------------------------------------------------------------------
//		IsTicketProxiable
// ---------------------------------------------------------------------------
Boolean CTicketListItem::IsTicketProxiable()
{
	return false;
}

// ---------------------------------------------------------------------------
//		IsTicketRenewable
// ---------------------------------------------------------------------------
Boolean CTicketListItem::IsTicketRenewable()
{
	return true;
}

// ---------------------------------------------------------------------------
//		EquivalentItem
// ---------------------------------------------------------------------------
		// doesn't test for full equality, just if they're close enough to be considered the same
		// some flags may be different
/*
Boolean CTicketListItem::EquivalentItem(CTicketListItem *comparisonItem)
{
	if (comparisonItem == nil)
		return false;
	
	if (this->mTicketItemType == comparisonItem->GetItemType()) {
		if (this->mKerberosVersion == comparisonItem->GetKerberosVersion()) {
			if (this->mTicketItemExpirationTime == comparisonItem->GetItemExpirationTime()) {
				Str255 comparisonPrinc;
				comparisonItem->GetItemPrincipalString(&comparisonPrinc);
		
				if (this->mTicketItemPrincipalString == comparisonPrinc)
					return true;
			}
		}
	}
	return false;
}
*/