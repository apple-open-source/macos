/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


// SampleGroup.h
//
//	a class interface to the CSSM_SAMPLEGROUP structure
//
//	Here are the relevant structures:
//
//	typedef struct cssm_samplegroup {
//	    uint32 NumberOfSamples;
//	    const CSSM_SAMPLE *Samples;
//	} CSSM_SAMPLEGROUP, *CSSM_SAMPLEGROUP_PTR;
//	
//		typedef struct cssm_sample {
//		    CSSM_LIST TypedSample;
//		    const CSSM_SUBSERVICE_UID *Verifier;
//		} CSSM_SAMPLE, *CSSM_SAMPLE_PTR;
//		
//			typedef struct cssm_list {
//			    CSSM_LIST_TYPE ListType;	/* type of this list */
//			    CSSM_LIST_ELEMENT_PTR Head;	/* head of the list */
//			    CSSM_LIST_ELEMENT_PTR Tail;	/* tail of the list */
//			} CSSM_LIST, *CSSM_LIST_PTR;
//			
//			typedef uint32 CSSM_LIST_TYPE, *CSSM_LIST_TYPE_PTR;
//			enum {
//				CSSM_LIST_TYPE_UNKNOWN =			0,
//				CSSM_LIST_TYPE_CUSTOM =				1,
//				CSSM_LIST_TYPE_SEXPR =				2
//			};
//			
//			typedef struct cssm_list_element {
//			    struct cssm_list_element *NextElement;	/* next list element */
//				CSSM_WORDID_TYPE WordID;	/* integer identifier associated */
//											/* with a Word value */
//			    CSSM_LIST_ELEMENT_TYPE ElementType;
//			    union {
//			        CSSM_LIST Sublist;		/* sublist */
//			        CSSM_DATA Word;		/* a byte-string */
//			    } Element;
//			} CSSM_LIST_ELEMENT;
//


#ifndef	__SAMPLEGROUP__
#define __SAMPLEGROUP__

#include <cdsa/cssmtype.h>
#include <cdsa_utilities/utilities.h>
#include <cdsa_utilities/cssm_adt_utils.h>

#ifdef _CPP_UTILITIES
#pragma export on
#endif


class	CssmSample : public PodWrapper<CssmSample, CSSM_SAMPLE> {
public:
		CssmSample();
		CssmSample( CSSM_LIST &list, CSSM_SUBSERVICE_UID *verifier);
		~CssmSample();
		
		CssmSample* operator = (CssmSample& sample);
		
		void	SetSubserviceUID( CSSM_SUBSERVICE_UID *verifier ) { Verifier = verifier; }
		void	SetList( CSSM_LIST *list ) { TypedSample.ListType = list->ListType; TypedSample.Head = list->Head; TypedSample.Tail = list->Tail; }
		
//		CSSM_SAMPLE_TYPE_PASSWORD =				CSSM_WORDID_PASSWORD,
		CSSM_RETURN		AddPasswordImmediate( char* password, CSSM_SUBSERVICE_UID *optionalVerifier );	// provide password without callback or reply to callback
		CSSM_RETURN		AddPasswordCallback( );				// triggers a callback that will acquire the password
		
//		CSSM_SAMPLE_TYPE_HASHED_PASSWORD =		CSSM_WORDID_HASHED_PASSWORD,
		CSSM_RETURN		AddHashedPassword( char* password, CSSM_SUBSERVICE_UID *optionalVerifier );	// this is always in reply to a callback

//		CSSM_SAMPLE_TYPE_PROTECTED_PASSWORD =	CSSM_WORDID_PROTECTED_PASSWORD,
		CSSM_RETURN		AddProtectedPasword(CSSM_SUBSERVICE_UID *optionalVerifier);	// this always provokes a callback, Verifier is optional
		
//		CSSM_SAMPLE_TYPE_PROMPTED_PASSWORD =	CSSM_WORDID_PROMPTED_PASSWORD,
		CSSM_RETURN		AddPromptedPassword( char* promptedPassword, CSSM_SUBSERVICE_UID *optionalVerifier );	// this is always in reply to a callback
		
//		CSSM_SAMPLE_TYPE_SIGNED_NONCE =			CSSM_WORDID_SIGNED_NONCE,
		CSSM_RETURN		AddSignedNonceForCallback( CSSM_SUBSERVICE_UID *requiredVerifier );
		CSSM_RETURN		AddSignedNonceReply( CSSM_DATA_PTR signedNonce, CSSM_SUBSERVICE_UID *requiredVerifier ); // used to reply to the callback for a signed nonce
		
//		CSSM_SAMPLE_TYPE_SIGNED_SECRET =		CSSM_WORDID_SIGNED_SECRET,
		CSSM_RETURN		AddSignedSecretForCallback( CSSM_SUBSERVICE_UID *requiredVerifier ); // will provoke a callback to fill in the actual signed secret
		CSSM_RETURN		AddSignedSecretImmediate( CSSM_DATA_PTR signedSecret, CSSM_SUBSERVICE_UID *requiredVerifier ); // use as the original request or as a response to a callback   
		
//		CSSM_SAMPLE_TYPE_BIOMETRIC =			CSSM_WORDID_BIOMETRIC,
		CSSM_RETURN		AddBiometricCallback( CSSM_SUBSERVICE_UID *requiredVerifier );
		CSSM_RETURN		AddBiometricImmediate( CSSM_DATA_PTR biometricData, CSSM_SUBSERVICE_UID *requiredVerifier );	// reply to callback or provide sample without callback
		
//		CSSM_SAMPLE_TYPE_PROTECTED_BIOMETRIC =	CSSM_WORDID_PROTECTED_BIOMETRIC,
		CSSM_RETURN		AddProtectedBiometric( CSSM_SUBSERVICE_UID *requiredVerifier );		// request for a callback for biometric data
			
//		CSSM_SAMPLE_TYPE_PROMPTED_BIOMETRIC =	CSSM_WORDID_PROMPTED_BIOMETRIC,
		CSSM_RETURN		AddPromptedBiometric( CSSM_DATA_PTR biometricData, CSSM_SUBSERVICE_UID *requiredVerifier );	// reply to callback only
		
//		CSSM_SAMPLE_TYPE_THRESHOLD =			CSSM_WORDID_THRESHOLD
 		CSSM_RETURN		AddThreshold();
		
};

class	CssmSampleGroup : public PodWrapper<CssmSampleGroup, CSSM_SAMPLEGROUP>
{
public:
			CssmSampleGroup();
			CssmSampleGroup( uint32 sampleCount, CSSM_SAMPLE *samples );
			~CssmSampleGroup();
			
	CSSM_RETURN		AddSample(CSSM_SAMPLE*	sample);
	
	CSSM_SAMPLE*	GetIthSample(uint32 sampleIndex);

	uint32			GetSampleCount() { return NumberOfSamples; }
	
	
private:
};

#ifdef _CPP_UTILITIES
#pragma export off
#endif

#endif	// __SAMPLEGROUP__
