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

// ===========================================================================
//	CTicketInfoWindow.cp
// ===========================================================================

#include <utime.h>
#include <string.h>

#include <UDrawingState.h>
#include <UMemoryMgr.h>
#include <URegistrar.h>
#include <UControlRegistry.h>
#include <UEnvironment.h>

#include <LWindow.h>
#include <LCaption.h>

#include <Appearance.h>

#if TARGET_RT_MAC_CFM
	#include <KerberosSupport/Utilities.h>
	#include <CredentialsCache/CredentialsCache.h>
	#include <Kerberos5/Kerberos5.h>
#else
	#include <sys/time.h>
	#include <CoreFoundation/CFDate.h>
	#include <Kerberos/Kerberos.h>
#endif

#include <LStaticText.h>

#include <LMultiPanelView.h>

#include <LTableView.h>
#include <LTextColumn.h>

#include <LScrollerView.h>

#include <PP_Debug.h>
#include <PP_DebugMacros.h>

#include "CTicketInfoWindow.h"
#include "CKerberosManagerApp.h"
#include "CTicketListCredentialsItem.h"

PP_Using_Namespace_PowerPlant

// Constant declarations

// Constants:

const ResIDT	Principal_ID			= 101;
const ResIDT	ServPrincipal_ID		= 102;
const ResIDT	IssueTime_ID			= 103;
const ResIDT	StartTime_ID			= 104;
const ResIDT	EndTime_ID				= 105;
const ResIDT	RenewTimeCaption		= 208;
const ResIDT	RenewTimeText			= 207;
const ResIDT	ExpiredWarning_ID		= 106;
const ResIDT	fForwardable_ID			= 107;
const ResIDT	fProxiable_ID			= 108;
const ResIDT	fForwarded_ID			= 109;
const ResIDT	fProxy_ID				= 110;
const ResIDT	fMayPostdate_ID			= 111;
const ResIDT	fPostdated_ID			= 112;
const ResIDT	fInvalid_ID				= 113;
const ResIDT	fRenewable_ID			= 114;
const ResIDT	fInitial_ID				= 115;
const ResIDT	fPreAuth_ID				= 116;
const ResIDT	fHWAuth_ID				= 117;
const ResIDT	fSKey_ID				= 118;

const ResIDT	EncryptionLabel1_ID		= 120;
const ResIDT	EncryptionType1_ID		= 121;
const ResIDT	EncryptionLabel2_ID		= 140;
const ResIDT	EncryptionType2_ID		= 141;
const ResIDT	Version_ID				= 122;
const ResIDT	SuperMultiPanel_ID		= 131;

const ResIDT	v4View_ID				= 8001;
const ResIDT	v5View_ID				= 8002;
const ResIDT	v4MultiPanel_ID			= 8102;
const ResIDT	v5MultiPanel_ID			= 8202;

// IP Address Constants:
const ResIDT	IPAddress_ID			= 119;
const ResIDT	IPAddressScroller_ID	= 219;
const ResIDT	IPAddressTable_ID		= 220;
const ResIDT	cIPAddress_ID			= 19;


// ---------------------------------------------------------------------------
//	¥ CTicketInfoWindow									[public]
// ---------------------------------------------------------------------------
//	CTicketInfoWindow Default Constructor

CTicketInfoWindow::CTicketInfoWindow()
{
	mTicketListItemRef = nil;
}

// ---------------------------------------------------------------------------
//	¥ CTicketInfoWindow									[public]
// ---------------------------------------------------------------------------
//	CTicketInfoWindow Stream Constructor

CTicketInfoWindow::CTicketInfoWindow(LStream* inStream)
	: LWindow(inStream)
{
	mTicketListItemRef = nil;
}

// ---------------------------------------------------------------------------
//	¥ ~CTicketInfoWindow									[public]
// ---------------------------------------------------------------------------
//	CTicketInfoWindow Destructor

CTicketInfoWindow::~CTicketInfoWindow()
{
	// tell the list item's flags that this window closed
	if (mTicketListItemRef != nil)
		((CTicketListCredentialsItem *)mTicketListItemRef)->MarkTicketInfoWindowAsClosed();
		
}

