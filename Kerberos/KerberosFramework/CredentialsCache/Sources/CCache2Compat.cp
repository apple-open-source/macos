/*
 * This is backwards compatibility for CCache API v2 clients to be able to run 
 * against the CCache API v3 library
 *
 * The tricky parts in the compatibility layer are:
 *   - layout of credentials structures changed. we have to remap one to the other in
 *     the compat layer
 *   - semantics of ccaches changed: v2 clients expect one type of credentials per
 *     ccache. ccache iterators in the compat layer have to return dual ccaches twice
 *     (once for v4, once for v5). credentials iterators have to return only a subset
 *     of the credentials that the caller wants.
 */

#include "CCache2Compat.h"

#include "CCache.h"
#include "CCacheIterator.h"
#include "CCacheString.h"
#include "Pointer.h"
#include "CredentialsIterator.h"
#include "AbstractFactory.h"

#if CCache_v2_compat

cc_result	cc_remap_error (cc_result	inNewError);

cc_result	cc_remap_error (cc_result	inNewError) {
	switch (inNewError) {
		case ccNoError:
			return CC_NOERROR;
		
		case ccIteratorEnd:
			return CC_END;
		
		case ccErrBadParam:
		case ccErrInvalidCredentials:
		case ccErrInvalidCCacheIterator:
		case ccErrInvalidCredentialsIterator:
		case ccErrBadLockType:
			return CC_BAD_PARM;
			
		case ccErrNoMem:
			return CC_NOMEM;
		
		case ccErrInvalidContext:
		case ccErrInvalidCCache:
		case ccErrCCacheNotFound:
			return CC_NO_EXIST;
		
		case ccErrCredentialsNotFound:
			return CC_NOTFOUND;
		
		case ccErrBadName:
			return CC_BADNAME;
			
		case ccErrContextLocked:
		case ccErrContextUnlocked:
		case ccErrCCacheLocked:
		case ccErrCCacheUnlocked:
			return CC_LOCKED;
		
        case ccErrServerUnavailable:
        case ccErrServerInsecure:
            return CC_IO;
        
		default:
			dprintf ("%s(): Unhandled error", __FUNCTION__);
			return CC_BAD_PARM;
	}
}


cc_result cc_shutdown (
	apiCB**		ioContext) {
	
	cc_result result = cc_context_release (*ioContext);
	
	if (result == ccNoError) {
		*ioContext = NULL;
	}
	
	result = cc_remap_error (result);
	
	CCIAssert_ ((result == CC_NOERROR) ||
				(result == CC_NO_EXIST) ||
				(result == CC_BAD_PARM));
	
	return result;
}

