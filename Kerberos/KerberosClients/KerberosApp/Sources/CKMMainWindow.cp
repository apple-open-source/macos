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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/CKMMainWindow.cp,v 1.54 2003/07/16 20:41:11 smcguire Exp $ */

// =================================================================================
//	CKMMainWindow.cp
// =================================================================================
/*
	Subclass of LWindow, just so we can put in a hooks to set the preferences
	when the window is moved, handle buttons, set active user info, etc.
*/

#include <utime.h>
#include <string.h>

#if !TARGET_RT_MAC_MACHO	// no UtilitiesLib in Mach-o land
	#include <KerberosSupport/Utilities.h>
	#include <MacApplication.h>
#else
	#include <sys/time.h>
#endif

#include <LPushButton.h>
#include <LStaticText.h>

#include "CKMMainWindow.h"
#include "CKerberosManagerApp.h"
#include "CKrbPreferences.h"
#include "CTicketListCredentialsItem.h"
#include "CTicketListPrincipalItem.h"
#include "CBSDAttachment.h"

PP_Using_Namespace_PowerPlant

// ---------------------------------------------------------------------------
//		• Constructor from stream
// ---------------------------------------------------------------------------
CKMMainWindow::CKMMainWindow(LStream*	inStream) : LWindow(inStream)
{
	mKPrincList = nil;
	mKSessionRef = nil;

	AddAttachment (new CBSDAttachment (msg_KeyPress, true));
}

// ---------------------------------------------------------------------------
//		• Destructor
// ---------------------------------------------------------------------------
CKMMainWindow::~CKMMainWindow()
{
	this->StopIdling();
	
	if (mKPrincList != NULL)
		delete mKPrincList;
}

// ---------------------------------------------------------------------------
//		• FinishCreateSelf
// ---------------------------------------------------------------------------
void CKMMainWindow::FinishCreateSelf()
{

	LPane *paneToLocate;
	
	mKPrincList = dynamic_cast<CPrincList *>(this->FindPaneByID(rPrincListTable));
	Assert_(mKPrincList != nil);
	this->SetLatentSub(mKPrincList);
	
	//get a pointer to the krb session
	CKerberosManagerApp *theApp = static_cast<CKerberosManagerApp *>(LCommander::GetTopCommander());
	mKSessionRef = theApp->GetKrbSession();
	Assert_(mKSessionRef != nil);

	// get a pointer to the preferences object
	mKrbPrefsRef = theApp->GetKrbPreferencesRef();
	Assert_(mKrbPrefsRef != nil);
	
	// add us as a listener to the prefs so we can get the prefs changed broadcast messages
	mKrbPrefsRef->AddListener(this);
	
	// listen to window's buttons
	UReanimator::LinkListenerToBroadcasters(this, this, rMainWindow);

	// set buttons right before we show the window...
	this->FixButtonStatus();
	
	this->StartIdling(0.5);

	// save initial window width and height
	mInitialWindowWidth = (UInt16)(mUserBounds.right - mUserBounds.left);

/*	
	// save title strings initial position
	paneToLocate = dynamic_cast<LPane*>(this->FindPaneByID(rTitleLabel1));
	Assert_(paneToLocate != nil);
	
	paneToLocate->GetFrameLocation(mTitleStringsInitialPosition);
	
	// save icon initial position
	paneToLocate = dynamic_cast<LPane*>(this->FindPaneByID(rIconControl));
	Assert_(paneToLocate != nil);
	
	paneToLocate->GetFrameLocation(mIconInitialPosition);
*/
	
	// save active user labels initial position
	paneToLocate = dynamic_cast<LPane*>(this->FindPaneByID(rActiveUserLabel));
	Assert_(paneToLocate != nil);
	
	paneToLocate->GetFrameLocation(mActiveUserLabelsInitialPosition);

	// save active user text fields initial position
	paneToLocate = dynamic_cast<LPane*>(this->FindPaneByID(rActiveUserText));
	Assert_(paneToLocate != nil);
	
	paneToLocate->GetFrameLocation(mActiveUserTextFieldsInitialPosition);
	
	// calculate an offset to use later to resize the active user text fields
	SDimension16 activeUserTextFieldsSize;
	paneToLocate->GetFrameSize(activeUserTextFieldsSize);
	mActiveUserTextFieldsRightOffset = (UInt16)(mInitialWindowWidth - mActiveUserTextFieldsInitialPosition.h - activeUserTextFieldsSize.width);

	// save renew button initial position
	paneToLocate = dynamic_cast<LPane*>(this->FindPaneByID(rRenewButton));
	Assert_(paneToLocate != nil);
	
	paneToLocate->GetFrameLocation(mRenewButtonInitialPosition);
	 
	// save destroy tix button initial position
	paneToLocate = dynamic_cast<LPane*>(this->FindPaneByID(rDestroyTicketsButton));
	Assert_(paneToLocate != nil);
	
	paneToLocate->GetFrameLocation(mDestroyTicketsButtonInitialPosition);

	// save change password button initial position
	paneToLocate = dynamic_cast<LPane*>(this->FindPaneByID(rChangePasswordButton));
	Assert_(paneToLocate != nil);
	
	paneToLocate->GetFrameLocation(mChangePasswordButtonInitialPosition);

}