// ---------------------------------------------------------------------------
//	¥ SetOwnerListItem								[public, virtual]
// ---------------------------------------------------------------------------
//	Set the mTicketListItemRef (the list item that "owns" this window) -
//  overwrites existing
void CTicketInfoWindow::SetOwnerListItem(CTicketListItem *inListRef)
{
	mTicketListItemRef = inListRef;
}

// ---------------------------------------------------------------------------
//	¥ ClickInGoAway
// ---------------------------------------------------------------------------
//	Handle a click inside the close box of a Window

void CTicketInfoWindow::ClickInGoAway(const EventRecord	&inMacEvent)
{
	if (::TrackGoAway(mMacWindowP, inMacEvent.where)) {
	
		// check to see if the option key is down; if so, tell other ticket info windows
		// to close as well by searching the window list for them
		if (inMacEvent.modifiers & optionKey) {
			WindowPtr	macWindowP = ::GetWindowList();
			LWindow*	theWindow;
			
			/*
			while ((theWindow = LWindow::FetchWindowObject(macWindowP)) != nil) {
				PaneIDT paneID = theWindow->GetPaneID();
				
				if ( (paneID == (PaneIDT)rTicketInfoWindow) && (theWindow != this)) {
					theWindow->AttemptClose();
				}
				
				macWindowP = GetNextWindow(macWindowP);
			}
			*/
			
			TArrayIterator  <WindowPtr>	windowIterator (sWindowList);
			
			while ( windowIterator.Next(macWindowP) ) {
				theWindow = LWindow::FetchWindowObject(macWindowP);
				
				if (theWindow != nil) {
					PaneIDT paneID = theWindow->GetPaneID();
					
					if ( (paneID == (PaneIDT)rTicketInfoWindow) && (theWindow != this)) {
						theWindow->AttemptClose();
					}
				}
			}
		}
		
		ProcessCommand(cmd_Close);
	}
}


// ---------------------------------------------------------------------------
//	¥ InitializeWindow								[public, virtual]
// ---------------------------------------------------------------------------
//	Set All Kerberos info into CTicketInfoWindow

