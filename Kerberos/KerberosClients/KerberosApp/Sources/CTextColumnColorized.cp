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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/CTextColumnColorized.cp,v 1.19 2003/05/09 21:12:46 smcguire Exp $ */

// =================================================================================
//	CTextColumnColorized.cp
// =================================================================================
/* Same as LTextColumn, but lets you set, get, and apply different fore and background
   colors.  This is good when you want a white list on a grey Appearance window.
*/

#include "CTextColumnColorized.h"

PP_Using_Namespace_PowerPlant

// constructor
CTextColumnColorized::CTextColumnColorized(LStream *inStream) : LTextColumn(inStream)
{
	mAllowDrops = true;
}

// destructor
CTextColumnColorized::~CTextColumnColorized() {
}


// ---------------------------------------------------------------------------
//	¥ ClickCell
// ---------------------------------------------------------------------------
//	Ovverride - Broadcast message for a double-click on a cell

void
CTextColumnColorized::ClickCell(
	const STableCell&		inCell,
	const SMouseDownEvent&	inMouseDown)
{
	if (GetClickCount() == 1) {
	
		if (LDragAndDrop::DragAndDropIsPresent() &&
			::WaitMouseMoved(inMouseDown.macEvent.where)) {

			FocusDraw();
			
			UInt32	dataSize;
			GetCellData(inCell, nil, dataSize);
			StPointerBlock	buffer((SInt32) dataSize);
			GetCellData(inCell, buffer, dataSize);
			
			Rect	cellRect;
			GetLocalCellRect(inCell, cellRect);
		
			LDragTask	theDragTask(inMouseDown.macEvent, cellRect, 1, mDragFlavor,
										buffer, (SInt32) dataSize, mFlavorFlags);
										
			OutOfFocus(nil);
		
			// Check for a drop in the trash.
			if ( this->DroppedInTrash( theDragTask.GetDragReference() ) ) {
			
				// Delete the cell and refresh the list.
				RemoveRows( 1, inCell.row, true );
				Refresh();
			}
		}
	

	} else if (GetClickCount() == 2) {
		if (mDoubleClickMsg != msg_Nothing) {
			BroadcastMessage(mDoubleClickMsg, (void*) this);
		}
	}
}

// ---------------------------------------------------------------------------
//	¥ HiliteCellActively
// ---------------------------------------------------------------------------
//	Draw or undraw active hiliting for a Cell

void CTextColumnColorized::HiliteCellActively(
	const STableCell&	inCell,
	Boolean				inHilite )
{
	Boolean insideFrame = false;
	Rect cellRect;
	
	StFocusAndClipIfHidden	focus(this);

    insideFrame = GetLocalCellRect(inCell, cellRect);

	ApplyForeAndBackColors();

    // this is to clean up anti-aliasing artifacts against a highlighted background
   	if (inHilite && insideFrame)
		::EraseRect(&cellRect);	
    
    // call inherited method to do the rest of the work
    LTableView::HiliteCellActively(inCell, inHilite);
    
    // this is to clean up anti-aliasing artifacts against a highlighted background
	if (!inHilite && insideFrame)
		::EraseRect(&cellRect);

    // draw the cell contents again after we've cleaned up
    if (insideFrame)
    	DrawCell(inCell, cellRect);
}

// ---------------------------------------------------------------------------
//	¥ HiliteCellInactively
// ---------------------------------------------------------------------------
//	Draw or undraw inactive hiliting for a Cell

void CTextColumnColorized::HiliteCellInactively(
	const STableCell&	inCell,
	Boolean				inHilite )
{
	Boolean insideFrame;
	Rect cellRect;

	StFocusAndClipIfHidden	focus(this);

    insideFrame = GetLocalCellRect(inCell, cellRect);

	ApplyForeAndBackColors();

    // this is to clean up anti-aliasing artifacts against a highlighted background
   	if (inHilite && insideFrame)
		::EraseRect(&cellRect);
    
    // call inherited method to do the rest of the work
    LTableView::HiliteCellInactively(inCell, inHilite);

    // this is to clean up anti-aliasing artifacts against a highlighted background
	if (!inHilite && insideFrame)
		::EraseRect(&cellRect);

    // draw the cell contents again after we've cleaned up
    if (insideFrame)
	    DrawCell(inCell, cellRect);
}