// ---------------------------------------------------------------------------
//		• ObeyCommand
// ---------------------------------------------------------------------------
//	Respond to commands

Boolean
CKMMainWindow::ObeyCommand(
	CommandT	inCommand,
	void		*ioParam)
{
	Boolean		cmdHandled = true;
	Str255		name;
	cc_int32	princVersion;
	short menuID, menuItem;

	//handle synthetic commands
	if (IsSyntheticCommand(inCommand, menuID, menuItem)) {
		cmdHandled = LWindow::ObeyCommand(inCommand, ioParam);
		return cmdHandled;
	}

	switch (inCommand) {
		case cmd_Close:
			this->AttemptClose();
			//ProcessCommand(cmd_Quit);
			break;
			
 		case cmd_RenewSelectedUser:
 			if (mKPrincList->GetSelectedPrincipal(name, &princVersion)) {
 				mKSessionRef->DoRenew(name, princVersion);
 			}
 			break;
 		
 		case cmd_RenewActiveUser:
 			if (mKPrincList->GetActivePrincipal(name, &princVersion)) {
 				mKSessionRef->DoRenew(name, princVersion);
 			}
 			break;
 		
 		case cmd_DestroySelectedUser:
 			if (mKPrincList->GetSelectedPrincipal(name, &princVersion)) {
	 			LString::PToCStr(name);
	 			mKSessionRef->DoLogout((char *)name, princVersion);
 			}
 			break;
 		
 		case cmd_DestroyActiveUser:
 			if (mKPrincList->GetActivePrincipal(name, &princVersion)) {
	 			LString::PToCStr(name);
	 			mKSessionRef->DoLogout((char *)name, princVersion);
 			}
 			break;
 		
 		case cmd_ChangePassword:
 			if (mKPrincList->GetSelectedPrincipal(name, &princVersion)) {
 				mKSessionRef->ChangePassword(name, princVersion);
			}
			break;
			
		case cmd_GetCredInfo:
			if ((mKSessionRef) && (mKPrincList)) {
				STableCell sel = mKPrincList->GetFirstSelectedCell();
				CTicketListItem *selectedItem;
				Boolean result = mKPrincList->GetItemFromCell(sel, &selectedItem);
				if (result)
					if (selectedItem->GetItemType() == kCredentialsItem)
						((CTicketListCredentialsItem *)selectedItem)->OpenTicketInfoWindow();
			}
			break;
			
		default:
			cmdHandled = LWindow::ObeyCommand(inCommand, ioParam);
			break;
	}

	return cmdHandled;

}

// ---------------------------------------------------------------------------
//		• FindCommandStatus
// ---------------------------------------------------------------------------
void CKMMainWindow::FindCommandStatus(
	CommandT	inCommand,
	Boolean		&outEnabled,
	Boolean		&outUsesMark,
	UInt16		&outMark,
	Str255		outName)
{
	short  menuID, menuItem;
	Str255 selectedPrinc;
	
	//handle synthetic commands
	if (IsSyntheticCommand(inCommand, menuID, menuItem)) {
		LWindow::FindCommandStatus(inCommand, outEnabled,
										outUsesMark, outMark, outName);
		return;
	}
	
	switch (inCommand) {
	
		case cmd_RenewSelectedUser:
			if ((mKSessionRef) && (mKPrincList) && (mKPrincList->GetSelectedPrincipal(selectedPrinc, nil)) && UDesktop::WindowIsSelected(this)) {
				outEnabled = true;
			} else {
				outEnabled = false;
			}
			
			break;
		case cmd_DestroySelectedUser:
			if ((mKSessionRef) && (mKPrincList) && (mKPrincList->GetSelectedPrincipal(selectedPrinc, nil)) && UDesktop::WindowIsSelected(this)) {
				outEnabled = true;
			} else {
				outEnabled = false;
			}
			
			break;
		case cmd_ChangePassword:
			if ((mKSessionRef) && (mKPrincList) && (mKPrincList->GetSelectedPrincipal(selectedPrinc, nil)) && UDesktop::WindowIsSelected(this)) {
				outEnabled = true;
			} else {
				outEnabled = false;
			}
			
			break;
		
		case cmd_GetCredInfo:
			if ((mKSessionRef) && (mKPrincList)) {
				outEnabled = false;
				
				// determine if there's a selected cell and if there is, what type of item it is
				STableCell sel = mKPrincList->GetFirstSelectedCell();
				CTicketListItem *selectedItem;
				Boolean result = mKPrincList->GetItemFromCell(sel, &selectedItem);
				if (result)
					if (selectedItem->GetItemType() == kCredentialsItem)
						outEnabled = true;
			} else {
				outEnabled = false;
			}
			break;
		
		default:
			LWindow::FindCommandStatus(inCommand, outEnabled,
												outUsesMark, outMark, outName);
			break;
	}		
}

