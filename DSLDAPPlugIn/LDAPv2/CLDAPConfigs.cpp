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
 * @header CLDAPConfigs
 * Code to parse a XML config file.
 */

#include "CLDAPConfigs.h"
#include <DirectoryServiceCore/CSharedData.h>

#include <CoreFoundation/CFPriv.h>		// used for ::CFCopySearchPathForDirectoriesInDomains

#include <string.h>				//used for strcpy, etc.
#include <stdlib.h>				//used for malloc
#include <sys/types.h>
#include <sys/stat.h>			//used for mkdir and stat

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>

#define kAllocatorDefault NULL


// --------------------------------------------------------------------------------
//	* CLDAPConfigs
// --------------------------------------------------------------------------------

CLDAPConfigs::CLDAPConfigs ( void )
{
	pConfigTable			= nil;
    pStdAttributeMapTuple	= nil;
    pStdRecordMapTuple		= nil;
	fConfigTableLen			= 0;
	fXMLData				= nil;
} // CLDAPConfigs


// --------------------------------------------------------------------------------
//	* ~CLDAPConfigs ()
// --------------------------------------------------------------------------------

CLDAPConfigs::~CLDAPConfigs ( void )
{
    uInt32				iTableIndex	= 0;
	sInt32				siResult 	= eDSNoErr;
    sLDAPConfigData	   *pConfig		= nil;

	//need to cleanup the config table ie. the internals
    for (iTableIndex=0; iTableIndex<fConfigTableLen; iTableIndex++)
    {
        pConfig = (sLDAPConfigData *)pConfigTable->GetItemData( iTableIndex );
        if (pConfig != nil)
        {
            // delete the contents of sLDAPConfigData here
            // not checking the return status of the clean here
			// since don't plan to continue
            CleanLDAPConfigData( pConfig );
            // delete the sLDAPConfigData itself
            delete( pConfig );
            pConfig = nil;
            // remove the table entry
            pConfigTable->RemoveItem( iTableIndex );
        }
    }
    fConfigTableLen = 0;
    if ( pConfigTable != nil)
    {
        delete ( pConfigTable );
        pConfigTable = nil;
    }
	
	if (fXMLData != nil)
	{
		CFRelease(fXMLData);
		fXMLData = nil;
	}

	//free up the standard mapping tables after the config table is freed
	siResult = CleanMapTuple(pStdAttributeMapTuple);
	siResult = CleanMapTuple(pStdRecordMapTuple);

} // ~CLDAPConfigs


// --------------------------------------------------------------------------------
//	* Init (CPlugInRef, sMapTuple, sMapTuple)
// --------------------------------------------------------------------------------

sInt32 CLDAPConfigs::Init ( CPlugInRef *inConfigTable, uInt32 &inConfigTableLen, sMapTuple **inStdAttributeMapTuple, sMapTuple **inStdRecordMapTuple )
{

	sInt32				siResult	= eDSNoErr;
	sLDAPConfigData	   *pConfig		= nil;
	uInt32				sIndex		= 0;
    uInt32				iTableIndex	= 0;

	try
		{	
				//Init is set up so that if it is called initially or by a custom call
				//it will keep on adding and deleting configs as required
				if (inConfigTableLen != 0)
				{
						fConfigTableLen = inConfigTableLen;
				}
				if ( inConfigTable == nil )
				{
						inConfigTable = new CPlugInRef( nil );
				}
	    
				pConfigTable = inConfigTable;

				//check for Generic node which has server name "unknown"
				if (!CheckForConfig((char *)"unknown", sIndex))
				{
						//build a default config entry that can be used when no config exists
						pConfig = MakeLDAPConfigData((char *)"Generic",(char *)"unknown",true,120,120,389,false, 0, 0);
						pConfigTable->AddItem( fConfigTableLen, pConfig );
						fConfigTableLen++;
				}
			
				//read the XML Config file
				if (fXMLData != nil)
				{
					CFRelease(fXMLData);
					fXMLData = nil;
				}
				siResult = ReadXMLConfig();
				
				//check if XML file was read
				if (siResult == eDSNoErr)
				{
					//need to set the Updated flag to false so that nodes will get Unregistered
					//if a config no longer exists for that entry
					//this needs to be done AFTER it is verified that a XML config file exists
					if (inConfigTableLen != 0)
					{
						//need to cycle through the config table
						for (iTableIndex=0; iTableIndex<fConfigTableLen; iTableIndex++)
						{
							pConfig = (sLDAPConfigData *)pConfigTable->GetItemData( iTableIndex );
							if (pConfig != nil)
							{
								pConfig->bUpdated = false;
							}
						}
					}
	    		
					//set up the config table
					siResult = ConfigLDAPServers();
				}
				
				//set/update the number of configs in the table
				inConfigTableLen = fConfigTableLen;
		
				//create the default mapping tables if the mappings are nil here
				//use the hardcode inside this class
				if (pStdAttributeMapTuple == nil)
				{
						siResult = BuildDefaultStdAttributeMap();
				}
				if (pStdRecordMapTuple == nil)
				{
						siResult = BuildDefaultStdRecordMap();
				}
				//assign the default mappings to be returned
				//since they are also used back in CLDAPPlugIn directly
				if (inStdAttributeMapTuple != nil)
				{
					*inStdAttributeMapTuple		= pStdAttributeMapTuple;
				}
				if (inStdRecordMapTuple != nil)
				{
					*inStdRecordMapTuple		= pStdRecordMapTuple;
				}
		

	} // try
	catch( sInt32 err )
	{
		siResult = err;
	}

	return( siResult );

} // Init


// ---------------------------------------------------------------------------
//	* BuildDefaultStdAttributeMap
// ---------------------------------------------------------------------------

sInt32 CLDAPConfigs::BuildDefaultStdAttributeMap ( void )
{
	sInt32		siResult		= eDSNoErr;
	sMapTuple   *pAttrMapTuple	= nil;
	sPtrString  *tempPtrString	= nil;

// note that we build this list in reverse order since it is easier to hook up the next
// pointers that way

	if (pStdAttributeMapTuple != nil)
	{
		//free up the standard attribute mapping table as created internally
		//if we have one in the config file
		CleanMapTuple(pStdAttributeMapTuple);
		pStdAttributeMapTuple = nil;
	}

	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDSNAttrVFSOpts)];
        ::strcpy(pAttrMapTuple->fStandard,kDSNAttrVFSOpts);
        
        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("vfsopts")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"vfsopts");
	        pAttrMapTuple->fNative->pNext = nil;
        }
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }

	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDS1AttrVFSLinkDir)];
        ::strcpy(pAttrMapTuple->fStandard,kDS1AttrVFSLinkDir);
        
        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("vfsdir")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"vfsdir");
	        pAttrMapTuple->fNative->pNext = nil;
        }
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }

	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDS1AttrVFSDumpFreq)];
        ::strcpy(pAttrMapTuple->fStandard,kDS1AttrVFSDumpFreq);
        
        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("vfsdumpfreq")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"vfsdumpfreq");
	        pAttrMapTuple->fNative->pNext = nil;
        }
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }

	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDS1AttrVFSPassNo)];
        ::strcpy(pAttrMapTuple->fStandard,kDS1AttrVFSPassNo);
        
        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("passno")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"passno");
	        pAttrMapTuple->fNative->pNext = nil;
        }
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }

	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDS1AttrVFSType)];
        ::strcpy(pAttrMapTuple->fStandard,kDS1AttrVFSType);
        
        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("vfstype")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"vfstype");
	        pAttrMapTuple->fNative->pNext = nil;
        }
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }

	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDSNAttrIPAddress)];
        ::strcpy(pAttrMapTuple->fStandard,kDSNAttrIPAddress);
        
        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("ipaddress")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"ipaddress");
	        pAttrMapTuple->fNative->pNext = nil;
        }
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }

	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDSNAttrDNSName)];
        ::strcpy(pAttrMapTuple->fStandard,kDSNAttrDNSName);
        
        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("dnsname")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"dnsname");
	        pAttrMapTuple->fNative->pNext = nil;
        }
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }

	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDSNAttrURLForNSL)];
        ::strcpy(pAttrMapTuple->fStandard,kDSNAttrURLForNSL);
        
        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("networklocurl")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"networklocurl");
	        pAttrMapTuple->fNative->pNext = nil;
        }
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }

	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDSNAttrURL)];
        ::strcpy(pAttrMapTuple->fStandard,kDSNAttrURL);
        
        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("urldata")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"urldata");
	        pAttrMapTuple->fNative->pNext = nil;
        }
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }

	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDSNAttrNBPEntry)];
        ::strcpy(pAttrMapTuple->fStandard,kDSNAttrNBPEntry);
        
        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("nbpdata")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"nbpdata");
	        pAttrMapTuple->fNative->pNext = nil;
        }
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }
	
	pAttrMapTuple = new sMapTuple;
	if ( pAttrMapTuple != nil )
	{
		::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
		pAttrMapTuple->fStandard = new char[1+::strlen(kDSNAttrPhoneNumber)];
		::strcpy(pAttrMapTuple->fStandard,kDSNAttrPhoneNumber);
		
		tempPtrString = new sPtrString;
		if ( tempPtrString != nil )
		{
			::memset( tempPtrString, 0, sizeof( sPtrString ) );
			tempPtrString->fName = new char[1+::strlen("phone")];
			::strcpy(tempPtrString->fName,"phone");
			tempPtrString->pNext = nil;
		}

		pAttrMapTuple->fNative = new sPtrString;
		if ( pAttrMapTuple->fNative != nil )
		{
			::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
			pAttrMapTuple->fNative->fName = new char[1+::strlen("telephone")];
			::strcpy(pAttrMapTuple->fNative->fName,"telephone");
			pAttrMapTuple->fNative->pNext = tempPtrString;
		}
		
		pAttrMapTuple->pNext = pStdAttributeMapTuple;
		pStdAttributeMapTuple = pAttrMapTuple;
		pAttrMapTuple = nil;
	}
	
	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDSNAttrRecordAlias)];
        ::strcpy(pAttrMapTuple->fStandard,kDSNAttrRecordAlias);
        
        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("aliasdata")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"aliasdata");
	        pAttrMapTuple->fNative->pNext = nil;
        }
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }
	
	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDSNAttrGroupMembership)];
        ::strcpy(pAttrMapTuple->fStandard,kDSNAttrGroupMembership);
        
        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("userlist")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"userlist");
	        pAttrMapTuple->fNative->pNext = nil;
        }
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }

	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDSNAttrGroup)];
        ::strcpy(pAttrMapTuple->fStandard,kDSNAttrGroup);
        
        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("grouplist")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"grouplist");
	        pAttrMapTuple->fNative->pNext = nil;
        }
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }

	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDS1AttrAuthenticationHint)];
        ::strcpy(pAttrMapTuple->fStandard,kDS1AttrAuthenticationHint);
        
        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("hint")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"hint");
	        pAttrMapTuple->fNative->pNext = nil;
        }
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }

	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDSNAttrEMailAddress)];
        ::strcpy(pAttrMapTuple->fStandard,kDSNAttrEMailAddress);
        
        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("mail")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"mail");
	        pAttrMapTuple->fNative->pNext = nil;
        }
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }

	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDS1AttrMailAttribute)];
        ::strcpy(pAttrMapTuple->fStandard,kDS1AttrMailAttribute);
        
        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("applemail")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"applemail");
	        pAttrMapTuple->fNative->pNext = nil;
        }
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }

	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDS1AttrUserShell)];
        ::strcpy(pAttrMapTuple->fStandard,kDS1AttrUserShell);
        
        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("shell")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"shell");
	        pAttrMapTuple->fNative->pNext = nil;
        }
        
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }

	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDSNAttrHomeDirectory)];
        ::strcpy(pAttrMapTuple->fStandard,kDSNAttrHomeDirectory);
        
        tempPtrString = new sPtrString;
        if ( tempPtrString != nil )
        {
        	::memset( tempPtrString, 0, sizeof( sPtrString ) );
	        tempPtrString->fName = new char[1+::strlen("home")];
	        ::strcpy(tempPtrString->fName,"home");
	        tempPtrString->pNext = nil;
        }

        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("homeloc")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"homeloc");
	        pAttrMapTuple->fNative->pNext = tempPtrString;
        }
        
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }

	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDS1AttrNFSHomeDirectory)];
        ::strcpy(pAttrMapTuple->fStandard,kDS1AttrNFSHomeDirectory);
        
        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("nfshome")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"nfshome");
	        pAttrMapTuple->fNative->pNext = nil;
        }
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }

	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDS1AttrPrimaryGroupID)];
        ::strcpy(pAttrMapTuple->fStandard,kDS1AttrPrimaryGroupID);
        
        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("groupid")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"groupid");
	        pAttrMapTuple->fNative->pNext = nil;
        }
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }

	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDS1AttrPassword)];
        ::strcpy(pAttrMapTuple->fStandard,kDS1AttrPassword);
        
        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("passwd")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"passwd");
	        pAttrMapTuple->fNative->pNext = nil;
        }
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }

	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDS1AttrUniqueID)];
        ::strcpy(pAttrMapTuple->fStandard,kDS1AttrUniqueID);
        
        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("unixid")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"unixid");
	        pAttrMapTuple->fNative->pNext = nil;
        }
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }

	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDS1AttrDistinguishedName)];
        ::strcpy(pAttrMapTuple->fStandard,kDS1AttrDistinguishedName);
        
        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("realname")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"realname");
	        pAttrMapTuple->fNative->pNext = nil;
        }
        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }

	pAttrMapTuple = new sMapTuple;
    if ( pAttrMapTuple != nil )
    {
        ::memset( pAttrMapTuple, 0, sizeof( sMapTuple ) );
        pAttrMapTuple->fStandard = new char[1+::strlen(kDSNAttrRecordName)];
        ::strcpy(pAttrMapTuple->fStandard,kDSNAttrRecordName);
        
        tempPtrString = new sPtrString;
        if ( tempPtrString != nil )
        {
        	::memset( tempPtrString, 0, sizeof( sPtrString ) );
	        tempPtrString->fName = new char[1+::strlen("sn")];
	        ::strcpy(tempPtrString->fName,"sn");
	        tempPtrString->pNext = nil;
        }

        pAttrMapTuple->fNative = new sPtrString;
        if ( pAttrMapTuple->fNative != nil )
        {
        	::memset( pAttrMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pAttrMapTuple->fNative->fName = new char[1+::strlen("cn")];
	        ::strcpy(pAttrMapTuple->fNative->fName,"cn");
	        pAttrMapTuple->fNative->pNext = tempPtrString;
        }
        
        tempPtrString = new sPtrString;
        if ( tempPtrString != nil )
        {
        	::memset( tempPtrString, 0, sizeof( sPtrString ) );
	        tempPtrString->fName = new char[1+::strlen("dn")];
	        ::strcpy(tempPtrString->fName,"dn");
	        tempPtrString->pNext = nil;
	        pAttrMapTuple->fNative->pNext->pNext = tempPtrString;
        }

        pAttrMapTuple->pNext = pStdAttributeMapTuple;
        pStdAttributeMapTuple = pAttrMapTuple;
        pAttrMapTuple = nil;
    }

	return( siResult );

} // BuildDefaultStdAttributeMap


