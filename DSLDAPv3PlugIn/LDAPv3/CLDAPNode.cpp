/*
	File:		CLDAPNode.cpp

	Contains:	LDAP node management class

	Copyright:	© 2001 by Apple Computer, Inc., all rights reserved.

	NOT_FOR_OPEN_SOURCE <to be reevaluated at a later time>

*/

#include <stdio.h>
#include <string.h>		//used for strcpy, etc.
#include <stdlib.h>		//used for malloc
#include <ctype.h>		//use for isprint
#include <syslog.h>		//error logging

#include <DirectoryService/DirServices.h>
#include <DirectoryService/DirServicesUtils.h>
#include <DirectoryService/DirServicesConst.h>

#include "CLDAPNode.h"
#include "CLDAPv3Configs.h"

#define OCSEPCHARS			" '()$"

extern CPlugInRef		   *gConfigTable; 		//TODO need STL type for config table instead
extern uInt32				gConfigTableLen;


// --------------------------------------------------------------------------------
//	* CLDAPNode ()
// --------------------------------------------------------------------------------

CLDAPNode::CLDAPNode ( void )
{
} // CLDAPNode


// --------------------------------------------------------------------------------
//	* ~CLDAPNode ()
// --------------------------------------------------------------------------------

CLDAPNode::~CLDAPNode ( void )
{
} // ~CLDAPNode


// ---------------------------------------------------------------------------
//	* SafeOpen
// ---------------------------------------------------------------------------

sInt32	CLDAPNode::SafeOpen		(	char	   *inNodeName,
									LDAP	  **outLDAPHost,
									uInt32	  *outLDAPConfigTableIndex )
{
	sInt32					siResult		= eDSNoErr;
	sLDAPNodeStruct		   *pLDAPNodeStruct	= nil;
	uInt32					iTableIndex		= 0;
	sLDAPConfigData		   *pConfig			= nil;
    int						ldapPort		= LDAP_PORT;
	char				   *aLDAPName		= nil;
	bool					bConfigFound	= false;
	LDAPNodeMapI			aLDAPNodeMapI;
	string					aNodeName(inNodeName);
	
//if already open then just get host and config index
//if not open then bind to get host and search for config index
//called from OpenDirNode

//allow the inNodeName to have a suffixed ":portNumber" for directed open

//inNodeName is NOT consumed here

	fLDAPNodeOpenMutex.Wait();
	aLDAPNodeMapI	= fLDAPNodeMap.find(aNodeName);
	if (aLDAPNodeMapI == fLDAPNodeMap.end())
	{
		pLDAPNodeStruct = (sLDAPNodeStruct *) calloc(1, sizeof(sLDAPNodeStruct));
		pLDAPNodeStruct->fRefCount = 1;
		pLDAPNodeStruct->fLDAPSessionMutex = new DSMutexSemaphore();
		// don't care if this was originally in the config file or not
		// ie. allow non-configured connections if possible
		// however, they need to use the standard LDAP PORT if no config entry exists
		// search now for possible LDAP port entry
		//Cycle through the gConfigTable to get the LDAP port to use for the ldap_open
		//start at index of one since 0 is generic config
		for (iTableIndex=1; iTableIndex<gConfigTableLen; iTableIndex++)
		{
			pConfig = (sLDAPConfigData *)gConfigTable->GetItemData( iTableIndex );
			if (pConfig != nil)
			{
				if (pConfig->fServerName != nil)
				{
					if (::strcmp(pConfig->fServerName,inNodeName) == 0)
					{
						ldapPort = pConfig->fServerPort;
						bConfigFound = true;
						pLDAPNodeStruct->fLDAPConfigTableIndex = iTableIndex;
						//exit the for loop if entry found
						break;
					} // if name found
				} // if name not nil
			}// if config entry not nil
		} // loop over config table entries
		
		if (!bConfigFound)
		{
			//here we have not found a configuration but will allow the open
			//first check if there is a suffixed ':' port number on the inNodeName
			siResult = ParseLDAPNodeName( inNodeName, &aLDAPName, &ldapPort );
			if (siResult == eDSNoErr)
			{
				pLDAPNodeStruct->fServerName			= aLDAPName;
				pLDAPNodeStruct->fDirectLDAPPort		= ldapPort;
				
				pLDAPNodeStruct->fLDAPConfigTableIndex	= 0;
				//TODO need to access the LDAP server for possible mapping configuration that can be added
				//thus fLDAPConfigTableIndex would then be non-zero
			}
		}
		
		//add this to the fLDAPNodeMap
		fLDAPNodeMap[aNodeName] = pLDAPNodeStruct;
		//fLDAPNodeMap.insert(pair<string, sLDAPNodeStruct*>(aNodeName, pLDAPNodeStruct));
		fLDAPNodeOpenMutex.Signal();
	}
	else
	{
		pLDAPNodeStruct = aLDAPNodeMapI->second;
		fLDAPNodeOpenMutex.Signal();
		pConfig = (sLDAPConfigData *)gConfigTable->GetItemData( pLDAPNodeStruct->fLDAPConfigTableIndex );
		ldapPort = pConfig->fServerPort;
		pLDAPNodeStruct->fRefCount++;
	}
	
	if (siResult == eDSNoErr)
	{
		if (pLDAPNodeStruct->fLDAPSessionMutex != nil)
		{
			pLDAPNodeStruct->fLDAPSessionMutex->Wait();
		}
		//call to bind here
		siResult = BindProc( pLDAPNodeStruct );
		if (pLDAPNodeStruct->fLDAPSessionMutex != nil)
		{
			pLDAPNodeStruct->fLDAPSessionMutex->Signal();
		}
		
		//set the out parameters now
		*outLDAPHost				= pLDAPNodeStruct->fHost;
		*outLDAPConfigTableIndex	= pLDAPNodeStruct->fLDAPConfigTableIndex;
	}

	return(siResult);
} // SafeOpen

// ---------------------------------------------------------------------------
//	* AuthOpen
// ---------------------------------------------------------------------------

