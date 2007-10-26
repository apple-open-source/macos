/*
 *  DSUtils.h
 *  NeST
 *
 *  Created by admin on Fri Apr 04 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef __DSUtils_H__
#define __DSUtils_H__

#include <Carbon/Carbon.h>
#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesConst.h>
#include <DirectoryService/DirServicesTypes.h>
#include <DirectoryService/DirServicesUtils.h>

class DSUtils
{
	public:
	
											DSUtils();
		virtual								~DSUtils();
	
		virtual void						CloseCurrentNodeRef( void );
		virtual tDirStatus					OpenDirectoryServices( void );
		virtual tDirStatus					OpenSpecificPasswordServerNode( const char *inServerAddress );
		virtual tDirStatus					OpenLocalLDAPNode( const char *inUser, const char *inPassword );
		virtual tDirStatus					OpenNodeByName( const char *inNodeName, const char *inUser, const char *inPassword );
		virtual tDirStatus					OpenNetInfoParentNode( void );
		virtual tDirStatus					GetLocallyHostedNodeList( void );
		virtual tDirStatus					OpenLocallyHostedNode( unsigned long inNodeIndex );
		
		virtual tDirStatus					OpenRecord( const char *inRecordType,
														const char *inRecordName,
														tRecordReference *outRecordRef,
														bool inCreate = false );
		
		virtual tDirStatus					DoActionOnCurrentNode( void );
		virtual tDirStatus					DoActionForAllLocalNodes( void );
		
		virtual tDirStatus					FillAuthBuff( tDataBuffer *inAuthBuff,
														  unsigned long inCount,
														  unsigned long inLen,
														  ... );
		virtual tDirStatus					GetServerAddressForUser( const char *uname, char *serverAddress, char **userNodeName );
		
		tDirReference						GetDSRef( void ) { return mDSRef; };
		tDirNodeReference					GetCurrentNodeRef( void ) { return mNodeRef; };
		unsigned long 						GetLocallyHostedNodeCount( void ) { return mNodeCount; };

	protected:
	
		tDirReference mDSRef;
		tDirNodeReference mNodeRef;
		tDataBuffer *mNodeListBuff;
		unsigned long mNodeCount;
		bool mCurrentNodeAuthenticated;
		bool mCurrentNodeIsLDAP;
};

#endif
