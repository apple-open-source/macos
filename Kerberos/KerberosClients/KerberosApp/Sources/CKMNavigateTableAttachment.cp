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

// ===========================================================================
//	CKMNavigateTableAttachment
// ===========================================================================
//	Attachment for CTableView which allows navigation through table by
//	pressing up/down keys and deletion of servers/domains by pressing the
//	delete key.


#include "CKMNavigateTableAttachment.h"

#include <LOutlineItem.h>
#include <LOutlineTable.h>
#include <LCommander.h>
#include <LMultiPanelView.h>
#include <LTabsControl.h>
#include <LCommander.h>
#include <PP_KeyCodes.h>

#include "CKerberosManagerApp.h"
#include "CKMMainWindow.h"

PP_Using_Namespace_PowerPlant

// ---------------------------------------------------------------------------
//	¥ CKMNavigateTableAttachment(LTableView*, MessageT)
// ---------------------------------------------------------------------------

CKMNavigateTableAttachment::CKMNavigateTableAttachment(
 LTableView*		inTableView,
	MessageT		inMessage)

: LAttachment(inMessage)
{
	Assert_(inTableView != nil);
	mTableView = dynamic_cast<LTableView*>(inTableView);
	
	// We must make sure that the window is listening here
	// because it is the only place we can do it
	LView* theSuperView = dynamic_cast<LView*>(mTableView->GetSuperView());
	ThrowIfNil_(theSuperView);
	
	LView* theBigView = dynamic_cast<LView*>(theSuperView->GetSuperView());
	ThrowIfNil_(theBigView);
	if (theBigView->GetPaneID() == rMainWindow) {
		CKMMainWindow* theRealmsWindow = dynamic_cast<CKMMainWindow*>(theBigView);
		ThrowIfNil_(theRealmsWindow);
		AddListener(theRealmsWindow);
		mSupportsSelectAll = false;
	}
	/*
	else {
		LMultiPanelView* theMultiPanel = dynamic_cast<LMultiPanelView*>(theBigView->GetSuperView());
		ThrowIfNil_(theMultiPanel);
		LTabsControl* theTabsControl = dynamic_cast<LTabsControl*>(theMultiPanel->GetSuperView());
		ThrowIfNil_(theTabsControl);
		CRealmEditWindow* theRealmEditWindow = dynamic_cast<CRealmEditWindow*>(theTabsControl->GetSuperView());
		ThrowIfNil_(theRealmEditWindow);
		AddListener(theRealmEditWindow);
		mSupportsSelectAll = true;
	}
	*/
}


// ---------------------------------------------------------------------------
//	¥ CKMNavigateTableAttachment(LStream*)
// ---------------------------------------------------------------------------
//	Stream constructor. No extra data members are defined.

CKMNavigateTableAttachment::CKMNavigateTableAttachment(
	LStream*	inStream)

: LAttachment(inStream)
{
	mTableView = (dynamic_cast<LTableView*>
						(LAttachable::GetDefaultAttachable()));
						
}


// ---------------------------------------------------------------------------
//	¥ ~CKMNavigateTableAttachment
// ---------------------------------------------------------------------------
//	Destructor

CKMNavigateTableAttachment::~CKMNavigateTableAttachment()
{
}



// ---------------------------------------------------------------------------
//	¥ ExecuteSelf											[protected]
// ---------------------------------------------------------------------------
//	Decode the message and dispatch it.

void
CKMNavigateTableAttachment::ExecuteSelf(
	MessageT		inMessage,
	void*			ioParam)
{
	SetExecuteHost(true);
	
	if (mTableView != nil) {

		switch (inMessage) {

			case msg_CommandStatus:
				FindCommandStatus((SCommandStatus*) ioParam);
				break;
			
			case cmd_SelectAll:
				SelectAll();
				break;

			case msg_KeyPress:
				HandleKeyEvent((EventRecord*) ioParam);
				break;
		}
	}
}

// ---------------------------------------------------------------------------
//	¥ FindCommandStatus										[protected]
// ---------------------------------------------------------------------------
//	Enable the Select All command only if we're in a CRealmsWindow

void
CKMNavigateTableAttachment::FindCommandStatus(
	SCommandStatus*	inCommandStatus)
{
	#pragma unused (inCommandStatus)
	if (inCommandStatus->command == cmd_SelectAll) {
		if (mSupportsSelectAll) {
			*(inCommandStatus->enabled) = true;
			SetExecuteHost(false);
		}
	}
}


