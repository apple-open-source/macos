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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/CEditRealmsDialog.cp,v 1.37 2003/06/27 00:49:33 smcguire Exp $ */

// ===========================================================================
//	CEditRealmsDialog.cp
// ===========================================================================

#include <string.h>

#if TARGET_RT_MAC_CFM
	#include <Kerberos4/Kerberos4.h>
	#include <KerberosProfile/KerberosProfile.h>

	#include <KerberosWrappers/UProfile.h>
	#include <KerberosWrappers/UKerberos5Context.h>
	#include <KerberosLogin/KerberosLogin.h>
#else
	#include <Kerberos/Kerberos.h>
	#include "UProfile.h"			// since these are internal to the Kerberos.framework, must include private copies
	#include "UKerberos5Context.h"
#endif

#include <LEditText.h>

#include "CEditRealmsDialog.h"
#include "CKerberosManagerApp.h"
#include "CKrbSession.h"
#include "CKrbErrorAlert.h"
#include "LPushButton.h"
#include "LWindow.h"
#include "LStdControl.h"
#include "UAMModalDialogs.h"

PP_Using_Namespace_PowerPlant

CEditRealmsDialog::CEditRealmsDialog(LStream *inStream) : LDialogBox(inStream) {
	CKerberosManagerApp *theApp;
	
	//get a pointer to the preferences
	theApp = static_cast<CKerberosManagerApp *>(LCommander::GetTopCommander());
	mKrbPrefsRef = theApp->GetKrbPreferencesRef();
	
	// should check for a failure here
	Assert_(mKrbPrefsRef != nil);

}
	
	
CEditRealmsDialog::~CEditRealmsDialog() {

}

/* Override to catch return keys when the user is typing in the Add Extra Realm field.  In that
   case we will accept the realm, but not exit the dialog. */
Boolean CEditRealmsDialog::HandleKeyPress(
	const EventRecord&	inKeyEvent)
{
	LEditText *extraRealmField;
	UInt8		theChar		= (UInt8) (inKeyEvent.message & charCodeMask);

	extraRealmField = dynamic_cast<LEditText *>(FindPaneByID(rEditRealmsExtraRealmField));
	Assert_(extraRealmField != NULL);

	if ( (theChar == char_Enter) || (theChar == char_Return) ) {
		
		if (extraRealmField->IsTarget()) {
			Str255 tempString;
			extraRealmField->GetDescriptor(tempString);
			if (tempString[0] != 0) {
				this->AddExtraRealm();
				return true;
			}
		}
	} else if (theChar == char_Tab) {
		if (!extraRealmField->IsTarget())
			LCommander::SwitchTarget(extraRealmField);
		else
			LCommander::SwitchTarget(mFavRealmsTable);
		
		this->SetButtonStatus();
	}
	
	// fall through to superclass's method	
	return LDialogBox::HandleKeyPress(inKeyEvent);

}

void CEditRealmsDialog::ListenToMessage(MessageT inMessage, void *ioParam)  {

	SDialogResponse *DResp;
	
	DResp = new SDialogResponse;
	//LWindow *theWindow = UDesktop::FetchTopModal();

	switch (inMessage) {
	
		case msg_OK:
			this->CopyRealmsDialogToPrefs();
			
			break;
		
		case msg_Cancel:
			break;
		
		case msg_EditRealms_FavSelect:
		case msg_EditRealms_AllSelect:
			// when selection in the list changes, enable/disable the buttons
			this->SetButtonStatus();
			break;
			
		case msg_EditRealmsAddButton:
			this->AddSingleRealm();
			this->SetButtonStatus();  // update button status so that remove button will get enabled when more than 1 realm
			break;
		
		case msg_EditRealmsAddAllButton:
			this->AddAllRealms();
			this->SetButtonStatus(); 
			break;
			
		case msg_EditRealmsRemoveButton:
			this->RemoveSingleRealm();
			this->SetButtonStatus(); 
			break;
		
		case msg_rEditRealmsExtraRealmField:
			this->SetButtonStatus();
			break;
		
		case msg_rEditRealmsExtraRealmAddButton:
			this->AddExtraRealm();
			break;
			
		default:
			LDialogBox::ListenToMessage(inMessage, ioParam);

	}
}

