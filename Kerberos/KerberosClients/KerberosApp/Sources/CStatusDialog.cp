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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/CStatusDialog.cp,v 1.8 2002/04/09 19:46:09 smcguire Exp $ */

// ===========================================================================
//	CStatusDialog.c
// ===========================================================================

#include "CStatusDialog.h"

#include <LStaticText.h>

PP_Using_Namespace_PowerPlant

CStatusDialog::CStatusDialog(LStream *inStream) : LDialogBox(inStream) {
}
	
	
CStatusDialog::~CStatusDialog() {
}
	
void
CStatusDialog::ListenToMessage(MessageT inMessage, void *ioParam)  {

	switch (inMessage) {
		default:
			LDialogBox::ListenToMessage(inMessage, ioParam);
			break;
	}
}

void CStatusDialog::FindCommandStatus(
	CommandT	inCommand,
	Boolean		&outEnabled,
	Boolean&	/* outUsesMark */ ,
	UInt16&		/* outMark */,
	Str255		/* outName */ )
{
	// Don't enable any commands except cmd_About, which will keep
	// the Apple menu enabled. This function purposely does not
	// call the inherited FindCommandStatus, thereby suppressing
	// commands that are handled by SuperCommanders. Only those
	// commands enabled by SubCommanders will be active.
	//
	// This is usually what you want for a movable modal dialog.
	// Commands such as "New", "Open" and "Quit" that are handled
	// by the Application are disabled, but items within the dialog
	// can enable commands. For example, an edit field would enable
	// items in the "Edit" menu.
	
	// Disable all commands.
	outEnabled = false;
	
	if ( inCommand == cmd_About ) {
	
		// Enable the about command.
		outEnabled = true;

	}
}

void CStatusDialog::FinishCreateSelf()  {
	// Link the dialog to the controls.
	UReanimator::LinkListenerToControls( this, this, rStatusDialog );
}

void CStatusDialog::SetStatusText(Str255 inStatusMessage)
{
	LStaticText *staticText;
	
	staticText = dynamic_cast<LStaticText *>(this->FindPaneByID(rStatusText));
	Assert_(staticText != NULL);
	staticText->SetDescriptor(inStatusMessage); //update the new user
	
	//force text to redraw now (otherwise PP waits until we get ouf of Modal dialog!)
	Rect frame;
	staticText->FocusDraw();
	staticText->CalcLocalFrameRect(frame);
	staticText->ApplyForeAndBackColors();
	::EraseRect(&frame);
	staticText->Draw(nil);
}
