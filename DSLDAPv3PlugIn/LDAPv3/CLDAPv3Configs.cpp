/*
	File:		CLDAPv3Configs.cpp

	Contains:	Code to parse a XML file and place the contents into a table of structs

	Copyright:	© 2001 by Apple Computer, Inc., all rights reserved.

	NOT_FOR_OPEN_SOURCE <to be reevaluated at a later time>

*/

#include "CLDAPv3Configs.h"
#include <DirectoryServiceCore/CSharedData.h>

#include <CoreFoundation/CFPriv.h>		// used for ::CFCopySearchPathForDirectoriesInDomains

#include <string.h>				//used for strcpy, etc.
#include <stdlib.h>				//used for malloc
#include <sys/types.h>
#include <sys/stat.h>			//used for mkdir and stat
#include <syslog.h>				//error logging

//#include <iostream.h>

#include <DirectoryServiceCore/PrivateTypes.h>			// try and catch macros

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>

#define kAllocatorDefault NULL


// --------------------------------------------------------------------------------
//	* CLDAPv3Configs
// --------------------------------------------------------------------------------

CLDAPv3Configs::CLDAPv3Configs ( void )
{
	pConfigTable			= nil;
    //pStdAttributeMapTuple	= nil;
    //pStdRecordMapTuple		= nil;
	fConfigTableLen			= 0;
	fXMLData				= nil;
	pXMLConfigLock			= new DSMutexSemaphore();
} // CLDAPv3Configs


// --------------------------------------------------------------------------------
//	* ~CLDAPv3Configs ()
// --------------------------------------------------------------------------------

CLDAPv3Configs::~CLDAPv3Configs ( void )
{
    uInt32				iTableIndex	= 0;
//	sInt32				siResult 	= eDSNoErr;
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
	
	if (pXMLConfigLock != nil)
	{
		delete(pXMLConfigLock);
		pXMLConfigLock = nil;
	}
	
	if (fXMLData != nil)
	{
		CFRelease(fXMLData);
		fXMLData = nil;
	}

	//free up the standard mapping tables after the config table is freed
	//siResult = CleanMapTuple(pStdAttributeMapTuple);
	//siResult = CleanMapTuple(pStdRecordMapTuple);

} // ~CLDAPv3Configs


// --------------------------------------------------------------------------------
//	* Init (CPlugInRef, sMapTuple, sMapTuple)
// --------------------------------------------------------------------------------

sInt32 CLDAPv3Configs::Init ( CPlugInRef *inConfigTable, uInt32 &inConfigTableLen, sMapTuple **inStdAttributeMapTuple, sMapTuple **inStdRecordMapTuple )
{

	sInt32				siResult	= eDSNoErr;
	sLDAPConfigData	   *pConfig		= nil;
	uInt32				sIndex		= 0;
    uInt32				iTableIndex	= 0;

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
			pConfig = MakeLDAPConfigData((char *)"Generic",(char *)"unknown",true,120,2,120,120,389,false, 0, 0, false, false, false);
			pConfigTable->AddItem( fConfigTableLen, pConfig );
			fConfigTableLen++;
	}

	XMLConfigLock();
	//read the XML Config file
	if (fXMLData != nil)
	{
		CFRelease(fXMLData);
		fXMLData = nil;
	}
	siResult = ReadXMLConfig();
	XMLConfigUnlock();
	
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
		XMLConfigLock();
		siResult = ConfigLDAPServers();
		XMLConfigUnlock();
	}
	
	//set/update the number of configs in the table
	inConfigTableLen = fConfigTableLen;

	/*
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
	*/

	return( siResult );

} // Init


