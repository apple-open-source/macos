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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/CKrbPrefsDialog.cp,v 1.63 2003/07/11 19:39:51 smcguire Exp $ */

// ===========================================================================
//	CKrbPrefsDialog.c
// ===========================================================================

#include <stdio.h>
#include <string.h>

#include <Kerberos/KerberosDebug.h>

#include <LPushButton.h>
#include <LWindow.h>
#include <LEditText.h>
#include <LStaticText.h>
#include <LStdControl.h>
#include <LSlider.h>
#include <LCheckBox.h>
#include <LPopupButton.h>
#include <LPopupGroupBox.h>
#include <LRadioButton.h>
#include <LRadioGroupView.h>
#include <LMultiPanelView.h>
#include <LTabsControl.h>

#if TARGET_RT_MAC_CFM
	#include <KerberosLogin/KerberosLogin.h>
	#include <TicketKeeper/TicketKeeper.h>
	#include <KerberosSupport/Utilities.h>
#else
	#include <Kerberos/Kerberos.h>
#endif

#include "CKerberosManagerApp.h"
#include "CKrbPrefsDialog.h"
#include "CEditRealmsDialog.h"
#include "CKrbErrorAlert.h"
#include "CCommanderView.h"

CKrbPrefsDialog::CKrbPrefsDialog(LStream *inStream) : LDialogBox(inStream) {
	CKerberosManagerApp *theApp;

	//get a pointer to the preferences
	theApp = static_cast<CKerberosManagerApp *>(LCommander::GetTopCommander());
	mKrbPrefsRef = theApp->GetKrbPreferencesRef();
	// should check for a failure here
	
	// add us as a listener to the prefs so we can get the prefs changed broadcast messages
	mKrbPrefsRef->AddListener(this);
	
	mOkayToClose = true;
}
	
	
CKrbPrefsDialog::~CKrbPrefsDialog() {
}
	
void CKrbPrefsDialog::FinishCreateSelf()  {
	
	// create correct set of tabs for 9 or Carbon, depending on which we're running on
	LMultiPanelView* theSystemMultiPanelView;
	LMultiPanelView* theTabsMultiPanelView;
	LView* theView;
	
	theSystemMultiPanelView = dynamic_cast<LMultiPanelView *>(FindPaneByID(rPrefsSystemMultiPanelView));
	Assert_(theSystemMultiPanelView != nil);
	
	theView = theSystemMultiPanelView->CreatePanel(kMacOSXPrefs);
	Assert_(theView != nil);

	theTabsMultiPanelView = dynamic_cast<LMultiPanelView *>(FindPaneByID(rPrefsTabsXMultiPanelView));
	Assert_(theTabsMultiPanelView != nil);
	
	theSystemMultiPanelView->SwitchToPanel(kMacOSXPrefs,false);

	// now create multipanelview that the tabs control
	theTabsMultiPanelView->CreateAllPanels();

	// Link the dialog to the controls.
	//UReanimator::LinkListenerToBroadcasters( this, this, rPrefsDialogWindow );
	this->LinkToBroadcasters(this);
	
	this->CopyPrefsToDialogSettings();
	
	this->SetControlStatus();
}