// ---------------------------------------------------------------------------
//	¥ HandleKeyEvent										[protected]
// ---------------------------------------------------------------------------
//	Recognize and dispatch the arrow keys.

void
CKMNavigateTableAttachment::HandleKeyEvent(
	const EventRecord* inEvent)
{
	// Decode the key-down message.

	SInt16 theKey = (SInt16) (inEvent->message & charCodeMask);
	
	switch (theKey) {
		
		case char_LeftArrow:
		case char_UpArrow: {
			Boolean shiftPressed = ((inEvent->modifiers) & shiftKey) != 0;
			UpArrow(shiftPressed);
			SetExecuteHost(false);
			LCommander::SetUpdateCommandStatus(true);
			break;
		}
		
		case char_RightArrow:
		case char_DownArrow: {
			Boolean shiftPressed = ((inEvent->modifiers) & shiftKey) != 0;
			DownArrow(shiftPressed);
			SetExecuteHost(false);
			LCommander::SetUpdateCommandStatus(true);
			break;
		}
		
		case char_Home:
		case char_End:
		case char_PageUp:
		case char_PageDown:
			DoNavigationKey(*inEvent);
			LCommander::SetUpdateCommandStatus(true);
			break;
			
		/*
		case char_Backspace: {
			// only works if cmd-delete pressed
			if (((inEvent->modifiers) & cmdKey) != 0) {
				GenericDeleteRow();
				SetExecuteHost(false);
				mTableView->SetUpdateCommandStatus(true);
			}
			break;
		}
		*/
		
		default:
			break;
	}
	
}



// ---------------------------------------------------------------------------
//	¥ SelectAll												[protected]
// ---------------------------------------------------------------------------
//	The Select All menu item has been chosen. Select all cells in the
//	outline.

void
CKMNavigateTableAttachment::SelectAll()
{
	mTableView->SelectAllCells();
	SetExecuteHost(false);
	mTableView->SelectionChanged();
}


// ---------------------------------------------------------------------------
//	¥ UpArrow												[protected]
// ---------------------------------------------------------------------------
//	Select the next cell above the current cell. If no cell is selected,
//	select the bottom left cell.

void
CKMNavigateTableAttachment::UpArrow(Boolean shiftPressed)
{

	// Find first selected cell.

	STableCell theCell;
	if (mTableView->GetNextSelectedCell(theCell)) {
	
		// Found a selected cell.
		// If not in the first row, move up one and select.

		if (theCell.row > 1) {
			theCell.row--;
			if (!(shiftPressed)) {
				mTableView->UnselectAllCells();
			}
			mTableView->SelectCell(theCell);
			mTableView->ScrollCellIntoFrame(theCell);
		}
	}
	else {
		TableIndexT rows,cols;
		// Nothing selected. Select First Cell
		mTableView->GetTableSize(rows, cols);
		if (rows > 0) {
			theCell.row = 1;
			theCell.col = 1;
			mTableView->UnselectAllCells();
			mTableView->SelectCell(theCell);
			mTableView->ScrollCellIntoFrame(theCell);
		}
		
	}
}


// ---------------------------------------------------------------------------
//	¥ DownArrow												[protected]
// ---------------------------------------------------------------------------
//	Select the next cell below the current cell. If no cell is selected,
//	select the top left cell.

void
CKMNavigateTableAttachment::DownArrow(Boolean shiftPressed)
{

	// Find first selected cell.

	TableIndexT rows, cols;
	STableCell theCell;
	if (!mTableView->GetNextSelectedCell(theCell)) {
		// Nothing selected. Start from top.
		TableIndexT rows, cols;
		mTableView->GetTableSize(rows, cols);
		
		if (rows > 0) {
			theCell.row = rows;
			theCell.col = 1;
			mTableView->UnselectAllCells();
			mTableView->SelectCell(theCell);
			mTableView->ScrollCellIntoFrame(theCell);
		}
		
	
	}
	else {

		// Found a selected cell. Look for last selected cell.
		
		STableCell lastCell = theCell;
		while (mTableView->GetNextSelectedCell(theCell)) {
			lastCell = theCell;
		}
		
		// Found last selected cell.
		// If not in the last row, move down one and select.
		
		mTableView->GetTableSize(rows, cols);
		
		if (lastCell.row < rows) {
			lastCell.row++;
			if (!(shiftPressed)) {
				mTableView->UnselectAllCells();
			}
			mTableView->SelectCell(lastCell);
			mTableView->ScrollCellIntoFrame(lastCell);
		}
	}
}