// ---------------------------------------------------------------------------
//	* BuildDefaultStdAttributeMap
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Configs::BuildDefaultStdAttributeMap ( void )
{
	sInt32		siResult		= eDSNoErr;
	sMapTuple   *pAttrMapTuple	= nil;
	sPtrString  *tempPtrString	= nil;

//KW in the future this routine will read a XML file or ?
//KW the error return code might then be useful
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

sInt32 CLDAPv3Configs::BuildDefaultStdRecordMap ( void )
{
	sInt32		siResult	= eDSNoErr;
	sMapTuple   *pRecMapTuple	= nil;

//KW in the future this routine will read a XML file or ?
//KW the error return code might then be useful
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
			//insert some object class data here
			pRecMapTuple->fNative->fSubNative = new sPtrString;
			if ( pRecMapTuple->fNative->fSubNative != nil )
			{
				::memset( pRecMapTuple->fNative->fSubNative, 0, sizeof( sPtrString ) );
				pRecMapTuple->fNative->fSubNative->fName = new char[1+::strlen("*")];
				::strcpy(pRecMapTuple->fNative->fSubNative->fName,"*");
				pRecMapTuple->fNative->fSubNative->pNext = nil;
			}
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

sInt32 CLDAPv3Configs::CleanMapTuple ( sMapTuple *inMapTuple )
{
	sInt32		siResult		= eDSNoErr;
	sMapTuple   *pMapTuple		= nil;
	sPtrString  *pPtrString		= nil;
	sPtrString  *pSubPtrString	= nil;

//KW is the error return code ever useful?

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
			
			pPtrString->fGroupSubNative = 0;
			while (pPtrString->fSubNative != nil)
			{
				pSubPtrString = pPtrString->fSubNative;
				pPtrString->fSubNative = pSubPtrString->pNext;
				if (pSubPtrString->fName != nil)
				{
					delete(pSubPtrString->fName);
				}
//always assume that there will only be one layer of fSubNative in the sPtrString struct
				delete(pSubPtrString);
			}
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

sInt32 CLDAPv3Configs::ReadXMLConfig ( void )
{
	sInt32					siResult				= eDSNoErr;
	CFURLRef				configFileURL			= nil;
	CFURLRef				configFileCorruptedURL	= nil;
	CFDataRef				xmlData;
	bool					bReadFile				= false;
	bool					bCorruptedFile			= false;
	bool					bWroteFile				= false;
	register CFIndex		iPath;
	CFArrayRef				aPaths					= nil;
	char					string[ PATH_MAX ];
	char					string2[ PATH_MAX ];
    struct stat				statResult;
	CFMutableDictionaryRef	configDict;
	CFStringRef				cfStringRef;
	sInt32					errorCode				= 0;
	CFStringRef				sBase					= nil;
	CFStringRef				sPath					= nil;
	CFStringRef				sCorruptedPath;

//Config data is read from a XML file
//KW eventually use Version from XML file to check against the code here?
//Steps in the process:
//1- see if the file exists
//2- if it exists then try to read it
//3- if existing file is corrupted then rename it and save it while creating a new default file
//4- if file doesn't exist then create a new default file - make sure directories exist/if not create them

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
				sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s" ), sBase, "/Preferences/DirectoryService/DSLDAPv3PlugInConfig.plist" );

				::memset(string,0,PATH_MAX);
				::CFStringGetCString( sPath, string, sizeof( string ), kCFStringEncodingUTF8 );
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
					::CFStringGetCString( sPath, string2, sizeof( string2 ), kCFStringEncodingUTF8 );
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
					::CFStringGetCString( sPath, string2, sizeof( string2 ), kCFStringEncodingUTF8 );
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
					
					cfStringRef = CFStringCreateWithCString(kCFAllocatorDefault, "DSLDAPv3PlugIn Version 1.5", kCFStringEncodingUTF8);
					CFDictionarySetValue( configDict, CFSTR( kXMLLDAPVersionKey ), cfStringRef );
					CFRelease(cfStringRef);
					cfStringRef = nil;
					//build the standard types in here so that there is something to start with
					//siResult = BuildDefaultStdAttributeMap();
					//siResult = BuildDefaultStdRecordMap();
					
					//now populate the dictionary with the std mappings
					//siResult = AddDefaultArrays(configDict);
					
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
			sCorruptedPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s" ), sBase, "/Preferences/DirectoryService/DSLDAPv3PlugInConfigCorrupted.plist" );

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
		
		cfStringRef = CFStringCreateWithCString(kCFAllocatorDefault, "DSLDAPv3PlugIn Version 1.5", kCFStringEncodingUTF8);
		CFDictionarySetValue( configDict, CFSTR( kXMLLDAPVersionKey ), cfStringRef );
		CFRelease(cfStringRef);
		cfStringRef = nil;
		/*
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
		*/
		
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

sInt32 CLDAPv3Configs::WriteXMLConfig ( void )
{
	sInt32					siResult			= eDSNoErr;
	CFURLRef				configFileURL;
//	CFURLRef				configFileBackupURL;
//	CFDataRef				xmlData;
	bool					bWroteFile			= false;
//	bool					bReadFile			= false;
	register CFIndex		iPath;
	CFArrayRef				aPaths				= nil;
	char					string[ PATH_MAX ];
	struct stat				statResult;
	sInt32					errorCode			= 0;

	//Config data is written to a XML file
	//Steps in the process:
	//1- see if the file exists
	//2- if it exists then overwrite it
	//3- rename existing file and save it while creating a new file
	//4- if file doesn't exist then create a new default file - make sure directories exist/if not create them
	//make sure file permissions are root only

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
				sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s" ), sBase, "/Preferences/DirectoryService/DSLDAPv3PlugInConfig.plist" );

				::memset(string,0,PATH_MAX);
				::CFStringGetCString( sPath, string, sizeof( string ), kCFStringEncodingUTF8 );
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
					
				//if file exists then we make a backup copy - why? - because
				/*if (siResult == eDSNoErr)
				{
					CFStringRef	sBackupPath;

					// Append the subpath.
					sBackupPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%@/%s" ), sBase, "/Preferences/DirectoryService/DSLDAPv3PlugInConfigBackup.plist" );

					// Convert it into a CFURL.
					configFileBackupURL = ::CFURLCreateWithFileSystemPath( kCFAllocatorDefault, sBackupPath, kCFURLPOSIXPathStyle, false );
					CFRelease( sBackupPath ); // build with Create so okay to dealloac here
					sBackupPath = nil;
					
					// Read the old XML property list file
					bReadFile = CFURLCreateDataAndPropertiesFromResource(
																			kAllocatorDefault,
																			configFileURL,
																			&xmlData,          // place to put file data
																			NULL,
																			NULL,
																			&siResult);
					//write the XML to the backup config file
					if (bReadFile)
					{

						bWroteFile = CFURLWriteDataAndPropertiesToResource( configFileBackupURL,
																			xmlData,
																			NULL,
																			&errorCode);
						//KW check the error code and the result?

						CFRelease(xmlData);
						xmlData = nil;
					}
				}*/
				//if file does not exist
				if (siResult != eDSNoErr)
				{
					siResult = eDSNoErr;
					//move down the path from the system defined local directory and check if it exists
					//if not create it
					sPath = ::CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@/%s" ), sBase, "/Preferences" );
					::memset(string,0,PATH_MAX);
					::CFStringGetCString( sPath, string, sizeof( string ), kCFStringEncodingUTF8 );
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
					::CFStringGetCString( sPath, string, sizeof( string ), kCFStringEncodingUTF8 );
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
		
	return( siResult );

} // WriteXMLConfig

// ---------------------------------------------------------------------------
//	* SetXMLConfig
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Configs::SetXMLConfig ( CFDataRef xmlData )
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
		//return ConfigLDAPServers(); // this refreshes the registered nodes
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

CFDataRef CLDAPv3Configs::GetXMLConfig ( void )
{
	return fXMLData;
}


sInt32 CLDAPv3Configs::AddDefaultArrays ( CFMutableDictionaryRef inDict )
{
	CFMutableArrayRef			cfArrayRef			= nil;
	CFMutableDictionaryRef		cfDictRef			= nil;
	CFIndex						cfMapCount			= 0;
	CFIndex						cfNativeMapCount	= 0;
	CFIndex						cfSubNativeMapCount	= 0;
	CFStringRef					cfStringRef			= nil;
	CFMutableArrayRef			cfNativeArrayRef	= nil;
	CFMutableArrayRef			cfSubNativeArrayRef	= nil;
	sMapTuple				   *pMapTuple			= nil;
	sInt32						siResult			= eDSNoErr;
	sPtrString				   *pPtrString			= nil;
	sPtrString				   *pSubPtrString		= nil;
	CFMutableDictionaryRef		cfSubDictRef		= nil;
        

    // if this already exists in the dictionary then remove it
    if ( CFDictionaryContainsKey( inDict, CFSTR( kXMLDefaultAttrTypeMapArrayKey ) ) )
    {
        //cfArrayRef = (CFMutableArrayRef)CFDictionaryGetValue( inDict, CFSTR( kXMLDefaultAttrTypeMapArrayKey ) );
        //KW is this sufficient ie. does remove do the release as well
        CFDictionaryRemoveValue( inDict, CFSTR( kXMLDefaultAttrTypeMapArrayKey ) );
        //CFRelease(cfArrayRef);
        //cfArrayRef = nil;
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
                cfStringRef = CFStringCreateWithCString(kCFAllocatorDefault, pMapTuple->fStandard, kCFStringEncodingUTF8);
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
                    cfStringRef = CFStringCreateWithCString(kCFAllocatorDefault, pPtrString->fName, kCFStringEncodingUTF8);
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
        //cfArrayRef = (CFMutableArrayRef)CFDictionaryGetValue( inDict, CFSTR( kXMLDefaultRecordTypeMapArrayKey ) );
        //KW is this sufficient ie. does remove do the release as well
        CFDictionaryRemoveValue( inDict, CFSTR( kXMLDefaultRecordTypeMapArrayKey ) );
        //CFRelease(cfArrayRef);
        //cfArrayRef = nil;
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
                cfStringRef = CFStringCreateWithCString(kCFAllocatorDefault, pMapTuple->fStandard, kCFStringEncodingUTF8);
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
                if (pPtrString->fSubNative != nil) //case used for ObjectClasses and SearchBase separated
                {
					cfSubDictRef = CFDictionaryCreateMutable(	kCFAllocatorDefault,
																0,
																&kCFTypeDictionaryKeyCallBacks,
																&kCFTypeDictionaryValueCallBacks );
					if (pPtrString->fName != nil)
					{
						cfStringRef = CFStringCreateWithCString(kCFAllocatorDefault, pPtrString->fName, kCFStringEncodingUTF8);
						CFDictionarySetValue( cfSubDictRef, CFSTR(kXMLSearchBase), cfStringRef);
						CFRelease(cfStringRef);
						cfStringRef = nil;
					}
					
					cfSubNativeArrayRef = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
					CFDictionarySetValue( cfSubDictRef, CFSTR(kXMLObjectClasses), cfSubNativeArrayRef);
					//release then get from the dictionary
					CFRelease(cfSubNativeArrayRef);
					cfSubNativeArrayRef = nil;
					cfSubNativeArrayRef = (CFMutableArrayRef)CFDictionaryGetValue( cfSubDictRef, CFSTR( kXMLObjectClasses ) );
					cfSubNativeMapCount = 0;
					pSubPtrString = pPtrString->fSubNative;
					if( pSubPtrString != nil)
					{
						if (pSubPtrString->fGroupSubNative != 0) //ie. do nothing if zero
						{
							cfStringRef = CFStringCreateWithCString(kCFAllocatorDefault, "OR", kCFStringEncodingUTF8);
							CFDictionarySetValue( cfSubDictRef, CFSTR(kXMLGroupObjectClasses), cfStringRef);
							CFRelease(cfStringRef);
							cfStringRef = nil;
						}
					}
					while( pSubPtrString != nil)
					{
						if (pSubPtrString->fName != nil)
						{
							cfStringRef = CFStringCreateWithCString(kCFAllocatorDefault, pSubPtrString->fName, kCFStringEncodingUTF8);
							CFArrayInsertValueAtIndex(cfSubNativeArrayRef, cfSubNativeMapCount, cfStringRef);
							CFRelease(cfStringRef);
							cfStringRef = nil;
							//next index value
							cfSubNativeMapCount++;
						}
						pSubPtrString = pSubPtrString->pNext;
					}
					CFArrayInsertValueAtIndex(cfNativeArrayRef, cfNativeMapCount, cfSubDictRef);
					CFRelease(cfSubDictRef);
					cfSubDictRef = nil;
					//next index value
					cfNativeMapCount++;
				}
				else
				{
					if (pPtrString->fName != nil)
					{
						cfStringRef = CFStringCreateWithCString(kCFAllocatorDefault, pPtrString->fName, kCFStringEncodingUTF8);
						CFArrayInsertValueAtIndex(cfNativeArrayRef, cfNativeMapCount, cfStringRef);
						CFRelease(cfStringRef);
						cfStringRef = nil;
						//next index value
						cfNativeMapCount++;
					}
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

bool CLDAPv3Configs::VerifyXML ( void )
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

// --------------------------------------------------------------------------------
//	* ConfigDHCPObtainedLDAPServer
// --------------------------------------------------------------------------------

void CLDAPv3Configs::ConfigDHCPObtainedLDAPServer ( char *inServer, char *inMapSearchBase, int inPortNumber, bool inIsSSL, uInt32 &inConfigTableLen )
{
	sInt32		siResult	= eDSNoErr;
	CFDataRef	ourXMLData	= nil;
	CFDataRef	newXMLData	= nil;
	
	ourXMLData = RetrieveServerMappings( inServer, inMapSearchBase, inPortNumber, inIsSSL );
	if (ourXMLData != nil)
	{
		//here we will make sure that the server location and port/SSL in the XML data is the same as given above
		//we also make sure that the MakeDefLDAPFlag is set so that this gets added to the Automatic search policy
		newXMLData = VerifyAndUpdateServerLocation(inServer, inPortNumber, inIsSSL, ourXMLData); //don't check return
		
		if (newXMLData != nil)
		{
			CFRelease(ourXMLData);
			ourXMLData = newXMLData;
			newXMLData = nil;
		}
		siResult = AddDefaultLDAPServer(ourXMLData);
		if (siResult != eDSNoErr)
		{
			syslog(LOG_INFO,"DSLDAPv3PlugIn: DHCP option 95 obtained [%s] LDAP server not added to search policy due to server mappings format error.", inServer);
		}

		//set/update the number of configs in the table
		inConfigTableLen = fConfigTableLen;
	}
	else
	{
		syslog(LOG_INFO,"DSLDAPv3PlugIn: DHCP option 95 obtained [%s] LDAP server not added to search policy due to server mappings error.", inServer);
	}
	
} // ConfigDHCPObtainedLDAPServer


// ---------------------------------------------------------------------------
//	* ConfigLDAPServers
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Configs::ConfigLDAPServers ( void )
{
	sInt32					siResult			= eDSNoErr;
	CFStringRef				errorString;
	CFPropertyListRef		configPropertyList	= nil;
	CFMutableDictionaryRef	configDict			= nil;
	CFArrayRef				cfArrayRef			= nil;
	CFIndex					cfConfigCount		= 0;
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
					if ( configVersion == nil ) throw( (sInt32)eDSVersionMismatch ); //KW need eDSPlugInConfigFileError
					if (configVersion != nil)
					{
					
						CShared::LogIt( 0x0F, (char *)"Have successfully read the LDAP XML config file:" );

                        //if config file is up to date with latest default mappings then use them
                        if (strcmp(configVersion,"DSLDAPv3PlugIn Version 1.5") == 0)
                        {
						/*
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
							*/
                        }
                        else
                        {
                            // update the version
                            // replace the default mappings in the configDict with the generated standard ones
                            // write the config file out to pick up the generated default mappings
                            
                            //remove old and add proper version
                            CFDictionaryRemoveValue( configDict, CFSTR( kXMLLDAPVersionKey ) );
                            cfStringRef = CFStringCreateWithCString(kCFAllocatorDefault, "DSLDAPv3PlugIn Version 1.5", kCFStringEncodingUTF8);
                            CFDictionarySetValue( configDict, CFSTR( kXMLLDAPVersionKey ), cfStringRef );
                            CFRelease(cfStringRef);
                            cfStringRef = nil;

							/*
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
							*/
                            
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
								CFDictionaryRef		serverConfigDict	= nil;
								CFDictionaryRef		suppliedServerDict	= nil;
								serverConfigDict = (CFDictionaryRef)::CFArrayGetValueAtIndex( cfArrayRef, iConfigIndex );
									if ( serverConfigDict != nil )
									{
										//here we check the serverConfigDict if it indicates server mappings
										suppliedServerDict = CheckForServerMappings(serverConfigDict);
										if (suppliedServerDict != nil)
										{
											siResult = MakeLDAPConfig(suppliedServerDict, fConfigTableLen);
											CFRelease(suppliedServerDict);
											suppliedServerDict = nil;
										}
										else
										{
											siResult = MakeLDAPConfig(serverConfigDict, fConfigTableLen);
										}
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
	catch ( sInt32 err )
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


//------------------------------------------------------------------------------------
//	* RetrieveServerMappings
//------------------------------------------------------------------------------------

CFDataRef CLDAPv3Configs::RetrieveServerMappings ( char *inServer, char *inMapSearchBase, int inPortNumber, bool inIsSSL )
{
	sInt32				siResult		= eDSNoErr;
	bool				bResultFound	= false;
    int					ldapMsgId		= -1;
	LDAPMessage		   *result			= nil;
	int					ldapReturnCode	= 0;
	char			   *attrs[2]		= {"description",NULL};
	BerElement		   *ber;
	struct berval	  **bValues;
	char			   *pAttr			= nil;
	LDAP			   *serverHost		= nil;
	CFDataRef			ourMappings		= nil;

	if ( (inServer != nil) && (inPortNumber != 0) )
	{
		serverHost = ldap_init( inServer, inPortNumber );

		if (serverHost != nil)
		{
			if (inIsSSL)
			{
				int ldapOptVal = LDAP_OPT_X_TLS_HARD;
				ldap_set_option(serverHost, LDAP_OPT_X_TLS, &ldapOptVal);
			}
			
			if (inMapSearchBase == nil)
			{
				ldapMsgId = ldap_search( serverHost, "", LDAP_SCOPE_SUBTREE, "(&(objectclass=*)(ou=macosxodconfig))", attrs, 0);
			}
			else
			{
				ldapMsgId = ldap_search( serverHost, inMapSearchBase, LDAP_SCOPE_SUBTREE, "(&(objectclass=*)(ou=macosxodconfig))", attrs, 0);
			}
			
			//here is the call to the LDAP server asynchronously which requires
			// host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
			// attribute list (NULL for all), return attrs values flag
			// Note: asynchronous call is made so that a MsgId can be used for future calls
			// This returns us the message ID which is used to query the server for the results
			//TODO KW do we want a retry here?
			if ( ldapMsgId == -1 )
			{
				bResultFound = false;
			}
			else
			{
				bResultFound = true;
				//retrieve the actual LDAP record data for use internally
				//useful only from the read-only perspective
				struct	timeval	tv;
				tv.tv_sec	= 60;
				tv.tv_usec	= 0;
				ldapReturnCode = ldap_result(serverHost, ldapMsgId, 0, &tv, &result);
			}
			
			if ( ( bResultFound ) && ( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
			{
				//get the XML data here
				//parse the attributes in the result - should only be one ie. macosxodmappings
				for (	pAttr = ldap_first_attribute (serverHost, result, &ber );
							pAttr != NULL; pAttr = ldap_next_attribute(serverHost, result, ber ) )
				{
					if (( bValues = ldap_get_values_len (serverHost, result, pAttr )) != NULL)
					{					
						// should be only one value of the attribute
						if ( bValues[0] != NULL )
						{
							ourMappings = CFDataCreate(NULL,(UInt8 *)(bValues[0]->bv_val), bValues[0]->bv_len);
						}
						
						ldap_value_free_len(bValues);
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
					
				ldap_msgfree( result );
				result = nil;

				siResult = eDSNoErr;
			}
			else if (ldapReturnCode == LDAP_TIMEOUT)
			{
				siResult = eDSServerTimeout;
				syslog(LOG_INFO,"DSLDAPv3PlugIn: Retrieval of Server Mappings for [%s] LDAP server has timed out.", inServer);
				if ( result != nil )
				{
					ldap_msgfree( result );
					result = nil;
				}
			}
			else
			{
				siResult = eDSRecordNotFound;
				syslog(LOG_INFO,"DSLDAPv3PlugIn: Server Mappings for [%s] LDAP server not found.", inServer);
				if ( result != nil )
				{
					ldap_msgfree( result );
					result = nil;
				}
			}
			ldap_unbind( serverHost );
		} // if (serverHost != nil)
	} // if ( (inServer != nil) && (inPortNumber != 0) )

	return( ourMappings );

} // RetrieveServerMappings


//------------------------------------------------------------------------------------
//	* WriteServerMappings
//------------------------------------------------------------------------------------

sInt32 CLDAPv3Configs::WriteServerMappings ( char* userName, char* password, CFDataRef inMappings )
{
	sInt32					siResult			= eDSNoErr;
	LDAP				   *serverHost			= nil;
	CFStringRef				errorString;
	CFPropertyListRef		configPropertyList	= nil;
	CFMutableDictionaryRef	serverConfigDict	= nil;
	char				   *server				= nil;
	int						portNumber			= 389;
	char				   *tmpBuff				= nil;
	CFIndex					cfBuffSize			= 1024;
	CFStringRef				cfStringRef			= nil;
	CFBooleanRef			cfBool				= nil;
	CFNumberRef				cfNumber			= nil;
	bool					cfNumBool			= false;
	char				   *mapSearchBase		= nil;
	bool					bIsSSL				= false;
    int						ldapReturnCode 		= 0;
    int						bindMsgId			= 0;
    LDAPMessage			   *result				= nil;
	char				   *ldapDNString		= nil;
	uInt32					ldapDNLength		= 0;
	char				   *ourXMLBlob			= nil;
	char				   *ouvals[2];
	char				   *mapvals[2];
	char				   *ocvals[3];
	LDAPMod					oumod;
	LDAPMod					mapmod;
	LDAPMod					ocmod;
	LDAPMod				   *mods[4];
	
	try
	{	
		if (inMappings != nil)
		{
			// extract the config dictionary from the XML data.
			configPropertyList = CFPropertyListCreateFromXMLData(	kAllocatorDefault,
																	inMappings,
																	kCFPropertyListImmutable,
																	//could also use kCFPropertyListImmutable, kCFPropertyListMutableContainers
																   &errorString);
			
			if (configPropertyList != nil )
			{
				//make the propertylist a dict
				if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
				{
					serverConfigDict = (CFMutableDictionaryRef) configPropertyList; //don't need mutable really
				}
				
				if (serverConfigDict != nil)
				{					
					//KW assume that the extracted strings will be significantly less than 1024 characters
					tmpBuff = (char *)::calloc(1, 1024);
					
					// retrieve all the relevant values (mapsearchbase, IsSSL)
					// to enable server mapping write
					//need to get the server name first
					if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLServerKey ) ) )
					{
						cfStringRef = (CFStringRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLServerKey ) );
						if ( cfStringRef != nil )
						{
							if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
							{
								::memset(tmpBuff,0,1024);
								if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
								{
									server = (char *)::calloc(1+strlen(tmpBuff),1);
									::strcpy(server, tmpBuff);
								}
							}
							//CFRelease(cfStringRef); // no since pointer only from Get
						}
					}
					if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLIsSSLFlagKey ) ) )
					{
						cfBool= (CFBooleanRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLIsSSLFlagKey ) );
						if (cfBool != nil)
						{
							bIsSSL = CFBooleanGetValue( cfBool );
							//CFRelease( cfBool ); // no since pointer only from Get
							if (bIsSSL)
							{
								portNumber = 636;
							}
						}
					}
					if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLPortNumberKey ) ) )
					{
						cfNumber = (CFNumberRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLPortNumberKey ) );
						if ( cfNumber != nil )
						{
							cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &portNumber);
							//CFRelease(cfNumber); // no since pointer only from Get
						}
					}
					
					if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLMapSearchBase ) ) )
					{
						cfStringRef = (CFStringRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLMapSearchBase ) );
						if ( cfStringRef != nil )
						{
							if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
							{
								::memset(tmpBuff,0,1024);
								if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
								{
									mapSearchBase = (char *)::calloc(1+strlen(tmpBuff),1);
									::strcpy(mapSearchBase, tmpBuff);
								}
							}
							//CFRelease(cfStringRef); // no since pointer only from Get
						}
					}
					
					//free up the tmpBuff
					free( tmpBuff );
					tmpBuff = nil;
					// don't release the serverConfigDict since it is the cast configPropertyList
				}//if (serverConfigDict != nil)
				
				CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
				configPropertyList = nil;
			}//if (configPropertyList != nil )

			// JT do something different for SSL here
			serverHost = ldap_init( server, portNumber );
			if ( serverHost == nil ) throw( (sInt32)eDSCannotAccessSession );
			if ( bIsSSL )
			{
				int ldapOptVal = LDAP_OPT_X_TLS_HARD;
				ldap_set_option(serverHost, LDAP_OPT_X_TLS, &ldapOptVal);
			}
			bindMsgId = ldap_bind( serverHost, userName, password, LDAP_AUTH_SIMPLE );
			if ( bindMsgId == -1 )
			{
				throw( (sInt32)eDSCannotAccessSession );
			}

			{
				struct	timeval	tv;
				tv.tv_sec		= 120; //openTO
				tv.tv_usec		= 0;
				ldapReturnCode	= ldap_result(serverHost, bindMsgId, 0, &tv, &result);
			}
			if ( ldapReturnCode == -1 )
			{
				throw( (sInt32)eDSCannotAccessSession );
			}
			else if ( ldapReturnCode == 0 )
			{
				// timed out, let's forget it
				//ldap_abandon(outLDAPHost, bindMsgId);
				ldap_unbind( serverHost );
				serverHost = NULL;
				throw( (sInt32)eDSCannotAccessSession );
			}
			else if ( ldap_result2error(serverHost, result, 1) != LDAP_SUCCESS )
			{
				throw( (sInt32)eDSCannotAccessSession );
			}			

			if ( (serverHost != nil) && (mapSearchBase != nil) )
			{
				//we use "ou" for the DN always:
				//"ou = macosxodconfig, mapSearchBase"
				ldapDNLength = 21 + strlen(mapSearchBase);
				ldapDNString = (char *)calloc(1, ldapDNLength + 1);
				strcpy(ldapDNString,"ou = macosxodconfig, ");
				strcat(ldapDNString,mapSearchBase);
			
				//attempt to delete what is there if anything
				ldapReturnCode = ldap_delete_s( serverHost, ldapDNString);
				if ( ( ldapReturnCode == LDAP_INSUFFICIENT_ACCESS ) || ( ldapReturnCode == LDAP_INVALID_CREDENTIALS ) )
				{
					siResult = eDSPermissionError;
				}
				else if ( ldapReturnCode == LDAP_NO_SUCH_OBJECT )
				{
					siResult = eDSRecordNotFound;
				}
				else if ( ldapReturnCode != LDAP_SUCCESS )
				{
					siResult = eDSBogusServer;
				}
				
				if ( (siResult == eDSRecordNotFound) || (siResult == eDSNoErr) )
				{
					//make the XML blob a manageable char*
					CFRange	aRange;
					aRange.location = 0;
					aRange.length = CFDataGetLength(inMappings);
					ourXMLBlob = (char *) calloc(1, aRange.length + 1);
					CFDataGetBytes( inMappings, aRange, (UInt8*)ourXMLBlob );

					//now attempt to create the record here
					//if it already exists then simply modify the attribute
					ouvals[0]			= "macosxodconfig";
					ouvals[1]			= NULL;
					oumod.mod_op		= 0;
					oumod.mod_type		= "ou";
					oumod.mod_values	= ouvals;
					mapvals[0]			= ourXMLBlob;
					mapvals[1]			= NULL;
					mapmod.mod_op		= 0;
					mapmod.mod_type		= "description";
					mapmod.mod_values	= mapvals;
					ocvals[0]			= "top";
					ocvals[1]			= "organizationalUnit";
					ocvals[2]			= NULL;
					ocmod.mod_op		= 0;
					ocmod.mod_type		= "objectclass";
					ocmod.mod_values	= ocvals;
					mods[0]				= &oumod;
					mods[1]				= &mapmod;
					mods[2]				= &ocmod;
					mods[3]				= NULL;
					ldapReturnCode = 0;
					siResult = eDSNoErr;
					ldapReturnCode = ldap_add_s( serverHost, ldapDNString, mods);
					if ( ( ldapReturnCode == LDAP_INSUFFICIENT_ACCESS ) || ( ldapReturnCode == LDAP_INVALID_CREDENTIALS ) )
					{
						siResult = eDSPermissionError;
					}
					else if ( ldapReturnCode == LDAP_ALREADY_EXISTS )
					{
						siResult = eDSRecordAlreadyExists;
					}
					else if ( ldapReturnCode == LDAP_NO_SUCH_OBJECT )
					{
						siResult = eDSRecordNotFound;
					}
					else if ( ldapReturnCode != LDAP_SUCCESS )
					{
						siResult = eDSBogusServer;
					}
				} //if ( (siResult == eDSRecordNotFound) || (siResult == eDSNoErr) )
			} // if ( (serverHost != nil) && (mapSearchBase != nil) )
		} // inMappings != nil
		
	} // try
	catch ( sInt32 err )
	{
		siResult = err;
		if (configPropertyList != nil)
		{
			CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
			configPropertyList = nil;
		}
	}

	if ( serverHost != nil )
	{
		ldap_unbind( serverHost );
		serverHost = nil;
	}

	if ( mapSearchBase != nil ) 
	{
		free( mapSearchBase );
		mapSearchBase = nil;
	}
			
	if ( ourXMLBlob != nil ) 
	{
		free( ourXMLBlob );
		ourXMLBlob = nil;
	}
			
	if ( ldapDNString != nil ) 
	{
		free( ldapDNString );
		ldapDNString = nil;
	}
			
	return( siResult );

} // WriteServerMappings


