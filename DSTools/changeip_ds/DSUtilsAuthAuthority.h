/*
 *  DSUtilsAuthAuthority.h
 *  NeST
 *
 *  Created by admin on Mon Oct 06 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef __DSUtilsAuthAuthority__
#define __DSUtilsAuthAuthority__

#include "DSUtils.h"

class DSUtilsAuthAuthority : public DSUtils
{
	public:
	
											DSUtilsAuthAuthority(
												const char *inUsername,
												const char *inServerAddress,
												const char *inPassword,
												const char *inUserID,
												bool inVerifyOnly,
												bool inSetToBasic );
											
		virtual								~DSUtilsAuthAuthority();
		
		virtual tDirStatus					DoActionOnCurrentNode( void );
		virtual void						HandleAuthAuthorityForRecord( tRecordReference inRecordRef );
		virtual void						CopyUserID( char *outUserID );
		
		virtual void						SetVerifyOnly( bool inVerifyOnly ) { mVerifyOnly = inVerifyOnly; };
		virtual void						SetAuthTypeToKerberos( const char *inKerberosDomain );
		virtual void						SetUserID( const char *inUserID );
		virtual void						SetPassword( const char *inPassword );
		
	protected:
		
		bool mVerifyOnly;
		bool mSetToBasic;
		bool mSetKerberosAA;
		char *mUserID;
		char mUsername[256];
		char mServerAddress[256];
		char mPassword[512];
		char mAuthType[128];
		char mKerberosDomain[256];
};
#endif