// ---------------------------------------------------------------------------
//	* BuildDefaultStdRecordMap
// ---------------------------------------------------------------------------

sInt32 CLDAPConfigs::BuildDefaultStdRecordMap ( void )
{
	sInt32		siResult	= eDSNoErr;
	sMapTuple   *pRecMapTuple	= nil;

// note that we build this list in reverse order since it is easier to hook up the next
// pointers that way

	if (pStdRecordMapTuple != nil)
	{
		//free up the standard record mapping table as created internally
		//if we have one in the config file
		CleanMapTuple(pStdRecordMapTuple);
		pStdRecordMapTuple = nil;
	}

	pRecMapTuple = new sMapTuple;
    if ( pRecMapTuple != nil )
    {
        ::memset( pRecMapTuple, 0, sizeof( sMapTuple ) );
        pRecMapTuple->fStandard = new char[1+::strlen(kDSStdRecordTypeQTSServer)];
        ::strcpy(pRecMapTuple->fStandard,kDSStdRecordTypeQTSServer);
        
        pRecMapTuple->fNative = new sPtrString;
        if ( pRecMapTuple->fNative != nil )
        {
        	::memset( pRecMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pRecMapTuple->fNative->fName = new char[1+::strlen("ou=qtsserver, o=company name")];
	        ::strcpy(pRecMapTuple->fNative->fName,"ou=qtsserver, o=company name");
	        pRecMapTuple->fNative->pNext = nil;
        }
        pRecMapTuple->pNext = pStdRecordMapTuple;
        pStdRecordMapTuple = pRecMapTuple;
        pRecMapTuple = nil;
    }

	pRecMapTuple = new sMapTuple;
    if ( pRecMapTuple != nil )
    {
        ::memset( pRecMapTuple, 0, sizeof( sMapTuple ) );
        pRecMapTuple->fStandard = new char[1+::strlen(kDSStdRecordTypeLDAPServer)];
        ::strcpy(pRecMapTuple->fStandard,kDSStdRecordTypeLDAPServer);
        
        pRecMapTuple->fNative = new sPtrString;
        if ( pRecMapTuple->fNative != nil )
        {
        	::memset( pRecMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pRecMapTuple->fNative->fName = new char[1+::strlen("ou=ldapserver, o=company name")];
	        ::strcpy(pRecMapTuple->fNative->fName,"ou=ldapserver, o=company name");
	        pRecMapTuple->fNative->pNext = nil;
        }
        pRecMapTuple->pNext = pStdRecordMapTuple;
        pStdRecordMapTuple = pRecMapTuple;
        pRecMapTuple = nil;
    }

	pRecMapTuple = new sMapTuple;
    if ( pRecMapTuple != nil )
    {
        ::memset( pRecMapTuple, 0, sizeof( sMapTuple ) );
        pRecMapTuple->fStandard = new char[1+::strlen(kDSStdRecordTypeWebServer)];
        ::strcpy(pRecMapTuple->fStandard,kDSStdRecordTypeWebServer);
        
        pRecMapTuple->fNative = new sPtrString;
        if ( pRecMapTuple->fNative != nil )
        {
        	::memset( pRecMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pRecMapTuple->fNative->fName = new char[1+::strlen("ou=webserver, o=company name")];
	        ::strcpy(pRecMapTuple->fNative->fName,"ou=webserver, o=company name");
	        pRecMapTuple->fNative->pNext = nil;
        }
        pRecMapTuple->pNext = pStdRecordMapTuple;
        pStdRecordMapTuple = pRecMapTuple;
        pRecMapTuple = nil;
    }

	pRecMapTuple = new sMapTuple;
    if ( pRecMapTuple != nil )
    {
        ::memset( pRecMapTuple, 0, sizeof( sMapTuple ) );
        pRecMapTuple->fStandard = new char[1+::strlen(kDSStdRecordTypeNFS)];
        ::strcpy(pRecMapTuple->fStandard,kDSStdRecordTypeNFS);
        
        pRecMapTuple->fNative = new sPtrString;
        if ( pRecMapTuple->fNative != nil )
        {
        	::memset( pRecMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pRecMapTuple->fNative->fName = new char[1+::strlen("ou=nfs, o=company name")];
	        ::strcpy(pRecMapTuple->fNative->fName,"ou=nfs, o=company name");
	        pRecMapTuple->fNative->pNext = nil;
        }
        pRecMapTuple->pNext = pStdRecordMapTuple;
        pStdRecordMapTuple = pRecMapTuple;
        pRecMapTuple = nil;
    }

	pRecMapTuple = new sMapTuple;
    if ( pRecMapTuple != nil )
    {
        ::memset( pRecMapTuple, 0, sizeof( sMapTuple ) );
        pRecMapTuple->fStandard = new char[1+::strlen(kDSStdRecordTypeSMBServer)];
        ::strcpy(pRecMapTuple->fStandard,kDSStdRecordTypeSMBServer);
        
        pRecMapTuple->fNative = new sPtrString;
        if ( pRecMapTuple->fNative != nil )
        {
        	::memset( pRecMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pRecMapTuple->fNative->fName = new char[1+::strlen("ou=smbserver, o=company name")];
	        ::strcpy(pRecMapTuple->fNative->fName,"ou=smbserver, o=company name");
	        pRecMapTuple->fNative->pNext = nil;
        }
        pRecMapTuple->pNext = pStdRecordMapTuple;
        pStdRecordMapTuple = pRecMapTuple;
        pRecMapTuple = nil;
    }

	pRecMapTuple = new sMapTuple;
    if ( pRecMapTuple != nil )
    {
        ::memset( pRecMapTuple, 0, sizeof( sMapTuple ) );
        pRecMapTuple->fStandard = new char[1+::strlen(kDSStdRecordTypeFTPServer)];
        ::strcpy(pRecMapTuple->fStandard,kDSStdRecordTypeFTPServer);
        
        pRecMapTuple->fNative = new sPtrString;
        if ( pRecMapTuple->fNative != nil )
        {
        	::memset( pRecMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pRecMapTuple->fNative->fName = new char[1+::strlen("ou=ftpserver, o=company name")];
	        ::strcpy(pRecMapTuple->fNative->fName,"ou=ftpserver, o=company name");
	        pRecMapTuple->fNative->pNext = nil;
        }
        pRecMapTuple->pNext = pStdRecordMapTuple;
        pStdRecordMapTuple = pRecMapTuple;
        pRecMapTuple = nil;
    }

	pRecMapTuple = new sMapTuple;
    if ( pRecMapTuple != nil )
    {
        ::memset( pRecMapTuple, 0, sizeof( sMapTuple ) );
        pRecMapTuple->fStandard = new char[1+::strlen(kDSStdRecordTypeAFPServer)];
        ::strcpy(pRecMapTuple->fStandard,kDSStdRecordTypeAFPServer);
        
        pRecMapTuple->fNative = new sPtrString;
        if ( pRecMapTuple->fNative != nil )
        {
        	::memset( pRecMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pRecMapTuple->fNative->fName = new char[1+::strlen("ou=afpserver, o=company name")];
	        ::strcpy(pRecMapTuple->fNative->fName,"ou=afpserver, o=company name");
	        pRecMapTuple->fNative->pNext = nil;
        }
        pRecMapTuple->pNext = pStdRecordMapTuple;
        pStdRecordMapTuple = pRecMapTuple;
        pRecMapTuple = nil;
    }

	pRecMapTuple = new sMapTuple;
    if ( pRecMapTuple != nil )
    {
        ::memset( pRecMapTuple, 0, sizeof( sMapTuple ) );
        pRecMapTuple->fStandard = new char[1+::strlen(kDSStdRecordTypePrinters)];
        ::strcpy(pRecMapTuple->fStandard,kDSStdRecordTypePrinters);
        
        pRecMapTuple->fNative = new sPtrString;
        if ( pRecMapTuple->fNative != nil )
        {
        	::memset( pRecMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pRecMapTuple->fNative->fName = new char[1+::strlen("ou=printers, o=company name")];
	        ::strcpy(pRecMapTuple->fNative->fName,"ou=printers, o=company name");
	        pRecMapTuple->fNative->pNext = nil;
        }
        pRecMapTuple->pNext = pStdRecordMapTuple;
        pStdRecordMapTuple = pRecMapTuple;
        pRecMapTuple = nil;
    }

	pRecMapTuple = new sMapTuple;
    if ( pRecMapTuple != nil )
    {
        ::memset( pRecMapTuple, 0, sizeof( sMapTuple ) );
        pRecMapTuple->fStandard = new char[1+::strlen(kDSStdRecordTypeMachines)];
        ::strcpy(pRecMapTuple->fStandard,kDSStdRecordTypeMachines);
        
        pRecMapTuple->fNative = new sPtrString;
        if ( pRecMapTuple->fNative != nil )
        {
        	::memset( pRecMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pRecMapTuple->fNative->fName = new char[1+::strlen("ou=machines, o=company name")];
	        ::strcpy(pRecMapTuple->fNative->fName,"ou=machines, o=company name");
	        pRecMapTuple->fNative->pNext = nil;
        }
        pRecMapTuple->pNext = pStdRecordMapTuple;
        pStdRecordMapTuple = pRecMapTuple;
        pRecMapTuple = nil;
    }

	pRecMapTuple = new sMapTuple;
    if ( pRecMapTuple != nil )
    {
        ::memset( pRecMapTuple, 0, sizeof( sMapTuple ) );
        pRecMapTuple->fStandard = new char[1+::strlen(kDSStdRecordTypeMounts)];
        ::strcpy(pRecMapTuple->fStandard,kDSStdRecordTypeMounts);
        
        pRecMapTuple->fNative = new sPtrString;
        if ( pRecMapTuple->fNative != nil )
        {
        	::memset( pRecMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pRecMapTuple->fNative->fName = new char[1+::strlen("ou=mounts, o=company name")];
	        ::strcpy(pRecMapTuple->fNative->fName,"ou=mounts, o=company name");
	        pRecMapTuple->fNative->pNext = nil;
        }
        pRecMapTuple->pNext = pStdRecordMapTuple;
        pStdRecordMapTuple = pRecMapTuple;
        pRecMapTuple = nil;
    }

	pRecMapTuple = new sMapTuple;
    if ( pRecMapTuple != nil )
    {
        ::memset( pRecMapTuple, 0, sizeof( sMapTuple ) );
        pRecMapTuple->fStandard = new char[1+::strlen(kDSStdRecordTypeGroupAliases)];
        ::strcpy(pRecMapTuple->fStandard,kDSStdRecordTypeGroupAliases);
        
        pRecMapTuple->fNative = new sPtrString;
        if ( pRecMapTuple->fNative != nil )
        {
        	::memset( pRecMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pRecMapTuple->fNative->fName = new char[1+::strlen("ou=groupaliases, o=company name")];
	        ::strcpy(pRecMapTuple->fNative->fName,"ou=groupaliases, o=company name");
	        pRecMapTuple->fNative->pNext = nil;
        }
        pRecMapTuple->pNext = pStdRecordMapTuple;
        pStdRecordMapTuple = pRecMapTuple;
        pRecMapTuple = nil;
    }

	pRecMapTuple = new sMapTuple;
    if ( pRecMapTuple != nil )
    {
        ::memset( pRecMapTuple, 0, sizeof( sMapTuple ) );
        pRecMapTuple->fStandard = new char[1+::strlen(kDSStdRecordTypeUserAliases)];
        ::strcpy(pRecMapTuple->fStandard,kDSStdRecordTypeUserAliases);
        
        pRecMapTuple->fNative = new sPtrString;
        if ( pRecMapTuple->fNative != nil )
        {
        	::memset( pRecMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pRecMapTuple->fNative->fName = new char[1+::strlen("ou=peoplealiases, o=company name")];
	        ::strcpy(pRecMapTuple->fNative->fName,"ou=peoplealiases, o=company name");
	        pRecMapTuple->fNative->pNext = nil;
        }
        pRecMapTuple->pNext = pStdRecordMapTuple;
        pStdRecordMapTuple = pRecMapTuple;
        pRecMapTuple = nil;
    }

	pRecMapTuple = new sMapTuple;
    if ( pRecMapTuple != nil )
    {
        ::memset( pRecMapTuple, 0, sizeof( sMapTuple ) );
        pRecMapTuple->fStandard = new char[1+::strlen(kDSStdRecordTypeGroups)];
        ::strcpy(pRecMapTuple->fStandard,kDSStdRecordTypeGroups);
        
        pRecMapTuple->fNative = new sPtrString;
        if ( pRecMapTuple->fNative != nil )
        {
        	::memset( pRecMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pRecMapTuple->fNative->fName = new char[1+::strlen("ou=groups, o=company name")];
	        ::strcpy(pRecMapTuple->fNative->fName,"ou=groups, o=company name");
	        pRecMapTuple->fNative->pNext = nil;
        }
        pRecMapTuple->pNext = pStdRecordMapTuple;
        pStdRecordMapTuple = pRecMapTuple;
        pRecMapTuple = nil;
    }

	pRecMapTuple = new sMapTuple;
    if ( pRecMapTuple != nil )
    {
        ::memset( pRecMapTuple, 0, sizeof( sMapTuple ) );
        pRecMapTuple->fStandard = new char[1+::strlen(kDSStdRecordTypeUsers)];
        ::strcpy(pRecMapTuple->fStandard,kDSStdRecordTypeUsers);
        
        pRecMapTuple->fNative = new sPtrString;
        if ( pRecMapTuple->fNative != nil )
        {
        	::memset( pRecMapTuple->fNative, 0, sizeof( sPtrString ) );
	        pRecMapTuple->fNative->fName = new char[1+::strlen("ou=people, o=company name")];
	        ::strcpy(pRecMapTuple->fNative->fName,"ou=people, o=company name");
	        pRecMapTuple->fNative->pNext = nil;
        }
        pRecMapTuple->pNext = pStdRecordMapTuple;
        pStdRecordMapTuple = pRecMapTuple;
        pRecMapTuple = nil;
    }

	return( siResult );

} // BuildDefaultStdRecordMap

// ---------------------------------------------------------------------------
//	* CleanMapTuple
// ---------------------------------------------------------------------------

sInt32 CLDAPConfigs::CleanMapTuple ( sMapTuple *inMapTuple )
{
	sInt32		siResult	= eDSNoErr;
	sMapTuple   *pMapTuple	= nil;
	sPtrString  *pPtrString	= nil;

	pMapTuple = inMapTuple;
	while (pMapTuple != nil)
	{
		inMapTuple = pMapTuple->pNext;
		if (pMapTuple->fStandard != nil)
		{
			delete( pMapTuple->fStandard );
		}
		pPtrString = pMapTuple->fNative;
		while (pPtrString != nil)
		{
			pMapTuple->fNative = pPtrString->pNext;
			if (pPtrString->fName != nil)
			{
				delete( pPtrString->fName );
			}
			pPtrString->pNext = nil;
			delete( pPtrString );
			pPtrString = pMapTuple->fNative;
		}
		pMapTuple->pNext = nil;
		delete( pMapTuple );
		pMapTuple = inMapTuple;
	}

	return( siResult );

} // CleanMapTuple

// ---------------------------------------------------------------------------
//	* ReadXMLConfig
// ---------------------------------------------------------------------------

sInt32 CLDAPConfigs::ReadXMLConfig ( void )
{
	sInt32					siResult			= eDSNoErr;
	CFURLRef				configFileURL		= nil;
	CFURLRef				configFileCorruptedURL	= nil;
	CFDataRef				xmlData;
	bool					bReadFile			= false;
	bool					bCorruptedFile			= false;
	bool					bWroteFile			= false;
	register CFIndex		iPath;
	CFArrayRef				aPaths				= nil;
	char					string[ PATH_MAX ];
	char					string2[ PATH_MAX ];
    struct stat				statResult;
	CFMutableDictionaryRef			configDict;
        CFStringRef			cfStringRef;
        sInt32				errorCode		= 0;
        CFStringRef			sBase			= nil;
        CFStringRef			sPath			= nil;
        CFStringRef			sCorruptedPath;

//Config data is read from a XML file
//KW eventually use Version from XML file to check against the code here?
//Steps in the process:
//1- see if the file exists
//2- if it exists then try to read it
//3- if existing file is corrupted then rename it and save it while creating a new default file
//4- if file doesn't exist then create a new default file - make sure directories exist/if not create them

    try
    {	
        // Get the local library search path -- only expect a single one
	aPaths = ::CFCopySearchPathForDirectoriesInDomains( kCFLibraryDirectory, kCFLocalDomainMask, true );
	if ( aPaths != nil )
	{
            iPath = ::CFArrayGetCount( aPaths );
            if ( iPath != 0 )
            {
                // count down here if more that the Local directory is specified
                // ie. in Local ( or user's home directory ).
                // for now reality is that there is NO countdown
                while (( iPath-- ) && (!bReadFile))
                {
                    configFileURL = (CFURLRef)::CFArrayGetValueAtIndex( aPaths, iPath );

                    // Append the subpath.
                    sBase = ::CFURLCopyFileSystemPath( configFileURL, kCFURLPOSIXPathStyle );
                    sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s" ), sBase, "/Preferences/DirectoryService/DSLDAPPlugInConfig.clpi" );

                    ::memset(string,0,PATH_MAX);
                    ::CFStringGetCString( sPath, string, sizeof( string ), kCFStringEncodingMacRoman );
                    CShared::LogIt( 0x0F, (char *)"Checking for LDAP XML config file:" );
                    CShared::LogIt( 0x0F, string );

                    // Convert it back into a CFURL.
                    configFileURL = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, sPath, kCFURLPOSIXPathStyle, false );
                    CFRelease( sPath ); // build with Create so okay to dealloac here
                    sPath = nil;

                    //step 1- see if the file exists
                    //if not then make sure the directories exist or create them
                    //then create a new file if necessary
                    siResult = ::stat( string, &statResult );
                    //if file does not exist
                    if (siResult != eDSNoErr)
                    {
                        //move down the path from the system defined local directory and check if it exists
                        //if not create it
                        sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s" ), sBase, "/Preferences" );
                        ::memset(string2,0,PATH_MAX);
                        ::CFStringGetCString( sPath, string2, sizeof( string2 ), kCFStringEncodingMacRoman );
                        siResult = ::stat( string2, &statResult );
                        //if first sub directory does not exist
                        if (siResult != eDSNoErr)
                        {
                            ::mkdir( string2 , 0775 );
							::chmod( string2, 0775 ); //above 0775 doesn't seem to work - looks like umask modifies it
                        }
                        CFRelease( sPath ); // build with Create so okay to dealloac here
                        sPath = nil;
                        //next subdirectory
                        sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s" ), sBase, "/Preferences/DirectoryService" );
                        ::memset(string2,0,PATH_MAX);
                        ::CFStringGetCString( sPath, string2, sizeof( string2 ), kCFStringEncodingMacRoman );
                        siResult = ::stat( string2, &statResult );
                        //if second sub directory does not exist
                        if (siResult != eDSNoErr)
                        {
                            ::mkdir( string2 , 0775 );
							::chmod( string2, 0775 ); //above 0775 doesn't seem to work - looks like umask modifies it
                        }
                        CFRelease( sPath ); // build with Create so okay to dealloac here
                        sPath = nil;
                        
                        //create a new dictionary for the file
                        configDict = CFDictionaryCreateMutable( kCFAllocatorDefault,
                                                                0,
                                                                &kCFTypeDictionaryKeyCallBacks,
                                                                &kCFTypeDictionaryValueCallBacks );
                        
                        cfStringRef = CFStringCreateWithCString(kCFAllocatorDefault, "DSLDAPPlugIn Version 1.5", kCFStringEncodingMacRoman);
                        CFDictionarySetValue( configDict, CFSTR( kXMLLDAPVersionKey ), cfStringRef );
                        CFRelease(cfStringRef);
						cfStringRef = nil;
                        //build the standard types in here so that there is something to start with
                        siResult = BuildDefaultStdAttributeMap();
                        siResult = BuildDefaultStdRecordMap();
                        
                        //now populate the dictionary with the std mappings
                        siResult = AddDefaultArrays(configDict);
                        
                    CShared::LogIt( 0x0F, (char *)"Created a new LDAP XML config file since it did not exist" );
                        //convert the dict into a XML blob
                        xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, configDict);
                        
                        //write the XML to the config file
                        siResult = CFURLWriteDataAndPropertiesToResource( configFileURL,
                                                                            xmlData,
                                                                            NULL,
                                                                            &errorCode);
                        //KW check the error code and the result?
                        
                        CFRelease(configDict);
						configDict = nil;
                        CFRelease(xmlData);
						xmlData = nil;
                        
                    } // file does not exist so creating one
					chmod( string, S_IRUSR | S_IWUSR );
                    // Read the XML property list file
                    bReadFile = CFURLCreateDataAndPropertiesFromResource(
                                                                            kAllocatorDefault,
                                                                            configFileURL,
                                                                            &xmlData,          // place to put file data
                                                                            NULL,           
                                                                            NULL,
                                                                            &siResult);
							   
                } // while (( iPath-- ) && (!bReadFile))
            } // if ( iPath != 0 )
			
            CFRelease(aPaths); // seems okay since Created above
			aPaths = nil;
	}// if ( aPaths != nil )

	if (bReadFile)
	{
            fXMLData = xmlData;
            //check if this XML blob is a property list and can be made into a dictionary
            if (!VerifyXML())
            {
                //if it is not then say the file is corrupted and save off the corrupted file
                CShared::LogIt( 0x0F, (char *)"LDAP XML config file is corrupted" );
                bCorruptedFile = true;
                //here we need to make a backup of the file - why? - because

                // Append the subpath.
                sCorruptedPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s" ), sBase, "/Preferences/DirectoryService/DSLDAPPlugInConfigCorrupted.clpi" );

                // Convert it into a CFURL.
                configFileCorruptedURL = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, sCorruptedPath, kCFURLPOSIXPathStyle, false );
                CFRelease( sCorruptedPath ); // build with Create so okay to dealloac here
                sCorruptedPath = nil;

                //write the XML to the corrupted copy of the config file
                    bWroteFile = CFURLWriteDataAndPropertiesToResource( configFileCorruptedURL,
                                                                        xmlData,
                                                                        NULL,
                                                                        &errorCode);
                    //KW check the error code and the result?
                
                CFRelease(xmlData);
				xmlData = nil;
            }
	}
	else //existing file is unreadable
	{
            CShared::LogIt( 0x0F, (char *)"LDAP XML config file is unreadable" );
            bCorruptedFile = true;
            //siResult = eDSPlugInConfigFileError; // not an error since we will attempt to recover
	}
        
        if (bCorruptedFile)
        {
            //create a new dictionary for the file
            configDict = CFDictionaryCreateMutable( kCFAllocatorDefault,
                                                    0,
                                                    &kCFTypeDictionaryKeyCallBacks,
                                                    &kCFTypeDictionaryValueCallBacks );
            
            cfStringRef = CFStringCreateWithCString(kCFAllocatorDefault, "DSLDAPPlugIn Version 1.5", kCFStringEncodingMacRoman);
            CFDictionarySetValue( configDict, CFSTR( kXMLLDAPVersionKey ), cfStringRef );
            CFRelease(cfStringRef);
			cfStringRef = nil;
            //check the std defaults if by off chance the above write of a new file was corrupted
            if (pStdAttributeMapTuple == nil)
            {
                siResult = BuildDefaultStdAttributeMap();
            }
            if (pStdRecordMapTuple == nil)
            {
                siResult = BuildDefaultStdRecordMap();
            }
            
            //now populate the dictionary with the std mappings
            //code makes sure that the arrays don't already exist although doesn't matter since configDict is new
            siResult = AddDefaultArrays(configDict); 
            
CShared::LogIt( 0x0F, (char *)"Writing a new LDAP XML config file" );
            //convert the dict into a XML blob
            xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, configDict);
            
            //assume that the XML blob is good since we created it here
            fXMLData = xmlData;

            //write the XML to the config file
            siResult = CFURLWriteDataAndPropertiesToResource( configFileURL,
                                                                xmlData,
                                                                NULL,
                                                                &errorCode);
            //KW check the error code and the result?
            
            CFRelease(configDict);
			configDict = nil;
        }
		
    } // try
    catch( sInt32 err )
    {
        siResult = err;
    }
    
    if (configFileURL != nil)
    {
    	CFRelease(configFileURL); // seems okay to dealloc since Create used and done with it now
        configFileURL = nil;
    }
    
    if (configFileCorruptedURL != nil)
    {
    	CFRelease(configFileCorruptedURL); // seems okay to dealloc since Create used and done with it now
        configFileCorruptedURL = nil;
    }
    
    if (sBase != nil)
    {
    	CFRelease( sBase ); // built with Copy so okay to dealloc
    	sBase = nil;
    }

    return( siResult );

} // ReadXMLConfig

// ---------------------------------------------------------------------------
//	* WriteXMLConfig
// ---------------------------------------------------------------------------

sInt32 CLDAPConfigs::WriteXMLConfig ( void )
{
	sInt32					siResult			= eDSNoErr;
	CFURLRef				configFileURL;
	bool					bWroteFile			= false;
	register CFIndex		iPath;
	CFArrayRef				aPaths				= nil;
	char					string[ PATH_MAX ];
	struct stat				statResult;
	sInt32				errorCode		= 0;

	//Config data is written to a XML file
	//Steps in the process:
	//1- see if the file exists
	//2- if it exists then overwrite it
	//3- rename existing file and save it while creating a new file
	//4- if file doesn't exist then create a new default file - make sure directories exist/if not create them
	//make sure file permissions are root only

	try
	{
		// Get the local library search path -- only expect a single one
		aPaths = ::CFCopySearchPathForDirectoriesInDomains( kCFLibraryDirectory, kCFLocalDomainMask, true );
		if ( aPaths != nil )
		{
			iPath = ::CFArrayGetCount( aPaths );
			if ( iPath != 0 )
			{
				// count down here if more that the Local directory is specified
				// ie. in Local ( or user's home directory ).
				// for now reality is that there is NO countdown
				while (( iPath-- ) && (!bWroteFile))
				{
					configFileURL = (CFURLRef)::CFArrayGetValueAtIndex( aPaths, iPath );
					CFStringRef	sBase, sPath;

					// Append the subpath.
					sBase = ::CFURLCopyFileSystemPath( configFileURL, kCFURLPOSIXPathStyle );
					sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s" ), sBase, "/Preferences/DirectoryService/DSLDAPPlugInConfig.clpi" );

					::memset(string,0,PATH_MAX);
					::CFStringGetCString( sPath, string, sizeof( string ), kCFStringEncodingMacRoman );
					CShared::LogIt( 0x0F, (char *)"Checking for LDAP XML config file:" );
					CShared::LogIt( 0x0F, string );

					// Convert it back into a CFURL.
					configFileURL = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, sPath, kCFURLPOSIXPathStyle, false );
					CFRelease( sPath ); // build with Create so okay to dealloac here
					sPath = nil;

					//step 1- see if the file exists
					//if not then make sure the directories exist or create them
					//then write the file
					siResult = ::stat( string, &statResult );
                        
					//if file does not exist
					if (siResult != eDSNoErr)
					{
						siResult = eDSNoErr;
						//move down the path from the system defined local directory and check if it exists
						//if not create it
						sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s" ), sBase, "/Preferences" );
						::memset(string,0,PATH_MAX);
						::CFStringGetCString( sPath, string, sizeof( string ), kCFStringEncodingMacRoman );
						siResult = ::stat( string, &statResult );
						//if first sub directory does not exist
						if (siResult != eDSNoErr)
						{
							siResult = eDSNoErr;
							::mkdir( string , 0775 );
							::chmod( string, 0775 ); //above 0775 doesn't seem to work - looks like umask modifies it
						}
						CFRelease( sPath ); // build with Create so okay to dealloac here
						sPath = nil;
						//next subdirectory
						sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s" ), sBase, "/Preferences/DirectoryService" );
						::memset(string,0,PATH_MAX);
						::CFStringGetCString( sPath, string, sizeof( string ), kCFStringEncodingMacRoman );
						siResult = ::stat( string, &statResult );
						//if second sub directory does not exist
						if (siResult != eDSNoErr)
						{
							siResult = eDSNoErr;
							::mkdir( string , 0775 );
							::chmod( string, 0775 ); //above 0775 doesn't seem to work - looks like umask modifies it
						}
						CFRelease( sPath ); // build with Create so okay to dealloac here
						sPath = nil;

					} // file does not exist so checking directory path to enable write of a new file

					//now write the updated file
					if (fXMLData != nil)
					{
						//write the XML to the config file
						bWroteFile = CFURLWriteDataAndPropertiesToResource( configFileURL,
																		fXMLData,
																		NULL,
																		&errorCode);
						//check the error code and the result?
					}

					CFRelease( sBase ); // built with Copy so okay to dealloc
					sBase = nil;

					CFRelease(configFileURL); // seems okay to dealloc since Create used and done with it now
					configFileURL = nil;
				} // while (( iPath-- ) && (!bWroteFile))
			} // if ( iPath != 0 )

			CFRelease(aPaths); // seems okay since Created above
			aPaths = nil;
		}// if ( aPaths != nil )

            
		if (bWroteFile)
		{
			CShared::LogIt( 0x0F, (char *)"Have written the LDAP XML config file:" );
			CShared::LogIt( 0x0F, string );
			siResult = eDSNoErr;
		}
		else
		{
			CShared::LogIt( 0x0F, (char *)"LDAP XML config file has NOT been written" );
			CShared::LogIt( 0x0F, (char *)"Update to LDAP Config File Failed" );
			siResult = eDSPlugInConfigFileError;
		}
		
	} // try
	catch( sInt32 err )
	{
		siResult = err;
	}
	return( siResult );

} // WriteXMLConfig

// ---------------------------------------------------------------------------
//	* SetXMLConfig
// ---------------------------------------------------------------------------

sInt32 CLDAPConfigs::SetXMLConfig ( CFDataRef xmlData )
{
	CFDataRef currentXMLData = fXMLData;
	
	fXMLData = xmlData;
	if (VerifyXML())
	{
		// looks ok, let's use it
		if (currentXMLData != nil)
		{
			CFRelease(currentXMLData);
			currentXMLData = nil;
		}
		CFRetain(fXMLData);
		return eDSNoErr;
	}
	else
	{
		// go back to what we had
		fXMLData = currentXMLData;
		
		return eDSInvalidPlugInConfigData;
	}
}


// ---------------------------------------------------------------------------
//	* GetXMLConfig
// ---------------------------------------------------------------------------

CFDataRef CLDAPConfigs::GetXMLConfig ( void )
{
	return fXMLData;
}


sInt32 CLDAPConfigs::AddDefaultArrays ( CFMutableDictionaryRef inDict )
{
	CFMutableArrayRef			cfArrayRef			= nil;
	CFMutableDictionaryRef			cfDictRef			= nil;
	CFIndex					cfMapCount			= 0;
	CFIndex					cfNativeMapCount	= 0;
	CFStringRef				cfStringRef			= nil;
	CFMutableArrayRef			cfNativeArrayRef	= nil;
	sMapTuple			   *pMapTuple			= nil;
	sInt32					siResult			= eDSNoErr;
        sPtrString			   *pPtrString			= nil;
        

    // if this already exists in the dictionary then remove it
    if ( CFDictionaryContainsKey( inDict, CFSTR( kXMLDefaultAttrTypeMapArrayKey ) ) )
    {
        //KW is this sufficient ie. does remove do the release as well
        CFDictionaryRemoveValue( inDict, CFSTR( kXMLDefaultAttrTypeMapArrayKey ) );
    }

    //check if this already exists in the dictionary
    if ( !CFDictionaryContainsKey( inDict, CFSTR( kXMLDefaultAttrTypeMapArrayKey ) ) )
    {

        //create the def attr array and add to the dictionary
        cfArrayRef = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        CFDictionarySetValue( inDict, CFSTR(kXMLDefaultAttrTypeMapArrayKey), cfArrayRef);
        //release then get from the dictionary
        CFRelease(cfArrayRef);
        cfArrayRef = nil;
        cfArrayRef = (CFMutableArrayRef)CFDictionaryGetValue( inDict, CFSTR( kXMLDefaultAttrTypeMapArrayKey ) );
        
        
        cfMapCount = 0;
        //go through the default attr mappings and add them to the array
        pMapTuple = pStdAttributeMapTuple;
        while (pMapTuple != nil)
        {
            cfDictRef = CFDictionaryCreateMutable(	kCFAllocatorDefault,
                                                    0,
                                                    &kCFTypeDictionaryKeyCallBacks,
                                                    &kCFTypeDictionaryValueCallBacks );
            CFArrayInsertValueAtIndex(cfArrayRef, cfMapCount, cfDictRef);
            
            if (pMapTuple->fStandard != nil)
            {
                cfStringRef = CFStringCreateWithCString(kCFAllocatorDefault, pMapTuple->fStandard, kCFStringEncodingMacRoman);
                CFDictionarySetValue( cfDictRef, CFSTR(kXMLStdNameKey), cfStringRef);
                CFRelease(cfStringRef);
				cfStringRef = nil;
                //next index value
                cfMapCount++;
            }
            cfNativeArrayRef = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
            CFDictionarySetValue( cfDictRef, CFSTR(kXMLNativeMapArrayKey), cfNativeArrayRef);
            //release then get from the dictionary
            CFRelease(cfNativeArrayRef);
            cfNativeArrayRef = nil;
            cfNativeArrayRef = (CFMutableArrayRef)CFDictionaryGetValue( cfDictRef, CFSTR( kXMLNativeMapArrayKey ) );
            cfNativeMapCount = 0;
            pPtrString = pMapTuple->fNative;
            while (pPtrString != nil)
            {
                if (pPtrString->fName != nil)
                {
                    cfStringRef = CFStringCreateWithCString(kCFAllocatorDefault, pPtrString->fName, kCFStringEncodingMacRoman);
                    CFArrayInsertValueAtIndex(cfNativeArrayRef, cfNativeMapCount, cfStringRef);
					CFRelease(cfStringRef);
					cfStringRef = nil;
                   //next index value
                    cfNativeMapCount++;
                }
                pPtrString = pPtrString->pNext;
            }
            pMapTuple = pMapTuple->pNext;
			if (cfDictRef != nil)
			{
				CFRelease(cfDictRef);
				cfDictRef = nil;
			}
        }
    }
    
    // if this already exists in the dictionary then remove it
    if ( CFDictionaryContainsKey( inDict, CFSTR( kXMLDefaultRecordTypeMapArrayKey ) ) )
    {
        //KW is this sufficient ie. does remove do the release as well
        CFDictionaryRemoveValue( inDict, CFSTR( kXMLDefaultRecordTypeMapArrayKey ) );
    }
    
    //check if this already exists in the dictionary
    if ( !CFDictionaryContainsKey( inDict, CFSTR( kXMLDefaultRecordTypeMapArrayKey ) ) )
    {
    
        //create the def record array and add to the dictionary
        cfArrayRef = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        CFDictionarySetValue( inDict, CFSTR(kXMLDefaultRecordTypeMapArrayKey), cfArrayRef);
        //release then get from the dictionary
        CFRelease(cfArrayRef);
        cfArrayRef = nil;
        cfArrayRef = (CFMutableArrayRef)CFDictionaryGetValue( inDict, CFSTR( kXMLDefaultRecordTypeMapArrayKey ) );
        
        
        cfMapCount = 0;
        //go through the default record mappings and add them to the array
        pMapTuple = pStdRecordMapTuple;
        while (pMapTuple != nil)
        {
            cfDictRef = CFDictionaryCreateMutable(	kCFAllocatorDefault,
                                                    0,
                                                    &kCFTypeDictionaryKeyCallBacks,
                                                    &kCFTypeDictionaryValueCallBacks );
            CFArrayInsertValueAtIndex(cfArrayRef, cfMapCount, cfDictRef);
            
            if (pMapTuple->fStandard != nil)
            {
                cfStringRef = CFStringCreateWithCString(kCFAllocatorDefault, pMapTuple->fStandard, kCFStringEncodingMacRoman);
                CFDictionarySetValue( cfDictRef, CFSTR(kXMLStdNameKey), cfStringRef);
                CFRelease(cfStringRef);
				cfStringRef = nil;
                //next index value
                cfMapCount++;
            }
            cfNativeArrayRef = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
            CFDictionarySetValue( cfDictRef, CFSTR(kXMLNativeMapArrayKey), cfNativeArrayRef);
            //release then get from the dictionary
            CFRelease(cfNativeArrayRef);
            cfNativeArrayRef = nil;
            cfNativeArrayRef = (CFMutableArrayRef)CFDictionaryGetValue( cfDictRef, CFSTR( kXMLNativeMapArrayKey ) );
            cfNativeMapCount = 0;
            pPtrString = pMapTuple->fNative;
            while (pPtrString != nil)
            {
                if (pPtrString->fName != nil)
                {
                    cfStringRef = CFStringCreateWithCString(kCFAllocatorDefault, pPtrString->fName, kCFStringEncodingMacRoman);
                    CFArrayInsertValueAtIndex(cfNativeArrayRef, cfNativeMapCount, cfStringRef);
					CFRelease(cfStringRef);
					cfStringRef = nil;
                    //next index value
                    cfNativeMapCount++;
                }
                pPtrString = pPtrString->pNext;
            }
            pMapTuple = pMapTuple->pNext;
			if (cfDictRef != nil)
			{
				CFRelease(cfDictRef);
				cfDictRef = nil;
			}
        }
    }
    
    return siResult;
    
} // AddDefaultArrays

// ---------------------------------------------------------------------------
//	* VerifyXML
// ---------------------------------------------------------------------------

bool CLDAPConfigs::VerifyXML ( void )
{
    bool			verified		= false;
    CFStringRef			errorString;
    CFPropertyListRef		configPropertyList;
//    char				   *configVersion		= nil;
//KW need to add in check on the version string

    if (fXMLData != nil)
    {
        // extract the config dictionary from the XML data.
        configPropertyList = CFPropertyListCreateFromXMLData( kAllocatorDefault,
                                fXMLData,
                                kCFPropertyListImmutable, 
                               &errorString);
        if (configPropertyList != nil )
        {
            //make the propertylist a dict
            if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
            {
                    verified = true;
            }
            CFRelease(configPropertyList);
			configPropertyList = nil;
        }
    }
    
    return( verified);
    
} // VerifyXML

sInt32 CLDAPConfigs::ConfigLDAPServers ( void )
{
	sInt32					siResult			= eDSNoErr;
	CFStringRef				errorString;
	CFPropertyListRef		configPropertyList	= nil;
	CFMutableDictionaryRef	configDict			= nil;
	CFArrayRef				cfArrayRef			= nil;
	CFIndex					cfConfigCount		= 0;
	char					string[ PATH_MAX ];
	CFDataRef				xmlData				= nil;
	char				   *configVersion		= nil;
	CFStringRef				cfStringRef;

	try
	{	
		if (fXMLData != nil)
		{
			// extract the config dictionary from the XML data.
			configPropertyList = CFPropertyListCreateFromXMLData( kAllocatorDefault,
						fXMLData,
						kCFPropertyListImmutable, //could also use kCFPropertyListImmutable, kCFPropertyListMutableContainers
					   &errorString);
			
			if (configPropertyList != nil )
			{
				//make the propertylist a dict
				if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
				{
					configDict = (CFMutableDictionaryRef) configPropertyList;
				}
				
				if (configDict != nil)
				{
					//get version, useThisFileFlag(potential feature), defaults mappings and array of LDAP server configs
					
					//config file version
					configVersion = GetVersion(configDict);
					if ( configVersion == nil ) throw( (sInt32)eDSOpenNodeFailed ); //KW need eDSPlugInConfigFileError
					if (configVersion != nil)
					{
					
						CShared::LogIt( 0x0F, (char *)"Have successfully read the LDAP XML config file:" );
						CShared::LogIt( 0x0F, string );

                        //if config file is up to date with latest default mappings then use them
                        if (strcmp(configVersion,"DSLDAPPlugIn Version 1.5") == 0)
                        {
                            //array of DefaultRecordTypeMapArray
                            cfArrayRef = nil;
                            cfArrayRef = GetDefaultRecordTypeMapArray(configDict);
                            if (cfArrayRef != nil)
                            {
                                if (pStdRecordMapTuple != nil)
                                {
                                    //free up the standard record mapping table as created internally
                                    //if we have one in the config file
                                    CleanMapTuple(pStdRecordMapTuple);
                                    pStdRecordMapTuple = nil;
                                }
                                pStdRecordMapTuple = BuildMapTuple(cfArrayRef);
                            } // if (cfArrayRef != nil) ie. an array of DefaultRecordTypeMapArray
    
                            //array of DefaultAttrTypeMapArray
                            cfArrayRef = nil;
                            cfArrayRef = GetDefaultAttrTypeMapArray(configDict);
                            if (cfArrayRef != nil)
                            {
                                if (pStdAttributeMapTuple != nil)
                                {
                                    //free up the standard attribute mapping table as created internally
                                    //if we have one in the config file
                                    CleanMapTuple(pStdAttributeMapTuple);
                                    pStdAttributeMapTuple = nil;
                                }
                                pStdAttributeMapTuple = BuildMapTuple(cfArrayRef);
                            } // if (cfArrayRef != nil) ie. an array of DefaultAttrTypeMapArray
                        }
                        else
                        {
                            // update the version
                            // replace the default mappings in the configDict with the generated standard ones
                            // write the config file out to pick up the generated default mappings
                            
                            //remove old and add proper version
                            CFDictionaryRemoveValue( configDict, CFSTR( kXMLLDAPVersionKey ) );
                            cfStringRef = CFStringCreateWithCString(kCFAllocatorDefault, "DSLDAPPlugIn Version 1.5", kCFStringEncodingMacRoman);
                            CFDictionarySetValue( configDict, CFSTR( kXMLLDAPVersionKey ), cfStringRef );
                            CFRelease(cfStringRef);
                            cfStringRef = nil;

                            //generate the default mapping tables if the mappings are nil
                            if (pStdAttributeMapTuple == nil)
                            {
                                    siResult = BuildDefaultStdAttributeMap();
                            }
                            if (pStdRecordMapTuple == nil)
                            {
                                    siResult = BuildDefaultStdRecordMap();
                            }
                            
                            //now populate the dictionary with the std mappings
                            siResult = AddDefaultArrays(configDict);
                            
                            //convert the dict into a XML blob
                            xmlData = CFPropertyListCreateXMLData( kCFAllocatorDefault, configDict);

                            //replace the XML data blob
                            siResult = SetXMLConfig(xmlData);
							
							//release this reference here
							CFRelease(xmlData);
							xmlData = nil;
							                            
                            //write the file out to save the change
                            if (siResult == eDSNoErr)
                            {
                                WriteXMLConfig();
                            }
                        }

						//array of LDAP server configs
						cfArrayRef = nil;
						cfArrayRef = GetConfigArray(configDict);
						if (cfArrayRef != nil)
						{
							//now we can retrieve each config
							cfConfigCount = ::CFArrayGetCount( cfArrayRef );
							//if (cfConfigCount == 0)
							//assume that this file has no Servers in it
							//and simply proceed forward ie. no Node will get registered from data in this file
							
								//loop through the configs
								//use iConfigIndex for the access to the cfArrayRef
								//use fConfigTableLen for the index to add to the table since we add at the end
							for (sInt32 iConfigIndex = 0; iConfigIndex < cfConfigCount; iConfigIndex++)
							{
								CFDictionaryRef		serverConfigDict;
								serverConfigDict = (CFDictionaryRef)::CFArrayGetValueAtIndex( cfArrayRef, iConfigIndex );
									if ( serverConfigDict != nil )
									{
										siResult = MakeLDAPConfig(serverConfigDict, fConfigTableLen);
										//KW do something here if siResult not eDSNoErr?
									}
							} // loop over configs
							
							//CFRelease( cfArrayRef ); // no since pointer only from Get
							
						} // if (cfArrayRef != nil) ie. an array of LDAP configs exists
						
						delete(configVersion);
						
					}//if (configVersion != nil)
					
					// don't release the configDict since it is the cast configPropertyList
					
				}//if (configDict != nil)
				
				CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
				configPropertyList = nil;
	
			}//if (configPropertyList != nil )
		} // fXMLData != nil
		
	} // try
	catch( sInt32 err )
	{
		siResult = err;
		if (configPropertyList != nil)
		{
			CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
			configPropertyList = nil;
		}
	}
	return( siResult );

} // ConfigLDAPServers

// --------------------------------------------------------------------------------
//	* MakeLDAPConfig
// --------------------------------------------------------------------------------

sInt32 CLDAPConfigs::MakeLDAPConfig ( CFDictionaryRef ldapDict, sInt32 inIndex )
{
	sInt32				siResult	= eDSNoErr;
	char			   *tmpBuff		= nil;
	CFIndex				cfBuffSize	= 1024;
	CFStringRef			cfStringRef	= nil;
	CFDataRef			cfDataRef	= nil;
	CFBooleanRef		cfBool		= false;
	unsigned char		cfNumBool	= false;
	CFNumberRef			cfNumber	= 0;
	char			   *uiName		= nil;
	char			   *server		= nil;
	char			   *account		= nil;
	char			   *password	= nil;
	int					passwordLen	= 0;
	int					opencloseTO	= 0;
	int					searchTO	= 0;
	int					portNumber	= 0;
	bool				bUseStdMap	= false;
	bool				bUseSecure	= false;
	bool				bUseConfig	= false;
    sLDAPConfigData	   *pConfig		= nil;
    sLDAPConfigData	   *xConfig		= nil;
	uInt32				serverIndex = 0;
	bool				reuseEntry	= false;

	if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLEnableUseFlagKey ) ) )
	{
		//KW assume that the extracted strings will be significantly less than 1024 characters
		tmpBuff = (char *)::calloc(1, 1024);
		
		cfBool = (CFBooleanRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLEnableUseFlagKey ) );
		if (cfBool != nil)
		{
			bUseConfig = CFBooleanGetValue( cfBool );
			//CFRelease( cfBool ); // no since pointer only from Get
		}
		//continue if this configuration was enabled by the user
		//no error condition returned if this configuration is not used due to the enable use flag
		if ( bUseConfig )
		{
			//need to get the server name first
			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLServerKey ) ) )
			{
				cfStringRef = (CFStringRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLServerKey ) );
				if ( cfStringRef != nil )
				{
					if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
					{
						::memset(tmpBuff,0,1024);
						if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingMacRoman))
						{
							server = (char *)::calloc(1+strlen(tmpBuff),1);
							::strcpy(server, tmpBuff);
						}
					}
					//CFRelease(cfStringRef); // no since pointer only from Get
				}
			}

			//KW Need to check here if the config already exists ie. the server name exists
			//if it does then assume that this will replace what was given before
			
			if (CheckForConfig(server, serverIndex))
			{
				reuseEntry = true;
		        xConfig = (sLDAPConfigData *)pConfigTable->GetItemData( serverIndex );
		        if (xConfig != nil)
		        {
		            // delete the contents of sLDAPConfigData here
		            // not checking the return status of the clean here
					// since we know xConfig is NOT nil going in
		            CleanLDAPConfigData( xConfig );
		            // delete the sLDAPConfigData itself
		            delete( xConfig );
		            xConfig = nil;
		            // remove the table entry
		            pConfigTable->RemoveItem( serverIndex );
		        }
			}
			
			//Enable Use flag is NOT provided to the configTable
			//retrieve all the others for the configTable
			
			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLOpenCloseTimeoutSecsKey ) ) )
			{
				cfNumber = (CFNumberRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLOpenCloseTimeoutSecsKey ) );
				if ( cfNumber != nil )
				{
					cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &opencloseTO);
					//CFRelease(cfNumber); // no since pointer only from Get
				}
			}

			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLSearchTimeoutSecsKey ) ) )
			{
				cfNumber = (CFNumberRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLSearchTimeoutSecsKey ) );
				if ( cfNumber != nil )
				{
					cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &searchTO);
					//CFRelease(cfNumber); // no since pointer only from Get
				}
			}

			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLPortNumberKey ) ) )
			{
				cfNumber = (CFNumberRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLPortNumberKey ) );
				if ( cfNumber != nil )
				{
					cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &portNumber);
					//CFRelease(cfNumber); // no since pointer only from Get
				}
			}

			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLSecureUseFlagKey ) ) )
			{
				cfBool= (CFBooleanRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLSecureUseFlagKey ) );
				if (cfBool != nil)
				{
					bUseSecure = CFBooleanGetValue( cfBool );
					//CFRelease( cfBool ); // no since pointer only from Get
				}
			}

			//null strings are acceptable but not preferred
			//ie. the new char will be of length one and the strcpy will copy the "" - empty string
			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLUserDefinedNameKey ) ) )
			{
				cfStringRef = (CFStringRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLUserDefinedNameKey ) );
				if ( cfStringRef != nil )
				{
					if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
					{
						::memset(tmpBuff,0,1024);
						if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingMacRoman))
						{
							uiName = (char *)::calloc(1+strlen(tmpBuff),1);
							::strcpy(uiName, tmpBuff);
						}
					}
					//CFRelease(cfStringRef); // no since pointer only from Get
				}
			}

			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLServerAccountKey ) ) )
			{
				cfStringRef = (CFStringRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLServerAccountKey ) );
				if ( cfStringRef != nil )
				{
					if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
					{
						::memset(tmpBuff,0,1024);
						if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingMacRoman))
						{
							account = (char *)::calloc(1+strlen(tmpBuff),1);
							::strcpy(account, tmpBuff);
						}
					}
					//CFRelease(cfStringRef); // no since pointer only from Get
				}
			}

			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLServerPasswordKey ) ) )
			{
				cfStringRef = (CFStringRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLServerPasswordKey ) );
				if ( cfStringRef != nil )
				{
					if ( CFGetTypeID( cfStringRef ) == CFDataGetTypeID() )
					{
						cfDataRef = (CFDataRef)cfStringRef;
						passwordLen = CFDataGetLength(cfDataRef);
						password = (char*)::calloc(1+passwordLen,1);
						CFDataGetBytes(cfDataRef, CFRangeMake(0,passwordLen), (UInt8*)password);
					}
					else if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
					{
						::memset(tmpBuff,0,1024);
						if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingMacRoman))
						{
							password = (char *)::calloc(1+strlen(tmpBuff),1);
							::strcpy(password, tmpBuff);
						}
					}
					//CFRelease(cfStringRef); // no since pointer only from Get
				}
			}

			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLStdMapUseFlagKey ) ) )
			{
				cfBool = (CFBooleanRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLStdMapUseFlagKey ) );
				if (cfBool != nil)
				{
					bUseStdMap = CFBooleanGetValue( cfBool );
					//CFRelease( cfBool ); // no since pointer only from Get
				}
			}

			//setup the config table
			// MakeLDAPConfigData does not consume the strings passed in so we need to free them below
			pConfig = MakeLDAPConfigData( uiName, server, bUseStdMap, opencloseTO, searchTO, portNumber, bUseSecure, account, password );
			
			if ( uiName != nil ) 
			{
				free( uiName );
				uiName = nil;
			}
			if ( server != nil ) 
			{
				free( server );
				server = nil;
			}
			if ( account != nil ) 
			{
				free( account );
				account = nil;
			}
			if ( password != nil ) 
			{
				free( password );
				password = nil;
			}
			
			if (!bUseStdMap)
			{
				//get the mappings from the config ldap dict
				BuildLDAPMap( pConfig, ldapDict );
			}
			
			if (reuseEntry)
			{
				pConfigTable->AddItem( serverIndex, pConfig );
			}
			else
			{
				pConfigTable->AddItem( inIndex, pConfig );
				fConfigTableLen++;
			}


		}// if ( bUseConfig )
		
		//free up the tmpBuff
		delete( tmpBuff );

	}// if kXMLEnableUseFlagKey set

	// return if nil or not
	return( siResult );

} // MakeLDAPConfig