//------------------------------------------------------------------------------------
//	* ReadServerMappings
//------------------------------------------------------------------------------------

CFDataRef CLDAPv3Configs::ReadServerMappings ( LDAP *serverHost, CFDataRef inMappings )
{
	sInt32					siResult			= eDSNoErr;
	CFStringRef				errorString;
	CFPropertyListRef		configPropertyList	= nil;
	CFMutableDictionaryRef	serverConfigDict	= nil;
	char				   *configVersion		= nil;
	char				   *tmpBuff				= nil;
	CFIndex					cfBuffSize			= 1024;
	CFStringRef				cfStringRef			= nil;
	CFBooleanRef			cfBool				= false;
	char				   *mapSearchBase		= nil;
	bool					bIsSSL				= false;
	bool					bServerMappings		= false;
	bool					bUseConfig			= false;
	unsigned char			cfNumBool			= false;
	CFNumberRef				cfNumber			= 0;
	char				   *server				= nil;
	int						portNumber			= 389;
	CFDataRef				outMappings			= nil;
	
	//takes in the partial XML config blob and extracts the mappings out of the server to return the true XML config blob
	try
	{	
		if (inMappings != nil)
		{
			// extract the config dictionary from the XML data.
			configPropertyList = CFPropertyListCreateFromXMLData(	kAllocatorDefault,
																	inMappings,
																	kCFPropertyListImmutable,
																	//could also use kCFPropertyListImmutable, kCFPropertyListMutableContainers
																   &errorString);
			
			if (configPropertyList != nil )
			{
				//make the propertylist a dict
				if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
				{
					serverConfigDict = (CFMutableDictionaryRef) configPropertyList; //don't need mutable really
				}
				
				if (serverConfigDict != nil)
				{					
					//config data version
					configVersion = GetVersion(serverConfigDict);
					//TODO KW check for correct version? not necessary really since backward compatible?
					if ( configVersion == nil ) throw( (sInt32)eDSVersionMismatch ); //KW need eDSPlugInConfigFileError
					if (configVersion != nil)
					{
                        if (strcmp(configVersion,"DSLDAPv3PlugIn Version 1.5") == 0)
                        {

							//get relevant parameters out of dict
							if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLEnableUseFlagKey ) ) )
							{
								//KW assume that the extracted strings will be significantly less than 1024 characters
								tmpBuff = (char *)::calloc(1, 1024);
								
								cfBool = (CFBooleanRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLEnableUseFlagKey ) );
								if (cfBool != nil)
								{
									bUseConfig = CFBooleanGetValue( cfBool );
									//CFRelease( cfBool ); // no since pointer only from Get
								}
								//continue if this configuration was enabled by the user
								//no error condition returned if this configuration is not used due to the enable use flag
								if ( bUseConfig )
								{
						
									if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLServerMappingsFlagKey ) ) )
									{
										cfBool = (CFBooleanRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLServerMappingsFlagKey ) );
										if (cfBool != nil)
										{
											bServerMappings = CFBooleanGetValue( cfBool );
											//CFRelease( cfBool ); // no since pointer only from Get
										}
									}
						
									if (bServerMappings)
									{
										// retrieve all the relevant values (server, portNumber, mapsearchbase, IsSSL)
										// to enable server mapping write
										
										if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLServerKey ) ) )
										{
											cfStringRef = (CFStringRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLServerKey ) );
											if ( cfStringRef != nil )
											{
												if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
												{
													::memset(tmpBuff,0,1024);
													if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
													{
														server = (char *)::calloc(1+strlen(tmpBuff),1);
														::strcpy(server, tmpBuff);
													}
												}
												//CFRelease(cfStringRef); // no since pointer only from Get
											}
										}
	
										if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLIsSSLFlagKey ) ) )
										{
											cfBool= (CFBooleanRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLIsSSLFlagKey ) );
											if (cfBool != nil)
											{
												bIsSSL = CFBooleanGetValue( cfBool );
												//CFRelease( cfBool ); // no since pointer only from Get
												if (bIsSSL)
												{
													portNumber = 636; // default for SSL ie. if no port given below
												}
											}
										}
						
										if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLPortNumberKey ) ) )
										{
											cfNumber = (CFNumberRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLPortNumberKey ) );
											if ( cfNumber != nil )
											{
												cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &portNumber);
												//CFRelease(cfNumber); // no since pointer only from Get
											}
										}

										if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLMapSearchBase ) ) )
										{
											cfStringRef = (CFStringRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLMapSearchBase ) );
											if ( cfStringRef != nil )
											{
												if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
												{
													::memset(tmpBuff,0,1024);
													if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
													{
														mapSearchBase = (char *)::calloc(1+strlen(tmpBuff),1);
														::strcpy(mapSearchBase, tmpBuff);
													}
												}
												//CFRelease(cfStringRef); // no since pointer only from Get
											}
										}
									}
						
								}// if ( bUseConfig )
		
								//free up the tmpBuff
								free( tmpBuff );
								tmpBuff = nil;
						
							}// if kXMLEnableUseFlagKey set
                        }                        
						free( configVersion );
						configVersion = nil;
						
					}//if (configVersion != nil)
					
					// don't release the serverConfigDict since it is the cast configPropertyList
					
				}//if (serverConfigDict != nil)
				
				CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
				configPropertyList = nil;
	
			}//if (configPropertyList != nil )
			
			outMappings = RetrieveServerMappings( server, mapSearchBase, portNumber, bIsSSL );

		} // inMappings != nil
		
	} // try
	catch ( sInt32 err )
	{
		siResult = err;
		if (configPropertyList != nil)
		{
			CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
			configPropertyList = nil;
		}
	}

	if ( server != nil ) 
	{
		free( server );
		server = nil;
	}
			
	if ( mapSearchBase != nil ) 
	{
		free( mapSearchBase );
		mapSearchBase = nil;
	}
			
	return( outMappings );

} // ReadServerMappings


