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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/CKrbPreferences.cp,v 1.56 2003/09/17 18:42:38 smcguire Exp $ */

// ===========================================================================
//	CKrbPreferences.cp
// ===========================================================================


#include <UResourceMgr.h>
#include <LHandleStream.h>

#include <CoreFoundation/CoreFoundation.h>

#include <Kerberos/Kerberos.h>

#include "CKrbPreferences.h"
#include "CKrbErrorAlert.h"
#include "CKerberosManagerApp.h"

PP_Using_Namespace_PowerPlant

enum {
	kTMMiscPrefsVersion1 = 1,
	kTMMiscPrefsVersion2 = 2,
	kTMMiscPrefsVersion3 = 3,
	kTMMiscPrefsVersion4 = 4,
	kTMMiscPrefsVersion5 = 5,	// last resource-based version
	kTMMiscPrefsVersion6 = 6,
	kTMMiscPrefsVersion7 = 7
};

const	UInt32		kCurrentKMMiscPrefsVersion		= kTMMiscPrefsVersion7;
const	Str255		kTMMiscPrefsResName				= "\pKerberos CP Preferences";

// keys for CFPreferences
const CFStringRef prefsVersionKey					= CFSTR("KAPreferencesVersion");
const CFStringRef alwaysExpandTicketListKey			= CFSTR("KAAlwaysExpandTicketList");
const CFStringRef displayTimeInDockKey				= CFSTR("KADisplayTimeInDock");
const CFStringRef ticketListStartupDisplayOptionKey = CFSTR("KATicketListStartupDisplayOption");
const CFStringRef ticketListLastOpenKey 			= CFSTR("KATicketListLastOpen");
const CFStringRef autoRenewTicketsKey 				= CFSTR("KAAutoRenewTickets");
const CFStringRef defaultPositionAndSizeMainWindownKey = CFSTR("KADefaultPositionAndSizeMainWindow");
const CFStringRef mainWindowRectTopKey 				= CFSTR("KAMainWindowRectTop");
const CFStringRef mainWindowRectLeftKey 			= CFSTR("KAMainWindowRectLeft");
const CFStringRef mainWindowRectBottomKey 			= CFSTR("KAMainWindowRectBottom");
const CFStringRef mainWindowRectRightKey 			= CFSTR("KAMainWindowRectRight");


// ---------------------------------------------------------------------------
//		¥ Constructors
// ---------------------------------------------------------------------------
CKrbPreferences::CKrbPreferences()
{		
	InitKrbPreferences();	
}

// ---------------------------------------------------------------------------
//		¥ Destructor
// ---------------------------------------------------------------------------
CKrbPreferences::~CKrbPreferences() {
	
}

// set up preferences to be used
void CKrbPreferences::InitKrbPreferences() {
	Boolean result = false;
	
	mMiscPrefsDirty = false;
	mPrefsInitialized = false;
	mDontSavePrefsMode = false;
	
	result = InitializeFromCFPreferences();
	
	if (result)
		return;

/*		
	result = InitializeFromPrefsResourceFile();
	
	if ( result )
		return;
*/	
	InitializeFromHardcodedDefaults();
}


#pragma mark -

Boolean CKrbPreferences::InitializeFromCFPreferences()
{
	Boolean foundPref;
	
	/* First, initialize from hardcoded defaults. That way, if an older version of preferences
	   doesn't have all fields, the missing fields will always be initialized to defaults */
	InitializeFromHardcodedDefaults();
	
	// but then assume we aren't done
	mPrefsInitialized = false;

	// if reading a prefs version number fails, we'll assume there is no CFPreferences version of the prefs, and thus
	// should fall back on trying to read from the resource Prefs
	mPrefs.miscPrefsVersion = (UInt32)CFPreferencesGetAppIntegerValue(prefsVersionKey, kCFPreferencesCurrentApplication, &foundPref);
	if (!foundPref)
		return false;
	
	// read prefs via CFPrefs.  unfortunately there's no real error checking we can do here, but if reading one
	// fails, we do have the hardcoded defaults we read above and pass in here to fall back on
	mPrefs.alwaysExpandTicketList = GetBooleanWithKey (alwaysExpandTicketListKey, false);
	mPrefs.displayTimeInDock = GetBooleanWithKey (displayTimeInDockKey, true);
	mPrefs.ticketListStartupDisplayOption = (UInt16) GetShortWithKey (ticketListStartupDisplayOptionKey, kAlwaysShowTicketListAtStartup);
	mPrefs.ticketListLastOpen = GetBooleanWithKey (ticketListLastOpenKey, true);
	mPrefs.autoRenewTickets = GetBooleanWithKey (autoRenewTicketsKey, true);
	mPrefs.defaultPositionAndSizeMainWindow = GetBooleanWithKey (defaultPositionAndSizeMainWindownKey, true);
	mPrefs.mainWindowRect.top = GetShortWithKey (mainWindowRectTopKey, 0);
	mPrefs.mainWindowRect.left = GetShortWithKey (mainWindowRectLeftKey, 0);
	mPrefs.mainWindowRect.bottom = GetShortWithKey (mainWindowRectBottomKey, 0);
	mPrefs.mainWindowRect.right = GetShortWithKey (mainWindowRectRightKey, 0);
		
	mPrefsInitialized = true;
	return true;

}

