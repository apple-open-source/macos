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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/Headers/CStatusDialog.h,v 1.7 2002/04/09 19:46:11 smcguire Exp $ */

// ===========================================================================
//	CLaunchTKDialog.h		MIT i/s
// ===========================================================================

#pragma once

#include <LDialogBox.h>

//resource constants
const PP_PowerPlant::ResIDT rStatusDialog		=		5500;
const PP_PowerPlant::ResIDT rStatusText		=		5501;

class CStatusDialog : public PP_PowerPlant::LDialogBox {

	public:
		enum {class_ID 	= FOUR_CHAR_CODE('lTKD')};
		
					CStatusDialog(PP_PowerPlant::LStream *inStream);
					~CStatusDialog();
	
		virtual void 	ListenToMessage(PP_PowerPlant::MessageT inMessage, void *ioParam);
		virtual void	FindCommandStatus(PP_PowerPlant::CommandT	inCommand,
										Boolean		&outEnabled,
										Boolean&	/* outUsesMark */ ,
										UInt16&		/* outMark */,
										Str255		/* outName */ );
		virtual void 	FinishCreateSelf();
				void	SetStatusText(Str255 inStatusMessage);		
};