// ---------------------------------------------------------------------------
//	* VerifyAndUpdateServerLocation
// ---------------------------------------------------------------------------

CFDataRef CLDAPv3Configs::VerifyAndUpdateServerLocation( char *inServer, int inPortNumber, bool inIsSSL, CFDataRef inXMLData )
{
	CFStringRef				errorString			= nil;
	CFPropertyListRef		configPropertyList	= nil;
	CFMutableDictionaryRef	serverConfigDict	= nil;
	char				   *configVersion		= nil;
	char				   *server				= nil;
	int						portNumber			= 389;
	bool					bIsSSL				= false;
	char				   *tmpBuff				= nil;
	CFStringRef				cfStringRef			= nil;
	bool					bUpdate				= false;
	CFBooleanRef			cfBool				= false;
	CFNumberRef				cfNumber			= 0;
	CFIndex					cfBuffSize			= 1024;
	unsigned char			cfNumBool			= false;
	CFDataRef				outXMLData			= nil;

	if (inXMLData != nil)
	{
		// extract the config dictionary from the XML data.
		configPropertyList = CFPropertyListCreateFromXMLData(	kAllocatorDefault,
																inXMLData,
																kCFPropertyListMutableContainers,
																//could also use kCFPropertyListImmutable, kCFPropertyListImmutable
																&errorString);
		
		if (configPropertyList != nil )
		{
			//make the propertylist a dict
			if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
			{
				serverConfigDict = (CFMutableDictionaryRef) configPropertyList; //don't need mutable really
			}
			
			if (serverConfigDict != nil)
			{
				//get version, and the specific LDAP server config
				
				//config data version
				configVersion = GetVersion(serverConfigDict);
				//bail out of checking in this routine
				if ( configVersion == nil )
				{
					CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
					configPropertyList = nil;
					return nil;
				}
				else
				{
				
					CShared::LogIt( 0x0F, (char *)"Have successfully read the LDAP XML config data" );

					//if config data is up to date with latest default mappings then use them
					if (strcmp(configVersion,"DSLDAPv3PlugIn Version 1.5") == 0)
					{
						//now verify the inServer, inPortNumber and inIsSSL
						
						if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLServerKey ) ) )
						{
							cfStringRef = (CFStringRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLServerKey ) );
							if ( cfStringRef != nil )
							{
								if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
								{
									//KW assume that the extracted strings will be significantly less than 1024 characters
									tmpBuff = (char *)::calloc(1, 1024);
									if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
									{
										server = (char *)::calloc(1+strlen(tmpBuff),1);
										::strcpy(server, tmpBuff);
										if (strcmp(server,inServer) != 0)
										{
											//replace the server value
											bUpdate = true;
											cfStringRef = CFStringCreateWithCString(kCFAllocatorDefault, inServer, kCFStringEncodingUTF8);
											CFDictionaryReplaceValue(serverConfigDict, CFSTR( kXMLServerKey ), cfStringRef);
											CFRelease(cfStringRef);
											cfStringRef = nil;
										}
										free(server);
										server = nil;
									}
									free(tmpBuff);
									tmpBuff = nil;
								}
								//CFRelease(cfStringRef); // no since pointer only from Get
							}
						}
			
						if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLPortNumberKey ) ) )
						{
							cfNumber = (CFNumberRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLPortNumberKey ) );
							if ( cfNumber != nil )
							{
								cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &portNumber);
								if (portNumber != inPortNumber)
								{
									//replace the port number
									bUpdate = true;
									cfNumber = CFNumberCreate(NULL,kCFNumberIntType,&inPortNumber);
									CFDictionaryReplaceValue(serverConfigDict, CFSTR( kXMLPortNumberKey ), cfNumber);
									CFRelease(cfNumber);
									cfNumber = 0;
								}
								//CFRelease(cfNumber); // no since pointer only from Get
							}
						}
			
						if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLIsSSLFlagKey ) ) )
						{
							cfBool= (CFBooleanRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLIsSSLFlagKey ) );
							if (cfBool != nil)
							{
								bIsSSL = CFBooleanGetValue( cfBool );
								if (bIsSSL != inIsSSL)
								{
									//replace the SSL flag
									bUpdate = true;
									if (inIsSSL)
									{
										cfBool = kCFBooleanTrue;
									}
									else
									{
										cfBool = kCFBooleanFalse;
									}
									CFDictionaryReplaceValue(serverConfigDict, CFSTR( kXMLIsSSLFlagKey ), cfBool);						
								}
								//CFRelease( cfBool ); // no since pointer only from Get
							}
						}
						
						if ( CFDictionaryContainsKey( serverConfigDict, CFSTR( kXMLMakeDefLDAPFlagKey ) ) )
						{
							cfBool = (CFBooleanRef)CFDictionaryGetValue( serverConfigDict, CFSTR( kXMLMakeDefLDAPFlagKey ) );
							if (cfBool == kCFBooleanFalse)
							{
								bUpdate = true;
								CFDictionaryReplaceValue(serverConfigDict, CFSTR( kXMLMakeDefLDAPFlagKey ), kCFBooleanTrue);
							}
						}
						else
						{
							bUpdate = true;
							CFDictionarySetValue(serverConfigDict, CFSTR( kXMLMakeDefLDAPFlagKey ), kCFBooleanTrue);
						}
			
						if (bUpdate)
						{
							//create a new XML blob
							outXMLData = CFPropertyListCreateXMLData( kCFAllocatorDefault, serverConfigDict);
						}
					}                        
					delete(configVersion);
				}//if (configVersion != nil)
				// don't release the serverConfigDict since it is the cast configPropertyList
			}//if (serverConfigDict != nil)
			
			CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
			configPropertyList = nil;

		}//if (configPropertyList != nil )
	} // inXMLData != nil
		
	return( outXMLData );

} // VerifyAndUpdateServerLocation

// ---------------------------------------------------------------------------
//	* AddDefaultLDAPServer
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Configs::AddDefaultLDAPServer( CFDataRef inXMLData )
{
	sInt32					siResult			= eDSNoErr;
	CFStringRef				errorString			= nil;
	CFPropertyListRef		configPropertyList	= nil;
	CFMutableDictionaryRef	serverConfigDict	= nil;
	char				   *configVersion		= nil;

	try
	{	
		if (inXMLData != nil)
		{
			// extract the config dictionary from the XML data.
			configPropertyList = CFPropertyListCreateFromXMLData(	kAllocatorDefault,
																	inXMLData,
																	kCFPropertyListImmutable,
																	//could also use kCFPropertyListImmutable, kCFPropertyListMutableContainers
																   &errorString);
			
			if (configPropertyList != nil )
			{
				//make the propertylist a dict
				if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
				{
					serverConfigDict = (CFMutableDictionaryRef) configPropertyList; //don't need mutable really
				}
				
				if (serverConfigDict != nil)
				{
					//get version, and the specific LDAP server config
					
					//config data version
					configVersion = GetVersion(serverConfigDict);
					//TODO KW check for correct version? not necessary really since backward compatible?
					if ( configVersion == nil )
					{
						syslog(LOG_INFO,"DSLDAPv3PlugIn: DHCP option 95 obtained LDAP server mappings is missing the version string.");
						throw( (sInt32)eDSVersionMismatch ); //KW need eDSPlugInConfigFileError
					}
					if (configVersion != nil)
					{
					
						CShared::LogIt( 0x0F, (char *)"Have successfully read the LDAP XML config data" );

                        //if config data is up to date with latest default mappings then use them
                        if (strcmp(configVersion,"DSLDAPv3PlugIn Version 1.5") == 0)
                        {
							siResult = MakeLDAPConfig(serverConfigDict, fConfigTableLen);
                        }
						else
						{
							syslog(LOG_INFO,"DSLDAPv3PlugIn: DHCP option 95 obtained LDAP server mappings contains incorrect version string [%s] instead of [DSLDAPv3PlugIn Version 1.5].", configVersion);
						}
						delete(configVersion);
						
					}//if (configVersion != nil)
					
					// don't release the serverConfigDict since it is the cast configPropertyList
					
				}//if (serverConfigDict != nil)
				
				CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
				configPropertyList = nil;
	
			}//if (configPropertyList != nil )
		} // fXMLData != nil
		
	} // try
	catch ( sInt32 err )
	{
		siResult = err;
		if (configPropertyList != nil)
		{
			CFRelease(configPropertyList); // built from Create on XML data so okay to dealloc here
			configPropertyList = nil;
		}
	}
	return( siResult );

} // AddDefaultLDAPServer

// --------------------------------------------------------------------------------
//	* CheckForServerMappings
// --------------------------------------------------------------------------------