void
CKrbPrefsDialog::ListenToMessage(MessageT inMessage, void *ioParam)  {

	SDialogResponse *DResp;
	long lifetime;
	LStaticText *capt;
	LPopupButton *realmsPopup;
	LCheckBox *cb;
	
	KLStatus klErr = klNoErr;
	
	DResp = new SDialogResponse;
	//LWindow *theWindow = UDesktop::FetchTopModal();

	switch (inMessage) {
	
		case msg_OK:
			//set prefs with new info
			this->CopyDialogSettingsToPrefs();
			
			break;
		
		case msg_Cancel:
			//do nothing, StDialogHandler will exit
			break;
		
		case msg_PrefsTabControl:
                    {
			LMultiPanelView* theTabsMultiPanelView;
			
			theTabsMultiPanelView = dynamic_cast<LMultiPanelView *>(FindPaneByID(rPrefsTabsXMultiPanelView));
			Assert_(theTabsMultiPanelView != nil);

			CCommanderView *currentPanel = dynamic_cast<CCommanderView *>(theTabsMultiPanelView->GetCurrentPanel());
			Assert_(currentPanel != nil);
		
			/* Restore the target of this panel.  This is important to make sure the edit fields
			   are selected, for instance.  However some things send the message back to us, which
			   can cause odd behavior, so only do it if it hasn't been done already. */
			if (!currentPanel->IsOnDuty())
				currentPanel->RestoreTarget();
			
			/* Sync the Login Options lifetime slider with settings from the Admin pane */
			PaneIDT currentPanelID = currentPanel->GetPaneID();
			
			if (currentPanel->GetPaneID() != rTicketLifetimeOptionsPanel) {
				if (!this->ValidateTimeRangeValues()) {
					LTabsControl *tabsControl = dynamic_cast<LTabsControl *>(FindPaneByID(rPrefsTabsControlForX));
					tabsControl->SetValue(kTicketLifetimeOptionsPanelForX);
				}
			}
			
			if (currentPanel->GetPaneID() == rTicketOptionsPanel) {
				LStr255 numberText;
				UInt32 minimumLifetime, maximumLifetime, defaultLifetime;
				SInt32 days, hours, minutes;
				LEditText *numberEditField = nil;
				LSlider *slid = nil;

				// get reference to ticket lifetime slider to use for following settings
				slid = dynamic_cast<LSlider *>(FindPaneByID(rDefaultLifetimeSlider));
				Assert_(slid != NULL);
				
				// read minimum ticket lifetime fields
				numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumTixLifetimeDays));
				Assert_(numberEditField != nil);
				numberEditField->GetDescriptor(numberText);
				days = numberText;
				
				numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumTixLifetimeHours));
				Assert_(numberEditField != nil);
				numberEditField->GetDescriptor(numberText);
				hours = numberText;
				
				numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumTixLifetimeMinutes));
				Assert_(numberEditField != nil);
				numberEditField->GetDescriptor(numberText);
				minutes = numberText;
				
				minimumLifetime = (UInt32)(days * 24 * 3600 + hours * 3600 + minutes * 60);

				// read maximum ticket lifetime fields
				numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumTixLifetimeDays));
				Assert_(numberEditField != nil);
				numberEditField->GetDescriptor(numberText);
				days = numberText;
				
				numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumTixLifetimeHours));
				Assert_(numberEditField != nil);
				numberEditField->GetDescriptor(numberText);
				hours = numberText;
				
				numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumTixLifetimeMinutes));
				Assert_(numberEditField != nil);
				numberEditField->GetDescriptor(numberText);
				minutes = numberText;
				
				maximumLifetime = (UInt32)(days * 24 * 3600 +hours * 3600 + minutes * 60);
	
				//set initial value of renewable time slider caption
				defaultLifetime = (UInt32)slid->GetValue();
				defaultLifetime *= mLifetimeSliderIncrement;

				this->SetSliderRangeAndIncrement(rDefaultLifetimeSlider, minimumLifetime, maximumLifetime, defaultLifetime, &mLifetimeSliderIncrement);

				defaultLifetime = (UInt32)slid->GetValue();
				this->SetSliderCaption(rDefaultLifetimeCaption, (SInt32)(defaultLifetime * mLifetimeSliderIncrement), nil, nil, nil, nil);

				// get reference to renewable lifetime slider to use for following settings
				slid = dynamic_cast<LSlider *>(FindPaneByID(rDefaultRenewableSlider));
				Assert_(slid != NULL);
				
				// read renewable ticket lifetime fields
				numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumRenewableLifetimeDays));
				Assert_(numberEditField != nil);
				numberEditField->GetDescriptor(numberText);
				days = numberText;
				
				numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumRenewableLifetimeHours));
				Assert_(numberEditField != nil);
				numberEditField->GetDescriptor(numberText);
				hours = numberText;
				
				numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumRenewableLifetimeMinutes));
				Assert_(numberEditField != nil);
				numberEditField->GetDescriptor(numberText);
				minutes = numberText;
				
				minimumLifetime = (UInt32)(days * 24 * 3600 + hours * 3600 + minutes * 60);

				// read renewable ticket lifetime fields
				numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumRenewableLifetimeDays));
				Assert_(numberEditField != nil);
				numberEditField->GetDescriptor(numberText);
				days = numberText;
				
				numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumRenewableLifetimeHours));
				Assert_(numberEditField != nil);
				numberEditField->GetDescriptor(numberText);
				hours = numberText;
				
				numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumRenewableLifetimeMinutes));
				Assert_(numberEditField != nil);
				numberEditField->GetDescriptor(numberText);
				minutes = numberText;
				
				maximumLifetime = (UInt32)(days * 24 * 3600 + hours * 3600 + minutes * 60);

				//set initial value of renewable time slider caption
				defaultLifetime = (UInt32)slid->GetValue();
				defaultLifetime *= mRenewableSliderIncrement;

				this->SetSliderRangeAndIncrement(rDefaultRenewableSlider, minimumLifetime, maximumLifetime, defaultLifetime, &mRenewableSliderIncrement);

				defaultLifetime = (UInt32)slid->GetValue();
				this->SetSliderCaption(rDefaultRenewableCaption, (SInt32)(defaultLifetime * mRenewableSliderIncrement), nil, nil, nil, nil);
			
			}
                        } // protect variables
			break;
			
		case msg_RememberPrincipalPopupBox:
			this->SetControlStatus();
			break;
			
		case msg_RememberOptionsPopupBox:
			this->SetControlStatus();
			break;
		
		case msg_DefaultUsernameEditText:
            {
                LRadioGroupView *radioGroup = dynamic_cast<LRadioGroupView *>(FindPaneByID(rPrefsRadioGroupView));
                Assert_(radioGroup != nil);
                
                radioGroup->SetCurrentRadioID(rDefaultUsernameRadio);
            }
			break;
			
		case msg_DefaultRealmsPopupMenu:
            {
                realmsPopup = dynamic_cast<LPopupButton *>(FindPaneByID(rDefaultRealmsPopupMenu));
                Assert_(realmsPopup != NULL);
                 
                if ( realmsPopup->GetCurrentMenuItem() == realmsPopup->GetMaxValue() ) {
                    ProcessCommand(cmd_EditRealmsList, this);
                    this->CopyRealmsListToPopupMenu();
                    
                    UInt32 realmResult;
                    klErr = KLGetKerberosDefaultRealm((KLIndex *)&realmResult);
                    
                    // if the default couldn't be found (probably because user removed it), just take the top one
                    if (klErr == klNoErr)
                         realmsPopup->SetCurrentMenuItem( (short) (realmResult + 1) ); // Mac menu 1-based, KLL realms 0-based
                    else
                         realmsPopup->SetCurrentMenuItem( 1 );
                }
			}
			break;
			
		case msg_DefaultLifetimeSlider:
			{
				UInt32 days, hours, minutes;
				
				// update the time caption while slider is tracking
				
				lifetime = *(long *)(ioParam);
				this->SetSliderCaption(rDefaultLifetimeCaption, (SInt32)(lifetime * mLifetimeSliderIncrement), &days, &hours, &minutes, nil);

				capt = dynamic_cast<LStaticText *>(FindPaneByID(rDefaultLifetimeCaption));
				Assert_(capt != NULL);
				
				//force caption to redraw now (otherwise PP waits until we let go of the mouse)
				Rect frame;
				capt->FocusDraw();
				capt->CalcLocalFrameRect(frame);
				capt->ApplyForeAndBackColors();
				::EraseRect(&frame);
				capt->Draw(nil);
				
			}
			break;

		case msg_DefaultRenewableSlider:
			{
				UInt32 days, hours, minutes, seconds;
				
				// update the time caption while slider is tracking
				
				lifetime = *(long *)(ioParam);
				this->SetSliderCaption(rDefaultRenewableCaption, (SInt32)(lifetime * mRenewableSliderIncrement), &days, &hours, &minutes, &seconds);

				capt = dynamic_cast<LStaticText *>(FindPaneByID(rDefaultRenewableCaption));
				Assert_(capt != NULL);
				
				//force caption to redraw now (otherwise PP waits until we let go of the mouse)
				Rect frame;
				capt->FocusDraw();
				capt->CalcLocalFrameRect(frame);
				capt->ApplyForeAndBackColors();
				::EraseRect(&frame);
				capt->Draw(nil);

			}
			break;

		case msg_PrefsDisplayTimeInDockChanged:
			cb = dynamic_cast<LCheckBox *>(FindPaneByID(rDisplayTimeInDockCheckbox));
			Assert_(cb != NULL);
			cb->SetValue( mKrbPrefsRef->GetDisplayTimeInDock() ) ;
			break;
			
		default:
			LDialogBox::ListenToMessage(inMessage, ioParam);
			break;
	}
}

