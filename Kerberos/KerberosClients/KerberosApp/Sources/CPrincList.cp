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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/CPrincList.cp,v 1.69 2003/06/12 15:46:51 smcguire Exp $ */

// =================================================================================
//	CPrincList.c
// =================================================================================

#include <utime.h>
#include <string.h>

#include <LowMem.h>

#if !TARGET_RT_MAC_MACHO
	#include <KerberosSupport/Utilities.h>
	#include <Kerberos5/Kerberos5.h>
#else
	#include <sys/time.h>
	#include <Kerberos/Kerberos.h>
	#include "UCCache.h"
#endif

#include <PP_DebugMacros.h>
#include <LArrayIterator.h>
#include <LTableMonoGeometry.h>
#include <LTableSingleSelector.h>
#include <LTableView.h>
#include <LNodeArrayTree.h>
#include "CDropFlagMod.h"


#include "CKerberosManagerApp.h"
#include "CKMMainWindow.h"
#include "CTicketListItem.h"
#include "CTicketListPrincipalItem.h"
#include "CTicketListSubPrincipalItem.h"
#include "CTicketListCredentialsItem.h"
#include "CTableTLIPtrArrayStorage.h"
#include "CPrincList.h"
#include "CTicketListItem.h"
#include "CKMNavigateTableAttachment.h"

PP_Using_Namespace_PowerPlant

//const	RGBColor	color_LtGrey = {60000, 60000, 60000};

// ---------------------------------------------------------------------------
//		¥ CPrincList
// ---------------------------------------------------------------------------
// constructor
CPrincList::CPrincList(LStream *inStream) : LHierarchyTable(inStream) {

	//initialize some fields
	fDirty = false;

	//get a pointer to the preferences
	CKerberosManagerApp *theApp = static_cast<CKerberosManagerApp *>(LCommander::GetTopCommander());
	mKrbPrefsRef = theApp->GetKrbPreferencesRef();
	Assert_(mKrbPrefsRef != nil);
	// should deal better with failures here

	
	// add us as a listener to the prefs so we can get the prefs changed broadcast messages
	mKrbPrefsRef->AddListener(this);

	InitList();
}

// ---------------------------------------------------------------------------
//		¥ InitList
// ---------------------------------------------------------------------------
// I wish i was a constructor
void CPrincList::InitList() {
	
	CKerberosManagerApp *theApp;
	
	//setup helpers
	mTableGeometry = new LTableMonoGeometry(this, (unsigned short)mFrameSize.width, kCellHeight);
	mTableSelector = new LTableSingleSelector(this);
	mTableStorage =  new CTableTLIPtrArrayStorage (this, sizeof(CTicketListItem *));
	
	// add our keyboard navigation attachment
	CKMNavigateTableAttachment* theAttachment = new CKMNavigateTableAttachment (this);
	AddAttachment(theAttachment, nil, true);

	//insert one column
	InsertCols(1,0,nil,nil,false);

	//get a pointer to the krb session
	theApp = static_cast<CKerberosManagerApp *>(LCommander::GetTopCommander());
	mKrbSession = theApp->GetKrbSession();

	//setup broadcasting
	this->AddListener(theApp);
	StartBroadcasting();
	
	UpdateListFromCache();	
	
	StartIdling();
	return;
}

// ---------------------------------------------------------------------------
//		¥ ~CPrincList
// ---------------------------------------------------------------------------
// destructor
CPrincList::~CPrincList() {
	
	// make sure we deallocate the memory for any item stored in the table
	RemoveAllRows(false);
	
}


