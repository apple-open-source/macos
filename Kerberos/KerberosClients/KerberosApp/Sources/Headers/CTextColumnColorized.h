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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/Headers/CTextColumnColorized.h,v 1.14 2003/05/09 21:12:48 smcguire Exp $ */

// =================================================================================
//	CTextColumnColorized.h
// =================================================================================
/* Same as LTextColumn, but lets you set, get, and apply different fore and background
   colors.  This is good when you want a white list on a grey Appearance window.
*/

#pragma once

#include <LTextColumn.h>

class CTextColumnColorized : public PP_PowerPlant::LTextColumn, public PP_PowerPlant::LCommander {

	public:
	
		enum {class_ID = FOUR_CHAR_CODE('txcC')};
		
							CTextColumnColorized(PP_PowerPlant::LStream *inStream);
							
		virtual				~CTextColumnColorized();
		
		virtual void		ClickCell(
									const PP_PowerPlant::STableCell&		inCell,
									const PP_PowerPlant::SMouseDownEvent&	inMouseDown);
								
		virtual void		HiliteCellActively(
									const PP_PowerPlant::STableCell		&inCell,
									Boolean					inHilite);
		virtual void		HiliteCellInactively(
									const PP_PowerPlant::STableCell		&inCell,
									Boolean					inHilite);

		virtual void		DrawSelf();

		virtual Boolean		ItemIsAcceptable( DragReference inDragRef,
								ItemReference inItemRef );
		virtual void		ReceiveDragItem( DragReference inDragRef,
							DragAttributes inDragAttrs, ItemReference inItemRef,
							Rect &inItemBounds );
		virtual void		EnterDropArea( DragReference inDragRef, Boolean inDragHasLeftSender );
		virtual void		LeaveDropArea( DragReference inDragRef );
		virtual void		InsideDropArea( DragReference inDragRef);
		//virtual void		HiliteDropArea( DragReference inDragRef );

		void				GetDividingLineGivenPoint( const Point &inPortPoint,
								PP_PowerPlant::TableIndexT &outRow );
		void				DrawDividingLine( PP_PowerPlant::TableIndexT inRow );
		Boolean 			DroppedInTrash( DragReference	inDragRef );
		
		Boolean				GetAllowDrops();
		void				SetAllowDrops(Boolean inOption);

	private:
		
		RGBColor 		mForeColor, mBackColor;
		PP_PowerPlant::TableIndexT		mDropRow;
		Boolean			mAllowDrops; // does this table accept drag and drop information?

};