cc_result cc_get_NC_info (
	apiCB*			inContext,
	infoNC***		outInfo)
{
	cc_result	result = ccNoError;

	ccache_cit*	iterator = NULL;
	infoNC**	newArray = NULL;
	ccache_p*	ccache = NULL;

	// Preflight the size 
	UInt32		numCaches = 0;

	result = cc_seq_fetch_NCs_begin (inContext, &iterator);
	if (result != CC_NOERROR)
		goto end;

	for (;;) {
		result = cc_seq_fetch_NCs_next (inContext, &ccache, iterator);
		if (result == CC_END) {
			break;
		} else if (result != CC_NOERROR) {
			goto end;
		}
		
		numCaches++;
		cc_close (inContext, &ccache);
		ccache = NULL;
	}
	
	result = cc_seq_fetch_creds_end (inContext, &iterator);
	if (result != CC_NOERROR)
		goto end;
	
	
	// Done preflighting, allocate the array
	CCIBeginSafeTry_ {
		newArray = new infoNC* [numCaches + 1];
		for (UInt32 i = 0; i < numCaches + 1; i++) {
			newArray [i] = NULL;
		}
		
		// Fill the array	
		result = cc_seq_fetch_NCs_begin (inContext, &iterator);
		if (result != CC_NOERROR)
			goto end;

		for (UInt32 j = 0; j < numCaches; j++) {
			result = cc_seq_fetch_NCs_next (inContext, &ccache, iterator);
			if (result == CC_END) {
				break;
			} else if (result != CC_NOERROR) {
				goto end;
			}

			infoNC*		newItem = new infoNC;
			newItem -> name = NULL;
			newItem -> principal = NULL;
			newItem -> vers = CC_CRED_UNKNOWN;
			
			newArray [j] = newItem;

			result = cc_get_name (inContext, ccache, &newItem -> name);
			if (result != CC_NOERROR) {
				goto end;
			}
				
			result = cc_get_principal (inContext, ccache, &newItem -> principal);
			if (result != CC_NOERROR) {
				goto end;
			}
			
			result = cc_get_cred_version (inContext, ccache, &newItem -> vers);
			if (result != CC_NOERROR) {
				goto end;
			}
					
			cc_close (inContext, &ccache);
			ccache = NULL;
		}
		
		result = cc_seq_fetch_creds_end (inContext, &iterator);
		if (result != CC_NOERROR)
			goto end;

		*outInfo = newArray;
	} catch (std::bad_alloc&) {
		result = CC_NOMEM;
		goto end;
	} catch (...) {
		result = CC_BAD_PARM;
		goto end;
	}
	
end:
	if ((newArray != NULL) && (result != CC_NOERROR))
		cc_free_NC_info (inContext, &newArray);
	if (iterator != NULL)
		cc_seq_fetch_NCs_end (inContext, &iterator);
	if (ccache != NULL) 
		cc_close (inContext, &ccache);
		
	CCIAssert_ ((result == CC_NOERROR) ||
				(result == CC_NO_EXIST) ||
				(result == CC_NOMEM) ||
				(result == CC_BAD_PARM));

	return result;
}
	

/*cc_result cc_get_NC_info (
	apiCB*			inContext,
	infoNC***		outInfo) {
	
#pragma unused (inContext)

	// This function has no counterpart in CCAPI v3
	infoNC** info = NULL;
	cc_uint32 i;
	
	cc_result result = ccNoError;
	
	// Note that we overallocate here. We are creating the initial array to be
	// able to accomodate one v4 and one v5 ccache for each ccache in the list
	// +1 for the terminating null. That's okay, because the specification says
	// "NULL terminated array of pointers", so other the potential waste of space
	// there is no problem
	info = (infoNC**) cci_malloc ((2 * gGlobalCCacheData.numCCaches + 1) * sizeof (infoNC*));
	
	if (info == NULL) {
		result = ccErrNoMem;
	} else {
		for (i = 0; i < 2 * gGlobalCCacheData.numCCaches + 1; i++) {
			info [i] = NULL;
		}
	}

	
	if (result == ccNoError) {
		// Note that this loop us pretty perverse. With i we are indexing over the array
		// allocated above, and ccache data is walking the list of ccaches. However,
		// a single ccachemight generate more than one array entry (because of
		// ccache duality in ccapi v3), so ccacheData is not advanced every time.
		// Throughout, seenCount is the number of times the ccache at ccacheData has
		// already been entered in the array (seenCount = 0 or 1)

		cci_ccache_data* ccacheData = gGlobalCCacheData.ccacheListHead;
		cc_int32 seenCount = 0;
		cc_bool advance = cc_false;
		
		for (i = 0; ccacheData != NULL; i++) {
			char* principal;
			cc_int32 version;

			CCIAssert_ (i < 2 * gGlobalCCacheData.numCCaches);
			CCIAssert_ ((seenCount == 0) || (seenCount == 1));
			
			// See if we are adding the v4 principal, which happens only the first
			// time we see a ccache that has a v4 principal
			if ((seenCount == 0) && (ccacheData -> v4principal != NULL)) {
				// We haven't seen this ccache already
				// If it has a v4 principal, we will add it now
				if (ccacheData -> v4principal != NULL) {
					principal = ccacheData -> v4principal;
					version = CC_CRED_V4;
					
					// If it _only_ has v4 principal, we will advance the pointer
					// for the next iteration
					if (ccacheData -> v5principal == NULL) {
						advance = cc_true;
					} else {
						advance = cc_false;
					}
				}
			} else {
				// We are adding a v5 principal, either because this is the
				// second time we are seeing this ccache, or because it has no v4
				// principal
				CCIAssert_ (ccacheData -> v5principal != NULL);
				principal = ccacheData -> v5principal;
				advance = cc_true;
				version = CC_CRED_V5;
			}
					

			info [i] = (infoNC*) cci_malloc (sizeof (infoNC));
			if (info [i] == NULL) {
				result = ccErrNoMem;
				break;
			}
			info [i] -> name = info [i] -> principal = NULL;
			info [i] -> vers = version;
			
			result = cci_string_copy (principal, &info [i] -> principal);
			result = cci_string_copy (ccacheData -> name, &info [i] -> name);
			
			if (advance) {
				ccacheData = ccacheData -> next;
				seenCount = 0;
			} else {
				seenCount++;
			}
		}
	}
	
	if (result == ccNoError) {
		*outInfo = info;
	}
	
	if (result != ccNoError) {
		cci_compat_deep_free_NC_info (info);
	}
		
	result = cc_remap_error (result);
	
	CCIAssert_ ((result == CC_NOERROR) ||
				(result == CC_NO_EXIST) ||
				(result == CC_NOMEM) ||
				(result == CC_BAD_PARM));
	
	return result;
}*/

