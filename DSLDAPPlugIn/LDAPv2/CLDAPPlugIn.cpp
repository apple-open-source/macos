/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header CLDAPPlugIn
 * LDAP plugin implementation to interface with Directory Services.
 */

#include <stdio.h>
#include <string.h>		//used for strcpy, etc.
#include <stdlib.h>		//used for malloc
#include <ctype.h>		//use for isprint

#include <Security/Authorization.h>

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>

#include "CLDAPPlugIn.h"
#include <DirectoryServiceCore/ServerModuleLib.h>
#include <DirectoryServiceCore/CRCCalc.h>
#include <DirectoryServiceCore/CPlugInRef.h>
#include <DirectoryServiceCore/CContinue.h>
#include <DirectoryServiceCore/DSCThread.h>
#include <DirectoryServiceCore/DSEventSemaphore.h>
#include <DirectoryServiceCore/DSMutexSemaphore.h>
#include <DirectoryServiceCore/CSharedData.h>
#include <DirectoryServiceCore/DSUtils.h>


// --------------------------------------------------------------------------------
//	Globals

CContinue				   *gContinueTable		= nil; //KW have yet to use this
CPlugInRef				   *gRefTable			= nil;
CPlugInRef				   *gConfigTable		= nil;
uInt32						gConfigTableLen		= 0;
static DSEventSemaphore	   *gKickSearchRequests	= nil;
static DSMutexSemaphore    *gLDAPOpenMutex		= nil;

// Consts ----------------------------------------------------------------------------

static const	uInt32	kBuffPad	= 16;

extern "C" {
CFUUIDRef ModuleFactoryUUID = CFUUIDGetConstantUUIDWithBytes ( NULL, \
								0xD9, 0x70, 0xD5, 0x2E, 0xD5, 0x15, 0x11, 0xD3, \
								0x9F, 0xF9, 0x00, 0x05, 0x02, 0xC1, 0xC7, 0x36 );

}

static CDSServerModule* _Creator ( void )
{
	return( new CLDAPPlugIn );
}

CDSServerModule::tCreator CDSServerModule::sCreator = _Creator;

// --------------------------------------------------------------------------------
//	* CLDAPPlugIn ()
// --------------------------------------------------------------------------------

CLDAPPlugIn::CLDAPPlugIn ( void )
{

	fState		= kUnknownState;

	if ( gLDAPOpenMutex == nil )
	{
		gLDAPOpenMutex = new DSMutexSemaphore();
	}

    //KW need to pass in a DeleteContextData method instead of nil
    if ( gContinueTable == nil )
    {
        gContinueTable = new CContinue( nil );
    }

    //KW need to pass in a DeleteContextData method instead of nil
    if ( gConfigTable == nil )
    {
        gConfigTable = new CPlugInRef( nil );
		if ( gConfigTable == nil ) throw( (sInt32)eMemoryAllocError );
    }
    
    if ( gRefTable == nil )
    {
        gRefTable = new CPlugInRef( CLDAPPlugIn::ContextDeallocProc );
		if ( gRefTable == nil ) throw( (sInt32)eMemoryAllocError );
    }

	if ( gKickSearchRequests == nil )
	{
		gKickSearchRequests = new DSEventSemaphore();
		if ( gKickSearchRequests == nil ) throw( (sInt32)eMemoryAllocError );
	}

    //ensure that the configXML is nil before initialization
    pConfigFromXML = nil;
    
} // CLDAPPlugIn


// --------------------------------------------------------------------------------
//	* ~CLDAPPlugIn ()
// --------------------------------------------------------------------------------

CLDAPPlugIn::~CLDAPPlugIn ( void )
{
	//cleanup the mappings and config data here
	//this will clean up the following:
	// 1) gConfigTable
	// 2) gConfigTableLen
	// 3) pStdAttributeMapTuple
	// 4) pStdRecordMapTuple
    if ( pConfigFromXML != nil)
    {
        delete ( pConfigFromXML );
        pConfigFromXML			= nil;
        gConfigTable			= nil;
        gConfigTableLen			= 0;
        pStdAttributeMapTuple	= nil;
        pStdRecordMapTuple		= nil;
    }

    //KW clean up the gContinueTable here
    // not only the table but any dangling references in the table as well
    //for now simply delete table
    if ( gContinueTable != nil)
    {
        delete ( gContinueTable );
        gContinueTable = nil;
    }
    
	if ( gLDAPOpenMutex != nil )
	{
		delete(gLDAPOpenMutex);
		gLDAPOpenMutex = nil;
	}

    //KW ensure the release of all LDAP session handles eventually
    //but probably NOT through CleanContextData since multiple contexts will
    //have the same session handle
    
} // ~CLDAPPlugIn


// --------------------------------------------------------------------------------
//	* Validate ()
// --------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::Validate ( const char *inVersionStr, const uInt32 inSignature )
{
	fSignature = inSignature;

	return( noErr );

} // Validate


// --------------------------------------------------------------------------------
//	* Initialize ()
// --------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::Initialize ( void )
{
    int					countNodes	= 0;
    sInt32				siResult	= eDSNoErr;
    tDataList		   *pldapName	= nil;
    sLDAPConfigData	   *pConfig		= nil;
    uInt32				iTableIndex	= 0;
   
    try
	{
	    if ( pConfigFromXML == nil )
	    {
	        pConfigFromXML = new CLDAPConfigs();
        	if ( pConfigFromXML  == nil ) throw( (sInt32)eDSOpenNodeFailed ); //KW need an eDSPlugInConfigFileError
	    }
	    siResult = pConfigFromXML->Init( gConfigTable, gConfigTableLen, &pStdAttributeMapTuple, &pStdRecordMapTuple );
	    if ( siResult != eDSNoErr ) throw( siResult );

	    //Cycle through the gConfigTable
	    //skip the first "generic unknown" configuration ie. nothing to register so start at 1 not 0
	    for (iTableIndex=1; iTableIndex<gConfigTableLen; iTableIndex++)
	    {
	        pConfig = (sLDAPConfigData *)gConfigTable->GetItemData( iTableIndex );
	        if (pConfig != nil)
	        {
				if (pConfig->fServerName != nil)
				{
					if (pConfig->bUpdated)
					{
//allow register of nodes that have NOT been verified by ldap_init calls
						{
							countNodes++;
							pConfig->bAvail = true;
							//add standard LDAPv2 prefix to the registered node names here
							pldapName = dsBuildListFromStringsPriv((char *)"LDAPv2", pConfig->fServerName, nil);
							if (pldapName != nil)
							{
								//same node does not get registered twice
								DSRegisterNode( fSignature, pldapName, kDirNodeType );
								dsDataListDeallocatePriv( pldapName);
								free(pldapName);
								pldapName = nil;
							}
						} //if connection to LDAP server possible
					} // Config has been updated OR is new so register the node
					else
					{
						//UN register the node
						//and remove it from the config table
						//but DO NOT decrement the gConfigTableLen counter since we allow empty entries
						
		                //add standard LDAPv2 prefix to the registered node names here
		                pldapName = dsBuildListFromStringsPriv((char *)"LDAPv2", pConfig->fServerName, nil);
		                if (pldapName != nil)
		                {
							//KW what happens when the same node is unregistered twice???
	                        DSUnregisterNode( fSignature, pldapName );
	                    	dsDataListDeallocatePriv( pldapName);
							free(pldapName);
	                    	pldapName = nil;
		                }
						pConfigFromXML->CleanLDAPConfigData( pConfig );
						// delete the sLDAPConfigData itself
						delete( pConfig );
						pConfig = nil;
						// remove the table entry
						gConfigTable->RemoveItem( iTableIndex );
					}
				} //if servername defined
//KW don't throw anything here since we want to go on and get the others
//				if (pConfig->fServerName == nil ) throw( (sInt32)eDSNullParameter);  //KW might want a specific err
	        } // pConfig != nil
	    } // loop over the LDAP config entries
	            
		// set the active and initted flags
		fState = kUnknownState;
		fState += kInitalized;
		fState += kActive;
        
        WakeUpRequests();

	} // try
	catch( sInt32 err )
	{
		siResult = err;
		// set the inactive and failedtoinit flags
		fState = kUnknownState;
		fState += kFailedToInit;
	}

	return( siResult );

} // Initialize


// --------------------------------------------------------------------------------
//	* SetPluginState ()
// --------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::SetPluginState ( const uInt32 inState )
{

    tDataList		   *pldapName	= nil;
    sLDAPConfigData	   *pConfig		= nil;
    uInt32				iTableIndex	= 0;

// don't allow any changes other than active / in-active

	if (kActive & inState) //want to set to active
    {
		//call to Init so that we re-init everything that requires it
		Initialize();
    }

	if (kInactive & inState) //want to set to in-active
    {
	    //Cycle through the gConfigTable
	    //skip the first "generic unknown" configuration ie. nothing to register so start at 1 not 0
	    for (iTableIndex=1; iTableIndex<gConfigTableLen; iTableIndex++)
	    {
	        pConfig = (sLDAPConfigData *)gConfigTable->GetItemData( iTableIndex );
	        if (pConfig != nil)
	        {
				if (pConfig->fServerName != nil)
				{
					{
						//UN register the node
						//but don't remove it from the config table
						
		                //add standard LDAPv2 prefix to the registered node names here
		                pldapName = dsBuildListFromStringsPriv((char *)"LDAPv2", pConfig->fServerName, nil);
		                if (pldapName != nil)
		                {
							//KW what happens when the same node is unregistered twice???
	                        DSUnregisterNode( fSignature, pldapName );
	                    	dsDataListDeallocatePriv( pldapName);
							free(pldapName);
	                    	pldapName = nil;
		                }
					}
				} //if servername defined
//KW don't throw anything here since we want to go on and get the others
//				if (pConfig->fServerName == nil ) throw( (sInt32)eDSNullParameter);  //KW might want a specific err
	        } // pConfig != nil
	    } // loop over the LDAP config entries

        if (!(fState & kInactive))
        {
                fState += kInactive;
        }
        if (fState & kActive)
        {
                fState -= kActive;
        }
    }

	return( eDSNoErr );

} // SetPluginState


//--------------------------------------------------------------------------------------------------
//	* WakeUpRequests() (static)
//
//--------------------------------------------------------------------------------------------------

void CLDAPPlugIn::WakeUpRequests ( void )
{
	gKickSearchRequests->Signal();
} // WakeUpRequests


// ---------------------------------------------------------------------------
//	* WaitForInit
//
// ---------------------------------------------------------------------------

void CLDAPPlugIn::WaitForInit ( void )
{
	volatile	uInt32		uiAttempts	= 0;

	if (!(fState & kActive))
	{
	while ( !(fState & kInitalized) &&
			!(fState & kFailedToInit) )
	{
		try
		{
			// Try for 2 minutes before giving up
			if ( uiAttempts++ >= 240 )
			{
				return;
			}

			// Now wait until we are told that there is work to do or
			//	we wake up on our own and we will look for ourselves

			gKickSearchRequests->Wait( (uInt32)(.5 * kMilliSecsPerSec) );

			try
			{
				gKickSearchRequests->Reset();
			}

			catch( sInt32 err )
			{
			}
		}

		catch( sInt32 err1 )
		{
		}
	}
	}//NOT already Active
} // WaitForInit


// ---------------------------------------------------------------------------
//	* ProcessRequest
//
// ---------------------------------------------------------------------------

sInt32 CLDAPPlugIn::ProcessRequest ( void *inData )
{
	sInt32		siResult	= 0;
	char	   *pathStr		= nil;

	if ( inData == nil )
	{
		return( ePlugInDataError );
	}
    
	if (((sHeader *)inData)->fType == kOpenDirNode)
	{
		if (((sOpenDirNode *)inData)->fInDirNodeName != nil)
		{
			pathStr = ::dsGetPathFromListPriv( ((sOpenDirNode *)inData)->fInDirNodeName, "/" );
			if (pathStr != nil)
			{
				if (strncmp(pathStr,"/LDAPv2",7) != 0)
				{
					free(pathStr);
					pathStr = nil;
					return(eDSOpenNodeFailed);
				}
				free(pathStr);
				pathStr = nil;
			}
		}
	}
	
    WaitForInit();

	if ( (fState & kFailedToInit) )
	{
        return( ePlugInFailedToInitialize );
	}

	if ( ((fState & kInactive) || !(fState & kActive))
		  && (((sHeader *)inData)->fType != kDoPlugInCustomCall)
		  && (((sHeader *)inData)->fType != kOpenDirNode) )
	{
        return( ePlugInNotActive );
	}
    
	if ( ((sHeader *)inData)->fType == kHandleNetworkTransition )
	{
		siResult = Initialize(); //only useful when we have the DHCP retrieved LDAP server
	}
	else
	{
		siResult = HandleRequest( inData );
	}

	return( siResult );

} // ProcessRequest



// ---------------------------------------------------------------------------
//	* HandleRequest
//
// ---------------------------------------------------------------------------

sInt32 CLDAPPlugIn::HandleRequest ( void *inData )
{
	sInt32	siResult	= 0;
	sHeader	*pMsgHdr	= nil;

	if ( inData == nil )
	{
		return( -8088 );
	}

	pMsgHdr = (sHeader *)inData;

	switch ( pMsgHdr->fType )
	{
		case kOpenDirNode:
			siResult = OpenDirNode( (sOpenDirNode *)inData );
			break;
			
		case kCloseDirNode:
			siResult = CloseDirNode( (sCloseDirNode *)inData );
			break;
			
		case kGetDirNodeInfo:
			siResult = GetDirNodeInfo( (sGetDirNodeInfo *)inData );
			break;
			
		case kGetRecordList:
			siResult = GetRecordList( (sGetRecordList *)inData );
			break;
			
		case kGetRecordEntry:
            siResult = GetRecordEntry( (sGetRecordEntry *)inData );
			break;
			
		case kGetAttributeEntry:
			siResult = GetAttributeEntry( (sGetAttributeEntry *)inData );
			break;
			
		case kGetAttributeValue:
			siResult = GetAttributeValue( (sGetAttributeValue *)inData );
			break;
			
		case kOpenRecord:
			siResult = OpenRecord( (sOpenRecord *)inData );
			break;
			
		case kGetRecordReferenceInfo:
			siResult = GetRecRefInfo( (sGetRecRefInfo *)inData );
			break;
			
		case kGetRecordAttributeInfo:
			siResult = GetRecAttribInfo( (sGetRecAttribInfo *)inData );
			break;
			
		case kGetRecordAttributeValueByID:
			siResult = eNotYetImplemented;
			break;
			
		case kGetRecordAttributeValueByIndex:
			siResult = GetRecAttrValueByIndex( (sGetRecordAttributeValueByIndex *)inData );
			break;
			
		case kFlushRecord:
			siResult = eNotYetImplemented;
			break;
			
		case kCloseAttributeList:
			siResult = CloseAttributeList( (sCloseAttributeList *)inData );
			break;

		case kCloseAttributeValueList:
			siResult = CloseAttributeValueList( (sCloseAttributeValueList *)inData );
			break;

		case kCloseRecord:
			siResult = CloseRecord( (sCloseRecord *)inData );
			break;
			
		case kSetRecordName:
			siResult = eNotYetImplemented;
			break;
			
		case kSetRecordType:
			siResult = eNotYetImplemented;
			break;
			
		case kDeleteRecord:
			siResult = eNotYetImplemented;
			break;
			
		case kCreateRecord:
			siResult = eNotYetImplemented;
			break;
			
		case kCreateRecordAndOpen:
			siResult = eNotYetImplemented;
			break;
			
		case kAddAttribute:
			siResult = eNotYetImplemented;
			break;
			
		case kRemoveAttribute:
			siResult = eNotYetImplemented;
			break;
			
		case kAddAttributeValue:
			siResult = eNotYetImplemented;
			break;
			
		case kRemoveAttributeValue:
			siResult = eNotYetImplemented;
			break;
			
		case kSetAttributeValue:
			siResult = eNotYetImplemented;
			break;
			
		case kDoDirNodeAuth:
			siResult = DoAuthentication( (sDoDirNodeAuth *)inData );
			break;
			
		case kDoAttributeValueSearch:
		case kDoAttributeValueSearchWithData:
			siResult = DoAttributeValueSearch( (sDoAttrValueSearchWithData *)inData );
			break;

		case kDoPlugInCustomCall:
			siResult = DoPlugInCustomCall( (sDoPlugInCustomCall *)inData );
			break;
			
		default:
			siResult = eNotHandledByThisNode;
			break;
	}

	pMsgHdr->fResult = siResult;

	return( siResult );

} // HandleRequest