// ---------------------------------------------------------------------------
//		• ListenToMessage
// ---------------------------------------------------------------------------
//	Listen to messages from things like buttons.
//
void CKMMainWindow::ListenToMessage(MessageT inMessage, void *ioParam)  {
#pragma unused (ioParam)
	
	//buttons send their paneID as a mesage.
	switch(inMessage) {
		case msg_Renew:
			ProcessCommand(cmd_RenewSelectedUser, this);
			break;
			
		case msg_Destroy:
			ProcessCommand(cmd_DestroySelectedUser, this);
			break;
			
		case msg_ChangePasswd:
			ProcessCommand(cmd_ChangePassword, this);
			break;
		
		case msg_PrefsDisplayTimeInDockChanged:
			this->UpdateActiveUserInfo(true);
			break;
			
		case CBSDAttachment::msg_BSDLayout:
			this->BSDButtonNames();
			break;
			
		case CBSDAttachment::msg_MooLayout:
			this->MooButtonNames();
			break;
			
		case CBSDAttachment::msg_MacLayout:
			this->StandardButtonNames();
			break;
			
		default:
			break;
	}
}

// ---------------------------------------------------------------------------
//		• SpendTime
// ---------------------------------------------------------------------------
// periodic activities for CKerberosManagerApp
void CKMMainWindow::SpendTime(const EventRecord&) {
	
	//fix up the static text fields with user info
	//also updates the ticket list
	UpdateActiveUserInfo(false);
}

// ---------------------------------------------------------------------------
//		• HandleKeyPress
// ---------------------------------------------------------------------------
// catch Return or Enter key and treat them as clicks on "New Login" button
Boolean CKMMainWindow::HandleKeyPress(const EventRecord& inKeyEvent) {
	Boolean		keyHandled	= true;
	LControl*	keyButton	= nil;
	UInt16		theChar = (UInt16) (inKeyEvent.message & charCodeMask);

	if ( (theChar == char_Enter) || (theChar == char_Return) ) {
		keyButton = dynamic_cast<LControl*>(this->FindPaneByID(rNewLoginButton));
		if (keyButton != nil) {
			keyButton->SimulateHotSpotClick(kControlButtonPart);
		} else {
			keyHandled = LCommander::HandleKeyPress(inKeyEvent);
		}
	} else {
		keyHandled =  LCommander::HandleKeyPress(inKeyEvent);
	}

	return keyHandled;
}

// ---------------------------------------------------------------------------
//		• ClickInDrag
// ---------------------------------------------------------------------------
void CKMMainWindow::ClickInDrag(const EventRecord	&inMacEvent)
{
	Rect oldBoundsRect, newBoundsRect;

	this->GetGlobalBounds(oldBoundsRect);

	LWindow::ClickInDrag(inMacEvent);
	
	this->GetGlobalBounds(newBoundsRect);
	// check to make sure user actually moved window, not just clicked in title bar
	if (!::EqualRect(&oldBoundsRect, &newBoundsRect)) {
		
		// tell preferences to stop autopositioning and to use new position
		mKrbPrefsRef->SetMainWindowRect(false, &newBoundsRect);
	}
}