// ---------------------------------------------------------------------------
//		¥ DrawCell
// ---------------------------------------------------------------------------
// an override of the standard table function, draw the string in the cell
// with an appropriate text style
void CPrincList::DrawCell( const STableCell &inCell, const Rect &inLocalRect ) {
	
	CTicketListItem *itemToDraw;
	TableIndexT	wideOpenIndex;
	UInt32 offset;
	UInt32 cellHeight;
	FontInfo fInfo;
	StTextState tState;
	ResIDT tt;
	unsigned char timeString[256], versString[10], cellTextString[256];
	RGBColor	saveForeColor, saveBackColor;
	Boolean		itemIsExpired = false;
	
	// make sure we can get a listItem from this cell, return if not
	if (!GetItemFromCell(inCell, &itemToDraw))
		return;

	// get index, draw drogflag if necessary
	wideOpenIndex = GetWideOpenIndex(inCell.row);

	DrawDropFlag(inCell, wideOpenIndex);
	
	// get cell height
	cellHeight = (UInt32) (inLocalRect.bottom - inLocalRect.top);
	
	// determine if item is expired
	// it's okay to use "local" time instead of KDC time because the CCacheLib stores
	// times in "local" time
	struct timeval currentTime;
	gettimeofday(&currentTime, nil);
	itemIsExpired = (itemToDraw->GetItemExpirationTime() < (unsigned long)currentTime.tv_sec);
	
	// is item valid?
	short itemValidity = itemToDraw->GetItemValidity();
	
	// get and set the right type style to draw the item in
	if (!itemIsExpired && (itemValidity == kTicketValid))
		tt = itemToDraw->GetItemTextTrait();  // not expired
	else
		tt = itemToDraw->GetItemExpiredTextTrait(); // expired
		
	offset = itemToDraw->GetItemDrawOffset();
	
	UTextTraits::SetPortTextTraits( tt );
	
	// Determine the version tags to append (if necessary)
	if (itemToDraw->GetItemType() != kCredentialsItem) {
		switch (itemToDraw->GetKerberosVersion()) {
			case cc_credentials_v4:
				strcpy((char *)versString,"(v4) ");
				break;
			case cc_credentials_v5:
				strcpy((char *)versString,"(v5) ");
				break;
			case cc_credentials_v4_v5:
				strcpy((char *)versString,"(v4/v5) ");
				break;
			case cc_credentials_no_version:
			default:
				strcpy((char *)versString,"");
				break;
		}
	} else {
		strcpy((char *)versString,"");
	}
	
	Str255 itemDisplayString;
	itemToDraw->GetItemDisplayString(&itemDisplayString);
	
	LString::PToCStr(itemDisplayString);
	
	// Create the string to be drawn
	sprintf((char *)cellTextString, "%s%s", versString, (char *)itemDisplayString);
	LString::CToPStr((char *)cellTextString);
	
	//do the actual QuickDraw drawing
	::GetFontInfo(&fInfo);
	
	::MoveTo((short)(inLocalRect.left + offset), (short)(inLocalRect.bottom - (cellHeight - fInfo.ascent + fInfo.descent) / 2) );
	::DrawString(cellTextString);
	
	//now draw the time
	
	//don't show a time for the principal line if expanded; also don't show one for sub-principals
	if ( ((itemToDraw->GetItemType() != kCredentialsItem) && (IsExpanded(wideOpenIndex))) || (itemToDraw->GetItemType() == kSubPrincipalItem) )
		return;
	
	// don't underline the active principal's time
	if (itemToDraw->GetItemType() == kPrincipalItem) {
		CTicketListPrincipalItem *itemAsPrincipal;
		
		itemAsPrincipal = dynamic_cast<CTicketListPrincipalItem *>(itemToDraw);
		if (itemAsPrincipal->PrincipalIsActive()) {
			itemAsPrincipal->SetPrincipalAsInactive(); // make item inactive temporarily to get inactive font style
			
			if (!itemIsExpired)
				tt = itemToDraw->GetItemTextTrait();  // not expired
			else
				tt = itemToDraw->GetItemExpiredTextTrait(); // expired
			UTextTraits::SetPortTextTraits( tt );
			::GetFontInfo(&fInfo);
			
			itemAsPrincipal->SetPrincipalAsActive(); // make it active again now that we've got the info
		}
	}
	
	if (!itemIsExpired && (itemValidity == kTicketValid)) {
		UInt32 hours, minutes, seconds, diff;
		UInt32 days;
		
		diff = (UInt32)(itemToDraw->GetItemExpirationTime() - currentTime.tv_sec);
		days = diff / (60 * 60 * 24);
		hours = (diff % (60 * 60 * 24)) / 3600;
		minutes = (diff % 3600) / 60;
		seconds = (diff % 3600) % 60;

/*		hours = diff / 3600;
		minutes = (diff % 3600) / 60;
		seconds = (diff % 3600) % 60;
*/		
		// special case 0:00 to say 0:01
		if ( (seconds > 0) && (minutes == 0) && (hours == 0) && (days == 0) ) {
			minutes = 1;
		}

		// if less than 5 minutes lifetime remaining, draw time in red as a warning
		if ( (minutes < 5) && (hours == 0) && (days == 0)) {
			if (UEnvironment::HasFeature(env_SupportsColor)) {
				
				UTextTraits::SetPortTextTraits(rTicketItemRegularRed);
			}
		}
		
		// draw time string
		if (days > 1)
			sprintf((char *)timeString, "%lu days:%02lu:%02lu", days, hours, minutes);
		else if (days == 1)
			sprintf((char *)timeString, "%lu day:%02u:%02lu", days, hours, minutes);
		else		
			sprintf((char *)timeString, "%lu:%02lu", hours, minutes);

		// set color back to normal if necessary
		if ( (minutes < 5) && (hours == 0) ) {
			if (UEnvironment::HasFeature(env_SupportsColor)) {
				this->SetForeAndBackColors(&saveForeColor, &saveBackColor);
			}
		}
	} else {
		// draw status string in red
		if (UEnvironment::HasFeature(env_SupportsColor)) {
				
			UTextTraits::SetPortTextTraits(rTicketItemItalicRed);
		}
		
		if (itemValidity == kTicketValid)
			sprintf((char *)timeString, "expired");
		else if (itemValidity == kTicketInvalidBadAddress)
			sprintf((char *)timeString, "not valid");
		else if (itemValidity == kTicketInvalidNeedsValidation)
			sprintf((char *)timeString, "needs validation");
		else
			sprintf((char *)timeString, "not valid");
		
		
		if (UEnvironment::HasFeature(env_SupportsColor)) {
			this->SetForeAndBackColors(&saveForeColor, &saveBackColor);
		}
	}
	
	LString::CToPStr((char *)timeString);
	::MoveTo((short)(inLocalRect.right - ::StringWidth(timeString) - 5), 
			(short)(inLocalRect.bottom - (cellHeight - fInfo.ascent + fInfo.descent) / 2) );
	DrawString(timeString);
	
}


