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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/CBSDAttachment.cp,v 1.4 2002/04/10 20:29:39 smcguire Exp $ */

#include "CBSDAttachment.h"

PP_Using_Namespace_PowerPlant

CBSDAttachment::CBSDAttachment (
	LStream*			inStream):
	
	LAttachment (inStream),
	mState (state_Mac),
	mLayout (msg_MacLayout)
{
}

CBSDAttachment::CBSDAttachment (
	MessageT	inMessage,
	Boolean		inExecuteHost):
	
	LAttachment (inMessage, inExecuteHost),
	mState (state_Mac),
	mLayout (msg_MacLayout)
	
{
}

CBSDAttachment::CBSDAttachment ():
	
	mState (state_Mac),
	mLayout (msg_MacLayout)
{
}

void CBSDAttachment::ExecuteSelf (
	MessageT			inMessage,
	void*				ioParam)
{
	if (inMessage == msg_KeyPress) {
		EventRecord*	event = static_cast <EventRecord*> (ioParam);

		// We only care about keydown, autokey, and mosedown events)
		switch (event -> what) {
			// Mouse down always resets state
			case mouseDown:
				ResetState ();
				break;

			// Key events change state				
			case keyDown:
			case autoKey:
				// If option key is not down, reset state
				if ((event -> modifiers & optionKey) != 0) {
					UInt32	keyChar = event -> message & charCodeMask;

					if (keyChar == char_OptionB) {
						SetState (state_B);
					} else if ((GetState () == state_B) && (keyChar == char_OptionS)) {
						SetState (state_BS);
					} else if ((GetState () == state_BS) && (keyChar == char_OptionD)) {
						SetState (state_BSD);
					} else if (keyChar == char_OptionM) {
						SetState (state_M);
					} else if ((GetState () == state_M) && (keyChar == char_OptionA)) {
						SetState (state_Ma);
					} else if ((GetState () == state_Ma) && (keyChar == char_OptionC)) {
						SetState (state_Mac);
					} else if ((GetState () == state_M) && (keyChar == char_OptionO)) {
						SetState (state_Mo);
					} else if ((GetState () == state_Mo) && (keyChar == char_OptionO)) {
						SetState (state_Moo);
					} else {
						ResetState ();
					}
				} else {
					ResetState ();
				}
				break;
		}
	}
}				
				
void CBSDAttachment::SetState (
	EState		inNewState)
{
	mState = inNewState;
	
	if (mState == state_BSD) {
		SetLayout (msg_BSDLayout);
	} else if (mState == state_Moo) {
		SetLayout (msg_MooLayout);
	} else if (mState == state_Mac) {
		SetLayout (msg_MacLayout);
	}
}

CBSDAttachment::EState CBSDAttachment::GetState () const
{
	return mState;
}

void CBSDAttachment::SetLayout (
	MessageT		inLayout)
{
	if (inLayout != mLayout) {
		LListener*		listener = dynamic_cast <LListener*> (GetOwnerHost ());
		if (listener != nil) {
			listener -> ListenToMessage (inLayout, nil);
		}
		mLayout = inLayout;
	}
}

void CBSDAttachment::ResetState ()
{
	SetLayout (mLayout);
	switch (mLayout) {
		case msg_BSDLayout:
			SetState (state_BSD);
			break;
		
		case msg_MooLayout:
			SetState (state_Moo);
			break;

		case msg_MacLayout:
			SetState (state_Mac);
			break;
	}
}