// ---------------------------------------------------------------------------
//		• DoSetBounds
// ---------------------------------------------------------------------------
void CKMMainWindow::DoSetBounds(const Rect&		inBounds)
{
	LPane *paneToMove = nil;
	
	short widthChange;
	SPoint32 currentPosition;
	SDimension16 currentSize;
	
	//widthChange = (short)((inBounds.right - inBounds.left) - (mUserBounds.right - mUserBounds.left));
	widthChange = (short)((inBounds.right - inBounds.left) - mInitialWindowWidth);
	
	/*
	// recenter title strings
	paneToMove = dynamic_cast<LPane*>(this->FindPaneByID(rTitleLabel1));
	Assert_(paneToMove != nil);
	
	paneToMove->GetFrameLocation(currentPosition);
	paneToMove->PlaceInSuperFrameAt(mTitleStringsInitialPosition.h + widthChange/2, currentPosition.v, true);
	
	paneToMove = dynamic_cast<LPane*>(this->FindPaneByID(rTitleLabel2));
	Assert_(paneToMove != nil);
	
	paneToMove->GetFrameLocation(currentPosition);
	paneToMove->PlaceInSuperFrameAt(mTitleStringsInitialPosition.h + widthChange/2, currentPosition.v, true);
	
	paneToMove = dynamic_cast<LPane*>(this->FindPaneByID(rTitleLabel3));
	Assert_(paneToMove != nil);
	
	paneToMove->GetFrameLocation(currentPosition);
	paneToMove->PlaceInSuperFrameAt(mTitleStringsInitialPosition.h + widthChange/2, currentPosition.v, true);
	
	// recenter icon
	paneToMove = dynamic_cast<LPane*>(this->FindPaneByID(rIconControl));
	Assert_(paneToMove != nil);
	
	paneToMove->PlaceInSuperFrameAt(mIconInitialPosition.h + widthChange/2, mIconInitialPosition.v, true);
	*/

	// recenter password button
	paneToMove = dynamic_cast<LPane*>(this->FindPaneByID(rChangePasswordButton));
	Assert_(paneToMove != nil);

	paneToMove->PlaceInSuperFrameAt(mChangePasswordButtonInitialPosition.h + widthChange/2, mChangePasswordButtonInitialPosition.v, true);
			
	// recenter destroy tickets button
	paneToMove = dynamic_cast<LPane*>(this->FindPaneByID(rDestroyTicketsButton));
	Assert_(paneToMove != nil);
	
	paneToMove->PlaceInSuperFrameAt(mDestroyTicketsButtonInitialPosition.h + widthChange/2, mDestroyTicketsButtonInitialPosition.v, true);

	// recenter renew button
	paneToMove = dynamic_cast<LPane*>(this->FindPaneByID(rRenewButton));
	Assert_(paneToMove != nil);
	
	paneToMove->PlaceInSuperFrameAt(mRenewButtonInitialPosition.h + widthChange/2, mRenewButtonInitialPosition.v, true);

	LPane *userGroupBox = dynamic_cast<LPane*>(this->FindPaneByID(rActiveUserGroupBox));
	Assert_(userGroupBox != nil);
	
	SPoint32 userGroupBoxFrame;
	userGroupBox->GetFrameLocation(userGroupBoxFrame);
	
	// recenter active user label
	paneToMove = dynamic_cast<LPane*>(this->FindPaneByID(rActiveUserLabel));
	Assert_(paneToMove != nil);
	
	paneToMove->GetFrameLocation(currentPosition);
	paneToMove->PlaceInSuperFrameAt(mActiveUserLabelsInitialPosition.h + widthChange/4 - userGroupBoxFrame.h, currentPosition.v - userGroupBoxFrame.v, true);

	// recenter active user realm label
	paneToMove = dynamic_cast<LPane*>(this->FindPaneByID(rActiveRealmLabel));
	Assert_(paneToMove != nil);
	
	paneToMove->GetFrameLocation(currentPosition);
	paneToMove->PlaceInSuperFrameAt(mActiveUserLabelsInitialPosition.h + widthChange/4 - userGroupBoxFrame.h, currentPosition.v - userGroupBoxFrame.v, true);

	// recenter active user time remaining label
	paneToMove = dynamic_cast<LPane*>(this->FindPaneByID(rActiveTimeRemainingLabel));
	Assert_(paneToMove != nil);
	
	paneToMove->GetFrameLocation(currentPosition);
	paneToMove->PlaceInSuperFrameAt(mActiveUserLabelsInitialPosition.h + widthChange/4 - userGroupBoxFrame.h, currentPosition.v - userGroupBoxFrame.v, true);
		
	// recenter active user text
	paneToMove = dynamic_cast<LPane*>(this->FindPaneByID(rActiveUserText));
	Assert_(paneToMove != nil);
	
	paneToMove->GetFrameLocation(currentPosition);
	paneToMove->PlaceInSuperFrameAt(mActiveUserTextFieldsInitialPosition.h + widthChange/4 - userGroupBoxFrame.h, currentPosition.v - userGroupBoxFrame.v, true);
	
	// resize active user text to take up full width
	paneToMove->GetFrameLocation(currentPosition);
	paneToMove->GetFrameSize(currentSize);
	paneToMove->ResizeFrameBy((SInt16)((inBounds.right - inBounds.left) - currentPosition.h - currentSize.width - mActiveUserTextFieldsRightOffset), 0, true);
		
	// recenter active user realm text
	paneToMove = dynamic_cast<LPane*>(this->FindPaneByID(rActiveRealmText));
	Assert_(paneToMove != nil);
	
	paneToMove->GetFrameLocation(currentPosition);
	paneToMove->PlaceInSuperFrameAt(mActiveUserTextFieldsInitialPosition.h + widthChange/4 - userGroupBoxFrame.h, currentPosition.v - userGroupBoxFrame.v, true);

	// resize active user realm text to take up full width
	paneToMove->GetFrameLocation(currentPosition);
	paneToMove->GetFrameSize(currentSize);
	paneToMove->ResizeFrameBy((SInt16)((inBounds.right - inBounds.left) - currentPosition.h - currentSize.width - mActiveUserTextFieldsRightOffset), 0, true);

	// recenter active user time remaining text
	paneToMove = dynamic_cast<LPane*>(this->FindPaneByID(rActiveTimeRemainingText));
	Assert_(paneToMove != nil);
	
	paneToMove->GetFrameLocation(currentPosition);
	paneToMove->PlaceInSuperFrameAt(mActiveUserTextFieldsInitialPosition.h + widthChange/4 - userGroupBoxFrame.h, currentPosition.v - userGroupBoxFrame.v, true);

	// resize active user time remaining text to take up full width
	paneToMove->GetFrameLocation(currentPosition);
	paneToMove->GetFrameSize(currentSize);
	paneToMove->ResizeFrameBy((SInt16)((inBounds.right - inBounds.left) - currentPosition.h - currentSize.width - mActiveUserTextFieldsRightOffset), 0, true);

	// tell preferences to stop autopositioning and to use new position and size
	mKrbPrefsRef->SetMainWindowRect(false, &inBounds);

	// call superclass's method
	LWindow::DoSetBounds(inBounds);
}

