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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/Headers/CKrbPrefsDialog.h,v 1.38 2003/07/11 19:39:52 smcguire Exp $ */

// ===========================================================================
//	CKrbPrefsDialog.h
// ===========================================================================

#pragma once

#include "CKrbPreferences.h"

PP_Using_Namespace_PowerPlant

//resource constants
// for the whole dialog
const ResIDT rPrefsDialogWindow			=		7000;
const ResIDT rPrefsOKButton				=		7001;
const ResIDT rPrefsCancelButton			=		7002;
const ResIDT rPrefsTabsXMultiPanelView		=		7051;
const ResIDT rPrefsSystemMultiPanelView		=		7055;
const ResIDT rPrefsTabsControlForX			=		7081;
const ResIDT rLoginOptionsPanel			=		7100;
const ResIDT rTicketOptionsPanel		=		7125;
const ResIDT rKMOptionsPanel			=		7150;
const ResIDT rTicketLifetimeOptionsPanel	=		7250;

// username default options
const ResIDT rBlankUsernameRadio		=		7003;
const ResIDT rDefaultUsernameRadio		=		7004;
const ResIDT rDefaultUsernameEditText	=		7005;
const ResIDT rRememberPrincipalPopupBox	=		7020;
const ResIDT rUsernameGroupbox			=		7023;
const ResIDT rPrefsRadioGroupView		=		7030;

// ticket default options
const ResIDT rDefaultForwardCheckbox	=		7006;
const ResIDT rDefaultSliderCaption		=		7007;
const ResIDT rDefaultLifetimeCaption	=		7008;
const ResIDT rDefaultLifetimeSlider		=		7009;
const ResIDT rDefaultRealmsPopupMenu	=		7010;
const ResIDT rDefaultAddresslessCheckbox	=	7012;
const ResIDT rDefaultProxiableCheckbox	=		7017;
const ResIDT rDefaultRealmsCaption		=		7013;
const ResIDT rDefaultRenewableCheckbox	=		7014;
const ResIDT rDefaultRenewableCaption	=		7015;
const ResIDT rDefaultRenewableSlider	=		7016;
const ResIDT rRememberOptionsPopupBox	=		7021;
const ResIDT rKerb5OptionsGroupbox		=		7024;

// Kerberos.app options
const ResIDT rAlwaysExpandTicketListCheckbox	=		7011;
const ResIDT rDisplayTimeInDockCheckbox			=		7039;
const ResIDT rTicketListStartupRadioLabel		= 		7040;
const ResIDT rAlwaysShowTicketListAtStartupRadio	=	7041;
const ResIDT rNeverShowTicketListAtStartupRadio		=	7042;
const ResIDT rRememberTicketListQuitStateRadio		=	7043;
const ResIDT rTicketListStartupRadioGroupView		= 	7045;
const ResIDT rKMOptionsGroupbox					=		7022;
const ResIDT rAutoRenewTicketsCheckbox			=		7044;

// ticket lifetime resources
const ResIDT rMinimumTixLifetimeDays	=		7253;
const ResIDT rMinimumTixLifetimeHours	=		7254;
const ResIDT rMinimumTixLifetimeMinutes	=		7255;
const ResIDT rMaximumTixLifetimeDays	=		7257;
const ResIDT rMaximumTixLifetimeHours	=		7258;
const ResIDT rMaximumTixLifetimeMinutes	=		7259;

const ResIDT rMinimumRenewableLifetimeDays	=		7270;
const ResIDT rMinimumRenewableLifetimeHours	=		7271;
const ResIDT rMinimumRenewableLifetimeMinutes	=	7272;
const ResIDT rMaximumRenewableLifetimeDays	=		7275;
const ResIDT rMaximumRenewableLifetimeHours	=		7276;
const ResIDT rMaximumRenewableLifetimeMinutes	=	7277;

const MessageT msg_DefaultLifetimeSlider		=	rDefaultLifetimeSlider;
const MessageT msg_DefaultRenewableSlider		=	rDefaultRenewableSlider;
const MessageT msg_DefaultRealmsPopupMenu		= 	rDefaultRealmsPopupMenu;
const MessageT msg_RememberPrincipalPopupBox	= 	rRememberPrincipalPopupBox;
const MessageT msg_RememberOptionsPopupBox		= 	rRememberOptionsPopupBox;
const MessageT msg_DefaultUsernameEditText		= 	rDefaultUsernameEditText;
const MessageT msg_PrefsTabControl				=	'TABS';

// system panel constants
enum {
	kMacOSXPrefs = 1
};

// tab panel constants
enum {
	kTicketOptionsPanel = 1,
	kUsernameOptionsPanel = 2,
	kTicketLifetimeOptionsPanelForX = 3,
	kKMOptionsPanel = 4
};

//menu constants
enum {
	kPrefsRememberPrincipal = 1,
	kPrefsDefaultPrincipal
};

enum {
	kPrefsRememberOptions = 1,
	kPrefsDefaultOptions
};

class CKrbPrefsDialog : public LDialogBox {

	public:
		enum {class_ID 	= FOUR_CHAR_CODE('PrfD')};
		
					CKrbPrefsDialog(LStream *inStream);
					~CKrbPrefsDialog();
	
		virtual void 	ListenToMessage(MessageT inMessage, void *ioParam);
		virtual void	FindCommandStatus( CommandT	inCommand,
										Boolean		&outEnabled,
										Boolean&	/* outUsesMark */ ,
										UInt16&		/* outMark */,
										Str255		/* outName */ );
		virtual Boolean	ObeyCommand( CommandT inCommand, void *ioParam);

		virtual void 	FinishCreateSelf();

		Boolean		AllowClose();
		
		void		CopyPrefsToDialogSettings();
		void		CopyRealmsListToPopupMenu();
		void		CopyDialogSettingsToPrefs();
		
	private:
	
		void 	LinkToBroadcasters (LPane*	inPane);
		void	SetControlStatus();
		void 	SetSliderCaption(ResIDT captionToSetID, SInt32 inSeconds, UInt32 *outDays,  UInt32 *outHours,
								 UInt32 *outMinutes, UInt32 *outSeconds);
		void	SetSliderRangeAndIncrement(ResIDT sliderToSetID, UInt32 inMinimum, UInt32 inMaximum,
											UInt32 inDefaultValue, UInt32 *outIncrement);
		Boolean	ValidateTimeRangeValues();
		
		CKrbPreferences *mKrbPrefsRef;
		Boolean mOkayToClose;
		UInt32 mLifetimeSliderIncrement, mRenewableSliderIncrement;
										
};

