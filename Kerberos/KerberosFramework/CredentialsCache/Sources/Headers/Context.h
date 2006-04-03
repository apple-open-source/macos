/*
 * CCIContext.h
 *
 * $Header$
 */
 
#include "Internalize.h"
#include "Internal.h"
#include "Magic.h"

#pragma once

#if PRAGMA_MARK
#pragma mark CCEContext
#endif

//
// CCEContext
//
// Implements static exported functions -- outermost layer of the API
//
 
class CCEContext {
public:
		static cc_int32 Clone (
			cc_context_t		inContext,
			cc_context_t*		outContext,
			cc_int32			inVersion,
			cc_int32*			outSupportedVersion,
			char const**		outVendor);

		static cc_int32 GetVersion (
			cc_context_t		inContext,
			cc_int32*			outVersion);
			
		static cc_int32 GetChangeTime (
			cc_context_t		inContext,
			cc_time_t*			outTime);

		static cc_int32 Release (
			cc_context_t		inContext);

		static cc_int32 GetDefaultCCacheName (
			cc_context_t		inContext,
			cc_string_t*		outName);

		static cc_int32 OpenCCache (
			cc_context_t		inContext,
			const char*			inName,
			cc_ccache_t*		outCCache);

		static cc_int32 OpenDefaultCCache (
			cc_context_t		inContext,
			cc_ccache_t*		outCCache);
			
		static cc_int32 CreateCCache (
			cc_context_t		inContext,
			const char*			inName,
			cc_uint32			inVersion,
			const char*			inPrincipal,
			cc_ccache_t*		outCCache);
			
		static cc_int32 CreateDefaultCCache (
			cc_context_t		inContext,
			cc_uint32			inVersion,
			const char*			inPrincipal,
			cc_ccache_t*		outCCache);
			
		static cc_int32 CreateNewCCache (
			cc_context_t		inContext,
			cc_uint32			inVersion,
			const char*			inPrincipal,
			cc_ccache_t*		outCCache);
			
		static cc_int32 NewCCacheIterator (
			cc_context_t			inContext,
			cc_ccache_iterator_t*	outIter);
			
		static cc_int32 Lock (
			cc_context_t			inContext,
			cc_uint32				inReadWrite,
			cc_uint32				inBlock);
			
		static cc_int32 Unlock (
			cc_context_t			inContext);
			
		static cc_int32 Compare (
			cc_context_t			inContext,
			cc_context_t			inCompareTo,
			cc_uint32*				outEqual);
			
	private:
		// Disallowed
		CCEContext ();
		CCEContext (const CCEContext&);
		CCEContext& operator = (const CCEContext&);
};

#if PRAGMA_MARK
#pragma mark CCIContext
#endif

//
// CCIContext
//
// Object which implements a reference to a ccache collection
//

class CCIContext:
	public CCIMagic <CCIContext>,
	public CCIInternal <CCIContext, cc_context_d>
{
	public:
		enum {
			class_ID = FOUR_CHAR_CODE ('CCTX'),
			invalidObject = ccErrInvalidContext
		};
	
					CCIContext (
						CCIUniqueID						inContextID,
						CCIInt32						inAPIVersion);
				
				CCIInt32
					GetAPIVersion () const
					{
						return mAPIVersion;
					}
				
		virtual 	~CCIContext ()
		{
		}

		virtual	CCITime
					GetChangeTime () = 0;
		
		virtual CCIUniqueID
					OpenCCache (
						const std::string&				inCCacheName) = 0;

		virtual	CCIUniqueID
					OpenDefaultCCache () = 0;
	
		virtual	std::string
					GetDefaultCCacheName () = 0;

		virtual	CCIUniqueID
					CreateCCache (
						const std::string&				inName,
						CCIUInt32						inVersion,
						const std::string&				inPrincipal) = 0;

		virtual	CCIUniqueID
					CreateDefaultCCache (
						CCIUInt32						inVersion,
						const std::string&				inPrincipal) = 0;

		virtual	CCIUniqueID
					CreateNewCCache (
						CCIUInt32						inVersion,
						const std::string&				inPrincipal) = 0;
				
		virtual	void
					GetCCacheIDs (
						std::vector <CCIObjectID>&		outCCacheIDs) const = 0;
				
		virtual	CCILockID
					Lock () = 0;
		
		virtual	void
					Unlock (
						CCILockID						inLock) = 0;
				
		virtual	bool
					Compare (
						const CCIContext&				inCompareTo) const = 0;

				const CCIUniqueID&
					GetContextID () const
					{
						return mContextID;
					}
                
                void
                    SetContextID (CCIUniqueID newContextID) const
                    {
                        mContextID = newContextID;
                    }
			
	private:
	
		mutable CCIUniqueID		mContextID;
		CCIInt32				mAPIVersion;

		void Validate ();

	 	friend cc_int32 cc_initialize (
			cc_context_t*		outContext,
			cc_int32			inVersion,
			cc_int32*			outSupportedVersion,
			char const**		outVendor);
			
		static const cc_context_f sFunctionTable;
			
		friend class StInternalize <CCIContext, cc_context_d>;
		friend class CCIInternal <CCIContext, cc_context_d>;

		// Disallowed
		CCIContext ();
		CCIContext (const CCIContext&);
		CCIContext& operator = (const CCIContext&);
};

typedef StInternalize <CCIContext, cc_context_d>	StContext;