sInt32	CLDAPNode::AuthOpen		(	char	   *inNodeName,
									LDAP	   *inHost,
									char	   *inUserName,
									void	   *inAuthCredential,
									char	   *inAuthType,
									LDAP	  **outLDAPHost,
									uInt32	  *inOutLDAPConfigTableIndex,
									bool	   shouldCloseOld )
{
	sInt32					siResult			= eDSNoErr;
	sLDAPNodeStruct		   *pLDAPNodeStruct		= nil;
	sLDAPNodeStruct		   *pLDAPAuthNodeStruct	= nil;
    int						ldapPort			= LDAP_PORT;
	char				   *aLDAPName			= nil;
	LDAPNodeMapI			aLDAPNodeMapI;
	string					aNodeName(inNodeName);

//there must be a fLDAPNodeMap entry for this ie. SafeOpen was already called since
//this would come from a know Node Ref then need to SafeClose the non Auth Open
//this is not ref counted NOR reused
//inOutLDAPConfigTableIndex could be passed in for a directed open ie. not zero
//called from the hierarchy below DoAuthentication

//inNodeName is NOT consumed here

	fLDAPNodeOpenMutex.Wait();
	aLDAPNodeMapI	= fLDAPNodeMap.find(aNodeName);
	if (aLDAPNodeMapI != fLDAPNodeMap.end())
	{
		pLDAPNodeStruct = aLDAPNodeMapI->second;
		
		pLDAPAuthNodeStruct = (sLDAPNodeStruct *) calloc(1, sizeof(sLDAPNodeStruct));
		if (pLDAPNodeStruct->fLDAPConfigTableIndex != 0)
		{
			pLDAPAuthNodeStruct->fLDAPConfigTableIndex = pLDAPNodeStruct->fLDAPConfigTableIndex;
		}
		else
		{
			pLDAPAuthNodeStruct->fServerName		= pLDAPNodeStruct->fServerName;
			pLDAPAuthNodeStruct->fDirectLDAPPort	= pLDAPNodeStruct->fDirectLDAPPort;
			
		}
		pLDAPAuthNodeStruct->fUserName			= inUserName;
		pLDAPAuthNodeStruct->fAuthCredential	= inAuthCredential;
		pLDAPAuthNodeStruct->fAuthType			= inAuthType;
		fLDAPNodeOpenMutex.Signal();

		//call to bind here
		siResult = BindProc( pLDAPAuthNodeStruct );
	
		if (siResult == eDSNoErr)
		{
			//set the out parameters now
			*outLDAPHost				= pLDAPAuthNodeStruct->fHost;
			*inOutLDAPConfigTableIndex	= pLDAPNodeStruct->fLDAPConfigTableIndex;
			//using duplicate of SafeClose(inNodeName) code here directly since within mutex
			if ( shouldCloseOld )
			{
				fLDAPNodeOpenMutex.Wait();
				if (inHost == pLDAPNodeStruct->fHost)
				{
					pLDAPNodeStruct->fRefCount--;
					if (pLDAPNodeStruct->fRefCount == 0)
					{
						//remove the entry if refcount is zero after cleaning contents
						CleanLDAPNodeStruct(pLDAPNodeStruct);
						fLDAPNodeMap.erase(aNodeName);
					}
				}
				fLDAPNodeOpenMutex.Signal();
			}
		}
		
		if (pLDAPAuthNodeStruct != nil)
		{
			free(pLDAPAuthNodeStruct);
			pLDAPAuthNodeStruct = nil;
		}
	}
	else if (inHost != nil)
	//case where a second Auth is made on the Dir Node but
	//original LDAPNodeMap entry was already SafeClosed
	{
		fLDAPNodeOpenMutex.Signal();
		pLDAPAuthNodeStruct = (sLDAPNodeStruct *) calloc(1, sizeof(sLDAPNodeStruct));
		
		//here we have no configuration but will allow the open
		//first check if there is a suffixed ':' port number on the inNodeName
		siResult = ParseLDAPNodeName( inNodeName, &aLDAPName, &ldapPort );
		if (siResult == eDSNoErr)
		{
			pLDAPAuthNodeStruct->fServerName			= aLDAPName;
			pLDAPAuthNodeStruct->fDirectLDAPPort		= ldapPort;
			pLDAPAuthNodeStruct->fLDAPConfigTableIndex	= *inOutLDAPConfigTableIndex;
			//if ( *inOutLDAPConfigTableIndex == 0 )
			//{
				//NO try again to access the LDAP server for possible mapping configuration that can be added
				//SINCE we assume that this cannot be retrieved since attempt will have already been made in
				//the original SafeOpen for this directed open
			//}
				
			//call to bind here
			siResult = BindProc( pLDAPAuthNodeStruct );
		
			if (siResult == eDSNoErr)
			{
				//set the out parameter now
				*outLDAPHost = pLDAPAuthNodeStruct->fHost;
				if ( shouldCloseOld )
				{
					ldap_unbind( inHost );
					inHost = nil;
				}
			}
			else
			{
				siResult = eDSAuthFailed;
			}
		}
		
		if (pLDAPAuthNodeStruct != nil)
		{
			if (pLDAPAuthNodeStruct->fServerName != nil)
			{
				free(pLDAPAuthNodeStruct->fServerName); //only var owned by this temporary struct here
			}
			free(pLDAPAuthNodeStruct);
			pLDAPAuthNodeStruct = nil;
		}
		
	}
	else
	{
		fLDAPNodeOpenMutex.Signal();
		siResult = eDSOpenNodeFailed;
	}
	
	return(siResult);

}// AuthOpen

// ---------------------------------------------------------------------------
//	* RebindSession
// ---------------------------------------------------------------------------

