/*
 *  slp_reg.c
 *  NSLPlugins
 *
 *  Created by karnold on Fri Nov 10 2000.
 *  Copyright (c) 2000 __CompanyName__. All rights reserved.
 *
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "slp.h"
#include "SLPDefines.h"
#include "SLPComm.h"

OSStatus DoSLPRegistration( char* scopeList, UInt32 scopeListLen, char* url, UInt32 urlLen, char* attributeList, UInt32 attributeListLen );
OSStatus DoSLPDeregistration( char* scopeList, UInt32 scopeListLen, char* url, UInt32 urlLen );
void PrintHelpInfo( void );

#define	kMaxURLLen				1024
#define	kMaxAttributeLen		1024
#define kMaxArgs		7		// [-r url] [-d url] [-a attribute_list] [-l]
int main(int argc, char *argv[])
{
	char*		scope = "";
	UInt32		scopeLen = strlen(scope);
	char		url[kMaxURLLen] = {0};
	UInt32		urlLen = 0;
	char		attributeList[kMaxAttributeLen] = {0};
	UInt32		attributeListLen = 0;
	Boolean		regFlagSet = false, deregFlagSet = false, listFlagSet = false, attributeFlagSet = false;
	OSStatus	status = 0;
	
	if ( argc > kMaxArgs || argc == 0 )
    {
        PrintHelpInfo();
        return -1;
    }
	
	for ( int i=1; i<argc; i++ )		// skip past [0]
	{
		if ( ( strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "-d") == 0 ) && !regFlagSet && !deregFlagSet ) 
		{
			i++;		// increment this as the next attribute is the url and we want the for loop to skip this
			urlLen = strlen(argv[i]);
			
			if ( urlLen >= kMaxURLLen )
			{
				PrintHelpInfo();
				return -1;
			}
			else
				strcpy( url, argv[i] );
				
			if ( strcmp(argv[i-1], "-r") == 0 )
				regFlagSet = true;
			else
				deregFlagSet = true;

		}
		else if ( i >=2 && strcmp(argv[i], "-a") == 0 && !attributeFlagSet )
		{
			if ( !regFlagSet && !deregFlagSet )
			{
				PrintHelpInfo();
				return -1;
			}
			
			i++;		// increment this as the next attribute is the url and we want the for loop to skip this

            attributeListLen = strlen(argv[i]);
			
			if ( attributeListLen >= kMaxAttributeLen )
			{
				PrintHelpInfo();
				return -1;
			}
			else
			{
                strcpy( attributeList, argv[i] );
			}
            	
			attributeFlagSet = true;
		}
		else if ( strcmp(argv[i], "-l") == 0 )
		{
			listFlagSet = true;
		}
		else
		{
			PrintHelpInfo();
			return -1;
		}
	}
	
	if ( regFlagSet )
		status = DoSLPRegistration( scope, scopeLen, url, urlLen, attributeList, attributeListLen );
	else if ( deregFlagSet )
		status = DoSLPDeregistration( scope, scopeLen, url, urlLen );
	
	if ( listFlagSet )
	{
		// we'll need to print out currently registered services here
		fprintf( stderr, "we'll need to print out currently registered services here" );	
	}	
	
	return status; 
}

OSStatus DoSLPRegistration( char* scopeList, UInt32 scopeListLen, char* url, UInt32 urlLen, char* attributeList, UInt32 attributeListLen )
{
	char*		dataBuffer = NULL;
	char*		returnBuffer = NULL;
	UInt32		dataBufferLen = 0;
	UInt32		returnBufferLen = 0;
	OSStatus	status = noErr;
	
	dataBuffer = MakeSLPRegistrationDataBuffer ( scopeList, scopeListLen, url, urlLen, attributeList, attributeListLen, &dataBufferLen );

	if ( dataBuffer )
	{
		status = SendDataToSLPd( dataBuffer, dataBufferLen, &returnBuffer, &returnBufferLen );
	}
	else
		status = memFullErr;
			
	// now check for any message status
	if ( !status && returnBuffer && returnBufferLen > 0 )
		status = ((SLPdMessageHeader*)returnBuffer)->messageStatus;
		
	return status;
}

OSStatus DoSLPDeregistration( char* scopeList, UInt32 scopeListLen, char* url, UInt32 urlLen )
{
	char*		dataBuffer = NULL;
	char*		returnBuffer = NULL;
	UInt32		dataBufferLen = 0;
	UInt32		returnBufferLen = 0;
	OSStatus	status = noErr;
	
	dataBuffer = MakeSLPDeregistrationDataBuffer ( scopeList, scopeListLen, url, urlLen, &dataBufferLen );

	if ( dataBuffer )
	{
		status = SendDataToSLPd( dataBuffer, dataBufferLen, &returnBuffer, &returnBufferLen );
	}
	else
		status = memFullErr;
			
	// now check for any message status
	if ( !status && returnBuffer && returnBufferLen > 0 )
		status = ((SLPdMessageHeader*)returnBuffer)->messageStatus;
		
	return status;
}

void PrintHelpInfo( void )
{
	fprintf( stderr,
		"Usage: slp_reg [-r|d <url>] [-a <attribute-list>] [-l]\n"
		"  where each of the following is optional:\n"
		"  -r <url> is a url the user wishes to register\n"
		"  -d <url> is a url the user wishes to deregister\n"
		"  -a <attribute-list> is a slp attribute list e.g. \"(a=1,2),boo,(c=false)\"\n"
		"  -l to receive as output a list of currently registered items (after tool is complete)\n" );
}