void CEditRealmsDialog::FinishCreateSelf()  {
	// Link the dialog to the controls
	UReanimator::LinkListenerToControls( this, this, rEditRealmsDialogWindow );	

	// Set up global member pointing to the Realms List Table (we refer to it a lot)
	mFavRealmsTable = dynamic_cast<CTextColumnColorized *>(FindPaneByID(rEditRealmsFavTextColumn));
	Assert_(mFavRealmsTable != NULL);

	mAllRealmsTable = dynamic_cast<CTextColumnColorized *>(FindPaneByID(rEditRealmsAllTextColumn));
	Assert_(mAllRealmsTable != NULL);

	// The realms table isn't added as a listener because it's hidden in the LScrollerView,
	// so add it manually
	mFavRealmsTable->AddListener(this);
	mAllRealmsTable->AddListener(this);

	// force us to draw white over the grey appearance background
	mFavRealmsTable->SetForeAndBackColors(&Color_Black, &Color_White);
	mAllRealmsTable->SetForeAndBackColors(&Color_Black, &Color_White);

	//fill realm tables with existing data
	this->FillAllRealmsTable();
	this->CopyPrefsToRealmsDialog();
	
	//you can drag out of but not into the All Realms table
	mAllRealmsTable->SetAllowDrops(false);
	
	//set buttons correctly for startup
	this->SetButtonStatus();
}

void CEditRealmsDialog::FindCommandStatus(
	CommandT	inCommand,
	Boolean		&outEnabled,
	Boolean&	/* outUsesMark */ ,
	UInt16&		/* outMark */,
	Str255		/* outName */ )
{
	// Don't enable any commands except cmd_About, which will keep
	// the Apple menu enabled. This function purposely does not
	// call the inherited FindCommandStatus, thereby suppressing
	// commands that are handled by SuperCommanders. Only those
	// commands enabled by SubCommanders will be active.
	//
	// This is usually what you want for a movable modal dialog.
	// Commands such as "New", "Open" and "Quit" that are handled
	// by the Application are disabled, but items within the dialog
	// can enable commands. For example, an edit field would enable
	// items in the "Edit" menu.
	
	// Disable all commands.
	outEnabled = false;
	
	if ( inCommand == cmd_About ) {
	
		// Enable the about command.
		outEnabled = true;

	}
}

/* Enable and disable the dialog's buttons based on the selection in the Edit Realms List */
void CEditRealmsDialog::SetButtonStatus() {
	STableCell selectedFavCell, selectedAllCell;
	LPushButton *theButton;
	TableIndexT numberRows, numberCols;
	LEditText *theField;
	Str255 fieldContents;
	
	selectedFavCell = mFavRealmsTable->GetFirstSelectedCell();
	mFavRealmsTable->GetTableSize(numberRows,numberCols);
	
	// enable/disable Remove button as appropriate
	if (((selectedFavCell.row == 0) && (selectedFavCell.col == 0)) || (numberRows <= 1)) {
		theButton = dynamic_cast<LPushButton *>(FindPaneByID(rEditRealmsRemoveButton));
		Assert_(theButton != NULL);
		theButton->Disable();
	} else {
		theButton = dynamic_cast<LPushButton *>(FindPaneByID(rEditRealmsRemoveButton));
		Assert_(theButton != NULL);		
		theButton->Enable();
	}
	
	// enable/disable Add button as appropriate
	selectedAllCell = mAllRealmsTable->GetFirstSelectedCell();

	if ((selectedAllCell.row == 0) && (selectedAllCell.col == 0)) {
		theButton = dynamic_cast<LPushButton *>(FindPaneByID(rEditRealmsAddButton));
		Assert_(theButton != NULL);
		theButton->Disable();
	} else {
		theButton = dynamic_cast<LPushButton *>(FindPaneByID(rEditRealmsAddButton));
		Assert_(theButton != NULL);
		theButton->Enable();
	}

	// enable/disable Add All button as appropriate
	theButton = dynamic_cast<LPushButton *>(FindPaneByID(rEditRealmsAddAllButton));
	Assert_(theButton != NULL);

	mAllRealmsTable->GetTableSize(numberRows,numberCols);
	if (numberRows > 0) {
		theButton->Enable();
	} else { 
		theButton->Disable();
	}

	// enble/disable Add Extra Realm button as appropriate
	theButton = dynamic_cast<LPushButton *>(FindPaneByID(rEditRealmsExtraRealmAddButton));
	Assert_(theButton != NULL);

	theField = dynamic_cast<LEditText *>(FindPaneByID(rEditRealmsExtraRealmField));
	Assert_(theField != NULL);
	
	theField->GetDescriptor(fieldContents);
	if (fieldContents[0] > 0) {
		theButton->Enable();
	} else {
		theButton->Disable();
	}
	
	if (theField->IsTarget() && theButton->IsEnabled())
		this->SetDefaultButton(rEditRealmsExtraRealmAddButton);
	else
		this->SetDefaultButton(rEditRealmsDoneButton);

}

