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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/Headers/CKrbPreferences.h,v 1.36 2003/09/17 18:42:40 smcguire Exp $ */

// ===========================================================================
//	CKrbPreferences.h
// ===========================================================================

#pragma once

#include "CTextColumnColorized.h"

// get #defines for preferences file name, type, creator
#include <Kerberos/Kerberos.h>

// messages for announced a pref changed
const PP_PowerPlant::MessageT msg_PrefsMainWindowPositionChanged	= 50006;
const PP_PowerPlant::MessageT msg_PrefsAlwaysExpandTicketListChanged		= 50009;
const PP_PowerPlant::MessageT msg_PrefsDisplayTimeInDockChanged		= 50014;
const PP_PowerPlant::MessageT msg_PrefsTicketListLastOpenChanged		= 50015;
const PP_PowerPlant::MessageT msg_PrefsTicketListStartupDisplayOptionChanged		= 50016;
const PP_PowerPlant::MessageT msg_PrefsAutoRenewTicketsChanged		= 50017;

// pragmas to make prefs struct a known alignment
#if PRAGMA_STRUCT_ALIGN
	#pragma options align=mac68k
#elif PRAGMA_STRUCT_PACKPUSH
	#pragma pack(push, 2)
#elif PRAGMA_STRUCT_PACK
	#pragma pack(2)
#endif

/*
// old prefs v2 structure
typedef struct {
	UInt32	miscPrefsVersion;

	Str255	username;
	Boolean	rememberUsername;
	
	UInt32	minimumTicketLifetime;
	UInt32	maximumTicketLifetime;
	UInt32	defaultTicketLifetime;
	
	Boolean	defaultForwardableTicket;
	
	Boolean	advancedLoginOptionDisplay;
	Boolean	alwaysExpandTicketList;
	
	Boolean	defaultPositionAndSizeMainWindow;
	Rect	mainWindowRect;
	
	Boolean	autoPositionLoginWindow;
	Point	loginWindowPosition;	// upper left corner
	
	UInt32	realmCount;
	Str255	userDefaultRealm;	// not the Kerberos default realm, but the one the user wants as default
	
} TicketManagerPrefs;
*/

// prefs v3 structure
/*
typedef struct {
	UInt32	miscPrefsVersion;

	Boolean	alwaysExpandTicketList;
	Boolean displayTimeInDock;	// new in v3 prefs
	
	Boolean	defaultPositionAndSizeMainWindow;
	Rect	mainWindowRect;
	
} TicketManagerPrefs;
*/

// prefs v4-v5 structure
/*
typedef struct {
	UInt32	miscPrefsVersion;

	Boolean	alwaysExpandTicketList;
	Boolean displayTimeInDock;	// new in v3 prefs
	//Boolean showTicketListAtStartup; // new in v4 prefs, out dated in v5
	
	UInt16	ticketListStartupDisplayOption;
	Boolean	ticketListLastOpen;
	
	Boolean	defaultPositionAndSizeMainWindow;
	Rect	mainWindowRect;
	
} TicketManagerPrefs;
*/


enum {
	kAlwaysShowTicketListAtStartup = 1,
	kNeverShowTicketListAtStartup,
	kRememberTicketListOpenState
};

// prefs v7 structure
typedef struct {
	UInt32	miscPrefsVersion;

	Boolean	alwaysExpandTicketList;
	Boolean displayTimeInDock;
	Boolean autoRenewTickets; // new in v7 prefs
	
	UInt16	ticketListStartupDisplayOption;
	Boolean	ticketListLastOpen;
	
	Boolean	defaultPositionAndSizeMainWindow;
	Rect	mainWindowRect;
	
} TicketManagerPrefs;

#if PRAGMA_STRUCT_ALIGN
	#pragma options align=reset
#elif PRAGMA_STRUCT_PACKPUSH
	#pragma pack(pop)
#elif PRAGMA_STRUCT_PACK
	#pragma pack()
#endif

class	CKrbPreferences : public PP_PowerPlant::LBroadcaster {

public:
						CKrbPreferences();
								
			virtual		~CKrbPreferences();
	
				void 	InitKrbPreferences();
				
				void	SetDontSavePrefsMode(Boolean inDontSavePrefsOption);
				Boolean	GetDontSavePrefsMode();

				// display options
				void	SetAlwaysExpandTicketList(Boolean inExpandTicketListOption);
				Boolean	GetAlwaysExpandTicketList();
				
				void	SetDisplayTimeInDock(Boolean inDisplayTimeInDockOption);
				Boolean	GetDisplayTimeInDock();

				void	SetAutoRenewTicketsPref(Boolean inAutoRenewTicketsOption);
				Boolean	GetAutoRenewTicketsPref();

				void	SetTicketListStartupDisplayOption(UInt16 inTicketListStartupDisplayOption);
				UInt16	GetTicketListStartupDisplayOption();

				void	SetTicketListLastOpen(Boolean inShowTicketListAtStartupOption);
				Boolean	GetTicketListLastOpen();

				// window position functions
				void	SetMainWindowRect(Boolean inDefaultPosition, const Rect *inWindowRect);
				void	GetMainWindowRect(Boolean *outDefaultPosition, Rect *outWindowRect);
				
				
protected:
				Boolean InitializeFromCFPreferences();
				void	InitializeFromHardcodedDefaults();
				
				void PrefsChanged(Boolean warnOnFailure = true);
				Boolean WritePrefsToDisk();

				Boolean AppendString (Str255 string, Handle strings);

				Boolean	PrefsAreSane();
				
				// functions for fiddling with CFPreferences
				short GetShortWithKey (const CFStringRef key, const short defaultNumber);
				Boolean SetShortWithKey (const CFStringRef key, const short valueNumber);
				Boolean GetBooleanWithKey (const CFStringRef key, const Boolean defaultBoolean);
				Boolean SetBooleanWithKey (const CFStringRef key, const Boolean valueBoolean);



private:
				Boolean				mMiscPrefsDirty;
				Boolean				mPrefsInitialized;
				Boolean				mDontSavePrefsMode;
				
				FSSpec				mCurrentPrefsFileSpec;

				TicketManagerPrefs	mPrefs;

};