void CPrincList::ResizeFrameBy(
	SInt16		inWidthDelta,
	SInt16		inHeightDelta,
	Boolean		inRefresh)
{
	LHierarchyTable::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);
	
	this->SetColWidth((unsigned short)mFrameSize.width, 0, 0);
}

// ---------------------------------------------------------------------------
//		¥ ClickCell
// ---------------------------------------------------------------------------
void CPrincList::ClickCell( const STableCell &inCell, const SMouseDownEvent &inMouseDown ) {

#pragma unused (inMouseDown)

	CTicketListItem *item;

	if (GetItemFromCell(inCell, &item))
		if (GetClickCount() >= 2) {
			if (item->GetItemType() == kPrincipalItem) {
				BroadcastMessage(msg_MakeActiveUser); //fake a activate user button press
			} else if (item->GetItemType() == kCredentialsItem) {
				((CTicketListCredentialsItem *)item)->OpenTicketInfoWindow();

			}
		}
}

// ---------------------------------------------------------------------------
//	¥ ClickSelf
// ---------------------------------------------------------------------------
//	Handle a mouse click within a HierarchyTable

void CPrincList::ClickSelf(const SMouseDownEvent	&inMouseDown)
{
	STableCell	hitCell;
	SPoint32	imagePt;
	CTicketListItem *itemFromCell;
	
	LocalToImagePoint(inMouseDown.whereLocal, imagePt);
	
	if (GetCellHitBy(imagePt, hitCell)) {
										// Click is inside hitCell
										// Check if click is inside DropFlag
		TableIndexT	woRow = mCollapsableTree->GetWideOpenIndex(hitCell.row);
		Rect	flagRect;
		CalcCellFlagRect(hitCell, flagRect);
		
		if (mCollapsableTree->IsCollapsable(woRow) &&
			::PtInRect(inMouseDown.whereLocal, &flagRect)) {
										// Click is inside DropFlag
			FocusDraw();
			ApplyForeAndBackColors();
			Boolean	expanded = mCollapsableTree->IsExpanded(woRow);

			RGBColor fillColor;
			if (mTableSelector->CellIsSelected(hitCell))
				LMGetHiliteRGB(&fillColor);
			else {
				RGBColor tempColor;
				
				GetForeAndBackColors(&tempColor, &fillColor);
				
				/*
				// grey highlighting disabled due to PP 2.2
				
		        if (GetItemFromCell(hitCell, &itemFromCell)) {
		        	if (itemFromCell->GetItemType() == kPrincipalItem) {
						fillColor = color_LtGrey;
					}
				}
				*/
			}
			
			if (CDropFlagMod::TrackClick(flagRect, inMouseDown.whereLocal,
									expanded, &fillColor)) {
//			if (LDropFlag::TrackClick(flagRect, inMouseDown.whereLocal,
//									expanded)) {
										// Mouse released inside DropFlag
										//   so toggle the Row
				if (inMouseDown.macEvent.modifiers & optionKey) {
										// OptionKey down means to do
										//   a deep collapse/expand						
					if (expanded) {
						// find current selection
						STableCell curSelectedCell = GetFirstSelectedCell();
						STableCell listCell = curSelectedCell;
						CTicketListItem *listItem;
						
						// find parent princpal of selection
						if (curSelectedCell.row != 0) {
							for (listCell.row = curSelectedCell.row; listCell.row >= 1; listCell.row--) {
								if (GetItemFromCell(listCell, &listItem)) {
									if (listItem->GetItemType() == kPrincipalItem) {
										break;
									}
								}
							}
						}

						DeepCollapseRow(woRow);
						
						// make new selection if there was a selection before, and the selection was one of the
						// child rows being collapsed (we compare the parent of the selection with the cell being
						// collapsed
						if ((curSelectedCell.row != 0) && (listCell.row == hitCell.row))
							SelectCell(listCell);

					} else {
						DeepExpandRow(woRow);
					}
				
				} else {				// Shallow collapse/expand
					if (expanded) {
						// find current selection
						STableCell curSelectedCell = GetFirstSelectedCell();
						STableCell listCell = curSelectedCell;
						CTicketListItem *listItem;
						
						// find parent princpal of selection
						if (curSelectedCell.row != 0) {
							for (listCell.row = curSelectedCell.row; listCell.row >= 1; listCell.row--) {
								if (GetItemFromCell(listCell, &listItem)) {
									if (listItem->GetItemType() == kPrincipalItem) {
										break;
									}
								}
							}
						}
						
						CollapseRow(woRow);
						
						// make new selection if there was a selection before, and the selection was one of the
						// child rows being collapsed (we compare the parent of the selection with the cell being
						// collapsed
						if ((curSelectedCell.row != 0) && (listCell.row == hitCell.row))
							SelectCell(listCell);
						
					} else {
						ExpandRow(woRow);
					}
				}
				
				// set item's expanded/not expanded flag so we can keep track
		        if (GetItemFromCell(hitCell, &itemFromCell)) {
		        	if (expanded)
		        		itemFromCell->SetItemIsExpanded(false);
		        	else
		        		itemFromCell->SetItemIsExpanded(true);
				}
				
				// this all forced the list to redraw, which may mean the time remainings changed
				// force the active user info to update now too so they stay in sync
				CKerberosManagerApp *theApp = static_cast<CKerberosManagerApp *>(LCommander::GetTopCommander());
				CKMMainWindow *theMainWindowRef = theApp->GetMainWindowRef();
				Assert_(theMainWindowRef != nil);
				
				theMainWindowRef->UpdateActiveUserInfo(false);

				// force an update
				Refresh();
			}
	
		} else if (ClickSelect(hitCell, inMouseDown)) {
										// Click outside of the DropFlag
			ClickCell(hitCell, inMouseDown);
		}
		
	} else {							// Click is outside of any Cell
		UnselectAllCells();
	}
}