// set the prefs from hardcoded values (last resort)
void CKrbPreferences::InitializeFromHardcodedDefaults()
{
	Assert_(!mPrefsInitialized);
	
	mPrefs.miscPrefsVersion = kCurrentKMMiscPrefsVersion;
	
	mPrefs.alwaysExpandTicketList = false;
	mPrefs.displayTimeInDock = true;
	mPrefs.autoRenewTickets = true;

	mPrefs.ticketListStartupDisplayOption = kAlwaysShowTicketListAtStartup;
	mPrefs.ticketListLastOpen = true;
	
	// set main window position to "position yourself" (the app will detect this)
	mPrefs.defaultPositionAndSizeMainWindow = true;
	SetRect(&(mPrefs.mainWindowRect), 0, 0, 0, 0);
	
	mPrefsInitialized = true;
}

// do some basic checking on the prefs we've read in to make sure they look reasonable
Boolean CKrbPreferences::PrefsAreSane()
{
	// check prefs version to make sure it's not greater than what we know how to deal with
	if (mPrefs.miscPrefsVersion > kCurrentKMMiscPrefsVersion) {
		CKrbErrorAlert *alrt = new CKrbErrorAlert("Unknown preferences version.",
			"The Kerberos application cannot understand the saved preferences. Defaults will be used for this session.",
			"OK", NULL, false);
		alrt->DisplayErrorAlert();
		delete alrt;
		return false;
	}
	
	return true;
}

void CKrbPreferences::PrefsChanged(Boolean warnOnFailure)
{
	Boolean writeSuccess = true;
	
	Assert_ (mPrefsInitialized);
	
	if (!mDontSavePrefsMode)
		writeSuccess = this->WritePrefsToDisk();
		
	if (!writeSuccess && warnOnFailure) {
		std::string errorString;
		
		errorString = "There was an error while trying to save your preferences."
					  " Changes will not be saved to disk, but may be retained in memory for now.";
		CKrbErrorAlert *alrt = new CKrbErrorAlert("Error saving preferences.",
			(char *)errorString.c_str(), "OK", NULL, false);
		alrt->DisplayErrorAlert();
		delete alrt;
			
		return;
	}
	
	mMiscPrefsDirty = false;
	
}

Boolean CKrbPreferences::WritePrefsToDisk()
{
	Boolean syncSuccess = true;
	
	syncSuccess = SetShortWithKey (prefsVersionKey, (short)kCurrentKMMiscPrefsVersion);
	if (!syncSuccess)
		return false;
		
	syncSuccess = SetBooleanWithKey (alwaysExpandTicketListKey, mPrefs.alwaysExpandTicketList);
	if (!syncSuccess)
		return false;
		
	syncSuccess = SetBooleanWithKey (displayTimeInDockKey, mPrefs.displayTimeInDock);
	if (!syncSuccess)
		return false;
		
	syncSuccess = SetShortWithKey (ticketListStartupDisplayOptionKey, (short)mPrefs.ticketListStartupDisplayOption);
	if (!syncSuccess)
		return false;
		
	syncSuccess = SetBooleanWithKey (ticketListLastOpenKey, mPrefs.ticketListLastOpen);
	if (!syncSuccess)
		return false;
		
	syncSuccess = SetBooleanWithKey (autoRenewTicketsKey, mPrefs.autoRenewTickets);
	if (!syncSuccess)
		return false;
		
	syncSuccess = SetBooleanWithKey (defaultPositionAndSizeMainWindownKey, mPrefs.defaultPositionAndSizeMainWindow);
	if (!syncSuccess)
		return false;
		
	syncSuccess = SetShortWithKey (mainWindowRectTopKey, mPrefs.mainWindowRect.top);
	if (!syncSuccess)
		return false;
		
	syncSuccess = SetShortWithKey (mainWindowRectLeftKey, mPrefs.mainWindowRect.left);
	if (!syncSuccess)
		return false;
		
	syncSuccess = SetShortWithKey (mainWindowRectBottomKey, mPrefs.mainWindowRect.bottom);
	if (!syncSuccess)
		return false;
		
	syncSuccess = SetShortWithKey (mainWindowRectRightKey, mPrefs.mainWindowRect.right);
	if (!syncSuccess)
		return false;
	
	return true;			
}