sInt32	CLDAPNode::RebindSession(	char	   *inNodeName,
									LDAP	   *inHost,
									LDAP	  **outLDAPHost )
{
	sInt32					siResult		= eDSNoErr;
	sLDAPNodeStruct		   *pLDAPNodeStruct	= nil;
	LDAPNodeMapI			aLDAPNodeMapI;
	string					aNodeName(inNodeName);

//must already be open
//called from ??

	fLDAPNodeOpenMutex.Wait();
	aLDAPNodeMapI	= fLDAPNodeMap.find(aNodeName);
	if (aLDAPNodeMapI != fLDAPNodeMap.end())
	{
		pLDAPNodeStruct = aLDAPNodeMapI->second;
		//check if host already rebound ie. only relevant when in a Continue
		//since we cannot rebind for this context but a rebind has already taken place
		if (pLDAPNodeStruct->fHost != inHost)
		{
			siResult = eDSOpenNodeFailed;
		}
		
		if (siResult == eDSNoErr)
		{
			if (pLDAPNodeStruct->fHost != nil)
			{
				ldap_unbind(pLDAPNodeStruct->fHost);
				pLDAPNodeStruct->fHost = nil;
			}
			fLDAPNodeOpenMutex.Signal();
			if (pLDAPNodeStruct->fLDAPSessionMutex != nil)
			{
				pLDAPNodeStruct->fLDAPSessionMutex->Wait();
			}
			//call to bind here
			//TODO How many retries do we do?
			siResult = BindProc( pLDAPNodeStruct );
			if (pLDAPNodeStruct->fLDAPSessionMutex != nil)
			{
				pLDAPNodeStruct->fLDAPSessionMutex->Signal();
			}
			
			if (siResult == eDSNoErr)
			{
				//set the out parameters now
				*outLDAPHost = pLDAPNodeStruct->fHost;
			}
		}
		else
		{
			fLDAPNodeOpenMutex.Signal();
		}
	}
	else //no entry in fLDAPNodeMap
	{
		fLDAPNodeOpenMutex.Signal();
		siResult = eDSOpenNodeFailed;
	}
	
	return(siResult);

}// RebindSession

// ---------------------------------------------------------------------------
//	* SimpleAuth
// ---------------------------------------------------------------------------

sInt32	CLDAPNode::SimpleAuth(	char	   *inNodeName,
								char	   *inUserName,
								void	   *inAuthCredential )
{
	sInt32					siResult			= eDSNoErr;
	sLDAPNodeStruct		   *pLDAPAuthNodeStruct	= nil;
	sLDAPNodeStruct		   *pLDAPNodeStruct		= nil;
	LDAPNodeMapI			aLDAPNodeMapI;
	string					aNodeName(inNodeName);

//use bind to do the auth
//called from DoClearTextAuth

	fLDAPNodeOpenMutex.Wait();
	aLDAPNodeMapI	= fLDAPNodeMap.find(aNodeName);
	if (aLDAPNodeMapI != fLDAPNodeMap.end())
	{
		pLDAPNodeStruct = aLDAPNodeMapI->second;
		//build the temporary auth node struct for use with BindProc
		pLDAPAuthNodeStruct = (sLDAPNodeStruct *) calloc(1, sizeof(sLDAPNodeStruct));
		
		if (pLDAPNodeStruct->fLDAPConfigTableIndex != 0)
		{
			pLDAPAuthNodeStruct->fLDAPConfigTableIndex = pLDAPNodeStruct->fLDAPConfigTableIndex;
		}
		else
		{
			pLDAPAuthNodeStruct->fServerName		= pLDAPNodeStruct->fServerName;
			pLDAPAuthNodeStruct->fDirectLDAPPort	= pLDAPNodeStruct->fDirectLDAPPort;
		}
		
		pLDAPAuthNodeStruct->fHost				= nil;	//new session handle for this directed auth
														//don't want this bind to disrupt
														//any other ops on the LDAP server
		pLDAPAuthNodeStruct->fLDAPSessionMutex	= nil;
		pLDAPAuthNodeStruct->fUserName			= inUserName;
		pLDAPAuthNodeStruct->fAuthCredential	= inAuthCredential;
		//fAuthType nil is default for using a password

		//now try to bind
		siResult = BindProc( pLDAPAuthNodeStruct );
		if (siResult != eDSNoErr)
		{
			siResult = eDSAuthFailed;
		}
		//no cleanup of contents other than session handle
		if (pLDAPAuthNodeStruct->fHost != nil)
		{
			ldap_unbind(pLDAPAuthNodeStruct->fHost);
		}
		free(pLDAPAuthNodeStruct);
		fLDAPNodeOpenMutex.Signal();
	}
	else //no entry in fLDAPNodeMap
	{
		fLDAPNodeOpenMutex.Signal();
		siResult = eDSAuthFailed;
	}
	
	return(siResult);

}// SimpleAuth

// ---------------------------------------------------------------------------
//	* RebindAuthSession
// ---------------------------------------------------------------------------

sInt32	CLDAPNode::RebindAuthSession(	char	   *inNodeName,
										LDAP	   *inHost,
										char	   *inUserName,
										void	   *inAuthCredential,
										char	   *inAuthType,
										uInt32		inLDAPConfigTableIndex,
										LDAP	  **outLDAPHost )
{
	sInt32					siResult			= eDSNoErr;
	sLDAPNodeStruct		   *pLDAPAuthNodeStruct	= nil;
    int						ldapPort			= LDAP_PORT;
	char				   *aLDAPName			= nil;

//must already have had a session
//called from ??

	if (inHost != nil)
	//want to use this Auth session for the Dir Node context
	//original LDAPNodeMap entry was already SafeClosed
	{
		pLDAPAuthNodeStruct = (sLDAPNodeStruct *) calloc(1, sizeof(sLDAPNodeStruct));
		
		//here we have no configuration but will allow the open
		//first check if there is a suffixed ':' port number on the inNodeName
		siResult = ParseLDAPNodeName( inNodeName, &aLDAPName, &ldapPort );

		if (siResult == eDSNoErr)
		{
			pLDAPAuthNodeStruct->fServerName			= aLDAPName;
			pLDAPAuthNodeStruct->fDirectLDAPPort		= ldapPort;
			pLDAPAuthNodeStruct->fHost					= nil;
			pLDAPAuthNodeStruct->fLDAPConfigTableIndex	= inLDAPConfigTableIndex;
				
			pLDAPAuthNodeStruct->fUserName				= inUserName;
			pLDAPAuthNodeStruct->fAuthCredential		= inAuthCredential;
			//fAuthType nil is default for using a password

			ldap_unbind(inHost);
			//call to bind here
			//TODO How many retries do we do?
			siResult = BindProc( pLDAPAuthNodeStruct );
		
			if (siResult == eDSNoErr)
			{
				//set the out parameter now
				*outLDAPHost				= pLDAPAuthNodeStruct->fHost;
			}
		}
		
		if (pLDAPAuthNodeStruct != nil)
		{
			if (pLDAPAuthNodeStruct->fServerName != nil)
			{
				free(pLDAPAuthNodeStruct->fServerName); //only var owned by this temporary struct here
			}
			free(pLDAPAuthNodeStruct);
			pLDAPAuthNodeStruct = nil;
		}
		
	}
	else
	{
		siResult = eDSOpenNodeFailed;
	}
	return(siResult);

}// RebindAuthSession

