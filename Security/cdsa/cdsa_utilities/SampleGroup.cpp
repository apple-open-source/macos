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


//	SampleGroup.cpp
//
//		CSSM_SAMPLE POD routines

#ifdef __MWERKS__
#define _CPP_UTILITIES
#endif

#include <cdsa_utilities/cssmlist.h>
#include <cdsa_utilities/SampleGroup.h>
#include <stdio.h>
#include <stdlib.h>

	CssmSample::CssmSample()
	{
		Verifier = NULL;
		TypedSample.ListType = CSSM_LIST_TYPE_UNKNOWN;
		TypedSample.Head = NULL;
		TypedSample.Tail = NULL;
	}
	
	CssmSample::CssmSample( CSSM_LIST &list, CSSM_SUBSERVICE_UID *verifier)
	{
		Verifier = verifier;
		TypedSample.ListType = list.ListType;
		TypedSample.Head = list.Head;
		TypedSample.Tail = list.Tail;
	}
	
	CssmSample::~CssmSample()
	{
	}


CssmSample* CssmSample::operator = (CssmSample& sample)
{
	if( this == &sample )
		return NULL;

	this->Verifier = sample.Verifier;
	
	this->TypedSample = sample.TypedSample;

	return this;
}
	
CSSM_RETURN	CssmSample::AddPasswordImmediate( char* password, CSSM_SUBSERVICE_UID *optionalVerifier )
{
 CSSM_RETURN result = CSSM_OK;
 
 CSSM_LIST_ELEMENT*	passwordTypeElement = MakeWordIDElement( CSSM_SAMPLE_TYPE_PASSWORD );	// declares the type to be password
  (CssmList::overlay(TypedSample)).append( ListElement::overlay(passwordTypeElement) );
 
 char* permanentPasswordData = (char*)malloc( strlen(password) + 1 ); // need error handling. Going to assume these succeed for now
 strcpy( permanentPasswordData, password );
 CSSM_LIST_ELEMENT*	passwordElement = MakeDatumElement( (void*)permanentPasswordData, strlen(password) );		// has the password CSSM_DATA in it
  (CssmList::overlay(TypedSample)).append( ListElement::overlay(passwordElement) );
  
 Verifier = optionalVerifier;
 
 return result;
}

	
CSSM_RETURN	CssmSample::AddPasswordCallback( )
{
CSSM_RETURN result = CSSM_OK;
 
 CSSM_LIST_ELEMENT*	passwordCallbackElement = MakeWordIDElement( CSSM_SAMPLE_TYPE_PASSWORD );	// declares the type to be password
 (CssmList::overlay(TypedSample)).append( ListElement::overlay(passwordCallbackElement) );
 
 Verifier = NULL;
 
 return result;
	
}

CSSM_RETURN CssmSample::AddHashedPassword( char* password, CSSM_SUBSERVICE_UID *optionalVerifier )
{
 CSSM_RETURN result = CSSM_OK;
 
 CSSM_LIST_ELEMENT*	passwordTypeElement = MakeWordIDElement( CSSM_SAMPLE_TYPE_HASHED_PASSWORD );	// declares the type to be password
 (CssmList::overlay(TypedSample)).append( ListElement::overlay(passwordTypeElement) );
 
 char* permanentPasswordData = (char*)malloc( strlen(password) + 1 ); // need error handling. Going to assume these succeed for now
 strcpy( permanentPasswordData, password );
 CSSM_LIST_ELEMENT*	passwordElement = MakeDatumElement( (void*)permanentPasswordData, strlen(password) );		// has the password CSSM_DATA in it
 (CssmList::overlay(TypedSample)).append( ListElement::overlay(passwordElement) );
 
 Verifier = optionalVerifier;
 
 return result;

}



CSSM_RETURN CssmSample::AddProtectedPasword(CSSM_SUBSERVICE_UID *optionalVerifier)
{
CSSM_RETURN	result = CSSM_OK;

 CSSM_LIST_ELEMENT*	passwordCallbackElement = MakeWordIDElement( CSSM_SAMPLE_TYPE_PROTECTED_PASSWORD );	// declares the type to be password
 (CssmList::overlay(TypedSample)).append( ListElement::overlay(passwordCallbackElement) );
 
 Verifier = optionalVerifier;
		
return result;
}


