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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/Headers/CBSDAttachment.h,v 1.4 2002/04/10 20:29:41 smcguire Exp $ */

// ===========================================================================
//	CBSDAttachment.cp			MIT IS MacDev
// ===========================================================================

/* Monitor events and detect option-bsd and option-mac sequences */
   
#include <LAttachment.h>

class CBSDAttachment:
	public PP_PowerPlant::LAttachment
{
	public:
						CBSDAttachment (
							PP_PowerPlant::LStream*			inStream);
							
						CBSDAttachment (
							PP_PowerPlant::MessageT	inMessage,
							Boolean		inExecuteHost);
	
						CBSDAttachment ();
		
		virtual	void	ExecuteSelf (
							PP_PowerPlant::MessageT			inMessage,
							void*				ioParam);
							
		enum {
			msg_BSDLayout = FOUR_CHAR_CODE ('bsdl'),
                        msg_MooLayout = FOUR_CHAR_CODE ('Mool'),
			msg_MacLayout = FOUR_CHAR_CODE ('Macl')
		};

	private:

		enum EState {
			state_B,
			state_BS,
			state_BSD,
			state_M,
			state_Ma,
			state_Mac,
                        state_Mo,
                        state_Moo
		};
		
		enum {
			char_OptionB = FOUR_CHAR_CODE ('\0\0\0∫'),
			char_OptionS = FOUR_CHAR_CODE ('\0\0\0ß'),
			char_OptionD = FOUR_CHAR_CODE ('\0\0\0∂'),
			char_OptionM = FOUR_CHAR_CODE ('\0\0\0µ'),
			char_OptionA = FOUR_CHAR_CODE ('\0\0\0å'),
			char_OptionC = FOUR_CHAR_CODE ('\0\0\0ç'),
                        char_OptionO = FOUR_CHAR_CODE ('\0\0\0ø')
		};
	
	
		EState		mState;
		PP_PowerPlant::MessageT	mLayout;
		
		void		SetState (
							EState				inNewState);
		
		EState		GetState () const;
		
		void		ResetState ();
		
		void		SetLayout (
							PP_PowerPlant::MessageT			inLayout);
};