CFDictionaryRef CLDAPv3Configs::CheckForServerMappings ( CFDictionaryRef ldapDict )
{
	char			   *tmpBuff		= nil;
	CFIndex				cfBuffSize	= 1024;
	CFStringRef			cfStringRef	= nil;
	CFBooleanRef		cfBool		= false;
	unsigned char		cfNumBool	= false;
	CFNumberRef			cfNumber	= 0;
	char			   *server		= nil;
	char			   *mapSearchBase = nil;
	int					portNumber	= 389;
	bool				bIsSSL		= false;
	bool				bServerMappings	= false;
	bool				bUseConfig	= false;
	CFDictionaryRef		outDict		= nil;
	CFStringRef			errorString;

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

			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLServerMappingsFlagKey ) ) )
			{
				cfBool = (CFBooleanRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLServerMappingsFlagKey ) );
				if (cfBool != nil)
				{
					bServerMappings = CFBooleanGetValue( cfBool );
					//CFRelease( cfBool ); // no since pointer only from Get
				}
			}

			if (bServerMappings)
			{
				//retrieve all the relevant values (servername, mapsearchbase, portnumber, IsSSL) to enable server mapping retrieval
				
				if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLServerKey ) ) )
				{
					cfStringRef = (CFStringRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLServerKey ) );
					if ( cfStringRef != nil )
					{
						if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
						{
							::memset(tmpBuff,0,1024);
							if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
							{
								server = (char *)::calloc(1+strlen(tmpBuff),1);
								::strcpy(server, tmpBuff);
							}
						}
						//CFRelease(cfStringRef); // no since pointer only from Get
					}
				}
	
				if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLIsSSLFlagKey ) ) )
				{
					cfBool= (CFBooleanRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLIsSSLFlagKey ) );
					if (cfBool != nil)
					{
						bIsSSL = CFBooleanGetValue( cfBool );
						//CFRelease( cfBool ); // no since pointer only from Get
						if (bIsSSL)
						{
							portNumber = 636; // default for SSL ie. if no port given below
						}
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
	
				if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLMapSearchBase ) ) )
				{
					cfStringRef = (CFStringRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLMapSearchBase ) );
					if ( cfStringRef != nil )
					{
						if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
						{
							::memset(tmpBuff,0,1024);
							if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
							{
								mapSearchBase = (char *)::calloc(1+strlen(tmpBuff),1);
								::strcpy(mapSearchBase, tmpBuff);
							}
						}
						//CFRelease(cfStringRef); // no since pointer only from Get
					}
				}
				
				CFDataRef ourXMLData = nil;
				ourXMLData = RetrieveServerMappings( server, mapSearchBase, portNumber, bIsSSL );
				if (ourXMLData != nil)
				{
					CFPropertyListRef configPropertyList = nil;
					// extract the config dictionary from the XML data.
					configPropertyList = CFPropertyListCreateFromXMLData(	kAllocatorDefault,
																			ourXMLData,
																			kCFPropertyListImmutable,
																			//could also use kCFPropertyListImmutable, kCFPropertyListMutableContainers
																		   &errorString);
					
					if (configPropertyList != nil )
					{
						//make the propertylist a dict
						if ( CFDictionaryGetTypeID() == CFGetTypeID( configPropertyList ) )
						{
							outDict = (CFDictionaryRef) configPropertyList;
						}
					}//if (configPropertyList != nil )
					
					CFRelease(ourXMLData);
					ourXMLData = nil;
				}

			}

			if ( server != nil ) 
			{
				free( server );
				server = nil;
			}
			if ( mapSearchBase != nil ) 
			{
				free( mapSearchBase );
				mapSearchBase = nil;
			}
			
		}// if ( bUseConfig )
		
		//free up the tmpBuff
		delete( tmpBuff );

	}// if kXMLEnableUseFlagKey set

	// return if nil or not
	return( outDict );

} // CheckForServerMappings


// --------------------------------------------------------------------------------
//	* MakeLDAPConfig
// --------------------------------------------------------------------------------