// ---------------------------------------------------------------------------
//	¥ HiliteCellActively
// ---------------------------------------------------------------------------
//	Draw or undraw active hiliting for a Cell
//  Overrides LTableView's function so we can tag cells when they're selected

void CPrincList::HiliteCellActively (
	const STableCell	&inCell,
	Boolean				 inHilite)
{
	CTicketListItem *itemFromCell;
	Rect cellRect;

	StFocusAndClipIfHidden	focus(this);

	// set item's stored selected state
    if (GetItemFromCell(inCell, &itemFromCell)) {
    	itemFromCell->SetItemIsSelected(inHilite);
    }
    
	ApplyForeAndBackColors();

    // this is to clean up anti-aliasing artifacts against a highlighted background
    GetLocalCellRect(inCell, cellRect);
	if (inHilite)
		::EraseRect(&cellRect);
    
    // call inherited method to do the rest of the work
    LTableView::HiliteCellActively(inCell, inHilite);
    
    // this is to clean up anti-aliasing artifacts against a highlighted background
	if (!inHilite)
		::EraseRect(&cellRect);

    // draw the cell contents again after we've cleaned up
    DrawCell(inCell, cellRect);
}

// ---------------------------------------------------------------------------
//	¥ HiliteCellInactively
// ---------------------------------------------------------------------------
//	Draw or undraw inactive hiliting for a Cell
//  Overrides LTableView's function so we can tag cells when they're selected