//------------------------------------------------------------------------------------
//	* OpenDirNode
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::OpenDirNode ( sOpenDirNode *inData )
{
    char			   *ldapName		= nil;
	char			   *pathStr			= nil;
    char			   *subStr			= nil;
    int					ldapPort		= LDAP_PORT;
	sLDAPContextData	   *pContext		= nil;
	sInt32				siResult		= eDSNoErr;
	sLDAPConfigData	   *pConfig			= nil;
	uInt32				iTableIndex		= 0;
    tDataListPtr		pNodeList		= nil;
	LDAP			   *ald				= nil;

    pNodeList	=	inData->fInDirNodeName;
    PrintNodeName(pNodeList);
    
	try
	{
            if ( inData != nil )
            {
                pathStr = dsGetPathFromListPriv( pNodeList, (char *)"/" );
                if ( pathStr == nil ) throw( (sInt32)eDSNullNodeName );

				//special case for the configure LDAPv2 node
				if (::strcmp(pathStr,"/LDAPv2") == 0)
				{
					// set up the context data now with the relevant parameters for the configure LDAPv2 node
                	// DS API reference number is used to access the reference table
                	pContext = MakeContextData();
                	pContext->fHost = nil;
                	pContext->fName = new char[1+::strlen("LDAPv2 Configure")];
                	::strcpy(pContext->fName,"LDAPv2 Configure");
                	//generic hash index
                	pContext->fConfigTableIndex = 0;
                	// add the item to the reference table
					gRefTable->AddItem( inData->fOutNodeRef, pContext );
				}
                // check that there is something after the delimiter or prefix
                // strip off the LDAPv2 prefix here
                else if ( (strlen(pathStr) > 8) && (::strncmp(pathStr,"/LDAPv2/",8) == 0) )
                {
					subStr = pathStr + 8;

                    ldapName = new char[1+strlen(subStr)];
                    if ( ldapName == nil ) throw( (sInt32)eDSNullNodeName );
                    
                    ::strcpy(ldapName,subStr);
                    // don't care if this was originally in the config file or not
                    // ie. allow non-configured connections if possible
                    // however, they need to use the standard LDAP PORT if no config entry exists
                    // search now for possible LDAP port entry
			    
                    //Cycle through the gConfigTable to get the LDAP port to use for the ldap_open
                    for (iTableIndex=0; iTableIndex<gConfigTableLen; iTableIndex++)
                    {
                        pConfig = (sLDAPConfigData *)gConfigTable->GetItemData( iTableIndex );
                        if (pConfig != nil)
                        {
                            if (pConfig->fName != nil)
                            {
                                if (::strcmp(pConfig->fServerName,ldapName) == 0)
                                {
                                    ldapPort = pConfig->fServerPort;
                                    //exit the for loop if entry found
                                    break;
                                } // if name found
                            } // if name not nil
                        }// if config entry not nil
                    } // loop over config table entries

					//protect against thread unsafe gethostbyname call within LDAP framework
					gLDAPOpenMutex->Wait();
					ald = ldap_open(ldapName, ldapPort);
					gLDAPOpenMutex->Signal();

					if ( ald != NULL )
                    {
                        // set up the context data now with the relevant parameters
                        // ldap host, ldap port number
                        // DS API reference number is used to access the reference table
                        pContext = MakeContextData();
                        pContext->fHost = ald;
                        pContext->fName = new char[1+::strlen(ldapName)];
                        ::strcpy(pContext->fName,ldapName);
                        pContext->fPort = ldapPort;
                        //KW actual intent is not really to use this type int anyways - pull it out later
                        //KW need to come up with some standard type for the reference collection?
                        //KW for now using "1" as a node and "2" as a record
                        pContext->fType = 1;
                        //set hash index if entry was found in the table above
                        if (iTableIndex < gConfigTableLen)
                        {
                            pContext->fConfigTableIndex = iTableIndex;
                        }
                        else // this is the "generic unknown" case
                        {
                            pContext->fConfigTableIndex = 0;
                        }
                        // add the item to the reference table
                        gRefTable->AddItem( inData->fOutNodeRef, pContext );
                    } // open LDAP session
                    else
                    {
                        siResult = eDSOpenNodeFailed;
                    }
					if ( ldapName != nil )
					{
						delete( ldapName );
                        ldapName = nil;
					}
                } // there was some name passed in here ie. length > 1
                else
                {
                    siResult = eDSOpenNodeFailed;
                }

                delete( pathStr );
                pathStr = nil;
            } // inData != nil
	} // try
	catch( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // OpenDirNode

//------------------------------------------------------------------------------------
//	* CloseDirNode
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::CloseDirNode ( sCloseDirNode *inData )
{
	int				ldapResult	=	LDAP_SUCCESS;
	sInt32			siResult	=	eDSNoErr;
	sLDAPContextData	*pContext	=	nil;

	try
	{
		pContext = (sLDAPContextData *)gRefTable->GetItemData( inData->fInNodeRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );
		// KW need to extract the reference COUNT out of the RefTable somehow
        // ie. an enhanced feature not in this RefTable code yet

		if ( pContext->fHost != nil )
		{
			ldapResult = ldap_unbind(pContext->fHost);
			if (ldapResult != LDAP_SUCCESS)
			{
				siResult = eDSNodeNotFound;
			}
		}
		
		gRefTable->RemoveItem( inData->fInNodeRef );
		// nothing is in the gContinueTable yet
		// but if there was this is our last chance to clean up anything we missed for that node 
		gContinueTable->RemoveItems( inData->fInNodeRef );

	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // CloseDirNode


// ---------------------------------------------------------------------------
//	* MakeContextData
// ---------------------------------------------------------------------------

sLDAPContextData* CLDAPPlugIn::MakeContextData ( void )
{
    sLDAPContextData   *pOut		= nil;
    sInt32			siResult	= eDSNoErr;

    pOut = (sLDAPContextData *) calloc(1, sizeof(sLDAPContextData));
    if ( pOut != nil )
    {
//        ::memset( pOut, 0, sizeof( sLDAPContextData ) );
        //do nothing with return here since we know this is new
        //and we did a memset above
        siResult = CleanContextData(pOut);
    }

    return( pOut );

} // MakeContextData

// ---------------------------------------------------------------------------
//	* CleanContextData
// ---------------------------------------------------------------------------

sInt32 CLDAPPlugIn::CleanContextData ( sLDAPContextData *inContext )
{
    sInt32	siResult = eDSNoErr;
    
    if ( inContext == nil )
    {
        siResult = eDSBadContextData;
	}
    else
    {
        //LDAP specific
        //can't release the LDAP servers here
        //since there are more than one context with the same fHost
        inContext->fHost			= nil;
        if (inContext->fName != nil)
        {
            delete ( inContext->fName );
        }
        inContext->fName			= nil;
        inContext->fPort			= 389;
        inContext->fConfigTableIndex= 0;
        inContext->fType			= 0;	//KW using 1 for Nodes and 2 for Records
        inContext->msgId			= 0;
        inContext->authCallActive	= false;
        if (inContext->authAccountName != nil)
        {
            delete ( inContext->authAccountName );
        }
        inContext->authAccountName			= nil;
        if (inContext->authPassword != nil)
        {
            delete ( inContext->authPassword );
        }
        inContext->authPassword			= nil;

        // remember ldap_msgfree( inContext->pResult ) will remove the LDAPMessage
        if (inContext->pResult != nil)
        {
        	ldap_msgfree( inContext->pResult );
	        inContext->pResult			= nil;
        }

        // data buffer handling parameters
        inContext->fRecNameIndex	= 1;
        inContext->fRecTypeIndex	= 1;
        inContext->fAttrIndex		= 1;
        inContext->offset			= 0;
        inContext->index			= 1;
        inContext->attrCnt			= 0;
        if (inContext->fOpenRecordType != nil)
        {
            delete ( inContext->fOpenRecordType );
        }
        inContext->fOpenRecordType	= nil;
        if (inContext->fOpenRecordName != nil)
        {
            delete ( inContext->fOpenRecordName );
        }
        inContext->fOpenRecordName	= nil;
        
   }

    return( siResult );

} // CleanContextData

//--------------------------------------------------------------------------------------------------
// * PrintNodeName ()
//--------------------------------------------------------------------------------------------------

void CLDAPPlugIn::PrintNodeName ( tDataListPtr inNodeList )
{
        char	   *pPath	= nil;

        pPath = dsGetPathFromListPriv( inNodeList, (char *)"/" );
        if ( pPath != nil )
        {
		CShared::LogIt( 0x0F, (char *)"CLDAPPlugIn::PrintNodeName" );
		CShared::LogIt( 0x0F, pPath );
            
                delete( pPath );
                pPath = nil;
        }

} // PrintNodeName


//------------------------------------------------------------------------------------
//	* GetRecordList
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::GetRecordList ( sGetRecordList *inData )
{
    sInt32					siResult			= eDSNoErr;
    uInt32					uiTotal				= 0;
    uInt32					uiCount				= 0;
    char				   *pRecName			= nil;
    char				   *pRecType			= nil;
    char				   *pLDAPRecType		= nil;
    bool					bAttribOnly			= false;
    tDirPatternMatch		pattMatch			= eDSNoMatch1;
    CAttributeList		   *cpRecNameList		= nil;
    CAttributeList		   *cpRecTypeList		= nil;
    CAttributeList		   *cpAttrTypeList 		= nil;
    sLDAPContextData		   *pContext			= nil;
    CBuff				   *outBuff				= nil;
    int						numRecTypes			= 1;
    bool					bBuffFull			= false;
    bool					separateRecTypes	= false;
    uInt32					countDownRecTypes	= 0;

    try
    {
        // Verify all the parameters
        if ( inData  == nil ) throw( (sInt32)eMemoryError );
        if ( inData->fInDataBuff  == nil ) throw( (sInt32)eDSEmptyBuffer );
        if (inData->fInDataBuff->fBufferSize == 0) throw( (sInt32)eDSEmptyBuffer );

        if ( inData->fInRecNameList  == nil ) throw( (sInt32)eDSEmptyRecordNameList );
        if ( inData->fInRecTypeList  == nil ) throw( (sInt32)eDSEmptyRecordTypeList );
        if ( inData->fInAttribTypeList  == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );

        // Node context data
        pContext = (sLDAPContextData *)gRefTable->GetItemData( inData->fInNodeRef );
        if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );

        // check to make sure context IN is the same as RefTable saved context
        if ( inData->fIOContinueData != nil )
        {
            if ( inData->fIOContinueData != pContext )
            {
                throw( (sInt32)eDSInvalidContext );
            }
        }
        else
        {
            //parameters used for data buffering
            pContext->fRecNameIndex = 1;
            pContext->fRecTypeIndex = 1;
            pContext->fAttrIndex = 1;
            pContext->fTotalRecCount = 0;
            pContext->fLimitRecSearch = 0;
			//check if the client has requested a limit on the number of records to return
			//we only do this the first call into this context for pContext
			if (inData->fOutRecEntryCount >= 0)
			{
				pContext->fLimitRecSearch = inData->fOutRecEntryCount;
			}
        }

        // start with the continue set to nil until buffer gets full and there is more data
        //OR we have more record types to look through
        inData->fIOContinueData		= nil;
		//return zero if nothing found here
		inData->fOutRecEntryCount	= 0;

        // copy the buffer data into a more manageable form
        outBuff = new CBuff();
        if ( outBuff  == nil ) throw( (sInt32)eMemoryError );

        siResult = outBuff->Initialize( inData->fInDataBuff, true );
        if ( siResult != eDSNoErr ) throw( siResult );

        siResult = outBuff->GetBuffStatus();
        if ( siResult != eDSNoErr ) throw( siResult );

        siResult = outBuff->SetBuffType( 'StdA' );
        if ( siResult != eDSNoErr ) throw( siResult );

        // Get the record name list for pattern matching
        cpRecNameList = new CAttributeList( inData->fInRecNameList );
        if ( cpRecNameList  == nil ) throw( (sInt32)eDSEmptyRecordNameList );
        if (cpRecNameList->GetCount() == 0) throw( (sInt32)eDSEmptyRecordNameList );

        // Get the record name pattern match type
        pattMatch = inData->fInPatternMatch;

        // Get the record type list
        // Record type mapping for LDAP to DS API is dealt with below
        cpRecTypeList = new CAttributeList( inData->fInRecTypeList );
        if ( cpRecTypeList  == nil ) throw( (sInt32)eDSEmptyRecordTypeList );
        //save the number of rec types here to use in separating the buffer data
        countDownRecTypes = cpRecTypeList->GetCount() - pContext->fRecTypeIndex + 1;
        if (cpRecTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyRecordTypeList );

        // Get the attribute list
        //KW? at this time would like to simply dump all attributes
        //would expect to do this always since this is where the databuffer is built
        cpAttrTypeList = new CAttributeList( inData->fInAttribTypeList );
        if ( cpAttrTypeList  == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );
        if (cpAttrTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyAttributeTypeList );

        // Get the attribute info only flag
        bAttribOnly = inData->fInAttribInfoOnly;

        // get records of these types
        while ((( cpRecTypeList->GetAttribute( pContext->fRecTypeIndex, &pRecType ) == eDSNoErr ) && (!bBuffFull)) && (!separateRecTypes))
        {
        	//mapping rec types - if std to native
        	numRecTypes = 1;
            pLDAPRecType = MapRecToLDAPType( pRecType, pContext->fConfigTableIndex, numRecTypes );
            //throw on first nil
            if ( pLDAPRecType == nil ) throw( (sInt32)eDSInvalidRecordType );
            while (( pLDAPRecType != nil ) && (!bBuffFull))
            {
				
                // Get the records of this type and these names
                while (( (cpRecNameList->GetAttribute( pContext->fRecNameIndex, &pRecName ) == eDSNoErr) && ( siResult == eDSNoErr) ) && (!bBuffFull))
                 {
                	bBuffFull = false;
                    if ( ::strcmp( pRecName, kDSRecordsAll ) == 0 )
                    {
                        siResult = GetAllRecords( pRecType, pLDAPRecType, cpAttrTypeList, pContext, bAttribOnly, outBuff, uiCount );
                    }
                    else
                    {
                        siResult = GetTheseRecords( pRecName, pRecType, pLDAPRecType, pattMatch, cpAttrTypeList, pContext, bAttribOnly, outBuff, uiCount );
                    }

                    //outBuff->GetDataBlockCount( &uiCount );
					//cannot use this as there may be records added from different record names
					//at which point the first name will fill the buffer with n records and
					//uiCount is reported as n but as the second name fills the buffer with m MORE records
					//the statement above will return the total of n+m and add it to the previous n
					//so that the total erroneously becomes 2n+m and not what it should be as n+m
					//therefore uiCount is extracted directly out of the GetxxxRecord(s) calls

                    if ( siResult == CBuff::kBuffFull )
                    {
                    	bBuffFull = true;
                        //set continue if there is more data available
                        inData->fIOContinueData = pContext;
                        
                        // check to see if buffer is full and no entries added
                        // which implies that the buffer is too small
						if (uiCount == 0)
						{
							throw( (sInt32)eDSBufferTooSmall );
						}

                        uiTotal += uiCount;
                        inData->fOutRecEntryCount = uiTotal;
                        outBuff->SetLengthToSize();
                        siResult = eDSNoErr;
                    }
                    else if ( siResult == eDSNoErr )
                    {
                        uiTotal += uiCount;
	                    pContext->fRecNameIndex++;
                    }

                } // while loop over record names
                
                if (pLDAPRecType != nil)
                {
                    delete( pLDAPRecType );
                    pLDAPRecType = nil;
                }
                
                if (!bBuffFull)
                {
	                numRecTypes++;
	                //get the next mapping
	                pLDAPRecType = MapRecToLDAPType( pRecType, pContext->fConfigTableIndex, numRecTypes );
                }
                
            } // while mapped Rec Type != nil
            
            if (!bBuffFull)
            {
	            pRecType = nil;
	            pContext->fRecTypeIndex++;
	            pContext->fRecNameIndex = 1;
	            //reset the LDAP message ID to zero since now going to go after a new type
	            pContext->msgId = 0;
	            
	            //KW? here we decide to exit with data full of the current type of records
	            // and force a good exit with the data we have so we can come back for the next rec type
            	separateRecTypes = true;
                //set continue since there may be more data available
                inData->fIOContinueData = pContext;
				siResult = eDSNoErr;
				
				//however if this was the last rec type then there will be no more data
	            // check the number of rec types left
	            countDownRecTypes--;
	            if (countDownRecTypes == 0)
	            {
	                inData->fIOContinueData = nil;
				}
            }
            
        } // while loop over record types

        if (( siResult == eDSNoErr ) & (!bBuffFull))
        {
            if ( uiTotal == 0 )
            {
				//if ( ( inData->fIOContinueData == nil ) && ( pContext->fTotalRecCount == 0) )
				//{
				//KW to remove all of this as per
				//2531386  dsGetRecordList should not return an error if record not found
					//only set the record not found if no records were found in any of the record types
					//and this is the last record type looked for
					//siResult = eDSRecordNotFound;
				//}
                outBuff->ClearBuff();
            }
            else
            {
                outBuff->SetLengthToSize();
            }

            inData->fOutRecEntryCount = uiTotal;
        }
    } // try
    
    catch( sInt32 err )
    {
            siResult = err;
    }

	if (pLDAPRecType != nil)
	{
		delete( pLDAPRecType );
		pLDAPRecType = nil;
	}

    if ( cpRecNameList != nil )
    {
            delete( cpRecNameList );
            cpRecNameList = nil;
    }

    if ( cpRecTypeList != nil )
    {
            delete( cpRecTypeList );
            cpRecTypeList = nil;
    }

    if ( cpAttrTypeList != nil )
    {
            delete( cpAttrTypeList );
            cpAttrTypeList = nil;
    }

    if ( outBuff != nil )
    {
            delete( outBuff );
            outBuff = nil;
    }

    return( siResult );

} // GetRecordList


// ---------------------------------------------------------------------------
//	* MapRecToLDAPType
// ---------------------------------------------------------------------------

char* CLDAPPlugIn::MapRecToLDAPType ( char *inRecType, uInt32 inConfigTableIndex, int inIndex )
{
    char				   *outResult	= nil;
    uInt32					uiStrLen	= 0;
    uInt32					uiNativeLen	= ::strlen( kDSNativeRecordTypePrefix );
    uInt32					uiStdLen	= ::strlen( kDSStdRecordTypePrefix );
	sLDAPConfigData		   *pConfig		= nil;
	sMapTuple			   *pMapTuple	= nil;
	int						countNative	= 0;
	sPtrString			   *pPtrString	= nil;
	bool					foundMap	= false;
	
	//idea here is to use the inIndex to request a specific map
	//if inIndex is 1 then the first map will be returned
	//if inIndex is >= 1 and <= totalCount then that map will be returned
	//if inIndex is <= 0 then nil will be returned
	//if inIndex is > totalCount nil will be returned
	//note the inIndex will reference the inIndexth entry ie. start at 1
	//caller can increment the inIndex starting at one and get maps until nil is returned

    if ( ( inRecType != nil ) && (inIndex > 0) )
    {
        uiStrLen = ::strlen( inRecType );

        // First look for native record type
        if ( ::strncmp( inRecType, kDSNativeRecordTypePrefix, uiNativeLen ) == 0 )
        {
            // Make sure we have data past the prefix
            if ( ( uiStrLen > uiNativeLen ) && (inIndex == 1) )
            {
                uiStrLen = uiStrLen - uiNativeLen;
                outResult = new char[ uiStrLen + 2 ];
                ::strcpy( outResult, inRecType + uiNativeLen );
            }
        }//native maps
        //now deal with the standard mappings
		else if ( ::strncmp( inRecType, kDSStdRecordTypePrefix, uiStdLen ) == 0 )
		{
			//find the rec map that we need using inConfigTableIndex
			if (( inConfigTableIndex < gConfigTableLen) && ( inConfigTableIndex >= 0 ))
			{
		        pConfig = (sLDAPConfigData *)gConfigTable->GetItemData( inConfigTableIndex );
		        if (pConfig != nil)
		        {
		        	pMapTuple = pConfig->pRecordMapTuple;
	        	}
			}
			
			//else we use the standard mappings
			if ( pMapTuple == nil )
			{
				pMapTuple = pStdRecordMapTuple;
			}
			
			//go get the mappings
			if ( pMapTuple != nil )
			{
	        	while ((pMapTuple != nil) && !(foundMap))
	        	{
	        		if (pMapTuple->fStandard != nil)
	        		{
	        			if (::strcmp( inRecType, pMapTuple->fStandard ) == 0 )
	        			{
	        				pPtrString = pMapTuple->fNative;
	        				countNative = 0;
	        				while ((pPtrString != nil) && !(foundMap))
	        				{
	        					if (pPtrString->fName != nil)
	        					{
	        						countNative++;
	        						if (inIndex == countNative)
	        						{
		        						outResult = new char[1+::strlen( pPtrString->fName )];
		        						::strcpy( outResult, pPtrString->fName );
		        						foundMap = true;
	        						}
	        					}
	        					pPtrString = pPtrString->pNext;
	        				}//loop through the native maps for a std rec type
	        			} // made Std match
	        		} // (pMapTuple->fStandard != nil)
					pMapTuple = pMapTuple->pNext;
        		}//loop through the std map tuples
    		}// pMapTuple != nil
		}//standard maps
        //passthrough since we don't know what to do with it
        //and we assume the DS API client knows that the LDAP server
        //can handle this record type
        else
        {
        	if ( inIndex == 1 )
        	{
	            outResult = new char[ 1 + ::strlen( inRecType ) ];
	            ::strcpy( outResult, inRecType );
            }
        }//passthrough map
        
    }// ( ( inRecType != nil ) && (inIndex > 0) )

    return( outResult );

} // MapRecToLDAPType

//------------------------------------------------------------------------------------
//	* GetAllRecords
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::GetAllRecords (	char		   *inRecType,
									char		   *inNativeRecType,
                                    CAttributeList *inAttrTypeList,
                                    sLDAPContextData   *inContext,
                                    bool			inAttrOnly,
                                    CBuff		   *inBuff,
									uInt32		   &outRecCount )
{
    sInt32				siResult		= eDSNoErr;
    sInt32				siValCnt		= 0;
    int					ldapReturnCode 	= 0;
    int					ldapMsgId		= 0;
    bool				bufferFull		= false;
    LDAPMessage		   *result			= nil;
    char			   *recName			= nil;
    sLDAPConfigData	   *pConfig			= nil;
    int					searchTO		= 0;
	CDataBuff		   *aRecData		= nil;
	CDataBuff		   *aAttrData		= nil;

	outRecCount = 0; //need to track how many records were found by this call to GetAllRecords
	
    try
    {
    	if (inContext == nil ) throw( (sInt32)eDSInvalidContext);
    	
		aRecData = new CDataBuff();
		if ( aRecData  == nil ) throw( (sInt32)eMemoryError );
		aAttrData = new CDataBuff();
		if ( aAttrData  == nil ) throw( (sInt32)eMemoryError );
		
		//retrieve the config data
		//don't need to retrieve for the case of "generic unknown" so don't check index 0
		if (( inContext->fConfigTableIndex < gConfigTableLen) && ( inContext->fConfigTableIndex >= 1 ))
		{
			pConfig = (sLDAPConfigData *)gConfigTable->GetItemData( inContext->fConfigTableIndex );
			if (pConfig != nil)
			{
				searchTO	= pConfig->fSearchTimeout;
			}
		}
		
        // Here is the bind to the LDAP server
		siResult = RebindTryProc(inContext);
		if ( siResult != eDSNoErr ) throw( siResult );

		// here we check if there was a LDAP message ID in the context
        // If there was we continue to query, otherwise we search anew
        if (inContext->msgId == 0)
        {
            // here is the call to the LDAP server asynchronously which requires
            // host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
            // attribute list (NULL for all), return attrs values flag
            // This returns us the message ID which is used to query the server for the results
            if ( (ldapMsgId = ldap_search( inContext->fHost, inNativeRecType, LDAP_SCOPE_SUBTREE, (char *)"(objectclass=*)", NULL, 0) ) == -1 )
            {
	            throw( (sInt32)eDSNoErr); // used to throw eDSRecordNotFound
            }
            inContext->msgId = ldapMsgId;
        } // msgId == 0
        else
        {
            ldapMsgId = inContext->msgId;
        }
        
		if ( (inContext->fTotalRecCount < inContext->fLimitRecSearch) || (inContext->fLimitRecSearch == 0) )
		{
			//check it there is a carried LDAP message in the context
			if (inContext->pResult != nil)
			{
				result = inContext->pResult;
				ldapReturnCode = LDAP_RES_SEARCH_ENTRY;
			}
			//retrieve a new LDAP message
			else
			{
				if (searchTO == 0)
				{
					ldapReturnCode = ldap_result(inContext->fHost, ldapMsgId, 0, NULL, &result);
				}
				else
				{
					struct	timeval	tv;
					tv.tv_sec	= searchTO;
					tv.tv_usec	= 0;
					ldapReturnCode = ldap_result(inContext->fHost, ldapMsgId, 0, &tv, &result);
				}
			}
		}

		while ( ( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) && !(bufferFull) &&
				( (inContext->fTotalRecCount < inContext->fLimitRecSearch) || (inContext->fLimitRecSearch == 0) ) )
        {
            // package the record into the DS format into the buffer
            // steps to add an entry record to the buffer
            // build the fRecData header
            // build the fAttrData
            // append the fAttrData to the fRecData
            // add the fRecData to the buffer inBuff

            aAttrData->Clear();
            aRecData->Clear();

            if ( inRecType != nil )
            {
				aRecData->AppendShort( ::strlen( inRecType ) );
				aRecData->AppendString( inRecType );
            } // what to do if the inRecType is nil? - never get here then
            else
            {
				aRecData->AppendShort( ::strlen( "Record Type Unknown" ) );
				aRecData->AppendString( "Record Type Unknown" );
            }

            // need to get the record name
            recName = GetRecordName( result, inContext, siResult );
            if ( siResult != eDSNoErr ) throw( siResult );
            if ( recName != nil )
            {
                aRecData->AppendShort( ::strlen( recName ) );
                aRecData->AppendString( recName );

                delete ( recName );
                recName = nil;
            }
            else
            {
                aRecData->AppendShort( ::strlen( "Record Name Unknown" ) );
                aRecData->AppendString( "Record Name Unknown" );
            }

            // need to calculate the number of attribute types ie. siValCnt
            // also need to extract the attributes and place them into fAttrData
            //siValCnt = 0;

            aAttrData->Clear();
            siResult = GetTheseAttributes( inAttrTypeList, result, inAttrOnly, inContext, siValCnt, aAttrData );
            if ( siResult != eDSNoErr ) throw( siResult );
			
            //add the attribute info to the fRecData
            if ( siValCnt == 0 )
            {
                // Attribute count
                aRecData->AppendShort( 0 );
            }
            else
            {
                // Attribute count
                aRecData->AppendShort( siValCnt );
                aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
            }

            // add the fRecData now to the inBuff
            siResult = inBuff->AddData( aRecData->GetData(), aRecData->GetLength() );

            // need to check if the buffer is full
			// need to handle full buffer and keep the result alive for the next call in
            if (siResult == CBuff::kBuffFull)
            {
                bufferFull = true;
                
                //save the result if buffer is full
                inContext->pResult = result;
            }
            else if ( siResult == eDSNoErr )
            {
                ldap_msgfree( result );
                
				outRecCount++; //another record added
				inContext->fTotalRecCount++;
				
                //make sure no result is carried in the context
                inContext->pResult = nil;

				//only get next result if buffer is not full
				if (searchTO == 0)
				{
					ldapReturnCode = ldap_result(inContext->fHost, ldapMsgId, 0, NULL, &result);
				}
				else
				{
					struct	timeval	tv;
					tv.tv_sec	= searchTO;
					tv.tv_usec	= 0;
					ldapReturnCode = ldap_result(inContext->fHost, ldapMsgId, 0, &tv, &result);
				}
            }
            else
            {
//                throw( (sInt32)eDSInvalidBuffFormat);
                
                //make sure no result is carried in the context
                inContext->pResult = nil;
                throw( (sInt32)eDSInvalidBuffFormat);
            }

        } // while loop over entries

        // KW need to check the ldapReturnCode for posible errors ie. ldapMsgId was stale
		if (ldapReturnCode == LDAP_TIMEOUT)
		{
	     	siResult = eDSServerTimeout;
		}
		if ( (result != inContext->pResult) && (result != nil) )
		{
			ldap_msgfree( result );
			result = nil;
		}

    } // try block

    catch( sInt32 err )
    {
        siResult = err;
    }

	if (aRecData != nil)
	{
		delete (aRecData);
		aRecData = nil;
	}
	if (aAttrData != nil)
	{
		delete (aAttrData);
		aAttrData = nil;
	}

    return( siResult );

} // GetAllRecords


//------------------------------------------------------------------------------------
//	* GetRecordEntry
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::GetRecordEntry ( sGetRecordEntry *inData )
{
    sInt32					siResult		= eDSNoErr;
    uInt32					uiIndex			= 0;
    uInt32					uiCount			= 0;
    uInt32					uiOffset		= 0;
	uInt32					uberOffset		= 0;
    char 				   *pData			= nil;
    tRecordEntryPtr			pRecEntry		= nil;
    sLDAPContextData		   *pContext		= nil;
    CBuff					inBuff;
	uInt32					offset			= 0;
	uInt16					usTypeLen		= 0;
	char				   *pRecType		= nil;
	uInt16					usNameLen		= 0;
	char				   *pRecName		= nil;
	uInt16					usAttrCnt		= 0;
	uInt32					buffLen			= 0;

    try
    {
        if ( inData  == nil ) throw( (sInt32)eMemoryError );
        if ( inData->fInOutDataBuff  == nil ) throw( (sInt32)eDSEmptyBuffer );
        if (inData->fInOutDataBuff->fBufferSize == 0) throw( (sInt32)eDSEmptyBuffer );

        siResult = inBuff.Initialize( inData->fInOutDataBuff );
        if ( siResult != eDSNoErr ) throw( siResult );

        siResult = inBuff.GetDataBlockCount( &uiCount );
        if ( siResult != eDSNoErr ) throw( siResult );

        uiIndex = inData->fInRecEntryIndex;
        if ((uiIndex > uiCount) || (uiIndex == 0)) throw( (sInt32)eDSInvalidIndex );

        pData = inBuff.GetDataBlock( uiIndex, &uberOffset );
        if ( pData  == nil ) throw( (sInt32)eDSCorruptBuffer );

		//assume that the length retrieved is valid
		buffLen = inBuff.GetDataBlockLength( uiIndex );
		
		// Skip past this same record length obtained from GetDataBlockLength
		pData	+= 4;
		offset	= 0; //buffLen does not include first four bytes

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffLen - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the record type
		::memcpy( &usTypeLen, pData, 2 );
		
		pData	+= 2;
		offset	+= 2;

		pRecType = pData;
		
		pData	+= usTypeLen;
		offset	+= usTypeLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffLen - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the record name
		::memcpy( &usNameLen, pData, 2 );
		
		pData	+= 2;
		offset	+= 2;

		pRecName = pData;
		
		pData	+= usNameLen;
		offset	+= usNameLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffLen - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the attribute count
		::memcpy( &usAttrCnt, pData, 2 );

		pRecEntry = (tRecordEntry *)::calloc( 1, sizeof( tRecordEntry ) + usNameLen + usTypeLen + 4 + kBuffPad );

		pRecEntry->fRecordNameAndType.fBufferSize	= usNameLen + usTypeLen + 4 + kBuffPad;
		pRecEntry->fRecordNameAndType.fBufferLength	= usNameLen + usTypeLen + 4;

		// Add the record name length
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData, &usNameLen, 2 );
		uiOffset += 2;

		// Add the record name
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, pRecName, usNameLen );
		uiOffset += usNameLen;

		// Add the record type length
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, &usTypeLen, 2 );

		// Add the record type
		uiOffset += 2;
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, pRecType, usTypeLen );

		pRecEntry->fRecordAttributeCount = usAttrCnt;

        pContext = MakeContextData();
        if ( pContext  == nil ) throw( (sInt32)eMemoryAllocError );

        pContext->offset = uberOffset + offset + 4; // context used by next calls of GetAttributeEntry
													// include the four bytes of the buffLen

        gRefTable->AddItem( inData->fOutAttrListRef, pContext );

        inData->fOutRecEntryPtr = pRecEntry;
    }

    catch( sInt32 err )
    {
        siResult = err;
    }

    return( siResult );

} // GetRecordEntry


//------------------------------------------------------------------------------------
//	* GetRecInfo
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::GetRecInfo ( char *inData, tRecordEntryPtr *outRecInfo )
{
    uInt32				siResult	= eDSNoErr;
    uInt32				uiOffset	= 0;
    char				*p			= inData;
    uInt16				usTypeLen	= 0;
    char 			   *pRecType	= nil;
    uInt16				usNameLen	= 0;
    char			   *pRecName	= nil;
    uInt16				usAttrCnt	= 0;
    tRecordEntry	   *pTmpRecInfo	= nil;

    if ( siResult == eDSNoErr )
    {
        // Skip past the record length
        p += 4;

        // Get the length for the record type
        ::memcpy( &usTypeLen, p, 2 );
        p += 2;

        pRecType = p;
        p += usTypeLen;

        // Get the length for the record name
        ::memcpy( &usNameLen, p, 2 );
        p += 2;

        pRecName = p + uiOffset;
        p += usNameLen;

        // Get the attribute count
        ::memcpy( &usAttrCnt, p, 2 );

        pTmpRecInfo = (tRecordEntry *)::calloc( 1, sizeof( tRecordEntry ) + usNameLen + usTypeLen + kBuffPad );
//        ::memset( pTmpRecInfo, 0, sizeof( tRecordEntry ) + usNameLen + usTypeLen + kBuffPad );

        pTmpRecInfo->fRecordNameAndType.fBufferSize		= usNameLen + usTypeLen + 4;
        pTmpRecInfo->fRecordNameAndType.fBufferLength	= usNameLen + usTypeLen + 4;

        // Add the record name length
        ::memcpy( pTmpRecInfo->fRecordNameAndType.fBufferData, &usNameLen, 2 );
        uiOffset += 2;

        // Add the record name
        ::memcpy( pTmpRecInfo->fRecordNameAndType.fBufferData + uiOffset, pRecName, usNameLen );
        uiOffset += usNameLen;

        // Add the record type length
        ::memcpy( pTmpRecInfo->fRecordNameAndType.fBufferData + uiOffset, &usTypeLen, 2 );

        // Add the record type
        uiOffset += 2;
        ::memcpy( pTmpRecInfo->fRecordNameAndType.fBufferData + uiOffset, pRecType, usTypeLen );

        pTmpRecInfo->fRecordAttributeCount = usAttrCnt;
        *outRecInfo = pTmpRecInfo;
    }

    return( siResult );

} // GetRecInfo