void CKrbPrefsDialog::FindCommandStatus(
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

Boolean CKrbPrefsDialog::ObeyCommand(
	CommandT	inCommand,
	void		*ioParam)
{
	Boolean		cmdHandled = true;
	short menuID, menuItem;
	LPopupGroupBox *popupBox;
	
	//handle synthetic commands
	if (IsSyntheticCommand(inCommand, menuID, menuItem)) {
		cmdHandled = LWindow::ObeyCommand(inCommand, ioParam);
		return cmdHandled;
	}

	// handle commands sent to us by the dock menu command handler; couldn't figure out how to
	// do it more elegantly on short notice
	
	switch (inCommand) {
		case cmd_RememberPrincipalPopup1:
			popupBox = dynamic_cast<LPopupGroupBox *>(FindPaneByID(rRememberPrincipalPopupBox));
			Assert_(popupBox != NULL);
			popupBox->SetValue(1);
			this->SetControlStatus();
			break;
			
		case cmd_RememberPrincipalPopup2:
			popupBox = dynamic_cast<LPopupGroupBox *>(FindPaneByID(rRememberPrincipalPopupBox));
			Assert_(popupBox != NULL);
			popupBox->SetValue(2);
			this->SetControlStatus();
			break;
			
		case cmd_RememberOptionsPopup1:
			popupBox = dynamic_cast<LPopupGroupBox *>(FindPaneByID(rRememberOptionsPopupBox));
			Assert_(popupBox != NULL);
			popupBox->SetValue(1);
			this->SetControlStatus();
			break;
			
		case cmd_RememberOptionsPopup2:
			popupBox = dynamic_cast<LPopupGroupBox *>(FindPaneByID(rRememberOptionsPopupBox));
			Assert_(popupBox != NULL);
			popupBox->SetValue(2);
			this->SetControlStatus();
			break;
			
		default:
			cmdHandled = LWindow::ObeyCommand(inCommand, ioParam);
			break;

	}

	return cmdHandled;
}

/* Link all subpages to the broadcasters.  Needed to make sure all panels of the multipanelview
   can listen. */
void CKrbPrefsDialog::LinkToBroadcasters (
	LPane*		inPane)
{
	LView*		view = dynamic_cast <LView*> (inPane);
	if (view != nil) {
		TArray <LPane*>&	subPanes = view -> GetSubPanes ();
		TArrayIterator <LPane*>	iterator (subPanes);
		
		LPane* subPane = nil;
		
		while (iterator.Next (subPane)) {
			LinkToBroadcasters (subPane);
		}
	}
	
	LBroadcaster*	broadcaster = dynamic_cast <LBroadcaster*> (inPane);
	
	if (broadcaster != nil) {
		broadcaster -> AddListener (this);
	}
}

/* Say if it's okay for the dialog to close.  It might not be okay if illegal values have been
   entered in the preferences. */
Boolean CKrbPrefsDialog::AllowClose()
{
	return mOkayToClose;
}

/* Enable and disable controls as appropriate to pop-up menu settings */
void CKrbPrefsDialog::SetControlStatus()
{
	LPopupGroupBox *popupBox;
	LPane *thePane;
	
	SInt32 menuValue;

	// enable/disable things based on remember principal popup
	popupBox = dynamic_cast<LPopupGroupBox *>(FindPaneByID(rRememberPrincipalPopupBox));
	Assert_(popupBox != NULL);
	menuValue = popupBox->GetValue();
	
	if (menuValue == kPrefsRememberPrincipal) {
		thePane = FindPaneByID(rBlankUsernameRadio);
		Assert_(thePane != NULL);
		thePane->Disable();

		thePane = FindPaneByID(rDefaultUsernameRadio);
		Assert_(thePane != NULL);
		thePane->Disable();

		thePane = FindPaneByID(rDefaultUsernameEditText);
		Assert_(thePane != NULL);
		thePane->Deactivate();
		thePane->Disable();

		thePane = FindPaneByID(rDefaultRealmsCaption);
		Assert_(thePane != NULL);
		thePane->Disable();

		thePane = FindPaneByID(rDefaultRealmsPopupMenu);
		Assert_(thePane != NULL);
		thePane->Disable();

		thePane = FindPaneByID(rPrefsRadioGroupView);
		Assert_(thePane != NULL);
		thePane->Disable();
	} else {
		thePane = FindPaneByID(rPrefsRadioGroupView);
		Assert_(thePane != NULL);
		thePane->Enable();

		thePane = FindPaneByID(rBlankUsernameRadio);
		Assert_(thePane != NULL);
		thePane->Enable();

		thePane = FindPaneByID(rDefaultUsernameRadio);
		Assert_(thePane != NULL);
		thePane->Enable();

		LEditText *theEditText = dynamic_cast<LEditText *>(FindPaneByID(rDefaultUsernameEditText));
		Assert_(theEditText != NULL);
		theEditText->Enable();
		theEditText->Activate();
		LCommander::SwitchTarget(theEditText);

		thePane = FindPaneByID(rDefaultRealmsCaption);
		Assert_(thePane != NULL);
		thePane->Enable();

		thePane = FindPaneByID(rDefaultRealmsPopupMenu);
		Assert_(thePane != NULL);
		thePane->Enable();
	}

	// enable/disable things based on remember ticket options popup
	popupBox = dynamic_cast<LPopupGroupBox *>(FindPaneByID(rRememberOptionsPopupBox));
	Assert_(popupBox != NULL);
	menuValue = popupBox->GetValue();
	
	if (menuValue == kPrefsRememberOptions) {
		thePane = FindPaneByID(rDefaultForwardCheckbox);
		Assert_(thePane != NULL);
		thePane->Disable();

		thePane = FindPaneByID(rDefaultAddresslessCheckbox);
		Assert_(thePane != NULL);
		thePane->Disable();

		thePane = FindPaneByID(rDefaultProxiableCheckbox);
		Assert_(thePane != NULL);
		thePane->Disable();

		thePane = FindPaneByID(rKerb5OptionsGroupbox);
		Assert_(thePane != NULL);
		thePane->Disable();

		thePane = FindPaneByID(rDefaultSliderCaption);
		Assert_(thePane != NULL);
		thePane->Disable();

		thePane = FindPaneByID(rDefaultLifetimeCaption);
		Assert_(thePane != NULL);
		thePane->Disable();

		thePane = FindPaneByID(rDefaultLifetimeSlider);
		Assert_(thePane != NULL);
		thePane->Disable();

		thePane = FindPaneByID(rDefaultRenewableCheckbox);
		Assert_(thePane != NULL);
		thePane->Disable();

		thePane = FindPaneByID(rDefaultRenewableCaption);
		Assert_(thePane != NULL);
		thePane->Disable();

		thePane = FindPaneByID(rDefaultRenewableSlider);
		Assert_(thePane != NULL);
		thePane->Disable();

	} else {
		thePane = FindPaneByID(rDefaultForwardCheckbox);
		Assert_(thePane != NULL);
		thePane->Enable();

		thePane = FindPaneByID(rDefaultAddresslessCheckbox);
		Assert_(thePane != NULL);
		thePane->Enable();

		thePane = FindPaneByID(rDefaultProxiableCheckbox);
		Assert_(thePane != NULL);
		thePane->Enable();

		thePane = FindPaneByID(rKerb5OptionsGroupbox);
		Assert_(thePane != NULL);
		thePane->Enable();

		thePane = FindPaneByID(rDefaultSliderCaption);
		Assert_(thePane != NULL);
		thePane->Enable();

		thePane = FindPaneByID(rDefaultLifetimeCaption);
		Assert_(thePane != NULL);
		thePane->Enable();

		thePane = FindPaneByID(rDefaultLifetimeSlider);
		Assert_(thePane != NULL);
		thePane->Enable();

		thePane = FindPaneByID(rDefaultRenewableCheckbox);
		Assert_(thePane != NULL);
		thePane->Enable();

		thePane = FindPaneByID(rDefaultRenewableCaption);
		Assert_(thePane != NULL);
		thePane->Enable();

		thePane = FindPaneByID(rDefaultRenewableSlider);
		Assert_(thePane != NULL);
		thePane->Enable();
	}
}

// Routine to set all the dialog controls to values that reflect the current preference values
void CKrbPrefsDialog::CopyPrefsToDialogSettings()
{
	SInt32 lifetime;
	LCheckBox *cb;
	LPopupGroupBox *popupBox;
	LRadioGroupView *radioGroup;
	LSlider *lifetimeSlider, *renewSlider;
	char *defaultUsername = nil;
	
	KLStatus klErr;
	UInt32 klBufferSize;
	Boolean klBooleanOption;
	Boolean prefsWarnFlag = false;

	//---- USERNAME DEFAULT OPTIONS
	//set initial values of the "remember principal" popup menu
	klBufferSize = sizeof(Boolean);
	klErr = KLGetDefaultLoginOption(loginOption_RememberPrincipal, (void *)&klBooleanOption, (KLSize *)&klBufferSize);
	if (klErr == klPreferencesReadErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);
	
	popupBox = dynamic_cast<LPopupGroupBox *>(FindPaneByID(rRememberPrincipalPopupBox));
	Assert_(popupBox != NULL);
	
	if (klBooleanOption)
		popupBox->SetValue( kPrefsRememberPrincipal );
	else
		popupBox->SetValue( kPrefsDefaultPrincipal );
	
	// set values of principal to remember and radio group
	radioGroup = dynamic_cast<LRadioGroupView *>(FindPaneByID(rPrefsRadioGroupView));
	Assert_(radioGroup != nil);
	
	// get size of loginName
	klErr = KLGetDefaultLoginOption(loginOption_LoginName, nil, (KLSize *)&klBufferSize);
	if (klErr == klPreferencesReadErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);

	if (klBufferSize >= 1) {
		defaultUsername = (char *)malloc(klBufferSize + 1);
		Assert_(defaultUsername != nil);
			
		klErr = KLGetDefaultLoginOption(loginOption_LoginName, defaultUsername, (KLSize *)&klBufferSize);
		if (klErr == klPreferencesReadErr)
			prefsWarnFlag = true;
		else
			Assert_(klErr == klNoErr);


		defaultUsername[klBufferSize] = '\0';
		
		LEditText *defaultUsernameField = dynamic_cast<LEditText *>(FindPaneByID(rDefaultUsernameEditText));
		Assert_(defaultUsernameField != nil);
		
		LString::CToPStr(defaultUsername);
		defaultUsernameField->SetDescriptor((ConstStringPtr)defaultUsername);
		
		radioGroup->SetCurrentRadioID(rDefaultUsernameRadio);

		if (defaultUsername != nil)
			free(defaultUsername);
	
	} else {
		radioGroup->SetCurrentRadioID(rBlankUsernameRadio);
	}

	//---- TICKET DEFAULT OPTIONS
	//set initial values of the "remember options" popup menu
	klBufferSize = sizeof(Boolean);
	klErr = KLGetDefaultLoginOption(loginOption_RememberExtras, (void *)&klBooleanOption, (KLSize *)&klBufferSize);
	if (klErr == klPreferencesReadErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);

	popupBox = dynamic_cast<LPopupGroupBox *>(FindPaneByID(rRememberOptionsPopupBox));
	Assert_(popupBox != NULL);
	
	if (klBooleanOption)
		popupBox->SetValue( kPrefsRememberOptions );
	else
		popupBox->SetValue( kPrefsDefaultOptions );
	
	//set initial values of the "forwardable tickets" checkbox
	klBufferSize = sizeof(Boolean);
	klErr = KLGetDefaultLoginOption(loginOption_DefaultForwardableTicket, (void *)&klBooleanOption, (KLSize *)&klBufferSize);
	if (klErr == klPreferencesReadErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);

	cb = dynamic_cast<LCheckBox *>(FindPaneByID(rDefaultForwardCheckbox));
	Assert_(cb != NULL);
	cb->SetValue( klBooleanOption ) ;
	
	//set initial values of the "addressless tickets" checkbox
	klBufferSize = sizeof(Boolean);
	klErr = KLGetDefaultLoginOption(loginOption_DefaultAddresslessTicket, (void *)&klBooleanOption, (KLSize *)&klBufferSize);
	if (klErr == klPreferencesReadErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);

	cb = dynamic_cast<LCheckBox *>(FindPaneByID(rDefaultAddresslessCheckbox));
	Assert_(cb != NULL);
	cb->SetValue( klBooleanOption ) ;
	
	//set initial values of the "proxiable tickets" checkbox
	klBufferSize = sizeof(Boolean);
	klErr = KLGetDefaultLoginOption(loginOption_DefaultProxiableTicket, (void *)&klBooleanOption, (KLSize *)&klBufferSize);
	if (klErr == klPreferencesReadErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);

	cb = dynamic_cast<LCheckBox *>(FindPaneByID(rDefaultProxiableCheckbox));
	Assert_(cb != NULL);
	cb->SetValue( klBooleanOption ) ;
	
	//set initial values of the "renewable tickets" checkbox
	klBufferSize = sizeof(Boolean);
	klErr = KLGetDefaultLoginOption(loginOption_DefaultRenewableTicket, (void *)&klBooleanOption, (KLSize *)&klBufferSize);
	if (klErr == klPreferencesReadErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);

	cb = dynamic_cast<LCheckBox *>(FindPaneByID(rDefaultRenewableCheckbox));
	Assert_(cb != NULL);
	cb->SetValue( klBooleanOption ) ;
	
	//set values of the ticket lifetime slider
	UInt32 currMinimum, currMaximum, currDefault;
	
	// get reference to renewable time slider to use for following settings
	renewSlider = dynamic_cast<LSlider *>(FindPaneByID(rDefaultRenewableSlider));
	Assert_(renewSlider != NULL);

	//set minimum value of the renewable time slider
	klBufferSize = sizeof(UInt32);
	klErr = KLGetDefaultLoginOption(loginOption_MinimalRenewableLifetime, (void *)&currMinimum, (KLSize *)&klBufferSize);
	if (klErr == klPreferencesReadErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);

	//set maximum value of the renewable time slider
	klBufferSize = sizeof(UInt32);
	klErr = KLGetDefaultLoginOption(loginOption_MaximalRenewableLifetime, (void *)&currMaximum, (KLSize *)&klBufferSize);
	if (klErr == klPreferencesReadErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);

	//set default initial value of the renewable time slider
	klBufferSize = sizeof(UInt32);
	klErr = KLGetDefaultLoginOption(loginOption_DefaultRenewableLifetime, (void *)&currDefault, (KLSize *)&klBufferSize);
	if (klErr == klPreferencesReadErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);

	this->SetSliderRangeAndIncrement(rDefaultRenewableSlider, currMinimum, currMaximum, currDefault, &mRenewableSliderIncrement);
	
	//set initial value of renewable time slider caption
	lifetime = renewSlider->GetValue();
	this->SetSliderCaption(rDefaultRenewableCaption, (SInt32)(lifetime * mRenewableSliderIncrement), nil, nil, nil, nil);

	// get reference to ticket lifetime slider to use for following settings
	lifetimeSlider = dynamic_cast<LSlider *>(FindPaneByID(rDefaultLifetimeSlider));
	Assert_(lifetimeSlider != NULL);

	klBufferSize = sizeof(UInt32);
	klErr = KLGetDefaultLoginOption(loginOption_MinimalTicketLifetime, (void *)&currMinimum, (KLSize *)&klBufferSize);
	if (klErr == klPreferencesReadErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);
	
	klBufferSize = sizeof(UInt32);
	klErr = KLGetDefaultLoginOption(loginOption_MaximalTicketLifetime, (void *)&currMaximum, (KLSize *)&klBufferSize);
	if (klErr == klPreferencesReadErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);
	
	klBufferSize = sizeof(UInt32);
	klErr = KLGetDefaultLoginOption(loginOption_DefaultTicketLifetime, (void *)&currDefault, (KLSize *)&klBufferSize);
	if (klErr == klPreferencesReadErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);
	
	this->SetSliderRangeAndIncrement(rDefaultLifetimeSlider, currMinimum, currMaximum, currDefault, &mLifetimeSliderIncrement);
	
	//set initial value of slider caption
	lifetime = lifetimeSlider->GetValue();
	this->SetSliderCaption(rDefaultLifetimeCaption, (SInt32)(lifetime * mLifetimeSliderIncrement), nil, nil, nil, nil);
	
	// fill out realms menu
	this->CopyRealmsListToPopupMenu();
	
	//---- KERBEROS.APP OPTIONS
	//set initial value of the "always expand tickets list" checkbox
	cb = dynamic_cast<LCheckBox *>(FindPaneByID(rAlwaysExpandTicketListCheckbox));
	Assert_(cb != NULL);
	cb->SetValue( mKrbPrefsRef->GetAlwaysExpandTicketList() ) ;

	//set initial value of the "display time remaining in dock" checkbox
	cb = dynamic_cast<LCheckBox *>(FindPaneByID(rDisplayTimeInDockCheckbox));
	Assert_(cb != NULL);
	cb->SetValue( mKrbPrefsRef->GetDisplayTimeInDock() ) ;
	
	//set initial value of the "auto-renew tickets" checkbox
	cb = dynamic_cast<LCheckBox *>(FindPaneByID(rAutoRenewTicketsCheckbox));
	Assert_(cb != NULL);
	cb->SetValue( mKrbPrefsRef->GetAutoRenewTicketsPref() ) ;
	
	//set initial value of the "ticket list startup state" radio group
	radioGroup = dynamic_cast<LRadioGroupView *>(FindPaneByID(rTicketListStartupRadioGroupView));
	Assert_(radioGroup != nil);
	
	UInt16 currentTicketListStartupStatePref = mKrbPrefsRef->GetTicketListStartupDisplayOption();
	if (currentTicketListStartupStatePref == kAlwaysShowTicketListAtStartup)
		radioGroup->SetCurrentRadioID(rAlwaysShowTicketListAtStartupRadio);
	else if (currentTicketListStartupStatePref == kNeverShowTicketListAtStartup)
		radioGroup->SetCurrentRadioID(rNeverShowTicketListAtStartupRadio);
	else
		radioGroup->SetCurrentRadioID(rRememberTicketListQuitStateRadio);
	

	//---- TICKET LIFETIME OPTIONS
	UInt32 days, hours, minutes;
	LStr255 lifetimeString;
	LEditText *numberEditField = nil;

	//set initial values of the "minimum lilfetime" edit fields
	klBufferSize = sizeof(UInt32);
	klErr = KLGetDefaultLoginOption(loginOption_MinimalTicketLifetime, (void *)&lifetime, (KLSize *)&klBufferSize);
	if (klErr == klPreferencesReadErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);

	days = (UInt32)(lifetime / (24 * 3600));
	hours = (UInt32)((lifetime % (24 * 3600)) / 3600);
	minutes = (UInt32)(lifetime / 60 % 60);
	
	lifetimeString.Assign((SInt32)days);
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumTixLifetimeDays));
	Assert_(numberEditField != nil);
	numberEditField->SetDescriptor(lifetimeString);
	
	if (hours < 10) {
		lifetimeString = "0";
		lifetimeString += (SInt32)hours;
	} else {
		lifetimeString.Assign((SInt32)hours);
	}
	
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumTixLifetimeHours));
	Assert_(numberEditField != nil);
	numberEditField->SetDescriptor(lifetimeString);
	
	if (minutes < 10) {
		lifetimeString = "0";
		lifetimeString += (SInt32)minutes;
	} else
		lifetimeString.Assign((SInt32)minutes);
		
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumTixLifetimeMinutes));
	Assert_(numberEditField != nil);
	numberEditField->SetDescriptor(lifetimeString);
	
	//set initial values of the "maximum lilfetime" edit fields
	klBufferSize = sizeof(UInt32);
	klErr = KLGetDefaultLoginOption(loginOption_MaximalTicketLifetime, (void *)&lifetime, (KLSize *)&klBufferSize);
	if (klErr == klPreferencesReadErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);

	days = (UInt32)(lifetime / (24 * 3600));
	hours = (UInt32)((lifetime % (24 * 3600)) / 3600);
	minutes = (UInt32)(lifetime / 60 % 60);
	
	lifetimeString.Assign((SInt32)days);
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumTixLifetimeDays));
	Assert_(numberEditField != nil);
	numberEditField->SetDescriptor(lifetimeString);
	
	if (hours < 10) {
		lifetimeString = "0";
		lifetimeString += (SInt32)hours;
	} else
		lifetimeString.Assign((SInt32)hours);
		
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumTixLifetimeHours));
	Assert_(numberEditField != nil);
	numberEditField->SetDescriptor(lifetimeString);
	
	if (minutes < 10) {
		lifetimeString = "0";
		lifetimeString += (SInt32)minutes;
	} else
		lifetimeString.Assign((SInt32)minutes);
		
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumTixLifetimeMinutes));
	Assert_(numberEditField != nil);
	numberEditField->SetDescriptor(lifetimeString);
		
	//---- RENEWABLE TIME RANGES
	//set initial values of the "minimum renewable lifetime" edit fields
	klBufferSize = sizeof(UInt32);
	klErr = KLGetDefaultLoginOption(loginOption_MinimalRenewableLifetime, (void *)&lifetime, (KLSize *)&klBufferSize);
	if (klErr == klPreferencesReadErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);

	days = (UInt32)(lifetime / (24 * 3600));
	hours = (UInt32)((lifetime % (24 * 3600)) / 3600);
	minutes = (UInt32)(lifetime / 60 % 60);
	
	lifetimeString.Assign((SInt32)days);
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumRenewableLifetimeDays));
	Assert_(numberEditField != nil);
	numberEditField->SetDescriptor(lifetimeString);
	
	if (hours < 10) {
		lifetimeString = "0";
		lifetimeString += (SInt32)hours;
	} else
		lifetimeString.Assign((SInt32)hours);
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumRenewableLifetimeHours));
	Assert_(numberEditField != nil);
	numberEditField->SetDescriptor(lifetimeString);
	
	if (minutes < 10) {
		lifetimeString = "0";
		lifetimeString += (SInt32)minutes;
	} else
		lifetimeString.Assign((SInt32)minutes);
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumRenewableLifetimeMinutes));
	Assert_(numberEditField != nil);
	numberEditField->SetDescriptor(lifetimeString);
	
	//set initial values of the "maximum renewable lifetime" edit fields
	klBufferSize = sizeof(UInt32);
	klErr = KLGetDefaultLoginOption(loginOption_MaximalRenewableLifetime, (void *)&lifetime, (KLSize *)&klBufferSize);
	if (klErr == klPreferencesReadErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);

	days = (UInt32)(lifetime / (24 * 3600));
	hours = (UInt32)((lifetime % (24 * 3600)) / 3600);
	minutes = (UInt32)(lifetime / 60 % 60);
	
	lifetimeString.Assign((SInt32)days);
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumRenewableLifetimeDays));
	Assert_(numberEditField != nil);
	numberEditField->SetDescriptor(lifetimeString);
	
	if (hours < 10) {
		lifetimeString = "0";
		lifetimeString += (SInt32)hours;
	} else
		lifetimeString.Assign((SInt32)hours);
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumRenewableLifetimeHours));
	Assert_(numberEditField != nil);
	numberEditField->SetDescriptor(lifetimeString);
	
	if (minutes < 10) {
		lifetimeString = "0";
		lifetimeString += (SInt32)minutes;
	} else
		lifetimeString.Assign((SInt32)minutes);
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumRenewableLifetimeMinutes));
	Assert_(numberEditField != nil);
	numberEditField->SetDescriptor(lifetimeString);
	
	//---- CLEAN UP
	// display dialog that prefs file is unwritable if necessary
	if (prefsWarnFlag) {
		CKrbErrorAlert *alrt = new CKrbErrorAlert("Error reading preferences.",
			"There was an error while trying to read the preferences.  Inaccurate values may be displayed.", "OK", NULL, false);
		alrt->DisplayErrorAlert();
		delete alrt;
	}
}