// utility function for adding a pascal string (such as as realm) to a handle
Boolean CKrbPreferences::AppendString (Str255 string, Handle strings)
{
	StringPtr  ptr = nil;

	Size oldSize = ::GetHandleSize (strings);

	::SetHandleSize (strings, oldSize + StrLength (string) + 1);
	if (MemError() != noErr)
		return false;
	
	::HLock(strings);
	ptr = (unsigned char *)(*strings + oldSize);

	::BlockMoveData (string, ptr, StrLength (string) + 1);
	::HUnlock (strings);
	
	return true;
}

#pragma mark -

/* Get and Set DontSavePrefsMode.  When true, prefs will allow changes but will not
   attempt to write them to disk.  Useful when application is making changes it must
   make without user's input. */
void CKrbPreferences::SetDontSavePrefsMode(Boolean inDontSavePrefsOption)
{
	mDontSavePrefsMode = inDontSavePrefsOption;
}

Boolean CKrbPreferences::GetDontSavePrefsMode()
{
	return mDontSavePrefsMode;
}

#pragma mark -

void CKrbPreferences::SetAlwaysExpandTicketList(Boolean inExpandTicketListOption)
{
	if (inExpandTicketListOption != mPrefs.alwaysExpandTicketList) {
		mPrefs.alwaysExpandTicketList = inExpandTicketListOption;

		mMiscPrefsDirty = true;
		this->PrefsChanged();
		BroadcastMessage(msg_PrefsAlwaysExpandTicketListChanged, nil);
	}
}

Boolean CKrbPreferences::GetAlwaysExpandTicketList()
{
	return mPrefs.alwaysExpandTicketList;
}
				
void CKrbPreferences::SetDisplayTimeInDock(Boolean inDisplayTimeInDockOption)
{
	if (inDisplayTimeInDockOption != mPrefs.displayTimeInDock) {
		mPrefs.displayTimeInDock = inDisplayTimeInDockOption;

		mMiscPrefsDirty = true;
		this->PrefsChanged();
		BroadcastMessage(msg_PrefsDisplayTimeInDockChanged, nil);
	}
}

Boolean CKrbPreferences::GetDisplayTimeInDock()
{
	return mPrefs.displayTimeInDock;
}

void CKrbPreferences::SetAutoRenewTicketsPref(Boolean inAutoRenewTicketsOption)
{
	if (inAutoRenewTicketsOption != mPrefs.autoRenewTickets) {
		mPrefs.autoRenewTickets = inAutoRenewTicketsOption;

		mMiscPrefsDirty = true;
		this->PrefsChanged();
		BroadcastMessage(msg_PrefsAutoRenewTicketsChanged, nil);
	}
}

Boolean CKrbPreferences::GetAutoRenewTicketsPref()
{
	return mPrefs.autoRenewTickets;
}

void CKrbPreferences::SetTicketListStartupDisplayOption(UInt16 inTicketListStartupDisplayOption)
{
	if (inTicketListStartupDisplayOption != mPrefs.ticketListStartupDisplayOption) {
		mPrefs.ticketListStartupDisplayOption = inTicketListStartupDisplayOption;

		mMiscPrefsDirty = true;
		this->PrefsChanged();
		BroadcastMessage(msg_PrefsTicketListStartupDisplayOptionChanged, nil);
	}
}