//------------------------------------------------------------------------------------
//	* GetTheseAttributes
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::GetTheseAttributes (CAttributeList	*inAttrTypeList,
										LDAPMessage		*inResult,
										bool			 inAttrOnly,
										sLDAPContextData	*inContext,
										sInt32			&outCount,
										CDataBuff		*inDataBuff )
{
	sInt32				siResult				= eDSNoErr;
	sInt32				attrTypeIndex			= 1;
	char			   *pLDAPAttrType			= nil;
	char			   *pAttrType				= nil;
	char			   *pAttr					= nil;
	BerElement		   *ber;
	struct berval	  **bValues;
	int					numAttributes			= 1;
	int					numStdAttributes		= 1;
	bool				bTypeFound				= false;
	int					valCount				= 0;
	char			   *pStdAttrType			= nil;
	bool				bAtLeastOneTypeValid	= false;
	CDataBuff		   *aTmpData				= nil;
	CDataBuff		   *aTmp2Data				= nil;
	bool				bStripCryptPrefix		= false;
	bool				bUsePlus				= true;
	char			   *pLDAPPasswdAttrType		= nil;

	try
	{
		if ( inContext  == nil ) throw( (sInt32)eDSBadContextData );
		
		outCount = 0;
		aTmpData = new CDataBuff();
		if ( aTmpData  == nil ) throw( (sInt32)eMemoryError );
		aTmp2Data = new CDataBuff();
		if ( aTmp2Data  == nil ) throw( (sInt32)eMemoryError );
		inDataBuff->Clear();

		// Get the record attributes with/without the values
		while ( inAttrTypeList->GetAttribute( attrTypeIndex++, &pAttrType ) == eDSNoErr )
		{
			siResult = eDSNoErr;
			if ( ::strcmp( kDSNAttrMetaNodeLocation, pAttrType ) == 0 )
			{
			// we look at kDSNAttrMetaNodeLocation with NO mapping
			// since we have special code to deal with it and we always place the
			// node name into it
				aTmpData->Clear();

				outCount++;

				// Append the attribute name
				aTmpData->AppendShort( ::strlen( pAttrType ) );
				aTmpData->AppendString( pAttrType );

				if ( inAttrOnly == false )
				{
					// Append the attribute value count
					aTmpData->AppendShort( 1 );

					char *tmpStr = nil;

					//extract name from the context data
					//need to add prefix of /LDAPv2/ here since the fName does not contain it in the context
					if ( inContext->fName != nil )
					{
		        		tmpStr = new char[1+8+::strlen(inContext->fName)];
		        		::strcpy( tmpStr, "/LDAPv2/" );
		        		::strcat( tmpStr, inContext->fName );
					}
					else
					{
						tmpStr = new char[1+::strlen("Unknown Node Location")];
						::strcpy( tmpStr, "Unknown Node Location" );
					}

					// Append attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );

					// Add the attribute length
					inDataBuff->AppendLong( aTmpData->GetLength() );
					inDataBuff->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

					// Clear the temp block
					aTmpData->Clear();
				} // if ( inAttrOnly == false )
			} // if ( ::strcmp( kDSNAttrMetaNodeLocation, pAttrType ) == 0 )
			else if ((::strcmp(pAttrType,kDSAttributesAll) == 0) || (::strcmp(pAttrType,kDSAttributesStandardAll) == 0) || (::strcmp(pAttrType,kDSAttributesNativeAll) == 0))
			{
				if ((::strcmp(pAttrType,kDSAttributesAll) == 0) || (::strcmp(pAttrType,kDSAttributesStandardAll) == 0))
				{
					// we look at kDSNAttrMetaNodeLocation with NO mapping
					// since we have special code to deal with it and we always place the
					// node name into it AND we output it here since ALL or ALL Std was asked for
					aTmpData->Clear();

					outCount++;

					// Append the attribute name
					aTmpData->AppendShort( ::strlen( kDSNAttrMetaNodeLocation ) );
					aTmpData->AppendString( kDSNAttrMetaNodeLocation );

					if ( inAttrOnly == false )
					{
						// Append the attribute value count
						aTmpData->AppendShort( 1 );

						char *tmpStr = nil;

						//extract name from the context data
						//need to add prefix of /LDAPv2/ here since the fName does not contain it in the context
						if ( inContext->fName != nil )
						{
							tmpStr = new char[1+8+::strlen(inContext->fName)];
							::strcpy( tmpStr, "/LDAPv2/" );
							::strcat( tmpStr, inContext->fName );
						}
						else
						{
							tmpStr = new char[1+::strlen("Unknown Node Location")];
							::strcpy( tmpStr, "Unknown Node Location" );
						}

						// Append attribute value
						aTmpData->AppendLong( ::strlen( tmpStr ) );
						aTmpData->AppendString( tmpStr );

						delete( tmpStr );

						// Add the attribute length
						inDataBuff->AppendLong( aTmpData->GetLength() );
						inDataBuff->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

						// Clear the temp block
						aTmpData->Clear();
					} // if ( inAttrOnly == false )
					
				//Get the mapping for kDS1AttrPassword
				//If it exists AND it is mapped to something that exists IN LDAP then we will use it
				//otherwise we set bUsePlus and use the kDS1AttrPasswordPlus value of "*******"
				//Don't forget to strip off the {crypt} prefix from kDS1AttrPassword as well
				pLDAPPasswdAttrType = MapAttrToLDAPType( kDS1AttrPassword, inContext->fConfigTableIndex, 1 );
				
				//plan is to output both standard and native attributes if request ALL ie. kDSAttributesAll
				// ie. all the attributes even if they are duplicated
				
				// std attributes go first
				numStdAttributes = 1;
				pStdAttrType = GetNextStdAttrType( inContext->fConfigTableIndex, numStdAttributes );
				while ( pStdAttrType != nil )
				{
					//get the first mapping
					numAttributes = 1;
					pLDAPAttrType = MapAttrToLDAPType( pStdAttrType, inContext->fConfigTableIndex, numAttributes );
					//throw if first nil since no more will be found otherwise proceed until nil
                                        //can't throw since that wipes out retrieval of all the following requested attributes
					//if (pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttrListRef); //KW would like a eDSNoMappingAvailable
					
					//set the indicator to check whether at least one of the native maps
					//form given standard type is correct
					//KW need to provide client with info on failed mappings??
					bAtLeastOneTypeValid = false;
					//set indicator of multiple native to standard mappings
					bTypeFound = false;
					while ( pLDAPAttrType != nil )
					{
						for (	pAttr = ldap_first_attribute (inContext->fHost, inResult, &ber );
								pAttr != NULL; pAttr = ldap_next_attribute(inContext->fHost, inResult, ber ) )
						{
							bStripCryptPrefix = false;
							if (::strcmp( pAttr, pLDAPAttrType ) == 0)
							{
								if (pLDAPPasswdAttrType != nil )
								{
									if ( ( ::strcmp( pAttr, pLDAPPasswdAttrType ) == 0 ) &&
									     ( ::strcmp( pStdAttrType, kDS1AttrPassword ) == 0 ) )
									{
										//want to remove leading "{crypt}" prefix from password if it exists
										bStripCryptPrefix = true;
										//don't need to use the "********" passwdplus
										bUsePlus = false;
										//cleanup
										delete (pLDAPPasswdAttrType);
										pLDAPPasswdAttrType = nil;
									}
								}
								//set the flag indicating that we got a match at least once
								bAtLeastOneTypeValid = true;
								//note that if standard type is incorrectly mapped ie. not found here
								//then the output will not contain any data on that std type
								if (!bTypeFound)
								{
									aTmp2Data->Clear();

									outCount++;
									//use given type in the output NOT mapped to type
									aTmp2Data->AppendShort( ::strlen( pStdAttrType ) );
									aTmp2Data->AppendString( pStdAttrType );
									//set indicator so that multiple values from multiple mapped to types
									//can be added to the given type
									bTypeFound = true;
									
									//set attribute value count to zero
									valCount = 0;
									
									// Clear the temp block
									aTmpData->Clear();
								}

								if (( inAttrOnly == false ) &&
									(( bValues = ldap_get_values_len (inContext->fHost, inResult, pAttr )) != NULL) )
								{
									if (bStripCryptPrefix)
									{
										bStripCryptPrefix = false; //only attempted once here
										// add to the number of values for this attribute
										for (int ii = 0; bValues[ii] != NULL; ii++ )
										valCount++;
										
										// for each value of the attribute
										for (int i = 0; bValues[i] != NULL; i++ )
										{
											if ( ( bValues[i]->bv_len > 7) &&
												(strncasecmp(bValues[i]->bv_val,"{crypt}",7) == 0) )
											{
												// Append attribute value without "{crypt}" prefix
												aTmpData->AppendLong( bValues[i]->bv_len - 7 );
												aTmpData->AppendBlock( (bValues[i]->bv_val) + 7, bValues[i]->bv_len - 7 );
											}
											else
											{
												// Append attribute value
												aTmpData->AppendLong( bValues[i]->bv_len );
												aTmpData->AppendBlock( bValues[i]->bv_val, bValues[i]->bv_len );
											}
										} // for each bValues[i]
										ldap_value_free_len(bValues);
									}
									else
									{
										// add to the number of values for this attribute
										for (int ii = 0; bValues[ii] != NULL; ii++ )
										valCount++;
										
										// for each value of the attribute
										for (int i = 0; bValues[i] != NULL; i++ )
										{
											// Append attribute value
											aTmpData->AppendLong( bValues[i]->bv_len );
											aTmpData->AppendBlock( bValues[i]->bv_val, bValues[i]->bv_len );
										} // for each bValues[i]
										ldap_value_free_len(bValues);
									}
								} // if ( inAttrOnly == false ) && bValues = ldap_get_values_len ...
								
							} // if (::strcmp( pAttr, pLDAPAttrType ) == 0) || 
							if (pAttr != nil)
							{
								ldap_memfree( pAttr );
							}
						} // for ( loop over ldap_next_attribute )
						
						if (ber != nil)
						{
							ber_free( ber, 0 );
						}

						//cleanup pLDAPAttrType if needed
						if (pLDAPAttrType != nil)
						{
							delete (pLDAPAttrType);
							pLDAPAttrType = nil;
						}
						numAttributes++;
						//get the next mapping
						pLDAPAttrType = MapAttrToLDAPType( pStdAttrType, inContext->fConfigTableIndex, numAttributes );				
					} // while ( pLDAPAttrType != nil )
					
					if (bAtLeastOneTypeValid)
					{
						// Append the attribute value count
						aTmp2Data->AppendShort( valCount );
						
						if (valCount != 0)
						{
							// Add the attribute values to the attribute type
							aTmp2Data->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
							valCount = 0;
						}

						// Add the attribute data to the attribute data buffer
						inDataBuff->AppendLong( aTmp2Data->GetLength() );
						inDataBuff->AppendBlock( aTmp2Data->GetData(), aTmp2Data->GetLength() );
					}
					
					//cleanup pStdAttrType if needed
					if (pStdAttrType != nil)
					{
						delete (pStdAttrType);
						pStdAttrType = nil;
					}
					numStdAttributes++;
					//get the next std attribute
					pStdAttrType = GetNextStdAttrType( inContext->fConfigTableIndex, numStdAttributes );
				}// while ( pStdAttrType != nil )
				
				if (bUsePlus)
				{
					// we add kDS1AttrPasswordPlus here
					aTmpData->Clear();

					outCount++;

					// Append the attribute name
					aTmpData->AppendShort( ::strlen( kDS1AttrPasswordPlus ) );
					aTmpData->AppendString( kDS1AttrPasswordPlus );

					if ( inAttrOnly == false )
					{
						// Append the attribute value count
						aTmpData->AppendShort( 1 );

						// Append attribute value
						aTmpData->AppendLong( 8 );
						aTmpData->AppendString( "********" );

						// Add the attribute length
						inDataBuff->AppendLong( aTmpData->GetLength() );
						inDataBuff->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

						// Clear the temp block
						aTmpData->Clear();
					} // if ( inAttrOnly == false )
				} //if (bUsePlus)
					
				}// Std and all
				if ((::strcmp(pAttrType,kDSAttributesAll) == 0) || (::strcmp(pAttrType,kDSAttributesNativeAll) == 0))
				{
				//now we output the native attributes
				for (	pAttr = ldap_first_attribute (inContext->fHost, inResult, &ber );
						pAttr != NULL; pAttr = ldap_next_attribute(inContext->fHost, inResult, ber ) )
				{
					aTmpData->Clear();

					outCount++;
					
					if ( pAttr != nil )
					{
						aTmpData->AppendShort( ::strlen( pAttr ) + ::strlen( kDSNativeAttrTypePrefix ) );
						aTmpData->AppendString( (char *)kDSNativeAttrTypePrefix );
						aTmpData->AppendString( pAttr );
					}

					if (( inAttrOnly == false ) &&
						(( bValues = ldap_get_values_len (inContext->fHost, inResult, pAttr )) != NULL) )
					{
					
						// calculate the number of values for this attribute
						valCount = 0;
						for (int ii = 0; bValues[ii] != NULL; ii++ )
						   valCount++;
						   
						// Append the attribute value count
						aTmpData->AppendShort( valCount );

						// for each value of the attribute
						for (int i = 0; bValues[i] != NULL; i++ )
						{
							// Append attribute value
							aTmpData->AppendLong( bValues[i]->bv_len );
							aTmpData->AppendBlock( bValues[i]->bv_val, bValues[i]->bv_len );
						} // for each bValues[i]
						ldap_value_free_len(bValues);
					} // if ( inAttrOnly == false ) && bValues = ldap_get_values_len ...
					
					// Add the attribute length
					inDataBuff->AppendLong( aTmpData->GetLength() );
					inDataBuff->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

					// Clear the temp block
					aTmpData->Clear();
					
					if (pAttr != nil)
					{
						ldap_memfree( pAttr );
					}
				} // for ( loop over ldap_next_attribute )
				
				if (ber != nil)
				{
					ber_free( ber, 0 );
				}

				}//Native and all
			}
			else //we have a specific attribute in mind
			{
				bStripCryptPrefix = false;
				//here we first check for the request for kDS1AttrPasswordPlus
				//ie. we see if kDS1AttrPassword is mapped and return that value if it is -- otherwise
				//we return the special value of "********" which is apparently never a valid crypt password
				//but this signals the requestor of kDS1AttrPasswordPlus to attempt to auth against the LDAP
				//server through doAuthentication via dsDoDirNodeAuth
				//get the first mapping
				numAttributes = 1;
				if (::strcmp( kDS1AttrPasswordPlus, pAttrType ) == 0)
				{
					//want to remove leading "{crypt}" prefix from password if it exists
					bStripCryptPrefix = true;
					pLDAPAttrType = MapAttrToLDAPType( kDS1AttrPassword, inContext->fConfigTableIndex, numAttributes );
					//set the indicator to check whether at least one of the native maps
					//form given standard type is correct
					//KW need to provide client with info on failed mappings??
					bAtLeastOneTypeValid = false;
					//set indicator of multiple native to standard mappings
					bTypeFound = false;

					if (pLDAPAttrType == nil)
					{
						bAtLeastOneTypeValid = true;
						
						//here we fill the value with "*******"
						aTmp2Data->Clear();
						outCount++;
						//use given type in the output NOT mapped to type
						aTmp2Data->AppendShort( ::strlen( pAttrType ) );
						aTmp2Data->AppendString( pAttrType );
						//set indicator so that multiple values from multiple mapped to types
						//can be added to the given type
						bTypeFound = true;
								
						//set attribute value count to one
						valCount = 1;
								
						// Clear the temp block
						aTmpData->Clear();
						// Append attribute value
						aTmpData->AppendLong( 8 );
						aTmpData->AppendString( "********" );
					}
				}
				else
				{
					pLDAPAttrType = MapAttrToLDAPType( pAttrType, inContext->fConfigTableIndex, numAttributes );
					//throw if first nil since no more will be found otherwise proceed until nil
					// can't throw since that wipes out retrieval of all the following requested attributes
					//if (pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttrListRef); //KW would like a eDSNoMappingAvailable

					//set the indicator to check whether at least one of the native maps
					//form given standard type is correct
					//KW need to provide client with info on failed mappings??
					bAtLeastOneTypeValid = false;
					//set indicator of multiple native to standard mappings
					bTypeFound = false;
				}
				
				while ( pLDAPAttrType != nil )
				{
					for (	pAttr = ldap_first_attribute (inContext->fHost, inResult, &ber );
							pAttr != NULL; pAttr = ldap_next_attribute(inContext->fHost, inResult, ber ) )
					{
						if (::strcmp( pAttr, pLDAPAttrType ) == 0)
						{
							bAtLeastOneTypeValid = true;
							//note that if standard type is incorrectly mapped ie. not found here
							//then the output will not contain any data on that std type
							if (!bTypeFound)
							{
								aTmp2Data->Clear();

								outCount++;
								//use given type in the output NOT mapped to type
								aTmp2Data->AppendShort( ::strlen( pAttrType ) );
								aTmp2Data->AppendString( pAttrType );
								//set indicator so that multiple values from multiple mapped to types
								//can be added to the given type
								bTypeFound = true;
								
								//set attribute value count to zero
								valCount = 0;
								
								// Clear the temp block
								aTmpData->Clear();
							}

							if (( inAttrOnly == false ) &&
								(( bValues = ldap_get_values_len (inContext->fHost, inResult, pAttr )) != NULL) )
							{
							
								if (bStripCryptPrefix)
								{
									// add to the number of values for this attribute
									for (int ii = 0; bValues[ii] != NULL; ii++ )
									valCount++;
									
									// for each value of the attribute
									for (int i = 0; bValues[i] != NULL; i++ )
									{
										if ( ( bValues[i]->bv_len > 7) &&
											(strncasecmp(bValues[i]->bv_val,"{crypt}",7) == 0) )
										{
											// Append attribute value without "{crypt}" prefix
											aTmpData->AppendLong( bValues[i]->bv_len - 7 );
											aTmpData->AppendBlock( (bValues[i]->bv_val) + 7, bValues[i]->bv_len - 7 );
										}
										else
										{
											// Append attribute value
											aTmpData->AppendLong( bValues[i]->bv_len );
											aTmpData->AppendBlock( bValues[i]->bv_val, bValues[i]->bv_len );
										}
									} // for each bValues[i]
									ldap_value_free_len(bValues);
								}
								else
								{
									// add to the number of values for this attribute
									for (int ii = 0; bValues[ii] != NULL; ii++ )
									valCount++;
									
									// for each value of the attribute
									for (int i = 0; bValues[i] != NULL; i++ )
									{
										// Append attribute value
										aTmpData->AppendLong( bValues[i]->bv_len );
										aTmpData->AppendBlock( bValues[i]->bv_val, bValues[i]->bv_len );
									} // for each bValues[i]
									ldap_value_free_len(bValues);
								}
							} // if ( inAttrOnly == false ) && bValues = ldap_get_values_len ...
							
						} // if ( (::strcmp( kDSAttributesAll, pLDAPAttrType ) == 0) || 
							//    (::strcmp( niProp.nip_name, pLDAPAttrType ) == 0) )
							
						if (pAttr != nil)
						{
							ldap_memfree( pAttr );
						}
					} // for ( loop over ldap_next_attribute )

					if (ber != nil)
					{
						ber_free( ber, 0 );
					}

					//cleanup pLDAPAttrType if needed
					if (pLDAPAttrType != nil)
					{
						delete (pLDAPAttrType);
						pLDAPAttrType = nil;
					}
					numAttributes++;
					//get the next mapping
					pLDAPAttrType = MapAttrToLDAPType( pAttrType, inContext->fConfigTableIndex, numAttributes );				
				} // while ( pLDAPAttrType != nil )
				
				if (bAtLeastOneTypeValid)
				{
					// Append the attribute value count
					aTmp2Data->AppendShort( valCount );
					
					if (valCount != 0)
					{
						// Add the attribute values to the attribute type
						aTmp2Data->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
						valCount = 0;
					}

					// Add the attribute data to the attribute data buffer
					inDataBuff->AppendLong( aTmp2Data->GetLength() );
					inDataBuff->AppendBlock( aTmp2Data->GetData(), aTmp2Data->GetLength() );
				}
			} // else not kDSNAttrMetaNodeLocation
			
		} // while ( inAttrTypeList->GetAttribute( attrTypeIndex++, &pAttrType ) == eDSNoErr )
		
	} // try

	catch( sInt32 err )
	{
		siResult = err;
	}
	
	if ( aTmpData != nil )
	{
		delete( aTmpData );
		aTmpData = nil;
	}
	if ( aTmp2Data != nil )
	{
		delete( aTmp2Data );
		aTmp2Data = nil;
	}

	return( siResult );

} // GetTheseAttributes


//------------------------------------------------------------------------------------
//	* GetRecordName
//------------------------------------------------------------------------------------

char *CLDAPPlugIn::GetRecordName (	LDAPMessage	*inResult,
                                        sLDAPContextData	*inContext,
                                        sInt32		&errResult )
{
        char		       *recName			= nil;
	char		       *pLDAPAttrType		= nil;
	char		       *pAttr			= nil;
	BerElement	       *ber;
	struct berval	      **bValues;
	int			numAttributes		= 1;
	bool			bTypeFound		= false;

	try
	{
			if ( inContext  == nil ) throw( (sInt32)eDSBadContextData );

            errResult = eDSNoErr;
            
            //get the first mapping
            numAttributes = 1;
            pLDAPAttrType = MapAttrToLDAPType( kDSNAttrRecordName, inContext->fConfigTableIndex, numAttributes );
            //throw if first nil since no more will be found otherwise proceed until nil
            if (pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttrListRef); //KW would like a eDSNoMappingAvailable
            
            //set indicator of multiple native to standard mappings
            bTypeFound = false;
            while ( ( pLDAPAttrType != nil ) && (!bTypeFound) )
            {
                for (	pAttr = ldap_first_attribute (inContext->fHost, inResult, &ber );
                                pAttr != NULL; pAttr = ldap_next_attribute(inContext->fHost, inResult, ber ) )
                {
                    if (::strcmp( pAttr, pLDAPAttrType ) == 0)
                    {
                        if ( ( bValues = ldap_get_values_len (inContext->fHost, inResult, pAttr )) != NULL )
                        {
                            // for first value of the attribute
                            recName = new char[1 + bValues[0]->bv_len];
                            ::strcpy( recName, bValues[0]->bv_val );
                            //we found a value so stop looking
                            bTypeFound = true;
							ldap_value_free_len(bValues);
                        } // if ( bValues = ldap_get_values_len ...)
                            
                    } // if ( (::strcmp( kDSAttributesAll, pLDAPAttrType ) == 0) ||) 
                                
					if (pAttr != nil)
					{
						ldap_memfree( pAttr );
					}
                } // for ( loop over ldap_next_attribute )
        
				if (ber != nil)
				{
					ber_free( ber, 0 );
				}

                //cleanup pLDAPAttrType if needed
                if (pLDAPAttrType != nil)
                {
                        delete (pLDAPAttrType);
                        pLDAPAttrType = nil;
                }
                numAttributes++;
                //get the next mapping
                pLDAPAttrType = MapAttrToLDAPType( kDSNAttrRecordName, inContext->fConfigTableIndex, numAttributes );				
            } // while ( pLDAPAttrType != nil )
			//cleanup pLDAPAttrType if needed
			if (pLDAPAttrType != nil)
			{
				delete (pLDAPAttrType);
				pLDAPAttrType = nil;
			}
           
	} // try

	catch( sInt32 err )
	{
		errResult = err;
	}

	return( recName );

} // GetRecordName


// ---------------------------------------------------------------------------
//	* MapAttrToLDAPType
// ---------------------------------------------------------------------------

char* CLDAPPlugIn::MapAttrToLDAPType ( char *inAttrType, uInt32 inConfigTableIndex, int inIndex )
{
	char				   *outResult	= nil;
	uInt32					uiStrLen	= 0;
	uInt32					uiNativeLen	= ::strlen( kDSNativeAttrTypePrefix );
	uInt32					uiStdLen	= ::strlen( kDSStdAttrTypePrefix );
	sLDAPConfigData		   *pConfig		= nil;
	sMapTuple			   *pMapTuple	= nil;
	int						countNative	= 0;
	sPtrString			   *pPtrString	= nil;
	bool					foundMap	= false;

	//idea here is to use the inIndex to request a specific map
	//if inIndex is 1 then the first map will be returned
	//if inIndex is >= 1 and <= totalCount then that map will be returned
	//if inIndex is <= 0 then nil will be returned
	//if inIndex is > totalCount nil will be returned
	//note the inIndex will reference the inIndexth entry ie. start at 1
	//caller can increment the inIndex starting at one and get maps until nil is returned

	if (( inAttrType != nil ) && (inIndex > 0))
	{
		uiStrLen = ::strlen( inAttrType );

		// First look for native attribute type
		if ( ::strncmp( inAttrType, kDSNativeAttrTypePrefix, uiNativeLen ) == 0 )
		{
			// Make sure we have data past the prefix
			if (( uiStrLen > uiNativeLen ) && (inIndex == 1))
			{
				uiStrLen = uiStrLen - uiNativeLen;
				outResult = new char[ uiStrLen + 1 ];
				::strcpy( outResult, inAttrType + uiNativeLen );
			}
		} // native maps
		else if ( ::strncmp( inAttrType, kDSStdAttrTypePrefix, uiStdLen ) == 0 )
		{
			//find the attr map that we need using inConfigTableIndex
			if (( inConfigTableIndex < gConfigTableLen) && ( inConfigTableIndex >= 0 ))
			{
		        pConfig = (sLDAPConfigData *)gConfigTable->GetItemData( inConfigTableIndex );
		        if (pConfig != nil)
		        {
		        	pMapTuple = pConfig->pAttributeMapTuple;
	        	}
			}
			
			//else we use the standard mappings
			if ( pMapTuple == nil )
			{
				pMapTuple = pStdAttributeMapTuple;
			}
			
			//go get the mappings
			if ( pMapTuple != nil )
			{
	        	while ((pMapTuple != nil) && !(foundMap))
	        	{
	        		if (pMapTuple->fStandard != nil)
	        		{
	        			if (::strcmp( inAttrType, pMapTuple->fStandard ) == 0 )
	        			{
	        				pPtrString = pMapTuple->fNative;
	        				countNative = 0;
	        				while ((pPtrString != nil) && !(foundMap))
	        				{
	        					if (pPtrString->fName != nil)
	        					{
	        						countNative++;
	        						if (inIndex == countNative)
	        						{
		        						outResult = new char[1+::strlen( pPtrString->fName )];
		        						::strcpy( outResult, pPtrString->fName );
		        						foundMap = true;
	        						}
	        					}
	        					pPtrString = pPtrString->pNext;
	        				}//loop through the native maps for a std attr type
	        			} // made Std match
	        		} // (pMapTuple->fStandard != nil)
					pMapTuple = pMapTuple->pNext;
        		}//loop through the std map tuples
    		}// pMapTuple != nil
		}//standard maps
        //passthrough since we don't know what to do with it
        //and we assume the DS API client knows that the LDAP server
        //can handle this attribute type
		else
		{
			if ( inIndex == 1 )
			{
				outResult = new char[ 1 + ::strlen( inAttrType ) ];
				::strcpy( outResult, inAttrType );
			}
		}// passthrough map
	}// if (( inAttrType != nil ) && (inIndex > 0))

	return( outResult );

} // MapAttrToLDAPType