// ---------------------------------------------------------------------------------
//		¥ ItemIsAcceptable
// ---------------------------------------------------------------------------------

Boolean CTextColumnColorized::ItemIsAcceptable(
	DragReference	inDragRef,
	ItemReference	inItemRef )
{
	// Make sure the table is enabled and
	// there's text in the drag data.
	FlavorFlags	theFlags;
	DragAttributes theAttributes;
	
	::GetDragAttributes(inDragRef, &theAttributes);
	
	return IsEnabled() &&  // table is active
		   mAllowDrops &&  // table is accepting drops
	       (::GetFlavorFlags( inDragRef, inItemRef, 'TEXT', &theFlags ) == noErr) && // drop is text
	       (theAttributes & kDragInsideSenderApplication);  // drop came from inside the application
}

// ---------------------------------------------------------------------------------
/*
   Override of LTableView::DrawSelf() so that we can do the highlighting of the
   selection before the drawing.  This solves some more problems with anti-aliased text.
*/
// ---------------------------------------------------------------------------------
void CTextColumnColorized::DrawSelf()
{
	DrawBackground();

		// Determine cells that need updating. Rather than checking
		// on a cell by cell basis, we just see which cells intersect
		// the bounding box of the update region. This is relatively
		// fast, but may result in unnecessary cell updates for
		// non-rectangular update regions.

	Rect	updateRect;
	{
		StRegion	localUpdateRgn( GetLocalUpdateRgn(), false );
		localUpdateRgn.GetBounds(updateRect);
	}
	
	STableCell	topLeftCell, botRightCell;
	FetchIntersectingCells(updateRect, topLeftCell, botRightCell);
	
	// these next two lines are swapped relative to LTableView::DrawSelf()
	HiliteSelection(IsActive(), true);

	DrawCellRange(topLeftCell, botRightCell);
}

// ---------------------------------------------------------------------------------
//		¥ ReceiveDragItem
// ---------------------------------------------------------------------------------

void CTextColumnColorized::ReceiveDragItem(
	DragReference	inDragRef,
	DragAttributes	inDragAttrs,
	ItemReference	inItemRef,
	Rect			&inItemBounds )
{
#pragma unused( inItemBounds )
#pragma unused( inDragAttrs )

	FlavorFlags	theFlags;
	
	ThrowIfOSErr_( ::GetFlavorFlags( inDragRef, 
		inItemRef, 'TEXT', &theFlags ) );

	// Get the data.
	Size	theDataSize = 255;
	Str255	theString;
	
	try {
		ThrowIfOSErr_( ::GetFlavorData( inDragRef, inItemRef,
			'TEXT', &theString[1], &theDataSize, 0 ) );
		
		// Get the data size and set the string length.
		ThrowIfOSErr_( ::GetFlavorDataSize( inDragRef,
			inItemRef, 'TEXT', &theDataSize ) );
		theString[0] = (unsigned char)theDataSize;

		//we're using these tables to store C strings
		LString::PToCStr(theString);
		
		STableCell cellToMove;

		// if we can find this data in the table, consider this a move operation
		if ( this->FindCellData(cellToMove, theString, (UInt32)theDataSize) ) {
				
			if ( mDropRow != cellToMove.row ) {
			
				// Delete the old data - if no selection IsValidCell prevents bad things
				if ( IsValidCell( cellToMove ) ) {

					// Delete the original cell.
					RemoveRows( 1, cellToMove.row, false);
				
				}

				// Add the new data.
				TableIndexT	theRow;
				if ( mDropRow == -1 ) {
					theRow = LArray::index_Last;
				} else {
					theRow = mDropRow;
					if ( (theRow > cellToMove.row) && IsValidCell( cellToMove) ) {
						// Adjust for deleted row
						// (call IsValidCell to make sure cellToMove.row isn't 0 for "no selection")
						theRow -= 1;
					}
				}
				
				
				InsertRows( 1, theRow, theString, (UInt32)theDataSize, false );
				
				// Select the new cell, but without calling
				// SelectCell to avoid immediate drawing.
				//mSelectedCell.row = theRow + 1;
				
				STableCell cellToSelect;
				
				cellToSelect.row = theRow + 1;
				cellToSelect.col = 1;
				
				SelectCell(cellToSelect);
			}

		} else { // it's a copy operation

			// Add the new data.
			TableIndexT	theRow;
			if ( mDropRow == -1 ) {
				theRow = LArray::index_Last;
			} else {
				theRow = mDropRow;
			}

			InsertRows( 1, theRow, theString, (UInt32)theDataSize, false );

			// Select the new cell, but without calling
			// SelectCell to avoid immediate drawing.
			//mSelectedCell.row = theRow + 1;
			
			STableCell cellToSelect;
			
			cellToSelect.row = theRow + 1;
			cellToSelect.col = 1;
			
			SelectCell(cellToSelect);

		}
		
		// Invalidate the table.
		Refresh();
	}
	
	catch (...) {
		// don't do anything
	}
}