/* Reads all available realms from the profile library and places them in "all realms" table */
void CEditRealmsDialog::FillAllRealmsTable()
{
	// get list of v5-only realms using profile library to add to list
	try {
		profile_t	profile;
		STableCell	foundCell;
		UProfileOutputString		name;
		UProfileOutputString		value;
		TableIndexT maxRows, maxCols;
		
		UKerberos5Context contextWrapper;

		krb5_error_code krb5err = krb5_get_profile (contextWrapper.Get(), &profile);
		
		// if there's no configuration file, just don't fill out column and continue
		// we can handle an empty list
		if (krb5err == KRB5_CONFIG_CANTOPEN) {
			return;
		} else if (krb5err != 0) {
			const char *errText = error_message(krb5err);
			CKrbErrorAlert *alrt = new CKrbErrorAlert("Error reading configuration file.",
			(char *)errText, "OK", NULL, true);
			alrt->DisplayErrorAlert();
			delete alrt;
			
			return;
		}
		
		/*
		Assert_(krb5err == 0);
		if (krb5err != 0)
			throw;
		*/
			
		UProfile profileWrapper5 (profile);
		
		UProfileInputList krb5RealmSection ("realms");
		
		UProfileIterator prof5Iterator  = profileWrapper5. NewIterator (krb5RealmSection, PROFILE_ITER_LIST_SECTION);
		
		while (prof5Iterator.Next (name, value)) {
			mAllRealmsTable->GetTableSize(maxRows, maxCols);
			if (!mAllRealmsTable->FindCellData(foundCell, name.Get(), strlen(name.Get())))  // don't add realm if already exists
				mAllRealmsTable->InsertRows(1, maxRows, name.Get(), strlen(name.Get()), true);
		}
		
		// get list of v4-only realms using profile library to add to list
		int krb4err = krb_get_profile (&profile);
		Assert_(krb4err == 0);
		if (krb4err != 0)
			throw;

		UProfile profileWrapper4(profile);
		
		UProfileInputList krb4RealmSection (REALMS_V4_PROF_REALMS_SECTION);
		
		UProfileIterator prof4Iterator = profileWrapper4. NewIterator (krb4RealmSection, PROFILE_ITER_LIST_SECTION);
		
		while (prof4Iterator.Next (name, value)) {
			mAllRealmsTable->GetTableSize(maxRows, maxCols);
			if (!mAllRealmsTable->FindCellData(foundCell, name.Get(), strlen(name.Get())))  // don't add realm if v5 already added it
				mAllRealmsTable->InsertRows(1, maxRows, name.Get(), strlen(name.Get()), true);
		}
	}
	catch (...) {
		SignalPStr_("\pUnexpected exception in FillAllRealmsTable() - realm list may be incomplete");
	}
	
}