void CKrbPrefsDialog::CopyRealmsListToPopupMenu()
{
	LPopupButton *realmsPopup;
	char *realmString = nil;
	UInt32 i;
	KLStatus klErr = klNoErr;

	// find popup menu
	realmsPopup = dynamic_cast<LPopupButton *>(FindPaneByID(rDefaultRealmsPopupMenu));
	Assert_(realmsPopup != NULL);
	
	// turn off broadcasting temporarily while we add stuff
	realmsPopup->StopBroadcasting();
	
	// remove list currently in menu (if any)
	SInt32 numItems = realmsPopup->GetMaxValue();
	
	for (i = (UInt32)(numItems - 2); i > 0; i--) // leave space for divider and "Edit Realms"
		realmsPopup->DeleteMenuItem(1);
	
	// add list to menu
	UInt32 numRealms = KLCountKerberosRealms();
	
	for (i = numRealms - 1; (numRealms > i) && (i >= 0); i--) {
		klErr = KLGetKerberosRealm(i, &realmString);
		Assert_(klErr == klNoErr);
		
		// make a copy of the realmString so we can convert it to a Pascal string
		size_t tempLen = strlen(realmString);
		char *tempRealm = (char *)malloc(tempLen + 1);
		if (tempRealm != nil) {
			strcpy(tempRealm, realmString);
			LString::CToPStr(tempRealm);
			
			realmsPopup->InsertMenuItem((unsigned char *)tempRealm, 0, true);
			
			free(tempRealm);
		}
		
		if (realmString != nil) {
			klErr = KLDisposeString(realmString);
			Assert_(klErr == klNoErr);
		}
	}
	
	UInt32 realmResult;
	klErr = KLGetKerberosDefaultRealm((KLIndex *)&realmResult);
	
	// if the default couldn't be found (probably because user removed it), just take the top one
	// add one because Login Library realms are 0-based
	if (klErr == klNoErr)
		realmsPopup->SetCurrentMenuItem( (short) (realmResult+1) );
	else
		realmsPopup->SetCurrentMenuItem( 1 );
	
	// okay, we're done, broadcast again
	realmsPopup->StartBroadcasting();
}