cc_result cc_get_change_time (
	apiCB*		inContext,
	cc_time_t*	outTime) {
	
	cc_result result = cc_context_get_change_time (inContext, outTime);
	
	result = cc_remap_error (result);
	
	CCIAssert_ ((result == CC_NOERROR) ||
				(result == CC_NO_EXIST) ||
				(result == CC_NOMEM) ||
				(result == CC_BAD_PARM));
	
	return result;
}

cc_int32 cc_open (
	apiCB*		inContext,
	const char*	inName,
	cc_int32	inVersion,
	cc_uint32	/* inFlags */,
	ccache_p**	outCCache) {

	cc_ccache_t	returnedCCache = NULL;
	cc_result result = ccNoError;
	
	if ((inVersion != CC_CRED_V4) && (inVersion != CC_CRED_V5)) {
		result = ccErrBadCredentialsVersion;
	}
	
	if (result == ccNoError) {
		result = cc_context_open_ccache (inContext, inName, &returnedCCache);
	}

	// We must not allow a CCAPI v2 caller to open a v5-only ccache
	// as a v4 ccache and vice versa. Allowing that would break (valid)
	// assumptions made by CCAPI v2 callers.
	if (result == ccNoError) {
		CCIBeginSafeTry_ {
			StCCache			ccache (returnedCCache);
			
			if (inVersion == CC_CRED_V4) {
				if ((ccache -> GetCredentialsVersion () & cc_credentials_v4) != 0) {
					ccache -> CompatSetVersion (cc_credentials_v4);
					*outCCache = ccache;
				} else {
					// ccache version mismatch, bail with an error
					cc_ccache_release (returnedCCache);
					result = ccErrInvalidCCache;
				}
			} else if (inVersion == CC_CRED_V5) {
				if ((ccache -> GetCredentialsVersion () & cc_credentials_v5) != 0) {
					ccache -> CompatSetVersion (cc_credentials_v5);
					*outCCache = ccache;
				} else {
					// ccache version mismatch, bail with an error
					cc_ccache_release (returnedCCache);
					result = ccErrInvalidCCache;
				}
			} else {
				result = ccErrBadCredentialsVersion;
			}
		} CCIEndSafeTry_ (result, ccErrBadParam)
	}
	
	result = cc_remap_error (result);
	
	CCIAssert_ ((result == CC_NOERROR) ||
				(result == CC_BADNAME) ||
				(result == CC_ERR_CRED_VERSION) ||
				(result == CC_NO_EXIST) ||
				(result == CC_NOMEM) ||
				(result == CC_BAD_PARM));
	
	return result;
}

