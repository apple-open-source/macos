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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/Headers/CKerberosManagerApp.h,v 1.58 2003/05/06 20:41:43 smcguire Exp $ */

// ===========================================================================
//	CKerberosManagerApp.h
// ===========================================================================

#pragma once

#include <LApplication.h>
#include <LListener.h>
#include <PP_Types.h>

#if MACDEV_DEBUG
#include <LDebugMenuAttachment.h>
#endif

#include "CKrbSession.h"
#include "CPrincList.h"
#include "CKrbPreferences.h"
#include "CKMMainWindow.h"

//errors
const short		noDateTimeCDEVerr		=	-128;
const short		noFindFolderErr			=	-129;

//resource constants
	// main window
const PP_PowerPlant::ResIDT	rMainWindow				= 	1000;
const PP_PowerPlant::ResIDT	rIconControl			=	1001;
const PP_PowerPlant::ResIDT	rTitleLabel1			=	1002;
const PP_PowerPlant::ResIDT	rNewLoginButton			=	1003;
const PP_PowerPlant::ResIDT	rActiveUserGroupBox		=	1004;
const PP_PowerPlant::ResIDT	rActiveUserLabel		=	1005;
const PP_PowerPlant::ResIDT	rActiveRealmLabel		=	1006;
const PP_PowerPlant::ResIDT	rActiveTimeRemainingLabel	=	1007;
const PP_PowerPlant::ResIDT	rActiveUserText			=	1008;
const PP_PowerPlant::ResIDT	rActiveRealmText		=	1009;
const PP_PowerPlant::ResIDT	rActiveTimeRemainingText 	= 	1010;
const PP_PowerPlant::ResIDT	rRenewButton			=	1011;
const PP_PowerPlant::ResIDT	rDestroyTicketsButton	=	1012;
const PP_PowerPlant::ResIDT	rChangePasswordButton	=	1013;
const PP_PowerPlant::ResIDT	rTitleLabel2			=	1014;
const PP_PowerPlant::ResIDT	rTicketLabel			=	1015;
const PP_PowerPlant::ResIDT	rTimeRemainingLabel		=	1016;
const PP_PowerPlant::ResIDT	rPrincListTable			=	1017;
const PP_PowerPlant::ResIDT	rMakeActiveUserButton	=	1018;
const PP_PowerPlant::ResIDT	rPrincListScroller		= 	1020;

// no Ticket Keeper warning alert
const PP_PowerPlant::ResIDT	rNoTKAlert				= 	3100;

//alert warnings
const PP_PowerPlant::ResIDT	rGenericFatalAlert		=	4500;
const PP_PowerPlant::ResIDT	rGenericErrorAlert		=	4600;

// about box(es)
//const ResIDT	rAboutBoxWindow			= 	5000;
const PP_PowerPlant::ResIDT	rAboutKFMBoxWindow		=	5100;

const PP_PowerPlant::ResIDT	rAboutVersionText		=	5005;

// text traits
const PP_PowerPlant::ResIDT	rSystemFontPlain	= 128;
const PP_PowerPlant::ResIDT	rSystemFontRed		= 228;
const PP_PowerPlant::ResIDT	rSystemFontItalic	= 229;
const PP_PowerPlant::ResIDT	rSystemFontRedItalic	= 230;
const PP_PowerPlant::ResIDT	rSystemFontWhiteBoldLarge	= 231;
const PP_PowerPlant::ResIDT rDockTimeRemainingLabelFont	= 232;
const PP_PowerPlant::ResIDT	rGeneva9Plain	= 130;
const PP_PowerPlant::ResIDT	rGeneva9Red		= 146;
const PP_PowerPlant::ResIDT	rGeneva9RedItalic		= 147;

//messages
const PP_PowerPlant::MessageT	msg_Renew			=	rRenewButton;
const PP_PowerPlant::MessageT  msg_NewLogin		=	rNewLoginButton;
const PP_PowerPlant::MessageT	msg_Destroy			=	rDestroyTicketsButton;
const PP_PowerPlant::MessageT	msg_ChangePasswd	=	rChangePasswordButton;
const PP_PowerPlant::MessageT	msg_MakeActiveUser	=	rMakeActiveUserButton;

//menu items
const PP_PowerPlant::CommandT	cmd_ShowTicketList	=	1400;

const PP_PowerPlant::CommandT	cmd_NewLogin		=	1500;
const PP_PowerPlant::CommandT	cmd_DestroySelectedUser			=	1501;
const PP_PowerPlant::CommandT  cmd_RenewSelectedUser			=	1502;
const PP_PowerPlant::CommandT  cmd_ChangePassword	=	1503;
const PP_PowerPlant::CommandT	cmd_ActiveUser		=	1504;
const PP_PowerPlant::CommandT	cmd_GetCredInfo		=	1505;

const PP_PowerPlant::CommandT	cmd_EditRealmsList	=	'Erlm';
//const CommandT cmd_quit is already defined

const PP_PowerPlant::CommandT	cmd_DestroyActiveUser = 1511;	// used by the dock menu
const PP_PowerPlant::CommandT	cmd_RenewActiveUser = 1512;		// used by the dock menu