// Set the preference values to match the settings of dialog controls
void CKrbPrefsDialog::CopyDialogSettingsToPrefs()
{
	LCheckBox *cb;
	LRadioGroupView *radioGroup;
	LPopupGroupBox *popupBox;
	LSlider *slid;
	LPopupButton *realmsPopup;
	Str255 realmStr;
	KLStatus klErr = klNoErr;

	Boolean klBooleanOption;
	UInt32 klUInt32Option;
	SInt32 controlValue;
	Str255 defaultUsername;
	Boolean prefsWarnFlag = false;

	// start by assuming it's okay to close
	mOkayToClose = true;
	
	// USERNAME DEFAULT OPTIONS
	//get "remember principal" pref
	popupBox = dynamic_cast<LPopupGroupBox *>(FindPaneByID(rRememberPrincipalPopupBox));
	Assert_(popupBox != NULL);
	controlValue = popupBox->GetValue();
	
	klBooleanOption = (controlValue == kPrefsRememberPrincipal);

	klErr = KLSetDefaultLoginOption(loginOption_RememberPrincipal, &klBooleanOption, sizeof(klBooleanOption));
	if (klErr == klPreferencesWriteErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);
	
	//set "always use" blank or default username pref and default realm, if necessary
	if (controlValue != kPrefsRememberPrincipal) {
		radioGroup = dynamic_cast<LRadioGroupView *>(FindPaneByID(rPrefsRadioGroupView));
		Assert_(radioGroup != nil);
		PaneIDT currentRadio = radioGroup->GetCurrentRadioID();
		
		if (currentRadio == rBlankUsernameRadio) {
			LString::CopyPStr("\p", defaultUsername, sizeof(defaultUsername));
		} else {
			LEditText *defaultUsernameField = dynamic_cast<LEditText *>(FindPaneByID(rDefaultUsernameEditText));
			Assert_(defaultUsernameField != nil);
			
			defaultUsernameField->GetDescriptor(defaultUsername);
		}
		
		LString::PToCStr(defaultUsername);
		klErr = KLSetDefaultLoginOption(loginOption_LoginName, defaultUsername, strlen((char *)defaultUsername));
		if (klErr == klPreferencesWriteErr)
			prefsWarnFlag = true;
		else
			Assert_(klErr == klNoErr);
		
		// always set login instance to empty because we don't provide a UI for editing it
		char blankInstance[1];
		strcpy(blankInstance,"\0");
		klErr = KLSetDefaultLoginOption(loginOption_LoginInstance, blankInstance, strlen((char *)blankInstance));
		if (klErr == klPreferencesWriteErr)
			prefsWarnFlag = true;
		else
			Assert_(klErr == klNoErr);
	
		//get default realm from pop-up
		realmsPopup = dynamic_cast<LPopupButton *>(FindPaneByID(rDefaultRealmsPopupMenu));
		Assert_(realmsPopup != NULL);
		realmsPopup->GetMenuItemText( realmsPopup->GetCurrentMenuItem(), realmStr );
		
		LString::PToCStr(realmStr);
		klErr = KLSetKerberosDefaultRealmByName( (char *)realmStr );
		if (klErr == klPreferencesWriteErr)
			prefsWarnFlag = true;
		else
			Assert_(klErr == klNoErr);
	}

	// TICKET DEFAULT OPTIONS
	//set "remember extras" pref
	popupBox = dynamic_cast<LPopupGroupBox *>(FindPaneByID(rRememberOptionsPopupBox));
	Assert_(popupBox != NULL);
	controlValue = popupBox->GetValue();
	
	klBooleanOption = (controlValue == kPrefsRememberOptions);

	klErr = KLSetDefaultLoginOption(loginOption_RememberExtras, &klBooleanOption, sizeof(klBooleanOption));
	if (klErr == klPreferencesWriteErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);
	
	//set "always use" ticket options, if necessary
	if (controlValue != kPrefsRememberOptions) {
		//set "forwardable tickets" pref
		cb = dynamic_cast<LCheckBox *>(FindPaneByID(rDefaultForwardCheckbox));
		Assert_(cb != NULL);
		klBooleanOption = (Boolean)cb->GetValue();
		
		klErr = KLSetDefaultLoginOption(loginOption_DefaultForwardableTicket, &klBooleanOption, sizeof(klBooleanOption));
		if (klErr == klPreferencesWriteErr)
			prefsWarnFlag = true;
		else
			Assert_(klErr == klNoErr);
		
		//set "addressless tickets" pref
		cb = dynamic_cast<LCheckBox *>(FindPaneByID(rDefaultAddresslessCheckbox));
		Assert_(cb != NULL);
		klBooleanOption = (Boolean)cb->GetValue();
		
		klErr = KLSetDefaultLoginOption(loginOption_DefaultAddresslessTicket, &klBooleanOption, sizeof(klBooleanOption));
		if (klErr == klPreferencesWriteErr)
			prefsWarnFlag = true;
		else
			Assert_(klErr == klNoErr);
		
		//set "proxiable tickets" pref
		cb = dynamic_cast<LCheckBox *>(FindPaneByID(rDefaultProxiableCheckbox));
		Assert_(cb != NULL);
		klBooleanOption = (Boolean)cb->GetValue();
		
		klErr = KLSetDefaultLoginOption(loginOption_DefaultProxiableTicket, &klBooleanOption, sizeof(klBooleanOption));
		if (klErr == klPreferencesWriteErr)
			prefsWarnFlag = true;
		else
			Assert_(klErr == klNoErr);
		
		//set "renewable tickets" pref
		cb = dynamic_cast<LCheckBox *>(FindPaneByID(rDefaultRenewableCheckbox));
		Assert_(cb != NULL);
		klBooleanOption = (Boolean)cb->GetValue();
		
		klErr = KLSetDefaultLoginOption(loginOption_DefaultRenewableTicket, &klBooleanOption, sizeof(klBooleanOption));
		if (klErr == klPreferencesWriteErr)
			prefsWarnFlag = true;
		else
			Assert_(klErr == klNoErr);
		
		//get default renewable time pref from slider
		slid = dynamic_cast<LSlider *>(FindPaneByID(rDefaultRenewableSlider));
		Assert_(slid != NULL);
		klUInt32Option = (UInt32)slid->GetValue();
		
		klUInt32Option *= mRenewableSliderIncrement;
		
		klErr = KLSetDefaultLoginOption(loginOption_DefaultRenewableLifetime, &klUInt32Option, sizeof(klUInt32Option));
		if (klErr == klPreferencesWriteErr)
			prefsWarnFlag = true;
		else
			Assert_(klErr == klNoErr);

		//get default ticket lifetime pref from slider
		slid = dynamic_cast<LSlider *>(FindPaneByID(rDefaultLifetimeSlider));
		Assert_(slid != NULL);
		klUInt32Option = (UInt32)slid->GetValue();
		
		klUInt32Option *= mLifetimeSliderIncrement;
		
		klErr = KLSetDefaultLoginOption(loginOption_DefaultTicketLifetime, &klUInt32Option, sizeof(klUInt32Option));
		if (klErr == klPreferencesWriteErr)
			prefsWarnFlag = true;
		else
			Assert_(klErr == klNoErr);

	}
	
	//---- KERBEROS.APP OPTIONS
	//set "always expand ticket list" pref
	cb = dynamic_cast<LCheckBox *>(FindPaneByID(rAlwaysExpandTicketListCheckbox));
	Assert_(cb != NULL);
	mKrbPrefsRef->SetAlwaysExpandTicketList( (Boolean)cb->GetValue() );
	
	//set "display time remaining in dock" pref
	cb = dynamic_cast<LCheckBox *>(FindPaneByID(rDisplayTimeInDockCheckbox));
	Assert_(cb != NULL);
	mKrbPrefsRef->SetDisplayTimeInDock( (Boolean)cb->GetValue() );
	
	//set "auto renew tickets" pref
	cb = dynamic_cast<LCheckBox *>(FindPaneByID(rAutoRenewTicketsCheckbox));
	Assert_(cb != NULL);
	mKrbPrefsRef->SetAutoRenewTicketsPref( (Boolean)cb->GetValue() );
	
	//set "ticket list startup state" pref
	radioGroup = dynamic_cast<LRadioGroupView *>(FindPaneByID(rTicketListStartupRadioGroupView));
	Assert_(radioGroup != nil);
	PaneIDT currentRadio = radioGroup->GetCurrentRadioID();
	UInt16 ticketListStartupSetting;
	
	if (currentRadio == rAlwaysShowTicketListAtStartupRadio)
		ticketListStartupSetting = kAlwaysShowTicketListAtStartup;
	else if (currentRadio == rNeverShowTicketListAtStartupRadio)
		ticketListStartupSetting = kNeverShowTicketListAtStartup;
	else
		ticketListStartupSetting = kRememberTicketListOpenState;
	
	mKrbPrefsRef->SetTicketListStartupDisplayOption(ticketListStartupSetting);
	
	//--- TICKET LIFETIME OPTIONS
	UInt32 minimumTixLifetime, maximumTixLifetime;
	LStr255 numberText;
	SInt32 days, hours, minutes;
	LEditText *numberEditField = nil;

	// read minimum ticket lifetime fields
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumTixLifetimeDays));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	days = numberText;
	
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumTixLifetimeHours));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	hours = numberText;
	
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumTixLifetimeMinutes));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	minutes = numberText;
	
	minimumTixLifetime = (UInt32)(days * 24 * 3600 + hours * 3600 + minutes * 60);
	
	// read maximum ticket lifetime fields
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumTixLifetimeDays));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	days = numberText;
	
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumTixLifetimeHours));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	hours = numberText;
	
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumTixLifetimeMinutes));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	minutes = numberText;
	
	maximumTixLifetime = (UInt32)(days * 24 * 3600 + hours * 3600 + minutes * 60);
	
	//--- RENEWABLE LIFETIME OPTIONS
	UInt32 minimumRenewLifetime, maximumRenewLifetime;

	// read minimum renewable lifetime fields
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumRenewableLifetimeDays));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	days = numberText;
	
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumRenewableLifetimeHours));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	hours = numberText;
	
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumRenewableLifetimeMinutes));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	minutes = numberText;
	
	minimumRenewLifetime = (UInt32)(days * 24 * 3600 + hours * 3600 + minutes * 60);
	
	// read maximum renewable lifetime fields
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumRenewableLifetimeDays));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	days = numberText;
	
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumRenewableLifetimeHours));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	hours = numberText;
	
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumRenewableLifetimeMinutes));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	minutes = numberText;
	
	maximumRenewLifetime = (UInt32)(days * 24 * 3600 + hours * 3600 + minutes * 60);
	
	mOkayToClose = this->ValidateTimeRangeValues();
		
	/* if there was a problem with the lifetime prefs, switch to the lifetime panel, and exit so we don't
	   save those values */
	if (!mOkayToClose) {
#if TARGET_API_MAC_CARBON
		LTabsControl *tabsControl = dynamic_cast<LTabsControl *>(FindPaneByID(rPrefsTabsControlForX));
		Assert_(tabsControl != nil);
		
		tabsControl->SetValue(kTicketLifetimeOptionsPanelForX);
#else
		LTabsControl *tabsControl = dynamic_cast<LTabsControl *>(FindPaneByID(rPrefsTabsControlFor9));
		Assert_(tabsControl != nil);
		
		tabsControl->SetValue(kTicketLifetimeOptionsPanelFor9);
#endif		
		return;
	}
	
	// write lifetime range preferences to Login Library
	klErr = KLSetDefaultLoginOption(loginOption_MinimalTicketLifetime, &minimumTixLifetime, sizeof(minimumTixLifetime));
	if (klErr == klPreferencesWriteErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);

	klErr = KLSetDefaultLoginOption(loginOption_MaximalTicketLifetime, &maximumTixLifetime, sizeof(maximumTixLifetime));
	if (klErr == klPreferencesWriteErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);

	klErr = KLSetDefaultLoginOption(loginOption_MinimalRenewableLifetime, &minimumRenewLifetime, sizeof(minimumRenewLifetime));
	if (klErr == klPreferencesWriteErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);

	klErr = KLSetDefaultLoginOption(loginOption_MaximalRenewableLifetime, &maximumRenewLifetime, sizeof(maximumRenewLifetime));
	if (klErr == klPreferencesWriteErr)
		prefsWarnFlag = true;
	else
		Assert_(klErr == klNoErr);

	// CLEAN UP
	
	// display dialog that prefs file is unwritable if necessary
	if (prefsWarnFlag) {
		CKrbErrorAlert *alrt = new CKrbErrorAlert("Error saving preferences.",
			"There was an error while trying to save your preferences to disk.  Changes to your preferences will not be saved to disk, but may be retained in memory for now.", "OK", NULL, false);
		alrt->DisplayErrorAlert();
		delete alrt;
	}
	
	mOkayToClose = true;
}