cc_result cc_create (
	apiCB*		inContext,
	const char*	inName,
	const char*	inPrincipal,
	cc_int32	inVersion,
	cc_uint32	/* inFlags */,
	ccache_p**	outCCache) {
	
	cc_result result =
		cc_context_create_ccache (inContext, inName, static_cast <cc_uint32> (inVersion), inPrincipal, outCCache);
	
	if (result == ccNoError) {
		CCIBeginSafeTry_ {
			StCCache		ccache (*outCCache);
			if (inVersion == CC_CRED_V4) {
				ccache -> CompatSetVersion (cc_credentials_v4);
			} else if (inVersion == CC_CRED_V5) {
				ccache -> CompatSetVersion (cc_credentials_v5);
			} else {
				result = ccErrBadCredentialsVersion;
			}
		} CCIEndSafeTry_ (result, ccErrBadParam)
	}
	
	result = cc_remap_error (result);
	
	CCIAssert_ ((result == CC_NOERROR) ||
				(result == CC_BADNAME) ||
				(result == CC_ERR_CRED_VERSION) ||
				(result == CC_NO_EXIST) ||
				(result == CC_NOMEM) ||
				(result == CC_BAD_PARM));
	
	return result;
}

cc_result cc_close (
	apiCB*		/* inContext */,
	ccache_p**	ioCCache) {
	
	cc_result result = cc_ccache_release (*ioCCache);
	
	if (result == ccNoError) {
		*ioCCache = NULL;
	}
	result = cc_remap_error (result);
	
	CCIAssert_ ((result == CC_NOERROR) ||
				(result == CC_NO_EXIST) ||
				(result == CC_BAD_PARM));
	
	return result;
}

cc_result cc_destroy (
	apiCB*		/* inContext */,
	ccache_p**	ioCCache) {
	
	cc_result result = cc_ccache_destroy (*ioCCache);
	
	if (result == ccNoError) {
		*ioCCache = NULL;
	}

	result = cc_remap_error (result);
	
	CCIAssert_ ((result == CC_NOERROR) ||
				(result == CC_NO_EXIST) ||
				(result == CC_BAD_PARM));
	
	return result;
}

// CCache iterators need to return some ccaches twice (when v3 ccache has
// two kinds of credentials). To do that, we use a single v3 iterator, but
// sometimes don't advance it.

cc_result cc_seq_fetch_NCs_begin (
	apiCB*				inContext,
	ccache_cit**		outIterator) {
	
	cc_result result = cc_context_new_ccache_iterator (inContext, (ccache_cit_ccache**) outIterator);
	
	result = cc_remap_error (result);
	
	CCIAssert_ ((result == CC_NOERROR) ||
				(result == CC_NOMEM) ||
				(result == CC_NO_EXIST) ||
				(result == CC_BAD_PARM));
	
	return result;
}

