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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/CKerberosManagerApp.cp,v 1.116 2003/08/22 16:05:56 smcguire Exp $ */

// ===========================================================================
//	CKerberosManagerApp.cp
// ===========================================================================
//
// started life as CPPStarterApp.cp.

/* Check version of PowerPlant (this needs at least version 2.1, which is in CW Pro 6 */
#ifdef __PowerPlant__
#	if (__PowerPlant__ >= 0x02108000) /* PP 2.1 */
#	else
#	error "You need at least PowerPlant version 2.1 to compile the Kerberos application."
#	endif
#endif /* PowerPlant */

/* Check version of Universal Headers (this needs at least version 3.4 pre-release */
#ifdef UNIVERSAL_INTERFACES_VERSION
#	if (UNIVERSAL_INTERFACES_VERSION >= 0x0335) /* Universal Headers 3.4 pre-release */
#	else
#	error "You need at least Universal Headers version 3.4 pre-release to compile the Kerberos application."
#	endif
#endif /* UNIVERSAL_INTERFACES_VERSION */

// ---------------------------------------------------------------------------
const	OSType	version_ResType		= 'vers';
const	SInt16	version_KM_ResID		= 1;
const	SInt16	version_KFM_ResID		= 2;


#include <string.h>
#include <new>

#if TARGET_RT_MAC_CFM
	#include "CLib.h"
	#include <KerberosSupport/Utilities.h>
	#include <KerberosSupport/Search.h>
	#include <TicketKeeper/TicketKeeper.h>

	#include <KerberosSupport/ShlibDriver.h>
	extern "C" {
	#include <CredentialsCache/CredentialsCacheInternal.h>
	}

	#include <KerberosLogin/KerberosLogin.h>
#else
	#include <sys/time.h>
	#include <MacApplication.h>
	#include <CoreFoundation/CoreFoundation.h>
	#include <Kerberos/Kerberos.h>
	#include "UCCache.h"
#endif

#include <PP_Messages.h>
#include <PP_Resources.h>
#include <PP_DebugMacros.h>
//#include <PPobClasses.h>

#include <LGrowZone.h>
#include <LGroupBox.h>
#include <LIconPane.h>
#include <LWindow.h>
#include <UDrawingState.h>
#include <UMemoryMgr.h>
#include <URegistrar.h>
#include <UControlRegistry.h>
#include <UGraphicUtils.h>
#include <UModalDialogs.h>
#include <UEnvironment.h>

#include <LView.h>
#include <LDialogBox.h>
#include <LTabGroup.h>
#include <LCaption.h>
#include <LCheckBox.h>
#include <LDisclosureTriangle.h>
#include <LEditText.h>
#include <LPushButton.h>
#include <LSeparatorLine.h>
#include <LSlider.h>
#include <LStdControl.h>
#include <LScroller.h>
#include <LTextGroupBox.h>
#include <LHierarchyTable.h>
#include <LPicture.h>
#include <LControlPane.h>
#include <LStaticText.h>
#include <LTextGroupBox.h>
#include <LPopupButton.h>
#include <LIconControl.h>
//#include <LGAIconControlImp.h>
#include <LPopupGroupBox.h>
#include <LRadioButton.h>
#include <LTabsControl.h>

#include <LScrollBar.h>
#include <LScrollerView.h>
#include <LTextColumn.h>
#include <LRadioGroupView.h>
#include <LMultiPanelView.h>

#include <LAMControlImp.h>
#include <LAMEditTextImp.h>
#include <LAMPushButtonImp.h>
#include <LAMTrackActionImp.h>
#include <LAMStaticTextImp.h>
#include <LAMPopupButtonImp.h>
#include <LAMPopupGroupBoxImp.h>
#include <LAMTextGroupBoxImp.h>

#ifndef __APPEARANCE__
#include <Appearance.h>
#endif

#include "CKerberosManagerApp.h"
#include "CKrbPrefsDialog.h"
#include "CEditRealmsDialog.h"
#include "CKrbErrorAlert.h"
#include "CTextColumnColorized.h"
#include "CStatusDialog.h"
#include "CTicketListPrincipalItem.h"
#include "CTicketListCredentialsItem.h"
#include "CTicketInfoWindow.h"
#include "CCommanderView.h"

// mini class to restore the dock icon to its normal state; declare a global variable of this
// class so the icon gets restored even if Kerberos app quits unexpectedly
#if TARGET_RT_MAC_MACHO
class CDockIconRestorer {
	public:
			 CDockIconRestorer() {};
 			~CDockIconRestorer() { RestoreApplicationDockTileImage(); };
};

CDockIconRestorer gDockIconRestorer;
#endif

// ===========================================================================
//		¥ Main Program
// ===========================================================================

int main(void)
{
									// Set Debugging options
	SetDebugThrow_(debugAction_Alert);
	SetDebugSignal_(debugAction_Alert);

	InitializeHeap(3);				// Initialize Memory Manager
									// Parameter is number of Master Pointer
									//   blocks to allocate
	
									// Initialize standard Toolbox managers
	UQDGlobals::InitializeToolbox();
	
									// Initialize PP environment internals
	UEnvironment::InitEnvironment();
	
	LGrowZone kmGrowZone(20000);	// Install a GrowZone function to catch
									//    low memory situations.

	CKerberosManagerApp	theApp;			// replace this with your App type
	theApp.Run();
	
	return 0;
}


// ---------------------------------------------------------------------------
//		¥ CKerberosManagerApp 			
// ---------------------------------------------------------------------------
//	Constructor