// --------------------------------------------------------------------------------
//	* CheckForConfig
// --------------------------------------------------------------------------------

bool CLDAPConfigs::CheckForConfig ( char *inServerName, uInt32 &inConfigTableIndex )
{
	bool				result 		= false;
    uInt32				iTableIndex	= 0;
    sLDAPConfigData	   *pConfig		= nil;

	//need to cycle through the config table
    for (iTableIndex=0; iTableIndex<fConfigTableLen; iTableIndex++)
    {
        pConfig = (sLDAPConfigData *)pConfigTable->GetItemData( iTableIndex );
        if (pConfig != nil)
        {
	        if (pConfig->fServerName != nil)
	        {
	            if (::strcmp(pConfig->fServerName, inServerName) == 0 )
	            {
	            	result = true;
	            	inConfigTableIndex = iTableIndex;
	            	break;
	            }
	        }
        }
    }
    
    return(result);
	
	
} // CheckForConfig


// --------------------------------------------------------------------------------
//	* BuildLDAPMap
// --------------------------------------------------------------------------------

sInt32 CLDAPConfigs::BuildLDAPMap ( sLDAPConfigData *inConfig, CFDictionaryRef ldapDict )
{
	sInt32					siResult			= eDSNoErr; // used for?
	CFArrayRef				cfArrayRef			= nil;
	sMapTuple			   *pRecMapTuple		= nil;
	sMapTuple			   *pAttrMapTuple		= nil;

	cfArrayRef = nil;
	cfArrayRef = GetRecordTypeMapArray(ldapDict);
	pRecMapTuple = BuildMapTuple(cfArrayRef);
	
	cfArrayRef = nil;
	cfArrayRef = GetAttributeTypeMapArray(ldapDict);
	pAttrMapTuple = BuildMapTuple(cfArrayRef);
	
	if (pRecMapTuple != nil)
	{
			inConfig->pRecordMapTuple		= pRecMapTuple;
	}
	else
	{
			inConfig->pRecordMapTuple		= pStdRecordMapTuple;
	}

	if (pAttrMapTuple != nil)
	{
			inConfig->pAttributeMapTuple	= pAttrMapTuple;
	}
	else
	{
			inConfig->pAttributeMapTuple	= pStdAttributeMapTuple;
	}

	return( siResult );

} // BuildLDAPMap