cc_result cc_seq_fetch_NCs_next (
	apiCB*				/* inContext */,
	ccache_p**			outCCache,
	ccache_cit*			inIterator) {
	
	CCIResult	result = ccNoError;
	
	CCIBeginSafeTry_ {
		StCCacheIterator			iterator (reinterpret_cast <ccache_cit_ccache*> (inIterator));
		
		if (iterator -> HasMore ()) {
			StPointer <cc_ccache_t>		newCCache (outCCache);

			// We've never returned this ccache before
			if (iterator -> CompatGetRepeatCount () == 0) {
				StCCache					current =
					CCIAbstractFactory::GetTheFactory () -> CreateCCache (
						iterator -> Current (), iterator -> GetAPIVersion ());
				newCCache = current;
				iterator -> CompatIncrementRepeatCount ();

				// We are opening it in single-version mode v4 if possible first
				if ((current -> GetCredentialsVersion () & cc_credentials_v4) != 0) {
					current -> CompatSetVersion (cc_credentials_v4);
				} else {
					current -> CompatSetVersion (cc_credentials_v5);
				}

			// We've returned this cache once before
			} else {
				std::auto_ptr <CCICCache>		current (CCIAbstractFactory::GetTheFactory () -> CreateCCache (
															iterator -> Current (), iterator -> GetAPIVersion ()));
				
				CCIUInt32		version = cc_credentials_none;
				bool			alreadyDeleted = false;
				// It is possible that the ccache has been deleted since our first visit.
				// If that's the case, GetCredentialsVersion will throw an appropriate
				// error. We handle that case by proceeding to the next ccache
				try {
					version = current -> GetCredentialsVersion ();
				} catch (CCIException& e) {
					// If we encounter an error other than ccErrCCacheNotFound, we
					// don't know what to do with it, so we rethrow. 
					if (e.Error () != ccErrCCacheNotFound)
						throw;
					else
						alreadyDeleted = true;
				}
				
				// The ccache has only one version, or it has been deleted; go to next
				if (version != cc_credentials_v4_v5) {
					iterator -> Next ();
					StCCache	next =
						CCIAbstractFactory::GetTheFactory () -> CreateCCache (
							iterator -> Current (), iterator -> GetAPIVersion ());
					newCCache = next;

					// We are opening it in single-version mode v4 if possible first
					if ((next -> GetCredentialsVersion () & cc_credentials_v4) != 0) {
						next -> CompatSetVersion (cc_credentials_v4);
					} else {
						next -> CompatSetVersion (cc_credentials_v5);
					}

					iterator -> CompatResetRepeatCount ();
					iterator -> CompatIncrementRepeatCount ();
					
				// The ccache has two versions, and we are seeing it the second time, so
				// it must be opened in v5 mode
				} else {
					StCCache	ccache =
						CCIAbstractFactory::GetTheFactory () -> CreateCCache (
							current -> GetCCacheID (), iterator -> GetAPIVersion ());
					ccache -> CompatSetVersion (cc_credentials_v5);

					newCCache = ccache;
				}
			}
		} else {
			result = ccIteratorEnd;
		}
	} CCIEndSafeTry_ (result, ccErrBadParam)

	result = cc_remap_error (result);
	
	CCIAssert_ ((result == CC_NOERROR) ||
				(result == CC_END) ||
				(result == CC_NOMEM) ||
				(result == CC_NO_EXIST) ||
				(result == CC_BAD_PARM));
	
	return result;
}

cc_result cc_seq_fetch_NCs_end (
	apiCB*				/* inContext */,
	ccache_cit**		ioIterator) {
	
	cc_result result = cc_ccache_iterator_release (*(ccache_cit_ccache**)ioIterator);
	
	if (result == ccNoError) {
		*ioIterator = NULL;
	}

	result = cc_remap_error (result);
	
	CCIAssert_ ((result == CC_NOERROR) ||
				(result == CC_NO_EXIST) ||
				(result == CC_BAD_PARM));
	
	return result;
}

cc_result cc_get_name (
	apiCB*		/* inContext */,
	ccache_p*	inCCache,
	char**		outName) {

	cc_result	result = ccNoError;
	
	CCIBeginSafeTry_ {
		StCCache			ccache (inCCache);
		std::string			ccacheName = ccache -> GetName ();
		StPointer <char*>	name (outName);
		
		char*				newName = new char [ccacheName.length () + 1];
		ccacheName.copy (newName, ccacheName.length ());
		newName [ccacheName.length ()] = '\0';
		
		name = newName;
		
	} CCIEndSafeTry_ (result, ccErrBadParam)

	result = cc_remap_error (result);
	
	CCIAssert_ ((result == CC_NOERROR) ||
				(result == CC_NOMEM) ||
				(result == CC_NO_EXIST) ||
				(result == CC_BAD_PARM));
	
	return result;
}