void CTicketInfoWindow::InitializeWindow(cc_credentials_t theCreds, CTicketListItem *inListRef)
{

	/* Initialize object pointers for the two inner multipanels (v4 and v5) */
						/* and the supermultipanel */
	LMultiPanelView* theSuperMultiPanelView;
	LMultiPanelView* theMultiPanelView;
	LView *theView;
	
	// remember the list item that opened this window so we can notify it when it's closed
	mTicketListItemRef = inListRef;

	/* Super Multi Panel */
	theSuperMultiPanelView = FindPaneByID_(this, SuperMultiPanel_ID, LMultiPanelView);
	
	// NOTE:  To conserve Memory we don't call theSuperMultiPanelView->CreateAllPanels()
    // because there is a chance that there will be only v4 or only v5 ticekts.

	// use the small system font to draw times and such so they draw correctly with non-English character sets
	ControlFontStyleRec smallSystemFont, smallSystemFontRed;
	smallSystemFont.flags = kControlUseFontMask;
	smallSystemFont.font = kControlFontSmallSystemFont;
	
	smallSystemFontRed = smallSystemFont;
	smallSystemFontRed.flags |= kControlUseForeColorMask;
	smallSystemFontRed.foreColor.red = 65535;
	smallSystemFontRed.foreColor.blue = 0;
	smallSystemFontRed.foreColor.green = 0;
	
	/* access v4 and v5 credentials within theCreds structure */
	cc_credentials_v4_t*	creds4 = theCreds -> data -> credentials.credentials_v4;
	cc_credentials_v5_t*	creds5 = theCreds -> data -> credentials.credentials_v5;

	switch (theCreds -> data -> version) {
		case cc_credentials_v4: {           // If it's a v4 ticket, we do this...
		
			theView = theSuperMultiPanelView->CreatePanel(1);	// Create the v4 Panel to display info
			Assert_(theView != nil);
			
			/* Within the v4 Panel we create all panels */
			theMultiPanelView = FindPaneByID_(this, v4MultiPanel_ID, LMultiPanelView);
			theMultiPanelView->CreateAllPanels();
			
			/* Obtain pointer to the v4 View (the one with three tabs) */
			LView* theView = FindPaneByID_ (this, v4View_ID, LView);
			
			/* Switch to v4 pane (having only three tabs) */		
			theSuperMultiPanelView->SwitchToPanel(1,false);
					
			/* Version Number */
			LStaticText* theText;
			theText = FindPaneByID_(this, Version_ID, LStaticText);
			LStr255 theString;			// NOTE: used 'this' instead of theView because version number
			theString.Assign("Kerberos v4");		// is inside the window but not inside the v4 pane.
			theText->SetText(theString);
			theText->SetFontStyle(smallSystemFont);
			
			/* Issue Time */
			theText = FindPaneByID_(theView, IssueTime_ID, LStaticText);
			theText->SetText( DumpDate(creds4->issue_date) );
			theText->SetFontStyle(smallSystemFont);
			
			/* Start Time (same as Issue Time in v4 tickets) */
			theText = FindPaneByID_(theView, StartTime_ID, LStaticText);
			theText->SetText( DumpDate(creds4->issue_date) );
			theText->SetFontStyle(smallSystemFont);
			
			/* End Time */
			theText = FindPaneByID_(theView, EndTime_ID, LStaticText);
			theText->SetText( DumpDate(creds4->issue_date+creds4->lifetime) );
			theText->SetFontStyle(smallSystemFont);
			
			/* Ticket Status: Valid, Expired, Not Valid */
			struct timeval currentTime;
			gettimeofday(&currentTime, nil);
			
			UInt32 expireSeconds = creds4->issue_date + creds4->lifetime;
			
			theText = FindPaneByID_(theView, ExpiredWarning_ID, LStaticText);
			
			if ((unsigned long)currentTime.tv_sec > expireSeconds) {
				theString.Assign("Expired");
				theText->SetFontStyle(smallSystemFontRed);
				theText->SetText(theString);
			} else {
				theString.Assign("Valid");
				theText->SetTextTraitsID(rGeneva9Plain);
				theText->SetFontStyle(smallSystemFont);
				theText->SetText(theString);
			}
			
			// expired warning for "invalid" tickets
			KLPrincipal currentPrincipal;
			KLBoolean	tixValid = true;
			KLStatus	klErr = klNoErr;
			
			klErr = KLCreatePrincipalFromTriplet(creds4->principal, creds4->principal_instance, creds4->realm, &currentPrincipal);
			if (klErr == klNoErr) {
				klErr = KLCacheHasValidTickets(currentPrincipal, kerberosVersion_V4, &tixValid, nil, nil);
				KLDisposePrincipal(currentPrincipal);
			}

			if (!tixValid) {
				// set color inside if blocks to avoid coloring not-really-expired service tix's status
				if (klErr == klCredentialsBadAddressErr) {
					theString.Assign("Not Valid (IP Address Changed)");
				} else if (klErr == klCredentialsNeedValidationErr) {
					theString.Assign("Not Valid Yet (Needs Validation)");
				} else if (klErr != klCredentialsExpiredErr) {
					theString.Assign("Not Valid (Reason Unknown)");
				}			
				theText->SetFontStyle(smallSystemFontRed);
				theText->SetText(theString);
			}
			
			
			/* Principal */
			theText = FindPaneByID_(this, Principal_ID, LStaticText);
			theString.Assign(creds4->principal);
			if ((*(creds4->principal_instance)!=NULL) && (creds4->principal_instance!=NULL)) {
				theString.Append(".");
				theString.Append(creds4->principal_instance);
			}
			theString.Append("@");
			theString.Append(creds4->realm);
			theText->SetText(theString);
			theText->SetFontStyle(smallSystemFont);
								
			/* Service Principal */
			theText = FindPaneByID_(this, ServPrincipal_ID, LStaticText);
			theString.Assign(creds4->service);
			if ((*(creds4->service_instance)!=NULL) && (creds4->service_instance!=NULL)) {
				theString.Append(".");
				theString.Append(creds4->service_instance);
			}
			theString.Append("@");
			theString.Append(creds4->realm);
			theText->SetText(theString);
			theText->SetFontStyle(smallSystemFont);

			/* Window Title */
			theString.Assign(creds4->principal);
			theString.Append("'s ");
			theString.Append("v4 ");
			theString.Append(creds4->service);
			theString.Append(" Ticket Info");
			this->SetDescriptor(theString);
			
			/* Should we call it "Encryption" or "String to Key Type"? */
			theText = FindPaneByID_(theView, EncryptionLabel1_ID, LStaticText);
			theString.Assign("String to Key Type:");
			theText->SetText(theString);

			/* IP Address (v4) */
			theText = FindPaneByID_(theView, cIPAddress_ID, LStaticText);
			theString.Assign("IP Address:");
			theText->SetText(theString);
			
			cc_uint32	address = creds4 -> address;
			
			LScrollerView* theScrollerView;
			theScrollerView = FindPaneByID_(theView, IPAddressScroller_ID, LScrollerView);
			theScrollerView->Hide();
			
			
			theText = FindPaneByID_(theView, IPAddress_ID, LStaticText);
			theText->SetFontStyle(smallSystemFont);
			if (address == NULL) {
				theString = "None";
				theText->SetText(theString);				
			} else {
				theString.Assign((long)((address & 0xFF000000) >> 24));
				theString.Append(".");
				theString.Append((long)((address & 0x00FF0000) >> 16));
				theString.Append(".");
				theString.Append((long)((address & 0x0000FF00) >> 8 ));
				theString.Append(".");
				theString.Append((long)(address & 0x000000FF));
				theText->SetText(theString);
			}
			
			/* String to Key Type */
			theText = FindPaneByID_(theView, EncryptionType1_ID, LStaticText);
			switch (creds4 -> string_to_key_type) {
				case cc_v4_stk_afs:
					theString.Assign("AFS");
					theText->SetText(theString);
					break;

				case cc_v4_stk_des:
					theString.Assign("DES");
					theText->SetText(theString);
					break;

				case cc_v4_stk_columbia_special:
					theString.Assign("Columbia Special");
					theText->SetText(theString);
					break;

				case cc_v4_stk_unknown:
					// we autodetect, and always fill string_to_key in as unknown
					// but we'll label it "Automatic" to make users feel better
					theString.Assign("Automatic");
					theText->SetText(theString);
					break;
				
				default:
					theString.Assign("Invalid");
					theText->SetText(theString);
				}
			theText->SetFontStyle(smallSystemFont);

			// set the second set of enctype labels to nil, since they're only used by v5
			theString.Assign("");
			theText = FindPaneByID_(theView, EncryptionLabel2_ID, LStaticText);
			theText->SetText(theString);
			
			theString.Assign("");
			theText = FindPaneByID_(theView, EncryptionType2_ID, LStaticText);
			theText->SetText(theString);

			/* Finally show the window */
			this->Show();
			break;
		}
		
		
		case cc_credentials_v5: {           // If it's a v5 ticket, we do this...
		
			/* Create the v5 Pane */
			theSuperMultiPanelView->CreatePanel(2);
		
			/* Now within the v5 panel create all panels*/
			theMultiPanelView = FindPaneByID_(this, v5MultiPanel_ID, LMultiPanelView);
			theMultiPanelView->CreateAllPanels();	
				
			/* Obtain pointer to the v5 Pane (the one with four tabs) */
			LView* theView = FindPaneByID_(this, v5View_ID, LView);
			
			/* Switch to the v5 pane */
			theSuperMultiPanelView->SwitchToPanel(2,false);
			
		
			/* Version Number */
			LStaticText* theText;
			theText = FindPaneByID_(this, Version_ID, LStaticText);
			LStr255 theString, anotherString;
			theString.Assign("Kerberos v5");
			theText->SetText(theString);
			theText->SetFontStyle(smallSystemFont);
			
			/* Start Time */
			theText = FindPaneByID_(theView, StartTime_ID, LStaticText);
			theText->SetText( DumpDate(creds5->starttime) );
			theText->SetFontStyle(smallSystemFont);
			
			/* End Time */
			theText = FindPaneByID_(theView, EndTime_ID, LStaticText);
			theText->SetText( DumpDate(creds5->endtime) );
			theText->SetFontStyle(smallSystemFont);
			
			/* Issue Time */
			theText = FindPaneByID_(theView, IssueTime_ID, LStaticText);
			theText->SetText( DumpDate(creds5->authtime) );
			theText->SetFontStyle(smallSystemFont);
			
			/* Renewable Time */
			theText = FindPaneByID_(theView, RenewTimeText, LStaticText);
			theText->SetFontStyle(smallSystemFont);
			if ((creds5 -> ticket_flags) & TKT_FLG_RENEWABLE) {
				theText->SetText( DumpDate(creds5->renew_till) );
			} else {
				theString.Assign("Not renewable");
				theText->SetText(theString);
			}
			
			/* Status of tickets - valid, expired, not valid */
			struct timeval currentTime;
			gettimeofday(&currentTime, nil);
			
			UInt32 expireSeconds = creds5->endtime;
			
			theText = FindPaneByID_(theView, ExpiredWarning_ID, LStaticText);
			
			if ((unsigned long)currentTime.tv_sec > expireSeconds) {
				theString.Assign("Expired");
				theText->SetFontStyle(smallSystemFontRed);
				theText->SetText(theString);
			} else {
				theString.Assign("Valid");
				theText->SetFontStyle(smallSystemFont);
				theText->SetText(theString);
			}
			
			KLPrincipal currentPrincipal;
			KLBoolean	tixValid = true;
			KLStatus	klErr = klNoErr;
			
			klErr = KLCreatePrincipalFromString(creds5->client, kerberosVersion_V5, &currentPrincipal);
			if (klErr == klNoErr) {
				klErr = KLCacheHasValidTickets(currentPrincipal, kerberosVersion_V5, &tixValid, nil, nil);
				KLDisposePrincipal(currentPrincipal);
			}

			if (!tixValid) {
				// set color inside if blocks to avoid coloring not-really-expired service tix's status
				if (klErr == klCredentialsBadAddressErr) {
					theString.Assign("Not Valid (IP Address Changed)");
				} else if (klErr == klCredentialsNeedValidationErr) {
					theString.Assign("Not Valid Yet (Needs Validation)");
				} else if (klErr != klCredentialsExpiredErr) {
					theString.Assign("Not Valid (Reason Unknown)");				
				}
				theText->SetFontStyle(smallSystemFontRed);
				theText->SetText(theString);
			}
			
			/* Principal */
			theText = FindPaneByID_(this, Principal_ID, LStaticText);
			theString.Assign(creds5->client);
			theText->SetText(theString);
			theText->SetFontStyle(smallSystemFont);

			/* Service Principal */
			theText = FindPaneByID_(this, ServPrincipal_ID, LStaticText);
			theString.Assign(creds5->server);
			theText->SetText(theString);
			theText->SetFontStyle(smallSystemFont);

			/* Window Title */
			theString.Assign(creds5->client);
			UInt8 theIndex = theString.Find("@", 1, 1);
			theString.Remove(theIndex, (UInt8)(theString.Length()-theIndex+1));
			theString.Append("'s ");
			theString.Append("v5 ");
			anotherString.Assign(creds5->server);
			theIndex = anotherString.Find("/", 1, 1);
			anotherString.Remove(theIndex, (UInt8)(anotherString.Length()-theIndex+1));
			theString.Append(anotherString);
			theString.Append(" Ticket Info");
			this->SetDescriptor(theString);
			
						
			/* FLAGS: */
			
			/* forwardable */
			theText = FindPaneByID_(theView, fForwardable_ID, LStaticText);
			if ((creds5 -> ticket_flags) & TKT_FLG_FORWARDABLE) {
				theString.Assign("Yes");
				theText->SetText(theString);}
			else {
				theString.Assign("No");
				theText->SetText(theString); }
				
			/* proxiable */
			theText = FindPaneByID_(theView, fProxiable_ID, LStaticText);
			if ((creds5 -> ticket_flags) & TKT_FLG_PROXIABLE) {
				theString.Assign("Yes");
				theText->SetText(theString); }
			else {
				theString.Assign("No");
				theText->SetText(theString); }
				
			/* forwarded */
			theText = FindPaneByID_(theView, fForwarded_ID, LStaticText);
			if ((creds5 -> ticket_flags) & TKT_FLG_FORWARDED) {
				theString.Assign("Yes");
				theText->SetText(theString); }
			else {
				theString.Assign("No");
				theText->SetText(theString); }
							
			/* proxy */
			theText = FindPaneByID_(theView, fProxy_ID, LStaticText);
			if ((creds5 -> ticket_flags) & TKT_FLG_PROXY) {
				theString.Assign("Yes");
				theText->SetText(theString); }
			else {
				theString.Assign("No");
				theText->SetText(theString); }
							
			/* may postdate */
			theText = FindPaneByID_(theView, fMayPostdate_ID, LStaticText);
			if ((creds5 -> ticket_flags) & TKT_FLG_MAY_POSTDATE) {
				theString.Assign("Yes");
				theText->SetText(theString); }
			else {
				theString.Assign("No");
				theText->SetText(theString); }
			
			/* postdated */
			theText = FindPaneByID_(theView, fPostdated_ID, LStaticText);
			if ((creds5 -> ticket_flags) & TKT_FLG_POSTDATED) {
				theString.Assign("Yes");
				theText->SetText(theString); }
			else {
				theString.Assign("No");
				theText->SetText(theString); }
						
			/* invalid */
			theText = FindPaneByID_(theView, fInvalid_ID, LStaticText);
			if ((creds5 -> ticket_flags) & TKT_FLG_INVALID) {
				theString.Assign("Yes");
				theText->SetText(theString); }
			else {
				theString.Assign("No");
				theText->SetText(theString); }
				
			/* renewable */
			theText = FindPaneByID_(theView, fRenewable_ID, LStaticText);
			if ((creds5 -> ticket_flags) & TKT_FLG_RENEWABLE) {
				theString.Assign("Yes");
				theText->SetText(theString); }
			else {
				theString.Assign("No");
				theText->SetText(theString); }
				
			/* initial */
			theText = FindPaneByID_(theView, fInitial_ID, LStaticText);
			if ((creds5 -> ticket_flags) & TKT_FLG_INITIAL) {
				theString.Assign("Yes");
				theText->SetText(theString); }
			else {
				theString.Assign("No");
				theText->SetText(theString); }
			
			/* pre-authenticated */
			theText = FindPaneByID_(theView, fPreAuth_ID, LStaticText);
			if ((creds5 -> ticket_flags) & TKT_FLG_PRE_AUTH) {
				theString.Assign("Yes");
				theText->SetText(theString); }
			else {
				theString.Assign("No");
				theText->SetText(theString); }
				
			/* HW Authenticated */
			theText = FindPaneByID_(theView, fHWAuth_ID, LStaticText);
			if ((creds5 -> ticket_flags) & TKT_FLG_HW_AUTH) {
				theString.Assign("Yes");
				theText->SetText(theString); }
			else {
				theString.Assign("No");
				theText->SetText(theString); }
			
			/* Is S-Key */
			theText = FindPaneByID_(theView, fSKey_ID, LStaticText);
			if (creds5 -> is_skey) {
				theString.Assign("Yes");
				theText->SetText(theString); }
			else {
				theString.Assign("No");
				theText->SetText(theString); }
							
						
			/* Table Implementation of IP Addresses */
			theText = FindPaneByID_(theView, cIPAddress_ID, LStaticText);
			theString.Assign("IP Addresses:");
			theText->SetText(theString);
			
			LTextColumn* theTextColumn;
			TableIndexT maxRows, maxCols;
						
			theTextColumn = FindPaneByID_(theView, IPAddressTable_ID, LTextColumn);
			
			// disallow selecting in the IP address table to avoid selection-under-Platnium bugs
			theTextColumn->SetTableSelector(nil);

			if ((creds5->addresses == NULL) || (*(creds5->addresses) == NULL)) {
				LScrollerView* theScrollerView;
				theScrollerView = FindPaneByID_(theView, IPAddressScroller_ID, LScrollerView);
				theScrollerView->Hide();
				theText = FindPaneByID_(theView, IPAddress_ID, LStaticText);
				theString.Assign("None");
				theText->SetText(theString);
				theText->SetFontStyle(smallSystemFont);
				theText->Show();
			} else {
				for (cc_data** data = creds5->addresses; *data != NULL; data++) {
					switch ((*data)->type) {
						case ADDRTYPE_INET: {
							cc_uint32 address = *(cc_uint32*)((*data) -> data);
							
							theString.Assign((long)((address & 0xFF000000) >> 24));
							theString.Append(".");
							theString.Append((long)((address & 0x00FF0000) >> 16));
							theString.Append(".");
							theString.Append((long)((address & 0x0000FF00) >> 8));
							theString.Append(".");
							theString.Append((long)(address & 0x000000FF));
							
							theTextColumn->GetTableSize(maxRows, maxCols);
							theTextColumn->InsertRows(1, maxRows, theString.ConstTextPtr(), theString.Length(), true);
							break;
							
							}
						}
						
					}
				}
			
			/* ENCTYPES */
			
			// session key
			krb5_error_code krb5err;
			char encTypeString[256];

			theText = FindPaneByID_(theView, EncryptionLabel1_ID, LStaticText);
			theString.Assign("Session Key Encryption Type:");
			theText->SetText(theString);
			
			theText = FindPaneByID_(theView, EncryptionType1_ID, LStaticText);
			
			krb5err = krb5_enctype_to_string((krb5_enctype)creds5->keyblock.type, encTypeString, 256);
			if (krb5err == 0) {
				theString.Assign(encTypeString);
			} else {
				theString.Assign("Unknown Enctype");
			}
			theText->SetText(theString);
			theText->SetFontStyle(smallSystemFont);
			
			// service principal key
			krb5_ticket *decodedTicket = nil;
			krb5_data tempKrb5Data;
			
			theText = FindPaneByID_(theView, EncryptionLabel2_ID, LStaticText);
			theString.Assign("Service Principal Key Encryption Type:");
			theText->SetText(theString);
			
			tempKrb5Data.length = (int)creds5->ticket.length;
			tempKrb5Data.data = (char *)creds5->ticket.data;
			
			krb5err = krb5_decode_ticket(&tempKrb5Data, &decodedTicket);
			
			theText = FindPaneByID_(theView, EncryptionType2_ID, LStaticText);

			krb5err = krb5_enctype_to_string(decodedTicket->enc_part.enctype, encTypeString, 256);
			if (krb5err == 0) {
				theString.Assign(encTypeString);
			} else if (krb5err == EINVAL) { // invalid argument
				theString.Assign("Enctype #");
				theString.Append((short)decodedTicket->enc_part.enctype);
			} else {
				theString.Assign("Unknown Enctype");
			}
			
			theText->SetText(theString);
			theText->SetFontStyle(smallSystemFont);
			
			krb5_context tempContext;
			krb5err = krb5_init_context(&tempContext);
			
			krb5_free_ticket(tempContext, decodedTicket);
			
			krb5_free_context(tempContext);
			
			/* Finally show the window */
			this->Show();
			break;
		}
	}
}