// --------------------------------------------------------------------------------
//	* BuildMapTuple
// --------------------------------------------------------------------------------

sMapTuple *CLDAPConfigs::BuildMapTuple ( CFArrayRef inArray )
{
	CFArrayRef				cfArrayRef			= nil;
	CFIndex					cfMapCount			= 0;
	CFIndex					cfNativeMapCount	= 0;
	sInt32					iMapIndex			= 0;
	sInt32					iNativeMapIndex		= 0;
	bool					bUseMap				= false;
	CFStringRef				cfStringRef			= nil;
	char				   *tmpBuff				= nil;
	CFIndex					cfBuffSize			= 1024;
	char				   *stdMapType			= nil;
	char				   *nativeMapType		= nil;
	CFArrayRef				cfNativeArrayRef	= nil;
	sMapTuple			   *pMapTuple			= nil;
	sMapTuple			   *currentMapTuple		= nil;
	sPtrString			   *currentNativeMap	= nil;
	sInt32					siResult			= eDSNoErr;

	cfArrayRef = inArray;
	
	if (cfArrayRef != nil)
	{
		//now we can retrieve each Type mapping
		cfMapCount = ::CFArrayGetCount( cfArrayRef );
		if (cfMapCount != 0)
		{
			//KW assume that the extracted strings will be significantly less than 1024 characters
			tmpBuff = (char *) calloc(1, 1024);
			
			//loop through the maps
			for (iMapIndex = 0; iMapIndex < cfMapCount; iMapIndex++)
			{
				CFDictionaryRef		typeMapDict;
				typeMapDict = (CFDictionaryRef)::CFArrayGetValueAtIndex( cfArrayRef, iMapIndex );
				if ( typeMapDict != nil )
				{
					//KW retrieve the mappings
					// get the standard type label first
					if ( CFDictionaryContainsKey( typeMapDict, CFSTR( kXMLStdNameKey ) ) )
					{
						cfStringRef = (CFStringRef)CFDictionaryGetValue( typeMapDict, CFSTR( kXMLStdNameKey ) );
						if ( cfStringRef != nil )
						{
							if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
							{
								::memset(tmpBuff,0,1024);
								if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingMacRoman))
								{
									stdMapType = new char[1+strlen(tmpBuff)];
									::strcpy(stdMapType, tmpBuff);
								}
							}
							//CFRelease(cfStringRef); // no since pointer only from Get
						}
					}
					// get the native map array of labels next
					cfNativeArrayRef = nil;
					cfNativeArrayRef = GetNativeTypeMapArray(typeMapDict);
					if (cfNativeArrayRef != nil)
					{
						//now we can retrieve each Native Type mapping to the given Standard type
						cfNativeMapCount = ::CFArrayGetCount( cfNativeArrayRef );
						//check here that we have a potential entry
						//ie. std type not nil and an entry in the native map array
						if ((cfNativeMapCount != 0) && (stdMapType != nil))
						{
							//create the mapping entry
							if (pMapTuple == nil)
							{
								pMapTuple = new sMapTuple;
								currentMapTuple = pMapTuple;
							}
							else
							{
								currentMapTuple->pNext = new sMapTuple;
								currentMapTuple = currentMapTuple->pNext;
							}
					        if ( currentMapTuple != nil )
					        {
					        	::memset( currentMapTuple, 0, sizeof( sMapTuple ) );
						        currentMapTuple->fStandard = stdMapType;
						        currentMapTuple->pNext = nil;
					        }										

							//loop through the Native Maps
							for (iNativeMapIndex = 0; iNativeMapIndex < cfNativeMapCount; iNativeMapIndex++)
							{
								CFStringRef	nativeMapString;
								nativeMapString = (CFStringRef)::CFArrayGetValueAtIndex( cfNativeArrayRef, iNativeMapIndex );
								if ( nativeMapString != nil )
								{
									if ( CFGetTypeID( nativeMapString ) == CFStringGetTypeID() )
									{
										::memset(tmpBuff,0,1024);
										if (CFStringGetCString(nativeMapString, tmpBuff, cfBuffSize, kCFStringEncodingMacRoman))
										{
											nativeMapType = new char[1+strlen(tmpBuff)];
											::strcpy(nativeMapType, tmpBuff);
											//add to the mapping with a native entry
											if (currentMapTuple->fNative == nil)
											{
												currentMapTuple->fNative = new sPtrString;
												currentNativeMap = currentMapTuple->fNative;
											}
											else
											{
												currentNativeMap->pNext = new sPtrString;
												currentNativeMap = currentNativeMap->pNext;
											}
									        if ( currentNativeMap != nil )
									        {
									        	::memset( currentNativeMap, 0, sizeof( sPtrString ) );
										        currentNativeMap->fName = nativeMapType;
										        currentNativeMap->pNext = nil;
										        //at least one map is valid so set to true and use it
										        bUseMap = true;
									        }										
										}
									}
									//CFRelease(nativeMapString); // no since pointer only from Get
								}// if ( nativeMapString != nil )
								
							}//loop through the Native Maps
							
						}// if (cfNativeMapCount != 0)
						else if (stdMapType != nil)
						{
							// we should free this since we didn't consume it
							free( stdMapType );
							stdMapType = nil;
						}
					}// if (cfNativeArrayRef != nil)
					
					//CFRelease( typeMapDict ); // no since pointer only from Get
					
				}//if ( typeMapDict != nil )
				
			} // loop over maps
			
			delete( tmpBuff );
			
		} // if (cfMapCount != 0)
		
	} // if (cfArrayRef != nil) ie. an array of Record Maps exists
	
	if (!bUseMap)
	{
		siResult = CleanMapTuple(pMapTuple);
	}
	
	return( pMapTuple );

} // BuildMapTuple