CKerberosManagerApp::CKerberosManagerApp()
{
	mKMMainWindow = nil;
	mKrbPreferences = nil;
	mKSession = nil;
	mFatalFlag = false;
	
	mDockMenuCommandHandler = nil;
	mDockMenuSetupHandler = nil;
	
	// increase our sleep time in WaitNextEvent to improve performance
	mSleepTime = 60;

	// turn on anti-aliasing of QD fonts
	QDSwapTextFlags(kQDUseCGTextRendering | kQDUseCGTextMetrics);
	
	this->RegisterTMClasses();
}

void CKerberosManagerApp::RegisterTMClasses()
{
	// Register functions to create core PowerPlant classes
	RegisterClass_(LView);
	RegisterClass_(LDialogBox);
	RegisterClass_(LTabGroup);
	RegisterClass_(LCaption);
	RegisterClass_(LGroupBox);
	RegisterClass_(LWindow);
	RegisterClass_(LStdControl);
	RegisterClass_(LStdCheckBox);
	RegisterClass_(LScroller);
	RegisterClass_(LHierarchyTable);
	RegisterClass_(LPicture);
	RegisterClass_(LScrollBar);
	RegisterClass_(LScrollerView);
	
	// Set up environment variables
	UEnvironment::InitEnvironment();
	
	//register the abstractions we use
	RegisterClass_(LWindowThemeAttachment);

	RegisterClass_(LTextColumn);
	RegisterClass_(LDisclosureTriangle);
	RegisterClass_(LEditText);
	RegisterClass_(LPushButton);
	RegisterClass_(LSeparatorLine);
	RegisterClass_(LSlider);
	RegisterClass_(LCheckBox);

	RegisterClass_(LControlPane);
	RegisterClass_(LStaticText);
	RegisterClass_(LTextGroupBox);
	RegisterClass_(LPopupButton);
	RegisterClass_(LIconControl);
	RegisterClass_(LIconPane);
	RegisterClass_(LPopupGroupBox);
	RegisterClass_(LRadioButton);
	RegisterClass_(LRadioGroupView);
	RegisterClass_(LMultiPanelView);
	RegisterClass_(LTabsControl);
	
	//we're assuming appearance, assert just in case
	Assert_(UEnvironment::HasFeature (env_HasAppearance));
	Assert_(UEnvironment::HasFeature (env_HasAppearance101));
	
	//these are the 'real' as in appearance manager controls
	RegisterClassID_(LAMControlImp,		LCheckBox::imp_class_ID);
	RegisterClassID_(LAMControlImp, 	LDisclosureTriangle::imp_class_ID);
	RegisterClassID_(LAMEditTextImp,	LEditText::imp_class_ID);
	RegisterClassID_(LAMPushButtonImp,	LPushButton::imp_class_ID);
	RegisterClassID_(LAMControlImp, 	LSeparatorLine::imp_class_ID);
	RegisterClassID_(LAMTrackActionImp, LSlider::imp_class_ID);
	RegisterClassID_(LAMStaticTextImp, 	LStaticText::imp_class_ID);
	RegisterClassID_(LAMTextGroupBoxImp, LTextGroupBox::imp_class_ID);		
	RegisterClassID_(LAMTrackActionImp,	LScrollBar::imp_class_ID);
	RegisterClassID_(LAMPopupButtonImp,	LPopupButton::imp_class_ID);
	RegisterClassID_(LAMControlImp,		LIconControl::imp_class_ID);
	RegisterClassID_(LAMPopupGroupBoxImp, LPopupGroupBox::imp_class_ID);
	RegisterClassID_(LAMControlImp,		LRadioButton::imp_class_ID);
	RegisterClassID_(LAMControlImp, LTabsControl::imp_class_ID);
	
	//register our custom classes
	RegisterClass_(CTextColumnColorized);
	RegisterClass_(CPrincList);
	RegisterClass_(CKMMainWindow);
	RegisterClass_(CEditRealmsDialog);
	RegisterClass_(CKrbPrefsDialog);
	RegisterClass_(CStatusDialog);
	RegisterClass_(CTicketInfoWindow);
	RegisterClass_(CCommanderView);
}

// ---------------------------------------------------------------------------
//		¥ ~CKerberosManagerApp			// replace this with your App type
// ---------------------------------------------------------------------------
//	Destructor
//

CKerberosManagerApp::~CKerberosManagerApp()
{
	if (mDockMenuCommandHandler != nil)
		::DisposeEventHandlerUPP(mDockMenuCommandHandler);
	if (mDockMenuSetupHandler != nil)
		::DisposeEventHandlerUPP(mDockMenuSetupHandler);

	if (mKrbPreferences != NULL)
		delete mKrbPreferences;
	if (mKSession != NULL)
		delete mKSession;

}

/* Override for LApplication::Run().  Almost identical to it, but we check to see
   if mState is programState_Quitting after the Initialize() before updating menus
   and setting mState = programState_ProcessingEvents .  Original PowerPlant code
   assumes you can't get a quit AE during initialization.  Oops. */

/* Check version of PowerPlant (most of this code is from PP 2.2 - if there's a newer version
   they may have fixed the problem and no override is necessary.  If it still is, update
   this version check in the code please. :) */
#ifdef __PowerPlant__
#if (__PowerPlant__ > 0x02218000) /* PP 2.2 */
	#error "Your PowerPlant version is newer than 2.2, you may not need to override LApplication::Run() anymore."
#endif
#endif /* PowerPlant */