void CPrincList::HiliteCellInactively (
	const STableCell	&inCell,
	Boolean				inHilite )
{
	CTicketListItem *itemFromCell;
	Rect cellRect;

	StFocusAndClipIfHidden	focus(this);

	// set item's stored selected state
    if (GetItemFromCell(inCell, &itemFromCell)) {
    	itemFromCell->SetItemIsSelected(inHilite);
    }

	ApplyForeAndBackColors();
    
    // this is to clean up anti-aliasing artifacts against a highlighted background
    GetLocalCellRect(inCell, cellRect);
	if (inHilite)
		::EraseRect(&cellRect);
    
    // call inherited method to do the rest of the work
    LTableView::HiliteCellInactively(inCell, inHilite);
    
    // this is to clean up anti-aliasing artifacts against a highlighted background
	if (!inHilite)
		::EraseRect(&cellRect);

    // draw the cell contents again after we've cleaned up
    DrawCell(inCell, cellRect);
}

// ---------------------------------------------------------------------------
//		¥ SpendTIme
// ---------------------------------------------------------------------------
// use our periodic time
void CPrincList::SpendTime(
       const EventRecord &inMacEvent) {
       
#pragma unused (inMacEvent)
       // All updating is handled by CKMMainWindow
       // The ticket time remaining code is handled by the call to princList->Refresh()
       // in CKMMainWindow::UpdateActiveUserInfo()
       		
}

void CPrincList::ListenToMessage(MessageT inMessage, void *ioParam)
{
#pragma unused(ioParam)

	switch (inMessage) {
		// redraw ticket list when user changes prefs
		case msg_PrefsAlwaysExpandTicketListChanged:
			this->UpdateListFromCache();
			break;

		default:
			break;
	}
}

// ---------------------------------------------------------------------------
//		¥ RemoveAllRows
// ---------------------------------------------------------------------------
/*
   Override of LHierarchyTable::RemoveAllRows().  The problem with the superclass
   method is that it doesn't really remove all rows, since it calls LTableView::RemoveRows(),
   not LHierarchyTable::RemoveRows().  Only the LHierarchyTable knows how to
   remove child rows of the top-level rows, so when LTableView method is called,
   the child rows are dropped on the floor and leak memory.  This function does
   what LHierarchyTable should do.
*/
void CPrincList::RemoveAllRows(Boolean inRefresh)
{
	// LHierarchyTable only allows you to remove one row at a time, so repeatedly remove rows
	/* keep deleting the first row until there are none left, due to the way HierarchyTables work,
	   you can't just iterate over the rows because they get renumbered during the deletion process
	   so proceeding down the list might miss some! */
	while (mRows != 0) {
		RemoveRows(1, 1, false);
	}
	
	// this is the unique behavior of LHierarchyTree:RemoveAllRows(), so we duplicate it here	
	delete mCollapsableTree;
	mCollapsableTree = new LNodeArrayTree;
	
	// now force the refresh, if requested
	if (inRefresh)
		Refresh();
}