// --------------------------------------------------------------------------------
//	* GetVersion
// --------------------------------------------------------------------------------

char *CLDAPConfigs::GetVersion ( CFDictionaryRef configDict )
{
	char			   *outVersion	= nil;
	CFStringRef			cfStringRef	= nil;
	char			   *tmpBuff		= nil;
	CFIndex				cfBuffSize	= 1024;

	if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLLDAPVersionKey ) ) )
	{
		cfStringRef = (CFStringRef)CFDictionaryGetValue( configDict, CFSTR( kXMLLDAPVersionKey ) );
		if ( cfStringRef != nil )
		{
			if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
			{
				//KW assume that the extracted strings will be significantly less than 1024 characters
				tmpBuff = new char[1024];
				::memset(tmpBuff,0,1024);
				if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingMacRoman))
				{
					outVersion = new char[1+strlen(tmpBuff)];
					::strcpy(outVersion, tmpBuff);
				}
				delete( tmpBuff );
			}
			//CFRelease( cfStringRef ); // no since pointer only from Get
		}
	}

	// return if nil or not
	return( outVersion );

} // GetVersion


// --------------------------------------------------------------------------------
//	* GetConfigArray
// --------------------------------------------------------------------------------

CFArrayRef CLDAPConfigs::GetConfigArray ( CFDictionaryRef configDict )
{
	CFArrayRef		cfArrayRef	= nil;

	if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLConfigArrayKey ) ) )
	{
		cfArrayRef = (CFArrayRef)CFDictionaryGetValue( configDict, CFSTR( kXMLConfigArrayKey ) );
	}

	// return if nil or not
	return( cfArrayRef );

} // GetConfigArray