void CKerberosManagerApp::Run()
{
	try {
		MakeMenuBar();
		MakeModelDirector();
		Initialize();

			ForceTargetSwitch(this);
			UCursor::InitTheCursor();
			UpdateMenus();

		if (mState != programState_Quitting) {
			mState = programState_ProcessingEvents;
		}
	}

	catch (...) {

		// Initialization failed. After signalling, the program
		// will terminate since mState will not have been
		// set to programState_ProcessingEvents.

		SignalStringLiteral_("App Initialization failed.");
	}

	while (mState == programState_ProcessingEvents) {
		try {
			ProcessNextEvent();
		}

			// You should catch all exceptions in your code.
			// If an exception reaches here, we'll signal
			// and continue running.

		catch (PP_STD::bad_alloc) {
			SignalStringLiteral_("bad_alloc (out of memory) exception caught "
								 "in LApplication::Run");
		}

		catch (const LException& inException) {
			CatchException(inException);
		}

		catch (ExceptionCode inError) {
			CatchExceptionCode(inError);
		}

		catch (...) {
			SignalStringLiteral_("Exception caught in LApplication::Run");
		}
	}
}

void CKerberosManagerApp::Initialize()
{
	SInt32 osVersion = UEnvironment::GetOSVersion();
	Boolean doStartupChecks = true;
	
	LMenuBar *menuBar = LMenuBar::GetCurrentMenuBar();
	ThrowIfNil_(menuBar);
	
	// *** OS version checks

	if (osVersion < 0x00001020) {
		::ParamText("\pThis version of the Kerberos application requires Mac OS X 10.2 or greater.", "\p", "\p", "\p");
		UModalAlerts::StopAlert(rGenericFatalAlert);
		((LApplication *)(LCommander::GetTopCommander()))->SendAEQuit();
		return;
	}

#if MACDEV_DEBUG
	// *** add Kerberos Manager Debug menu
	LMenu *debugMenu = new LMenu(rDebugMenu);
	
	menuBar->InstallMenu(debugMenu, InstallMenu_AtEnd);
	
#endif // MACDEV_DEBUG
	
	// *** initialize our preferences object
	mKrbPreferences = new CKrbPreferences();
	Assert_(mKrbPreferences != NULL);
	
	// *** initialize our kerberos session
	mKSession = new CKrbSession();
	Assert_(mKSession != NULL);
	
	// *** additional menu setup
	
	// setup the users menu
	LMenu *activeUsersMenu = menuBar->FetchMenu(rActiveMenu);
	::AppendMenu(activeUsersMenu->GetMacMenuH(), "\pNo Users Authenticated");	 
	
	// adjust menus if under Aqua menu layout
#if TARGET_API_MAC_CARBON
	long result;
	
	Gestalt( gestaltMenuMgrAttr, &result);
	if (result & gestaltMenuMgrAquaLayoutMask) {
		// enable Application menu Preferences item
		::EnableMenuCommand(nil, kAEShowPreferences);
		
		// remove KM's Quit and Preferences items
		LMenu *menuPtr;
		
		menuPtr = menuBar->FetchMenu(rFileMenu);
		SInt16 quitMenuItem = menuPtr->IndexFromCommand(cmd_Quit);
		MenuHandle fileMenuH = menuPtr->GetMacMenuH();
		
		menuPtr->RemoveCommand(cmd_Quit);

		::DeleteMenuItem(fileMenuH, (short)(quitMenuItem - 1));	// remove divider that went before quit
		
		menuPtr = menuBar->FetchMenu(rEditMenu);
		menuPtr->RemoveCommand(cmd_Preferences);
	}
#endif

	// install dock menu - this is an empty function if we're not Mach-O)
	this->SetupDockMenuAndHandlers();

	// install PowerPlant Debug menu
#if MACDEV_DEBUG	
        	LDebugMenuAttachment::InstallDebugMenu(this);
#endif /* MACDEV_DEBUG */

	//create the window if we're not quitting
	if (mFatalFlag == false)
		this->CreateMainWindow();

}

/*
	SetupDockMenuAndHandlers()
	
	Install our Carbon Event handlers.  Even though we're not a Carbon-event based
	application, WaitNextEvent() will call CarbonEvent handlers so you can deal with
	specialized events.
	
	Empty function on non-Mach-O targets.
*/
void CKerberosManagerApp::SetupDockMenuAndHandlers()
{
#if TARGET_RT_MAC_MACHO
	EventTypeSpec     eventType;
	
	mDockMenuCommandHandler = ::NewEventHandlerUPP(CKerberosManagerApp::DockMenuCommandHandler);
	
	if (mDockMenuCommandHandler != nil) {
		eventType.eventClass = kEventClassCommand;
		eventType.eventKind = kEventCommandProcess;
		
		InstallApplicationEventHandler(mDockMenuCommandHandler, 1, &eventType, NULL, NULL);
	}
	
	mDockMenuSetupHandler = ::NewEventHandlerUPP(CKerberosManagerApp::DockMenuSetupHandler);
	
	if (mDockMenuSetupHandler != nil) {
		eventType.eventClass = kEventClassApplication;
		eventType.eventKind = kEventAppGetDockTileMenu;
		
		InstallApplicationEventHandler(mDockMenuSetupHandler, 1, &eventType, NULL, NULL);
	}
		
#endif /* TARGET_RT_MAC_MACHO */
}