/* Check to make sure values in edit fields of time ranges make sense, display error and return flag if not */
Boolean CKrbPrefsDialog::ValidateTimeRangeValues()
{
	//--- TICKET LIFETIME OPTIONS
	UInt32 minimumTixLifetime, maximumTixLifetime;
	LStr255 numberText;
	SInt32 days, hours, minutes;
	LEditText *numberEditField = nil;
	Boolean valuesGood = true;

	// read minimum ticket lifetime fields
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumTixLifetimeDays));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	days = numberText;
	
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumTixLifetimeHours));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	hours = numberText;
	
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumTixLifetimeMinutes));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	minutes = numberText;
	
	minimumTixLifetime = (UInt32)(days * 24 * 3600 + hours * 3600 + minutes * 60);
	
	// read maximum ticket lifetime fields
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumTixLifetimeDays));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	days = numberText;
	
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumTixLifetimeHours));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	hours = numberText;
	
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumTixLifetimeMinutes));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	minutes = numberText;
	
	maximumTixLifetime = (UInt32)(days * 24 * 3600 + hours * 3600 + minutes * 60);
	
	// check that requested preferences make sense and that min is non-zero
	if (minimumTixLifetime == 0) {
		CKrbErrorAlert *alrt = new CKrbErrorAlert("Minimum ticket lifetime cannot be zero.",
			"The minimum lifetime cannot be zero.  Please try again.", "OK", NULL, false);
		alrt->DisplayErrorAlert();
		delete alrt;
		
		valuesGood = false;
	}
	
	if (minimumTixLifetime > maximumTixLifetime) {
		CKrbErrorAlert *alrt = new CKrbErrorAlert("Minimum ticket lifetime longer than maximum ticket lifetime.",
			"The minimum lifetime must be shorter than the maximum lifetime.  Please try again.", "OK", NULL, false);
		alrt->DisplayErrorAlert();
		delete alrt;
		
		valuesGood = false;
	}
	
	//--- RENEWABLE LIFETIME OPTIONS
	UInt32 minimumRenewLifetime, maximumRenewLifetime;

	// read minimum renewable lifetime fields
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumRenewableLifetimeDays));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	days = numberText;
	
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumRenewableLifetimeHours));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	hours = numberText;
	
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMinimumRenewableLifetimeMinutes));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	minutes = numberText;
	
	minimumRenewLifetime = (UInt32)(days * 24 * 3600 + hours * 3600 + minutes * 60);
	
	// read maximum renewable lifetime fields
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumRenewableLifetimeDays));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	days = numberText;
	
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumRenewableLifetimeHours));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	hours = numberText;
	
	numberEditField = dynamic_cast<LEditText *>(FindPaneByID(rMaximumRenewableLifetimeMinutes));
	Assert_(numberEditField != nil);
	numberEditField->GetDescriptor(numberText);
	minutes = numberText;
	
	maximumRenewLifetime = (UInt32)(days * 24 * 3600 + hours * 3600 + minutes * 60);
	
	// check that requested renewable lifetimes make sense and that min is non-zero
	if (minimumRenewLifetime == 0) {
		CKrbErrorAlert *alrt = new CKrbErrorAlert("Minimum renewable lifetime cannot be zero.",
			"The minimum lifetime cannot be zero.  Please try again.", "OK", NULL, false);
		alrt->DisplayErrorAlert();
		delete alrt;
		
		valuesGood = false;
	}
	
	if (minimumRenewLifetime > maximumRenewLifetime) {
		CKrbErrorAlert *alrt = new CKrbErrorAlert("Minimum renewable lifetime longer than maximum renewable lifetime.",
			"The minimum lifetime must be shorter than the maximum lifetime.  Please try again.", "OK", NULL, false);
		alrt->DisplayErrorAlert();
		delete alrt;
		
		valuesGood = false;
	}
	
	return valuesGood;
}