/* Copy contents of prefs into the favorite realms table for display and editing. */
void CEditRealmsDialog::CopyPrefsToRealmsDialog()
{
	UInt32 numRealms;
	char *realmStr;
	unsigned short i;
	TableIndexT maxRows, maxCols;
	KLStatus klErr = klNoErr;
	Boolean prefsWarnFlag = false;
	
	// fill out realms table with contents of realms list in preferences
	numRealms = KLCountKerberosRealms();
	
	for (i = 0; i < numRealms; i++) {
		klErr = KLGetKerberosRealm(i, &realmStr);
		if (klErr == klPreferencesReadErr)
			prefsWarnFlag = true;
		else
			Assert_(klErr == klNoErr);
			
		if (klErr == klNoErr) {
			mFavRealmsTable->GetTableSize(maxRows, maxCols);
			mFavRealmsTable->InsertRows(1, maxRows, realmStr, strlen((char *)realmStr), true);
			
			klErr = KLDisposeString(realmStr);
			Assert_(klErr == klNoErr);
		}
	}

	// display dialog that prefs file is unreadable if necessary
	if (prefsWarnFlag) {
		CKrbErrorAlert *alrt = new CKrbErrorAlert("Error reading preferences.",
			"There was an error while trying to read the preferences.  An incomplete list may be displayed.", "OK", NULL, false);
		alrt->DisplayErrorAlert();
		delete alrt;
	}
}


/* Copy contents of realms table to the Login Library.  First blow away current contents of
   prefs realms list; this may not be ideal because the user may not have actually
   changed anything.
   It's also possible we might not want to let the user remove the default realm. */
void CEditRealmsDialog::CopyRealmsDialogToPrefs()
{
	char realmStr[256];
	char *defaultRealm = nil;
	unsigned short i;
	TableIndexT maxRows, maxCols;
	UInt32 ioDataSize;
	STableCell cellToFetch;
	KLStatus klErr = klNoErr;
	Boolean prefsWarnFlag = false;

	// get default realm to reset (because KLRemoveAllKerberosRealms will unset it)
	klErr = KLGetKerberosDefaultRealmByName(&defaultRealm);
	Assert_(klErr == klNoErr);			
	
	//clear out current realms in Login Lib
	klErr = KLRemoveAllKerberosRealms();
	if (klErr == klPreferencesWriteErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);			

	//copy realms table to Login Lib
	mFavRealmsTable->GetTableSize(maxRows, maxCols);

	for (i = 1; i <= maxRows; i++) {
		cellToFetch.row = i;
		cellToFetch.col = 1;
		
		ioDataSize = sizeof(realmStr);

		mFavRealmsTable->GetCellData(cellToFetch, realmStr, ioDataSize);
	
		//the string doesn't necessarily come null-terminated from the table, grr
		realmStr[ioDataSize] = '\0';
		
		klErr = KLInsertKerberosRealm(KLCountKerberosRealms(), realmStr);
		if (klErr == klPreferencesWriteErr)
			prefsWarnFlag = true;
		else
			Assert_(klErr == klNoErr);			
	}

	// reset default realm
	if (defaultRealm != nil) {
		klErr = KLSetKerberosDefaultRealmByName(defaultRealm);
		if (klErr == klPreferencesWriteErr)
			prefsWarnFlag = true;
		else if (klErr != klRealmDoesNotExistErr) // probably user deleted it and LoginLib will deal, so don't do display an error
			Assert_(klErr == klNoErr);			
			
		KLDisposeString(defaultRealm);
	}
	
	// display dialog that prefs file is unwritable if necessary
	if (prefsWarnFlag) {
		CKrbErrorAlert *alrt = new CKrbErrorAlert("Error saving preferences.",
			"There was an error while trying to save your preferences to disk.  Changes to your preferences will not be saved to disk, but may be retained in memory for now.", "OK", NULL, false);
		alrt->DisplayErrorAlert();
		delete alrt;
	}
}

#define SIZE_OF_REALM	256

/* Copy realms selected in the "all realms" list to "favorite realms" if they're not in
   in already. */