/*
	DockMenuSetupHandler()

	Static function installed as a Carbon Event handler.
	Called whenever the user clicks on a dock tile to bring up the menu; we need to use
	this so that we can modify the menu on the fly; reads the base menu from resources,
	disables as necessary, adds user list.

	Empty function on non-Mach-O targets.
*/
pascal OSStatus CKerberosManagerApp::DockMenuSetupHandler(EventHandlerCallRef theHandler, EventRef theEvent, void *userData)
{
#if TARGET_RT_MAC_MACHO
	#pragma unused (theHandler, userData)

	OSErr err = noErr;
	
	// get pointers to the KM classes we need
	CKerberosManagerApp *theApp = static_cast<CKerberosManagerApp *>(LCommander::GetTopCommander());
	Assert_(theApp != nil);
	
	CKMMainWindow *mainWindowRef = theApp->GetMainWindowRef();
	Assert_(mainWindowRef != nil);
	
	CPrincList *princListRef = mainWindowRef->GetPrincListRef();
	Assert_(princListRef != nil);
	
	// get the menu
	LMenu *dockMenu = new LMenu(rDockMenu);

	MenuRef dockMenuRef = (MenuRef)dockMenu->GetMacMenuH();
	
	// set the basic menu command IDs
	err = ::SetMenuItemCommandID(dockMenuRef, kDockMenuDisplayTimeRemainingItem, kCommandDisplayTimeRemaining);
	err = ::SetMenuItemCommandID(dockMenuRef, kDockMenuGetTicketsItem, kCommandGetTickets);
	err = ::SetMenuItemCommandID(dockMenuRef, kDockMenuDestroyTicketsItem, kCommandDestroyTickets);
	err = ::SetMenuItemCommandID(dockMenuRef, kDockMenuRenewTicketsItem, kCommandRenewTickets);
	
	// set the checkmark on the "display time remaining" item
	::CheckMenuItem(dockMenuRef, kDockMenuDisplayTimeRemainingItem, theApp->mKrbPreferences->GetDisplayTimeInDock());
		
	// disable destroy and renew if there aren't any users
	if (princListRef->GetPrincipalCount() == 0) {
		
		::DisableMenuItem(dockMenuRef, kDockMenuDestroyTicketsItem);
		::DisableMenuItem(dockMenuRef, kDockMenuRenewTicketsItem);
	} else {
		// add users list to menu
		
		CTicketListPrincipalItem *user;
		short menuIndex;
		Str255 userDisplayPrincipalString;
		
		//get strings
		LArray *princNameList = princListRef->GetAllPrincipals();
		LArrayIterator iterate(*princNameList, 0L);
		
		//insert separator item before users
		::AppendMenu(dockMenuRef, "\p-");

		//stuff our strings into the menu
		//can't stuff directly because of slashes in instances
		menuIndex = kDockMenuRenewTicketsItem + 2;
		
		// we arbitrarily limit the number of users in the menu to 32, so that we can bound the number of command ID's given out
		UInt32 numUsers = 1;
		
		while (iterate.Next( &user) && numUsers <= kDockMenuMaxUsers) {
			::AppendMenu(dockMenuRef, "\pfoo");
			user->GetItemDisplayString(&userDisplayPrincipalString);
			::SetMenuItemText(dockMenuRef, menuIndex, userDisplayPrincipalString);
			// give the user menu items a series of command IDs so we can identify them later; unfortunately
			// you can't detect which menu item was chose from the dock menu (doh!)
			::SetMenuItemCommandID(dockMenuRef, menuIndex, kCommandDockMenuChangeUserBase+numUsers);
			
			// put a checkmark next to the active user
			if (user->PrincipalIsActive())
				::CheckMenuItem(dockMenuRef, menuIndex, true);
			
			// make expired/invalid users italic
			struct timeval currentTime;
			gettimeofday(&currentTime, nil);
			if ((user->GetItemExpirationTime() < (unsigned long)currentTime.tv_sec) || (user->GetItemValidity() != kTicketValid)) {
				::SetItemStyle(dockMenuRef, menuIndex, italic);
			}

			menuIndex++;
			numUsers++;
		}
		
		delete princNameList;
	}
	
	// tell the OS to adopt our menu
	::SetEventParameter(theEvent, kEventParamMenuRef, typeMenuRef, sizeof(dockMenuRef), &dockMenuRef);
		
	return noErr;
#else
	#pragma unused (theHandler, theEvent, userData)
	return noErr;
#endif
}

