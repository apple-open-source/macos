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

/* $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/Headers/CTableTLIPtrArrayStorage.h,v 1.4 2002/04/09 19:46:15 smcguire Exp $ */

// =================================================================================
//	CTableTLIPtrArrayStorage.h
// =================================================================================

/* Simple subclass of LTableArrayStorage that assumes the items in the array
   are pointed to CTicketListItem objects, so that the storage for those objects should be
   deleted before the array entries are removed, to prevent memory leaks. */

#pragma once

#include <LTableArrayStorage.h>

class	CTableTLIPtrArrayStorage : public PP_PowerPlant::LTableArrayStorage {
public:
						CTableTLIPtrArrayStorage(
								PP_PowerPlant::LTableView	*inTableView,
								UInt32				inDataSize);
								
						CTableTLIPtrArrayStorage(
								PP_PowerPlant::LTableView	*inTableView,
								PP_PowerPlant::LArray				*inDataArray);
								
	virtual				~CTableTLIPtrArrayStorage();
	
	virtual void		RemoveRows(
								UInt32				inHowMany,
								PP_PowerPlant::TableIndexT	inFromRow);
	virtual void		RemoveCols(
								UInt32				inHowMany,
								PP_PowerPlant::TableIndexT	inFromCol);
									
};