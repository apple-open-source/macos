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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/Headers/CEditRealmsDialog.h,v 1.17 2003/05/07 21:38:41 smcguire Exp $ */

// ===========================================================================
//	CEditRealmsDialog.h
// ===========================================================================

#pragma once

#include "CKrbPreferences.h"
#include "CTextColumnColorized.h"

//resource constants for Edit Realms List Dialog
const PP_PowerPlant::ResIDT rEditRealmsDialogWindow		=		2500;
const PP_PowerPlant::ResIDT rEditRealmsFavScroller		=		2501;
const PP_PowerPlant::ResIDT rEditRealmsFavTextColumn	=		2502;
const PP_PowerPlant::ResIDT rEditRealmsAddButton		=		2503;
const PP_PowerPlant::ResIDT rEditRealmsAddAllButton		=		2504;
const PP_PowerPlant::ResIDT rEditRealmsRemoveButton		=		2505;
const PP_PowerPlant::ResIDT rEditRealmsCancelButton		=		2506;
const PP_PowerPlant::ResIDT rEditRealmsDoneButton		=		2507;

const PP_PowerPlant::ResIDT rEditRealmsExtraRealmField		=	2510;
const PP_PowerPlant::ResIDT rEditRealmsExtraRealmAddButton	=	2511;

const PP_PowerPlant::ResIDT rEditRealmsAllScroller		=		2521;
const PP_PowerPlant::ResIDT rEditRealmsAllTextColumn	=		2522;


//message constants for Edit Realms List Dialog
const PP_PowerPlant::MessageT msg_EditRealmsFavTextColumn		=		rEditRealmsFavTextColumn;
const PP_PowerPlant::MessageT msg_EditRealmsAllTextColumn		=		rEditRealmsAllTextColumn;
const PP_PowerPlant::MessageT msg_EditRealmsAddButton			=		rEditRealmsAddButton;
const PP_PowerPlant::MessageT msg_EditRealmsAddAllButton		=		rEditRealmsAddAllButton;
const PP_PowerPlant::MessageT msg_EditRealmsRemoveButton		=		rEditRealmsRemoveButton;

const PP_PowerPlant::MessageT msg_rEditRealmsExtraRealmField		=	rEditRealmsExtraRealmField;
const PP_PowerPlant::MessageT msg_rEditRealmsExtraRealmAddButton	=	rEditRealmsExtraRealmAddButton;

const PP_PowerPlant::MessageT msg_EditRealms_FavSelect			=	2550;
const PP_PowerPlant::MessageT msg_EditRealms_FavDoubleClick	=	2551;
const PP_PowerPlant::MessageT msg_EditRealms_AllSelect			=	2552;
const PP_PowerPlant::MessageT msg_EditRealms_AllDoubleClick	=	2553;

const PP_PowerPlant::MessageT msg_EditRealms_OK			=	2560;
const PP_PowerPlant::MessageT msg_EditRealms_Cancel		=	2561;


//resource constants for Add/Edit Realm Dialog
const PP_PowerPlant::ResIDT rAddRealmDialog	=		2600;

const PP_PowerPlant::ResIDT rAddRealmAddButton		=		2601;
const PP_PowerPlant::ResIDT rAddRealmCancelButton	=		2602;
const PP_PowerPlant::ResIDT rAddRealmCaptionText	=		2603;
const PP_PowerPlant::ResIDT rAddRealmEditText		=		2604;

const PP_PowerPlant::MessageT msg_AddRealmEditText	= 	rAddRealmEditText;

class CEditRealmsDialog : public PP_PowerPlant::LDialogBox, public PP_PowerPlant::LBroadcaster {

	public:
		enum {class_ID 	= FOUR_CHAR_CODE('RlmD')};
		
		CEditRealmsDialog(PP_PowerPlant::LStream *inStream);
		~CEditRealmsDialog();
		
		// override functions
		virtual Boolean HandleKeyPress(const EventRecord&	inKeyEvent);
		virtual void ListenToMessage(PP_PowerPlant::MessageT inMessage, void *ioParam);
		virtual void FinishCreateSelf();
		virtual void FindCommandStatus( PP_PowerPlant::CommandT	inCommand,
										Boolean		&outEnabled,
										Boolean&	/* outUsesMark */ ,
										UInt16&		/* outMark */,
										Str255		/* outName */ );
		
		// new to this subclass
		void FillAllRealmsTable();
		void CopyPrefsToRealmsDialog();
		void CopyRealmsDialogToPrefs();
		void SetButtonStatus();
		void AddSingleRealm();
		void AddAllRealms();
		void AddExtraRealm();
		void RemoveSingleRealm();
	
	protected:
		CKrbPreferences *mKrbPrefsRef;
		CTextColumnColorized *mFavRealmsTable, *mAllRealmsTable;
};