// ---------------------------------------------------------------------------------
//		¥ EnterDropArea
// ---------------------------------------------------------------------------------

void CTextColumnColorized::EnterDropArea(
	DragReference	inDragRef,
	Boolean			inDragHasLeftSender )
{
	// Call inherited.
	LDragAndDrop::EnterDropArea( inDragRef, inDragHasLeftSender );

	// Invalidate the last drop cell.
	mDropRow = -1UL;
}


// ---------------------------------------------------------------------------------
//		¥ LeaveDropArea
// ---------------------------------------------------------------------------------

void CTextColumnColorized::LeaveDropArea(
	DragReference	inDragRef )
{
	// Undo dividing line drawing.
	DrawDividingLine( mDropRow );

	// Invalidate the last drop cell.
	mDropRow = -1UL;

	// Call inherited.
	LDragAndDrop::LeaveDropArea( inDragRef );
}


// ---------------------------------------------------------------------------------
//		¥ InsideDropArea
// ---------------------------------------------------------------------------------

void CTextColumnColorized::InsideDropArea( DragReference	inDragRef )
{
	// Call inherited.
	LDragAndDrop::InsideDropArea( inDragRef );

	// Focus.
	if ( FocusDraw() ) {

		// Get the mouse location and
		// convert to port coordinates.
		Point	thePoint;
		::GetDragMouse( inDragRef, &thePoint, nil );
		GlobalToPortPoint( thePoint );

		// Get the dividing line point.
		TableIndexT	theRow;
		GetDividingLineGivenPoint( thePoint, theRow );
		
		if ( mDropRow != theRow ) {
		
			if ( mDropRow >= 0 ) {
			
				// Undo the previous dividing line.
				DrawDividingLine( mDropRow );
			
			}
			
			// Update the drop cell and
			// draw the new dividing line.
			mDropRow = theRow;
			DrawDividingLine( mDropRow );
		
		}

	}
}

// ---------------------------------------------------------------------------------
//		¥ HiliteDropArea
// ---------------------------------------------------------------------------------

/*
void CTextColumnColorized::HiliteDropArea(
	DragReference	inDragRef )
{
	// Get the frame rect.
	Rect	theRect;
	CalcLocalFrameRect( theRect );

	// Show the drag hilite in the drop area.
	RgnHandle	theRgnH = ::NewRgn();
	::RectRgn( theRgnH, &theRect );
	::ShowDragHilite( inDragRef, theRgnH, true );
	::DisposeRgn( theRgnH );
}
*/

// ---------------------------------------------------------------------------------
//		¥ GetDividingLineGivenPoint
// ---------------------------------------------------------------------------------

void CTextColumnColorized::GetDividingLineGivenPoint(
	const Point		&inPortPoint,
	TableIndexT		&outRow )
{
	Boolean	isValid = false;
	UInt16 rowHeight;
	
	rowHeight = this->GetRowHeight(1); // should be okay just to take the first one since we're single geometry
	
	// Convert to local coordinates.
	Point	theLocalPoint = inPortPoint;
	PortToLocalPoint( theLocalPoint );
	
	// Convert to image coordinates.
	SPoint32	theImagePoint;
	LocalToImagePoint( theLocalPoint, theImagePoint );
	
	// Calculate the cell index given the image point.
	outRow = (TableIndexT)((theImagePoint.v - 1) / rowHeight + 1);
	
	// Calculate the cell midpoint.
	UInt32	theMidPoint = (outRow - 1) * rowHeight + rowHeight / 2;

	if ( theImagePoint.v < theMidPoint ) {
	
		// The point is less than the midpoint,
		// so use the previous cell index.
		outRow -= 1;
	
	}
	
	// Constrain it to the range of cells.
	// Note: zero is used to mean "insert at the beginning".
	if ( outRow < 0 ) {
		outRow = 0;
	} else if ( outRow > mRows ) {
		outRow = mRows;
	}
}