/*
	DocMenuCommandHandler()
	
	Static funcion installed as a Carbon Event handler.
	Called whenever a menu item is chosen; normally we wouldn't need this but this is the only
	way to handle dock menu commands.  Check for them and handle them, then check and handle
	special OS commands (preferences and quit), then pass off the rest to be handled by PowerPlant.

	Empty function on non-Mach-O targets.
*/
pascal OSStatus CKerberosManagerApp::DockMenuCommandHandler(EventHandlerCallRef theHandler, EventRef theEvent, void *userData)
{
#if TARGET_RT_MAC_MACHO
	#pragma unused (theHandler, userData)
	
	CKerberosManagerApp *theApp = static_cast<CKerberosManagerApp *>(LCommander::GetTopCommander());
	Assert_(theApp != nil);
	
	if (GetEventKind(theEvent) == kEventProcessCommand) {
		
		HICommand theCommand;
		OSStatus err = noErr;
		UInt32 outSize;
		EventParamType outType;
		
		err = ::GetEventParameter(theEvent, kEventParamDirectObject, typeHICommand, &outType, sizeof(HICommand), &outSize, &theCommand);
		
		// ugly hack to deal with the users in the dock menu, because the stupid event doesn't give us the menu index
		if ((theCommand.commandID > kCommandDockMenuChangeUserBase) && (theCommand.commandID <= kCommandDockMenuChangeUserBase + kDockMenuMaxUsers)) {
			Str255		princName;
			CKrbSession *krbSessionRef = theApp->GetKrbSession();
			
			UInt32 userIndex = theCommand.commandID - kCommandDockMenuChangeUserBase;
			
			//get active users menu info - the order of it and the dock menu are the same
			LMenuBar *menuBar = LMenuBar::GetCurrentMenuBar();
			LMenu *activeUsersMenu = menuBar->FetchMenu(rActiveMenu);
			MenuRef macMenu = activeUsersMenu->GetMacMenuH();
			
			Assert_(macMenu != nil);
			
			//the text is the name of the principal
			::GetMenuItemText(macMenu, (short)userIndex, princName);
		
			LString::PToCStr(princName);
			
			//make the princpal active
			if (krbSessionRef != nil)
				krbSessionRef->SetPrincipalAsDefault((char *)princName);
			
			return noErr;
		}
		
		switch (theCommand.commandID) {
		
			// commands from the Dock Menu
			
			case kCommandDisplayTimeRemaining:
				theApp->mKrbPreferences->SetDisplayTimeInDock(!(theApp->mKrbPreferences->GetDisplayTimeInDock()));
				return noErr;
				break;
				
			case kCommandGetTickets:

				theApp->ProcessCommand(cmd_NewLogin, theApp);
				return noErr;
				break;
			
			case kCommandDestroyTickets:
				theApp->mKMMainWindow->ProcessCommand(cmd_DestroyActiveUser);
				return noErr;
				break;
			
			case kCommandRenewTickets:
				theApp->mKMMainWindow->ProcessCommand(cmd_RenewActiveUser);
				return noErr;
				break;
			
			// built-in Mac OS X commands that we must support since we're filtering through here
			case kHICommandPreferences:
				theApp->ProcessCommand(cmd_Preferences);
				return noErr;
				break;
			
			case kHICommandQuit:
				theApp->ProcessCommand(cmd_Quit);
				return noErr;
				break;
			
			// everything else must be from another menu item; fall through and return "not handled"
			default:
				break;
				/*
				LMenuBar *menuBar = LMenuBar::GetCurrentMenuBar();
				LCommander *target = LCommander::GetTarget();
				
				Assert_(menuBar != nil);
				Assert_(target != nil);
				
				MenuID menuID = ::GetMenuID(theCommand.menu.menuRef);
				
				// special case the two pop-up menus from the prefs dialog and send commands for them directly;
				// the generic conversion below doesn't work because they're not part of the menu bar, I guess
				// This is a VERY UGLY HACK!  Sorry everybody.
				
				if (menuID == rRememberPrincipalPopup) {
					UInt16 item = theCommand.menu.menuItemIndex;
					if (item == 1)
						target->ProcessCommand(cmd_RememberPrincipalPopup1);
					if (item == 2)
						target->ProcessCommand(cmd_RememberPrincipalPopup2);
				} else if (menuID == rRememberOptionsPopup) {
					UInt16 item = theCommand.menu.menuItemIndex;
					if (item == 1)
						target->ProcessCommand(cmd_RememberOptionsPopup1);
					if (item == 2)
						target->ProcessCommand(cmd_RememberOptionsPopup2);
				} else {
					CommandT thePPCommand = menuBar->FindCommand((ResIDT)menuID, (short)theCommand.menu.menuItemIndex);
					target->ProcessCommand(thePPCommand);
				}
				*/
				
		}
	}
	
	return eventNotHandledErr;
#else
	#pragma unused(theHandler, theEvent, userData)
	// empty function if not Mach-O
	return noErr;
#endif
}

// ---------------------------------------------------------------------------
//		¥ StartUp
// ---------------------------------------------------------------------------
//	This function lets you do something when the application starts up
//	without a document. For example, you could issue your own new command.

void CKerberosManagerApp::StartUp()
{
}

// ---------------------------------------------------------------------------
//	¥ DoReopenApp												   [protected]
// ---------------------------------------------------------------------------
//	Respond to Reopen Application AppleEvent
//
//	The system sends the reopen application AppleEvent when the user
//	resumes an application that has no open windows, or when they click on
//  the dock icon.
void CKerberosManagerApp::DoReopenApp()
{
	if (mKMMainWindow != nil) {
		mKMMainWindow->Show();
		mKMMainWindow->Select();
	}
}

void CKerberosManagerApp::DoQuit(SInt32	inSaveOption)
{
	if (mKMMainWindow != nil)
		if (mKrbPreferences != nil)
			if (mKrbPreferences->GetTicketListStartupDisplayOption() == kRememberTicketListOpenState) {
				mKrbPreferences->SetTicketListLastOpen(mKMMainWindow->IsVisible());
			}
	
	LApplication::DoQuit(inSaveOption);
}

// ---------------------------------------------------------------------------
//		¥ ObeyCommand
// ---------------------------------------------------------------------------
//	Respond to commands