// ---------------------------------------------------------------------------
//	* MapAttrToLDAPTypeArray
// ---------------------------------------------------------------------------

char** CLDAPPlugIn::MapAttrToLDAPTypeArray ( char *inAttrType, uInt32 inConfigTableIndex )
{
	char				  **outResult	= nil;
	uInt32					uiStrLen	= 0;
	uInt32					uiNativeLen	= ::strlen( kDSNativeAttrTypePrefix );
	uInt32					uiStdLen	= ::strlen( kDSStdAttrTypePrefix );
	sLDAPConfigData		   *pConfig		= nil;
	sMapTuple			   *pMapTuple	= nil;
	int						countNative	= 0;
	sPtrString			   *pPtrString	= nil;
	bool					foundMap	= false;

	if ( inAttrType != nil )
	{
		uiStrLen = ::strlen( inAttrType );

		// First look for native attribute type
		if ( ::strncmp( inAttrType, kDSNativeAttrTypePrefix, uiNativeLen ) == 0 )
		{
			// Make sure we have data past the prefix
			if ( uiStrLen > uiNativeLen )
			{
				uiStrLen = uiStrLen - uiNativeLen;
				outResult = (char **)::calloc( 2, sizeof( char * ) );
				*outResult = new char[ uiStrLen + 1 ];
				::strcpy( *outResult, inAttrType + uiNativeLen );
			}
		} // native maps
		else if ( ::strncmp( inAttrType, kDSStdAttrTypePrefix, uiStdLen ) == 0 )
		{
			//find the attr map that we need using inConfigTableIndex
			if (( inConfigTableIndex < gConfigTableLen) && ( inConfigTableIndex >= 0 ))
			{
		        pConfig = (sLDAPConfigData *)gConfigTable->GetItemData( inConfigTableIndex );
		        if (pConfig != nil)
		        {
		        	pMapTuple = pConfig->pAttributeMapTuple;
	        	}
			}
			
			//else we use the standard mappings
			if ( pMapTuple == nil )
			{
				pMapTuple = pStdAttributeMapTuple;
			}
			
			//go get the mappings
			if ( pMapTuple != nil )
			{
	        	while ((pMapTuple != nil) && !(foundMap))
	        	{
	        		if (pMapTuple->fStandard != nil)
	        		{
	        			if (::strcmp( inAttrType, pMapTuple->fStandard ) == 0 )
	        			{
	        				foundMap = true;
							pPtrString = pMapTuple->fNative;
	        				countNative = 0;
	        				while (pPtrString != nil)
	        				{
	        					if (pPtrString->fName != nil)
	        					{
	        						countNative++;
	        					}
	        					pPtrString = pPtrString->pNext;
	        				}//loop through the native maps to count them
							outResult = (char **)::calloc( countNative + 1, sizeof(char *) );
							pPtrString = pMapTuple->fNative;
	        				countNative = 0;
	        				while (pPtrString != nil)
	        				{
	        					if (pPtrString->fName != nil)
	        					{
									outResult[countNative] = new char[1+::strlen( pPtrString->fName )];
									::strcpy( outResult[countNative], pPtrString->fName );
	        						countNative++;
	        					}
	        					pPtrString = pPtrString->pNext;
	        				}//loop through the native maps to copy them out
	        			} // made Std match
	        		} // (pMapTuple->fStandard != nil)
					pMapTuple = pMapTuple->pNext;
        		}//loop through the std map tuples
    		}// pMapTuple != nil
		}//standard maps
        //passthrough since we don't know what to do with it
        //and we assume the DS API client knows that the LDAP server
        //can handle this attribute type
		else
		{
			outResult = (char **)::calloc( 2, sizeof(char *) );
			*outResult = new char[ 1 + ::strlen( inAttrType ) ];
			::strcpy( *outResult, inAttrType );
		}// passthrough map
	}// if ( inAttrType != nil )

	return( outResult );

} // MapAttrToLDAPTypeArray