// --------------------------------------------------------------------------------
//	* GetDefaultRecordTypeMapArray
// --------------------------------------------------------------------------------

CFArrayRef CLDAPConfigs::GetDefaultRecordTypeMapArray ( CFDictionaryRef configDict )
{
	CFArrayRef		cfArrayRef	= nil;

	if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLDefaultRecordTypeMapArrayKey ) ) )
	{
		cfArrayRef = (CFArrayRef)CFDictionaryGetValue( configDict, CFSTR( kXMLDefaultRecordTypeMapArrayKey ) );
	}

	// return if nil or not
	return( cfArrayRef );

} // GetDefaultRecordTypeMapArray


// --------------------------------------------------------------------------------
//	* GetDefaultAttrTypeMapArray
// --------------------------------------------------------------------------------

CFArrayRef CLDAPConfigs::GetDefaultAttrTypeMapArray ( CFDictionaryRef configDict )
{
	CFArrayRef		cfArrayRef	= nil;

	if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLDefaultAttrTypeMapArrayKey ) ) )
	{
		cfArrayRef = (CFArrayRef)CFDictionaryGetValue( configDict, CFSTR( kXMLDefaultAttrTypeMapArrayKey ) );
	}

	// return if nil or not
	return( cfArrayRef );

} // GetDefaultAttrTypeMapArray