void CEditRealmsDialog::AddSingleRealm() {
	TableIndexT maxRows, maxCols;
	STableCell selectedCell, tempCell;
	UInt32 ioDataSize;
	char realmToAdd[SIZE_OF_REALM];
	
	selectedCell.row = selectedCell.col = 0;
	
	while ( mAllRealmsTable->GetNextSelectedCell(selectedCell) ) {
		ioDataSize = SIZE_OF_REALM;

		mAllRealmsTable->GetCellData(selectedCell, realmToAdd, ioDataSize);

		//the string doesn't necessarily come null-terminated from the table, grr
		realmToAdd[ioDataSize] = '\0';
		
		// only add if it's not in the table already
		if (!mFavRealmsTable->FindCellData(tempCell, realmToAdd, ioDataSize)) {
			mFavRealmsTable->GetTableSize(maxRows, maxCols);
			mFavRealmsTable->InsertRows(1, maxRows, realmToAdd, strlen(realmToAdd), true);
		}
	}
}

/* Copy all realms from the "all realms" list to the "favorite realms" list, if they're
   not in it already. */
void CEditRealmsDialog::AddAllRealms() {
	TableIndexT maxRows, maxCols;
	STableCell theCell, tempCell;
	UInt32 ioDataSize;
	char realmToAdd[SIZE_OF_REALM];
	
	theCell.row = theCell.col = 0;
	
	while ( mAllRealmsTable->GetNextCell(theCell) ) {
		ioDataSize = SIZE_OF_REALM;

		mAllRealmsTable->GetCellData(theCell, realmToAdd, ioDataSize);

		//the string doesn't necessarily come null-terminated from the table, grr
		realmToAdd[ioDataSize] = '\0';
		
		// only add if it's not in the table already
		if (!mFavRealmsTable->FindCellData(tempCell, realmToAdd, ioDataSize)) {
			mFavRealmsTable->GetTableSize(maxRows, maxCols);
			mFavRealmsTable->InsertRows(1, maxRows, realmToAdd, strlen(realmToAdd), true);
		}
	}
}

/* Add realm from the edit field that might not be in the realm list becuase of DNS. */
void CEditRealmsDialog::AddExtraRealm() {
	TableIndexT maxRows, maxCols;
	STableCell tempCell;
	UInt32 ioDataSize;
	Str255 realmToAdd;
	LEditText *theField;
	
	theField = dynamic_cast<LEditText *>(FindPaneByID(rEditRealmsExtraRealmField));
	Assert_(theField != NULL);
	
	theField->GetDescriptor(realmToAdd);
	ioDataSize = sizeof(realmToAdd);

	LString::PToCStr(realmToAdd);
	
	// only add if it's not in the table already
	if (!mFavRealmsTable->FindCellData(tempCell, realmToAdd, strlen((char *)realmToAdd)/*ioDataSize*/)) {
		mFavRealmsTable->GetTableSize(maxRows, maxCols);
		mFavRealmsTable->InsertRows(1, maxRows, realmToAdd, strlen((char *)realmToAdd), true);
	}
	
	theField->SetDescriptor("\p");
	
	Boolean result = LCommander::SwitchTarget(mFavRealmsTable);
	
	this->SetButtonStatus();
}

/* Remove realms selected in the favorites list */
void CEditRealmsDialog::RemoveSingleRealm() {
	STableCell sel, lastCell;
	TableIndexT maxRows, maxCols;

	sel.row = sel.col = 0;
	
	while ( mFavRealmsTable->GetNextSelectedCell(sel) ) {
		mFavRealmsTable->RemoveRows(1, sel.row, true);
		
		lastCell = sel;
		
		sel.row = sel.col = 0;
	}
	
	// restore a selection
	mFavRealmsTable->GetTableSize(maxRows, maxCols);

	if (lastCell.row > maxRows) {
		// the bottom cell was deleted
		lastCell.row = maxRows;
		mFavRealmsTable->SelectCell(lastCell);
	} else {
		// any cell other than the bottom
		mFavRealmsTable->SelectCell(lastCell);
	}
}