void CKrbPrefsDialog::SetSliderCaption(ResIDT captionToSetID, SInt32 inLifetime, UInt32 *outDays,  UInt32 *outHours,
								 		UInt32 *outMinutes, UInt32 *outSeconds)
{
	SInt32 days, hours, minutes, seconds;
	LStaticText *capt;
	LStr255 timeString;
	
	capt = dynamic_cast<LStaticText *>(FindPaneByID(captionToSetID));
	Assert_(capt != NULL);

	days = inLifetime / (24 * 3600);
	hours = inLifetime / 3600 % 24;
	minutes = inLifetime / 60 % 60;
	seconds = inLifetime % 60;
	
	//strcpy(timeString, "\0");
	
	if ((days == 0) && (hours == 0) && (minutes == 0) && (seconds == 0)) {
		timeString = "None";
	} else {
		if (days > 0) {
			timeString += days;
			timeString += (days == 1 ? " day" : " days");
		}

		if (hours > 0) {
			if (timeString.Length() > 0)
				timeString += ", ";
			timeString += hours;
			timeString += (hours == 1 ? " hour" : " hours");
		}
		
		if (minutes > 0) {
			if (timeString.Length() > 0)
				timeString += ", ";
			timeString += minutes;
			timeString += (minutes == 1 ? " minute" : " minutes");
		}
		
		if (seconds > 0) {
			if (timeString.Length() > 0)
				timeString += ", ";
			timeString += seconds;
			timeString += (seconds == 1 ? " second" : " seconds");
		}
	}
	
	capt->SetDescriptor((StringPtr)timeString);

	if (outDays != nil)
		*outDays = (UInt32)days;
		
	if (outHours != nil)
		*outHours = (UInt32)hours;
		
	if (outMinutes != nil)
		*outMinutes = (UInt32)minutes;
		
	if (outSeconds != nil)
		*outSeconds = (UInt32)seconds;
		

}