// ---------------------------------------------------------------------------
//	* SafeClose
// ---------------------------------------------------------------------------

sInt32	CLDAPNode::SafeClose	(	char	   *inNodeName,
									LDAP	   *inHost )
{
	sInt32					siResult		= eDSNoErr;
	sLDAPNodeStruct		   *pLDAPNodeStruct	= nil;
	LDAPNodeMapI			aLDAPNodeMapI;
	string					aNodeName(inNodeName);

//decrement ref count and delete if ref count zero
//if Auth call was active then the inHost will NOT be nil and we need to unbind it
//called from CloseDirNode

	fLDAPNodeOpenMutex.Wait();
	if (inHost != nil)
	{
		//this check is for auth call active closes
		ldap_unbind( inHost );
	}
	else
	{
		aLDAPNodeMapI	= fLDAPNodeMap.find(aNodeName);
		if (aLDAPNodeMapI != fLDAPNodeMap.end())
		{
			pLDAPNodeStruct = aLDAPNodeMapI->second;
			pLDAPNodeStruct->fRefCount--;
			
			if (pLDAPNodeStruct->fRefCount == 0)
			{
				//remove the entry if refcount is zero after cleaning contents
				CleanLDAPNodeStruct(pLDAPNodeStruct);
				fLDAPNodeMap.erase(aNodeName);
				free(pLDAPNodeStruct);
			}
			
		}
	}
	fLDAPNodeOpenMutex.Signal();
	
	return(siResult);

}// SafeClose

// ---------------------------------------------------------------------------
//	* GetSchema
// ---------------------------------------------------------------------------