// ---------------------------------------------------------------------------
//		• AttemptClose
// ---------------------------------------------------------------------------
void CKMMainWindow::AttemptClose()
{
	if (RunningUnderMacOSX())
		this->Hide();
	else
		ProcessCommand(cmd_Quit);
}

// ---------------------------------------------------------------------------
//		• DoClose
// ---------------------------------------------------------------------------
void CKMMainWindow::DoClose()
{
	if (RunningUnderMacOSX())
		this->Hide();
	else
		ProcessCommand(cmd_Quit);
}

// ---------------------------------------------------------------------------
//		• UpdateActiveUserInfo
// ---------------------------------------------------------------------------
// update the static text fields for the active user
// also, due to a race condition, fix up the list too
void CKMMainWindow::UpdateActiveUserInfo(Boolean forceUpdate) {
	
	Str255 activeUserPrincipal, activeUserRealm;
	LStaticText *staticText;
	long  activeUserDiffTime;
	int hours = 0, minutes = 0, seconds = 0;
	char hourString[10], minuteString[10];
	char timeString[200];
	char shortTimeString[50];
	static long lastTime = (1 << 31);
	bool cacheChanged = false;
	short activeUserValidity;
	
	//update buttons
	this->FixButtonStatus();
	
	//get info about the active user
	mKSessionRef->GetActiveUserInfo(activeUserPrincipal, activeUserDiffTime, activeUserRealm, activeUserValidity);

	//update the active user info
	//test to see if the user has changed
	cacheChanged = mKSessionRef->CCacheChanged();
	if (cacheChanged || forceUpdate) {
		//also update the list contents
		mKPrincList->UpdateListFromCache();
		
		this->UpdateUsersMenu();

		//get the user caption from the window
		staticText = dynamic_cast<LStaticText *>(this->FindPaneByID(rActiveUserText));
		Assert_(staticText != NULL);
		
		staticText->SetDescriptor(activeUserPrincipal); //update the new user

		staticText = dynamic_cast<LStaticText *>(this->FindPaneByID(rActiveRealmText));
		Assert_(staticText != NULL);

		staticText->SetDescriptor(activeUserRealm); //update the realm

	}
	
	//tick expiration time (only called once every 30 seconds)
	if ((lastTime - activeUserDiffTime > 30) || (cacheChanged) || (forceUpdate)){
		if ((activeUserDiffTime <= 0) || (activeUserValidity != kTicketValid)) {
			if (::EqualString(kNoActiveUserRealmString, activeUserRealm, false, false)) {
				// no authenticated user
				LString::CopyPStr(kNoActiveUserTimeString, (unsigned char *)timeString, sizeof(timeString));
				LString::PToCStr((unsigned char *)timeString);
			} else {
				// authenticated user but expired or not valid
				if (activeUserValidity == kTicketInvalidBadAddress)
					sprintf(timeString, "Tickets Not Valid (IP Address Changed)");
				else if (activeUserValidity == kTicketInvalidNeedsValidation)
					sprintf(timeString, "Tickets Not Valid Yet (Needs Validation)");
				else if (activeUserValidity == kTicketInvalidUnknown)
					sprintf(timeString, "Tickets Not Valid (Unknown Reason)");
				else
					sprintf(timeString, "Expired");
			}
			
			sprintf(shortTimeString,"--:--");
		} else {
			hours = activeUserDiffTime / (60*60);
			minutes = (activeUserDiffTime - hours*3600) / 60;
			seconds = (activeUserDiffTime - hours*3600) % 60;
			
			sprintf(shortTimeString, "%d:%02d", hours, minutes);
			
			if (hours == 1)
				strcpy(hourString,"hour");
			else
				strcpy(hourString,"hours");
			
			if (minutes == 1)
				strcpy(minuteString,"minute");
			else
				strcpy(minuteString,"minutes");
				
			if ( (seconds > 0) && (minutes == 0) && (hours == 0) ) {
					sprintf(timeString, "Less than 1 minute");
					sprintf(shortTimeString, "0:01");	// special case less than 1 minute so that it doesn't display "0:00" which looks stupid
		} else if (minutes < 10 && hours >= 1)
					sprintf(timeString, "About %d %s", hours, hourString);
			else if (minutes > 50)
				sprintf(timeString, "Less than %d hours", hours + 1);
			else
				sprintf(timeString, "%d %s, %d %s", hours, hourString, minutes, minuteString);
		}
		LString::CToPStr(timeString);

		staticText = dynamic_cast<LStaticText *>(this->FindPaneByID(rActiveTimeRemainingText));
		Assert_(staticText != NULL);

		// set color of tix expiration time lesss than 5 minutes in red, unless it's the special "no user" time string
		if ( ( ((minutes < 5) && (hours == 0)) || (activeUserValidity != kTicketValid) ) && !::EqualString((unsigned char *)timeString, kNoActiveUserTimeString, false, false)) {
			staticText->SetTextTraitsID(rSystemFontRed);
		} else {
			staticText->SetTextTraitsID(rSystemFontPlain);
		}

		// draw the text of the expiration time
		staticText->SetDescriptor((unsigned char *)timeString);
		
		// scan the tix list for principals to auto-renew, if that pref is set
		if (mKrbPrefsRef->GetAutoRenewTicketsPref())
			mKPrincList->CheckPrincipalsForAutoRenewal();
		
		// THIS CALL FORCES THE TICKET LIST TO UPDATE THE TICKET TIME REMAININGS WITHOUT RELOADING THE CACHE
		// because it invalidates the list rectange and that calls DrawCell() for all of them
		
		mKPrincList->Refresh();

		// update dock icon as appropriate (only applies to Mac OS X)
		if ((activeUserDiffTime <= 0) || (activeUserValidity != kTicketValid)) {
			this->UpdateDockIcon(kTicketsExpiredIcon, shortTimeString);		
		} else if((minutes < 5) && (hours == 0)) {
			this->UpdateDockIcon(kTicketsWarningIcon, shortTimeString);
		} else	{	
			this->UpdateDockIcon(kTicketsVaildIcon, shortTimeString);
		}

		// reset counter
		lastTime = activeUserDiffTime;
	}
/*
	// attempt to make minimized ticket list window update in the dock; this sometimes works, sometimes doesn't
	if (cacheChanged || forceUpdate) {
		//QDFlushPortBuffer(this->GetMacPort(), NULL);
		::UpdateCollapsedWindowDockTile((WindowRef)this->GetMacWindow());
	}
*/
}