cc_result cc_get_cred_version (
	apiCB*		/* inContext */,
	ccache_p*	inCCache,
	cc_int32*	outVersion) {
	
	cc_result	result = ccNoError;
	
	CCIBeginSafeTry_ {
		StCCache				ccache (inCCache);
		StPointer <cc_int32>	version (outVersion);
		
		if (ccache -> CompatGetVersion () == cc_credentials_v4) {
			version = CC_CRED_V4;
		} else {
			version = CC_CRED_V5;
		}
	} CCIEndSafeTry_ (result, ccErrBadParam);

	result = cc_remap_error (result);
	
	CCIAssert_ ((result == CC_NOERROR) ||
				(result == CC_NO_EXIST) ||
				(result == CC_BAD_PARM));
	
	return result;
}

cc_result cc_set_principal (
	apiCB*			/* inContext */,
	ccache_p*		inCCache,
	cc_int32		inVersion,
	char*			inPrincipal) {
	
	// In ccapi v3 set_principal also deletes credentials. That wasn't the case
	// in ccapi v2, so we can't just call through
	
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StCCache					ccache (inCCache);
		ccache -> CompatSetPrincipal (static_cast <CCIUInt32> (inVersion), inPrincipal);

	} CCIEndSafeTry_ (result, ccErrBadParam) 
	
	result = cc_remap_error (result);
	
	CCIAssert_ ((result == CC_NOERROR) ||
				(result == CC_NO_EXIST) ||
				(result == CC_ERR_CRED_VERSION) ||
				(result == CC_NOMEM) ||
				(result == CC_BAD_PARM));
	
	return result;
}
	
cc_result cc_get_principal (
	apiCB*		/* inContext */,
	ccache_p*	inCCache,
	char**		outPrincipal) {

	cc_result	result = ccNoError;
	
	CCIBeginSafeTry_ {
		StCCache			ccache (inCCache);
		std::string			ccachePrincipal = ccache -> GetPrincipal (ccache -> CompatGetVersion ());
		StPointer <char*>	principal (outPrincipal);
		
		char*				newPrincipal = new char [ccachePrincipal.length () + 1];
		ccachePrincipal.copy (newPrincipal, ccachePrincipal.length ());
		newPrincipal [ccachePrincipal.length ()] = '\0';
		
		principal = newPrincipal;
	} CCIEndSafeTry_ (result, ccErrBadParam)

	result = cc_remap_error (result);
	
	CCIAssert_ ((result == CC_NOERROR) ||
				(result == CC_NOMEM) ||
				(result == CC_NO_EXIST) ||
				(result == CC_BAD_PARM));
	
	return result;
}