// ---------------------------------------------------------------------------
//		¥ UpdateListFromCache
// ---------------------------------------------------------------------------
// query the CKrbSession object for the latest cache info and rebuild the list
void CPrincList::UpdateListFromCache() {

	LArray *vArr;
	TableIndexT lastPrincRow = 1;
	CTicketListItem *listItem = nil, *compareItem = nil;
	STableCell compareCell;
	Boolean foundItem = false;
	
	// get the new list
	vArr = mKrbSession->GetCacheInfo();
	if (vArr == nil)
		return;
	
	// copy over selection and expanded settings from old list
	LArrayIterator copyIterator(*vArr, 0L);
	try {
		while (copyIterator.Next(&listItem)) {
			compareCell.col = 1;
			foundItem = false;
			
			// loop through all the rows in the old list to see if there's a match
			for (compareCell.row = 1; ((compareCell.row <= mRows) && !foundItem); compareCell.row++) {
				if (GetItemFromCell(compareCell, &compareItem)) {
					
					if (listItem->EquivalentItem(compareItem)) {
						// found one, copy properties over
						foundItem = true;
						listItem->SetItemIsSelected(compareItem->GetItemIsSelected());
						listItem->SetItemIsExpanded(compareItem->GetItemIsExpanded());
						
						if (listItem->GetItemType() == kCredentialsItem) {
							(dynamic_cast<CTicketListCredentialsItem *>(listItem))->AssumeTicketInfoWindowOwnership(dynamic_cast<CTicketListCredentialsItem *>(compareItem));
						}
					}
					
				}
			}
			
			// didn't find an equivalent item from old list, must be new, set default properties
			if (!foundItem) {
				listItem->SetItemIsSelected(false);
				if (listItem->GetItemType() == kPrincipalItem)
					listItem->SetItemIsExpanded( mKrbPrefsRef->GetAlwaysExpandTicketList() );
				else
					listItem->SetItemIsExpanded( false );
			}
		}
	}
	catch (UCCacheLogicError &err) {
		char errString[256];
		sprintf(errString, "UCCacheLogicError %d in UpdateListFromCache()",err.Error() );
		SignalString_(errString);
		
		return;
	}
	catch (UCCacheRuntimeError &err) {
		if (err.Error() != ccErrCCacheNotFound) {
			char errString[256];
			sprintf(errString, "UCCacheLogicError %d in UpdateListFromCache()",err.Error() );
			SignalString_(errString);
		}
		
		return;
	}
	catch (...) {
		SignalStringLiteral_("Unexpected exception in UpdateListFromCache()");
		
		return;
	}
			
	// delete the old list
	RemoveAllRows(false);
	
	// insert new list
	LArrayIterator iterate(*vArr, 0L);
	while (iterate.Next( &listItem) ) {
		if (listItem->GetItemType() == kPrincipalItem) {
			TableIndexT insertRow;
			
			/* find the bottom-most top-level row of the table (a principal entry) to insert the next
			   principal after.  We have to do this because a bug in LHierarchyTable doesn't let you
			   insert a new top-level row at the bottom of the table, if you try you get a sibling row
			   to the last row, which in our case is a child row of the last principal, which we don't
			   want. */
			for (insertRow = this->mRows; insertRow > 0; insertRow--) {
				UInt32 nestLevel = mCollapsableTree->GetNestingLevel(insertRow);
				if (nestLevel == 0) // top level
					break;
			}
			
			lastPrincRow = InsertSiblingRows(1, insertRow, &listItem, (UInt32)sizeof(CTicketListItem *), true, true);
			
			// if flag to expand is false, collapse the row
			if ( !listItem->GetItemIsExpanded() )
				CollapseRow(lastPrincRow);
		} else {
			AddLastChildRow(lastPrincRow, &listItem, sizeof(CTicketListItem *), false, true);
		}
		
		// remove storage for this item in the array, but this array only stores the *pointer*
		// don't delete the actual item because it's still being pointed to by the table
		vArr->Remove(&listItem);
	}
	
	// reset selection in new list
	STableCell listCell;
	
	listCell.col = 1;
	for (listCell.row = 1; listCell.row <= mRows; listCell.row++) {
		if (GetItemFromCell(listCell, &listItem)) {
			if (listItem->GetItemIsSelected()) {
				SelectCell(listCell);
			}
		}
	}
	
	// check list for selection; if there isn't one, select default user
	listCell = GetFirstSelectedCell();
	if ((listCell.row == 0) && (listCell.col == 0)) {
		listCell.col = 1;
		for (listCell.row = 1; listCell.row <= mRows; listCell.row++) {
			if (GetItemFromCell(listCell, &listItem)) {
				if (listItem->GetItemType() == kPrincipalItem) {
					if (((CTicketListPrincipalItem *)listItem)->PrincipalIsActive())
						SelectCell(listCell);
				}
			}
		}
	
	}
	
	Refresh();
	
	delete vArr;
}