CSSM_RETURN		CssmSample::AddPromptedPassword( char* promptedPassword, CSSM_SUBSERVICE_UID *optionalVerifier )	
{
 CSSM_RETURN result = CSSM_OK;
 
 CSSM_LIST_ELEMENT*	passwordTypeElement = MakeWordIDElement( CSSM_SAMPLE_TYPE_PROMPTED_PASSWORD );	// declares the type to be password
 (CssmList::overlay(TypedSample)).append( ListElement::overlay(passwordTypeElement) );

 char* permanentPasswordData = (char*)malloc( strlen(promptedPassword) + 1 ); // need error handling. Going to assume these succeed for now
 strcpy( permanentPasswordData, promptedPassword );
 CSSM_LIST_ELEMENT*	passwordPromptElement = MakeDatumElement( (void*)permanentPasswordData, strlen(promptedPassword) );		// has the password CSSM_DATA in it 
 (CssmList::overlay(TypedSample)).append( ListElement::overlay(passwordPromptElement) );
 
 Verifier = optionalVerifier;
 
 return result;
}


CSSM_RETURN		CssmSample::AddSignedNonceForCallback( CSSM_SUBSERVICE_UID *requiredVerifier )
{
CSSM_RETURN	result = CSSM_OK;

 CSSM_LIST_ELEMENT*	signedNonceCallbackElement = MakeWordIDElement( CSSM_SAMPLE_TYPE_SIGNED_NONCE );	
 (CssmList::overlay(TypedSample)).append( ListElement::overlay(signedNonceCallbackElement) );
 
 Verifier = requiredVerifier;
		
return result;
}

CSSM_RETURN		CssmSample::AddSignedNonceReply( CSSM_DATA_PTR signedNonce, CSSM_SUBSERVICE_UID *requiredVerifier )
{
CSSM_RETURN	result = CSSM_OK;

 CSSM_LIST_ELEMENT*	signedNonceTypeElement = MakeWordIDElement( CSSM_SAMPLE_TYPE_SIGNED_NONCE );
 (CssmList::overlay(TypedSample)).append( ListElement::overlay(signedNonceTypeElement) );

 CSSM_LIST_ELEMENT* 	signedNonceDataElement = MakeDatumElement( (void*)signedNonce->Data, signedNonce->Length ); 
 (CssmList::overlay(TypedSample)).append( ListElement::overlay(signedNonceDataElement) );
 
 Verifier = requiredVerifier;
		
return result;
}

CSSM_RETURN		CssmSample::AddSignedSecretForCallback( CSSM_SUBSERVICE_UID *requiredVerifier )
{
CSSM_RETURN	result = CSSM_OK;
	CSSM_LIST_ELEMENT*	signedSecretCallbackElement = MakeWordIDElement( CSSM_SAMPLE_TYPE_SIGNED_SECRET );
	(CssmList::overlay(TypedSample)).append( ListElement::overlay(signedSecretCallbackElement) );
	
	Verifier = requiredVerifier;	
	
return result;
}

CSSM_RETURN		CssmSample::AddSignedSecretImmediate( CSSM_DATA_PTR signedSecret, CSSM_SUBSERVICE_UID *requiredVerifier )
{
CSSM_RETURN	result = CSSM_OK;

	CSSM_LIST_ELEMENT*	signedSecretTypeElement = MakeWordIDElement( CSSM_SAMPLE_TYPE_SIGNED_SECRET );
	(CssmList::overlay(TypedSample)).append( ListElement::overlay(signedSecretTypeElement) );

	CSSM_LIST_ELEMENT*	signedSecretElement = MakeDatumElement( (void*)signedSecret->Data, signedSecret->Length );	
	(CssmList::overlay(TypedSample)).append( ListElement::overlay(signedSecretElement) );
 
	Verifier = requiredVerifier;	
return result;

}

