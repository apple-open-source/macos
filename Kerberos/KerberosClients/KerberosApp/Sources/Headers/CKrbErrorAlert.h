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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/Headers/CKrbErrorAlert.h,v 1.8 2002/04/09 19:45:54 smcguire Exp $ */

#pragma once
// ===========================================================================
//	CKrbErrorAlert.h
// ===========================================================================
/* This class is intended to be a generic interface for providing error information
	to the user. It will display a dialog containing one or two strings and one or
	two buttons. Errors may be classified as fatal, in which case the application will
	be killed */
	
const PP_PowerPlant::ResIDT	res_IconID	=	131;
const PP_PowerPlant::ResIDT	res_AlertID	=	4000;
const PP_PowerPlant::ResIDT	res_MajorTextID	 =	4002;
const PP_PowerPlant::ResIDT	res_MinorTextID		=	4003;
const PP_PowerPlant::ResIDT	res_DefaultButtonID	=	4004;
const PP_PowerPlant::ResIDT	res_OptionalButtonID	=	4005;
	
const short		kDefaultButton	=	1;
const short		kOtherButton	=	2;
const short		kErrorCreatingDialog = 3;

class CKrbErrorAlert {

	public:
		
		//blank constructor
		CKrbErrorAlert();
		
		//full constructor
		CKrbErrorAlert(char *majorString, char *minorString, char *buttonText, 
						char *otherButtonText,  bool fatal);
						
		//destructor
		~CKrbErrorAlert();
		
		
		//display method
		//returns one if the main button was used to dismiss the dialog, two otherwise
		PP_PowerPlant::MessageT DisplayErrorAlert();
		
	protected:
	
		//pstrings for all of the constructor args
		Str255 mMajorString;
		Str255 mMinorString;
		Str255 mButtonText;
		Str255 mOtherButtonText;
		bool   mFatalError;
		
};