sInt32 CLDAPv3Configs::MakeLDAPConfig ( CFDictionaryRef ldapDict, sInt32 inIndex )
{
	sInt32				siResult	= eDSNoErr;
	char			   *tmpBuff		= nil;
	CFIndex				cfBuffSize	= 1024;
//	char			   *outVersion	= nil;
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
	int					opencloseTO	= 120;
	int					idleTO		= 2;
	int					delayRebindTry = 120;
	int					searchTO	= 120;
	int					portNumber	= 389;
	bool				bIsSSL		= false;
	bool				bServerMappings	= false;
	bool				bMakeDefLDAP= false;
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
						if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
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
			
			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLIsSSLFlagKey ) ) )
			{
				cfBool= (CFBooleanRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLIsSSLFlagKey ) );
				if (cfBool != nil)
				{
					bIsSSL = CFBooleanGetValue( cfBool );
					//CFRelease( cfBool ); // no since pointer only from Get
					if (bIsSSL)
					{
						portNumber = 636; // default for SSL ie. if no port given below
					}
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

			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLServerMappingsFlagKey ) ) )
			{
				cfBool = (CFBooleanRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLServerMappingsFlagKey ) );
				if (cfBool != nil)
				{
					bServerMappings = CFBooleanGetValue( cfBool );
					//CFRelease( cfBool ); // no since pointer only from Get
				}
			}

			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLOpenCloseTimeoutSecsKey ) ) )
			{
				cfNumber = (CFNumberRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLOpenCloseTimeoutSecsKey ) );
				if ( cfNumber != nil )
				{
					cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &opencloseTO);
					//CFRelease(cfNumber); // no since pointer only from Get
				}
			}

			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLIdleTimeoutMinsKey ) ) )
			{
				cfNumber = (CFNumberRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLIdleTimeoutMinsKey ) );
				if ( cfNumber != nil )
				{
					cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &idleTO);
					//CFRelease(cfNumber); // no since pointer only from Get
				}
			}

			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLDelayedRebindTrySecsKey ) ) )
			{
				cfNumber = (CFNumberRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLDelayedRebindTrySecsKey ) );
				if ( cfNumber != nil )
				{
					cfNumBool = CFNumberGetValue(cfNumber, kCFNumberIntType, &delayRebindTry);
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
						if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
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
						if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
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
						if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
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

			if ( CFDictionaryContainsKey( ldapDict, CFSTR( kXMLMakeDefLDAPFlagKey ) ) )
			{
				cfBool = (CFBooleanRef)CFDictionaryGetValue( ldapDict, CFSTR( kXMLMakeDefLDAPFlagKey ) );
				if (cfBool != nil)
				{
					bMakeDefLDAP = CFBooleanGetValue( cfBool );
					//CFRelease( cfBool ); // no since pointer only from Get
				}
			}

			//setup the config table
			// MakeLDAPConfigData does not consume the strings passed in so we need to free them below
			pConfig = MakeLDAPConfigData( uiName, server, bUseStdMap, opencloseTO, idleTO, delayRebindTry, searchTO, portNumber, bUseSecure, account, password, bMakeDefLDAP, bServerMappings, bIsSSL );
			
			if (!bUseStdMap) //TODO bUseStdMap will be replaced with templates
			{
				//get the mappings from the config ldap dict
				BuildLDAPMap( pConfig, ldapDict );
			}
			
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

bool CLDAPv3Configs::CheckForConfig ( char *inServerName, uInt32 &inConfigTableIndex )
{
	bool				result 		= false;
    uInt32				iTableIndex	= 0;
    sLDAPConfigData	   *pConfig		= nil;

	if (inServerName != nil)
	{
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
	}
    
    return(result);
	
	
} // CheckForConfig


// --------------------------------------------------------------------------------
//	* BuildLDAPMap
// --------------------------------------------------------------------------------

sInt32 CLDAPv3Configs::BuildLDAPMap ( sLDAPConfigData *inConfig, CFDictionaryRef ldapDict )
{
	sInt32					siResult			= eDSNoErr; // used for?
	CFArrayRef				cfArrayRef			= nil;
//	sMapTuple			   *pRecMapTuple		= nil;
//	sMapTuple			   *pAttrMapTuple		= nil;

	cfArrayRef = nil;
	cfArrayRef = GetRecordTypeMapArray(ldapDict);
	if (cfArrayRef != nil)
	{
		inConfig->fRecordTypeMapCFArray = CFArrayCreateCopy(kCFAllocatorDefault, cfArrayRef);
	}
	
	cfArrayRef = nil;
	cfArrayRef = GetAttributeTypeMapArray(ldapDict);
	if (cfArrayRef != nil)
	{
		inConfig->fAttrTypeMapCFArray = CFArrayCreateCopy(kCFAllocatorDefault, cfArrayRef);
	}
	/*
	pRecMapTuple = BuildMapTuple(cfArrayRef);
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
	*/
	
	return( siResult );

} // BuildLDAPMap


// --------------------------------------------------------------------------------
//	* BuildMapTuple
// --------------------------------------------------------------------------------

sMapTuple *CLDAPv3Configs::BuildMapTuple ( CFArrayRef inArray )
{
	CFArrayRef				cfArrayRef			= nil;
	CFIndex					cfMapCount			= 0;
	CFIndex					cfNativeMapCount	= 0;
	CFIndex					cfSubNativeMapCount	= 0;
	sInt32					iMapIndex			= 0;
	sInt32					iNativeMapIndex		= 0;
	sInt32					iSubNativeMapIndex	= 0;
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
	sPtrString			   *currentSubNativeMap	= nil;
	sInt32					siResult			= eDSNoErr;
	char				   *objectClass			= nil;

	cfArrayRef = inArray;
	
	if (cfArrayRef != nil)
	{
		//now we can retrieve each Type mapping
		cfMapCount = ::CFArrayGetCount( cfArrayRef );
		if (cfMapCount != 0)
		{
			//KW assume that the extracted strings will be significantly less than 1024 characters
			tmpBuff = (char *) calloc(1, 1024);
//			tmpBuff = new char[1024];
//			::memset(tmpBuff,0,1024);
			
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
								if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
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
					//now we need to determine for each array entry whether it is a string(searchbase)
					//or a dictionary(objectclass and searchbase)
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
								//determine whether the array entry is a string or an array
								if (CFGetTypeID(CFArrayGetValueAtIndex( cfNativeArrayRef, iNativeMapIndex )) == CFStringGetTypeID())
								{
									CFStringRef	nativeMapString;
									nativeMapString = (CFStringRef)::CFArrayGetValueAtIndex( cfNativeArrayRef, iNativeMapIndex );
									if ( nativeMapString != nil )
									{
										if ( CFGetTypeID( nativeMapString ) == CFStringGetTypeID() )
										{
											::memset(tmpBuff,0,1024);
											if (CFStringGetCString(nativeMapString, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
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
								}// array entry is a string ie. no ObjectClasses
								else //assume this is a dict since not a string
								{
									CFDictionaryRef subNativeDict;
									subNativeDict = (CFDictionaryRef)CFArrayGetValueAtIndex( cfNativeArrayRef, iNativeMapIndex );
									if (subNativeDict != nil)
									{
										if ( CFGetTypeID( subNativeDict ) == CFDictionaryGetTypeID() )
										{
											CFStringRef searchBase;
											searchBase = (CFStringRef)CFDictionaryGetValue( subNativeDict, CFSTR( kXMLSearchBase ) );
											if (searchBase != nil)
											{
												if ( CFGetTypeID( searchBase ) == CFStringGetTypeID() )
												{
													::memset(tmpBuff,0,1024);
													if (CFStringGetCString(searchBase, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
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
															
															//now deal with the objectclass entries if appropriate
															CFArrayRef objectClasses;
															objectClasses = (CFArrayRef)CFDictionaryGetValue( subNativeDict, CFSTR( kXMLObjectClasses ) );
															if (objectClasses != nil)
															{
																if ( CFGetTypeID( objectClasses ) == CFArrayGetTypeID() )
																{
																	CFStringRef groupOCString = nil;
																	groupOCString = (CFStringRef)CFDictionaryGetValue( subNativeDict, CFSTR( kXMLGroupObjectClasses ) );
																	if ( groupOCString != nil )
																	{
																		if ( CFGetTypeID( groupOCString ) == CFStringGetTypeID() )
																		{
																			if (CFStringCompare( groupOCString, CFSTR("AND"), 0 ) == kCFCompareEqualTo)
																			{
																				currentNativeMap->fGroupSubNative = 0;
																			}
																			else
																			{
																				currentNativeMap->fGroupSubNative = 1;
																			}
																		}
																	}
																	cfSubNativeMapCount = ::CFArrayGetCount( objectClasses );
																	//loop through the object classes
																	for (iSubNativeMapIndex = 0; iSubNativeMapIndex < cfSubNativeMapCount; iSubNativeMapIndex++)
																	{
																		CFStringRef	objectClassString = nil;
																		objectClassString = (CFStringRef)::CFArrayGetValueAtIndex( objectClasses, iSubNativeMapIndex );
																		if ( objectClassString != nil )
																		{
																			if ( CFGetTypeID( objectClassString ) == CFStringGetTypeID() )
																			{
																				::memset(tmpBuff,0,1024);
																				if (CFStringGetCString(objectClassString, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
																				{
																					objectClass = new char[1+strlen(tmpBuff)];
																					::strcpy(objectClass, tmpBuff);
																					//add to the mapping with a native entry
																					if (currentNativeMap->fSubNative == nil)
																					{
																						currentNativeMap->fSubNative = new sPtrString;
																						currentSubNativeMap = currentNativeMap->fSubNative;
																					}
																					else
																					{
																						currentSubNativeMap->pNext = new sPtrString;
																						currentSubNativeMap = currentSubNativeMap->pNext;
																					}
																					if ( currentSubNativeMap != nil )
																					{
																						::memset( currentSubNativeMap, 0, sizeof( sPtrString ) );
																						currentSubNativeMap->fName = objectClass;
																						currentSubNativeMap->pNext = nil;
																					}
																				}// if (CFStringGetCString(searchBase, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
																			}// if ( CFGetTypeID( objectClassString ) == CFStringGetTypeID() )
																		}// if ( objectClassString != nil )
																	}// for (iSubNativeMapIndex = 0; iSubNativeMapIndex < cfSubNativeMapCount; iSubNativeMapIndex++)
																}// if ( CFGetTypeID( objectClasses ) == CFArrayGetTypeID() )
															}// if (objectClasses != nil)
														}// if ( currentNativeMap != nil )									
													}// if (CFStringGetCString(searchBase, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
												}// if ( CFGetTypeID( searchBase ) == CFStringGetTypeID() )
											}
										}
									}
								}								
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

char *CLDAPv3Configs::GetVersion ( CFDictionaryRef configDict )
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
				if (CFStringGetCString(cfStringRef, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
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

CFArrayRef CLDAPv3Configs::GetConfigArray ( CFDictionaryRef configDict )
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

CFArrayRef CLDAPv3Configs::GetDefaultRecordTypeMapArray ( CFDictionaryRef configDict )
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

CFArrayRef CLDAPv3Configs::GetDefaultAttrTypeMapArray ( CFDictionaryRef configDict )
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

CFArrayRef CLDAPv3Configs::GetRecordTypeMapArray ( CFDictionaryRef configDict )
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

CFArrayRef CLDAPv3Configs::GetAttributeTypeMapArray ( CFDictionaryRef configDict )
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

CFArrayRef CLDAPv3Configs::GetNativeTypeMapArray ( CFDictionaryRef configDict )
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

sLDAPConfigData *CLDAPv3Configs::MakeLDAPConfigData (	char *inName, char *inServerName, bool inUseStd,
													int inOpenCloseTO, int inIdleTO, int inDelayRebindTry,
													int inSearchTO, int inPortNum,
													bool inUseSecure,
													char *inAccount, char *inPassword,
													bool inMakeDefLDAP,
													bool inServerMappings,
													bool inIsSSL )
{
	sInt32				siResult		= eDSNoErr;
    sLDAPConfigData	   *configOut		= nil;

	if (inServerName != nil) 
	{
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
			
			configOut->fServerName			= new char[1+::strlen( inServerName )];
			::strcpy(configOut->fServerName, inServerName);
			
			configOut->bUseStdMapping		= inUseStd;
			configOut->fOpenCloseTimeout	= inOpenCloseTO;
			configOut->fIdleTimeout			= inIdleTO;
			configOut->fDelayRebindTry		= inDelayRebindTry;
			configOut->fSearchTimeout		= inSearchTO;
			configOut->fServerPort			= inPortNum;
			configOut->bSecureUse			= inUseSecure;
			configOut->bUpdated				= true;
			configOut->bUseAsDefaultLDAP	= inMakeDefLDAP;
			configOut->bServerMappings		= inServerMappings;
			configOut->bIsSSL				= inIsSSL;
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
			/*
			if (inUseStd)
			{
				configOut->pAttributeMapTuple	= pStdAttributeMapTuple;
				configOut->pRecordMapTuple		= pStdRecordMapTuple;
			}
			*/
			//TODO KW expect the default for the CFArray mapping to be a template
			configOut->fRecordTypeMapCFArray	= 0;
			configOut->fAttrTypeMapCFArray		= 0;
		}
	} // if (inServerName != nil)

	return( configOut );

} // MakeLDAPConfigData


// ---------------------------------------------------------------------------
//	* CleanLDAPConfigData
// ---------------------------------------------------------------------------

sInt32 CLDAPv3Configs::CleanLDAPConfigData ( sLDAPConfigData *inConfig )
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
        //if (!(inConfig->bUseStdMapping))
        //{
        	//CleanMapTuple(inConfig->pAttributeMapTuple);
        	//CleanMapTuple(inConfig->pRecordMapTuple);
        //}
		if (inConfig->fRecordTypeMapCFArray != 0)
		{
			CFRelease(inConfig->fRecordTypeMapCFArray);
			inConfig->fRecordTypeMapCFArray	= 0;
		}
		if (inConfig->fAttrTypeMapCFArray != 0)
		{
			CFRelease(inConfig->fAttrTypeMapCFArray);
			inConfig->fAttrTypeMapCFArray	= 0;
		}
		inConfig->bUseStdMapping		= true;
		inConfig->fOpenCloseTimeout		= 120;
		inConfig->fIdleTimeout			= 2;
		inConfig->fDelayRebindTry		= 120;
		inConfig->fSearchTimeout		= 120;
		inConfig->fServerPort			= 389;
		inConfig->bSecureUse			= false;
		inConfig->bAvail				= false;
		inConfig->bUpdated				= false;
		inConfig->bUseAsDefaultLDAP		= false;
		inConfig->bServerMappings		= false;
		inConfig->bIsSSL				= false;
		if (inConfig->fObjectClassSchema != nil)
		{
			inConfig->fObjectClassSchema->clear();
		}
        
   }

    return( siResult );

} // CleanLDAPConfigData

// ---------------------------------------------------------------------------
//	* ExtractRecMap
// ---------------------------------------------------------------------------

char* CLDAPv3Configs::ExtractRecMap( char *inRecType, CFArrayRef inRecordTypeMapCFArray, int inIndex, bool *outOCGroup, CFArrayRef *outOCListCFArray, ber_int_t* outScope )
{
	char				   *outResult			= nil;
	CFIndex					cfMapCount			= 0;
	CFIndex					cfNativeMapCount	= 0;
	sInt32					iMapIndex			= 0;
	CFStringRef				cfStringRef			= nil;
	CFStringRef				cfRecTypeRef		= nil;
	CFBooleanRef			cfBoolRef			= nil;
	char				   *tmpBuff				= nil;
	CFIndex					cfBuffSize			= 1024;
	CFArrayRef				cfNativeArrayRef	= nil;

	if ( (inRecordTypeMapCFArray != nil) && (inRecType != nil) )
	{
		cfRecTypeRef = CFStringCreateWithCString(kCFAllocatorDefault, inRecType, kCFStringEncodingUTF8);
		
		//now we can look for our Type mapping
		cfMapCount = ::CFArrayGetCount( inRecordTypeMapCFArray );
		if (cfMapCount != 0)
		{
			//loop through the maps
			for (iMapIndex = 0; iMapIndex < cfMapCount; iMapIndex++)
			{
				CFDictionaryRef		typeMapDict;
				typeMapDict = (CFDictionaryRef)::CFArrayGetValueAtIndex( inRecordTypeMapCFArray, iMapIndex );
				if ( typeMapDict != nil )
				{
					//retrieve the mappings
					// get the standard type label first
					if ( CFDictionaryContainsKey( typeMapDict, CFSTR( kXMLStdNameKey ) ) )
					{
						cfStringRef = (CFStringRef)CFDictionaryGetValue( typeMapDict, CFSTR( kXMLStdNameKey ) );
						if ( cfStringRef != nil )
						{
							if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
							{
								if (CFStringCompare(cfStringRef, cfRecTypeRef, 0) == kCFCompareEqualTo)
								{
									// found the std mapping
									// get the native map array of labels next
									cfNativeArrayRef = GetNativeTypeMapArray(typeMapDict);
									//now we need to determine for each array entry whether it is a string(searchbase)
									//or a dictionary(objectclass and searchbase)
									if (cfNativeArrayRef != nil)
									{
										//now we can retrieve each Native Type mapping to the given Standard type
										cfNativeMapCount = ::CFArrayGetCount( cfNativeArrayRef );
										//check here that we have a potential entry
										//ie. std type not nil and an entry in the native map array
										if (cfNativeMapCount != 0)
										{
											//get the inIndex 'th  Native Map
											if ( (inIndex >= 1) && (inIndex <= cfNativeMapCount) )
											{
												//assume that the std type extracted strings will be significantly less than 1024 characters
												tmpBuff = (char *) calloc(1, 1024);
			
												//determine whether the array entry is a string or a dictionary
												if (CFGetTypeID(CFArrayGetValueAtIndex( cfNativeArrayRef, inIndex-1 )) == CFStringGetTypeID())
												{
													CFStringRef	nativeMapString;
													nativeMapString = (CFStringRef)::CFArrayGetValueAtIndex( cfNativeArrayRef, inIndex-1 );
													if ( nativeMapString != nil )
													{
														if (CFStringGetCString(nativeMapString, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
														{
															outResult = (char *) calloc(1, 1+strlen(tmpBuff));
															::strcpy(outResult, tmpBuff);
														}
														//CFRelease(nativeMapString); // no since pointer only from Get
													}// if ( nativeMapString != nil )
													if ( outScope != nil )
													{
														*outScope = LDAP_SCOPE_SUBTREE;
													}
												}// array entry is a string ie. no ObjectClasses
												else //assume this is a dict since not a string
												{
													CFDictionaryRef subNativeDict;
													subNativeDict = (CFDictionaryRef)CFArrayGetValueAtIndex( cfNativeArrayRef, inIndex-1 );
													if (subNativeDict != nil)
													{
														if ( CFGetTypeID( subNativeDict ) == CFDictionaryGetTypeID() )
														{
															CFStringRef searchBase;
															searchBase = (CFStringRef)CFDictionaryGetValue( subNativeDict, CFSTR( kXMLSearchBase ) );
															if (searchBase != nil)
															{
																if ( CFGetTypeID( searchBase ) == CFStringGetTypeID() )
																{
																	::memset(tmpBuff,0,1024);
																	if (CFStringGetCString(searchBase, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
																	{
																		outResult = (char *) calloc(1, 1+strlen(tmpBuff));
																		::strcpy(outResult, tmpBuff);
																		
																		//now deal with the objectclass entries if appropriate
																		CFArrayRef objectClasses;
																		objectClasses = (CFArrayRef)CFDictionaryGetValue( subNativeDict, CFSTR( kXMLObjectClasses ) );
																		if ( (objectClasses != nil) && (outOCListCFArray != nil) && (outOCGroup != nil) )
																		{
																			if ( CFGetTypeID( objectClasses ) == CFArrayGetTypeID() )
																			{
																				*outOCGroup = 0;
																				CFStringRef groupOCString = nil;
																				groupOCString = (CFStringRef)CFDictionaryGetValue( subNativeDict, CFSTR( kXMLGroupObjectClasses ) );
																				if ( groupOCString != nil )
																				{
																					if ( CFGetTypeID( groupOCString ) == CFStringGetTypeID() )
																					{
																						if (CFStringCompare( groupOCString, CFSTR("AND"), 0 ) == kCFCompareEqualTo)
																						{
																							*outOCGroup = 1;
																						}
																					}
																				}
																				//make a copy of the CFArray of the objectClasses
																				*outOCListCFArray = CFArrayCreateCopy(kCFAllocatorDefault, objectClasses);
																			}// if ( CFGetTypeID( objectClasses ) == CFArrayGetTypeID() )
																		}// if (objectClasses != nil)
																	}// if (CFStringGetCString(searchBase, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
																}// if ( CFGetTypeID( searchBase ) == CFStringGetTypeID() )
															}
															if (outScope != nil)
															{
																cfBoolRef = (CFBooleanRef)CFDictionaryGetValue( subNativeDict, CFSTR( kXMLOneLevelSearchScope ) );
																if (cfBoolRef != nil)
																{
																	if (CFBooleanGetValue(cfBoolRef))
																	{
																		*outScope = LDAP_SCOPE_ONELEVEL;
																	}
																	else
																	{
																		*outScope = LDAP_SCOPE_SUBTREE;
																	}
																}
																else
																{
																	*outScope = LDAP_SCOPE_SUBTREE;
																}
															}

														}
													}
												}
												free(tmpBuff);
											}//get the correct indexed Native Map
											
										}// if (cfNativeMapCount != 0)
									}// if (cfNativeArrayRef != nil)
									//done so don't look for any more
									break;
								}
							}
							//CFRelease(cfStringRef); // no since pointer only from Get
						}
					}
					
					//CFRelease( typeMapDict ); // no since pointer only from Get
					
				}//if ( typeMapDict != nil )
				
			} // loop over std rec maps - break above takes us out of this loop
			
		} // if (cfMapCount != 0)
		
		CFRelease(cfRecTypeRef);
		
	} // if (inRecordTypeMapCFArray != nil) ie. an array of Record Maps exists
	
	return( outResult );

} // ExtractRecMap


// ---------------------------------------------------------------------------
//	* ExtractAttrMap
// ---------------------------------------------------------------------------

char* CLDAPv3Configs::ExtractAttrMap( char *inRecType, char *inAttrType, CFArrayRef inRecordTypeMapCFArray, CFArrayRef inAttrTypeMapCFArray, int inIndex )
{
	char				   *outResult				= nil;
	CFIndex					cfMapCount				= 0;
	sInt32					iMapIndex				= 0;
	CFStringRef				cfStringRef				= nil;
	CFStringRef				cfRecTypeRef			= nil;
	CFStringRef				cfAttrTypeRef			= nil;
	CFArrayRef				cfAttrMapArrayRef		= nil;
	bool					bNoRecSpecificAttrMap	= true;

	if ( (inRecordTypeMapCFArray != nil) && (inRecType != nil) && (inAttrType != nil) )
	{
		cfRecTypeRef	= CFStringCreateWithCString(kCFAllocatorDefault, inRecType, kCFStringEncodingUTF8);
		cfAttrTypeRef	= CFStringCreateWithCString(kCFAllocatorDefault, inAttrType, kCFStringEncodingUTF8);
		
		//now we can look for our Type mapping
		cfMapCount = ::CFArrayGetCount( inRecordTypeMapCFArray );
		if (cfMapCount != 0)
		{
			//loop through the maps
			for (iMapIndex = 0; iMapIndex < cfMapCount; iMapIndex++)
			{
				CFDictionaryRef		typeMapDict;
				typeMapDict = (CFDictionaryRef)::CFArrayGetValueAtIndex( inRecordTypeMapCFArray, iMapIndex );
				if ( typeMapDict != nil )
				{
					//retrieve the mappings
					// get the standard type label first
					if ( CFDictionaryContainsKey( typeMapDict, CFSTR( kXMLStdNameKey ) ) )
					{
						cfStringRef = (CFStringRef)CFDictionaryGetValue( typeMapDict, CFSTR( kXMLStdNameKey ) );
						if ( cfStringRef != nil )
						{
							if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
							{
								if (CFStringCompare(cfStringRef, cfRecTypeRef, 0) == kCFCompareEqualTo)
								{
									// found the std mapping
									// get the Attr map array for this std rec map
									cfAttrMapArrayRef = GetAttributeTypeMapArray(typeMapDict);
									outResult = ExtractAttrMapFromArray( cfAttrTypeRef, cfAttrMapArrayRef, inIndex, &bNoRecSpecificAttrMap );
									if (bNoRecSpecificAttrMap)
									{
										//here we search the COMMON attr maps if std attr type not found above
										outResult = ExtractAttrMapFromArray( cfAttrTypeRef, inAttrTypeMapCFArray, inIndex, &bNoRecSpecificAttrMap ); //here don't care about the return of bNoRecSpecificAttrMap
									}
									//done so don't look for any more
									break;
								}
							}
							//CFRelease(cfStringRef); // no since pointer only from Get
						}
					}
					//CFRelease( typeMapDict ); // no since pointer only from Get
				}//if ( typeMapDict != nil )
				
			} // loop over std rec maps - break above takes us out of this loop
			
		} // if (cfMapCount != 0)
		
		CFRelease(cfRecTypeRef);
		CFRelease(cfAttrTypeRef);
		
	} // if (inRecordTypeMapCFArray != nil) ie. an array of Record Maps exists
	
	return( outResult );

} // ExtractAttrMap


// ---------------------------------------------------------------------------
//	* ExtractAttrMapFromArray
// ---------------------------------------------------------------------------

char* CLDAPv3Configs::ExtractAttrMapFromArray( CFStringRef inAttrTypeRef, CFArrayRef inAttrTypeMapCFArray, int inIndex, bool *bNoRecSpecificAttrMap )
{
	char				   *outResult				= nil;
	CFIndex					cfAttrMapCount			= 0;
	CFIndex					cfNativeMapCount		= 0;
	sInt32					iAttrMapIndex			= 0;
	CFStringRef				cfAttrStringRef			= nil;
	char				   *tmpBuff					= nil;
	CFIndex					cfBuffSize				= 1024;
	CFArrayRef				cfNativeMapArrayRef		= nil;

	if ( (inAttrTypeRef != nil) && (inAttrTypeMapCFArray != nil) )
	{
		//now we search for the inAttrType
		cfAttrMapCount = ::CFArrayGetCount( inAttrTypeMapCFArray );
		//check here that we have a potential entry
		//ie. std type not nil and an entry in the native map array
		if (cfAttrMapCount != 0)
		{
			//loop through the Attr maps
			for (iAttrMapIndex = 0; iAttrMapIndex < cfAttrMapCount; iAttrMapIndex++)
			{
				CFDictionaryRef		typeAttrMapDict;
				typeAttrMapDict = (CFDictionaryRef)::CFArrayGetValueAtIndex( inAttrTypeMapCFArray, iAttrMapIndex );
				if ( typeAttrMapDict != nil )
				{
					//retrieve the mappings
					// get the standard Attr type label first
					if ( CFDictionaryContainsKey( typeAttrMapDict, CFSTR( kXMLStdNameKey ) ) )
					{
						cfAttrStringRef = (CFStringRef)CFDictionaryGetValue( typeAttrMapDict, CFSTR( kXMLStdNameKey ) );
						if ( cfAttrStringRef != nil )
						{
							if ( CFGetTypeID( cfAttrStringRef ) == CFStringGetTypeID() )
							{
								if (CFStringCompare(cfAttrStringRef, inAttrTypeRef, 0) == kCFCompareEqualTo)
								{
									*bNoRecSpecificAttrMap = false; //found a rec type map specific attr map
									
									// found the std Attr mapping
									// get the native map array for this std Attr map
									cfNativeMapArrayRef = GetNativeTypeMapArray(typeAttrMapDict);
									if (cfNativeMapArrayRef != nil)
									{
										//now we search for the inAttrType
										cfNativeMapCount = ::CFArrayGetCount( cfNativeMapArrayRef );
										
										if (cfNativeMapCount != 0)
										{
											//get the inIndex 'th  Native Map
											if ( (inIndex >= 1) && (inIndex <= cfNativeMapCount) )
											{
												//assume that the std type extracted strings will be significantly less than 1024 characters
												tmpBuff = (char *) calloc(1, 1024);
			
												//determine whether the array entry is a string
												if (CFGetTypeID(CFArrayGetValueAtIndex( cfNativeMapArrayRef, inIndex-1 )) == CFStringGetTypeID())
												{
													CFStringRef	nativeMapString;
													nativeMapString = (CFStringRef)::CFArrayGetValueAtIndex( cfNativeMapArrayRef, inIndex-1 );
													if ( nativeMapString != nil )
													{
														if (CFStringGetCString(nativeMapString, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
														{
															outResult = (char *) calloc(1, 1+strlen(tmpBuff));
															::strcpy(outResult, tmpBuff);
														}
														//CFRelease(nativeMapString); // no since pointer only from Get
													}// if ( nativeMapString != nil )
												}
												free(tmpBuff);
											}//get the correct indexed Native Map
										}// (cfNativeMapCount != 0)
									}// if (cfNativeMapArrayRef != nil)
									//done so don't look for any more
									break;
								}
							}
							//CFRelease(cfAttrStringRef); // no since pointer only from Get
						}
					}
					//CFRelease( typeAttrMapDict ); // no since pointer only from Get
				}//if ( typeAttrMapDict != nil )
			} //loop over the Attr maps
		}// if (cfAttrMapCount != 0)
	}// if (inAttrTypeMapCFArray != nil)

	return(outResult);
	
} // ExtractAttrMapFromArray


// ---------------------------------------------------------------------------
//	* ExtractStdAttr
// ---------------------------------------------------------------------------

char* CLDAPv3Configs::ExtractStdAttr( char *inRecType, CFArrayRef inRecordTypeMapCFArray, CFArrayRef inAttrTypeMapCFArray, int &inputIndex )
{
	char				   *outResult				= nil;
	CFIndex					cfMapCount				= 0;
	CFIndex					cfAttrMapCount			= 0;
	CFIndex					cfAttrMapCount2			= 0;
	sInt32					iMapIndex				= 0;
	CFStringRef				cfStringRef				= nil;
	CFStringRef				cfRecTypeRef			= nil;
	CFArrayRef				cfAttrMapArrayRef		= nil;
	bool					bUsedIndex				= false;
	char				   *tmpBuff					= nil;
	CFIndex					cfBuffSize				= 1024;
	int						inIndex					= inputIndex;

	if ( (inRecordTypeMapCFArray != nil) && (inRecType != nil) )
	{
		cfRecTypeRef	= CFStringCreateWithCString(kCFAllocatorDefault, inRecType, kCFStringEncodingUTF8);
		
		//now we can look for our Type mapping
		cfMapCount = ::CFArrayGetCount( inRecordTypeMapCFArray );
		if (cfMapCount != 0)
		{
			//loop through the maps
			for (iMapIndex = 0; iMapIndex < cfMapCount; iMapIndex++)
			{
				CFDictionaryRef		typeMapDict;
				typeMapDict = (CFDictionaryRef)::CFArrayGetValueAtIndex( inRecordTypeMapCFArray, iMapIndex );
				if ( typeMapDict != nil )
				{
					//retrieve the mappings
					// get the standard type label first
					if ( CFDictionaryContainsKey( typeMapDict, CFSTR( kXMLStdNameKey ) ) )
					{
						cfStringRef = (CFStringRef)CFDictionaryGetValue( typeMapDict, CFSTR( kXMLStdNameKey ) );
						if ( cfStringRef != nil )
						{
							if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
							{
								if (CFStringCompare(cfStringRef, cfRecTypeRef, 0) == kCFCompareEqualTo)
								{
									// found the std mapping
									// get the Attr map array for this std rec map
									cfAttrMapArrayRef = GetAttributeTypeMapArray(typeMapDict);
									if (cfAttrMapArrayRef != nil)
									{
										cfAttrMapCount = ::CFArrayGetCount( cfAttrMapArrayRef );
										if (cfAttrMapCount != 0)
										{
											//get the inIndex 'th  Native Map
											if ( (inIndex >= 1) && (inIndex <= cfAttrMapCount) )
											{
												bUsedIndex = true;
												//assume that the std type extracted strings will be significantly less than 1024 characters
												tmpBuff = (char *) calloc(1, 1024);
												
												//determine whether the array entry is a dict
												if (CFGetTypeID(CFArrayGetValueAtIndex( cfAttrMapArrayRef, inIndex-1 )) == CFDictionaryGetTypeID())
												{
													CFDictionaryRef	stdAttrTypeDict;
													stdAttrTypeDict = (CFDictionaryRef)CFArrayGetValueAtIndex( cfAttrMapArrayRef, inIndex-1 );
													if ( stdAttrTypeDict != nil )
													{
														if ( CFDictionaryContainsKey( stdAttrTypeDict, CFSTR( kXMLStdNameKey ) ) )
														{
															CFStringRef	attrMapString;
															attrMapString = (CFStringRef)CFDictionaryGetValue( stdAttrTypeDict, CFSTR( kXMLStdNameKey ) );
															if ( attrMapString != nil )
															{
																if (CFStringGetCString(attrMapString, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
																{
																	outResult = (char *) calloc(1, 1+strlen(tmpBuff));
																	::strcpy(outResult, tmpBuff);
																}
																//CFRelease(attrMapString); // no since pointer only from Get
															}// if ( attrMapString != nil )
														} //std attr name present
													} //std attr type dict present
												}
												free(tmpBuff);
											}//get the correct indexed Native Map
										}// (cfAttrMapCount != 0)
									}// if (cfAttrMapArrayRef != nil)
									
									while (!bUsedIndex)
									{
										bUsedIndex = true;
										if (inAttrTypeMapCFArray != nil)
										{
											CFIndex commonIndex = inIndex - cfAttrMapCount;
											cfAttrMapCount2 = ::CFArrayGetCount( inAttrTypeMapCFArray );
											if (cfAttrMapCount2 != 0)
											{
												//get the commonIndex 'th  Native Map
												if ( (commonIndex >= 1) && (commonIndex <= cfAttrMapCount2) )
												{
													//assume that the std type extracted strings will be significantly less than 1024 characters
													tmpBuff = (char *) calloc(1, 1024);
													
													//determine whether the array entry is a dict
													if (CFGetTypeID(CFArrayGetValueAtIndex( inAttrTypeMapCFArray, commonIndex-1 )) == CFDictionaryGetTypeID())
													{
														CFDictionaryRef	stdAttrTypeDict;
														stdAttrTypeDict = (CFDictionaryRef)CFArrayGetValueAtIndex( inAttrTypeMapCFArray, commonIndex-1 );
														if ( stdAttrTypeDict != nil )
														{
															if ( CFDictionaryContainsKey( stdAttrTypeDict, CFSTR( kXMLStdNameKey ) ) )
															{
																CFStringRef	attrMapString;
																attrMapString = (CFStringRef)CFDictionaryGetValue( stdAttrTypeDict, CFSTR( kXMLStdNameKey ) );
																if ( attrMapString != nil )
																{
																	bool bNoDuplicate = true;
																	//this is the Std Attr Name that we compare to if
																	//cfAttrMapCount is not zero ie. there were record specific attr maps that
																	//we do not wish to add to here
																	if ( (cfAttrMapArrayRef != NULL) && (cfAttrMapCount != 0) )
																	{
																		for (sInt32 aIndex = 0; aIndex < cfAttrMapCount; aIndex++)
																		{
																			//determine whether the array entry is a dict
																			if (CFGetTypeID(CFArrayGetValueAtIndex( cfAttrMapArrayRef, aIndex )) == CFDictionaryGetTypeID())
																			{
																				CFDictionaryRef	stdAttrTypeDict;
																				stdAttrTypeDict = (CFDictionaryRef)CFArrayGetValueAtIndex( cfAttrMapArrayRef, aIndex );
																				if ( stdAttrTypeDict != nil )
																				{
																					if ( CFDictionaryContainsKey( stdAttrTypeDict, CFSTR( kXMLStdNameKey ) ) )
																					{
																						CFStringRef	attrMapStringOld;
																						attrMapStringOld = (CFStringRef)CFDictionaryGetValue( stdAttrTypeDict, CFSTR( kXMLStdNameKey ) );
																						if ( attrMapStringOld != nil )
																						{
																							if (CFStringCompare(attrMapStringOld, attrMapString, 0) == kCFCompareEqualTo)
																							{
																								bNoDuplicate	= false;
																								bUsedIndex		= false;
																								inIndex++;
																								break;
																							}
																							//CFRelease(attrMapStringOld); // no since pointer only from Get
																						}// if ( attrMapStringOld != nil )
																					} //std attr name present
																				} //std attr type dict present
																			}
																		}//for (uInt32 aIndex = 0; aIndex < cfAttrMapCount; aIndex++)
																	}
																	if (bNoDuplicate)
																	{
																		if (CFStringGetCString(attrMapString, tmpBuff, cfBuffSize, kCFStringEncodingUTF8))
																		{
																			outResult = (char *) calloc(1, 1+strlen(tmpBuff));
																			::strcpy(outResult, tmpBuff);
																		}
																	}
																	//CFRelease(attrMapString); // no since pointer only from Get
																}// if ( attrMapString != nil )
															} //std attr name present
														} //std attr type dict present
													}
													free(tmpBuff);
												}//get the correct indexed Native Map
											}// (cfAttrMapCount2 != 0)
										}// if (inAttrTypeMapCFArray != nil)
									} //(!bUsedIndex)
									//done so don't look for any more
									break;
								}
							}
							//CFRelease(cfStringRef); // no since pointer only from Get
						}
					}
					//CFRelease( typeMapDict ); // no since pointer only from Get
				}//if ( typeMapDict != nil )
				
			} // loop over std rec maps - break above takes us out of this loop
			
		} // if (cfMapCount != 0)
		
		CFRelease(cfRecTypeRef);
		
	} // if (inRecordTypeMapCFArray != nil) ie. an array of Record Maps exists
	
	if (inIndex != inputIndex)
	{
		inputIndex = inIndex;
	}
	return( outResult );

} // ExtractStdAttr


// ---------------------------------------------------------------------------
//	* AttrMapsCount
// ---------------------------------------------------------------------------

int CLDAPv3Configs::AttrMapsCount( char *inRecType, char *inAttrType, CFArrayRef inRecordTypeMapCFArray, CFArrayRef inAttrTypeMapCFArray )
{
	int						outCount				= 0;
	CFIndex					cfMapCount				= 0;
	sInt32					iMapIndex				= 0;
	CFStringRef				cfStringRef				= nil;
	CFStringRef				cfRecTypeRef			= nil;
	CFStringRef				cfAttrTypeRef			= nil;
	CFArrayRef				cfAttrMapArrayRef		= nil;
	bool					bNoRecSpecificAttrMap	= true;

	if ( (inRecordTypeMapCFArray != nil) && (inRecType != nil) && (inAttrType != nil) )
	{
		cfRecTypeRef	= CFStringCreateWithCString(kCFAllocatorDefault, inRecType, kCFStringEncodingUTF8);
		cfAttrTypeRef	= CFStringCreateWithCString(kCFAllocatorDefault, inAttrType, kCFStringEncodingUTF8);
		
		//now we can look for our Type mapping
		cfMapCount = ::CFArrayGetCount( inRecordTypeMapCFArray );
		if (cfMapCount != 0)
		{
			//loop through the maps
			for (iMapIndex = 0; iMapIndex < cfMapCount; iMapIndex++)
			{
				CFDictionaryRef		typeMapDict;
				typeMapDict = (CFDictionaryRef)::CFArrayGetValueAtIndex( inRecordTypeMapCFArray, iMapIndex );
				if ( typeMapDict != nil )
				{
					//retrieve the mappings
					// get the standard type label first
					if ( CFDictionaryContainsKey( typeMapDict, CFSTR( kXMLStdNameKey ) ) )
					{
						cfStringRef = (CFStringRef)CFDictionaryGetValue( typeMapDict, CFSTR( kXMLStdNameKey ) );
						if ( cfStringRef != nil )
						{
							if ( CFGetTypeID( cfStringRef ) == CFStringGetTypeID() )
							{
								if (CFStringCompare(cfStringRef, cfRecTypeRef, 0) == kCFCompareEqualTo)
								{
									// found the std mapping
									// get the Attr map array for this std rec map
									cfAttrMapArrayRef = GetAttributeTypeMapArray(typeMapDict);
									outCount = AttrMapFromArrayCount( cfAttrTypeRef, cfAttrMapArrayRef, &bNoRecSpecificAttrMap );
									if (bNoRecSpecificAttrMap)
									{
										//here we search the COMMON attr maps if std attr type not found above
										outCount = AttrMapFromArrayCount( cfAttrTypeRef, inAttrTypeMapCFArray, &bNoRecSpecificAttrMap ); //here don't care about the return of bNoRecSpecificAttrMap
									}
									//done so don't look for any more
									break;
								}
							}
							//CFRelease(cfStringRef); // no since pointer only from Get
						}
					}
					//CFRelease( typeMapDict ); // no since pointer only from Get
				}//if ( typeMapDict != nil )
				
			} // loop over std rec maps - break above takes us out of this loop
			
		} // if (cfMapCount != 0)
		
		CFRelease(cfRecTypeRef);
		CFRelease(cfAttrTypeRef);
		
	} // if (inRecordTypeMapCFArray != nil) ie. an array of Record Maps exists
	
	return( outCount );

} // AttrMapsCount


// ---------------------------------------------------------------------------
//	* AttrMapFromArrayCount
// ---------------------------------------------------------------------------

int CLDAPv3Configs::AttrMapFromArrayCount( CFStringRef inAttrTypeRef, CFArrayRef inAttrTypeMapCFArray, bool *bNoRecSpecificAttrMap )
{
	int						outCount				= 0;
	CFIndex					cfAttrMapCount			= 0;
	sInt32					iAttrMapIndex			= 0;
	CFStringRef				cfAttrStringRef			= nil;
	CFArrayRef				cfNativeMapArrayRef		= nil;

	if ( (inAttrTypeRef != nil) && (inAttrTypeMapCFArray != nil) )
	{
		//now we search for the inAttrType
		cfAttrMapCount = ::CFArrayGetCount( inAttrTypeMapCFArray );
		//check here that we have a potential entry
		//ie. std type not nil and an entry in the native map array
		if (cfAttrMapCount != 0)
		{
			//loop through the Attr maps
			for (iAttrMapIndex = 0; iAttrMapIndex < cfAttrMapCount; iAttrMapIndex++)
			{
				CFDictionaryRef		typeAttrMapDict;
				typeAttrMapDict = (CFDictionaryRef)::CFArrayGetValueAtIndex( inAttrTypeMapCFArray, iAttrMapIndex );
				if ( typeAttrMapDict != nil )
				{
					//retrieve the mappings
					// get the standard Attr type label first
					if ( CFDictionaryContainsKey( typeAttrMapDict, CFSTR( kXMLStdNameKey ) ) )
					{
						cfAttrStringRef = (CFStringRef)CFDictionaryGetValue( typeAttrMapDict, CFSTR( kXMLStdNameKey ) );
						if ( cfAttrStringRef != nil )
						{
							if ( CFGetTypeID( cfAttrStringRef ) == CFStringGetTypeID() )
							{
								if (CFStringCompare(cfAttrStringRef, inAttrTypeRef, 0) == kCFCompareEqualTo)
								{
									*bNoRecSpecificAttrMap = false; //found a rec type map specific attr map
									
									// found the std Attr mapping
									// get the native map array for this std Attr map
									cfNativeMapArrayRef = GetNativeTypeMapArray(typeAttrMapDict);
									if (cfNativeMapArrayRef != nil)
									{
										//now we search for the inAttrType
										outCount = ::CFArrayGetCount( cfNativeMapArrayRef );
									}// if (cfNativeMapArrayRef != nil)
									//done so don't look for any more
									break;
								}
							}
							//CFRelease(cfAttrStringRef); // no since pointer only from Get
						}
					}
					//CFRelease( typeAttrMapDict ); // no since pointer only from Get
				}//if ( typeAttrMapDict != nil )
			} //loop over the Attr maps
		}// if (cfAttrMapCount != 0)
	}// if (inAttrTypeMapCFArray != nil)

	return(outCount);
	
} // AttrMapFromArrayCount


void CLDAPv3Configs::XMLConfigLock( void )
{
	if (pXMLConfigLock != nil)
	{
		pXMLConfigLock->Wait();
	}
}

void CLDAPv3Configs::XMLConfigUnlock( void )
{
	if (pXMLConfigLock != nil)
	{
		pXMLConfigLock->Signal();
	}
}
