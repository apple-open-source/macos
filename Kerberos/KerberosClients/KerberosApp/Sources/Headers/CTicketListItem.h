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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/Headers/CTicketListItem.h,v 1.13 2003/06/10 21:12:03 smcguire Exp $ */

// =================================================================================
//	CTicketListItem.h
// =================================================================================

#pragma once

#include <LString.h>

#if TARGET_RT_MAC_CFM
	#include <CredentialsCache/CredentialsCache.h>
	#include <KerberosWrappers/UCCache.h>
#else
	#include <Kerberos/Kerberos.h>
	#include "UCCache.h"
#endif

//text traits
const PP_PowerPlant::ResIDT	rGeneva		= 130;
const PP_PowerPlant::ResIDT	rGenevaBold	= 132;
const PP_PowerPlant::ResIDT	rGenevaBoldUnderline	= 134;
const PP_PowerPlant::ResIDT	rGenevaItalic	= 143;
const PP_PowerPlant::ResIDT	rGenevaBoldItalic	= 144;
const PP_PowerPlant::ResIDT	rGenevaBoldItalicUnderline	= 145;

const PP_PowerPlant::ResIDT rTicketItemRegular = 148;
const PP_PowerPlant::ResIDT rTicketItemItalic = 149;
const PP_PowerPlant::ResIDT rTicketItemBold = 150;
const PP_PowerPlant::ResIDT rTicketItemBoldItalic = 151;
const PP_PowerPlant::ResIDT rTicketItemBoldUnderline = 152;
const PP_PowerPlant::ResIDT rTicketItemBoldItalicUnderline = 153;
const PP_PowerPlant::ResIDT rTicketItemRegularRed = 154;
const PP_PowerPlant::ResIDT rTicketItemItalicRed = 155;




enum {
	kUndefinedItem = 1,		// initial state - you shouldn't have one of these if you use subclasses!
	kPrincipalItem = 2,		// overall principal entry for a set of creds, not the cache collection default
	kCredentialsItem = 3,	// a credentials entry
	kSubPrincipalItem = 4	// the v4 of v5 representation of the principal
};

// used by Principal and SubPrincipal subclasses
enum {
	cc_credentials_no_version = 0
};

enum {
	kTicketValid = 1,				// ticket is valid (but may still be expired)
	kTicketInvalidBadAddress = 2,	// ticket is invalid because of IP address change
	kTicketInvalidNeedsValidation = 3,
	kTicketInvalidUnknown = 4		// ticket is invalid for some other unknown reason
};


class CTicketListItem {

	public:
	
		virtual			~CTicketListItem();
		
				short	GetItemType() { return mTicketItemType; }
		
				PP_PowerPlant::ResIDT	GetItemTextTrait() { return mTicketItemTextTrait; }
				PP_PowerPlant::ResIDT	GetItemExpiredTextTrait() { return mTicketItemExpiredTextTrait; }
		
				short	GetItemDrawOffset() { return mTicketItemDrawOffset; }
		
				void	GetItemPrincipalString (Str255 *outPrincipalString);
				void	SetItemPrincipalString (PP_PowerPlant::LStr255 inPrincipalString);
		
				void	GetItemDisplayString (Str255 *outDisplayString);
				void	SetItemDisplayString (PP_PowerPlant::LStr255 inDisplayString);
		
				unsigned long GetItemStartTime ();
				void	SetItemStartTime (unsigned long inPrincipalString);
		
				unsigned long GetItemExpirationTime ();
				void	SetItemExpirationTime (unsigned long inPrincipalString);
		
				short	GetItemValidity ();
				void	SetItemValidity (short inItemValidity);
		
				cc_int32 GetKerberosVersion();
				void	SetKerberosVersion(cc_int32 inVersion);
				
				Boolean GetItemIsExpanded();
				void	SetItemIsExpanded(const Boolean &inIsExpanded);
				
				Boolean	GetItemIsSelected();
				void	SetItemIsSelected(const Boolean &inIsSelected);

		virtual	Boolean	IsTicketForwardable();
		
		virtual	Boolean IsTicketProxiable();

		virtual	Boolean IsTicketRenewable();
		
		virtual Boolean	EquivalentItem(CTicketListItem *comparisonItem) = 0;
		// doesn't test for full equality, just if they're close enough to be considered the same
		// some flags may be different
		
	protected:
				
					CTicketListItem(
								short inItemType,
								PP_PowerPlant::ResIDT inTextTrait,
								PP_PowerPlant::ResIDT inExpiredTextTrait,
								short inDrawOffset,
								cc_int32 inKerberosVersion,
								PP_PowerPlant::LStr255 inPrincipalString,
								PP_PowerPlant::LStr255 inDisplayString,
								unsigned long inStartTime,
								unsigned long inExpirationTime,
								short inItemValidity = kTicketValid);
									
		short					mTicketItemType;			// type of item: principal or credential
		PP_PowerPlant::ResIDT			mTicketItemTextTrait;		// font and style to be used to draw the item
		PP_PowerPlant::ResIDT			mTicketItemExpiredTextTrait;	// font and style to be used to draw the item
		short					mTicketItemDrawOffset;		// horizontal offset to indent when drawing item, in pixels
		PP_PowerPlant::LStr255			mTicketItemPrincipalString;	// item's "real" principal (usually principal or service in "bigstring" format)
		PP_PowerPlant::LStr255			mTicketItemDisplayString;	// text display of item (usually principal or service in "bigstring" format)
													// stripped of any escape characters
		unsigned long	mTicketItemStartTime;		// absolute start time as seconds from 1904
		unsigned long	mTicketItemExpirationTime;	// absolute expiration time as seconds from 1904
		short			mTicketItemValidity;		// whether the ticket is valid or not
		cc_int32 		mKerberosVersion;			// version of credentials of this item - can be v4, v5, or both
		Boolean			mItemIsExpanded;			// flag for whether this ticket item is expanded or collapsed
		Boolean			mItemIsSelected;			// flag for whether this ticket item is selected
	
	private:
		CTicketListItem();
		CTicketListItem (const CTicketListItem &);
		CTicketListItem& operator =(const CTicketListItem &);
};
