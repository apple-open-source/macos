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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/CTableTLIPtrArrayStorage.cp,v 1.5 2002/04/09 19:46:13 smcguire Exp $ */

// =================================================================================
//	CTableTLIPtrArrayStorage.cp
// =================================================================================

/* Simple subclass of LTableArrayStorage that assumes the items in the array
   are pointed to CTicketListItem objects, so that the storage for those objects should be
   deleted before the array entries are removed, to prevent memory leaks. */
   
#include "CTableTLIPtrArrayStorage.h"
#include "CTicketListItem.h"

PP_Using_Namespace_PowerPlant

// ---------------------------------------------------------------------------
//	¥ CTableTLIPtrArrayStorage					Constructor				  [public]
// ---------------------------------------------------------------------------
// just pass arguments along to the superclass

CTableTLIPtrArrayStorage::CTableTLIPtrArrayStorage(
	LTableView	*inTableView,
	UInt32		inDataSize)
		: LTableArrayStorage(inTableView, inDataSize)
{
}

// ---------------------------------------------------------------------------
//	¥ CTableTLIPtrArrayStorage					Constructor				  [public]
// ---------------------------------------------------------------------------
// just pass arguments along to the superclass

CTableTLIPtrArrayStorage::CTableTLIPtrArrayStorage(
	LTableView		*inTableView,
	LArray			*inDataArray)
		: LTableArrayStorage(inTableView, inDataArray)
{
}

// ---------------------------------------------------------------------------
//	¥ ~CTableTLIPtrArrayStorage					Destructor				  [public]
// ---------------------------------------------------------------------------
// does nothing

CTableTLIPtrArrayStorage::~CTableTLIPtrArrayStorage()
{
}

// ---------------------------------------------------------------------------
//	¥ RemoveRows													  [public]
// ---------------------------------------------------------------------------
//	Removes rows from an PtrArrayStorage
//  Deletes object pointed to by item storage, then calls inherited method

void
CTableTLIPtrArrayStorage::RemoveRows(
	UInt32		inHowMany,
	TableIndexT	inFromRow)
{
	STableCell	cellToFree(inFromRow, 1);
	UInt32		dataSize;
	CTicketListItem		*ptrToFree;

	TableIndexT	rows, cols;
	mTableView->GetTableSize(rows, cols);
	
	for (cellToFree.row = inFromRow; cellToFree.row < inFromRow+inHowMany; cellToFree.row++) {
		for (cellToFree.col = 1; cellToFree.col <= cols; cellToFree.col++) {
			
			// map cell to array index
			//mTableView->CellToIndex(cellToFree, arrayIndex);
			
			mTableView->GetCellData(cellToFree, nil, dataSize);
			mTableView->GetCellData(cellToFree, &ptrToFree, dataSize);
			
			delete ptrToFree; 
			
		}
	}
		
	// call superclass's method
	LTableArrayStorage::RemoveRows(inHowMany, inFromRow);
}


// ---------------------------------------------------------------------------
//	¥ RemoveCols													  [public]
// ---------------------------------------------------------------------------
//	Removes columns from an PtrArrayStorage
//  Deletes object pointed to by item storage, then calls inherited method

void
CTableTLIPtrArrayStorage::RemoveCols(
	UInt32		inHowMany,
	TableIndexT	inFromCol)
{
	
	STableCell	cellToFree(1, inFromCol);
	UInt32		arrayIndex, dataSize;
	CTicketListItem		*ptrToFree;

	TableIndexT	rows, cols;
	mTableView->GetTableSize(rows, cols);
	
	for (cellToFree.row = 1; cellToFree.row <= rows; cellToFree.row++) {
		for (cellToFree.col = inFromCol; cellToFree.col < inFromCol + inHowMany; cellToFree.col++) {
			
			// map cell to array index
			mTableView->CellToIndex(cellToFree, arrayIndex);
			
			mTableView->GetCellData(arrayIndex, nil, dataSize);
			mTableView->GetCellData(arrayIndex, &ptrToFree, dataSize);
			
			delete ptrToFree; 
			
		}
	}

	// call superclass's method
	LTableArrayStorage::RemoveCols(inHowMany, inFromCol);
}
