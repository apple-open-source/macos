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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/Headers/CKrbSession.h,v 1.44 2003/06/12 15:47:40 smcguire Exp $ */

// ===========================================================================
//	CKrbSession.h
// ===========================================================================

#pragma once

#include <string>

#include <LCommander.h>
#include <LListener.h>
#include <LModelObject.h>
#include <LPeriodical.h>
#include <LVariableArray.h>
#include <LWindow.h>
#include "CKrbPreferences.h"

#if TARGET_RT_MAC_CFM
extern "C" {
#include <CredentialsCache/CredentialsCache.h>
}
#else
#include <Kerberos/Kerberos.h>
#endif

#define _MACINTOSH
#if TARGET_RT_MAC_CFM
#include <Kerberos5/Kerberos5.h>
#include <KerberosWrappers/UKerberos5Context.h>	// from KrbWrappersLib
#else
#include "UKerberos5Context.h"
#endif

const PP_PowerPlant::ResIDT	res_CruftyPWD		=	6000;
const PP_PowerPlant::ResIDT	res_PasswordEdit	=	6101;
const int			err_PowerPlant		=	128;

class CKrbSession : public PP_PowerPlant::LListener, public PP_PowerPlant::LPeriodical {

	public:
	
							CKrbSession();
		virtual				~CKrbSession();
		
				void	 	DoLogin();
				void 		DoRenew(Str255 inPrinc, cc_int32 inPrincVersion);
				Boolean		DoAutoRenew(Str255 inPrinc, cc_int32 inPrincVersion);
				void 		DoLogout(char *princ, cc_int32 inPrincVersion);
		
		virtual	void		ListenToMessage( PP_PowerPlant::MessageT inMessage, void *ioParam); 
				void		SpendTime(const EventRecord &inMacEvent);

				PP_PowerPlant::LArray*		GetCacheInfo();
				
		//check the access time on the credentials cache to see if
		//we need to update
				bool 		CCacheChanged();
				void		SetCacheDirty(Boolean inDirty);
				void		SetPrincipalAsDefault(char *inPrincName);
				void		GetActiveUserInfo(Str255 outPrincipal, long &outTimeTillExp, Str255 outRealm, short &outValidity);
				void		SetActiveUserInfo(const char *newFullPrinc, UInt32 newExpTime, short cacheValidity);
				void		ChangePassword(Str255 inPrinc, cc_int32 inPrincVersion);
				bool		FindPrincipalInCache(char *princ, std::string *outCacheName);

	protected:
		CKrbPreferences *mKrbPrefsRef;
		
		Str255 mActiveUserPrincipal;
		Str255 mActiveUserRealm;
		UInt32 mActiveUserExpTime;
		short  mActiveUserValidity;
		
		bool mCacheDirty;
		UInt32 mLastChangeTime;

		/* This K5Contest is here in order to solve a sleep bug - the repeated call
		to KLLastChangedTime() in SpendTime() when no other context was being used
		caused a continual disk hit which would cause KM to prevent the Mac from
		going to sleep.  Having a context available prevents this, thanks to the Miro
		profile optimizations. */

		cc_context_t mCCContext;		// our CCache context

		//ticket info fields
		Str255 mUsername;
		Str255 mPassword;
		long mLifetime;
		bool mForwardable;
		
		void SetActiveUserFromCache(void);
		
	};
