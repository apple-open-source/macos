/* $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/Headers/CCache.debug.h,v 1.22 2003/03/17 20:46:13 lxs Exp $ */

/* $Copyright:
 *
 * Copyright 1998-2000 by the Massachusetts Institute of Technology.
 * 
 * All rights reserved.
 * 
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of M.I.T. not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Furthermore if you modify
 * this software you must label your software as modified software and not
 * distribute it in such a fashion that it might be confused with the
 * original MIT software. M.I.T. makes no representations about the
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

/* $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/Headers/CCache.debug.h,v 1.22 2003/03/17 20:46:13 lxs Exp $ */

/*
 * CCache debugging utilities
 */
 
#ifndef CCache_debug_h__
#define CCache_debug_h__

#include "CCache.config.h"

#include <TargetConditionals.h>
#include <Kerberos/KerberosDebug.h>

#ifdef CCI_DEBUG
#define	CCIAssert_(x)		CCIDEBUG_ASSERT(x)
#define CCISignal_(x)		CCIDEBUG_SIGNAL(x)
#define CCIValidPointer_(x)	CCIDEBUG_VALIDPOINTER(x)
#define CCIDebugThrow_(x)	CCIDEBUG_THROW(x)
#else
#define CCIAssert_(x)
#define CCISignal_(x)
#define CCIValidPointer_(x)	cc_true
#define CCIDebugThrow_(x)	throw (x)
#endif

#define CCIBeginSafeTry_											\
	try
	
#define CCIEndSafeTry_(resultVariable, exceptionError)				\
	catch (std::bad_alloc&) { resultVariable = ccErrNoMem; }		\
	catch (CCIException& e) { resultVariable = e.Error (); }	\
	catch (...) { resultVariable = exceptionError; } 

#endif /* CCache_debug_h__ */
