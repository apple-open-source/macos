/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */
/*
 * SecNssCoder.h: simple C++ wrapper for PLArenaPool and the
 * high-level ANS1 encode/decode routines.
 */
#ifndef	_SEC_NSS_CODER_H_
#define _SEC_NSS_CODER_H_

#include <SecurityNssAsn1/plarenas.h>
#include <SecurityNssAsn1/prerror.h>
#include <SecurityNssAsn1/secasn1t.h>
#include <SecurityNssAsn1/seccomon.h>
#include <Security/cssmalloc.h>
#include <Security/cssmdata.h>
#include <Security/utilities.h>

/* 
 * Default chunk size for new arena pool.
 * FIXME: analyze & measure different defaults here. I'm pretty sure
 * that only performance - not correct behavior - is affected by
 * an arena pool's chunk size.
 */
#define SNC_CHUNKSIZE_DEF		1024		

class SecNssCoder
{
public:
	SecNssCoder(
		PRUint32 chunkSize = SNC_CHUNKSIZE_DEF);
	~SecNssCoder();
	
	/*
	 * BER decode an untyped item per the specified
	 * template array. The result is allocated 
	 * by this object's PLArenaPool and is freed when 
	 * this object is deleted.
	 *
	 * The dest pointer is a template-specific struct allocated
	 * by the caller and must be zeroed by the caller. 
	 *
	 * This does not throw any exceptions; error status 
	 * (obtained from PR_GetError() is returned. 
	 */
	PRErrorCode	decode(
		const void				*src,		// BER-encoded source
		unsigned long			len,
		const SEC_ASN1Template 	*templ,	
		void					*dest);
		
	/* convenience routine, decode from an SECItem */
	PRErrorCode	decodeItem(
		const SECItem			&item,		// BER-encoded source
		const SEC_ASN1Template 	*templ,	
		void					*dest)
		{
			return decode(item.Data, item.Length, templ, dest);
		}
		
	
	/*
	 * BER-encode. This object's arena pool retains a copy of 
	 * the encoded data.
	 *
	 * The src pointer is a template-specific struct.
	 * 
	 * This does not throw any exceptions; error status 
	 * (obtained from PR_GetError() is returned. 
	 */
	PRErrorCode encodeItem(
		const void				*src,
		const SEC_ASN1Template 	*templ,	
		SECItem					&dest);
		
	/*
	 * Some alloc-related methods which come in handy when using
	 * this class. All memory is allocated using this object's 
	 * arena pool. Caller never has to free it. Used for
	 * temp allocs of memory which only needs a scope which is the
	 * same as this object. 
	 *
	 * These throw a CssmError in the highly unlikely event of 
	 * a malloc failure.
	 */
	void *malloc(
		size_t					len);
		
	/* allocate space for num copies of specified type */
	template <class T> T *mallocn(unsigned num = 1) 
		{ return reinterpret_cast<T *>(malloc(sizeof(T) * num)); }

	/* malloc item.Data, set item.Length */
	void allocItem(
		SECItem					&item,
		size_t					len);
		
	/* malloc and copy, various forms */
	void allocCopyItem(
		const void				*src,
		size_t					len,
		SECItem					&dest);
		
	void allocCopyItem(
		const SECItem			&src,
		SECItem					&dest)
			{ allocCopyItem(src.Data, src.Length, dest); }
			
	void allocCopyItem(
		const CssmData			&src,
		SECItem					&dest)
			{ allocCopyItem(src.data(), src.length(), dest); }
		
	PLArenaPool	*pool() const { return mPool;}
	
private:
	PLArenaPool		*mPool;
};

/*
 * Stateless function to BER-encode directly into a CssmAllocator's
 * space. The only persistent allocated memory is allocated by 
 * the CssmAllocator.
 *
 * The src pointer is a template-specific struct.
 * 
 * This does not throw any exceptions; error status 
 * (obtained from PR_GetError() is returned. 
 */
PRErrorCode SecNssEncodeItem(
	const void				*src,
	const SEC_ASN1Template 	*templ,	
	CssmAllocator			&alloc,
	SECItem					&dest);

/*
 * Same thing, using a CssmOwnedData.
 */
PRErrorCode SecNssEncodeItemOdata(
	const void				*src,
	const SEC_ASN1Template 	*templ,	
	CssmOwnedData			&odata);

#endif	/* _SEC_NSS_CODER_H_ */