// ---------------------------------------------------------------------------
//		• UpdateDockIcon
// ---------------------------------------------------------------------------
// this doesn't actually change the main window, but it's related to updating the other info
// only works on Mac OS X
void CKMMainWindow::UpdateDockIcon(short iconToDraw, char *timeRemainingString)
{
#if TARGET_API_MAC_CARBON
	if (RunningUnderMacOSX()) {
 		OSErr err = noErr;

		// get the requested icon
		IconRef myIconRef = NULL;
		
		IconFamilyHandle myIconFamilyH = (IconFamilyHandle)::GetResource(kIconFamilyType, iconToDraw);
		
		Assert_(myIconFamilyH != NULL);
		
		err = RegisterIconRefFromIconFamily('KrbM', 'KrbM', myIconFamilyH, &myIconRef);
		
		Assert_(err == noErr);
		
		Assert_(::IsValidIconRef(myIconRef));
		
		// get a context to draw the icon on
		CGContextRef dockCGContext;
		
		dockCGContext = BeginCGContextForApplicationDockTile();
		
		CGRect iconRect = ::CGRectMake(0,0,128,128);
		
		CGContextClearRect( dockCGContext, iconRect );
		
		RGBColor iconLabelColor = { 0, 0, 0 };
		
		::PlotIconRefInContext(dockCGContext, &iconRect, kAlignAbsoluteCenter, kTransformNone, &iconLabelColor, kPlotIconRefNormalFlags, myIconRef);
		
		CGContextFlush( dockCGContext );
		
		EndCGContextForApplicationDockTile(dockCGContext);
		
		// get the QD GrafPtr to do the rest of the drawing
		CGrafPtr dockTileGrafPtr = NULL;
		Rect boundingRect;
		
 		dockTileGrafPtr = BeginQDContextForApplicationDockTile();
		
		if (dockTileGrafPtr != NULL) {
			
			::GetPortBounds(dockTileGrafPtr, &boundingRect);

			StGrafPortSaver savedPort;
			
			// draw the icon
			SetPort(dockTileGrafPtr);

			// draw time remaining on dock icon (if pref says to)
			if (mKrbPrefsRef->GetDisplayTimeInDock()) {
				LString::CToPStr(timeRemainingString);
				
				StColorPenState savePenState();
				StTextState saveTextState();
				
				UTextTraits::SetPortTextTraits(rDockTimeRemainingLabelFont);
				
				short strWidth = ::StringWidth((unsigned char *)timeRemainingString);
				
				short offset = (short)((boundingRect.right - boundingRect.left - strWidth) / 2);
				
				::MoveTo(offset, 113);
				
				::DrawString((unsigned char *)timeRemainingString);
			}
			
			::QDFlushPortBuffer(dockTileGrafPtr, NULL);
			
			::EndQDContextForApplicationDockTile(dockTileGrafPtr);
		
		}
                
               ::ReleaseIconRef(myIconRef);
               
		::UnregisterIconRef('KrbM', 'KrbM');
 
                ::ReleaseResource((Handle)myIconFamilyH);
 	}
	
	
#else
	// empty function on Classic
	#pragma unused(iconToDraw, timeRemainingString)
#endif
}

