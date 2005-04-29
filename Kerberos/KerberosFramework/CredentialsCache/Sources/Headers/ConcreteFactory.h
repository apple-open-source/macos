#pragma once

#include "AbstractFactory.h"

template <class Context, class CCache, class Credentials, class CompatCredentials>
class CCIConcreteFactory:
	public CCIAbstractFactory {
	public:
		CCIConcreteFactory			()
		{
		}

		virtual	CCIContext*				CreateContext (
											CCIInt32			inAPIVersion)
		{
			return new Context (inAPIVersion);
		}
		
		virtual	CCIContext*				CreateContext (
											CCIUniqueID			inContextID,
											CCIInt32			inAPIVersion)
		{
			return new Context (inContextID, inAPIVersion);
		}
		
		virtual CCICCache*				CreateCCache (
											CCIUniqueID			inCCacheID,
											CCIInt32			inAPIVersion)
		{
			return new CCache (inCCacheID, inAPIVersion);
		}
		
		virtual CCICredentials*			CreateCredentials (
											CCIUniqueID			inCredentialsID,
											CCIInt32			inAPIVersion,
											bool				inInitialize)
		{
			return new Credentials (inCredentialsID, inAPIVersion);
		}

		virtual CCICompatCredentials*	CreateCompatCredentials (
											CCIUniqueID			inCredentialsID,
											CCIInt32			inAPIVersion)
		{
			return new CompatCredentials (inCredentialsID, inAPIVersion);
		}
};