void	CLDAPNode::GetSchema	( sLDAPContextData *inContext )
{
	sInt32					siResult		= eDSNoErr;
	sLDAPConfigData		   *pConfig			= nil;
	LDAPMessage			   *LDAPResult		= nil;
	BerElement			   *ber;
	struct berval		  **bValues;
	char				   *pAttr			= nil;
	sObjectClassSchema	   *aOCSchema		= nil;
	bool					bSkipToTag		= true;
	char				   *lineEntry		= nil;
	char				   *strtokContext	= nil;
	LDAP				   *aHost			= nil;
	
	if ( inContext != nil )
	{
		pConfig = (sLDAPConfigData *)gConfigTable->GetItemData( inContext->fConfigTableIndex );
		
		if (pConfig != nil)
		{
			aHost = LockSession(inContext);
			if ( (aHost != nil) && !(pConfig->bOCBuilt) ) //valid LDAP handle and schema not already built
			{
				//at this point we can make the call to the LDAP server to determine the object class schema
				//then after building the ObjectClassMap we can assign it to the pConfig->fObjectClassSchema
				//in either case we set the pConfig->bOCBuilt since we made the attempt
				siResult = GetSchemaMessage( aHost, pConfig->fSearchTimeout, &LDAPResult);
				
				if (siResult == eDSNoErr)
				{
					//parse the attributes in the LDAPResult - should only be one ie. objectclass
					for (	pAttr = ldap_first_attribute (aHost, LDAPResult, &ber );
							pAttr != NULL; pAttr = ldap_next_attribute(aHost, LDAPResult, ber ) )
					{
						if (( bValues = ldap_get_values_len (aHost, LDAPResult, pAttr )) != NULL)
						{
							ObjectClassMap *aOCClassMap = new(ObjectClassMap);
						
							// for each value of the attribute we need to parse and add as an entry to the objectclass schema map
							for (int i = 0; bValues[i] != NULL; i++ )
							{
								aOCSchema = nil;
								if (lineEntry != nil) //delimiter chars will be overwritten by NULLs
								{
									free(lineEntry);
									lineEntry = nil;
								}
								
								//here we actually parse the values
								lineEntry = (char *)calloc(1,bValues[i]->bv_len+1);
								strcpy(lineEntry, bValues[i]->bv_val);
								
								char	   *aToken			= nil;
								
								//find the objectclass name
								aToken = strtok_r(lineEntry,OCSEPCHARS, &strtokContext);
								while ( (aToken != nil) && (strcmp(aToken,"NAME") != 0) )
								{
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
								}
								if (aToken != nil)
								{
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken != nil)
									{
										//now use the NAME to create an entry
										//first check if that NAME is already present - unlikely
										if (aOCClassMap->find(aToken) == aOCClassMap->end())
										{
											aOCSchema = new(sObjectClassSchema);
											(*aOCClassMap)[aToken] = aOCSchema;
										}
									}
								}
								
								if (aOCSchema == nil)
								{
									continue;
								}
								if (aToken == nil)
								{
									continue;
								}
								//here we have the NAME - at least one of them
								//now check if there are any more NAME values
								bSkipToTag = true;
								while (bSkipToTag)
								{
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken == nil)
									{
										break;
									}
									bSkipToTag = IsTokenNotATag(aToken);
									if (bSkipToTag)
									{
										aOCSchema->fOtherNames.insert(aOCSchema->fOtherNames.begin(),aToken);
									}
								}
								if (aToken == nil)
								{
									continue;
								}
								
								if (strcmp(aToken,"DESC") == 0)
								{
									bSkipToTag = true;
									while (bSkipToTag)
									{
										aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
										if (aToken == nil)
										{
											break;
										}
										bSkipToTag = IsTokenNotATag(aToken);
									}
									if (aToken == nil)
									{
										continue;
									}
								}
								
								if (strcmp(aToken,"OBSOLETE") == 0)
								{
									bSkipToTag = true;
									while (bSkipToTag)
									{
										aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
										if (aToken == nil)
										{
											break;
										}
										bSkipToTag = IsTokenNotATag(aToken);
									}
									if (aToken == nil)
									{
										continue;
									}
								}
		
								if (strcmp(aToken,"SUP") == 0)
								{
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken == nil)
									{
										continue;
									}
									aOCSchema->fParentOCs.insert(aOCSchema->fParentOCs.begin(),aToken);
									//get the other SUP entries
									bSkipToTag = true;
									while (bSkipToTag)
									{
										aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
										if (aToken == nil)
										{
											break;
										}
										bSkipToTag = IsTokenNotATag(aToken);
										if (bSkipToTag)
										{
											aOCSchema->fParentOCs.insert(aOCSchema->fParentOCs.begin(),aToken);
										}
									}
									if (aToken == nil)
									{
										continue;
									}
								}
								
								if (strcmp(aToken,"ABSTRACT") == 0)
								{
									aOCSchema->fType = 0;
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken == nil)
									{
										continue;
									}
								}
								
								if (strcmp(aToken,"STRUCTURAL") == 0)
								{
									aOCSchema->fType = 1;
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken == nil)
									{
										continue;
									}
								}
								
								if (strcmp(aToken,"AUXILIARY") == 0)
								{
									aOCSchema->fType = 2;
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken == nil)
									{
										continue;
									}
								}
								
								if (strcmp(aToken,"MUST") == 0)
								{
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken == nil)
									{
										continue;
									}
									aOCSchema->fRequiredAttrs.insert(aOCSchema->fRequiredAttrs.begin(),aToken);
									//get the other MUST entries
									bSkipToTag = true;
									while (bSkipToTag)
									{
										aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
										if (aToken == nil)
										{
											break;
										}
										bSkipToTag = IsTokenNotATag(aToken);
										if (bSkipToTag)
										{
											aOCSchema->fRequiredAttrs.insert(aOCSchema->fRequiredAttrs.begin(),aToken);
										}
									}
									if (aToken == nil)
									{
										continue;
									}
								}
								
								if (strcmp(aToken,"MAY") == 0)
								{
									aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
									if (aToken == nil)
									{
										continue;
									}
									aOCSchema->fAllowedAttrs.insert(aOCSchema->fAllowedAttrs.begin(),aToken);
									//get the other MAY entries
									bSkipToTag = true;
									while (bSkipToTag)
									{
										aToken = strtok_r(NULL,OCSEPCHARS, &strtokContext);
										if (aToken == nil)
										{
											break;
										}
										bSkipToTag = IsTokenNotATag(aToken);
										if (bSkipToTag)
										{
											aOCSchema->fAllowedAttrs.insert(aOCSchema->fAllowedAttrs.begin(),aToken);
										}
									}
									if (aToken == nil)
									{
										continue;
									}
								}
								
							} // for each bValues[i]
							
							if (lineEntry != nil) //delimiter chars will be overwritten by NULLs
							{
								free(lineEntry);
								lineEntry = nil;
							}
							
							ldap_value_free_len(bValues);
							
							pConfig->fObjectClassSchema = aOCClassMap;
							
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
					
					ldap_msgfree( LDAPResult );
				}
			}
			pConfig->bOCBuilt = true;
			UnLockSession(inContext);
		}
	}

} // GetSchema

// ---------------------------------------------------------------------------
//	* ParseLDAPNodeName
// ---------------------------------------------------------------------------

sInt32	CLDAPNode::ParseLDAPNodeName(	char	   *inNodeName,
										char	  **outLDAPName,
										int		   *outLDAPPort )
{
	sInt32			siResult	= eDSNoErr;
	char		   *portPos		= nil;
	uInt32			inLength	= 0;
	int				ldapPort	= LDAP_PORT;
	char		   *ldapName	= nil;
	
//parse a string with a name and possibly a suffix of ':' followed by a port number

	if (inNodeName != nil)
	{
		inLength	= strlen(inNodeName);
		portPos		= strchr(inNodeName, ':');
		//check if ':' found
		if (portPos != nil)
		{
			portPos++;
			//check if nothing after ':'
			if (portPos != nil)
			{
				ldapPort = strtoul(portPos,NULL,NULL);
				//if error in conversion set back to default
				if (ldapPort == 0)
				{
					ldapPort = LDAP_PORT;
				}
				
				inLength = inLength - strlen(portPos);					
			}
			//strip off the suffix ':???'
			ldapName = (char *) calloc(1, inLength);
			strncpy(ldapName, inNodeName, inLength-1);
		}
		else
		{
			ldapName = (char *) calloc(1, inLength+1);
			strncpy(ldapName, inNodeName, inLength);
		}
		
		*outLDAPName	= ldapName;
		*outLDAPPort	= ldapPort;
	}
	else
	{
		siResult = eDSNullParameter;
	}

	return(siResult);

}// ParseLDAPNodeName

// ---------------------------------------------------------------------------
//	* CleanLDAPNodeStruct
// ---------------------------------------------------------------------------