// ---------------------------------------------------------------------------
//		• UpdateUsersMenu
// ---------------------------------------------------------------------------
// updates the hierarchical menu that shows users logged in
void CKMMainWindow::UpdateUsersMenu() {

	CTicketListPrincipalItem *user;
	short menuIndex;
	Str255 userDisplayPrincipalString;
	
	//get strings
	LArray *princNameList = mKPrincList->GetAllPrincipals();
	LArrayIterator iterate(*princNameList, 0L);
	
	//get the menu
	LMenuBar *menuBar = LMenuBar::GetCurrentMenuBar();
	LMenu *activeUsersMenu = menuBar->FetchMenu(rActiveMenu);
	MenuHandle macMenu = activeUsersMenu->GetMacMenuH();
	
	//clear out old menu items
	int itemCount = ::CountMenuItems(macMenu);
	for (int i=0; i <= itemCount; i++) ::DeleteMenuItem(macMenu, 1);
	
	//stuff our strings into the menu
	//can't stuff directly because of slashes in instances
	menuIndex = 1;
	while (iterate.Next( &user) ) {
		::AppendMenu(macMenu, "\pfoo");
		user->GetItemDisplayString(&userDisplayPrincipalString);
		::SetMenuItemText(macMenu, menuIndex, userDisplayPrincipalString);
		
		// put a checkmark next to the active user
		if (user->PrincipalIsActive())
			::CheckMenuItem(macMenu, menuIndex, true);
		
		// make expired/invalid users italic
		struct timeval currentTime;
		gettimeofday(&currentTime, nil);
		if ((user->GetItemExpirationTime() < (unsigned long)currentTime.tv_sec) || (user->GetItemValidity() != kTicketValid)) {
			::SetItemStyle(macMenu, menuIndex, italic);
		}

		menuIndex++;
	}
	
	delete princNameList;
	
	LCommander::SetUpdateCommandStatus(true);
}

// ---------------------------------------------------------------------------
//		• GetPrincList
// ---------------------------------------------------------------------------
//	return a pointer to the CPricList object that we initialized in StartUp
//
CPrincList * CKMMainWindow::GetPrincListRef() {

	return mKPrincList;
}

// ---------------------------------------------------------------------------
//		• FixButtonStatus
// ---------------------------------------------------------------------------
// Enable or Disable buttons as appropriate
//
void CKMMainWindow::FixButtonStatus() {

	LPushButton *theButton[4];
	const int renew = 0;
	const int dest = 1;
	const int pass = 2;
	const int active = 3;
	Str255 dummyPrinc;

	//get pointers to all of the buttons
	theButton[renew] = dynamic_cast<LPushButton *>(this->FindPaneByID(rRenewButton) );
	Assert_(theButton[renew] != NULL);
	theButton[dest] = dynamic_cast<LPushButton *>(this->FindPaneByID(rDestroyTicketsButton) );
	Assert_(theButton[dest] != NULL);
	theButton[pass] = dynamic_cast<LPushButton *>(this->FindPaneByID(rChangePasswordButton) );
	Assert_(theButton[pass] != NULL);
	theButton[active] = dynamic_cast<LPushButton *>(this->FindPaneByID(rMakeActiveUserButton) );
	Assert_(theButton[active] != NULL);

	//a principal is selected, enable make user active and destroy tickets and renew
	if ((mKPrincList) && (mKPrincList->GetSelectedPrincipal(dummyPrinc, nil))) {
		theButton[active]->Enable();
		theButton[dest]->Enable();
		theButton[renew]->Enable();
		theButton[pass]->Enable();
	} else {
		theButton[active]->Disable();
		theButton[dest]->Disable();
		theButton[renew]->Disable();
		theButton[pass]->Disable();
 	}
 }