cc_result cc_store (
	apiCB*			/* inContext */,
	ccache_p*		inCCache,
	cred_union		inCredentials) {
	
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StCCache					ccache (inCCache);
		ccache -> CompatStoreCredentials (inCredentials);
	} CCIEndSafeTry_ (result, ccErrBadParam) 

	result = cc_remap_error (result);
	
	// v2 spec says we can't return CC_NOMEM (cool, huh?)
	if (result == CC_NOMEM)
		result = CC_ERR_CACHE_FULL;
	
	CCIAssert_ ((result == CC_NOERROR) ||
				(result == CC_ERR_CRED_VERSION) ||
				(result == CC_ERR_CACHE_FULL) ||
				(result == CC_NO_EXIST) ||
				(result == CC_BAD_PARM));
	
	return result;
}
	
	
// In CCAPI v2, you create fake credentials and pass them in; the library
// compares them with all the credentials and removes the appropriate ones.
// In CCAPI v3, you can only remove what you got out of an iterator.
// Unfortunately, we need to fake some error codes...
cc_result cc_remove_cred (
	apiCB*			/* inContext */,
	ccache_p*		inCCache,
	cred_union		inCredentials) {
	
	cc_result	result = ccNoError;
	cc_credentials_iterator_t iterator;
	cc_credentials_t creds;
	
	result = cc_ccache_new_credentials_iterator (inCCache, &iterator);
	if (result != ccNoError)
		result = CC_NOTFOUND;
	
	if (result == ccNoError) {
		for (;;) {
			result = cc_credentials_iterator_next (iterator, &creds);
			if (result != ccNoError) {
				result = CC_NOTFOUND;
				break;
			}
			
			if (cci_compat_credentials_equal (&inCredentials, creds -> data)) {
				result = cc_ccache_remove_credentials (inCCache, creds);
				cc_credentials_release (creds);
				if (result != ccNoError)
					result = CC_NOTFOUND;
				
				break;
			}
		}
	}
	
	if (result == ccNoError)
		result = CC_NOERROR;
	
	CCIAssert_ ((result == CC_NOERROR) ||
				(result == CC_NOTFOUND) ||
				(result == CC_ERR_CACHE_FULL) ||
				(result == CC_NO_EXIST) ||
				(result == CC_BAD_PARM));
	
	return result;
}	
	
cc_result cc_seq_fetch_creds_begin (
	apiCB*				/* inContext */,
	const ccache_p*		inCCache,
	ccache_cit**		outIterator) {
	
	cc_result result = cc_ccache_new_credentials_iterator ((ccache_p*) inCCache, (ccache_cit_creds**) outIterator);
	
	result = cc_remap_error (result);
	
	CCIAssert_ ((result == CC_NOERROR) ||
				(result == CC_NOMEM) ||
				(result == CC_NO_EXIST) ||
				(result == CC_BAD_PARM));
	
	return result;
}