sInt32	CLDAPNode::CleanLDAPNodeStruct	( sLDAPNodeStruct *inLDAPNodeStruct )
{
	sInt32 siResult = eDSNoErr;

//assumes struct has ownership of all members
	if (inLDAPNodeStruct != nil)
	{
		if (inLDAPNodeStruct->fLDAPSessionMutex != nil)
		{
			inLDAPNodeStruct->fLDAPSessionMutex->Wait();
		}
		if (inLDAPNodeStruct->fHost != nil)
		{
			ldap_unbind( inLDAPNodeStruct->fHost );
			inLDAPNodeStruct->fHost = nil;
		}
		if (inLDAPNodeStruct->fLDAPSessionMutex != nil)
		{
			inLDAPNodeStruct->fLDAPSessionMutex->Signal();
			delete(inLDAPNodeStruct->fLDAPSessionMutex);
			inLDAPNodeStruct->fLDAPSessionMutex = nil;
		}
		
		if (inLDAPNodeStruct->fServerName != nil)
		{
			free( inLDAPNodeStruct->fServerName );
			inLDAPNodeStruct->fServerName = nil;
		}
		
		if (inLDAPNodeStruct->fUserName != nil)
		{
			free( inLDAPNodeStruct->fUserName );
			inLDAPNodeStruct->fUserName = nil;
		}
		
		if (inLDAPNodeStruct->fAuthCredential != nil)
		{
			free( inLDAPNodeStruct->fAuthCredential );
			inLDAPNodeStruct->fAuthCredential = nil;
		}

		if (inLDAPNodeStruct->fAuthType != nil)
		{
			free( inLDAPNodeStruct->fAuthType );
			inLDAPNodeStruct->fAuthType = nil;
		}
	}

	return(siResult);

}// CleanLDAPNodeStruct

// ---------------------------------------------------------------------------
//	* BindProc
// ---------------------------------------------------------------------------

sInt32 CLDAPNode::BindProc ( sLDAPNodeStruct *inLDAPNodeStruct )
{

    sInt32				siResult		= eDSNoErr;
    int					bindMsgId		= 0;
	int					version			= -1;
    sLDAPConfigData	   *pConfig			= nil;
    char			   *ldapAcct		= nil;
    char			   *ldapPasswd		= nil;
    int					openTO			= 0;
	LDAP			   *inLDAPHost		= inLDAPNodeStruct->fHost;
	LDAPMessage		   *result			= nil;
	int					ldapReturnCode	= 0;
	int					retryDelayTime	= 0;

	try
	{
		if ( inLDAPNodeStruct == nil ) throw( (sInt32)eDSNullParameter );
		
		if (inLDAPNodeStruct->fLDAPSessionMutex != nil)
		{
			inLDAPNodeStruct->fLDAPSessionMutex->Wait();
		}
        // Here is the bind to the LDAP server
		// Note that there may be stored name/password in the config table
		// ie. always use the config table data if authentication has not explicitly been set
		// use LDAPAuthNodeMap if inLDAPNodeStruct contains a username
		
		//check that we were already here
		if (inLDAPHost == NULL)
		{
			//retrieve the config data
			//don't need to retrieve for the case of "generic unknown" so don't check index 0
			if (( inLDAPNodeStruct->fLDAPConfigTableIndex < gConfigTableLen) && ( inLDAPNodeStruct->fLDAPConfigTableIndex >= 1 ))
			{
				pConfig = (sLDAPConfigData *)gConfigTable->GetItemData( inLDAPNodeStruct->fLDAPConfigTableIndex );
				if (pConfig != nil)
				{
					if ( (pConfig->bSecureUse) && (inLDAPNodeStruct->fUserName == nil) )
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
					else
					{
						if (inLDAPNodeStruct->fUserName != nil)
						{
							ldapAcct = new char[1+::strlen(inLDAPNodeStruct->fUserName)];
							::strcpy( ldapAcct, inLDAPNodeStruct->fUserName );
						}
						if (inLDAPNodeStruct->fAuthCredential != nil)
						{
							if (inLDAPNodeStruct->fAuthType != nil)
							{
								//auth type of clear text means char * password
								if (strcmp(inLDAPNodeStruct->fAuthType,kDSStdAuthClearText) == 0)
								{
									ldapPasswd = new char[1+::strlen((char*)(inLDAPNodeStruct->fAuthCredential))];
									::strcpy( ldapPasswd, (char*)(inLDAPNodeStruct->fAuthCredential) );
								}
							}
							else //default is password
							{
								ldapPasswd = new char[1+::strlen((char*)(inLDAPNodeStruct->fAuthCredential))];
								::strcpy( ldapPasswd, (char*)(inLDAPNodeStruct->fAuthCredential) );
							}
						}
					}
					openTO		= pConfig->fOpenCloseTimeout;
				}
			}

			if (inLDAPNodeStruct->fLDAPConfigTableIndex != 0)
			{
				if (pConfig != nil)
				{
					inLDAPHost = ldap_init( pConfig->fServerName, pConfig->fServerPort );
				}
			}
			else
			{
				inLDAPHost = ldap_init( inLDAPNodeStruct->fServerName, inLDAPNodeStruct->fDirectLDAPPort );
			}
			
			if ( inLDAPHost == nil ) throw( (sInt32)eDSCannotAccessSession );
			
			if (pConfig != nil)
			{
				if ( pConfig->bIsSSL )
				{
					int ldapOptVal = LDAP_OPT_X_TLS_HARD;
					ldap_set_option(inLDAPHost, LDAP_OPT_X_TLS, &ldapOptVal);
				}
			}
			/* LDAPv3 only */
			version = LDAP_VERSION3;
			ldap_set_option( inLDAPHost, LDAP_OPT_PROTOCOL_VERSION, &version );
			
			//heuristic to prevent many consecutive failures with long timeouts
			//ie. forcing quick failures after first failure during a window of
			//the same length as the timeout value
			fLDAPNodeOpenMutex.Wait();
			if ( inLDAPNodeStruct->bHasFailed )
			{
				if ( time( nil ) < inLDAPNodeStruct->fDelayedBindTime )
				{
					fLDAPNodeOpenMutex.Signal();
					throw( (sInt32)eDSCannotAccessSession );
				}
				else
				{
					inLDAPNodeStruct->bHasFailed = false;
					//fDelayedBindTime then is unused so no need to reset
				}
			}
			fLDAPNodeOpenMutex.Signal();

			//this is our and only our LDAP session for now
			//need to use our timeout so we don't hang indefinitely
			bindMsgId = ldap_bind( inLDAPHost, ldapAcct, ldapPasswd, LDAP_AUTH_SIMPLE );
			
			if (openTO == 0)
			{
				ldapReturnCode = ldap_result(inLDAPHost, bindMsgId, 0, NULL, &result);
			}
			else
			{
				struct	timeval	tv;
				tv.tv_sec		= openTO;
				tv.tv_usec		= 0;
				ldapReturnCode	= ldap_result(inLDAPHost, bindMsgId, 0, &tv, &result);
			}

			if ( ldapReturnCode == -1 )
			{
				throw( (sInt32)eDSCannotAccessSession );
			}
			else if ( ldapReturnCode == 0 )
			{
				// timed out, let's forget it
				ldap_abandon(inLDAPHost, bindMsgId);
				if ( openTO < 120 )
				{
					retryDelayTime = 120; //case where LDAP user has specified low timeout value so defaulting this
				}
				else
				{
					retryDelayTime	= openTO;
				}
				//log this timed out connection
				if (pConfig != nil)
				{
					syslog(LOG_INFO,"DSLDAPv3PlugIn: Timed out in attempt to bind to [%s] LDAP server.", pConfig->fServerName);
					syslog(LOG_INFO,"DSLDAPv3PlugIn: Disabled future attempts to bind to [%s] LDAP server for next %d seconds.", pConfig->fServerName, retryDelayTime);
				}
				else
				{
					syslog(LOG_INFO,"DSLDAPv3PlugIn: Timed out in attempt to bind to [%s] LDAP server.", inLDAPNodeStruct->fServerName);
					syslog(LOG_INFO,"DSLDAPv3PlugIn: Disabled future attempts to bind to [%s] LDAP server for next %d seconds.", inLDAPNodeStruct->fServerName, retryDelayTime);
				}
				fLDAPNodeOpenMutex.Wait();
				inLDAPNodeStruct->bHasFailed = true;
				inLDAPNodeStruct->fDelayedBindTime = time( nil ) + retryDelayTime;
				fLDAPNodeOpenMutex.Signal();
				throw( (sInt32)eDSCannotAccessSession );
			}
			else if ( ldap_result2error(inLDAPHost, result, 1) != LDAP_SUCCESS )
			{
				fLDAPNodeOpenMutex.Wait();
				inLDAPNodeStruct->fHost = inLDAPHost;
				fLDAPNodeOpenMutex.Signal();
				throw( (sInt32)eDSCannotAccessSession );
			}
			fLDAPNodeOpenMutex.Wait();
			inLDAPNodeStruct->fHost = inLDAPHost;
			fLDAPNodeOpenMutex.Signal();

			//result is consumed above within ldap_result2error
			result = nil;
		}
		
	} // try
	
	catch ( sInt32 err )
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
	
	if (inLDAPNodeStruct->fLDAPSessionMutex != nil)
	{
		inLDAPNodeStruct->fLDAPSessionMutex->Signal();
	}

	return (siResult);
	
}// BindProc