// --------------------------------------------------------------------------------
//	* GetRecordTypeMapArray
// --------------------------------------------------------------------------------

CFArrayRef CLDAPConfigs::GetRecordTypeMapArray ( CFDictionaryRef configDict )
{
	CFArrayRef		cfArrayRef	= nil;

	if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLRecordTypeMapArrayKey ) ) )
	{
		cfArrayRef = (CFArrayRef)CFDictionaryGetValue( configDict, CFSTR( kXMLRecordTypeMapArrayKey ) );
	}

	// return if nil or not
	return( cfArrayRef );

} // GetRecordTypeMapArray

// --------------------------------------------------------------------------------
//	* GetAttributeTypeMapArray
// --------------------------------------------------------------------------------

CFArrayRef CLDAPConfigs::GetAttributeTypeMapArray ( CFDictionaryRef configDict )
{
	CFArrayRef		cfArrayRef	= nil;

	if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLAttrTypeMapArrayKey ) ) )
	{
		cfArrayRef = (CFArrayRef)CFDictionaryGetValue( configDict, CFSTR( kXMLAttrTypeMapArrayKey ) );
	}

	// return if nil or not
	return( cfArrayRef );

} // GetAttributeTypeMapArray

// --------------------------------------------------------------------------------
//	* GetNativeTypeMapArray
// --------------------------------------------------------------------------------