// ---------------------------------------------------------------------------------
//		¥ DrawDividingLine
// ---------------------------------------------------------------------------------

void CTextColumnColorized::DrawDividingLine( TableIndexT	inRow )
{
	// Setup the target cell.
	STableCell	theCell;
	UInt16 rowHeight, colWidth;
	
	rowHeight = this->GetRowHeight(1); // should be okay just to take the first one since we're single geometry
	colWidth = this->GetColWidth(1);
	
	theCell.row = inRow;
	theCell.col = 1;

	// Focus the pane and get the table and cell frames.
	Rect	theFrame;
	if ( FocusDraw() && CalcLocalFrameRect( theFrame ) ) {

		// Save the draw state.
		StColorPenState	theDrawState;

		// Save the clip region state and clip the list view rect.
		StClipRgnState	theClipState( theFrame );

		// Setup the color and pen state.
		::ForeColor( blackColor );
		::PenMode( patXor );
		::PenSize( 2, 2 );

		// Calculate the dividing line position.		
		Point	thePoint;
		thePoint.v = (short)(inRow * rowHeight);
		thePoint.h = 0;

		// Draw the line.
		::MoveTo( thePoint.h, (short)(thePoint.v - 1) );
		::LineTo( (short)(thePoint.h + colWidth), (short)(thePoint.v - 1) );
	
	}
}

// ---------------------------------------------------------------------------------
//		¥ DroppedInTrash
// ---------------------------------------------------------------------------------
Boolean CTextColumnColorized::DroppedInTrash( DragReference	inDragRef )
{
	Boolean	isTrash = false;
	
	try {

#ifdef	Debug_Throw
		// It's okay to fail here, so 
		// temporarily turn off debug actions.
		StValueChanger<EDebugAction>
			theDebugAction( UDebugging::gDebugThrow, debugAction_Nothing );
#endif

		// Get the drop location from the drag ref.
		StAEDescriptor	theDropDestination;
		ThrowIfOSErr_( ::GetDropLocation( inDragRef, theDropDestination ) );

		// Make sure we're dealing with an alias.
		ThrowIf_( theDropDestination.DescriptorType() != typeAlias );

		// Get the file spec of the destination to
		// which the user dragged the item.
		FSSpec	theDestinationFSSpec;
		
		{
			// Lock the descriptor data handle.
			AliasHandle theDestinationAliasH;
			

	#if ACCESSOR_CALLS_ARE_FUNCTIONS

			ThrowIfOSErr_(::AEGetDescData(theDropDestination, &theDestinationAliasH, sizeof(AliasHandle)));
	
	#else
			theDestinationAliasH = (AliasHandle)theDropDestination.mDesc.dataHandle;
	#endif		
			StHandleLocker	theLock( (Handle)theDestinationAliasH );
			
			// Attempt to resolve the alias.
			Boolean	isChanged;
			ThrowIfOSErr_( ::ResolveAlias( nil,
				theDestinationAliasH,
				&theDestinationFSSpec, &isChanged ) );
		}
		
		// Get the file spec for the trash.
		FSSpec	theTrashFSSpec;
		SInt16	theTrashVRefNum;
		SInt32	theTrashDirID;
		ThrowIfOSErr_( ::FindFolder( kOnSystemDisk, kTrashFolderType,
			kDontCreateFolder, &theTrashVRefNum, &theTrashDirID ) );
		ThrowIfOSErr_( ::FSMakeFSSpec( theTrashVRefNum,
			theTrashDirID, nil, &theTrashFSSpec ) );

		// Compare the two file specs.
		isTrash =
			(theDestinationFSSpec.vRefNum == theTrashFSSpec.vRefNum ) &&
			(theDestinationFSSpec.parID	 ==	theTrashFSSpec.parID ) &&
			(::EqualString( theDestinationFSSpec.name,
				theTrashFSSpec.name, false, true ));
	
	} catch (...) {
	
		// Nothing to do here.
	}

	return isTrash;
}

// set whether this table will accept drops from drag and drop
Boolean CTextColumnColorized::GetAllowDrops()
{
	return mAllowDrops;
}

// get whether this table accepts drops from drag and drop
void CTextColumnColorized::SetAllowDrops(Boolean inOption)
{
	mAllowDrops = inOption;
}