// ---------------------------------------------------------------------------
//	¥ DumpDate										[private]
// ---------------------------------------------------------------------------
//	Dump Date Info into an LString

LStr255
CTicketInfoWindow::DumpDate(cc_time_t inDate)
{
	/* Construct an LString Object */
	LStr255 theDateString;
	LStr255 theTimeString;
	
	
	/* 1970 time */
	time_t numSeconds = (time_t) inDate;
	
	/* In the case of v4 long lifetimes, unix_time_to_mac_time can't deal because
	   the times are UInt32's and time_t is signed long.  So until we fix that,
	   just display a message that the end time is really far away... */
	if (numSeconds < 0) {
		theDateString.Append("Essentially Infinite");
		return theDateString;
	}
	
	/* then we go to 1904 time */
#if !TARGET_RT_MAC_MACHO
	unix_time_to_mac_time (&numSeconds);
#else
	/* Don't have unix_time_to_mac_time on Mac, so we have to hack using constants from a variety of locations */
	MachineLocation ml;
	long gmt_offset = 0;
	
	::ReadLocation(&ml);
	if (ml.latitude != 0 || ml.longitude != 0) {
		gmt_offset = ml.u.gmtDelta & 0x00ffffff;
		if ((gmt_offset >> 23) == 1)
			gmt_offset |= 0xff000000;
	} else {
		gmt_offset = 0;
	}
	
	double unixToMacTimeOffset =  (double)kCFAbsoluteTimeIntervalSince1904 - (double)kCFAbsoluteTimeIntervalSince1970;
	
	numSeconds = numSeconds + (long)unixToMacTimeOffset + gmt_offset;
  
#endif
	
	DateString(numSeconds, longDate, theDateString, nil);
	TimeString(numSeconds, true, theTimeString, nil);
	theDateString.Append(" ");
	theDateString.Append(theTimeString);
	
	return theDateString;
}