Boolean
CKerberosManagerApp::ObeyCommand(
	CommandT	inCommand,
	void		*ioParam)
{
	Boolean		cmdHandled = true;
	Str255		name;
	short menuID, menuItem;
	
	//handle synthetic commands
	if (IsSyntheticCommand(inCommand, menuID, menuItem)) {
		if (menuID == rActiveMenu) {
			//get menu info
			LMenuBar *menuBar = LMenuBar::GetCurrentMenuBar();
			LMenu *activeUsersMenu = menuBar->FetchMenu(rActiveMenu);
			MenuHandle macMenu = activeUsersMenu->GetMacMenuH();
			//the text is the name of the principal
			GetMenuItemText(macMenu, menuItem, name);
			LString::PToCStr(name);
			//make the princpal active
			if (mKSession != nil)
				mKSession->SetPrincipalAsDefault((char *)name);
			cmdHandled = true;
		} else {
			cmdHandled = LApplication::ObeyCommand(inCommand, ioParam);
		}
		return cmdHandled;
	}

	switch (inCommand) {
		case cmd_ShowTicketList:
			if (mKMMainWindow != nil) {
				mKMMainWindow->Show();
				mKMMainWindow->Select();
			}
			break;
			
		case cmd_Preferences:
		{
			CKrbPrefsDialog *prefsDialog;
			StDialogHandler prefsDialogHandler(rPrefsDialogWindow, this);
			
			prefsDialog = dynamic_cast<CKrbPrefsDialog *>(prefsDialogHandler.GetDialog());
			Assert_(prefsDialog != NULL);
			
			prefsDialog->Show();

			Boolean done = false;
			while (!done) {
				MessageT	theMessage = prefsDialogHandler.DoDialog();

				if ( ((theMessage == msg_OK) && prefsDialog->AllowClose()) || (theMessage == msg_Cancel) ) {
					done = true;
				}
			}
			break;
		}
					
 		case cmd_NewLogin:
 			if (mKSession != nil)
	 			mKSession->DoLogin();
 			break;
 		
		case cmd_EditRealmsList:
		{
			CEditRealmsDialog *realmsDialog;
			StDialogHandler realmsDialogHandler(rEditRealmsDialogWindow, this);
			
			realmsDialog = dynamic_cast<CEditRealmsDialog *>(realmsDialogHandler.GetDialog());
			Assert_(realmsDialog != NULL);
			
			realmsDialog->Show();

			Boolean done = false;
			while (!done) {
				MessageT	theMessage = realmsDialogHandler.DoDialog();

				if ( (theMessage == msg_OK) || (theMessage == msg_Cancel) ) {
					done = true;
				}
			}
			break;
		}
		
		// debug menu commands
		case cmd_ForceTicketListRefresh:
			if (mKSession != nil)
				mKSession->SetCacheDirty(true);
			break;

#if !TARGET_API_MAC_CARBON			
		case cmd_UnloadKClientDriver:
			// this code, contributed by Miro, only works for KClient 3.0 and greater (and not on Carbon)!
			ProcessSerialNumber	offendingProcess;
			OSErr err = UnloadSharedLibraryDriver ("\p.kerberos", &offendingProcess);
			char errStr[256];
			CKrbErrorAlert *alrt;
			
			if (err == closErr) {
				ProcessInfoRec	processInfo;
				Str255			processName;
				processInfo.processInfoLength = sizeof (processInfo);
				processInfo.processName = processName;
				processInfo.processAppSpec = nil;

				err = GetProcessInformation (&offendingProcess, &processInfo);
				if (err == noErr) {
					LString::PToCStr (processName);
					sprintf(errStr,"Can't close driver because %s is still running.", (char*) processName);
				} else {
					sprintf(errStr,"Can't close driver and can't get process information (weird).");
				}
				alrt = new CKrbErrorAlert("Error unloading KClient driver.", errStr, "OK", NULL, false);
				alrt->DisplayErrorAlert();
				delete alrt;
				
			} else if (err != noErr) {
				sprintf(errStr,"UnloadSharedLibraryDriver returned %d.", err);
				alrt = new CKrbErrorAlert("Error unloading KClient driver.", errStr, "OK", NULL, false);
				alrt->DisplayErrorAlert();
				delete alrt;
			}
			
			break;
			
		case cmd_LoadKClientDriver:
			// this code, contributed by Miro, only works for KClient 3.0 and greater (and not on Carbon)!
			err =  LoadSharedLibraryDriver (
				"\p.kerberos",
				"\pMIT Kerberos¥KClientDriverDispatchLib.debug");
			
			if (err == dupFNErr) { // means KClient is already installed
				alrt = new CKrbErrorAlert("KClient driver already loaded.", nil, "OK", NULL, false);
				alrt->DisplayErrorAlert();
				delete alrt;
			}
			break;
#endif
			
		case cmd_SyncCredentialCaches:
#if !TARGET_RT_MAC_MACHO
			if (::RunningUnderClassic ()) {
			  ::__CredentialsCacheInternalSyncWithYellowCache (nil);
			}
#endif
			break;
			
		default:
			cmdHandled = LApplication::ObeyCommand(inCommand, ioParam);
			break;
	}
	

	return cmdHandled;
}

// ---------------------------------------------------------------------------
//		¥ FindCommandStatus
// ---------------------------------------------------------------------------
//	This function enables menu commands.
//