CFArrayRef CLDAPConfigs::GetNativeTypeMapArray ( CFDictionaryRef configDict )
{
	CFArrayRef		cfArrayRef	= nil;

	if ( CFDictionaryContainsKey( configDict, CFSTR( kXMLNativeMapArrayKey ) ) )
	{
		cfArrayRef = (CFArrayRef)CFDictionaryGetValue( configDict, CFSTR( kXMLNativeMapArrayKey ) );
	}

	// return if nil or not
	return( cfArrayRef );

} // GetNativeTypeMapArray

// ---------------------------------------------------------------------------
//	* MakeLDAPConfigData
// ---------------------------------------------------------------------------

sLDAPConfigData *CLDAPConfigs::MakeLDAPConfigData (	char *inName, char *inServerName, bool inUseStd,
													int inOpenCloseTO, int inSearchTO, int inPortNum,
													bool inUseSecure,
													char *inAccount, char *inPassword )
{
	sInt32				siResult		= eDSNoErr;
    sLDAPConfigData	   *configOut		= nil;

    configOut = new sLDAPConfigData;
    if ( configOut != nil )
    {
        ::memset( configOut, 0, sizeof( sLDAPConfigData ) );
        //do nothing with return here since we know this is new
        //and we did a memset above
        siResult = CleanLDAPConfigData(configOut);

		if (inName != nil)
		{
			configOut->fName			= new char[1+::strlen( inName )];
			::strcpy(configOut->fName, inName);
		}
		if (inServerName != nil)
		{
			configOut->fServerName			= new char[1+::strlen( inServerName )];
			::strcpy(configOut->fServerName, inServerName);
		}
		configOut->bUseStdMapping		= inUseStd;
		configOut->fOpenCloseTimeout	= inOpenCloseTO;
		configOut->fSearchTimeout		= inSearchTO;
		configOut->fServerPort			= inPortNum;
		configOut->bSecureUse			= inUseSecure;
		configOut->bUpdated				= true;
		if (inAccount != nil)
		{
			configOut->fServerAccount	= new char[1+::strlen( inAccount )];
			::strcpy(configOut->fServerAccount, inAccount);
		}
		if (inPassword != nil)
		{
			configOut->fServerPassword	= new char[1+::strlen( inPassword )];
			::strcpy(configOut->fServerPassword, inPassword);
		}
		if (inUseStd)
		{
			configOut->pAttributeMapTuple	= pStdAttributeMapTuple;
			configOut->pRecordMapTuple		= pStdRecordMapTuple;
		}
    }
	

	return( configOut );

} // MakeLDAPConfigData


// ---------------------------------------------------------------------------
//	* CleanLDAPConfigData
// ---------------------------------------------------------------------------

sInt32 CLDAPConfigs::CleanLDAPConfigData ( sLDAPConfigData *inConfig )
{
    sInt32	siResult = eDSNoErr;
    
    if ( inConfig == nil )
    {
        siResult = eDSBadContextData; // KW want an eDSBadConfigData??
	}
    else
    {
        if (inConfig->fName != nil)
        {
            delete ( inConfig->fName );
        }
        if (inConfig->fServerName != nil)
        {
            delete ( inConfig->fServerName );
        }
        if (inConfig->fServerAccount != nil)
        {
            delete ( inConfig->fServerAccount );
        }
        if (inConfig->fServerPassword != nil)
        {
            delete ( inConfig->fServerPassword );
        }
		inConfig->fName					= nil;
		inConfig->fServerName			= nil;
		inConfig->fServerAccount		= nil;
		inConfig->fServerPassword		= nil;
        if (!(inConfig->bUseStdMapping))
        {
        	CleanMapTuple(inConfig->pAttributeMapTuple);
        	CleanMapTuple(inConfig->pRecordMapTuple);
        }
		inConfig->bUseStdMapping		= true;
		inConfig->fOpenCloseTimeout		= 120;
		inConfig->fSearchTimeout		= 120;
		inConfig->fServerPort			= 389;
		inConfig->bSecureUse			= false;
		inConfig->bAvail				= false;
		inConfig->bUpdated				= false;
        
   }

    return( siResult );

} // CleanLDAPConfigData