// ---------------------------------------------------------------------------
//	¥ DoNavigationKey
// ---------------------------------------------------------------------------
//	Implements keyboard navigation by supporting selection change using
//	the arrow keys, page up, page down, home, and end

void
CKMNavigateTableAttachment::DoNavigationKey(
	const EventRecord	&inKeyEvent)
{

	char	theKey = (char) (inKeyEvent.message & charCodeMask);
	//Boolean	cmdKeyDown = (inKeyEvent.modifiers & cmdKey) != 0;
	//Boolean	extendSelection = ((inKeyEvent.modifiers & shiftKey) != 0); // AND SELECTION ALLOWS IT

	switch (theKey) {
		case char_Home:
                    {
			STableCell	firstCell(1, 1);
			mTableView->ScrollCellIntoFrame(firstCell);
                    }
                    break;

		case char_End:
                    {
                        LTableStorage*	storage = mTableView->GetTableStorage();
                        if (storage != nil) {
                                TableIndexT  numRows, numCols;
                                mTableView->GetTableSize(numRows, numCols);
                                STableCell	lastCell(numRows, 1);
                                mTableView->ScrollCellIntoFrame(lastCell);
                        }
                    }
                    break;


//Page Up and Down pinched from LKeyScrollAttachment::ExecuteSelf 
		case char_PageUp:
                    {		// Scroll up by height of Frame,
                                //   but not past top of Image
			SPoint32		frameLoc;
			SPoint32		imageLoc;
			mTableView->GetFrameLocation(frameLoc);
			mTableView->GetImageLocation(imageLoc);
			
			SInt32	upMax = frameLoc.v - imageLoc.v;
			if (upMax > 0) {
				SPoint32		scrollUnit;
				SDimension16	frameSize;
				mTableView->GetScrollUnit(scrollUnit);
				mTableView->GetFrameSize(frameSize);

				SInt32	up = (frameSize.height - 1) / scrollUnit.v;
				if (up <= 0) {
					up = 1;
				}
				up *= scrollUnit.v;
				if (up > upMax) {
					up = upMax;
				}
				mTableView->ScrollImageBy(0, -up, true);
			}
			break;
                    }
			
		case char_PageDown:
                    {	// Scroll down by height of Frame,
                        //   but not past bottom of Image
			SPoint32		frameLoc;
			SPoint32		imageLoc;
			SDimension16	frameSize;
			SDimension32	imageSize;
			mTableView->GetFrameLocation(frameLoc);
			mTableView->GetImageLocation(imageLoc);
			mTableView->GetFrameSize(frameSize);
			mTableView->GetImageSize(imageSize);
			
			SInt32	downMax = imageSize.height - frameSize.height -
								(frameLoc.v - imageLoc.v);
			if (downMax > 0) {
				SPoint32		scrollUnit;
				mTableView->GetScrollUnit(scrollUnit);

				SInt32	down = (frameSize.height - 1) / scrollUnit.v;
				if (down <= 0) {
					down = 1;
				}
				down *= scrollUnit.v;
				if (down > downMax) {
					down = downMax;
				}
				mTableView->ScrollImageBy(0, down, true);
			}
			break;
                    }
			
	}

}	

// ---------------------------------------------------------------------------
//	¥ GenericDeleteRow											[protected]
// ---------------------------------------------------------------------------
//	Allows the user to delete a row by pressing the backspace/delete key, but
//	delete in the proper manner by broadcasting the message msg_sRemove or
//	msg_dRemove based on what tab they're in.

/*
void
CKMNavigateTableAttachment::GenericDeleteRow()
{
	
	
	PaneIDT curPaneID = mTableView->GetPaneID();
	switch (curPaneID) {
		case sServerListTable_ID:
			BroadcastMessage(msg_sRemove, nil);
			break;
		case dDomainListTable_ID:
			BroadcastMessage(msg_dRemove, nil);
			break;
		case RealmsTable_ID:
			BroadcastMessage(msg_Remove, nil);
			break;
	}
}

*/