void
CKerberosManagerApp::FindCommandStatus(
	CommandT	inCommand,
	Boolean		&outEnabled,
	Boolean		&outUsesMark,
	UInt16		&outMark,
	Str255		outName)
{

	short  menuID, menuItem;
	
	//handle synthetic commands
	if (IsSyntheticCommand(inCommand, menuID, menuItem)) {
		if (menuID == rActiveMenu) {
			outEnabled = true;
		} else {
			LApplication::FindCommandStatus(inCommand, outEnabled,
												outUsesMark, outMark, outName);
		}
		return;
	}

	switch (inCommand) {
	
		case cmd_ShowTicketList:
			outEnabled = true;
			break;
			
		// Return menu item status according to command messages.
		// Any that you don't handle will be passed to LApplication
		case cmd_Quit:
			outEnabled = true;
			break;

		case cmd_Preferences:
			outEnabled = true;
			break;
			
		case cmd_EditRealmsList:
			if (RunningUnderClassic())
				outEnabled = false;
			else
				outEnabled = true;
			break;
			
		case cmd_NewLogin:
			 // if you hit command-N fast enough it's possible for PP to process this command
			 // before mKSession is instanstiated...
			 if (mKSession != NULL) 
				outEnabled = true;
			else
				outEnabled = false;
			break;
			
		case cmd_ActiveUser:
			// if the main window is nil, either it hasn't been created yet or isn't going to be
			// because we're quitting
			if (mKMMainWindow != nil) {
				CPrincList *princList = mKMMainWindow->GetPrincListRef();
				if ((princList) && (princList->GetPrincipalCount() > 0))
					outEnabled = true;
				else
					outEnabled = false;
			}
			break;
		
		case cmd_ForceTicketListRefresh:
#if MACDEV_DEBUG
			outEnabled = true;
#else
			outEnabled = false;
#endif
			break;

		case cmd_UnloadKClientDriver:
		case cmd_LoadKClientDriver:
#if MACDEV_DEBUG && !TARGET_API_MAC_CARBON
			outEnabled = true;
#else
			outEnabled = false;
#endif
			break;

		case cmd_SyncCredentialCaches:
#if MACDEV_DEBUG && !TARGET_RT_MAC_MACHO
			if (RunningUnderClassic())
				outEnabled = true;
			else
				outEnabled = false;
#else
			outEnabled = false;
#endif
			break;
					
		default:
			LApplication::FindCommandStatus(inCommand, outEnabled,
												outUsesMark, outMark, outName);
			break;
	}
}


// ---------------------------------------------------------------------------
//		¥ ListenToMessage
// ---------------------------------------------------------------------------
//	Listen to messages from things like buttons.
//
void
CKerberosManagerApp::ListenToMessage(MessageT inMessage, void *ioParam)  {

#pragma unused (ioParam)

	Str255 princ;

	//buttons send their paneID as a mesage.
	switch(inMessage) {
		case msg_NewLogin:
			ProcessCommand(cmd_NewLogin,this);
			break;
		case msg_MakeActiveUser:
			if (mKMMainWindow != nil) {
				CPrincList *princList = mKMMainWindow->GetPrincListRef();
				if (princList->GetSelectedPrincipal(princ, nil)) {
					LString::PToCStr(princ);
					if (mKSession != nil)
						mKSession->SetPrincipalAsDefault((char *)princ);
				}
			}
			break;
		default:
			break;
	}
}

// ---------------------------------------------------------------------------
//		¥ HandleAppleEvent
// ---------------------------------------------------------------------------
void
CKerberosManagerApp::HandleAppleEvent(
	const AppleEvent&	inAppleEvent,
	AppleEvent&			outAEReply,
	AEDesc&				outResult,
	long				inAENumber)
{
	switch (inAENumber) {

/*     result := AEInstallEventHandler(kCoreEventClass,
         kAEShowPreferences,
         NewAEEventHandlerUPP(@HandleShowPreferences), 0, FALSE);
*/
		case ae_NewLogin:
			ProcessCommand(cmd_NewLogin,this);
			break;
		
		case ae_ShowPreferences:
			ProcessCommand(cmd_Preferences,this);
			break;
		
		default:
			LApplication::HandleAppleEvent(inAppleEvent, outAEReply,
								outResult, inAENumber);
			break;
	}
}

// ---------------------------------------------------------------------------
//		¥ SpendTime
// ---------------------------------------------------------------------------
// periodic activities for CKerberosManagerApp
void CKerberosManagerApp::SpendTime(const EventRecord&) {
	
	// nothing to do but we have to override
}

// ---------------------------------------------------------------------------
//		¥ CreateMainWindow
// ---------------------------------------------------------------------------
//	Create the window
//

void CKerberosManagerApp::CreateMainWindow() {

	Boolean autoPosition;
	Rect windowRect, minMaxRect;
	Point windowPos;
	
	//create the window and show it
	mKMMainWindow = (CKMMainWindow *)LWindow::CreateWindow(rMainWindow, this);
	Assert_(mKMMainWindow != NULL);
	
	Assert_(mKrbPreferences != nil);
	mKrbPreferences->GetMainWindowRect(&autoPosition, &windowRect);
	
	windowPos.h = windowRect.left;
	windowPos.v = windowRect.top;
	
	// if the window position is 0,0 we'll let it position itself
	if ( !autoPosition ) {
		// make sure the stored window position fits in the current screen setup
		// if not, let window auto-position
		if (::PtInRgn(windowPos,::GetGrayRgn())) {
			
			// set position
			mKMMainWindow->DoSetPosition(windowPos);
			
			// get min and max size to make sure stored value of prefs is legal
			// and fix if necessary
			mKMMainWindow->GetMinMaxSize(minMaxRect);
			
			short windowWidth = (short)(windowRect.right - windowRect.left);
			short windowHeight = (short)(windowRect.bottom - windowRect.top);
			
			if (windowWidth < minMaxRect.left)
				windowRect.right = (short)(windowRect.left + minMaxRect.left);
				
			if (windowWidth > minMaxRect.right)
				windowRect.right = (short)(windowRect.left + minMaxRect.right);
				
			if (windowHeight < minMaxRect.top)
				windowRect.bottom = (short)(windowRect.top + minMaxRect.top);
				
			if (windowHeight > minMaxRect.bottom)
				windowRect.bottom = (short)(windowRect.top + minMaxRect.bottom);
				
			// set size
			mKMMainWindow->ResizeWindowTo((SInt16)(windowRect.right - windowRect.left), (SInt16)(windowRect.bottom - windowRect.top));
		}
	}
	
	if (!RunningUnderMacOSX()) {
		mKMMainWindow->Show();
	} else {
		if ((mKrbPreferences->GetTicketListStartupDisplayOption() == kAlwaysShowTicketListAtStartup) ||
			((mKrbPreferences->GetTicketListStartupDisplayOption() == kRememberTicketListOpenState) && mKrbPreferences->GetTicketListLastOpen()))
				mKMMainWindow->Show();
	}

	UReanimator::LinkListenerToControls(this, mKMMainWindow, rMainWindow);

	StartIdling();	

}