CSSM_RETURN		CssmSample::AddBiometricCallback( CSSM_SUBSERVICE_UID *requiredVerifier )
{
CSSM_RETURN	result = CSSM_OK;

	CSSM_LIST_ELEMENT*	callbackElement = MakeWordIDElement( CSSM_SAMPLE_TYPE_BIOMETRIC );
	(CssmList::overlay(TypedSample)).append( ListElement::overlay(callbackElement) );
	
	Verifier = requiredVerifier;	
	
return result;
}


CSSM_RETURN CssmSample::AddBiometricImmediate( CSSM_DATA_PTR biometricData, CSSM_SUBSERVICE_UID *requiredVerifier )
{
CSSM_RETURN	result = CSSM_OK;

	CSSM_LIST_ELEMENT*	typeElement = MakeWordIDElement( CSSM_SAMPLE_TYPE_BIOMETRIC );
	(CssmList::overlay(TypedSample)).append( ListElement::overlay(typeElement) );

	CSSM_LIST_ELEMENT*	dataElement = MakeDatumElement( (void*)biometricData->Data, biometricData->Length );	
	(CssmList::overlay(TypedSample)).append( ListElement::overlay(dataElement) );
 
	Verifier = requiredVerifier;	
	
return result;
}


CSSM_RETURN CssmSample::AddProtectedBiometric( CSSM_SUBSERVICE_UID *requiredVerifier )
{
CSSM_RETURN	result = CSSM_OK;

	CSSM_LIST_ELEMENT*	callbackElement = MakeWordIDElement( CSSM_SAMPLE_TYPE_PROTECTED_BIOMETRIC );
	(CssmList::overlay(TypedSample)).append( ListElement::overlay(callbackElement) );
	
	Verifier = requiredVerifier;	
return result;
}

CSSM_RETURN CssmSample::AddPromptedBiometric( CSSM_DATA_PTR biometricData, CSSM_SUBSERVICE_UID *requiredVerifier )
{
CSSM_RETURN	result = CSSM_OK;

	CSSM_LIST_ELEMENT*	callbackElement = MakeWordIDElement( CSSM_SAMPLE_TYPE_PROMPTED_BIOMETRIC );
	(CssmList::overlay(TypedSample)).append( ListElement::overlay(callbackElement) );


	CSSM_LIST_ELEMENT*	dataElement = MakeDatumElement( (void*)biometricData->Data, biometricData->Length );	
	(CssmList::overlay(TypedSample)).append( ListElement::overlay(dataElement) );
 	
	Verifier = requiredVerifier;	
return result;
}

// CssmSampleGroup

CssmSampleGroup::CssmSampleGroup()
{	// creates the nothing sample group
	NumberOfSamples = 0;
	Samples = NULL;
}

CSSM_RETURN	CssmSampleGroup::AddSample(CSSM_SAMPLE*	sample)
{
CSSM_RETURN		result = CSSM_OK;
CSSM_SAMPLE*	sampleBase;
	if( NumberOfSamples == 0 )
	{ // malloc to create the first item
		sampleBase = (CSSM_SAMPLE*)malloc( sizeof(CSSM_SAMPLE) );
		Samples = sampleBase;
	}
		else
		{ // realloc to add the next item
			sampleBase = (CSSM_SAMPLE*)realloc( (void*)Samples, sizeof(CSSM_SAMPLE) * (NumberOfSamples + 1) );
			Samples = sampleBase;
		}
		
	sampleBase[NumberOfSamples].TypedSample.ListType	= sample->TypedSample.ListType;
	sampleBase[NumberOfSamples].TypedSample.Head		= sample->TypedSample.Head;
	sampleBase[NumberOfSamples].TypedSample.Tail		= sample->TypedSample.Tail;
	sampleBase[NumberOfSamples].Verifier 				= sample->Verifier;
	
	NumberOfSamples++;
	
return result;
}

CSSM_SAMPLE*	CssmSampleGroup::GetIthSample(uint32 sampleIndex)
{
	if( (0 != NumberOfSamples) && (sampleIndex < NumberOfSamples-1) )
		return (CSSM_SAMPLE*)&Samples[sampleIndex];
	  else
	  	return NULL;
}