UInt16 CKrbPreferences::GetTicketListStartupDisplayOption()
{
	return mPrefs.ticketListStartupDisplayOption;
}

void CKrbPreferences::SetTicketListLastOpen(Boolean inTicketListLastOpenOption)
{
	if (inTicketListLastOpenOption != mPrefs.ticketListLastOpen) {
		mPrefs.ticketListLastOpen = inTicketListLastOpenOption;

		mMiscPrefsDirty = true;
		this->PrefsChanged();
		BroadcastMessage(msg_PrefsTicketListLastOpenChanged, nil);
	}
}

Boolean	CKrbPreferences::GetTicketListLastOpen()
{
	return mPrefs.ticketListLastOpen;
}

#pragma mark -

// main window position functions
// Use 0,0 to tell the application to let the window position itself
void CKrbPreferences::SetMainWindowRect(Boolean inDefaultPosition, const Rect *inWindowRect)
{
	if (!::EqualRect(&(mPrefs.mainWindowRect),inWindowRect) ||
		(inDefaultPosition != mPrefs.defaultPositionAndSizeMainWindow)) {
		
		mPrefs.defaultPositionAndSizeMainWindow = inDefaultPosition;
		mPrefs.mainWindowRect = *inWindowRect;
		
		mMiscPrefsDirty = true;
		this->PrefsChanged(false);
		BroadcastMessage(msg_PrefsMainWindowPositionChanged, nil);
	}
}

void CKrbPreferences::GetMainWindowRect(Boolean *outDefaultPosition, Rect *outWindowRect)
{
	*outDefaultPosition = mPrefs.defaultPositionAndSizeMainWindow;
	*outWindowRect = mPrefs.mainWindowRect;
}

#pragma mark -
// ---------------------------------------------------------------------------

short
CKrbPreferences::GetShortWithKey (const CFStringRef key, const short defaultNumber)
{
    CFNumberRef cfValue = NULL;
    short value = defaultNumber;
    
    if (key != NULL) {
        cfValue = (CFNumberRef) CFPreferencesCopyAppValue (key, kCFPreferencesCurrentApplication);
    }
    
    if (cfValue != NULL && (CFGetTypeID (cfValue) == CFNumberGetTypeID ())) {
        if (CFNumberGetValue (cfValue, kCFNumberShortType, &value) == true) {
            return value;
        }
    }
    return defaultNumber;
}

// ---------------------------------------------------------------------------

Boolean
CKrbPreferences::SetShortWithKey (const CFStringRef key, const short valueNumber)
{
    Boolean wrotePrefs = false;
    CFNumberRef value = NULL;
    
    value = CFNumberCreate (kCFAllocatorDefault, kCFNumberShortType, &valueNumber);
    if ((key != NULL) && (value != NULL)) {
        CFPreferencesSetAppValue (key, value, kCFPreferencesCurrentApplication);
        wrotePrefs = CFPreferencesAppSynchronize (kCFPreferencesCurrentApplication);
    }
    if (value != NULL) {
        CFRelease (value);
    }
    return wrotePrefs;
}

#pragma mark -

// ---------------------------------------------------------------------------

Boolean
CKrbPreferences::GetBooleanWithKey (const CFStringRef key, const Boolean defaultBoolean)
{
    CFBooleanRef value = NULL;
    
    if (key != NULL) {
        value = (CFBooleanRef) CFPreferencesCopyAppValue (key, kCFPreferencesCurrentApplication);
    }
    
    if (value != NULL && (CFGetTypeID (value) == CFBooleanGetTypeID ())) {
        //dprintf ("GetBooleanWithKey: got boolean \"%s\"\n", CFBooleanGetValue (value) ? "true" : "false");
        return CFBooleanGetValue (value);
    }
    return defaultBoolean;
}

// ---------------------------------------------------------------------------

Boolean
CKrbPreferences::SetBooleanWithKey (const CFStringRef key, const Boolean valueBoolean)
{
    Boolean wrotePrefs = false;
    CFBooleanRef value = valueBoolean ? kCFBooleanTrue : kCFBooleanFalse;
    
    if (key != NULL) {
        CFPreferencesSetAppValue (key, value, kCFPreferencesCurrentApplication);
        wrotePrefs = CFPreferencesAppSynchronize (kCFPreferencesCurrentApplication);
    }
    return wrotePrefs;
}