//------------------------------------------------------------------------------------
//	* GetSchemaMessage
//------------------------------------------------------------------------------------

sInt32 CLDAPNode::GetSchemaMessage ( LDAP *inHost, int inSearchTO, LDAPMessage **outResultMsg )
{
	sInt32				siResult		= eDSNoErr;
	bool				bResultFound	= false;
    int					ldapMsgId		= 0;
	LDAPMessage		   *result			= nil;
	int					ldapReturnCode	= 0;
	char			   *sattrs[2]		= {"subschemasubentry",NULL};
	char			   *attrs[2]		= {"objectclasses",NULL};
	char			   *subschemaDN		= nil;
	BerElement		   *ber;
	struct berval	  **bValues;
	char			   *pAttr			= nil;

	try
	{
		//search for the specific LDAP record subschemasubentry at the rootDSE which contains
		//the "dn" of the subschema record
		
		// here is the call to the LDAP server asynchronously which requires
		// host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
		// attribute list (NULL for all), return attrs values flag
		// Note: asynchronous call is made so that a MsgId can be used for future calls
		// This returns us the message ID which is used to query the server for the results
		if ( (ldapMsgId = ldap_search( inHost, "", LDAP_SCOPE_BASE, "(objectclass=*)", sattrs, 0) ) == -1 )
		{
			bResultFound = false;
		}
		else
		{
			bResultFound = true;
			//retrieve the actual LDAP record data for use internally
			//useful only from the read-only perspective
			if (inSearchTO == 0)
			{
				ldapReturnCode = ldap_result(inHost, ldapMsgId, 0, NULL, &result);
			}
			else
			{
				struct	timeval	tv;
				tv.tv_sec	= inSearchTO;
				tv.tv_usec	= 0;
				ldapReturnCode = ldap_result(inHost, ldapMsgId, 0, &tv, &result);
			}
		}
	
		if (	(bResultFound) &&
				( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
		{
			siResult = eDSNoErr;
			//get the subschemaDN here
			//parse the attributes in the result - should only be one ie. subschemasubentry
			for (	pAttr = ldap_first_attribute (inHost, result, &ber );
						pAttr != NULL; pAttr = ldap_next_attribute(inHost, result, ber ) )
			{
				if (( bValues = ldap_get_values_len (inHost, result, pAttr )) != NULL)
				{					
					// should be only one value of the attribute
					if ( bValues[0] != NULL )
					{
						subschemaDN = (char *) calloc(1, bValues[0]->bv_len + 1);
						strcpy(subschemaDN,bValues[0]->bv_val);
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
		
		if (subschemaDN != nil)
		{
			//here we call to get the actual subschema record
			
			//here is the call to the LDAP server asynchronously which requires
			// host handle, search base, search scope(LDAP_SCOPE_SUBTREE for all), search filter,
			// attribute list (NULL for all), return attrs values flag
			// Note: asynchronous call is made so that a MsgId can be used for future calls
			// This returns us the message ID which is used to query the server for the results
			if ( (ldapMsgId = ldap_search( inHost, subschemaDN, LDAP_SCOPE_BASE, "(objectclass=subSchema)", attrs, 0) ) == -1 )
			{
				bResultFound = false;
			}
			else
			{
				bResultFound = true;
				//retrieve the actual LDAP record data for use internally
				//useful only from the read-only perspective
				//KW when write capability is added, we will need to re-read the result after a write
				if (inSearchTO == 0)
				{
					ldapReturnCode = ldap_result(inHost, ldapMsgId, 0, NULL, &result);
				}
				else
				{
					struct	timeval	tv;
					tv.tv_sec	= inSearchTO;
					tv.tv_usec	= 0;
					ldapReturnCode = ldap_result(inHost, ldapMsgId, 0, &tv, &result);
				}
			}
			
			free(subschemaDN);
			subschemaDN = nil;
		
			if (	(bResultFound) &&
					( ldapReturnCode == LDAP_RES_SEARCH_ENTRY ) )
			{
				siResult = eDSNoErr;
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
	}

	catch ( sInt32 err )
	{
		siResult = err;
	}

	if (result != nil)
	{
		*outResultMsg = result;
	}

	return( siResult );

} // GetSchemaMessage


//------------------------------------------------------------------------------------
//	* IsTokenNotATag
//------------------------------------------------------------------------------------

bool CLDAPNode::IsTokenNotATag ( char *inToken )
{
	
	if (inToken == nil)
	{
		return true;
	}
	
	//check for first char in inToken as an uppercase letter in the following set
	//"NDOSAMX" since that will cover the following tags
	//NAME,DESC,OBSOLETE,SUP,ABSTRACT,STRUCTURAL,AUXILIARY,MUST,MAY,X-ORIGIN

	switch(*inToken)
	{
		case 'N':
		case 'D':
		case 'O':
		case 'S':
		case 'A':
		case 'M':
		case 'X':
		
			if (strcmp(inToken,"DESC") == 0)
			{
				return false;
			}
		
			if (strcmp(inToken,"SUP") == 0)
			{
				return false;
			}
			
			if (strlen(inToken) > 7)
			{
				if (strcmp(inToken,"OBSOLETE") == 0)
				{
					return false;
				}
		
				if (strcmp(inToken,"ABSTRACT") == 0)
				{
					return false;
				}
			
				if (strcmp(inToken,"STRUCTURAL") == 0)
				{
					return false;
				}
			
				if (strcmp(inToken,"AUXILIARY") == 0)
				{
					return false;
				}

				if (strcmp(inToken,"X-ORIGIN") == 0) //appears that iPlanet uses a non-standard tag ie. post RFC 2252
				{
					return false;
				}
			}
		
			if (strcmp(inToken,"MUST") == 0)
			{
				return false;
			}
		
			if (strcmp(inToken,"MAY") == 0)
			{
				return false;
			}
		
			if (strcmp(inToken,"NAME") == 0)
			{
				return false;
			}
			break;
		default:
			break;
	}

	return( true );

} // IsTokenNotATag

// ---------------------------------------------------------------------------
//	* Lock Session
// ---------------------------------------------------------------------------

LDAP* CLDAPNode::LockSession( sLDAPContextData *inContext )
{
	sLDAPNodeStruct		   *pLDAPNodeStruct		= nil;
	LDAPNodeMapI			aLDAPNodeMapI;

	if (inContext != nil)
	{
		if (inContext->authCallActive)
		{
			if (inContext->fLDAPSessionMutex != nil)
			{
				inContext->fLDAPSessionMutex->Wait();
			}
			return inContext->fHost;
		}
		else
		{
			string aNodeName(inContext->fName);
			fLDAPNodeOpenMutex.Wait();
			aLDAPNodeMapI	= fLDAPNodeMap.find(aNodeName);
			if (aLDAPNodeMapI != fLDAPNodeMap.end())
			{
				pLDAPNodeStruct = aLDAPNodeMapI->second;
				pLDAPNodeStruct->fRefCount++;
			}
			fLDAPNodeOpenMutex.Signal();
		
			if (pLDAPNodeStruct != nil)
			{
				if (pLDAPNodeStruct->fLDAPSessionMutex != nil)
				{
					pLDAPNodeStruct->fLDAPSessionMutex->Wait();
				}
				return pLDAPNodeStruct->fHost;
			}
		}
	}
	return nil;
} // LockSession

// ---------------------------------------------------------------------------
//	* UnLock Session
// ---------------------------------------------------------------------------

void CLDAPNode::UnLockSession( sLDAPContextData *inContext )
{
	sLDAPNodeStruct		   *pLDAPNodeStruct		= nil;
	LDAPNodeMapI			aLDAPNodeMapI;

	if (inContext != nil)
	{
		if (inContext->authCallActive)
		{
			if (inContext->fLDAPSessionMutex != nil)
			{
				inContext->fLDAPSessionMutex->Signal();
			}
		}
		else
		{
			string aNodeName(inContext->fName);
			fLDAPNodeOpenMutex.Wait();
			aLDAPNodeMapI	= fLDAPNodeMap.find(aNodeName);
			if (aLDAPNodeMapI != fLDAPNodeMap.end())
			{
				pLDAPNodeStruct = aLDAPNodeMapI->second;
			}
		
			if (pLDAPNodeStruct != nil)
			{
				if (pLDAPNodeStruct->fLDAPSessionMutex != nil)
				{
					pLDAPNodeStruct->fLDAPSessionMutex->Signal();
				}
				pLDAPNodeStruct->fRefCount--;
				if (pLDAPNodeStruct->fRefCount == 0)
				{
					//remove the entry if refcount is zero after cleaning contents
					CleanLDAPNodeStruct(pLDAPNodeStruct);
					fLDAPNodeMap.erase(aNodeName);
				}
			}
			fLDAPNodeOpenMutex.Signal();
		}
	}
} //UnLockSession

