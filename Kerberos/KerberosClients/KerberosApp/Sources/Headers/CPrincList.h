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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/Headers/CPrincList.h,v 1.28 2003/06/10 21:16:44 smcguire Exp $ */

// =================================================================================
//	CPrincList.h
// =================================================================================

#pragma once

extern "C" {
#if TARGET_RT_MAC_CFM
#include <CredentialsCache/CredentialsCache.h>
#else
#include <Kerberos/Kerberos.h>
#endif
}

#include <LBroadcaster.h>
#include <LCommander.h>
#include <LHierarchyTable.h>
#include <UNavigableTable.h>
#include <LPeriodical.h>

#include "CTicketListItem.h"
#include "CKrbSession.h"
#include "CKrbPreferences.h"


//constants
const int kCellHeight = 16;
const int kPrincHOffset = 18;
const int kSubPrincHOffset = 28;
const int kCredHOffset = 38;

class CPrincList : public PP_PowerPlant::LHierarchyTable, public PP_PowerPlant::LPeriodical,
                   public PP_PowerPlant::LBroadcaster, public PP_PowerPlant::LListener, public PP_PowerPlant::LCommander {

	public:
	
		enum {class_ID = FOUR_CHAR_CODE('pLst')};
		
		//constructor
							CPrincList(PP_PowerPlant::LStream *inStream);
							
		//destructor
		virtual				~CPrincList();
		
		// override methods
		virtual void			RemoveAllRows(Boolean inRefresh);
		
		// new methods
		virtual void			UpdateListFromCache();
		bool 				GetItemFromCell(const PP_PowerPlant::STableCell &inCell, CTicketListItem **outItem);
		Boolean				GetSelectedPrincipal(Str255 outPrinc, cc_int32 *outVersion);
		Boolean				GetActivePrincipal(Str255 outPrinc, cc_int32 *outVersion);
		PP_PowerPlant::LArray *		GetAllPrincipals();
		void				CheckPrincipalsForAutoRenewal();
		short				GetPrincipalCount();
		bool				PrincipalInList(char *princ);
		bool 				GetAnotherPrincipal(Str255 outPrinc, Str255 princToIgnore);
	protected:
		
		//fields
		bool fDirty;
		
		//more methods
		virtual void		DrawCell( const PP_PowerPlant::STableCell &inCell,
								const Rect &inLocalRect );

		void 				ResizeFrameBy(
								SInt16		inWidthDelta,
								SInt16		inHeightDelta,
								Boolean		inRefresh);

		virtual void		ClickCell( const PP_PowerPlant::STableCell &inCell,
								const PP_PowerPlant::SMouseDownEvent &inMouseDown );

		void				ClickSelf(const PP_PowerPlant::SMouseDownEvent	&inMouseDown);
		
		virtual void 		SpendTime( const EventRecord &inMacEvent);
		
		void				ListenToMessage(PP_PowerPlant::MessageT inMessage, void *ioParam);

	virtual void		HiliteCellActively(
								const PP_PowerPlant::STableCell		&inCell,
								Boolean					inHilite);
	virtual void		HiliteCellInactively(
								const PP_PowerPlant::STableCell		&inCell,
								Boolean					inHilite);
	private:
		
		void				InitList();
		
		//private fields
		CKrbPreferences		*mKrbPrefsRef;
		CKrbSession		*mKrbSession;
		RGBColor		mForeColor, mBackColor;
		
	};
