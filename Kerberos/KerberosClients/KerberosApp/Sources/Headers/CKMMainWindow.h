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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/Headers/CKMMainWindow.h,v 1.16 2002/04/10 20:29:45 smcguire Exp $ */

// =================================================================================
//	CKMMainWindow.h
// =================================================================================
/*
	Minor subclass of LWindow, just so we can put in a hook to set the preferences
	when the window is moved.
*/

#pragma once

#include <LWindow.h>

#include "CPrincList.h"

enum {
	kTicketsVaildIcon = 200,
	kTicketsWarningIcon,
	kTicketsExpiredIcon
};

class CKMMainWindow : public PP_PowerPlant::LWindow, public PP_PowerPlant::LListener, public PP_PowerPlant::LPeriodical {

	public:
	
		enum {class_ID = FOUR_CHAR_CODE('tmMW')};
		
				CKMMainWindow(PP_PowerPlant::LStream*	inStream);
				
		virtual	~CKMMainWindow();

		virtual	void		FinishCreateSelf();
		
		virtual Boolean 	ObeyCommand(PP_PowerPlant::CommandT	inCommand, void		*ioParam);
		virtual void 		FindCommandStatus(
								PP_PowerPlant::CommandT	inCommand,
								Boolean		&outEnabled,
								Boolean		&outUsesMark,
								UInt16		&outMark,
								Str255		outName);
		virtual	void		ListenToMessage(PP_PowerPlant::MessageT inMessage, void *ioParam);

		virtual void 		SpendTime(const EventRecord&);
		
		virtual	void 		ClickInDrag(const EventRecord	&inMacEvent);
		virtual Boolean		HandleKeyPress(const EventRecord& inKeyEvent);
		virtual void 		DoSetBounds(const Rect&		inBounds);
		virtual void		AttemptClose();
		virtual void		DoClose();

		void 				UpdateActiveUserInfo(Boolean forceUpdate);
		void				UpdateUsersMenu();
		void				UpdateDockIcon(short iconToDraw, char *timeRemainingString);
		CPrincList*			GetPrincListRef();
		
protected:
		void 				FixButtonStatus();
		void 				StandardButtonNames();
		void				MooButtonNames();
		void				BSDButtonNames();

private:
		CPrincList		*mKPrincList;
		CKrbSession 	*mKSessionRef;
		CKrbPreferences	*mKrbPrefsRef;

		UInt16	mInitialWindowWidth;
		UInt16	mActiveUserTextFieldsRightOffset;
		PP_PowerPlant::SPoint32 mTitleStringsInitialPosition;
		PP_PowerPlant::SPoint32 mIconInitialPosition;
		PP_PowerPlant::SPoint32 mRenewButtonInitialPosition;
		PP_PowerPlant::SPoint32 mDestroyTicketsButtonInitialPosition;
		PP_PowerPlant::SPoint32 mChangePasswordButtonInitialPosition;
		PP_PowerPlant::SPoint32 mActiveUserLabelsInitialPosition;
		PP_PowerPlant::SPoint32 mActiveUserTextFieldsInitialPosition;
};