// ---------------------------------------------------------------------------
//		¥ GetItemFromCell
// ---------------------------------------------------------------------------
// Get the data pointer given a cell
bool CPrincList::GetItemFromCell(const STableCell &inCell, CTicketListItem **outItem)
{
	Boolean	isValid = false;
	UInt32	theDataSize = sizeof(CTicketListItem *);

	if ((inCell.row < 1) || (inCell.col < 1))
		return false;

	// Get the wide open index for the row.
	TableIndexT	theWideOpenIndex;
	theWideOpenIndex = GetWideOpenIndex( inCell.row );
	Assert_(theWideOpenIndex > 0);
	
	// Create an STableCell object for the cell
	STableCell	theWideOpenCell( theWideOpenIndex, 1 );

	// Get the cell data (the bookmark item).
	GetCellData( theWideOpenCell, &(*outItem), theDataSize );
	
	if ( theDataSize == sizeof(CTicketListItem *) ) {
		// The item must be valid.
		isValid = true;
	}

	return isValid;
}

// ---------------------------------------------------------------------------
//		¥ GetSelectedPrincipal
// ---------------------------------------------------------------------------
// return the principal name currently selected in the list
//   and the version of it, if outVersion isn't nil
// returns false if no principal is selected
Boolean CPrincList::GetSelectedPrincipal(Str255 outPrinc, cc_int32 *outVersion) {
	
	STableCell sel;
	CTicketListItem* item;
	
	sel = GetFirstSelectedCell();
	
	if ((sel.row == 0) && (sel.col == 0))
		return false;
	
	if (GetItemFromCell(sel, &item) && (item->GetItemType() == kPrincipalItem) ) {
		Str255 itemPrincString;
		item->GetItemPrincipalString(&itemPrincString);
		memcpy(outPrinc, itemPrincString, 255);
		if (outVersion != nil)
			*outVersion = item->GetKerberosVersion();
		return true;
	} else
		return false;
		
	
}

// ---------------------------------------------------------------------------
//		¥ GetActivePrincipal
// ---------------------------------------------------------------------------
// return the principal name currently active
//   and the version of it, if outVersion isn't nil
// returns false if no principal is active (e.g. there are no principals)
Boolean CPrincList::GetActivePrincipal(Str255 outPrinc, cc_int32 *outVersion) {
	
	STableCell listCell;
	CTicketListItem* listItem;
	
	listCell.col = 1;
	for (listCell.row = 1; listCell.row <= mRows; listCell.row++) {
		if (GetItemFromCell(listCell, &listItem)) {
			if (listItem->GetItemType() == kPrincipalItem) {
				if (((CTicketListPrincipalItem *)listItem)->PrincipalIsActive()) {
					Str255 itemPrincString;
					listItem->GetItemPrincipalString(&itemPrincString);
					memcpy(outPrinc, itemPrincString, 255);
					if (outVersion != nil)
						*outVersion = listItem->GetKerberosVersion();
					return true;
				}
			}
		}
	}
			
	return false;
}

