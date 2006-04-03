#pragma once

#include "Context.h"
#include "CCache.h"
#include "Credentials.h"

//
// This factory creates various objects used in the API, and makes sure to use 

class CCIAbstractFactory {
	public:
				CCIAbstractFactory ()
				{
				}

				virtual ~CCIAbstractFactory ()
				{
				}

				virtual	CCIContext*				CreateContext (
					CCIInt32						inAPIVersion) = 0;
				
				virtual	CCIContext*				CreateContext (
					CCIUniqueID						inContextID,
					CCIInt32						inAPIVersion) = 0;
				
				virtual CCICCache*				CreateCCache (
					CCIUniqueID						inCCacheID,
					CCIInt32						inAPIVersion) = 0;
				
				virtual CCICredentials*			CreateCredentials (
					CCIUniqueID						inCredentialsID,
					CCIInt32						inAPIVersion,
					bool							inInitialize = true) = 0;

                static CCIAbstractFactory*		GetTheFactory ();
				
	protected:
		static	CCIAbstractFactory*			sTheFactory;

                static CCIAbstractFactory*		MakeFactory ();
};