cc_result cc_seq_fetch_creds_next (
	apiCB*				/* inContext */,
	cred_union**		outCreds,
	ccache_cit*			inIterator) {
	
	CCIResult	result = ccNoError;
	
	CCIBeginSafeTry_ {
		StCredentialsIterator			iterator (reinterpret_cast <ccache_cit_creds*> (inIterator));
		StPointer <cred_union*>			newCredentials (outCreds);

		bool found = false;

		// Find creds of the appropriate version
		while (iterator -> HasMore ()) {
			StCompatCredentials					credentials =
				new CCICompatCredentials (iterator -> Next (), iterator -> GetAPIVersion ());
					
			if ((credentials -> GetCredentialsVersion () & iterator -> CompatGetVersion ()) != 0) {
				found = true;
				newCredentials = credentials;
				break;
			}

			delete credentials.Get ();
		}

		if (!found) {
			result = ccIteratorEnd;
		}
	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	result = cc_remap_error (result);
	
	CCIAssert_ ((result == CC_NOERROR) ||
				(result == CC_END) ||
				(result == CC_NOMEM) ||
				(result == CC_NO_EXIST) ||
				(result == CC_BAD_PARM));
	
	return result;
}

cc_result cc_seq_fetch_creds_end (
	apiCB*				/* inContext */,
	ccache_cit**		ioIterator) {
	
	cc_result result = cc_credentials_iterator_release (*(ccache_cit_creds**)ioIterator);
	
	if (result == ccNoError) {
		*ioIterator = NULL;
	}
	
	result = cc_remap_error (result);
	
	CCIAssert_ ((result == CC_NOERROR) ||
				(result == CC_NO_EXIST) ||
				(result == CC_BAD_PARM));
	
	return result;
}

cc_result cc_free_principal (
	apiCB*			/* inContext */,
	char**			ioPrincipal) {
	
	delete [] *ioPrincipal;
	*ioPrincipal = NULL;
	return CC_NOERROR;
}

cc_result cc_free_name (
	apiCB*			/* inContext */,
	char**			ioName) {
	
	delete [] *ioName;
	*ioName = NULL;
	return CC_NOERROR;
}

cc_result cc_free_creds (
	apiCB*			/* inContext */,
	cred_union**	inCredentials) {
	
	CCIResult	result = ccNoError;
	
	CCIBeginSafeTry_ {
		StCompatCredentials			credentials (*inCredentials);
		
		delete credentials.Get ();
		*inCredentials = NULL;
		return CC_NOERROR;
	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	result = cc_remap_error (result);
	
	CCIAssert_ (result == CC_NOERROR);
	
	return result;
}

cc_result cc_free_NC_info (
	apiCB*			inContext,
	infoNC***		ioInfo) {
	
	cci_compat_deep_free_NC_info (inContext, *ioInfo);
	
	*ioInfo = NULL;
	
	return CC_NOERROR;
}

void cci_compat_deep_free_NC_info (
	apiCB*			inContext,
	infoNC**		data) {
	
	cc_uint32 i;
	
	for (i = 0; data [i] != NULL; i++) {
		cc_free_principal (inContext, &data [i] -> principal);
		cc_free_name (inContext, &data [i] -> name);
		delete (data [i]);
	}
	
	delete [] data;
}

cc_bool
cci_compat_credentials_equal (
	const cred_union*				inOldCreds,
	const cc_credentials_union*		inNewCreds) {

	cc_credentials_v4_compat*		oldCredsV4;
	cc_credentials_v4_t*			newCredsV4;
	cc_credentials_v5_compat*		oldCredsV5;
	cc_credentials_v5_t*			newCredsV5;
	
	CCIAssert_ ((inOldCreds -> cred_type == CC_CRED_V4) || (inOldCreds -> cred_type == CC_CRED_V5));
	CCIAssert_ ((inNewCreds -> version == cc_credentials_v4) || (inNewCreds -> version == cc_credentials_v5));
	
	if (((inOldCreds -> cred_type == CC_CRED_V4) && (inNewCreds -> version != cc_credentials_v4)) ||
	    ((inOldCreds -> cred_type == CC_CRED_V5) && (inNewCreds -> version != cc_credentials_v5))) {
	    return false;
	}
	
	// if one of the credentials is NULL, then match is false		
	if (inOldCreds -> cred_type == CC_CRED_V4) {
		oldCredsV4 = inOldCreds -> cred.pV4Cred;
		newCredsV4 = inNewCreds -> credentials.credentials_v4;
		
		if ((oldCredsV4 == NULL) || (newCredsV4 == NULL))
			return cc_false;
		
		if ((strcmp (oldCredsV4 -> principal, newCredsV4 -> principal) == 0) &&
		    (strcmp (oldCredsV4 -> principal_instance, newCredsV4 -> principal_instance) == 0) &&
		    (strcmp (oldCredsV4 -> realm, newCredsV4 -> realm) == 0) &&
		    (static_cast <CCIUInt32> (oldCredsV4 -> issue_date) == newCredsV4 -> issue_date))
			return cc_true;
		else
			return cc_false;
	} else if (inOldCreds -> cred_type == CC_CRED_V5) {
		// for v5 creds, we are doing SRVNAME_ONLY | EXACT_TIMES comparison
		
		oldCredsV5 = inOldCreds -> cred.pV5Cred;
		newCredsV5 = inNewCreds -> credentials.credentials_v5;
		
		if ((oldCredsV5 == NULL) || (oldCredsV5 == NULL))
			return cc_false;

		// I am not sure this is right! The correct thing to do is is krb5_parse and krb5_compare
		if ((strcmp (oldCredsV5 -> client, newCredsV5 -> client) == 0) &&
		    (strcmp (oldCredsV5 -> server, oldCredsV5 -> server) == 0) &&
		    (oldCredsV5 -> starttime == newCredsV5 -> starttime))
			return cc_true;
		else
			return cc_false;
	}
	
	return cc_false;
}


#endif // CCache_v2_compat
