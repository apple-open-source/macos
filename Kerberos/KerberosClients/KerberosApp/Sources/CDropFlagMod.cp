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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/CDropFlagMod.cp,v 1.11 2002/04/09 19:45:36 smcguire Exp $ */

// ===========================================================================
//	CDropFlagMod.cp
// ===========================================================================
//
//	Draws and tracks clicks for a drop flag.
//
//  This override makes sure the drop flag icon draws properly on the correct background.
//  Still needed with PowerPlant 2.2 because while they fixed the "white background" problem
//  with tables, they didn't fix this right.

#include <Icons.h>

//	Icon family resource ID's for the states of the drop flag
//	The size of the rectangle you pass to Draw and TrackClick determine
//	which icon size gets used.
//		Large Icon		32 x 32
//		Small Icon		16 x 16
//		Mini Icon		16 x 12

#include <UEventMgr.h>
#include <UGWorld.h>

#include "CDropFlagMod.h"

PP_Using_Namespace_PowerPlant

const ResIDT	icon_Up			= 2101;
const ResIDT	icon_UpDark		= icon_Up + 1;
const ResIDT	icon_Side		= 2103;
const ResIDT	icon_Down		= 2111;
const ResIDT	icon_DownDark	= icon_Down + 1;

const SInt32		delay_Animation	= 4;



// ---------------------------------------------------------------------------
//	¥ TrackClick
// ---------------------------------------------------------------------------
//	Track the mouse after an initial click inside a LDropFlag. This
//	functions draws the hilited and intermediate states of the flag
//	as necessary, so the current port must be set up properly.
//
//	inRect specifies the location of the DropFlag
//	inMouse is the mouse location (usually from a MouseDown EventRecord)
//		in local coordinates
//	inIsUp specifies if the flag was up or down when the click started
//
//	Returns whether the mouse was release inside the DropFlag.

Boolean
CDropFlagMod::TrackClick(
	const Rect	&inRect,
	const Point	&inMouse,
	Boolean		inIsDown,
	RGBColor	*inBackColor)
{
	Boolean		goodClick = false;
	
	if (!::PtInRect(inMouse, &inRect)) {
		return false;
	}
	
	ResIDT		iconID = icon_Up;
	if (inIsDown) {
		iconID = icon_Down;
	}
	
									// For the initial mouse down, the
									// mouse is currently inside the HotSpot
									// when it was previously outside
	Boolean		currInside = true;
	Boolean		prevInside = false;
	::PlotIconID(&inRect, atNone, ttNone, (SInt16) (iconID + 1));
	
									// Track the mouse while it is down
	Point	currPt = inMouse;
	while (::StillDown()) {
		::GetMouse(&currPt);		// Must keep track if mouse moves from
		prevInside = currInside;	// In-to-Out or Out-To-In
		currInside = PtInRect(currPt, &inRect);
		
		if (prevInside != currInside) {
			ResIDT	trackIconID = iconID;
			if (currInside) {
				trackIconID = (SInt16) (iconID + 1);
			}
			::PlotIconID(&inRect, atNone, ttNone, trackIconID);
		}
	}
	
	EventRecord	macEvent;			// Get location from MouseUp event
	if (UEventMgr::GetMouseUp(macEvent)) {
		currPt = macEvent.where;
		::GlobalToLocal(&currPt);
	}
									// Check if MouseUp occurred in HotSpot
	goodClick = ::MacPtInRect(currPt, &inRect);
	
	if (goodClick) {
		UInt32	ticks;				// Draw intermediate state
		::Delay(delay_Animation, &ticks);
		
		{
			StOffscreenGWorld	offWorld(inRect, 0, 0, nil, nil, inBackColor);
			::PlotIconID(&inRect, atNone, ttNone, icon_Side);
		}
		
		::Delay(delay_Animation, &ticks);
		
									// Draw dark end state
		ResIDT	endIconID = icon_Down + 1;
		if (inIsDown) {
			endIconID = icon_Up + 1;
		}
		
		{
			StOffscreenGWorld	offWorld(inRect, 0, 0, nil, nil, inBackColor);
			::PlotIconID(&inRect, atNone, ttNone, endIconID);
		}
		
		::Delay(delay_Animation, &ticks);
									// Draw normal end state
		::PlotIconID(&inRect, atNone, ttNone, (SInt16) (endIconID - 1));
		
	} else {						// Draw original state
		::PlotIconID(&inRect, atNone, ttNone, iconID);
	}
		
	return goodClick;
}
