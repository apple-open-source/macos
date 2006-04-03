/*
 * CCICache.h
 *
 * $Header$
 */
 
#pragma once

#include "Magic.h"
#include "Internal.h"
#include "Internalize.h"
#include "CCache.h"

class CCECCacheIterator {
	public:
		static cc_int32 Release (
			cc_ccache_iterator_t	inCCache);
			
		static cc_int32 Next (
			cc_ccache_iterator_t	inIterator,
			cc_ccache_t*			outCCache);
			
	private:
		// Disallowed
		CCECCacheIterator ();
		CCECCacheIterator (const CCECCacheIterator&);
		CCECCacheIterator& operator = (const CCECCacheIterator&);
};

class CCIContext;

class CCICCacheIterator:
	public CCIMagic <CCICCacheIterator>,
	public CCIInternal <CCICCacheIterator, cc_ccache_iterator_d> {

	public:
		CCICCacheIterator (
			const	CCIContext&		inContext,
			CCIInt32				inAPIVersion);
			
		~CCICCacheIterator ();
			
		bool
			HasMore () const;

		CCIUniqueID
			Next ();
			
		CCIUniqueID
			Current ();
			
#if CCache_v2_compat
		// Needed by v2 compat iterator implementation
		void		CompatResetRepeatCount () { mRepeatCount = 0; }
		void		CompatIncrementRepeatCount () { mRepeatCount ++; }
		CCIUInt32	CompatGetRepeatCount () { return mRepeatCount; }
#endif
			
		enum {
			class_ID = FOUR_CHAR_CODE ('CCIt'),
			invalidObject = ccErrInvalidCCacheIterator
		};
		
		CCIInt32 GetAPIVersion () const { return mAPIVersion; }
		
	private:
		std::vector <CCIObjectID>						mIterationSet;
		std::vector <CCIObjectID>::iterator				mIterator;
		std::auto_ptr <CCIContext>						mContext;
		CCILockID										mContextLock;
#if CCache_v2_compat
		CCIUInt32										mRepeatCount;
#endif
		CCIInt32										mAPIVersion;
		
		void		Validate ();

		static const	cc_ccache_iterator_f	sFunctionTable;

		friend class StInternalize <CCICCacheIterator, cc_ccache_iterator_d>;
		friend class CCIInternal <CCICCacheIterator, cc_ccache_iterator_d>;

		// Disallowed
		CCICCacheIterator (const CCICCacheIterator&);
		CCICCacheIterator& operator = (const CCICCacheIterator&);
};

typedef StInternalize <CCICCacheIterator, cc_ccache_iterator_d>	StCCacheIterator;