// ---------------------------------------------------------------------------
//		• StandardButtonNames
// ---------------------------------------------------------------------------
void CKMMainWindow::StandardButtonNames() {
	LPane *buttonToRename = nil;

	// renew button
	buttonToRename = dynamic_cast<LPane*>(this->FindPaneByID(rRenewButton));
	Assert_(buttonToRename != nil);
	buttonToRename->SetDescriptor("\pRenew Tickets…");
	
	// password button
	buttonToRename = dynamic_cast<LPane*>(this->FindPaneByID(rChangePasswordButton));
	Assert_(buttonToRename != nil);
	buttonToRename->SetDescriptor("\pChange Password…");

	// destroy tickets button
	buttonToRename = dynamic_cast<LPane*>(this->FindPaneByID(rDestroyTicketsButton));
	Assert_(buttonToRename != nil);
	buttonToRename->SetDescriptor("\pDestroy Tickets");
	
	// set active users button
	buttonToRename = dynamic_cast<LPane*>(this->FindPaneByID(rMakeActiveUserButton));
	Assert_(buttonToRename != nil);
	buttonToRename->SetDescriptor("\pMake User Active");
		
	// get tickets button
	buttonToRename = dynamic_cast<LPane*>(this->FindPaneByID(rNewLoginButton));
	Assert_(buttonToRename != nil);
	buttonToRename->SetDescriptor("\pGet Tickets…");
}

// ---------------------------------------------------------------------------
//		• MooButtonNames
// ---------------------------------------------------------------------------
void CKMMainWindow::MooButtonNames() {
	LPane *buttonToRename = nil;

	// renew button
	buttonToRename = dynamic_cast<LPane*>(this->FindPaneByID(rRenewButton));
	Assert_(buttonToRename != nil);
	buttonToRename->SetDescriptor("\plxs");
	
	// password button
	buttonToRename = dynamic_cast<LPane*>(this->FindPaneByID(rChangePasswordButton));
	Assert_(buttonToRename != nil);
	buttonToRename->SetDescriptor("\pmeeroh");

	// destroy tickets button
	buttonToRename = dynamic_cast<LPane*>(this->FindPaneByID(rDestroyTicketsButton));
	Assert_(buttonToRename != nil);
	buttonToRename->SetDescriptor("\pmjv");
	
	// set active users button
	buttonToRename = dynamic_cast<LPane*>(this->FindPaneByID(rMakeActiveUserButton));
	Assert_(buttonToRename != nil);
	buttonToRename->SetDescriptor("\psmcguire");
		
	// get tickets button
	buttonToRename = dynamic_cast<LPane*>(this->FindPaneByID(rNewLoginButton));
	Assert_(buttonToRename != nil);
	buttonToRename->SetDescriptor("\pmit macdev");
}

// ---------------------------------------------------------------------------
//		• BSDButtonNames
// ---------------------------------------------------------------------------
void CKMMainWindow::BSDButtonNames() {
	LPane *buttonToRename = nil;

	// renew button
	buttonToRename = dynamic_cast<LPane*>(this->FindPaneByID(rRenewButton));
	Assert_(buttonToRename != nil);
	buttonToRename->SetDescriptor("\pkinit -R");
	
	// password button
	buttonToRename = dynamic_cast<LPane*>(this->FindPaneByID(rChangePasswordButton));
	Assert_(buttonToRename != nil);
	buttonToRename->SetDescriptor("\pchpasswd");

	// destroy tickets button
	buttonToRename = dynamic_cast<LPane*>(this->FindPaneByID(rDestroyTicketsButton));
	Assert_(buttonToRename != nil);
	buttonToRename->SetDescriptor("\pkdestroy");
	
	// set active users button
	buttonToRename = dynamic_cast<LPane*>(this->FindPaneByID(rMakeActiveUserButton));
	Assert_(buttonToRename != nil);
	buttonToRename->SetDescriptor("\psetenv");
		
	// get tickets button
	buttonToRename = dynamic_cast<LPane*>(this->FindPaneByID(rNewLoginButton));
	Assert_(buttonToRename != nil);
	buttonToRename->SetDescriptor("\pkinit");
}