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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/CKrbErrorAlert.cp,v 1.13 2002/04/09 19:45:52 smcguire Exp $ */

// ===========================================================================
//	CKrbErrorAlert.c
// ===========================================================================

#include "string.h"

#include "CKrbErrorAlert.h"
#include "CKerberosManagerApp.h"

#include <UModalDialogs.h>
#include <LPushButton.h>
#include <LStaticText.h>

PP_Using_Namespace_PowerPlant

// ===========================================================================
//		¥ (blank) constructor
// ===========================================================================
CKrbErrorAlert::CKrbErrorAlert() {

	}

// ===========================================================================
//		¥ Destructor
// ===========================================================================
CKrbErrorAlert::~CKrbErrorAlert() {

}

// ===========================================================================
//		¥ Full Constructor
// ===========================================================================
CKrbErrorAlert::CKrbErrorAlert(char *majorString, char *minorString, 
		char *buttonText, char *otherButtonText,  bool fatal)	{
		
		//at least these must be non-null
		Assert_(majorString != NULL);
		Assert_(buttonText != NULL);
		
		//copy parameters
		this->mFatalError = fatal;
		
		//copy strings
		strncpy((char *)mMajorString, majorString, 255);
		mMajorString[255] = '\0';
		LString::CToPStr((char *)mMajorString);
		
		if (minorString) {
			strncpy((char *)mMinorString, minorString, 255);
			mMinorString[255] = '\0';
			LString::CToPStr((char *)mMinorString);
		} else mMinorString[0] = 0; //truncate pstring
			
		strncpy((char *)mButtonText, buttonText, 255);
		mButtonText[255] = '\0';
		LString::CToPStr((char *)mButtonText);
		
		if (otherButtonText) {
			strncpy((char *)mOtherButtonText, otherButtonText, 255);
			mOtherButtonText[255] = '\0';
			LString::CToPStr((char *)mOtherButtonText);
		} else mOtherButtonText[0] = 0; //truncate pstring

}

// ===========================================================================
//		¥ DisplayErrorAlert()
// ===========================================================================
// display the alert and return which button was clicked;
MessageT CKrbErrorAlert::DisplayErrorAlert() {
	
	//use the coolio stack based dialog handler
	StDialogHandler errHandler(res_AlertID, LCommander::GetTopCommander());
	
	//read the dialog from the resource file
	LWindow *errDialog = errHandler.GetDialog();
	if (errDialog == NULL) return kErrorCreatingDialog; //return an error code
	
	//setup the dialog
	LStaticText * capt = dynamic_cast<LStaticText *>(errDialog->FindPaneByID(res_MajorTextID));
	Assert_(capt != NULL);
	capt->SetDescriptor(mMajorString);
	
	//set minor text
	capt = dynamic_cast<LStaticText *>(errDialog->FindPaneByID(res_MinorTextID));
	Assert_(capt != NULL);
	capt->SetDescriptor(mMinorString);
	
	//set button text
	LPushButton *btn = dynamic_cast<LPushButton *>(errDialog->FindPaneByID(res_DefaultButtonID));
	Assert_(btn != NULL);
	btn->SetDescriptor(mButtonText);
	
	//set other button text or hide it if no text was specified
	btn = dynamic_cast<LPushButton *>(errDialog->FindPaneByID(res_OptionalButtonID));
	Assert_(btn != NULL);
	if (mOtherButtonText[0] == 0)
		btn->Hide();
	else
		btn->SetDescriptor(mOtherButtonText);
	
	//do the dialog loop
	errDialog->Show();
	MessageT msg;
	do {
		msg = errHandler.DoDialog();
	} while (msg == 0);
	
	errDialog->Hide();
		
	if (mFatalError) {
		//get a pointer to the application
		CKerberosManagerApp *theApp = static_cast<CKerberosManagerApp *>(LCommander::GetTopCommander());
		Assert_(theApp != NULL);
		
		theApp->SetFatalFlag(true);
		
		theApp->ObeyCommand(cmd_Quit, NULL);
		
		//((LApplication *)(LCommander::GetTopCommander()))->SendAEQuit();
	}
	
	return msg;
	
}