//------------------------------------------------------------------------------------
//	* GetAttributeEntry
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::GetAttributeEntry ( sGetAttributeEntry *inData )
{
	sInt32					siResult			= eDSNoErr;
	uInt16					usAttrTypeLen		= 0;
	uInt16					usAttrCnt			= 0;
	uInt32					usAttrLen			= 0;
	uInt16					usValueCnt			= 0;
	uInt32					usValueLen			= 0;
	uInt32					i					= 0;
	uInt32					uiIndex				= 0;
	uInt32					uiAttrEntrySize		= 0;
	uInt32					uiOffset			= 0;
	uInt32					uiTotalValueSize	= 0;
	uInt32					offset				= 4;
	uInt32					buffSize			= 0;
	uInt32					buffLen				= 0;
	char				   *p			   		= nil;
	char				   *pAttrType	   		= nil;
	tDataBufferPtr			pDataBuff			= nil;
	tAttributeEntryPtr		pAttribInfo			= nil;
	sLDAPContextData		   *pAttrContext		= nil;
	sLDAPContextData		   *pValueContext		= nil;

	try
	{
		if ( inData  == nil ) throw( (sInt32)eMemoryError );

		pAttrContext = (sLDAPContextData *)gRefTable->GetItemData( inData->fInAttrListRef );
		if ( pAttrContext  == nil ) throw( (sInt32)eDSBadContextData );

		uiIndex = inData->fInAttrInfoIndex;
		if (uiIndex == 0) throw( (sInt32)eDSInvalidIndex );
				
		pDataBuff = inData->fInOutDataBuff;
		if ( pDataBuff  == nil ) throw( (sInt32)eDSNullDataBuff );
		
		buffSize	= pDataBuff->fBufferSize;
		//buffLen		= pDataBuff->fBufferLength;
		//here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
		//and the fBufferLength is the overall length of the data for all blocks at the end of the data block
		//the value ALSO includes the bookkeeping data at the start of the data block
		//so we need to read it here

		p		= pDataBuff->fBufferData + pAttrContext->offset;
		offset	= pAttrContext->offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );
				
		// Get the attribute count
		::memcpy( &usAttrCnt, p, 2 );
		if (uiIndex > usAttrCnt) throw( (sInt32)eDSInvalidIndex );

		// Move 2 bytes
		p		+= 2;
		offset	+= 2;

		// Skip to the attribute that we want
		for ( i = 1; i < uiIndex; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (4 > (sInt32)(buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
			// Get the length for the attribute
			::memcpy( &usAttrLen, p, 4 );

			// Move the offset past the length word and the length of the data
			p		+= 4 + usAttrLen;
			offset	+= 4 + usAttrLen;
		}

		// Get the attribute offset
		uiOffset = offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (4 > (sInt32)(buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the attribute block
		::memcpy( &usAttrLen, p, 4 );

		// Skip past the attribute length
		p		+= 4;
		offset	+= 4;

		//set the bufLen to stricter range
		buffLen = offset + usAttrLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffLen - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the length for the attribute type
		::memcpy( &usAttrTypeLen, p, 2 );
		
		pAttrType = p + 2;
		p		+= 2 + usAttrTypeLen;
		offset	+= 2 + usAttrTypeLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffLen - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get number of values for this attribute
		::memcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;
		
		for ( i = 0; i < usValueCnt; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (4 > (sInt32)(buffLen - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
			// Get the length for the value
			::memcpy( &usValueLen, p, 4 );
			
			p		+= 4 + usValueLen;
			offset	+= 4 + usValueLen;
			
			uiTotalValueSize += usValueLen;
		}

		uiAttrEntrySize = sizeof( tAttributeEntry ) + usAttrTypeLen + kBuffPad;
		pAttribInfo = (tAttributeEntry *)::calloc( 1, uiAttrEntrySize );

		pAttribInfo->fAttributeValueCount				= usValueCnt;
		pAttribInfo->fAttributeDataSize					= uiTotalValueSize;
		pAttribInfo->fAttributeValueMaxSize				= 512;				// KW this is not used anywhere
		pAttribInfo->fAttributeSignature.fBufferSize	= usAttrTypeLen + kBuffPad;
		pAttribInfo->fAttributeSignature.fBufferLength	= usAttrTypeLen;
		::memcpy( pAttribInfo->fAttributeSignature.fBufferData, pAttrType, usAttrTypeLen );

		pValueContext = MakeContextData();
		if ( pValueContext  == nil ) throw( (sInt32)eMemoryAllocError );

		pValueContext->offset = uiOffset;

		gRefTable->AddItem( inData->fOutAttrValueListRef, pValueContext );

		inData->fOutAttrInfoPtr = pAttribInfo;
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetAttributeEntry


//------------------------------------------------------------------------------------
//	* GetAttributeValue
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::GetAttributeValue ( sGetAttributeValue *inData )
{
	sInt32						siResult		= eDSNoErr;
	uInt16						usValueCnt		= 0;
	uInt32						usValueLen		= 0;
	uInt16						usAttrNameLen	= 0;
	uInt32						i				= 0;
	uInt32						uiIndex			= 0;
	uInt32						offset			= 0;
	char					   *p				= nil;
	tDataBuffer				   *pDataBuff		= nil;
	tAttributeValueEntry	   *pAttrValue		= nil;
	sLDAPContextData			   *pValueContext	= nil;
	uInt32						buffSize		= 0;
	uInt32						buffLen			= 0;
	uInt32						attrLen			= 0;

	try
	{
		pValueContext = (sLDAPContextData *)gRefTable->GetItemData( inData->fInAttrValueListRef );
		if ( pValueContext  == nil ) throw( (sInt32)eDSBadContextData );

		uiIndex = inData->fInAttrValueIndex;
		if (uiIndex == 0) throw( (sInt32)eDSInvalidIndex );
		
		pDataBuff = inData->fInOutDataBuff;
		if ( pDataBuff  == nil ) throw( (sInt32)eDSNullDataBuff );

		buffSize	= pDataBuff->fBufferSize;
		//buffLen		= pDataBuff->fBufferLength;
		//here we can't use fBufferLength for the buffLen SINCE the buffer is packed at the END of the data block
		//and the fBufferLength is the overall length of the data for all blocks at the end of the data block
		//the value ALSO includes the bookkeeping data at the start of the data block
		//so we need to read it here

		p		= pDataBuff->fBufferData + pValueContext->offset;
		offset	= pValueContext->offset;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (4 > (sInt32)(buffSize - offset)) throw( (sInt32)eDSInvalidBuffFormat );
				
		// Get the buffer length
		::memcpy( &attrLen, p, 4 );

		//now add the offset to the attr length for the value of buffLen to be used to check for buffer overruns
		//AND add the length of the buffer length var as stored ie. 4 bytes
		buffLen		= attrLen + pValueContext->offset + 4;
		if (buffLen > buffSize) throw( (sInt32)eDSInvalidBuffFormat );

		// Skip past the attribute length
		p		+= 4;
		offset	+= 4;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffLen - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the attribute name length
		::memcpy( &usAttrNameLen, p, 2 );
		
		p		+= 2 + usAttrNameLen;
		offset	+= 2 + usAttrNameLen;

		// Do record check, verify that offset is not past end of buffer, etc.
		if (2 > (sInt32)(buffLen - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
		// Get the value count
		::memcpy( &usValueCnt, p, 2 );
		
		p		+= 2;
		offset	+= 2;

		if (uiIndex > usValueCnt) throw( (sInt32)eDSInvalidIndex );

		// Skip to the value that we want
		for ( i = 1; i < uiIndex; i++ )
		{
			// Do record check, verify that offset is not past end of buffer, etc.
			if (4 > (sInt32)(buffLen - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
			// Get the length for the value
			::memcpy( &usValueLen, p, 4 );
			
			p		+= 4 + usValueLen;
			offset	+= 4 + usValueLen;
		}

		// Do record check, verify that offset is not past end of buffer, etc.
		if (4 > (sInt32)(buffLen - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
		::memcpy( &usValueLen, p, 4 );
		
		p		+= 4;
		offset	+= 4;

		//if (usValueLen == 0) throw( (sInt32)eDSInvalidBuffFormat ); //if zero is it okay?

		pAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + usValueLen + kBuffPad );

		pAttrValue->fAttributeValueData.fBufferSize		= usValueLen + kBuffPad;
		pAttrValue->fAttributeValueData.fBufferLength	= usValueLen;
		
		// Do record check, verify that offset is not past end of buffer, etc.
		if ( (sInt32)usValueLen > (sInt32)(buffLen - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		
		::memcpy( pAttrValue->fAttributeValueData.fBufferData, p, usValueLen );

			// Set the attribute value ID
		pAttrValue->fAttributeValueID = CalcCRC( pAttrValue->fAttributeValueData.fBufferData );

		inData->fOutAttrValue = pAttrValue;
			
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetAttributeValue



// ---------------------------------------------------------------------------
//	* CalcCRC
// ---------------------------------------------------------------------------

uInt32 CLDAPPlugIn::CalcCRC ( char *inStr )
{
	char		   *p			= inStr;
	sInt32			siI			= 0;
	sInt32			siStrLen	= 0;
	uInt32			uiCRC		= 0xFFFFFFFF;
	CRCCalc			aCRCCalc;

	if ( inStr != nil )
	{
		siStrLen = ::strlen( inStr );

		for ( siI = 0; siI < siStrLen; ++siI )
		{
			uiCRC = aCRCCalc.UPDC32( *p, uiCRC );
			p++;
		}
	}

	return( uiCRC );

} // CalcCRC


//------------------------------------------------------------------------------------
//	* GetTheseRecords
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::GetTheseRecords (	char			   *inConstRecName,
									char			   *inConstRecType,
									char			   *inNativeRecType,
									tDirPatternMatch	patternMatch,
									CAttributeList	   *inAttrTypeList,
									sLDAPContextData	   *inContext,
									bool				inAttrOnly,
									CBuff			   *inBuff,
									uInt32			   &outRecCount )
{
	sInt32					siResult		= eDSNoErr;
    sInt32					siValCnt		= 0;
    int						ldapReturnCode 	= 0;
    int						ldapMsgId		= 0;
    bool					bufferFull		= false;
    LDAPMessage			   *result			= nil;
    char				   *recName			= nil;
    char				   *queryFilter		= nil;
    sLDAPConfigData		   *pConfig			= nil;
    int						searchTO		= 0;
	CDataBuff			   *aAttrData		= nil;
	CDataBuff			   *aRecData		= nil;

	//build the record query string
	queryFilter = BuildLDAPQueryFilter((char *)kDSNAttrRecordName, inConstRecName, patternMatch, inContext->fConfigTableIndex, false);
	    
	outRecCount = 0; //need to track how many records were found by this call to GetTheseRecords
	
    try
    {
    
    	if (inContext == nil ) throw( (sInt32)eDSInvalidContext);
    	
    	// check to make sure the queryFilter is not nil
    	if ( queryFilter == nil ) throw( (sInt32)eDSNullParameter );
    	
		aAttrData = new CDataBuff();
		if ( aAttrData  == nil ) throw( (sInt32)eMemoryError );
		aRecData = new CDataBuff();
		if ( aRecData  == nil ) throw( (sInt32)eMemoryError );
		
		//retrieve the config data
		//don't need to retrieve for the case of "generic unknown" so don't check index 0
		if (( inContext->fConfigTableIndex < gConfigTableLen) && ( inContext->fConfigTableIndex >= 1 ))
		{
			pConfig = (sLDAPConfigData *)gConfigTable->GetItemData( inContext->fConfigTableIndex );
			if (pConfig != nil)
			{
				searchTO	= pConfig->fSearchTimeout;
			}
		}
		
        // Here is the bind to the LDAP server
		siResult = RebindTryProc(inContext);
		if ( siResult != eDSNoErr ) throw( siResult );

        // here we check if there was a LDAP message ID in the context
        // If there was we continue to query, otherwise we search anew
        if ( inContext->msgId == 0 )
        {
            // here is the call to the LDAP server asynchronously which requires
            // host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
            // attribute list (NULL for all), return attrs values flag
            // This returns us the message ID which is used to query the server for the results
            if ( (ldapMsgId = ldap_search( inContext->fHost, inNativeRecType, LDAP_SCOPE_SUBTREE, queryFilter, NULL, 0) ) == -1 )
            {
	            throw( (sInt32)eDSNoErr); // used to throw eDSRecordNotFound
            }
            inContext->msgId = ldapMsgId;
        } // msgId == 0
        else
        {
            ldapMsgId = inContext->msgId;
        }
        
		if ( (inContext->fTotalRecCount < inContext->fLimitRecSearch) || (inContext->fLimitRecSearch == 0) )
		{
			//check if there is a carried LDAP message in the context
			//KW with a rebind here in between context calls we still have the previous result
			//however, the next call will start right over in the whole context of the ldap_search
			if (inContext->pResult != nil)
			{
				result = inContext->pResult;
				ldapReturnCode = LDAP_RES_SEARCH_ENTRY;
			}
			//retrieve a new LDAP message
			else
			{
				if (searchTO == 0)
				{
					ldapReturnCode = ldap_result(inContext->fHost, ldapMsgId, 0, NULL, &result);
				}
				else
				{
					struct	timeval	tv;
					tv.tv_sec	= searchTO;
					tv.tv_usec	= 0;
					ldapReturnCode = ldap_result(inContext->fHost, ldapMsgId, 0, &tv, &result);
				}
			}
		}

		while ( ( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) && !(bufferFull) &&
				( (inContext->fTotalRecCount < inContext->fLimitRecSearch) || (inContext->fLimitRecSearch == 0) ) )
        {
            // package the record into the DS format into the buffer
            // steps to add an entry record to the buffer
            // build the aRecData header
            // build the aAttrData
            // append the aAttrData to the aRecData
            // add the aRecData to the buffer inBuff

            aRecData->Clear();

            if ( inConstRecType != nil )
            {
                aRecData->AppendShort( ::strlen( inConstRecType ) );
                aRecData->AppendString( inConstRecType );
            }
            else
            {
                aRecData->AppendShort( ::strlen( "Record Type Unknown" ) );
                aRecData->AppendString( "Record Type Unknown" );
            }

            // need to get the record name
            recName = GetRecordName( result, inContext, siResult );
            if ( siResult != eDSNoErr ) throw( siResult );
            if ( recName != nil )
            {
                aRecData->AppendShort( ::strlen( recName ) );
                aRecData->AppendString( recName );

                delete ( recName );
                recName = nil;
            }
            else
            {
                aRecData->AppendShort( ::strlen( "Record Name Unknown" ) );
                aRecData->AppendString( "Record Name Unknown" );
            }

            // need to calculate the number of attribute types ie. siValCnt
            // also need to extract the attributes and place them into fAttrData
            //siValCnt = 0;

            aAttrData->Clear();
            siResult = GetTheseAttributes( inAttrTypeList, result, inAttrOnly, inContext, siValCnt, aAttrData );
            if ( siResult != eDSNoErr ) throw( siResult );
			
            //add the attribute info to the aRecData
            if ( siValCnt == 0 )
            {
                // Attribute count
                aRecData->AppendShort( 0 );
            }
            else
            {
                // Attribute count
                aRecData->AppendShort( siValCnt );
                aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
            }

            // add the aRecData now to the inBuff
            siResult = inBuff->AddData( aRecData->GetData(), aRecData->GetLength() );

            // need to check if the buffer is full
			// need to handle full buffer and keep the result alive for the next call in
            if (siResult == CBuff::kBuffFull)
            {
                bufferFull = true;
				inContext->msgId = ldapMsgId;
                
                //save the result if buffer is full
                inContext->pResult = result;
            }
            else if ( siResult == eDSNoErr )
            {
                ldap_msgfree( result );
				result = nil;
				inContext->msgId = 0;
                
				outRecCount++; //added another record
				inContext->fTotalRecCount++;
				
                //make sure no result is carried in the context
                inContext->pResult = nil;

				//only get next result if buffer is not full
				if (searchTO == 0)
				{
					ldapReturnCode = ldap_result(inContext->fHost, ldapMsgId, 0, NULL, &result);
				}
				else
				{
					struct	timeval	tv;
					tv.tv_sec	= searchTO;
					tv.tv_usec	= 0;
					ldapReturnCode = ldap_result(inContext->fHost, ldapMsgId, 0, &tv, &result);
				}
            }
            else
            {
				inContext->msgId = 0;
                
                //make sure no result is carried in the context
                inContext->pResult = nil;
                throw( (sInt32)eDSInvalidBuffFormat);
            }
            
        } // while loop over entries

        // KW need to check the ldapReturnCode for posible errors ie. ldapMsgId was stale?
		//KW here we might have run into a dropped ldap server connection where we take what data we can get and then next
		//time in to this routine with context will report the error
		if (ldapReturnCode == LDAP_TIMEOUT)
		{
	     	siResult = eDSServerTimeout;
		}
		if ( (result != inContext->pResult) && (result != nil) )
		{
			ldap_msgfree( result );
			result = nil;
		}

    } // try block

    catch( sInt32 err )
    {
        siResult = err;
    }

	if ( queryFilter != nil )
	{
		delete( queryFilter );
		queryFilter = nil;
	}

	if (aAttrData != nil)
	{
		delete (aAttrData);
		aAttrData = nil;
	}
	if (aRecData != nil)
	{
		delete (aRecData);
		aRecData = nil;
	}
			
    return( siResult );

} // GetTheseRecords


//------------------------------------------------------------------------------------
//	* BuildLDAPQueryFilter
//------------------------------------------------------------------------------------

char *CLDAPPlugIn::BuildLDAPQueryFilter (	char			   *inConstAttrType,
											char			   *inConstAttrName,
											tDirPatternMatch	patternMatch,
											int	   				inConfigTableIndex,
											bool				useWellKnownRecType )
{
    char				   *queryFilter		= nil;
	unsigned long			matchType		= eDSExact;
	char				   *nativeRecType	= nil;
	uInt32					recTypeLen		= 0;
	uInt32					recNameLen		= 0;
	int						numAttributes	= 1;
	CFMutableStringRef		cfStringRef		= nil;
	char				   *escapedName		= nil;
	uInt32					escapedIndex	= 0;
	uInt32					originalIndex	= 0;
	bool					bOnceThru		= false;
	uInt32					offset			= 3;
	uInt32					callocLength	= 0;
	
	//first check to see if input not nil
	if (inConstAttrName != nil)
	{
		//KW assume that the queryfilter will be significantly less than 1024 characters
		recNameLen = strlen(inConstAttrName);
		escapedName = (char *)::calloc(1, 2 * recNameLen + 1);
		// assume at most all characters will be escaped
		while (originalIndex < recNameLen)
		{
			switch (inConstAttrName[originalIndex])
			{
				case '*':
				case '(':
				case ')':
					escapedName[escapedIndex] = '\\';
					++escapedIndex;
					// add \ escape character then fall through and pick up the original character
				default:
					escapedName[escapedIndex] = inConstAttrName[originalIndex];
					++escapedIndex;
					break;
			}
			++originalIndex;
		}
		
		//assume that the query is "OR" based ie. meet any of the criteria
		recTypeLen += 2;
		cfStringRef = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("(|"));
		
		//get the first mapping
		numAttributes = 1;
		//KW Note that kDS1RecordName is single valued so we are using kDSNAttrRecordName
		//as a multi-mapped std type which will easily lead to multiple values
		nativeRecType = MapAttrToLDAPType( inConstAttrType, inConfigTableIndex, numAttributes );
		//would throw if first nil since no more will be found otherwise proceed until nil
		//however simply set to default LDAP native in this case
                //ie. we are trying regardless if kDSNAttrRecordName is mapped or not
                //whether or not "cn" is a good choice is a different story
		if (nativeRecType == nil)
		{
			nativeRecType = new char[ 3 ];
			::strcpy(nativeRecType, "cn");
		}

		while ( nativeRecType != nil )
		{
			if (bOnceThru)
			{
				if (useWellKnownRecType)
				{
					//need to allow for condition that we want only a single query
					//that uses only a single native type - use the first one - 
					//for perhaps an open record or a write to a specific record
					bOnceThru = false;
					break;
				}
				offset = 0;
			}
			matchType = (unsigned long) (patternMatch);
			switch (matchType)
			{
		//		case eDSAnyMatch:
		//			cout << "Pattern match type of <eDSAnyMatch>" << endl;
		//			break;
		//		case eDSLocalNodeNames:
		//			cout << "Pattern match type of <eDSLocalNodeNames>" << endl;
		//			break;
		//		case eDSSearchNodeName:
		//			cout << "Pattern match type of <eDSSearchNodeName>" << endl;
		//			break;
				case eDSStartsWith:
				case eDSiStartsWith:
					CFStringAppendCString(cfStringRef,"(", kCFStringEncodingMacRoman);
					CFStringAppendCString(cfStringRef, nativeRecType, kCFStringEncodingMacRoman);
					CFStringAppendCString(cfStringRef,"=", kCFStringEncodingMacRoman);
					CFStringAppendCString(cfStringRef, escapedName, kCFStringEncodingMacRoman);
					CFStringAppendCString(cfStringRef,"*)", kCFStringEncodingMacRoman);
					bOnceThru = true;
					break;
				case eDSEndsWith:
				case eDSiEndsWith:
					CFStringAppendCString(cfStringRef,"(", kCFStringEncodingMacRoman);
					CFStringAppendCString(cfStringRef, nativeRecType, kCFStringEncodingMacRoman);
					CFStringAppendCString(cfStringRef,"=*", kCFStringEncodingMacRoman);
					CFStringAppendCString(cfStringRef, escapedName, kCFStringEncodingMacRoman);
					CFStringAppendCString(cfStringRef,")", kCFStringEncodingMacRoman);
					bOnceThru = true;
					break;
				case eDSContains:
				case eDSiContains:
					CFStringAppendCString(cfStringRef,"(", kCFStringEncodingMacRoman);
					CFStringAppendCString(cfStringRef, nativeRecType, kCFStringEncodingMacRoman);
					CFStringAppendCString(cfStringRef,"=*", kCFStringEncodingMacRoman);
					CFStringAppendCString(cfStringRef, escapedName, kCFStringEncodingMacRoman);
					CFStringAppendCString(cfStringRef,"*)", kCFStringEncodingMacRoman);
					bOnceThru = true;
					break;
				case eDSWildCardPattern:
				case eDSiWildCardPattern:
					//assume the inConstAttrName is wild
					CFStringAppendCString(cfStringRef,"(", kCFStringEncodingMacRoman);
					CFStringAppendCString(cfStringRef, nativeRecType, kCFStringEncodingMacRoman);
					CFStringAppendCString(cfStringRef,"=", kCFStringEncodingMacRoman);
					CFStringAppendCString(cfStringRef, inConstAttrName, kCFStringEncodingMacRoman);
					CFStringAppendCString(cfStringRef,")", kCFStringEncodingMacRoman);
					bOnceThru = true;
					break;
				case eDSRegularExpression:
				case eDSiRegularExpression:
					//assume inConstAttrName replaces entire wild expression
					CFStringAppendCString(cfStringRef, inConstAttrName, kCFStringEncodingMacRoman);
					bOnceThru = true;
					break;
				case eDSExact:
				case eDSiExact:
				default:
					CFStringAppendCString(cfStringRef,"(", kCFStringEncodingMacRoman);
					CFStringAppendCString(cfStringRef, nativeRecType, kCFStringEncodingMacRoman);
					CFStringAppendCString(cfStringRef,"=", kCFStringEncodingMacRoman);
					CFStringAppendCString(cfStringRef, escapedName, kCFStringEncodingMacRoman);
					CFStringAppendCString(cfStringRef,")", kCFStringEncodingMacRoman);
					bOnceThru = true;
					break;
			} // switch on matchType
				
			//cleanup nativeRecType if needed
			if (nativeRecType != nil)
			{
				delete (nativeRecType);
				nativeRecType = nil;
			}
			numAttributes++;
			//get the next mapping
            nativeRecType = MapAttrToLDAPType( inConstAttrType, inConfigTableIndex, numAttributes );
		} // while ( nativeRecType != nil )

		callocLength = CFStringGetLength(cfStringRef) + 2;  //+2 since possible ")" added as well as the NULL terminator
		queryFilter = (char *) calloc(1, callocLength);
		// building search like "sn=name"
		if (offset == 3)
		{
			CFRange	aRangeToDelete;
			aRangeToDelete.location = 1;
			aRangeToDelete.length = 2;			
			CFStringDelete(cfStringRef, aRangeToDelete);
			CFStringGetCString( cfStringRef, queryFilter, callocLength, kCFStringEncodingMacRoman );
		}
		//building search like "(|(sn=name)(cn=name))"
		else
		{
			CFStringAppendCString(cfStringRef,")", kCFStringEncodingMacRoman);
			CFStringGetCString( cfStringRef, queryFilter, callocLength, kCFStringEncodingMacRoman );
		}
		
		if (cfStringRef != nil)
		{
			CFRelease(cfStringRef);
			cfStringRef = nil;
		}
		
		if (escapedName != nil)
		{
			free(escapedName);
			escapedName = nil;
		}

    } // if inConstAttrName not nil

	return (queryFilter);
	
} // BuildLDAPQueryFilter

//------------------------------------------------------------------------------------
//	* GetDirNodeInfo
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::GetDirNodeInfo ( sGetDirNodeInfo *inData )
{
	sInt32				siResult		= eDSNoErr;
	uInt32				uiOffset		= 0;
	uInt32				uiCntr			= 1;
	uInt32				uiAttrCnt		= 0;
	CAttributeList	   *inAttrList		= nil;
	char			   *pAttrName		= nil;
	char			   *pData			= nil;
	sLDAPContextData	   *pContext		= nil;
	sLDAPContextData	   *pAttrContext	= nil;
	CBuff				outBuff;
	CDataBuff		   *aRecData		= nil;
	CDataBuff		   *aAttrData		= nil;
	CDataBuff		   *aTmpData		= nil;
//	sLDAPConfigData	   *pConfig			= nil;

// Can extract here the following:
// kDSAttributesAll
// kDSNAttrNodePath
// kDS1AttrReadOnlyNode
// kDSNAttrAuthMethod
// dsAttrTypeStandard:AcountName
//KW need to add mappings info next

	try
	{
		if ( inData  == nil ) throw( (sInt32)eMemoryError );

		pContext = (sLDAPContextData *)gRefTable->GetItemData( inData->fInNodeRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );

		inAttrList = new CAttributeList( inData->fInDirNodeInfoTypeList );
		if ( inAttrList == nil ) throw( (sInt32)eDSNullNodeInfoTypeList );
		if (inAttrList->GetCount() == 0) throw( (sInt32)eDSEmptyNodeInfoTypeList );

		siResult = outBuff.Initialize( inData->fOutDataBuff, true );
		if ( siResult != eDSNoErr ) throw( siResult );

		siResult = outBuff.SetBuffType( 'Gdni' );  //can't use 'StdB' since a tRecordEntry is not returned
		if ( siResult != eDSNoErr ) throw( siResult );

		aRecData = new CDataBuff();
		if ( aRecData  == nil ) throw( (sInt32)eMemoryError );
		aAttrData = new CDataBuff();
		if ( aAttrData  == nil ) throw( (sInt32)eMemoryError );
		aTmpData = new CDataBuff();
		if ( aTmpData  == nil ) throw( (sInt32)eMemoryError );

		// Set the record name and type
		aRecData->AppendShort( ::strlen( "dsAttrTypeStandard:DirectoryNodeInfo" ) );
		aRecData->AppendString( (char *)"dsAttrTypeStandard:DirectoryNodeInfo" );
		aRecData->AppendShort( ::strlen( "DirectoryNodeInfo" ) );
		aRecData->AppendString( (char *)"DirectoryNodeInfo" );

		while ( inAttrList->GetAttribute( uiCntr++, &pAttrName ) == eDSNoErr )
		{
			//package up all the dir node attributes dependant upon what was asked for
			if ((::strcmp( pAttrName, kDSAttributesAll ) == 0) ||
				(::strcmp( pAttrName, kDSNAttrNodePath ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDSNAttrNodePath ) );
				aTmpData->AppendString( kDSNAttrNodePath );

				if ( inData->fInAttrInfoOnly == false )
				{
					// Attribute value count always two
					aTmpData->AppendShort( 2 );
					
					// Append attribute value
					aTmpData->AppendLong( ::strlen( "LDAPv2" ) );
					aTmpData->AppendString( (char *)"LDAPv2" );

					char *tmpStr = nil;
					//don't need to retrieve for the case of "generic unknown" so don't check index 0
					// simply always use the pContext->fName since case of registered it is identical to
					// pConfig->fServerName and in the case of generic it will be correct for what was actually opened
					/*
					if (( pContext->fConfigTableIndex < gConfigTableLen) && ( pContext->fConfigTableIndex >= 1 ))
					{

				        pConfig = (sLDAPConfigData *)gConfigTable->GetItemData( pContext->fConfigTableIndex );
				        if (pConfig != nil)
				        {
				        	if (pConfig->fServerName != nil)
				        	{
				        		tmpStr = new char[1+::strlen(pConfig->fServerName)];
				        		::strcpy( tmpStr, pConfig->fServerName );
			        		}
		        		}
	        		}
					*/
					if (pContext->fName != nil)
					{
						tmpStr = new char[1+::strlen(pContext->fName)];
						::strcpy( tmpStr, pContext->fName );
					}
					else
					{
						tmpStr = new char[1+::strlen("Unknown Node Location")];
						::strcpy( tmpStr, "Unknown Node Location" );
					}
					
					// Append attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );

				} // fInAttrInfoOnly is false
				
				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
			} // kDSAttributesAll or kDSNAttrNodePath
			
			if ( (::strcmp( pAttrName, kDSAttributesAll ) == 0) || 
				 (::strcmp( pAttrName, kDS1AttrReadOnlyNode ) == 0) )
			{
				aTmpData->Clear();

				uiAttrCnt++;

				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDS1AttrReadOnlyNode ) );
				aTmpData->AppendString( kDS1AttrReadOnlyNode );

				if ( inData->fInAttrInfoOnly == false )
				{
					// Attribute value count
					aTmpData->AppendShort( 1 );

					//possible for a node to be ReadOnly, ReadWrite, WriteOnly
					//note that ReadWrite does not imply fully readable or writable
					
					// Add the root node as an attribute value
					aTmpData->AppendLong( ::strlen( "ReadOnly" ) );
					aTmpData->AppendString( "ReadOnly" );

				}
				// Add the attribute length and data
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );

				// Clear the temp block
				aTmpData->Clear();
			}
				 
			if ((::strcmp( pAttrName, kDSAttributesAll ) == 0) ||
				(::strcmp( pAttrName, kDSNAttrAuthMethod ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( kDSNAttrAuthMethod ) );
				aTmpData->AppendString( kDSNAttrAuthMethod );

				if ( inData->fInAttrInfoOnly == false )
				{
					char *tmpStr = nil;
					
					// Attribute value count
					aTmpData->AppendShort( 4 );
					
					tmpStr = new char[1+::strlen( kDSStdAuthCrypt )];
					::strcpy( tmpStr, kDSStdAuthCrypt );
					
					// Append first attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );
					tmpStr = nil;

					tmpStr = new char[1+::strlen( kDSStdAuthClearText )];
					::strcpy( tmpStr, kDSStdAuthClearText );
					
					// Append second attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );
					tmpStr = nil;
                                        
					tmpStr = new char[1+::strlen( kDSStdAuthNodeNativeClearTextOK )];
					::strcpy( tmpStr, kDSStdAuthNodeNativeClearTextOK );
					
					// Append third attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );
					tmpStr = nil;
					
					tmpStr = new char[1+::strlen( kDSStdAuthNodeNativeNoClearText )];
					::strcpy( tmpStr, kDSStdAuthNodeNativeNoClearText );
					
					// Append fourth attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );
					tmpStr = nil;
				} // fInAttrInfoOnly is false

				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
			
			} // kDSAttributesAll or kDSNAttrAuthMethod

			if ((::strcmp( pAttrName, kDSAttributesAll ) == 0) ||
				(::strcmp( pAttrName, "dsAttrTypeStandard:AccountName" ) == 0) )
			{
				aTmpData->Clear();
				
				uiAttrCnt++;
			
				// Append the attribute name
				aTmpData->AppendShort( ::strlen( "dsAttrTypeStandard:AccountName" ) );
				aTmpData->AppendString( (char *)"dsAttrTypeStandard:AccountName" );

				if ( inData->fInAttrInfoOnly == false )
				{
					char *tmpStr = nil;
		        	if (pContext->authCallActive)
		        	{
		        		tmpStr = new char[1+::strlen(pContext->authAccountName)];
		        		::strcpy( tmpStr, pContext->authAccountName );
	        		}

					if (tmpStr == nil)
					{
						tmpStr = new char[1+::strlen("No Account Name")];
						::strcpy( tmpStr, "No Account Name" );
					}
					
					// Attribute value count
					aTmpData->AppendShort( 1 );
					
					// Append attribute value
					aTmpData->AppendLong( ::strlen( tmpStr ) );
					aTmpData->AppendString( tmpStr );

					delete( tmpStr );

				} // fInAttrInfoOnly is false
				
				// Add the attribute length
				aAttrData->AppendLong( aTmpData->GetLength() );
				aAttrData->AppendBlock( aTmpData->GetData(), aTmpData->GetLength() );
                aTmpData->Clear();
			
			} // kDSAttributesAll or dsAttrTypeStandard:AccountName

		} // while

		aRecData->AppendShort( uiAttrCnt );
		if (uiAttrCnt > 0)
		{
			aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
		}

		outBuff.AddData( aRecData->GetData(), aRecData->GetLength() );
		inData->fOutAttrInfoCount = uiAttrCnt;

		pData = outBuff.GetDataBlock( 1, &uiOffset );
		if ( pData != nil )
		{
			pAttrContext = MakeContextData();
			if ( pAttrContext  == nil ) throw( (sInt32)eMemoryAllocError );
			
			pAttrContext->fConfigTableIndex = pContext->fConfigTableIndex;

		//add to the offset for the attr list the length of the GetDirNodeInfo fixed record labels
//		record length = 4
//		aRecData->AppendShort( ::strlen( "dsAttrTypeStandard:DirectoryNodeInfo" ) ); = 2
//		aRecData->AppendString( "dsAttrTypeStandard:DirectoryNodeInfo" ); = 36
//		aRecData->AppendShort( ::strlen( "DirectoryNodeInfo" ) ); = 2
//		aRecData->AppendString( "DirectoryNodeInfo" ); = 17
//		total adjustment = 4 + 2 + 36 + 2 + 17 = 61

			pAttrContext->offset = uiOffset + 61;

			gRefTable->AddItem( inData->fOutAttrListRef, pAttrContext );
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if ( inAttrList != nil )
	{
		delete( inAttrList );
		inAttrList = nil;
	}
	if ( aRecData != nil )
	{
		delete( aRecData );
		aRecData = nil;
	}
	if ( aAttrData != nil )
	{
		delete( aAttrData );
		aAttrData = nil;
	}
	if ( aTmpData != nil )
	{
		delete( aTmpData );
		aTmpData = nil;
	}

	return( siResult );

} // GetDirNodeInfo

//------------------------------------------------------------------------------------
//	* OpenRecord
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::OpenRecord ( sOpenRecord *inData )
{
	sInt32				siResult		= eDSNoErr;
	tDataNodePtr		pRecName		= nil;
	tDataNodePtr		pRecType		= nil;
	char			   *pLDAPRecType	= nil;
	sLDAPContextData	   *pContext		= nil;
	sLDAPContextData	   *pRecContext		= nil;
    int					ldapMsgId		= 0;
    char			   *queryFilter		= nil;
	LDAPMessage		   *result			= nil;
	int					ldapReturnCode	= 0;
	int					numRecTypes		= 1;
	bool				bResultFound	= false;
    sLDAPConfigData	   *pConfig			= nil;
    int					searchTO   		= 0;


	try
	{
		pContext = (sLDAPContextData *)gRefTable->GetItemData( inData->fInNodeRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );

		pRecType = inData->fInRecType;
		if ( pRecType  == nil ) throw( (sInt32)eDSNullRecType );

		pRecName = inData->fInRecName;
		if ( pRecName  == nil ) throw( (sInt32)eDSNullRecName );

		if (pRecName->fBufferLength == 0) throw( (sInt32)eDSEmptyRecordNameList );
		if (pRecType->fBufferLength == 0) throw( (sInt32)eDSEmptyRecordTypeList );

		//build the record query string
		//removed the use well known map only condition ie. true to false
		queryFilter = BuildLDAPQueryFilter((char *)kDSNAttrRecordName, pRecName->fBufferData, eDSExact, pContext->fConfigTableIndex, false);
    	if ( queryFilter == nil ) throw( (sInt32)eDSNullParameter );

		//search for the specific LDAP record now
		
		//retrieve the config data
		//don't need to retrieve for the case of "generic unknown" so don't check index 0
		if (( pContext->fConfigTableIndex < gConfigTableLen) && ( pContext->fConfigTableIndex >= 1 ))
		{
			pConfig = (sLDAPConfigData *)gConfigTable->GetItemData( pContext->fConfigTableIndex );
			if (pConfig != nil)
			{
				searchTO	= pConfig->fSearchTimeout;
			}
		}
		
        // Here is the bind to the LDAP server
		siResult = RebindTryProc(pContext);
		if ( siResult != eDSNoErr ) throw( siResult );

        // we will search over all the rectype mappings until we find the first
        // result for the search criteria in the queryfilter
        numRecTypes = 1;
		pLDAPRecType = MapRecToLDAPType( pRecType->fBufferData, pContext->fConfigTableIndex, numRecTypes );
		//only throw this for first time since we need at least one map
		if ( pLDAPRecType == nil ) throw( (sInt32)eDSInvalidRecordType );
		
		while ( (pLDAPRecType != nil) && (!bResultFound) )
		{

	        //here is the call to the LDAP server asynchronously which requires
	        // host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
	        // attribute list (NULL for all), return attrs values flag
	        // Note: asynchronous call is made so that a MsgId can be used for future calls
	        // This returns us the message ID which is used to query the server for the results
	        if ( (ldapMsgId = ldap_search( pContext->fHost, pLDAPRecType, LDAP_SCOPE_SUBTREE, queryFilter, NULL, 0) ) == -1 )
	        {
	        	bResultFound = false;
	        }
	        else
	        {
	        	bResultFound = true;
				//retrieve the actual LDAP record data for use internally
				//useful only from the read-only perspective
				//KW when write capability is added, we will need to re-read the result after a write
				if (searchTO == 0)
				{
					ldapReturnCode = ldap_result(pContext->fHost, ldapMsgId, 0, NULL, &result);
				}
				else
				{
					struct	timeval	tv;
					tv.tv_sec	= searchTO;
					tv.tv_usec	= 0;
					ldapReturnCode = ldap_result(pContext->fHost, ldapMsgId, 0, &tv, &result);
				}
	        }
		
			if (pLDAPRecType != nil)
			{
				delete (pLDAPRecType);
				pLDAPRecType = nil;
			}
			numRecTypes++;
			pLDAPRecType = MapRecToLDAPType( pRecType->fBufferData, pContext->fConfigTableIndex, numRecTypes );
		} // while ( (pLDAPRecType != nil) && (!bResultFound) )

		if ( (bResultFound) && ( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
		{
		
			pRecContext = MakeContextData();
			if ( pRecContext  == nil ) throw( (sInt32)eMemoryAllocError );

	        pRecContext->pResult = result;
	        
			if (pContext->fName != nil)
			{
				pRecContext->fName = new char[1+::strlen(pContext->fName)];
				::strcpy( pRecContext->fName, pContext->fName );
			}
			pRecContext->fType = 2;
	        pRecContext->msgId = ldapMsgId;
	        pRecContext->fHost = pContext->fHost;
	        pRecContext->fConfigTableIndex = pContext->fConfigTableIndex;
			if (pRecType->fBufferData != nil)
			{
				pRecContext->fOpenRecordType = new char[1+::strlen(pRecType->fBufferData)];
				::strcpy( pRecContext->fOpenRecordType, pRecType->fBufferData );
			}
			if (pRecName->fBufferData != nil)
			{
				pRecContext->fOpenRecordName = new char[1+::strlen(pRecName->fBufferData)];
				::strcpy( pRecContext->fOpenRecordName, pRecName->fBufferData );
			}
	        
			gRefTable->AddItem( inData->fOutRecRef, pRecContext );
		} // if bResultFound and ldapReturnCode okay
		else if (ldapReturnCode == LDAP_TIMEOUT)
		{
	     	siResult = eDSServerTimeout;
			if ( result != nil )
			{
				ldap_msgfree( result );
				result = nil;
			}
		}
		else
		{
	     	siResult = eDSRecordNotFound;
			if ( result != nil )
			{
				ldap_msgfree( result );
				result = nil;
			}
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}


	if ( pLDAPRecType != nil )
	{
		delete( pLDAPRecType );
		pLDAPRecType = nil;
	}

	if ( queryFilter != nil )
	{
		delete( queryFilter );
		queryFilter = nil;
	}

	return( siResult );

} // OpenRecord

//------------------------------------------------------------------------------------
//	* CloseRecord
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::CloseRecord ( sCloseRecord *inData )
{
	sInt32			siResult	=	eDSNoErr;
	sLDAPContextData	*pContext	=	nil;

	try
	{
		pContext = (sLDAPContextData *)gRefTable->GetItemData( inData->fInRecRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );
		
		gRefTable->RemoveItem( inData->fInRecRef );
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // CloseRecord


//------------------------------------------------------------------------------------
//	* CloseAttributeList
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::CloseAttributeList ( sCloseAttributeList *inData )
{
	sInt32				siResult		= eDSNoErr;
	sLDAPContextData	   *pContext		= nil;

	pContext = (sLDAPContextData *)gRefTable->GetItemData( inData->fInAttributeListRef );
	if ( pContext != nil )
	{
		//only "offset" should have been used in the Context
		gRefTable->RemoveItem( inData->fInAttributeListRef );
	}
	else
	{
		siResult = eDSInvalidAttrListRef;
	}

	return( siResult );

} // CloseAttributeList


//------------------------------------------------------------------------------------
//	* CloseAttributeValueList
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::CloseAttributeValueList ( sCloseAttributeValueList *inData )
{
	sInt32				siResult		= eDSNoErr;
	sLDAPContextData	   *pContext		= nil;

	pContext = (sLDAPContextData *)gRefTable->GetItemData( inData->fInAttributeValueListRef );
	if ( pContext != nil )
	{
		//only "offset" should have been used in the Context
		gRefTable->RemoveItem( inData->fInAttributeValueListRef );
	}
	else
	{
		siResult = eDSInvalidAttrValueRef;
	}

	return( siResult );

} // CloseAttributeValueList


//------------------------------------------------------------------------------------
//	* GetRecRefInfo
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::GetRecRefInfo ( sGetRecRefInfo *inData )
{
	sInt32			siResult	= eDSNoErr;
	uInt32			uiRecSize	= 0;
	tRecordEntry   *pRecEntry	= nil;
	sLDAPContextData   *pContext	= nil;
	char		   *refType		= nil;
    uInt32			uiOffset	= 0;
    char		   *refName		= nil;

	try
	{
		pContext = (sLDAPContextData *)gRefTable->GetItemData( inData->fInRecRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );

		//place in the record type from the context data of an OpenRecord
		if ( pContext->fOpenRecordType != nil)
		{
			refType = new char[1+::strlen(pContext->fOpenRecordType)];
			::strcpy( refType, pContext->fOpenRecordType );
		}
		else //assume Record type of "Record Type Unknown"
		{
			refType = new char[1+::strlen("Record Type Unknown")];
			::strcpy( refType, "Record Type Unknown" );
		}
		
		//place in the record name from the context data of an OpenRecord
		if ( pContext->fOpenRecordName != nil)
		{
			refName = new char[1+::strlen(pContext->fOpenRecordName)];
			::strcpy( refName, pContext->fOpenRecordName );
		}
		else //assume Record name of "Record Name Unknown"
		{
			refName = new char[1+::strlen("Record Name Unknown")];
			::strcpy( refName, "Record Name Unknown" );
		}
		
		uiRecSize = sizeof( tRecordEntry ) + ::strlen( refType ) + ::strlen( refName ) + 4 + kBuffPad;
		pRecEntry = (tRecordEntry *)::calloc( 1, uiRecSize );
		
		pRecEntry->fRecordNameAndType.fBufferSize	= ::strlen( refType ) + ::strlen( refName ) + 4 + kBuffPad;
		pRecEntry->fRecordNameAndType.fBufferLength	= ::strlen( refType ) + ::strlen( refName ) + 4;
		
		uiOffset = 0;
		uInt16 strLen = 0;
		// Add the record name length and name itself
		strLen = ::strlen( refName );
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, &strLen, 2);
		uiOffset += 2;
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, refName, strLen);
		uiOffset += strLen;
		
		// Add the record type length and type itself
		strLen = ::strlen( refType );
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, &strLen, 2);
		uiOffset += 2;
		::memcpy( pRecEntry->fRecordNameAndType.fBufferData + uiOffset, refType, strLen);
		uiOffset += strLen;

		inData->fOutRecInfo = pRecEntry;
	}

	catch( sInt32 err )
	{
		siResult = err;
	}
		
	if (refType != nil)
	{
		delete( refType );
		refType = nil;
	}
	if (refName != nil)
	{
		delete( refName );
		refName = nil;
	}

	return( siResult );

} // GetRecRefInfo


//------------------------------------------------------------------------------------
//	* GetRecAttribInfo
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::GetRecAttribInfo ( sGetRecAttribInfo *inData )
{
	sInt32					siResult		= eDSNoErr;
	uInt32					uiTypeLen		= 0;
	uInt32					uiDataLen		= 0;
	tDataNodePtr			pAttrType		= nil;
	char				   *pLDAPAttrType	= nil;
	tAttributeEntryPtr		pOutAttrEntry	= nil;
	sLDAPContextData		   *pContext		= nil;
	LDAPMessage			   *result			= nil;
	BerElement			   *ber;
	struct berval		  **bValues;
	char				   *pAttr			= nil;
	int						numAttributes	= 1;
	bool					bTypeFound		= false;
	int						valCount		= 0;
	
	try
	{
		pContext = (sLDAPContextData *)gRefTable->GetItemData( inData->fInRecRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );

		pAttrType = inData->fInAttrType;
		if ( pAttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );

		//use the context data Result to get the correct LDAP record entries
		result = pContext->pResult;
        if ( result != nil )
        {

			//get the first mapping
			numAttributes = 1;
			pLDAPAttrType = MapAttrToLDAPType( pAttrType->fBufferData, pContext->fConfigTableIndex, numAttributes );
			//throw if first nil since no more will be found otherwise proceed until nil
			if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

			//set indicator of multiple native to standard mappings
			bTypeFound = false;
			while ( pLDAPAttrType != nil )
			{

	        	//cycle through the attributes until we find the correct one
				for (	pAttr = ldap_first_attribute (pContext->fHost, result, &ber );
						pAttr != NULL; pAttr = ldap_next_attribute(pContext->fHost, result, ber ) )
				{
					if (::strcmp( pAttr, pLDAPAttrType ) == 0) // found at least the first one
					{
						if (!bTypeFound)
						{
							//set up the length of the attribute type
							uiTypeLen = ::strlen( pAttrType->fBufferData );
							pOutAttrEntry = (tAttributeEntry *)::calloc( 1, sizeof( tAttributeEntry ) + uiTypeLen + kBuffPad );
							pOutAttrEntry->fAttributeSignature.fBufferSize		= uiTypeLen;
							pOutAttrEntry->fAttributeSignature.fBufferLength	= uiTypeLen;
							::memcpy( pOutAttrEntry->fAttributeSignature.fBufferData, pAttrType->fBufferData, uiTypeLen ); 
							bTypeFound = true;
							valCount = 0;
							uiDataLen = 0;
						}
						if (( bValues = ldap_get_values_len (pContext->fHost, result, pAttr )) != NULL)
						{
						
							// calculate the number of values for this attribute
							for (int ii = 0; bValues[ii] != NULL; ii++ )
							   valCount++;
							   
							// for each value of the attribute
							for (int i = 0; bValues[i] != NULL; i++ )
							{
								// Append attribute value
								uiDataLen += bValues[i]->bv_len;
							} // for each bValues[i]
							ldap_value_free_len(bValues);
						} // if ( inAttrOnly == false ) && bValues = ldap_get_values_len ...
						
						if (pAttr != nil)
						{
							ldap_memfree( pAttr );
						}
						break;  // don't continue in for loop since we found one
						
					} // if (::strcmp( pAttr, pLDAPAttrType ) == 0)
						
					if (pAttr != nil)
					{
						ldap_memfree( pAttr );
					}
				} // for ( loop over ldap_next_attribute )
				
				if (ber != nil)
				{
					ber_free( ber, 0 );
				}

				//cleanup pLDAPAttrType if needed
				if (pLDAPAttrType != nil)
				{
					delete (pLDAPAttrType);
					pLDAPAttrType = nil;
				}
				numAttributes++;
				//get the next mapping
				pLDAPAttrType = MapAttrToLDAPType( pAttrType->fBufferData, pContext->fConfigTableIndex, numAttributes );
			} // while ( pLDAPAttrType != nil )

			if ( pOutAttrEntry == nil )
			{
				inData->fOutAttrInfoPtr = nil;
				throw( (sInt32)eDSAttributeNotFound);
			}
			// Number of attribute values
			pOutAttrEntry->fAttributeValueCount = valCount;
			//KW seems arbitrary max length
			pOutAttrEntry->fAttributeValueMaxSize = 255;
			//set the total length of all the attribute data
			pOutAttrEntry->fAttributeDataSize = uiDataLen;
			//assign the result out
			inData->fOutAttrInfoPtr = pOutAttrEntry;
	
        } // retrieve the result from the LDAP server
        else
        {
        	throw( (sInt32)eDSRecordNotFound); //KW???
        }
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if (pLDAPAttrType != nil)
	{
		delete( pLDAPAttrType );
		pLDAPAttrType = nil;
	}				

	return( siResult );

} // GetRecAttribInfo


//------------------------------------------------------------------------------------
//	* GetRecAttrValueByIndex
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::GetRecAttrValueByIndex ( sGetRecordAttributeValueByIndex *inData )
{
	sInt32					siResult		= eDSNoErr;
	uInt32					uiDataLen		= 0;
	tDataNodePtr			pAttrType		= nil;
	char				   *pLDAPAttrType	= nil;
	tAttributeValueEntryPtr	pOutAttrValue	= nil;
	sLDAPContextData		   *pContext		= nil;
	LDAPMessage			   *result			= nil;
	BerElement			   *ber;
	struct berval		  **bValues;
	char				   *pAttr			= nil;
	char				   *pAttrVal		= nil;
	unsigned long			valCount		= 0;
	bool					bFoundIt		= false;
	int						numAttributes	= 1;

	try
	{
		pContext = (sLDAPContextData *)gRefTable->GetItemData( inData->fInRecRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );

		pAttrType = inData->fInAttrType;
		if ( pAttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );

		//use the context data Result to get the correct LDAP record entries
		result = pContext->pResult;
        if ( result != nil )
        {
        
			//get the first mapping
			numAttributes = 1;
			pLDAPAttrType = MapAttrToLDAPType( pAttrType->fBufferData, pContext->fConfigTableIndex, numAttributes );
			//throw if first nil since no more will be found otherwise proceed until nil
			if ( pLDAPAttrType == nil ) throw( (sInt32)eDSInvalidAttributeType );  //KW would like a eDSNoMappingAvailable

			//set indicator of multiple native to standard mappings
			bFoundIt = false;
			while ( ( pLDAPAttrType != nil ) && (!bFoundIt) )
			{
	        	//cycle through the attributes until we find the correct one
				for (	pAttr = ldap_first_attribute (pContext->fHost, result, &ber );
						pAttr != NULL; pAttr = ldap_next_attribute(pContext->fHost, result, ber ) )
				{
					if (::strcmp( pAttr, pLDAPAttrType ) == 0) // found it
					{
						if (( bValues = ldap_get_values_len (pContext->fHost, result, pAttr )) != NULL)
						{
						
							// for each value of the attribute
							for (int i = 0; bValues[i] != NULL; i++ )
							{
								valCount++;
								if (valCount == inData->fInAttrValueIndex)
								{
									// Append attribute value
									uiDataLen = bValues[i]->bv_len;
									pAttrVal = new char[uiDataLen+1];
									::strcpy(pAttrVal,bValues[i]->bv_val);
									
									pOutAttrValue = (tAttributeValueEntry *)::calloc( 1, sizeof( tAttributeValueEntry ) + uiDataLen + kBuffPad );
									pOutAttrValue->fAttributeValueData.fBufferSize = uiDataLen;
									pOutAttrValue->fAttributeValueData.fBufferLength = uiDataLen;
									if ( pAttrVal != nil )
									{
										pOutAttrValue->fAttributeValueID = CalcCRC( pAttrVal );
										::memcpy( pOutAttrValue->fAttributeValueData.fBufferData, pAttrVal, uiDataLen );
									}

									bFoundIt = true;
									break;
								} // if valCount correct one
							} // for each bValues[i]
							ldap_value_free_len(bValues);
						} // if bValues = ldap_get_values_len ...
						
					} // if (::strcmp( pAttr, pLDAPAttrType ) == 0)
						
					if (bFoundIt)
					{
						inData->fOutEntryPtr = pOutAttrValue;				
						if (pAttr != nil)
						{
							ldap_memfree( pAttr );
						}
						break;  // don't continue in for loop since we found one
					}
						
					if (pAttr != nil)
					{
						ldap_memfree( pAttr );
					}
				} // for ( loop over ldap_next_attribute )
								
				if (ber != nil)
				{
					ber_free( ber, 0 );
				}

				//cleanup pLDAPAttrType if needed
				if (pLDAPAttrType != nil)
				{
					delete (pLDAPAttrType);
					pLDAPAttrType = nil;
				}
				numAttributes++;
				//get the next mapping
				pLDAPAttrType = MapAttrToLDAPType( pAttrType->fBufferData, pContext->fConfigTableIndex, numAttributes );
			} // while ( pLDAPAttrType != nil )
			
	        if (!bFoundIt)
	        {
				if (valCount < inData->fInAttrValueIndex)
				{
					siResult = eDSIndexOutOfRange;
				}
				else
				{
					siResult = eDSAttributeNotFound;
				}
	        }

        } // retrieve the result from the LDAP server
        else
        {
        	throw( (sInt32)eDSRecordNotFound); //KW???
        }

		if (pAttrVal != nil)
		{
			delete( pAttrVal );
			pAttrVal = nil;
		}
		
		if (pLDAPAttrType != nil)
		{
			delete( pLDAPAttrType );
			pLDAPAttrType = nil;
		}				
			
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // GetRecAttrValueByIndex


// ---------------------------------------------------------------------------
//	* GetNextStdAttrType
// ---------------------------------------------------------------------------

char* CLDAPPlugIn::GetNextStdAttrType ( uInt32 inConfigTableIndex, int inIndex )
{
	char				   *outResult	= nil;
	sLDAPConfigData		   *pConfig		= nil;
	sMapTuple			   *pMapTuple	= nil;
	int						countStd	= 0;
	bool					foundStd	= false;

	//idea here is to use the inIndex to request a specific std Attr Type
	//if inIndex is 1 then the first std attr type will be returned
	//if inIndex is >= 1 and <= totalCount then that std attr type will be returned
	//if inIndex is <= 0 then nil will be returned
	//if inIndex is > totalCount nil will be returned
	//note the inIndex will reference the inIndexth entry ie. start at 1
	//caller can increment the inIndex starting at one and get std attr types until nil is returned

	if (inIndex > 0)
	{
		//if no std attr type is found then NIL will be returned
		
		//find the attr map that we need using inConfigTableIndex
		if (( inConfigTableIndex < gConfigTableLen) && ( inConfigTableIndex >= 0 ))
		{
	        pConfig = (sLDAPConfigData *)gConfigTable->GetItemData( inConfigTableIndex );
	        if (pConfig != nil)
	        {
	        	pMapTuple = pConfig->pAttributeMapTuple;
        	}
		}
		
		//else we use the standard mappings
		if ( pMapTuple == nil )
		{
			pMapTuple = pStdAttributeMapTuple;
		}
		
		//go get the mappings to retrieve the std attr types that are mapped here
		if ( pMapTuple != nil )
		{
			countStd = 0;
        	while ((pMapTuple != nil) && !(foundStd))
        	{
        		if (pMapTuple->fStandard != nil)
        		{
    				countStd++;
    				if (inIndex == countStd)
    				{
						outResult = new char[1+::strlen( pMapTuple->fStandard )];
						::strcpy( outResult, pMapTuple->fStandard );
						foundStd = true;
    				}
        		} // (pMapTuple->fStandard != nil)
				pMapTuple = pMapTuple->pNext;
    		}//loop through the std map tuples
		}// pMapTuple != nil

	}// if (inIndex > 0)

	return( outResult );

} // GetNextStdAttrType


//------------------------------------------------------------------------------------
//	* DoAttributeValueSearch
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::DoAttributeValueSearch ( sDoAttrValueSearchWithData *inData )
{
    sInt32					siResult			= eDSNoErr;
    bool					bAttribOnly			= false;
    uInt32					uiCount				= 0;
    uInt32					uiTotal				= 0;
    CAttributeList		   *cpRecTypeList		= nil;
    CAttributeList		   *cpAttrTypeList 		= nil;
    char				   *pSearchStr			= nil;
	char				   *pRecType			= nil;
    char				   *pAttrType			= nil;
    char				   *pLDAPRecType		= nil;
    tDirPatternMatch		pattMatch			= eDSExact;
    sLDAPContextData		   *pContext			= nil;
    CBuff				   *outBuff				= nil;
	tDataList			   *pTmpDataList	= nil;
    int						numRecTypes			= 1;
    bool					bBuffFull			= false;
    bool					separateRecTypes	= false;
    uInt32					countDownRecTypes	= 0;

    try
    {
        // Verify all the parameters
        if ( inData  == nil ) throw( (sInt32)eMemoryError );
        if ( inData->fOutDataBuff  == nil ) throw( (sInt32)eDSEmptyBuffer );
        if (inData->fOutDataBuff->fBufferSize == 0) throw( (sInt32)eDSEmptyBuffer );

        if ( inData->fInRecTypeList  == nil ) throw( (sInt32)eDSEmptyRecordTypeList );
//depends on call in        if ( inData->fInAttribTypeList  == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );

        // Node context data
        pContext = (sLDAPContextData *)gRefTable->GetItemData( inData->fInNodeRef );
        if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );

        // check to make sure context IN is the same as RefTable saved context
        if ( inData->fIOContinueData != nil )
        {
            if ( inData->fIOContinueData != pContext )
            {
                throw( (sInt32)eDSInvalidContext );
            }
        }
        else
        {
            //parameters used for data buffering
//might be needed?            pContext->fRecNameIndex = 1;
            pContext->fRecTypeIndex = 1;
            pContext->fAttrIndex = 1;
            pContext->fTotalRecCount = 0;
            pContext->fLimitRecSearch = 0;
			//check if the client has requested a limit on the number of records to return
			//we only do this the first call into this context for pContext
			if (inData->fOutMatchRecordCount >= 0)
			{
				pContext->fLimitRecSearch = inData->fOutMatchRecordCount;
			}
        }

        // start with the continue set to nil until buffer gets full and there is more data
        //OR we have more record types to look through
        inData->fIOContinueData			= nil;
		//return zero here if nothing found
		inData->fOutMatchRecordCount	= 0;

        // copy the buffer data into a more manageable form
        outBuff = new CBuff();
        if ( outBuff  == nil ) throw( (sInt32)eMemoryError );

        siResult = outBuff->Initialize( inData->fOutDataBuff, true );
        if ( siResult != eDSNoErr ) throw( siResult );

        siResult = outBuff->GetBuffStatus();
        if ( siResult != eDSNoErr ) throw( siResult );

        siResult = outBuff->SetBuffType( 'StdA' );
        if ( siResult != eDSNoErr ) throw( siResult );

        // Get the record type list
        // Record type mapping for LDAP to DS API is dealt with below
        cpRecTypeList = new CAttributeList( inData->fInRecTypeList );
        if ( cpRecTypeList  == nil ) throw( (sInt32)eDSEmptyRecordTypeList );
        //save the number of rec types here to use in separating the buffer data
        countDownRecTypes = cpRecTypeList->GetCount() - pContext->fRecTypeIndex + 1;
        if (cpRecTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyRecordTypeList );

        // Get the attribute type
		pAttrType = inData->fInAttrType->fBufferData;
		if ( pAttrType == nil ) throw( (sInt32)eDSEmptyAttributeType );

        // Get the attribute string match
		pSearchStr = inData->fInPatt2Match->fBufferData;
		if ( pSearchStr == nil ) throw( (sInt32)eDSEmptyPattern2Match );

        // Get the attribute pattern match type
        pattMatch = inData->fInPattMatchType;

		if ( inData->fType == kDoAttributeValueSearchWithData )
		{
			// Get the attribute list
			cpAttrTypeList = new CAttributeList( inData->fInAttrTypeRequestList );
			if ( cpAttrTypeList == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );
			if (cpAttrTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyAttributeTypeList );

			// Get the attribute info only flag
			bAttribOnly = inData->fInAttrInfoOnly;
		}
		else
		{
			pTmpDataList = dsBuildListFromStringsPriv( kDSAttributesAll, nil );
			if ( pTmpDataList != nil )
			{
				cpAttrTypeList = new CAttributeList( pTmpDataList );
				if ( cpAttrTypeList == nil ) throw( (sInt32)eDSEmptyAttributeTypeList );
				if (cpAttrTypeList->GetCount() == 0) throw( (sInt32)eDSEmptyAttributeTypeList );
			}
		}

        // get records of these types
        while ((( cpRecTypeList->GetAttribute( pContext->fRecTypeIndex, &pRecType ) == eDSNoErr ) && (!bBuffFull)) && (!separateRecTypes))
        {
        	//mapping rec types - if std to native
        	numRecTypes = 1;
            pLDAPRecType = MapRecToLDAPType( pRecType, pContext->fConfigTableIndex, numRecTypes );
            //throw on first nil
            if ( pLDAPRecType == nil ) throw( (sInt32)eDSInvalidRecordType );
            while (( pLDAPRecType != nil ) && (!bBuffFull))
            {
				bBuffFull = false;
				if (	(::strcmp( pAttrType, kDSAttributesAll ) == 0 ) ||
						(::strcmp( pAttrType, kDSAttributesStandardAll ) == 0 ) ||
						(::strcmp( pAttrType, kDSAttributesNativeAll ) == 0 ) )
				{
					//go get me all records that have any attribute equal to pSearchStr with pattMatch constraint
					//KW this is a very difficult search to do
					//approach A: set up a very complex search filter to pass to the LDAP server
					//need to be able to handle all standard types that are mapped
					//CHOSE THIS approach B: get each record and parse it completely using native attr types
					//approach C: just like A but concentrate on a selected subset of attr types
					siResult = FindAllRecords( pSearchStr, pRecType, pLDAPRecType, pattMatch, cpAttrTypeList, pContext, bAttribOnly, outBuff, uiCount );
				}
				else
				{
					//go get me all records that have pAttrType equal to pSearchStr with pattMatch constraint
					siResult = FindTheseRecords( pAttrType, pSearchStr, pRecType, pLDAPRecType, pattMatch, cpAttrTypeList, pContext, bAttribOnly, outBuff, uiCount );
				}

				//outBuff->GetDataBlockCount( &uiCount );
				//cannot use this as there may be records added from different record names
				//at which point the first name will fill the buffer with n records and
				//uiCount is reported as n but as the second name fills the buffer with m MORE records
				//the statement above will return the total of n+m and add it to the previous n
				//so that the total erroneously becomes 2n+m and not what it should be as n+m
				//therefore uiCount is extracted directly out of the FindxxxRecord(s) calls

				if ( siResult == CBuff::kBuffFull )
				{
					bBuffFull = true;
					//set continue if there is more data available
					inData->fIOContinueData = pContext;
					
					// check to see if buffer is full and no entries added
					// which implies that the buffer is too small
					if (uiCount == 0)
					{
						throw( (sInt32)eDSBufferTooSmall );
					}

					uiTotal += uiCount;
					inData->fOutMatchRecordCount = uiTotal;
					outBuff->SetLengthToSize();
					siResult = eDSNoErr;
				}
				else if ( siResult == eDSNoErr )
				{
					uiTotal += uiCount;
//	                    pContext->fRecNameIndex++;
				}

                if (pLDAPRecType != nil)
                {
                    delete( pLDAPRecType );
                    pLDAPRecType = nil;
                }
                
                if (!bBuffFull)
                {
	                numRecTypes++;
	                //get the next mapping
	                pLDAPRecType = MapRecToLDAPType( pRecType, pContext->fConfigTableIndex, numRecTypes );
                }
                
            } // while mapped Rec Type != nil
            
            if (!bBuffFull)
            {
	            pRecType = nil;
	            pContext->fRecTypeIndex++;
//	            pContext->fRecNameIndex = 1;
	            //reset the LDAP message ID to zero since now going to go after a new type
	            pContext->msgId = 0;
	            
	            //KW? here we decide to exit with data full of the current type of records
	            // and force a good exit with the data we have so we can come back for the next rec type
            	separateRecTypes = true;
                //set continue since there may be more data available
                inData->fIOContinueData = pContext;
				siResult = eDSNoErr;
				
				//however if this was the last rec type then there will be no more data
	            // check the number of rec types left
	            countDownRecTypes--;
	            if (countDownRecTypes == 0)
	            {
	                inData->fIOContinueData = nil;
				}
            }
            
        } // while loop over record types

        if (( siResult == eDSNoErr ) & (!bBuffFull))
        {
            if ( uiTotal == 0 )
            {
				//KW to remove all of this eDSRecordNotFound as per
				//2531386  dsGetRecordList should not return an error if record not found
                //siResult = eDSRecordNotFound;
                outBuff->ClearBuff();
            }
            else
            {
                outBuff->SetLengthToSize();
            }

            inData->fOutMatchRecordCount = uiTotal;
        }
    } // try
    
    catch( sInt32 err )
    {
            siResult = err;
    }

	if (pLDAPRecType != nil)
	{
		delete( pLDAPRecType );
		pLDAPRecType = nil;
	}

    if ( cpRecTypeList != nil )
    {
            delete( cpRecTypeList );
            cpRecTypeList = nil;
    }

    if ( cpAttrTypeList != nil )
    {
            delete( cpAttrTypeList );
            cpAttrTypeList = nil;
    }

	if ( pTmpDataList != nil )
	{
		dsDataListDeallocatePriv( pTmpDataList );
		free(pTmpDataList);
		pTmpDataList = nil;
	}

    if ( outBuff != nil )
    {
            delete( outBuff );
            outBuff = nil;
    }

    return( siResult );

} // DoAttributeValueSearch


//------------------------------------------------------------------------------------
//	* DoTheseAttributesMatch
//------------------------------------------------------------------------------------

bool CLDAPPlugIn::DoTheseAttributesMatch (	sLDAPContextData	   *inContext,
											char			   *inAttrName,
											tDirPatternMatch	pattMatch,
											LDAPMessage		   *inResult)
{
	char			   *pAttr					= nil;
	char			   *pVal					= nil;
	BerElement		   *ber;
	struct berval	  **bValues;
	bool				bFoundMatch				= false;

	//let's check all the attribute values for a match on the input name
	//with the given patt match constraint - first match found we stop and
	//then go get it all
	for (	pAttr = ldap_first_attribute (inContext->fHost, inResult, &ber );
			pAttr != NULL; pAttr = ldap_next_attribute(inContext->fHost, inResult, ber ) )
	{
		if (( bValues = ldap_get_values_len (inContext->fHost, inResult, pAttr )) != NULL)
		{
			// for each value of the attribute
			for (int i = 0; bValues[i] != NULL; i++ )
			{
				//need this since bValues might be binary data with no NULL terminator
				pVal = (char *) calloc(1,bValues[i]->bv_len + 1);
				memcpy(pVal, bValues[i]->bv_val, bValues[i]->bv_len);
				if (DoesThisMatch(pVal, inAttrName, pattMatch))
				{
					bFoundMatch = true;
					free(pVal);
					pVal = nil;
					break;
				}
				else
				{
					free(pVal);
					pVal = nil;
				}
			} // for each bValues[i]
			ldap_value_free_len(bValues);
			
			if (bFoundMatch)
			{
				if (pAttr != nil)
				{
					ldap_memfree( pAttr );
				}
				break;
			}
		} // if bValues = ldap_get_values_len ...
			
		if (pAttr != nil)
		{
			ldap_memfree( pAttr );
		}
	} // for ( loop over ldap_next_attribute )

	if (ber != nil)
	{
		ber_free( ber, 0 );
	}

	return( bFoundMatch );

} // DoTheseAttributesMatch


// ---------------------------------------------------------------------------
//	* DoesThisMatch
// ---------------------------------------------------------------------------

bool CLDAPPlugIn::DoesThisMatch (	const char		   *inString,
									const char		   *inPatt,
									tDirPatternMatch	inPattMatch )
{
	const char	   *p			= nil;
	bool			bMatched	= false;
	char		   *string1;
	char		   *string2;
	uInt32			length1		= 0;
	uInt32			length2		= 0;
	uInt32			uMatch		= 0;
	uInt16			usIndex		= 0;

	if ( (inString == nil) || (inPatt == nil) )
	{
		return( false );
	}

	length1 = strlen(inString);
	length2 = strlen(inPatt);
	string1 = new char[length1 + 1];
	string2 = new char[length2 + 1];
	
	if ( (inPattMatch >= eDSExact) && (inPattMatch <= eDSRegularExpression) )
	{
		strcpy(string1,inString);
		strcpy(string2,inPatt);
	}
	else
	{
		p = inString;
		for ( usIndex = 0; usIndex < length1; usIndex++  )
		{
			string1[usIndex] = toupper( *p );
			p++;
		}

		p = inPatt;
		for ( usIndex = 0; usIndex < length2; usIndex++  )
		{
			string2[usIndex] = toupper( *p );
			p++;
		}
	}

	uMatch = (uInt32) inPattMatch;
	switch ( uMatch )
	{
		case eDSExact:
		case eDSiExact:
		{
			if ( strcmp( string1, string2 ) == 0 )
			{
				bMatched = true;
			}
		}
		break;

		case eDSStartsWith:
		case eDSiStartsWith:
		{
			if ( strncmp( string1, string2, length2 ) == 0 )
			{
				bMatched = true;
			}
		}
		break;

		case eDSEndsWith:
		case eDSiEndsWith:
		{
			if ( length1 >= length2 )
			{
				if ( strcmp( string1 + length1 - length2, string2 ) == 0 )
				{
					bMatched = true;
				}
			}
		}
		break;

		case eDSContains:
		case eDSiContains:
		{
			if ( length1 >= length2 )
			{
				if ( strstr( string1, string2 ) != nil )
				{
					bMatched = true;
				}
			}
		}
		break;

		case eDSLessThan:
		case eDSiLessThan:
		{
			if ( strcmp( string1, string2 ) < 0 )
			{
				bMatched = true;
			}
		}
		break;

		case eDSGreaterThan:
		case eDSiGreaterThan:
		{
			if ( strcmp( string1, string2 ) > 0 )
			{
				bMatched = true;
			}
		}
		break;

		case eDSLessEqual:
		case eDSiLessEqual:
		{
			if ( strcmp( string1, string2 ) <= 0 )
			{
				bMatched = true;
			}
		}
		break;

		case eDSGreaterEqual:
		case eDSiGreaterEqual:
		{
			if ( strcmp( string1, string2 ) >= 0 )
			{
				bMatched = true;
			}
		}
		break;

		case eDSAnyMatch:
		default:
			break;
	}
	
	if (string1 != nil)
	{
		delete(string1);
	}

	if (string2 != nil)
	{
		delete(string2);
	}

	return( bMatched );

} // DoesThisMatch


//------------------------------------------------------------------------------------
//	* FindAllRecords
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::FindAllRecords (	char			   *inConstAttrName,
										char			   *inConstRecType,
										char			   *inNativeRecType,
										tDirPatternMatch	patternMatch,
										CAttributeList	   *inAttrTypeList,
										sLDAPContextData	   *inContext,
										bool				inAttrOnly,
										CBuff			   *inBuff,
										uInt32			   &outRecCount )
{
    sInt32				siResult		= eDSNoErr;
    sInt32				siValCnt		= 0;
    int					ldapReturnCode 	= 0;
    int					ldapMsgId		= 0;
    bool				bufferFull		= false;
    LDAPMessage		   *result			= nil;
    char			   *recName			= nil;
    sLDAPConfigData	   *pConfig			= nil;
    int					searchTO		= 0;
	bool				bFoundMatch		= false;
	CDataBuff		   *aRecData		= nil;
	CDataBuff		   *aAttrData		= nil;

	outRecCount = 0; //need to track how many records were found by this call to FindAllRecords
	
    try
    {
    	if (inContext == nil ) throw( (sInt32)eDSInvalidContext);
    	
 		aRecData = new CDataBuff();
		if ( aRecData  == nil ) throw( (sInt32)eMemoryError );
		aAttrData = new CDataBuff();
		if ( aAttrData  == nil ) throw( (sInt32)eMemoryError );

		//retrieve the config data
		//don't need to retrieve for the case of "generic unknown" so don't check index 0
		if (( inContext->fConfigTableIndex < gConfigTableLen) && ( inContext->fConfigTableIndex >= 1 ))
		{
			pConfig = (sLDAPConfigData *)gConfigTable->GetItemData( inContext->fConfigTableIndex );
			if (pConfig != nil)
			{
				searchTO	= pConfig->fSearchTimeout;
			}
		}
		
        // Here is the bind to the LDAP server
		siResult = RebindTryProc(inContext);
		if ( siResult != eDSNoErr ) throw( siResult );

        // here we check if there was a LDAP message ID in the context
        // If there was we continue to query, otherwise we search anew
        if (inContext->msgId == 0)
        {
            // here is the call to the LDAP server asynchronously which requires
            // host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
            // attribute list (NULL for all), return attrs values flag
            // This returns us the message ID which is used to query the server for the results
            if ( (ldapMsgId = ldap_search( inContext->fHost, inNativeRecType, LDAP_SCOPE_SUBTREE, (char *)"(objectclass=*)", NULL, 0) ) == -1 )
            {
	            throw( (sInt32)eDSNoErr); // used to throw eDSRecordNotFound
            }
            inContext->msgId = ldapMsgId;
        } // msgId == 0
        else
        {
            ldapMsgId = inContext->msgId;
        }
        
		if ( (inContext->fTotalRecCount < inContext->fLimitRecSearch) || (inContext->fLimitRecSearch == 0) )
		{
			//check it there is a carried LDAP message in the context
			if (inContext->pResult != nil)
			{
				result = inContext->pResult;
				ldapReturnCode = LDAP_RES_SEARCH_ENTRY;
			}
			//retrieve a new LDAP message
			else
			{
				if (searchTO == 0)
				{
					ldapReturnCode = ldap_result(inContext->fHost, ldapMsgId, 0, NULL, &result);
				}
				else
				{
					struct	timeval	tv;
					tv.tv_sec	= searchTO;
					tv.tv_usec	= 0;
					ldapReturnCode = ldap_result(inContext->fHost, ldapMsgId, 0, &tv, &result);
				}
			}
		}

		while ( ( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) && !(bufferFull) &&
				( (inContext->fTotalRecCount < inContext->fLimitRecSearch) || (inContext->fLimitRecSearch == 0) ) )
        {
			// check to see if there is a match
            // package the record into the DS format into the buffer
            // steps to add an entry record to the buffer
            // build the fRecData header
            // build the fAttrData
            // append the fAttrData to the fRecData
            // add the fRecData to the buffer inBuff
			
			bFoundMatch = false;
			if ( DoTheseAttributesMatch(inContext, inConstAttrName, patternMatch, result) )
			{
				bFoundMatch = true;

				aRecData->Clear();
	
				if ( inConstRecType != nil )
				{
					aRecData->AppendShort( ::strlen( inConstRecType ) );
					aRecData->AppendString( inConstRecType );
				} // what to do if the inConstRecType is nil? - never get here then
				else
				{
					aRecData->AppendShort( ::strlen( "Record Type Unknown" ) );
					aRecData->AppendString( "Record Type Unknown" );
				}
	
				// need to get the record name
				recName = GetRecordName( result, inContext, siResult );
				if ( siResult != eDSNoErr ) throw( siResult );
				if ( recName != nil )
				{
					aRecData->AppendShort( ::strlen( recName ) );
					aRecData->AppendString( recName );
	
					delete ( recName );
					recName = nil;
				}
				else
				{
					aRecData->AppendShort( ::strlen( "Record Name Unknown" ) );
					aRecData->AppendString( "Record Name Unknown" );
				}
	
				// need to calculate the number of attribute types ie. siValCnt
				// also need to extract the attributes and place them into aAttrData
				//siValCnt = 0;
	
				aAttrData->Clear();
				siResult = GetTheseAttributes( inAttrTypeList, result, inAttrOnly, inContext, siValCnt, aAttrData );
				if ( siResult != eDSNoErr ) throw( siResult );
				
				//add the attribute info to the fRecData
				if ( siValCnt == 0 )
				{
					// Attribute count
					aRecData->AppendShort( 0 );
				}
				else
				{
					// Attribute count
					aRecData->AppendShort( siValCnt );
					aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
				}
	
				// add the aRecData now to the inBuff
				siResult = inBuff->AddData( aRecData->GetData(), aRecData->GetLength() );
				
			} // DoTheseAttributesMatch?

            // need to check if the buffer is full
			// need to handle full buffer and keep the result alive for the next call in
            if (siResult == CBuff::kBuffFull)
            {
                bufferFull = true;
                
                //save the result if buffer is full
                inContext->pResult = result;
            }
            else if ( siResult == eDSNoErr )
            {
                ldap_msgfree( result );
				result = nil;
                
				if (bFoundMatch)
				{
					outRecCount++; //another record added
					inContext->fTotalRecCount++;
				}
				
                //make sure no result is carried in the context
                inContext->pResult = nil;

				//only get next result if buffer is not full
				if (searchTO == 0)
				{
					ldapReturnCode = ldap_result(inContext->fHost, ldapMsgId, 0, NULL, &result);
				}
				else
				{
					struct	timeval	tv;
					tv.tv_sec	= searchTO;
					tv.tv_usec	= 0;
					ldapReturnCode = ldap_result(inContext->fHost, ldapMsgId, 0, &tv, &result);
				}
            }
            else
            {
//                throw( (sInt32)eDSInvalidBuffFormat);
                
                //make sure no result is carried in the context
                inContext->pResult = nil;
                throw( (sInt32)eDSInvalidBuffFormat);
            }

        } // while loop over entries

        // KW need to check the ldapReturnCode for posible errors ie. ldapMsgId was stale
		if (ldapReturnCode == LDAP_TIMEOUT)
		{
	     	siResult = eDSServerTimeout;
		}
		if ( (result != inContext->pResult) && (result != nil) )
		{
			ldap_msgfree( result );
			result = nil;
		}
    } // try block

    catch( sInt32 err )
    {
        siResult = err;
    }

	if ( aRecData != nil )
	{
		delete( aRecData );
		aRecData = nil;
	}
	if ( aAttrData != nil )
	{
		delete( aAttrData );
		aAttrData = nil;
	}

    return( siResult );

} // FindAllRecords


//------------------------------------------------------------------------------------
//	* FindTheseRecords
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::FindTheseRecords (	char			   *inConstAttrType,
										char			   *inConstAttrName,
										char			   *inConstRecType,
										char			   *inNativeRecType,
										tDirPatternMatch	patternMatch,
										CAttributeList	   *inAttrTypeList,
										sLDAPContextData	   *inContext,
										bool				inAttrOnly,
										CBuff			   *inBuff,
										uInt32			   &outRecCount )
{
	sInt32					siResult		= eDSNoErr;
    sInt32					siValCnt		= 0;
    int						ldapReturnCode 	= 0;
    int						ldapMsgId		= 0;
    bool					bufferFull		= false;
    LDAPMessage			   *result			= nil;
    char				   *recName			= nil;
    char				   *queryFilter		= nil;
    sLDAPConfigData		   *pConfig			= nil;
    int						searchTO		= 0;
	CDataBuff			   *aRecData		= nil;
	CDataBuff			   *aAttrData		= nil;

	//build the record query string
	queryFilter = BuildLDAPQueryFilter(inConstAttrType, inConstAttrName, patternMatch, inContext->fConfigTableIndex, false);
	    
	outRecCount = 0; //need to track how many records were found by this call to FindTheseRecords
	
    try
    {
    
    	if (inContext == nil ) throw( (sInt32)eDSInvalidContext);
    	
    	// check to make sure the queryFilter is not nil
    	if ( queryFilter == nil ) throw( (sInt32)eDSNullParameter );
    	
 		aRecData = new CDataBuff();
		if ( aRecData  == nil ) throw( (sInt32)eMemoryError );
		aAttrData = new CDataBuff();
		if ( aAttrData  == nil ) throw( (sInt32)eMemoryError );

		//retrieve the config data
		//don't need to retrieve for the case of "generic unknown" so don't check index 0
		if (( inContext->fConfigTableIndex < gConfigTableLen) && ( inContext->fConfigTableIndex >= 1 ))
		{
			pConfig = (sLDAPConfigData *)gConfigTable->GetItemData( inContext->fConfigTableIndex );
			if (pConfig != nil)
			{
				searchTO	= pConfig->fSearchTimeout;
			}
		}
		
        // Here is the bind to the LDAP server
		siResult = RebindTryProc(inContext);
		if ( siResult != eDSNoErr ) throw( siResult );

        // here we check if there was a LDAP message ID in the context
        // If there was we continue to query, otherwise we search anew
        if (inContext->msgId == 0)
        {
            // here is the call to the LDAP server asynchronously which requires
            // host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
            // attribute list (NULL for all), return attrs values flag
            // This returns us the message ID which is used to query the server for the results
            if ( (ldapMsgId = ldap_search( inContext->fHost, inNativeRecType, LDAP_SCOPE_SUBTREE, queryFilter, NULL, 0) ) == -1 )
            {
	            throw( (sInt32)eDSNoErr); // used to throw eDSRecordNotFound
            }
            inContext->msgId = ldapMsgId;
        } // msgId == 0
        else
        {
            ldapMsgId = inContext->msgId;
        }
        
		if ( (inContext->fTotalRecCount < inContext->fLimitRecSearch) || (inContext->fLimitRecSearch == 0) )
		{
			//check if there is a carried LDAP message in the context
			if (inContext->pResult != nil)
			{
				result = inContext->pResult;
				ldapReturnCode = LDAP_RES_SEARCH_ENTRY;
			}
			//retrieve a new LDAP message
			else
			{
				if (searchTO == 0)
				{
					ldapReturnCode = ldap_result(inContext->fHost, ldapMsgId, 0, NULL, &result);
				}
				else
				{
					struct	timeval	tv;
					tv.tv_sec	= searchTO;
					tv.tv_usec	= 0;
					ldapReturnCode = ldap_result(inContext->fHost, ldapMsgId, 0, &tv, &result);
				}
			}
		}

		while ( ( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) && !(bufferFull) &&
				( (inContext->fTotalRecCount < inContext->fLimitRecSearch) || (inContext->fLimitRecSearch == 0) ) )
        {
            // package the record into the DS format into the buffer
            // steps to add an entry record to the buffer
            // build the fRecData header
            // build the fAttrData
            // append the fAttrData to the fRecData
            // add the fRecData to the buffer inBuff

            aRecData->Clear();

            if ( inConstRecType != nil )
            {
                aRecData->AppendShort( ::strlen( inConstRecType ) );
                aRecData->AppendString( inConstRecType );
            }
            else
            {
                aRecData->AppendShort( ::strlen( "Record Type Unknown" ) );
                aRecData->AppendString( "Record Type Unknown" );
            }

            // need to get the record name
            recName = GetRecordName( result, inContext, siResult );
            if ( siResult != eDSNoErr ) throw( siResult );
            if ( recName != nil )
            {
                aRecData->AppendShort( ::strlen( recName ) );
                aRecData->AppendString( recName );

                delete ( recName );
                recName = nil;
            }
            else
            {
                aRecData->AppendShort( ::strlen( "Record Name Unknown" ) );
                aRecData->AppendString( "Record Name Unknown" );
            }

            // need to calculate the number of attribute types ie. siValCnt
            // also need to extract the attributes and place them into fAttrData
            //siValCnt = 0;

            aAttrData->Clear();
            siResult = GetTheseAttributes( inAttrTypeList, result, inAttrOnly, inContext, siValCnt, aAttrData );
            if ( siResult != eDSNoErr ) throw( siResult );
			
            //add the attribute info to the fRecData
            if ( siValCnt == 0 )
            {
                // Attribute count
                aRecData->AppendShort( 0 );
            }
            else
            {
                // Attribute count
                aRecData->AppendShort( siValCnt );
                aRecData->AppendBlock( aAttrData->GetData(), aAttrData->GetLength() );
            }

            // add the fRecData now to the inBuff
            siResult = inBuff->AddData( aRecData->GetData(), aRecData->GetLength() );

            // need to check if the buffer is full
			// need to handle full buffer and keep the result alive for the next call in
            if (siResult == CBuff::kBuffFull)
            {
                bufferFull = true;
				inContext->msgId = ldapMsgId;
                
                //save the result if buffer is full
                inContext->pResult = result;
            }
            else if ( siResult == eDSNoErr )
            {
                ldap_msgfree( result );
				inContext->msgId = 0;
                
				outRecCount++; //added another record
				inContext->fTotalRecCount++;
				
                //make sure no result is carried in the context
                inContext->pResult = nil;

				//only get next result if buffer is not full
				if (searchTO == 0)
				{
					ldapReturnCode = ldap_result(inContext->fHost, ldapMsgId, 0, NULL, &result);
				}
				else
				{
					struct	timeval	tv;
					tv.tv_sec	= searchTO;
					tv.tv_usec	= 0;
					ldapReturnCode = ldap_result(inContext->fHost, ldapMsgId, 0, &tv, &result);
				}
            }
            else
            {
//                throw( (sInt32)eDSInvalidBuffFormat);
				inContext->msgId = 0;
                
                //make sure no result is carried in the context
                inContext->pResult = nil;
                throw( (sInt32)eDSInvalidBuffFormat);
            }
            
        } // while loop over entries

        // KW need to check the ldapReturnCode for posible errors ie. ldapMsgId was stale?
		if (ldapReturnCode == LDAP_TIMEOUT)
		{
	     	siResult = eDSServerTimeout;
		}
		if ( (result != inContext->pResult) && (result != nil) )
		{
			ldap_msgfree( result );
			result = nil;
		}

    } // try block

    catch( sInt32 err )
    {
        siResult = err;
    }

	if ( queryFilter != nil )
	{
		delete( queryFilter );
		queryFilter = nil;
	}

	if ( aRecData != nil )
	{
		delete( aRecData );
		aRecData = nil;
	}
	if ( aAttrData != nil )
	{
		delete( aAttrData );
		aAttrData = nil;
	}

    return( siResult );

} // FindTheseRecords


//------------------------------------------------------------------------------------
//	* DoAuthentication
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::DoAuthentication ( sDoDirNodeAuth *inData )
{
	sInt32				siResult		= noErr;
	UInt32				uiAuthMethod	= 0;
	sLDAPContextData	   *pContext		= nil;

	try
	{
		pContext = (sLDAPContextData *)gRefTable->GetItemData( inData->fInNodeRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );

		siResult = GetAuthMethod( inData->fInAuthMethod, &uiAuthMethod );
		if ( siResult == noErr )
		{
			switch( uiAuthMethod )
			{
				case kAuthCrypt:
				case kAuthNativeNoClearText:
					siResult = DoUnixCryptAuth( pContext, inData->fInAuthStepData );
					break;

				case kAuthNativeClearTextOK:
					if ( inData->fInDirNodeAuthOnlyFlag == true )
					{
						// auth only
						siResult = DoUnixCryptAuth( pContext, inData->fInAuthStepData );
						if ( siResult == eDSNoErr )
						{
							if ( inData->fOutAuthStepDataResponse->fBufferSize > ::strlen( kDSStdAuthCrypt ) )
							{
								::strcpy( inData->fOutAuthStepDataResponse->fBufferData, kDSStdAuthCrypt );
							}
						}
					}
					if ( (siResult != eDSNoErr) || (inData->fInDirNodeAuthOnlyFlag == false) )
					{
						siResult = DoClearTextAuth( pContext, inData->fInAuthStepData, inData->fInDirNodeAuthOnlyFlag );
						if ( siResult == eDSNoErr )
						{
							if ( inData->fOutAuthStepDataResponse->fBufferSize > ::strlen( kDSStdAuthClearText ) )
							{
								::strcpy( inData->fOutAuthStepDataResponse->fBufferData, kDSStdAuthClearText );
							}
						}
					}
					break;

				case kAuthClearText:
					siResult = DoClearTextAuth( pContext, inData->fInAuthStepData, inData->fInDirNodeAuthOnlyFlag );
					if ( siResult == eDSNoErr )
					{
						if ( inData->fOutAuthStepDataResponse->fBufferSize > ::strlen( kDSStdAuthClearText ) )
						{
							::strcpy( inData->fOutAuthStepDataResponse->fBufferData, kDSStdAuthClearText );
						}
					}
					break;

				default:
					siResult = eDSAuthMethodNotSupported;
					break;
			}
		}
	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	inData->fResult = siResult;

	return( siResult );

} // DoAuthentication


// ---------------------------------------------------------------------------
//	* GetAuthMethod
// ---------------------------------------------------------------------------

sInt32 CLDAPPlugIn::GetAuthMethod ( tDataNode *inData, uInt32 *outAuthMethod )
{
	sInt32			siResult		= noErr;
	char		   *p				= nil;

	if ( inData == nil )
	{
		*outAuthMethod = kAuthUnknowMethod;
		return( eDSAuthParameterError );
	}

	p = (char *)inData->fBufferData;

	CShared::LogIt( 0x0F, "LDAP PlugIn: Attempting use of authentication method %s", p );

	if ( ::strcmp( p, kDSStdAuthClearText ) == 0 )
	{
		// Clear text auth method
		*outAuthMethod = kAuthClearText;
	}
	else if ( ::strcmp( p, kDSStdAuthNodeNativeClearTextOK ) == 0 )
	{
		// Node native auth method
		*outAuthMethod = kAuthNativeClearTextOK;
	}
	else if ( ::strcmp( p, kDSStdAuthNodeNativeNoClearText ) == 0 )
	{
		// Node native auth method
		*outAuthMethod = kAuthNativeNoClearText;
	}
	else if ( ::strcmp( p, kDSStdAuthCrypt ) == 0 )
	{
		// Unix crypt auth
		*outAuthMethod = kAuthCrypt;
	}
	else
	{
		*outAuthMethod = kAuthUnknowMethod;
		siResult = eDSAuthMethodNotSupported;
	}

	return( siResult );

} // GetAuthMethod


//------------------------------------------------------------------------------------
//	* DoUnixCryptAuth
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::DoUnixCryptAuth ( sLDAPContextData *inContext, tDataBuffer *inAuthData )
{
	sInt32			siResult			= eDSAuthFailed;
	sInt32			bindResult			= eDSNoErr;
	char		   *pData				= nil;
	uInt32			offset				= 0;
	uInt32			buffSize			= 0;
	uInt32			buffLen				= 0;
	char		   *userName			= nil;
	sInt32			nameLen				= 0;
	char		   *pwd					= nil;
	sInt32			pwdLen				= 0;
	char		   *cryptPwd			= nil;
	char		   *pLDAPRecType		= nil;
    int				ldapMsgId			= 0;
    char		   *queryFilter			= nil;
	LDAPMessage	   *result				= nil;
	LDAPMessage	   *entry				= nil;
	char		   *attr				= nil;
	char		  **vals				= nil;
	BerElement	   *ber					= nil;
	int				ldapReturnCode 		= 0;
	int				numRecTypes			= 1;
	bool			bResultFound		= false;
    sLDAPConfigData	   *pConfig			= nil;
	char		  **attrs				= nil;
    int				searchTO			= 0;
	
	try
	{
		if ( inContext  == nil ) throw( (sInt32)eDSBadContextData );
		if ( inAuthData == nil ) throw( (sInt32)eDSNullDataBuff );

		pData		= inAuthData->fBufferData;
		if ( pData == nil ) throw( (sInt32)eDSNullDataBuff );
		
		buffSize	= inAuthData->fBufferSize;
		buffLen		= inAuthData->fBufferLength;
		if (buffLen > buffSize) throw( (sInt32)eDSInvalidBuffFormat );
	
		if ( (sInt32)(2 * sizeof( unsigned long ) + 1) > (sInt32)(buffLen - offset) ) throw( (sInt32)eDSInvalidBuffFormat );
		// need username length, password length, and username must be at least 1 character

		// Get the length of the user name
		::memcpy( &nameLen, pData, sizeof( unsigned long ) );
		if (nameLen == 0) throw( (sInt32)eDSInvalidBuffFormat );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );

		userName = (char *)::calloc(1, nameLen + 1);
		if ( userName == nil ) throw( (sInt32)eMemoryError );

		if ( (sInt32)nameLen > (sInt32)(buffLen - offset)) throw( (sInt32)eDSInvalidBuffFormat );
		// Copy the user name
		::memcpy( userName, pData, nameLen );
		
		pData += nameLen;
		offset += nameLen;

		if ( (sInt32)sizeof( unsigned long ) > (sInt32)(buffLen - offset) ) throw( (sInt32)eDSInvalidBuffFormat );
		// Get the length of the user password
		::memcpy( &pwdLen, pData, sizeof( unsigned long ) );
		pData += sizeof( unsigned long );
		offset += sizeof( unsigned long );

		pwd = (char *)::calloc(1, pwdLen + 1);
		if ( pwd == nil ) throw( (sInt32)eMemoryError );

		if ( (sInt32)pwdLen > (sInt32)(buffLen - offset) ) throw( (sInt32)eDSInvalidBuffFormat );
		// Copy the user password
		::memcpy( pwd, pData, pwdLen );

		queryFilter = BuildLDAPQueryFilter((char *)kDSNAttrRecordName, userName, eDSExact, inContext->fConfigTableIndex, false);
    	if ( queryFilter == nil ) throw( (sInt32)eDSNullParameter );
		
		attrs = MapAttrToLDAPTypeArray( kDS1AttrPassword, inContext->fConfigTableIndex );

		CShared::LogIt( 0x0F, "LDAP PlugIn: Attempting Crypt Authentication" );

		//(ldapMsgId = ldap_search( inContext->fHost, pLDAPRecType, LDAP_SCOPE_SUBTREE, queryFilter, NULL, 0) ) == -1
		//search for the specific LDAP record now
		
		//retrieve the config data
		//don't need to retrieve for the case of "generic unknown" so don't check index 0
		if (( inContext->fConfigTableIndex < gConfigTableLen) && ( inContext->fConfigTableIndex >= 1 ))
		{
			pConfig = (sLDAPConfigData *)gConfigTable->GetItemData( inContext->fConfigTableIndex );
			if (pConfig != nil)
			{
				searchTO	= pConfig->fSearchTimeout;
			}
		}
		
        // Here is the bind to the LDAP server
		bindResult = RebindTryProc(inContext);
		if ( bindResult != eDSNoErr ) throw( bindResult );

        // we will search over all the rectype mappings until we find the first
        // result for the search criteria in the queryfilter
        numRecTypes = 1;
		pLDAPRecType = MapRecToLDAPType( (char *)kDSStdRecordTypeUsers, inContext->fConfigTableIndex, numRecTypes );
		//only throw this for first time since we need at least one map
		if ( pLDAPRecType == nil ) throw( (sInt32)eDSInvalidRecordType );
		
		while ( (pLDAPRecType != nil) && (!bResultFound) )
		{
	        //here is the call to the LDAP server asynchronously which requires
	        // host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
	        // attribute list (NULL for all), return attrs values flag
	        // Note: asynchronous call is made so that a MsgId can be used for future calls
	        // This returns us the message ID which is used to query the server for the results
	        if ( (ldapMsgId = ldap_search( inContext->fHost, pLDAPRecType, LDAP_SCOPE_SUBTREE, queryFilter, attrs, 0) ) == -1 )
	        {
	        	bResultFound = false;
	        }
	        else
	        {
	        	bResultFound = true;
				//retrieve the actual LDAP record data for use internally
				//useful only from the read-only perspective
				//KW when write capability is added, we will need to re-read the result after a write
				if (searchTO == 0)
				{
					ldapReturnCode = ldap_result(inContext->fHost, ldapMsgId, 0, NULL, &result);
				}
				else
				{
					struct	timeval	tv;
					tv.tv_sec	= searchTO;
					tv.tv_usec	= 0;
					ldapReturnCode = ldap_result(inContext->fHost, ldapMsgId, 0, &tv, &result);
				}
	        }
		
			if (pLDAPRecType != nil)
			{
				delete (pLDAPRecType);
				pLDAPRecType = nil;
			}
			numRecTypes++;
			pLDAPRecType = MapRecToLDAPType( (char *)kDSStdRecordTypeUsers, inContext->fConfigTableIndex, numRecTypes );
		} // while ( (pLDAPRecType != nil) && (!bResultFound) )

		if ( (bResultFound) && ( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
		{
			//get the passwd attribute here
			//we are only going to look at the first attribute, first value
			entry = ldap_first_entry( inContext->fHost, result );
			if ( entry != nil )
			{
				attr = ldap_first_attribute( inContext->fHost, entry, &ber );
				if ( attr != nil )
				{
					vals = ldap_get_values( inContext->fHost, entry, attr );
					if ( vals != nil )
					{
						if ( vals[0] != nil )
						{
							cryptPwd = vals[0];
						}
						else
						{
							cryptPwd = (char *)""; //don't free this
						}
						if (::strncasecmp(cryptPwd,"{crypt}",7) == 0)
						{
							// special case for OpenLDAP's crypt password attribute
							// advance past {crypt} to the actual crypt password we want to compare against
							cryptPwd = cryptPwd + 7;
						}
						//account for the case where cryptPwd == "" such that we will auth if pwdLen is 0
						if (::strcmp(cryptPwd,"") != 0)
						{
							char salt[ 9 ];
							char hashPwd[ 32 ];
							salt[ 0 ] = cryptPwd[0];
							salt[ 1 ] = cryptPwd[1];
							salt[ 2 ] = '\0';
				
							::memset( hashPwd, 0, 32 );
							::strcpy( hashPwd, ::crypt( pwd, salt ) );
				
							siResult = eDSAuthFailed;
							if ( ::strcmp( hashPwd, cryptPwd ) == 0 )
							{
								siResult = eDSNoErr;
							}
							
						}
						else // cryptPwd is == ""
						{
							if ( ::strcmp(pwd,"") == 0 )
							{
								siResult = eDSNoErr;
							}
						}
						ldap_value_free( vals );
						vals = nil;
					}
					ldap_memfree( attr );
					attr = nil;
				}
				if ( ber != nil )
				{
					ber_free( ber, 0 );
				}
   				ldap_abandon( inContext->fHost, ldapMsgId ); // we don't care about the other results, just the first
			}		
		} // if bResultFound and ldapReturnCode okay
		/*else if ( bResultFound && ( ldapReturnCode == 0 ) )
		{
			// there was a timeout
			ldap_abandon( inContext->fHost, ldapMsgId );
		}*/
		
		//no check for LDAP_TIMEOUT on ldapReturnCode since we will return nil
		if ( result != nil )
		{
			ldap_msgfree( result );
			result = nil;
		}
	}

	catch( sInt32 err )
	{
		CShared::LogIt( 0x0F, "LDAP PlugIn: Crypt authentication error %l", err );
		siResult = err;
	}
	
	if ( attrs != nil )
	{
		for ( int i = 0; attrs[i] != nil; ++i )
		{
			free( attrs[i] );
		}
		free( attrs );
		attrs = nil;
	}

	if ( userName != nil )
	{
		delete( userName );
		userName = nil;
	}

	if ( pwd != nil )
	{
		delete( pwd );
		pwd = nil;
	}

	if ( queryFilter != nil )
	{
		delete( queryFilter );
		queryFilter = nil;
	}
	
	return( siResult );

} // DoUnixCryptAuth


//------------------------------------------------------------------------------------
//	* DoClearTextAuth
//------------------------------------------------------------------------------------

sInt32 CLDAPPlugIn::DoClearTextAuth ( sLDAPContextData *inContext, tDataBuffer *inAuthData, bool authCheckOnly )
{
	sInt32				siResult			= eDSAuthFailed;
	char			   *pData				= nil;
	char			   *userName			= nil;
	char			   *accountId			= nil;
	sInt32				nameLen				= 0;
	char			   *pwd					= nil;
	sInt32				pwdLen				= 0;
	int					ldapBindReturn		= LDAP_INVALID_CREDENTIALS;
	bool				clearCredentials	= false;
    sLDAPConfigData	   *pConfig				= nil;
    int					bindMsgId			= 0;
	LDAPMessage		   *result				= nil;
    int					openTO				= 0;

	try
	{
	//check the authCheckOnly if true only test name and password
	//ie. need to retain current credentials
		if ( inContext  == nil ) throw( (sInt32)eDSBadContextData );
		if ( inAuthData == nil ) throw( (sInt32)eDSNullDataBuff );

		pData = inAuthData->fBufferData;
		if ( pData == nil ) throw( (sInt32)eDSNullDataBuff );
		
		// Get the length of the user name
		::memcpy( &nameLen, pData, sizeof( long ) );
		//accept the case of a NO given name and NO password
		//LDAP servers use this to reset the credentials
		//if (nameLen == 0) throw( (sInt32)eDSAuthInBuffFormatError );

		if (nameLen > 0)
		{
			userName = (char *) calloc(1, nameLen + 1);
			if ( userName == nil ) throw( (sInt32)eMemoryError );

			// Copy the user name
//			::memset( userName, 0, nameLen + 1 );
			pData += sizeof( long );
			::memcpy( userName, pData, nameLen );
			pData += nameLen;
		}

		// Get the length of the user password
		::memcpy( &pwdLen, pData, sizeof( long ) );
		//accept the case of a given name and NO password
		//LDAP servers use this as tracking info when no password is required
		//if (pwdLen == 0) throw( (sInt32)eDSAuthInBuffFormatError );

		if (pwdLen > 0)
		{
			pwd = (char *) calloc(1, pwdLen + 1);
			if ( pwd == nil ) throw( (sInt32)eMemoryError );

			// Copy the user password
//			::memset( pwd, 0, pwdLen + 1 );
			pData += sizeof( long );
			::memcpy( pwd, pData, pwdLen );
			pData += pwdLen;
		}

		// Use this username and password if it is given
		// Want to be able to call this here with NULLs
		// so that the credentials can be released
		// however, the in data needs to have two zero lengths indicated
		if ((nameLen == 0) && (pwdLen == 0) && (!authCheckOnly))
		{
			clearCredentials = true;
		}
		CShared::LogIt( 0x0F, "LDAP PlugIn: Attempting Auth Server Cleartext Authentication" );

		//get the correct account id
		//we assume that the DN is always used for this
		if (userName)
		{
			//here again we choose the first match
			accountId = GetDNForRecordName ( userName, inContext );
		}
                
		//if username did not garner an accountId then fail authentication
		if (!accountId)
		{
			throw( (sInt32)eDSAuthFailed);
		}
		
		//KW need to update this with an auth policy specific to LDAP server usage
		//ie use the authCheckOnly flag for true auth versus LDAP access
		//note this plugin is currently READ ONLY but the code has the access handling
		//for write potential
//		if (authCheckOnly && ((pwdLen == 0) || (nameLen == 0)))
//		{
//		//here we are doing the auth check while not opening LDAP for later use
//			throw( (sInt32)eDSAuthFailed);
//		}
//		else
		//KW fix for RADAR# 2537199 to be reworked with code above AFTER user seed
		if ((pwd != NULL) && (pwd[0] != '\0') && (nameLen != 0))
		{
		// Do the authentication
		//here is the bind to the LDAP server
		//ldapBindReturn = ldap_bind_s( inContext->fHost, accountId, pwd, LDAP_AUTH_SIMPLE );
		
		//need to use our timeout so we don't hang indefinitely
		bindMsgId = ldap_bind( inContext->fHost, accountId, pwd, LDAP_AUTH_SIMPLE );
		if ( bindMsgId == -1 )
		{
			// maybe the server went down, let's try once to reconnect
			ldap_unbind( inContext->fHost );
			inContext->fHost = NULL;
			inContext->fHost = ldap_init( inContext->fName, inContext->fPort );
			if ( inContext->fHost == nil ) throw( (sInt32)eDSCannotAccessSession );
				
			//protect against thread unsafe gethostbyname call within LDAP framework
			gLDAPOpenMutex->Wait();
			bindMsgId = ldap_bind( inContext->fHost, accountId, pwd, LDAP_AUTH_SIMPLE );
			gLDAPOpenMutex->Signal();

			if (bindMsgId == -1)
			{
				throw( (sInt32)eDSCannotAccessSession);
			}
		}
		//get the timeout value from the config of this context
		if (( inContext->fConfigTableIndex < gConfigTableLen) && ( inContext->fConfigTableIndex >= 1 ))
		{
			pConfig = (sLDAPConfigData *)gConfigTable->GetItemData( inContext->fConfigTableIndex );
			if (pConfig != nil)
			{
				openTO = pConfig->fOpenCloseTimeout;
			}
		}
		
		if (openTO == 0)
		{
			ldapBindReturn = ldap_result(inContext->fHost, bindMsgId, 0, NULL, &result);
		}
		else
		{
			struct	timeval	tv;
			tv.tv_sec		= openTO;
			tv.tv_usec		= 0;
			ldapBindReturn	= ldap_result(inContext->fHost, bindMsgId, 0, &tv, &result);
		}
		if ( ldapBindReturn == -1 )
		{
			throw( (sInt32)eDSCannotAccessSession);
		}
		else if ( ldapBindReturn == 0 )
		{
			// timed out, let's forget it
			ldap_abandon(inContext->fHost, bindMsgId);
			throw( (sInt32)eDSCannotAccessSession);
		}
		else
		{
			ldapBindReturn = ldap_result2error(inContext->fHost, result, 1);
		}
		bindMsgId		= 0;
		//result is consumed above within ldap_result2error
		result = nil;

		if (ldapBindReturn == LDAP_SUCCESS)
		{
	        siResult = eDSNoErr;
	            
			if (!authCheckOnly)
			{
				//cleanup past credentials
				if (inContext->authAccountName)
				{
					delete inContext->authAccountName;
					inContext->authAccountName = nil;
				}
				if (inContext->authPassword)
				{
					delete inContext->authPassword;
					inContext->authPassword = nil;
				}
				
				if (clearCredentials)
				{
					inContext->authCallActive = false;
				}
				else
				{
					//auth call is active if either
					//- name is given for tracking purposes
					//- name and password are given for true authentication
					inContext->authCallActive = true;
					//set with new credentials if they exist
					if (accountId)
					{
						inContext->authAccountName = new char(1+::strlen(accountId));
						::strcpy(inContext->authAccountName, accountId);
					}
					if (pwd)
					{
						inContext->authPassword = new char(1+::strlen(pwd));
						::strcpy(inContext->authPassword, pwd);
					}
				} // not clearCredentials
			}
		}
		else if (ldapBindReturn == LDAP_INVALID_CREDENTIALS)
		{
			throw( (sInt32)eDSAuthFailed);
		}
		else if (ldapBindReturn == LDAP_AUTH_UNKNOWN)
		{
			throw( (sInt32)eDSAuthFailed);
		}
		else
		{
			throw( (sInt32)eDSCannotAccessSession);
		}
		}

	}

	catch( sInt32 err )
	{
		CShared::LogIt( 0x0F, "LDAP PlugIn: Cleartext authentication error %l", err );
		siResult = err;
	}

	if ( accountId != nil )
	{
		delete( accountId );
		accountId = nil;
	}

	if ( userName != nil )
	{
		delete( userName );
		userName = nil;
	}

	if ( pwd != nil )
	{
		delete( pwd );
		pwd = nil;
	}

	return( siResult );

} // DoClearTextAuth

//------------------------------------------------------------------------------------
//	* GetDNForRecordName
//------------------------------------------------------------------------------------

char* CLDAPPlugIn::GetDNForRecordName ( char* inRecName, sLDAPContextData *inContext )
{
	char			   *ldapDN			= nil;	
	char			   *pLDAPRecType	= nil;
    int					ldapMsgId		= 0;
    char			   *queryFilter		= nil;
	LDAPMessage		   *result			= nil;
	int					ldapReturnCode	= 0;
	int					numRecTypes		= 1;
	bool				bResultFound	= false;
    sLDAPConfigData	   *pConfig			= nil;
    int					searchTO		= 0;
	sInt32				siResult		= eDSNoErr;


	try
	{
		if ( inRecName  == nil ) throw( (sInt32)eDSNullRecName );
		if ( inContext  == nil ) throw( (sInt32)eDSBadContextData );

		//build the record query string
		queryFilter = BuildLDAPQueryFilter((char *)kDSNAttrRecordName, inRecName, eDSExact, inContext->fConfigTableIndex, false);
    	if ( queryFilter == nil ) throw( (sInt32)eDSNullParameter );

		//search for the specific LDAP record now
		
		//retrieve the config data
		//don't need to retrieve for the case of "generic unknown" so don't check index 0
		if (( inContext->fConfigTableIndex < gConfigTableLen) && ( inContext->fConfigTableIndex >= 1 ))
		{
			pConfig = (sLDAPConfigData *)gConfigTable->GetItemData( inContext->fConfigTableIndex );
			if (pConfig != nil)
			{
				searchTO	= pConfig->fSearchTimeout;
			}
		}
		
        // Here is the bind to the LDAP server
		siResult = RebindTryProc(inContext);
		if ( siResult != eDSNoErr ) throw( siResult );

        // we will search over all the rectype mappings until we find the first
        // result for the search criteria in the queryfilter
        numRecTypes = 1;
		pLDAPRecType = MapRecToLDAPType( (char *)kDSStdRecordTypeUsers, inContext->fConfigTableIndex, numRecTypes );
		//only throw this for first time since we need at least one map
		if ( pLDAPRecType == nil ) throw( (sInt32)eDSInvalidRecordType );
		
		while ( (pLDAPRecType != nil) && (!bResultFound) )
		{

	        //here is the call to the LDAP server asynchronously which requires
	        // host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
	        // attribute list (NULL for all), return attrs values flag
	        // Note: asynchronous call is made so that a MsgId can be used for future calls
	        // This returns us the message ID which is used to query the server for the results
	        if ( (ldapMsgId = ldap_search( inContext->fHost, pLDAPRecType, LDAP_SCOPE_SUBTREE, queryFilter, NULL, 0) ) == -1 )
	        {
	        	bResultFound = false;
	        }
	        else
	        {
	        	bResultFound = true;
				//retrieve the actual LDAP record data for use internally
				//useful only from the read-only perspective
				//KW when write capability is added, we will need to re-read the result after a write
				if (searchTO == 0)
				{
					ldapReturnCode = ldap_result(inContext->fHost, ldapMsgId, 0, NULL, &result);
				}
				else
				{
					struct	timeval	tv;
					tv.tv_sec	= searchTO;
					tv.tv_usec	= 0;
					ldapReturnCode = ldap_result(inContext->fHost, ldapMsgId, 0, &tv, &result);
				}
	        }
		
			if (pLDAPRecType != nil)
			{
				delete (pLDAPRecType);
				pLDAPRecType = nil;
			}
			numRecTypes++;
			pLDAPRecType = MapRecToLDAPType( (char *)kDSStdRecordTypeUsers, inContext->fConfigTableIndex, numRecTypes );
		} // while ( (pLDAPRecType != nil) && (!bResultFound) )

		if ( (bResultFound) && ( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
		{
			//get the ldapDN here
			ldapDN = ldap_get_dn(inContext->fHost, result);
		
		} // if bResultFound and ldapReturnCode okay
		
		//no check for LDAP_TIMEOUT on ldapReturnCode since we will return nil
		if ( result != nil )
		{
			ldap_msgfree( result );
			result = nil;
		}
	}

	catch( sInt32 err )
	{
		ldapDN = nil;
	}

	if ( pLDAPRecType != nil )
	{
		delete( pLDAPRecType );
		pLDAPRecType = nil;
	}

	if ( queryFilter != nil )
	{
		delete( queryFilter );
		queryFilter = nil;
	}

	return( ldapDN );

} // GetDNForRecordName

//------------------------------------------------------------------------------------
//      * DoPlugInCustomCall
//------------------------------------------------------------------------------------ 

sInt32 CLDAPPlugIn::DoPlugInCustomCall ( sDoPlugInCustomCall *inData )
{
	sInt32			siResult	= eDSNoErr;
	unsigned long	aRequest	= 0;
	sLDAPContextData	   *pContext		= nil;
	sInt32				xmlDataLength	= 0;
	CFDataRef   		xmlData			= nil;
	unsigned long		bufLen			= 0;
	AuthorizationRef	authRef			= 0;
	AuthorizationItemSet   *resultRightSet = NULL;

//seems that the client needs to have a tDirNodeReference 
//to make the custom call even though it will likely be non-dirnode specific related

	try
	{
		if ( inData == nil ) throw( (sInt32)eDSNullParameter );
		if ( inData->fInRequestData == nil ) throw( (sInt32)eDSNullDataBuff );
		if ( inData->fInRequestData->fBufferData == nil ) throw( (sInt32)eDSEmptyBuffer );

		pContext = (sLDAPContextData *)gRefTable->GetItemData( inData->fInNodeRef );
		if ( pContext  == nil ) throw( (sInt32)eDSBadContextData );
		
		if ( strcmp(pContext->fName,"LDAPv2 Configure") == 0 )
		{
			aRequest = inData->fInRequestCode;
			bufLen = inData->fInRequestData->fBufferLength;
			if ( bufLen < sizeof( AuthorizationExternalForm ) ) throw( (sInt32)eDSInvalidBuffFormat );
			siResult = AuthorizationCreateFromExternalForm((AuthorizationExternalForm *)inData->fInRequestData->fBufferData,
				&authRef);
			if (siResult != errAuthorizationSuccess)
			{
				throw( (sInt32)eDSPermissionError );
			}

			AuthorizationItem rights[] = { {"system.services.directory.configure", 0, 0, 0} };
			AuthorizationItemSet rightSet = { sizeof(rights)/ sizeof(*rights), rights };
		
			siResult = AuthorizationCopyRights(authRef, &rightSet, NULL,
				kAuthorizationFlagExtendRights, &resultRightSet);
			if (resultRightSet != NULL)
			{
				AuthorizationFreeItemSet(resultRightSet);
				resultRightSet = NULL;
			}
			if (siResult != errAuthorizationSuccess)
			{
				throw( (sInt32)eDSPermissionError );
			}
			switch( aRequest )
			{
				case 66:
					// get length of XML file
						
					if ( inData->fOutRequestResponse == nil ) throw( (sInt32)eDSNullDataBuff );
					if ( inData->fOutRequestResponse->fBufferData == nil ) throw( (sInt32)eDSEmptyBuffer );
					if ( inData->fOutRequestResponse->fBufferSize < sizeof( CFIndex ) ) throw( (sInt32)eDSInvalidBuffFormat );
					if (pConfigFromXML)
					{
						// need four bytes for size
						xmlData = pConfigFromXML->GetXMLConfig();
						if (xmlData != 0)
						{
							CFRetain(xmlData);
							*(CFIndex*)(inData->fOutRequestResponse->fBufferData) = CFDataGetLength(xmlData);
							inData->fOutRequestResponse->fBufferLength = sizeof( CFIndex );
							CFRelease(xmlData);
							xmlData = 0;
						}
					}
					break;
					
				case 77:
					// read xml config
					CFRange	aRange;
						
					if ( inData->fOutRequestResponse == nil ) throw( (sInt32)eDSNullDataBuff );
					if ( inData->fOutRequestResponse->fBufferData == nil ) throw( (sInt32)eDSEmptyBuffer );
					if (pConfigFromXML)
					{
						xmlData = pConfigFromXML->GetXMLConfig();
						if (xmlData != 0)
						{
							CFRetain(xmlData);
							aRange.location = 0;
							aRange.length = CFDataGetLength(xmlData);
							if ( inData->fOutRequestResponse->fBufferSize < (unsigned int)aRange.length) throw( (sInt32)eDSBufferTooSmall );
							CFDataGetBytes( xmlData, aRange, (UInt8*)(inData->fOutRequestResponse->fBufferData) );
							inData->fOutRequestResponse->fBufferLength = aRange.length;
							CFRelease(xmlData);
							xmlData = 0;
						}
					}
					break;
					
				case 88:
					//here we accept an XML blob to replace the current config file
					//need to make xmlData large enough to receive the data
					xmlDataLength = (sInt32) bufLen - sizeof( AuthorizationExternalForm );
					if ( xmlDataLength <= 0) throw( (sInt32)eDSInvalidBuffFormat );
					xmlData = CFDataCreate(NULL,(UInt8 *)(inData->fInRequestData->fBufferData + sizeof( AuthorizationExternalForm )),
						xmlDataLength);
					if (pConfigFromXML)
					{
						// refresh registered nodes
						siResult = pConfigFromXML->SetXMLConfig(xmlData);
						siResult = pConfigFromXML->WriteXMLConfig();
						Initialize();
					}
					CFRelease(xmlData);
					break;
					
				case 99:
					Initialize();
					break;

				default:
					break;
			}
		}

	}

	catch( sInt32 err )
	{
		siResult = err;
	}

	if (authRef != 0)
	{
		AuthorizationFree(authRef, 0);
		authRef = 0;
	}

	return( siResult );

} // DoPlugInCustomCall


// ---------------------------------------------------------------------------
//	* ContextDeallocProc
// ---------------------------------------------------------------------------

void CLDAPPlugIn::ContextDeallocProc ( void* inContextData )
{
	sLDAPContextData *pContext = (sLDAPContextData *) inContextData;

	if ( pContext != nil )
	{
		CleanContextData( pContext );
		
		free( pContext );
		pContext = nil;
	}
} // ContextDeallocProc

// ---------------------------------------------------------------------------
//	* RebindTryProc
// ---------------------------------------------------------------------------

sInt32 CLDAPPlugIn:: RebindTryProc ( sLDAPContextData *inContext )
{

    sInt32				siResult		= eDSNoErr;
    int					ldapReturnCode 	= 0;
    int					bindMsgId		= 0;
    LDAPMessage		   *result			= nil;
    sLDAPConfigData	   *pConfig			= nil;
    char			   *ldapAcct		= nil;
    char			   *ldapPasswd		= nil;
    int					openTO			= 0;

	try
	{
		// if the inContext->fHost == nil at this point then the node was really never
		// opened as in the case of the Configure capability of this plugin
		// so we exit here
		if ( inContext->fHost == nil ) throw( (sInt32)eDSCannotAccessSession );
		
        // Here is the bind to the LDAP server
        // Would expect to always bind since can't guarantee the client made calls
        // quick enough in succession
		// Note that there may be stored name/password in the config table
		// ie. always use the config table data if authentication has not explicitly been set
		
        //NOTE: we don't rebind if authCallActive since the accountname and password are in the context
		
		if (!(inContext->authCallActive))
		{
			//retrieve the config data
			//don't need to retrieve for the case of "generic unknown" so don't check index 0
			if (( inContext->fConfigTableIndex < gConfigTableLen) && ( inContext->fConfigTableIndex >= 1 ))
			{
				pConfig = (sLDAPConfigData *)gConfigTable->GetItemData( inContext->fConfigTableIndex );
				if (pConfig != nil)
				{
					if (pConfig->bSecureUse)
					{
						if (pConfig->fServerAccount != nil)
						{
							ldapAcct = new char[1+::strlen(pConfig->fServerAccount)];
							::strcpy( ldapAcct, pConfig->fServerAccount );
						}
						if (pConfig->fServerPassword != nil)
						{
							ldapPasswd = new char[1+::strlen(pConfig->fServerPassword)];
							::strcpy( ldapPasswd, pConfig->fServerPassword );
						}
					}
					openTO		= pConfig->fOpenCloseTimeout;
				}
			}

			//need to use our timeout so we don't hang indefinitely
			bindMsgId = ldap_bind( inContext->fHost, ldapAcct, ldapPasswd, LDAP_AUTH_SIMPLE );
			//KW we don't try a rebind here if we were in the middle of our context
			//ie. let the client try the call from the beginning again since we can't rebind
			//and continue in the middle of a ldap_search
	        if ( (bindMsgId == -1) && (inContext->msgId == 0) )
	        {
				// maybe the server went down, let's try once to reconnect
				ldap_unbind( inContext->fHost );
				inContext->fHost = NULL;
				inContext->fHost = ldap_init( inContext->fName, inContext->fPort );
				if ( inContext->fHost == nil ) throw( (sInt32)eDSCannotAccessSession );
				
				//protect against thread unsafe gethostbyname call within LDAP framework
				gLDAPOpenMutex->Wait();
				bindMsgId = ldap_bind( inContext->fHost, ldapAcct, ldapPasswd, LDAP_AUTH_SIMPLE );
				gLDAPOpenMutex->Signal();

				if (bindMsgId == -1)
	        	{
	            	throw( (sInt32)eDSCannotAccessSession);
				}
	        }
			else if ( bindMsgId == -1 )
			{
				throw( (sInt32)eDSCannotAccessSession);
			}
			if (openTO == 0)
			{
				ldapReturnCode = ldap_result(inContext->fHost, bindMsgId, 0, NULL, &result);
			}
			else
			{
				struct	timeval	tv;
				tv.tv_sec		= openTO;
				tv.tv_usec		= 0;
				ldapReturnCode	= ldap_result(inContext->fHost, bindMsgId, 0, &tv, &result);
			}
			if ( ldapReturnCode == -1 )
			{
				// maybe the server went down, let's try once to reconnect
				ldap_unbind( inContext->fHost );
				inContext->fHost = NULL;
				inContext->fHost = ldap_init( inContext->fName, inContext->fPort );
				if ( inContext->fHost == nil ) throw( (sInt32)eDSCannotAccessSession );
				
				//protect against thread unsafe gethostbyname call within LDAP framework
				gLDAPOpenMutex->Wait();
				bindMsgId = ldap_bind( inContext->fHost, ldapAcct, ldapPasswd, LDAP_AUTH_SIMPLE );
				gLDAPOpenMutex->Signal();

				if (bindMsgId == -1)
	        	{
	            	throw( (sInt32)eDSCannotAccessSession);
				}
				if (openTO == 0)
				{
					ldapReturnCode = ldap_result(inContext->fHost, bindMsgId, 0, NULL, &result);
				}
				else
				{
					struct	timeval	tv;
					tv.tv_sec		= openTO;
					tv.tv_usec		= 0;
					ldapReturnCode	= ldap_result(inContext->fHost, bindMsgId, 0, &tv, &result);
				}
			}
			if ( ldapReturnCode == -1 )
			{
				throw( (sInt32)eDSCannotAccessSession);
			}
			else if ( ldapReturnCode == 0 )
			{
				// timed out, let's forget it
				ldap_abandon(inContext->fHost, bindMsgId);
				throw( (sInt32)eDSCannotAccessSession);
			}
			else if ( ldap_result2error(inContext->fHost, result, 1) != LDAP_SUCCESS )
			{
				throw( (sInt32)eDSCannotAccessSession);
			}
			bindMsgId		= 0;
			ldapReturnCode	= 0;
			//result is consumed above within ldap_result2error
			result = nil;
	        
        } // (!(inContext->authCallActive))
		
	} // try
	
	catch( sInt32 err )
	{
		siResult = err;
	}
	
	if (ldapAcct != nil)
	{
		delete (ldapAcct);
		ldapAcct = nil;
	}
	if (ldapPasswd != nil)
	{
		delete (ldapPasswd);
		ldapPasswd = nil;
	}
	

	return (siResult);
	
}// RebindTryProc