void CKrbPrefsDialog::SetSliderRangeAndIncrement(ResIDT sliderToSetID, UInt32 inMinimum, UInt32 inMaximum,
												 UInt32 inDefaultValue, UInt32 *outIncrement)
{
	LSlider *slider;
    UInt32 minimum = inMinimum;
    UInt32 maximum = inMaximum;
    UInt32 increment = 0;

    if (maximum < minimum) {
        // swap values
        UInt32 temp = maximum;
        maximum = minimum;
        minimum = temp;
    }

    UInt32	range = maximum - minimum;

    if (range < 5*60)              { increment = 1;       // 1 second if under 5 minutes
    } else if (range < 30*60)      { increment = 5;       // 5 seconds if under 30 minutes
    } else if (range < 60*60)      { increment = 15;      // 15 seconds if under 1 hour
    } else if (range < 2*60*60)    { increment = 30;      // 30 seconds if under 2 hours
    } else if (range < 5*60*60)    { increment = 60;      // 1 minute if under 5 hours
    } else if (range < 50*60*60)   { increment = 5*60;    // 5 minutes if under 50 hours
    } else if (range < 200*60*60)  { increment = 15*60;   // 15 minutes if under 200 hours
    } else if (range < 500*60*60)  { increment = 30*60;   // 30 minutes if under 500 hours
    } else if (range < 1000*60*60) { increment = 60*60;   // 1 hour if under 1000 hours
    } else                         { increment = 60*60; } // 1 hour so it's readable

    UInt32 roundedMinimum = (minimum / increment) * increment;
    if (roundedMinimum > minimum) {
    	roundedMinimum -= increment;
    }
    if (roundedMinimum <= 0) {
    	roundedMinimum += increment;  // don't let it go below 1
    }

    UInt32 roundedMaximum = (maximum / increment) * increment;
    if (roundedMaximum < maximum) {
    	roundedMaximum += increment;
    }

    UInt32 roundedValue = (inDefaultValue / increment) * increment;
    
    if (roundedValue < roundedMinimum) {
    	roundedValue = roundedMinimum;
    }
    
    if (roundedValue > roundedMaximum) {
    	roundedValue = roundedMaximum;
    }

	// get reference to ticket lifetime slider to use for following settings
	slider = dynamic_cast<LSlider *>(FindPaneByID(sliderToSetID));
	Assert_(slider != NULL);

	slider->SetMinValue((SInt32)(roundedMinimum / increment));
	//slider->SetMinValue(1);

	slider->SetMaxValue((SInt32)(roundedMaximum / increment));
	//	slider->SetMaxValue((SInt32)((roundedMaximum - roundedMinimum) / increment + 1));
	
	slider->SetValue((SInt32)(roundedValue / increment));
//	slider->SetValue((SInt32)((roundedValue - roundedMinimum) / increment + 1));
	
	if (outIncrement != nil) {
		*outIncrement = increment;
	}
}