// ---------------------------------------------------------------------------
//		¥ CheckPrincipalsForAutoRenewal
// ---------------------------------------------------------------------------
// go through the list looking to see if we should auto-renew any principals
void CPrincList::CheckPrincipalsForAutoRenewal() {

	STableCell nextCell;
	CTicketListItem *item;
	
	struct timeval currentTime;
	gettimeofday(&currentTime, nil);

	while (GetNextCell(nextCell)) {
		GetItemFromCell(nextCell, &item);
		if (item->GetItemType() == kPrincipalItem) {
			if (item->IsTicketRenewable()) {
				if (item->GetItemValidity() == kTicketValid) {
					if ((item->GetKerberosVersion() == cc_credentials_v5) || (item->GetKerberosVersion() == cc_credentials_v4_v5)) {
						CTicketListPrincipalItem *princItem = DebugCast_ (item, CTicketListItem, CTicketListPrincipalItem);
						// attempt renew if greater than renewAttemptHalfLife, but not if tix are expired
						if (((unsigned long)currentTime.tv_sec >= (item->GetItemExpirationTime() - princItem->GetRenewAttemptHalflife())) &&
							((unsigned long)currentTime.tv_sec <= item->GetItemExpirationTime())) {
							Str255 itemPrincString;
							item->GetItemPrincipalString(&itemPrincString);
							Boolean success = mKrbSession->DoAutoRenew(itemPrincString, item->GetKerberosVersion());
							if (!success) {
								unsigned long newHalflife = princItem->GetRenewAttemptHalflife() / 2;
								// when we're down to 30 seconds, don't want to rapid fire until we hit zero
								if (newHalflife < 30)
									newHalflife = 0;
								princItem->SetRenewAttemptHalflife(newHalflife);
							}
						} 
					}
				}
			}
		}
	
	}
}

// ---------------------------------------------------------------------------
//		¥ GetAllPrincipals
// ---------------------------------------------------------------------------
// return an array of pstrings containing the names of all known main principals
LArray *
CPrincList::GetAllPrincipals() {

	STableCell nextCell;
	LArray *ret = new LArray(sizeof(CTicketListPrincipalItem *));
	CTicketListItem *item;
	
	while (GetNextCell(nextCell)) {
		GetItemFromCell(nextCell, &item);
		if (item->GetItemType() == kPrincipalItem)
			ret->AddItem(&item);
	}
	
	return ret;
}

// ---------------------------------------------------------------------------
//		¥ GetPrincipalCount
// ---------------------------------------------------------------------------
// count how many main principals there are (really, how many caches there are)
short CPrincList::GetPrincipalCount() {

	STableCell nextCell;
	short principalCount = 0;
	CTicketListItem *item;
	
	while (GetNextCell(nextCell)) {
		GetItemFromCell(nextCell, &item);
		if (item->GetItemType() == kPrincipalItem)
			principalCount++;
	}
	
	return principalCount;
}

// ---------------------------------------------------------------------------
//		¥ PrincipalInList
// ---------------------------------------------------------------------------
// return true if the named principal is already in the list
// KCP 1.5b2 - this function doesn't appear to be used anymore
bool
CPrincList::PrincipalInList(char *inPrincipal) {

	STableCell nextCell;
	CTicketListItem *item;
	
	//look through the list (rather than the cache) for the principal in question
	while (GetNextCell(nextCell)) {
		GetItemFromCell(nextCell, &item);
		if (item->GetItemType() == kPrincipalItem) {
			Str255 itemPrincipalString;
			item->GetItemPrincipalString(&itemPrincipalString);
			if (LString::CompareBytes(inPrincipal, itemPrincipalString+1, (UInt8)strlen(inPrincipal), itemPrincipalString[0]) == 0) {
				return true;
			}
		}
	}
	
	return false;
	
}


// ---------------------------------------------------------------------------
//		¥ GetAnotherPrincipal
// ---------------------------------------------------------------------------
// go through the princ list and find get the next principal which is not
// the princToIgnore
// KCP 1.5b2 - this function doesn't appear to be used anymore
bool CPrincList::GetAnotherPrincipal(Str255 outPrinc, Str255 princToIgnore) {

	STableCell nextCell;
	CTicketListItem *item;
	
	//look through the list
	while (GetNextCell(nextCell)) {
		GetItemFromCell(nextCell, &item);
		// if this item is a principal
		if ( item->GetItemType() == kPrincipalItem )   {
			Str255 itemPrincipalString;
			item->GetItemPrincipalString(&itemPrincipalString);
			
			// if we're not supposed to ignore this principal (because it's the one about to be removed)
			if (LString::CompareBytes(itemPrincipalString, princToIgnore, (UInt8)(itemPrincipalString[0]+1), (UInt8)(princToIgnore[0]+1)) != 0) {
				// then update out stored time and the result we're going to pass back
				LString::CopyPStr(itemPrincipalString, outPrinc, (SInt16)(itemPrincipalString[0]+1));
				continue;
			}
		}
	}
	
	return (outPrinc[0] != 0);
	
}
