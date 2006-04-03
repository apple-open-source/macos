/* $Copyright:
 *
 * Copyright 1998-2000 by the Massachusetts Institute of Technology.
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

/* $Header$ */

/*
 * Internal cache manipulation functions for CCache library
 */
 
#ifndef CCache_internal_h__
#define CCache_internal_h__
 
#include <Kerberos/CredentialsCache.h>

#define kInitialDefaultCCacheName "Initial default ccache"

/*
 * Internal types
 */
 
typedef		cc_uint32		cc_lock;
typedef		cc_int32		cc_bool;
typedef		cc_uint32		cc_ccache_id;
typedef		cc_uint32		cc_context_id;
typedef		cc_uint32		cc_credentials_id;
typedef		cc_uint32		cc_default_id;

typedef		cc_int32		CCIInt32;
typedef		cc_uint32		CCIUInt32;
typedef		cc_int32		CCIResult;
typedef		cc_time_t		CCITime;

typedef		CCIUInt32		CCIObjectID;

struct CCIUniqueID {
                CCIObjectID		object;
                
                #ifdef __cplusplus
                CCIUniqueID () {
                }
                
                CCIUniqueID (const CCIUniqueID& inOtherID) {
                	object = inOtherID.object;
                }
                
                CCIUniqueID (const CCIObjectID inObjectID):
                    object (inObjectID) {
                }
                
                bool operator == (const struct CCIUniqueID& inCompareTo) const {
                    return (object == inCompareTo.object);
                }
                
                bool operator < (const CCIUniqueID& inRhs) const {
                    return object < inRhs.object;
                }
                #endif
};

typedef struct CCIUniqueID CCIUniqueID;


/*
 * Internal constants
 */

enum {
	cc_true		= 1,
	cc_false	= 0
};

enum {
	cc_credentials_none = 0
};

// Dummy for now
typedef int CCILockID;

#ifdef __cplusplus

// Stack-based read lock utility class (lock in ctor, unlock in dtor)
class StReadLock {
	public:
		StReadLock (
			CCILockID inLockID):
			mLockID (inLockID)
		{
		}

		operator CCILockID () const
		{
			return mLockID;
		}
		
	private:
		CCILockID		mLockID;
};

// Stack-based write lock utility class (lock in ctor, unlock in dtor)
class StWriteLock {
	public:
		StWriteLock (
			CCILockID inLockID):
			mLockID (inLockID)
		{
		}

		operator CCILockID () const
		{
			return mLockID;
		}
		
	private:
		CCILockID		mLockID;
};

#endif /* __cplusplus */

#endif /* __CCache_internal_h__ */
