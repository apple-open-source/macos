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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/CTicketListCredentialsItem.cp,v 1.11 2003/06/10 21:11:56 smcguire Exp $ */

#include "CTicketListCredentialsItem.h"
#include "CTicketInfoWindow.h"
#include "CKerberosManagerApp.h"

PP_Using_Namespace_PowerPlant

const int kCredentialsHorizOffset = 38;


// ---------------------------------------------------------------------------
//		¥ Constructor
// ---------------------------------------------------------------------------
CTicketListCredentialsItem::CTicketListCredentialsItem()
	: CTicketListItem (kCredentialsItem, rTicketItemRegular, rTicketItemItalic, kCredentialsHorizOffset,
						cc_credentials_no_version, "\pNo User", "\pNo User", 0, 0)
{
	mInfoWindowPtr = nil;
	mInfoWindowIsOpen = false;
}

// ---------------------------------------------------------------------------
//		¥ Constructor
// ---------------------------------------------------------------------------
CTicketListCredentialsItem::CTicketListCredentialsItem(
	LStr255 inPrincipalString,
	LStr255 inDisplayString,
	unsigned long inStartTime,
	unsigned long inExpirationTime,
	cc_int32 inKerberosVersion,
	UCredentials inCreds,
	short inItemValidity)
	: CTicketListItem (kCredentialsItem, rTicketItemRegular, rTicketItemItalic, kCredentialsHorizOffset,
						inKerberosVersion, inPrincipalString, inDisplayString, inStartTime, inExpirationTime, inItemValidity),
						mCredentials(inCreds)
{
	mInfoWindowPtr = nil;
	mInfoWindowIsOpen = false;
}

// ---------------------------------------------------------------------------
//		¥ Destructor
// ---------------------------------------------------------------------------
CTicketListCredentialsItem::~CTicketListCredentialsItem()
{
	// this function cleans up the associated info window, if necessary
	this->CloseTicketInfoWindow();
}

// ---------------------------------------------------------------------------
//		¥ SetCredentials - set the credentials associated with this item, in
//			UCredentials format
// ---------------------------------------------------------------------------
void CTicketListCredentialsItem::SetCredentials(UCredentials inCreds)
{
	mCredentials = inCreds; // this should make a copy of the UCredentials structure
}

// ---------------------------------------------------------------------------
//		¥ OpenTicketInfoWindow - open the ticket info window
// ---------------------------------------------------------------------------
void CTicketListCredentialsItem::OpenTicketInfoWindow()
{
	// find the app so we can make it the supercommander
	CKerberosManagerApp *theApp;
	theApp = static_cast<CKerberosManagerApp *>(LCommander::GetTopCommander());

	// Create CTicketInfoWindow to display info for the ticket
	if (!mInfoWindowIsOpen) {
		CTicketInfoWindow	*theWindow = dynamic_cast<CTicketInfoWindow*>(CTicketInfoWindow::CreateWindow(rTicketInfoWindow, theApp));
		ThrowIfNil_(theWindow);
		theWindow->InitializeWindow(mCredentials.Get(), this);
		mInfoWindowPtr = theWindow;
		mInfoWindowIsOpen = true;
	} else {
		Assert_(mInfoWindowPtr != nil);
		mInfoWindowPtr->InitializeWindow(mCredentials.Get(), this);
		mInfoWindowPtr->Select();
	}
}

// ---------------------------------------------------------------------------
//		¥ CloseTicketInfoWindow
//		Close the associated ticket info window if it's open, and mark the
//		mInfoWindowIsOpen as closed.
//		Caution - if something else closed the window, this function might
//		try to close a non-existant window.
// ---------------------------------------------------------------------------
void CTicketListCredentialsItem::CloseTicketInfoWindow()
{
	if (mInfoWindowIsOpen) {
		if (mInfoWindowPtr != nil) {
			mInfoWindowPtr->AttemptClose();
		}
	}
	mInfoWindowPtr = nil;
	mInfoWindowIsOpen = false;
}

// ---------------------------------------------------------------------------
//		¥ MarkTicketInfoWindowAsClosed
//		Set the mInfoWindowIsOpen flag to false and the window pointer to nil.
//		If the window hasn't actually been closed, problems might ensue.
// ---------------------------------------------------------------------------
void CTicketListCredentialsItem::MarkTicketInfoWindowAsClosed()
{
	mInfoWindowPtr = nil;
	mInfoWindowIsOpen = false;
}

// ---------------------------------------------------------------------------
//		¥ AssumeTicketInfoWindowOwnership
//		Move ownership of a ticket info window from a source item to this one,
//		and then mark the sourceItem as not having a ticket info window.  This
//		will be used to prevent tix info windows from disappearing when the tix
//		list refreshes.
// ---------------------------------------------------------------------------
void CTicketListCredentialsItem::AssumeTicketInfoWindowOwnership(CTicketListCredentialsItem *sourceItem)
{
	this->mInfoWindowIsOpen = sourceItem->mInfoWindowIsOpen;
	
	if (mInfoWindowIsOpen) {
		Assert_(sourceItem->mInfoWindowPtr != nil);
		
		this->mInfoWindowPtr = sourceItem->mInfoWindowPtr;
		this->mInfoWindowPtr->SetOwnerListItem(this);
		
		sourceItem->mInfoWindowPtr = nil;
		sourceItem->mInfoWindowIsOpen = false;
	} else {
		this->mInfoWindowPtr = nil;
	}
}

// ---------------------------------------------------------------------------
//		¥ IsForwardable
//		Override for CTicketListItem method, checks the credentials for value
// ---------------------------------------------------------------------------
Boolean CTicketListCredentialsItem::IsTicketForwardable()
{
	if (mCredentials.GetVersion() == UPrincipal::kerberosV4) {
		return false;
	} else {
		if ((mCredentials.GetV5Credentials()->ticket_flags) & TKT_FLG_FORWARDABLE)
			return true;
		else
			return false;
	}
}

// ---------------------------------------------------------------------------
//		¥ IsProxiable
//		Override for CTicketListItem method, checks the credentials for value
// ---------------------------------------------------------------------------
Boolean CTicketListCredentialsItem::IsTicketProxiable()
{
	if (mCredentials.GetVersion() == UPrincipal::kerberosV4) {
		return false;
	} else {
		if ((mCredentials.GetV5Credentials()->ticket_flags) & TKT_FLG_PROXIABLE)
			return true;
		else
			return false;
	}
}

// ---------------------------------------------------------------------------
//		¥ IsRenewable
//		Override for CTicketListItem method, checks the credentials for value
// ---------------------------------------------------------------------------
Boolean CTicketListCredentialsItem::IsTicketRenewable()
{
	if (mCredentials.GetVersion() == UPrincipal::kerberosV4) {
		return false;
	} else {
		if ((mCredentials.GetV5Credentials()->ticket_flags) & TKT_FLG_RENEWABLE)
			return true;
		else
			return false;
	}
}

// ---------------------------------------------------------------------------
//		EquivalentItem
// ---------------------------------------------------------------------------
// doesn't test for full equality, just if the contained UCredentaials match, which means the creds are the same
Boolean CTicketListCredentialsItem::EquivalentItem(CTicketListItem *comparisonItem)
{
	Assert_(comparisonItem != nil);
	
	if (comparisonItem == nil)
		return false;
		
	// check first to make sure these items are the same type, otherwise it doesn't make sense
	// to compare them (and could cause a crash if we did)
		if (this->mTicketItemType != comparisonItem->GetItemType())
		return false;
		
	return (this->mCredentials == (dynamic_cast<CTicketListCredentialsItem *>(comparisonItem))->mCredentials);
}