// ---------------------------------------------------------------------------
//		¥ GetKrbPreferencesRef
// ---------------------------------------------------------------------------
//	return the GetKrbPreferencesRef object (a pointer to it actually) that we initialized in StartUp
//
CKrbPreferences * CKerberosManagerApp::GetKrbPreferencesRef() {

	return mKrbPreferences;
}

// ---------------------------------------------------------------------------
//		¥ GetKrbSession
// ---------------------------------------------------------------------------
//	return the CKrbSession object (a pointer to it actually) that we initialized in StartUp
//
CKrbSession * CKerberosManagerApp::GetKrbSession() {

	return mKSession;
}

// ---------------------------------------------------------------------------
//		¥ GetKrbSession
// ---------------------------------------------------------------------------
//	return the CKrbSession object (a pointer to it actually) that we initialized in StartUp
//
CKMMainWindow * CKerberosManagerApp::GetMainWindowRef() {

	return mKMMainWindow;
}

// ---------------------------------------------------------------------------
//		¥ ShowAboutBox
/*
void CKerberosManagerApp::ShowAboutBox() {
	StDialogHandler aboutHandler(rAboutBoxWindow, this);
	
	LWindow *aboutDialog = aboutHandler.GetDialog();
	if (aboutDialog == NULL) return;
	
	LStaticText *versionText;
	versionText = dynamic_cast<LStaticText *>(aboutDialog->FindPaneByID(rAboutVersionText) );
	Assert_(versionText != NULL);
	
	// read the 'vers' resource and display the short version string in the about box
	// (maybe we should be reading the number directly but this is easier)
	VersRecHndl versionResource = (VersRecHndl) ::Get1Resource (version_ResType, version_KM_ResID);

	if (versionResource != nil)  {
		StringPtr versionString = (*versionResource) -> shortVersion;
		versionText->SetDescriptor(versionString);
	} else {
		versionText->SetDescriptor("\pUnknown Version");
	}
	
	aboutDialog->Show();
	while(!aboutHandler.DoDialog()) {
		// this space intentionally blank
	}
	aboutDialog->Hide();
	
}
*/

/* Show the about box, which tells about Kerberos for Macintosh */
void CKerberosManagerApp::ShowAboutBox() {

	StDialogHandler aboutHandler(rAboutKFMBoxWindow, this);
	
	LWindow *aboutDialog = aboutHandler.GetDialog();
	if (aboutDialog == NULL) return;
	
	LStaticText *versionText;
	LStr255 versionString = "";
	
	versionText = dynamic_cast<LStaticText *>(aboutDialog->FindPaneByID(rAboutVersionText) );
	Assert_(versionText != NULL);
	
#if !TARGET_RT_MAC_MACHO
	// read the 'vers' resource and display the short version string in the about box
	// (maybe we should be reading the number directly but this is easier)
	VersRecHndl versionResource = (VersRecHndl) ::Get1Resource (version_ResType, version_KFM_ResID);

	if (versionResource != nil)  {
		StringPtr versionNumberString = (*versionResource) -> shortVersion;
		versionString += "Version ";
		versionString += versionNumberString;
		
	} else {
		versionString += "Unknown Version";
	}
#else
    //std::string versionString;
    CFStringRef value = NULL;
    CFBundleRef bundle = NULL;

    bundle = CFBundleGetBundleWithIdentifier (CFSTR("edu.mit.Kerberos"));
    
    if (bundle != NULL) {
            value = (CFStringRef) CFBundleGetValueForInfoDictionaryKey (
                                    bundle, CFSTR("CFBundleGetInfoString"));
    }
    
    if ((value != NULL) && (CFGetTypeID (value) == CFStringGetTypeID() )) {
            // We got a string back! 
            CFIndex stringSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength (value), GetApplicationTextEncoding()) + 1;
            char *tempString = static_cast<char *>(malloc ((unsigned long)stringSize));
        if (tempString != NULL) {
                Boolean found = CFStringGetCString (value, tempString, stringSize, GetApplicationTextEncoding() );
                if (found) {
					versionString += "Version ";
					versionString += tempString;
                } else {
					versionString += "Unknown Version";
                }
                free (tempString);
            }
    }
#endif

	versionText->SetDescriptor(versionString);
	
	aboutDialog->Show();
	while(!aboutHandler.DoDialog()) {
		// this space intentionally blank
	}
	aboutDialog->Hide();
	
}

/* Set the flag which tells the app whether a fatal error has occurred, and we're quitting,
   so we shouldn't try to do anything complicated because it might not be initialized. */
void CKerberosManagerApp::SetFatalFlag(Boolean inFatalFlag)
{
	mFatalFlag = inFatalFlag;
}