const PP_PowerPlant::CommandT	cmd_ForceTicketListRefresh	= 1600;
const PP_PowerPlant::CommandT	cmd_UnloadKClientDriver		= 1601;
const PP_PowerPlant::CommandT	cmd_LoadKClientDriver		= 1602;
const PP_PowerPlant::CommandT	cmd_LaunchTicketKeeper		= 1603;
const PP_PowerPlant::CommandT	cmd_KillTicketKeeper		= 1604;
const PP_PowerPlant::CommandT	cmd_SyncCredentialCaches	= 1605;

const PP_PowerPlant::CommandT	cmd_RememberPrincipalPopup1	= 1701;
const PP_PowerPlant::CommandT	cmd_RememberPrincipalPopup2	= 1702;
const PP_PowerPlant::CommandT	cmd_RememberOptionsPopup1	= 1703;
const PP_PowerPlant::CommandT	cmd_RememberOptionsPopup2	= 1704;

// menu ID's
const PP_PowerPlant::ResIDT	rFileMenu			=	129;
const PP_PowerPlant::ResIDT	rEditMenu			=	130;
const PP_PowerPlant::ResIDT	rDebugMenu			=	160;
const PP_PowerPlant::ResIDT	rActiveMenu			=	200;
const PP_PowerPlant::ResIDT	rRememberPrincipalPopup	=	400;
const PP_PowerPlant::ResIDT	rRememberOptionsPopup	=	401;
const PP_PowerPlant::ResIDT	rDockMenu			= 	500;

//AppleEvent IDs
const long		ae_NewLogin			=	10000;
const long		ae_ShowPreferences	=	10001;

//Dock Menu menu item positions
enum {
	kDockMenuDisplayTimeRemainingItem = 1,
	kDockMenuGetTicketsItem	=	3,
	kDockMenuDestroyTicketsItem,
	kDockMenuRenewTicketsItem
};

//Dock Menu Command IDs
const MenuCommand kCommandDisplayTimeRemaining = FOUR_CHAR_CODE ('DTRe');
const MenuCommand kCommandGetTickets = FOUR_CHAR_CODE ('GTix');
const MenuCommand kCommandDestroyTickets = FOUR_CHAR_CODE ('DTix');
const MenuCommand kCommandRenewTickets = FOUR_CHAR_CODE ('RTix');
const MenuCommand kCommandDockMenuChangeUserBase = FOUR_CHAR_CODE ('DMcu');

const MenuCommand kDockMenuMaxUsers = 32;

//some string constants
const Str255 	kNoActiveUserUserString		=	"\pNo active user";
const Str255	kNoActiveUserRealmString	=	"\p——";
const Str255	kNoActiveUserTimeString	=	"\p——";


// macros testing for shared libraries
#define KrbLibIsPresent_ ((Ptr) krb5_init_context != (Ptr) kUnresolvedCFragSymbolAddress)
#define CCacheLibIsPresent_ ((Ptr) cc_initialize != (Ptr) kUnresolvedCFragSymbolAddress)

// some sort of forward declaration?
class CPrincList;

class	CKerberosManagerApp : public PP_PowerPlant::LApplication, public PP_PowerPlant::LListener, public PP_PowerPlant::LPeriodical {

public:
							CKerberosManagerApp();		// constructor registers all PPobs
		virtual 			~CKerberosManagerApp();		// stub destructor
	
		// this overriding function performs application functions
		virtual void		Run();
		
		virtual Boolean		ObeyCommand(PP_PowerPlant::CommandT inCommand, void* ioParam);	
		
		void				SetupDockMenuAndHandlers();
		static pascal OSStatus DockMenuSetupHandler(EventHandlerCallRef theHandler, EventRef theEvent, void *userData);
		static pascal OSStatus DockMenuCommandHandler(EventHandlerCallRef theHandler, EventRef theEvent, void *userData);
		
		virtual		void	HandleAppleEvent(
								const AppleEvent&	inAppleEvent,
								AppleEvent&			outAEReply,
								AEDesc&				outResult,
								long				inAENumber);
								
		// this overriding function returns the status of menu items
		
		virtual void		FindCommandStatus(PP_PowerPlant::CommandT inCommand,
									Boolean &outEnabled, Boolean &outUsesMark,
									UInt16 &outMark, Str255 outName);

		//override to listen to controls
		virtual void 		ListenToMessage( PP_PowerPlant::MessageT inMessage, void *ioParam);
		
		virtual void 		SpendTime(const EventRecord&);
		
		CKrbPreferences		*GetKrbPreferencesRef();
		CKrbSession			*GetKrbSession();
		CKMMainWindow		*GetMainWindowRef();
		void 				ShowAboutBox();
		void				SetFatalFlag(Boolean inFatalFlag);

		
protected:

		void				RegisterTMClasses();
		virtual void		Initialize();
		virtual void		StartUp();		// overriding startup functions
		virtual void		DoReopenApp();
		virtual void		DoQuit( SInt32 inSaveOption );

		CKMMainWindow	*mKMMainWindow;
		CKrbPreferences	*mKrbPreferences;
		
		// need mKSession as pointer so we can check for presence of Kerb5 lib before initializing
		CKrbSession 	*mKSession; 
		
		Boolean			mFatalFlag;
		
		EventHandlerUPP	mDockMenuCommandHandler, mDockMenuSetupHandler;

private:
		void CreateMainWindow();
};
