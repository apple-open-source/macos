/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
 
/*!
 *  @header SLPInternalProcessListener
 *  A thread that will actively listen for communications from our SA Plugin or
 *  RAdmin plugin to administer this deamon
 */
 
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/un.h>

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "mslpd_store.h"
#include "mslp_dat.h"
#include "mslpd.h"
#include "slpipc.h"

#include <DirectoryServiceCore/DSLThread.h>
#include <DirectoryService/DirServicesTypes.h>

#include "SLPDefines.h"
#include "SLPComm.h"
#include "SLPRegistrar.h"
#include "SLPDAAdvertiser.h"
#include "SLPDARegisterer.h"
#include "SLPInternalProcessListener.h"

static SLPInternalProcessListener* gIPL = NULL;

int InitializeInternalProcessListener( SAState* psa )
{
    OSStatus	status = SLP_OK;

    if ( !gIPL )
    {
#ifdef ENABLE_SLP_LOGGING
        SLP_LOG( SLP_LOG_DEBUG, "RunSLPInternalProcessListener is creating a new listener" );
#endif        
        gIPL = new SLPInternalProcessListener( psa, &status );
	
        if ( !gIPL )
            status = eMemoryAllocError;
        else
        {
            status = gIPL->Initialize();
			
            if ( status )
            {
                delete gIPL;
                gIPL = NULL;
            }
        }
    }
    
    return (int)status;
}

// This is our C function wrapper to start this threaded object from the main mslp code
int RunSLPInternalProcessListener( SAState* psa )
{
    OSStatus	status = SLP_OK;

    if ( !gIPL )
    status = InitializeInternalProcessListener( psa );
    
    if ( !status )
        gIPL->Resume();
            
    return (int)status;
}

SLPInternalProcessListener::SLPInternalProcessListener( SAState* psa, OSStatus *status )
{
    mServerState = psa;
    mSLPSA = NULL;
    
    mSelfPtr = this;
}

SLPInternalProcessListener::~SLPInternalProcessListener()
{
    mSelfPtr = NULL;

    if ( mSLPSA )
        SLPClose( mSLPSA );
}

OSStatus SLPInternalProcessListener::Initialize()
{
    int						servlen;
    struct sockaddr_un		serv_addr;
    OSStatus				status;
    
    if ( (mSockfd = socket( AF_LOCAL, SOCK_STREAM, 0)) < 0 )
    {
        status = mSockfd;

        SLP_LOG( SLP_LOG_FAIL, "SLPInternalProcessListener: can't open stream socket: %s", strerror(errno));
    }
    else
    {
        char 	tmp[1024];

        unlink(kSLPdPath);
        
        bzero( (char*)&serv_addr, sizeof(serv_addr));
        serv_addr.sun_family = AF_LOCAL;
        strcpy( serv_addr.sun_path, kSLPdPath );

        servlen = sizeof(serv_addr);	// Richard Stevens Unix Network Programming pg 379


        status = bind( mSockfd, (struct sockaddr*)&serv_addr, servlen );
                
        if ( status < 0 )
        {
            sprintf( tmp, "SLPInternalProcessListener: can't bind local IPC address: %s, %s", kSLPdPath, strerror(errno) );
            SLPLOG(SLP_LOG_FAIL, tmp);
        }
        else
        {
            status = chmod( kSLPdPath, S_IRWXU + S_IRGRP + S_IWGRP + S_IROTH + S_IWOTH );
            if ( status )
            {
                sprintf( tmp, "SLPInternalProcessListener: error calling chmod on the IPC socket: %s, error: %s", kSLPdPath, strerror(errno) );
                SLP_LOG( SLP_LOG_ERR, tmp );
            }
        }
    }
	
	return status;
}

void* SLPInternalProcessListener::Run()
{
    socklen_t			clientLen = 0;
	int					newsockfd = 0;
    struct 				sockaddr_un	cli_addr;
    
#ifdef ENABLE_SLP_LOGGING
    SLP_LOG( SLP_LOG_DEBUG, "SLPInternalProcessListener::Run() calling listen for next control call" );
#endif
    listen( mSockfd, 5 );

    for ( ; ; )
    {
        clientLen = sizeof(cli_addr);
	    newsockfd = accept(mSockfd, (struct sockaddr*)&cli_addr, &clientLen);

        if ( IsProcessTerminated() )
        {
            return NULL;
        }

        if ( newsockfd < 0 )
        {
            if ( IsProcessTerminated() )
            {
                return NULL;
            }
            else if ( errno == EINTR )
            {
#ifdef ENABLE_SLP_LOGGING
                SLP_LOG( SLP_LOG_DROP, "slpd: accept received EINTR");
#endif
                continue;				// back to for
            }
            else
            {
                SLP_LOG( SLP_LOG_ERR, "slpd: accept error on IPC socket: %s", strerror(errno) );
                
                // try reinitializing ourselves before bailing...
                if ( this->Initialize() != noErr )
                {
                    SLP_LOG( SLP_LOG_FAIL, "Reinitialization of our IPC socket failed, slpd cannot continue" );
                    return NULL;		// if something is wrong with the socket, should we bail?
                }
                else
                    continue;
            }
        }
        else
        {
            SLPInternalProcessHandlerThread*	newHandler = new SLPInternalProcessHandlerThread();
            
            if ( newHandler )
            {
                newHandler->Initialize( newsockfd, mServerState );
                newHandler->Resume();
            }
        }
    }

    return NULL;
}


SLPInternalProcessHandlerThread::SLPInternalProcessHandlerThread()
    : DSLThread()
{

}

SLPInternalProcessHandlerThread::~SLPInternalProcessHandlerThread()
{
    close( mRequestSD );
}

void SLPInternalProcessHandlerThread::Initialize( SOCKET newRequest, SAState* serverState )
{
    mRequestSD = newRequest;
    mServerState = serverState;
}

void* SLPInternalProcessHandlerThread::Run()
{
    HandleCommunication();
    
    return NULL;
}

void SLPInternalProcessHandlerThread::HandleCommunication()
{
    char				internalBuffer[kMaxSizeOfParam];
    int					readBufLen;
    SLPdMessageType			messageType;
    OSStatus				status = noErr;
    Boolean				propertyListIsDirty = false;		// if we set any properties, update the config file
    
    readBufLen = read( mRequestSD, internalBuffer, kMaxSizeOfParam );

    if ( IsProcessTerminated() )
    {
        return;
    }

    if ( readBufLen < 0 )
    {
        if ( errno == EINTR )
        {
#ifdef ENABLE_SLP_LOGGING
            SLP_LOG( SLP_LOG_DROP, "slpd: read received EINTR");
#endif
        }
        else
        {
            SLP_LOG( SLP_LOG_ERR, "slpd: read error on IPC socket: %s", strerror(errno) );
        }
        
        return;
    }

    messageType = GetSLPMessageType( internalBuffer );
#ifdef ENABLE_SLP_LOGGING
    mslplog( SLP_LOG_DEBUG, "Received IPC message: ", PrintableHeaderType(internalBuffer) );
#endif    
    switch ( messageType )
    {
#pragma mark case kSLPStartUp:
		case kSLPStartUp:
		{
			// nothing to do, we already up and running!
		}
		break;
		
#pragma mark case kSLPShutDown:
		case kSLPShutDown:
		{
#ifdef ENABLE_SLP_LOGGING
            SLP_LOG( SLP_LOG_STATE, "slpd: received a quit command.");
#endif
			exit(0);
		}
		break;
		
#pragma mark case kSLPRegisterURL:
#pragma mark case kSLPDeregisterURL:
        case kSLPRegisterURL:
        case kSLPDeregisterURL:
        {
            char*		urlPtr = NULL;
            char*		scopeListPtr = NULL;
            char*		scopeList = NULL;
            char*		attributeListPtr = NULL;
            UInt32		urlLen, scopeListLen, attributeListLen;

            status = GetSLPRegDeregDataFromBuffer( messageType, internalBuffer, &scopeListPtr, &scopeListLen, &urlPtr, &urlLen, &attributeListPtr, &attributeListLen );

            if ( status )
            {
#ifdef ENABLE_SLP_LOGGING
                SLP_LOG( SLP_LOG_DEBUG, "SLPInternalProcessHandlerThread::HandleCommunication: error returned from GetSLPRegistrationDataFromBuffer!");
#endif
            }
            else
            {
                if ( scopeListPtr && scopeListLen > 0 )
					scopeList = (char*)malloc( scopeListLen+1 );
				else
					scopeList = (char*)malloc( strlen(SLPGetProperty("com.apple.slp.defaultRegistrationScope")) +1 );
					
                if ( scopeListLen > 0 )
                    memcpy( scopeList, scopeListPtr, scopeListLen );
				else
					strcpy( scopeList, SLPGetProperty("com.apple.slp.defaultRegistrationScope") );
					
                scopeList[scopeListLen] = '\0';
                
                char	attributeList[1024] = "";			
                
                if ( messageType == kSLPRegisterURL )
                {
                    if ( scopeList && scopeList[0] != '\0' )
                        sprintf( attributeList, "(scopes=%s)", scopeList );
                
                    if ( attributeListLen > 0 )
                    {
                        strcat( attributeList, "," );
                        memcpy( &attributeList[strlen(attributeList)], attributeListPtr, attributeListLen );
                        attributeList[strlen(attributeList)+attributeListLen] = '\0';
                    }
            
                    RegData*	newReg = new RegData( urlPtr, urlLen, scopeListPtr, scopeListLen, attributeList, strlen(attributeList) );
                    
                    RegisterNewService( newReg );		// takes responsibility of newReg
                }
                else
                {
                    RegData*	newReg = new RegData( urlPtr, urlLen, scopeListPtr, scopeListLen, attributeList, strlen(attributeList) );
                    
                    DeregisterService( newReg );		// takes responsibility of newReg
                }
                
                SLPdMessageHeader		returnBuf;
                returnBuf.messageType = messageType;
                returnBuf.messageLength = sizeof(SLPdMessageHeader);
                returnBuf.messageStatus = status;
                
                write( mRequestSD, &returnBuf, returnBuf.messageLength );		// send back our result					

                if ( scopeList )
                    free( scopeList );
            }
        }
        break;
		
#pragma mark case kSLPTurnOnDA:
#pragma mark case kSLPTurnOffDA:
        case kSLPTurnOnDA:
        case kSLPTurnOffDA:
        {            
            // we need to reset the slpd by pointing at the proper config file
            if ( messageType == kSLPTurnOnDA )
            {
                TurnOnDA();
                propertyListIsDirty = true;
                
                if ( AreWeADirectoryAgent() )
                {
                    InitSLPRegistrar();						// this resets it if it already has data
                    ResetStatelessBootTime();
                    StartSLPUDPListener( mServerState );
                    StartSLPTCPListener( mServerState );
                    StartSLPDAAdvertiser( mServerState );
                }
                else
                    status = SLP_REFRESH_REJECTED;
            }
            else
            {
                propertyListIsDirty = true;
                
                TearDownSLPRegistrar();					// this also sets the boottime to zero for the next call...
                StartSLPDAAdvertiser( mServerState );	// this will send out an advert saying we are shutting down
                StopSLPDAAdvertiser();
                TurnOffDA();							// have to do this after we advertise, otherwise the advertisement is skipped if we aren't a DA!
            }
            
            SLPdMessageHeader		returnBuf;
            returnBuf.messageType = messageType;
            returnBuf.messageLength = sizeof(SLPdMessageHeader);
            returnBuf.messageStatus = status;
            
            write( mRequestSD, &returnBuf, returnBuf.messageLength );		// send back our result					
        }
        
#pragma mark case kSLPDAStatusQuery:
        case kSLPDAStatusQuery:
        {
            UInt32		bufferLen = 0;
            char*		buffer = NULL;
            SLPDAStatus daStatus;
            
            if ( SLPGetProperty("com.apple.slp.isDA") && !SDstrcasecmp(SLPGetProperty("com.apple.slp.isDA"),"true") )
                daStatus = kSLPDARunning;
            else
                daStatus = kSLPDANotRunning;
            
            MakeSLPDAStatus( daStatus, &bufferLen, &buffer  );
            write( mRequestSD, buffer, bufferLen );		// send back our result	
            free( buffer );				
        }
        break;
        
#pragma mark case kSLPGetScopeList:
        case kSLPGetScopeList:
        {
            SLPRegistrar::TheSLPR()->SendRAdminAllScopes();

            SLPdMessageHeader		returnBuf;
            returnBuf.messageType = messageType;
            returnBuf.messageLength = sizeof(SLPdMessageHeader);
            returnBuf.messageStatus = noErr;
            
            write( mRequestSD, &returnBuf, returnBuf.messageLength );		// send back our result					
        }
        break;
        
#pragma mark case kSLPSetScopeList:
        case kSLPSetScopeList:
        {
            SLPdMessageHeader*	messageHeader = (SLPdMessageHeader*)internalBuffer;
            char*				newScopeList = NULL;

            if ( messageHeader->messageLength - sizeof(SLPdMessageHeader) > 0 )
                newScopeList = (char*)malloc( messageHeader->messageLength - sizeof(SLPdMessageHeader) + 1 );
            
            if ( newScopeList )
            {
                memcpy( newScopeList, (char*)messageHeader + sizeof(SLPdMessageHeader), messageHeader->messageLength - sizeof(SLPdMessageHeader) );
                newScopeList[messageHeader->messageLength - sizeof(SLPdMessageHeader)] = '\0';
            }
 
            SLPRegistrar::TheSLPR()->SendRAdminDeleteAllScopes();		// force all these to go as we are resetting our scopes
            
            SLPSetProperty( "com.apple.slp.daScopeList", newScopeList );

            SLPBoolean 	needToSetOverflow = SLP_FALSE;
            
            while ( strlen( newScopeList ) > kMaxScopeListLenForUDP )	// as long as we have a scope list that is too long
            {
                // message is too long
                needToSetOverflow = SLP_TRUE;
                
                char* tempPtr = strrchr( newScopeList, ',' );
                
                if ( !tempPtr )
                {
                    free( newScopeList );
#ifdef ENABLE_SLP_LOGGING
                    SLP_LOG( SLP_LOG_DROP, "we can't fit a single scope in our advertisement!" );			// bad error
#endif
                    break;
                }
                
                if ( tempPtr && *tempPtr == ',' )
                    *tempPtr = '\0';				// chop off here and try again
            }
        
            if ( needToSetOverflow == SLP_TRUE )
                SLPSetProperty( "com.apple.slp.daPrunedScopeList", newScopeList );
            else
                SLPSetProperty( "com.apple.slp.daPrunedScopeList", NULL );

            propertyListIsDirty = true;
            
#ifdef ENABLE_SLP_LOGGING
            mslplog( SLP_LOG_RADMIN, "Setting property: \"com.apple.slp.daScopeList\" to", newScopeList );
#endif
            SLPdMessageHeader		returnBuf;
            returnBuf.messageType = messageType;
            returnBuf.messageLength = sizeof(SLPdMessageHeader);
            returnBuf.messageStatus = kSLPNullBufferErr;				// they can't set the scope list to nothing!
            
            write( mRequestSD, &returnBuf, returnBuf.messageLength );		// send back our result					

            SLPRegistrar::TheSLPR()->SendRAdminAllScopes();
            
            if ( AreWeADirectoryAgent() )
            {                
                ResetStatelessBootTime();
                StartSLPDAAdvertiser( mServerState );					// will update if currently running
            }
        }
        break;
        
#pragma mark case kSLPAddScope:
        case kSLPAddScope:
        {
            SLPdMessageHeader*	messageHeader = (SLPdMessageHeader*)internalBuffer;
            const char*			daScopeList = SLPGetProperty("com.apple.slp.daScopeList");
            char*				newScope = NULL;
            int					newScopeLen = messageHeader->messageLength - sizeof(SLPdMessageHeader);
            char*				daScopeListTemp = NULL;
            int					daScopeListTempLen = 0;
            OSStatus			status = noErr;
            
            if ( daScopeList && strlen( daScopeList ) + newScopeLen < kMaxSizeOfParam )
            {
                if ( messageHeader->messageLength - sizeof(SLPdMessageHeader) > 0 )
                    newScope = (char*)malloc( messageHeader->messageLength - sizeof(SLPdMessageHeader) + 1 );
                
                if ( newScope )
                {
                    memcpy( newScope, (char*)messageHeader + sizeof(SLPdMessageHeader), messageHeader->messageLength - sizeof(SLPdMessageHeader) );
                    newScope[messageHeader->messageLength - sizeof(SLPdMessageHeader)] = '\0';
                
                    daScopeListTemp = safe_malloc(strlen(daScopeList)+strlen(newScope)+1,daScopeList,strlen(daScopeList));
                    assert( daScopeListTemp );
                    
                    daScopeListTempLen = strlen( daScopeListTemp );
                    list_merge( newScope, &daScopeListTemp, &daScopeListTempLen, 1 );
    
                    if ( strcmp( daScopeListTemp, daScopeList ) != 0 )
                    {
                        SLPSetProperty( "com.apple.slp.daScopeList", daScopeListTemp );

                        SLPBoolean 	needToSetOverflow = SLP_FALSE;
                        
                        while ( strlen( daScopeListTemp ) > kMaxScopeListLenForUDP )	// as long as we have a scope list that is too long
                        {
                            // message is too long
                            needToSetOverflow = SLP_TRUE;
                            
                            char* tempPtr = strrchr( daScopeListTemp, ',' );
                            
                            if ( !tempPtr )
                            {
                                free( daScopeListTemp );
#ifdef ENABLE_SLP_LOGGING
                                SLP_LOG( SLP_LOG_DROP, "we can't fit a single scope in our advertisement!" );			// bad error
#endif
                                break;
                            }
                            
                            if ( tempPtr && *tempPtr == ',' )
                                *tempPtr = '\0';				// chop off here and try again
                        }
                    
                        if ( needToSetOverflow == SLP_TRUE )
                        {
                            SLPSetProperty( "com.apple.slp.daPrunedScopeList", daScopeListTemp );
#ifdef ENABLE_SLP_LOGGING
                            SLP_LOG( SLP_LOG_RADMIN, "We have a scope list that is longer than can be advertised via multicasting.  Some SLP implementations may see a truncated list." );
#endif
                        }
                        else
                            SLPSetProperty( "com.apple.slp.daPrunedScopeList", NULL );

#ifdef ENABLE_SLP_LOGGING
                        mslplog( SLP_LOG_RADMIN, "Setting property: \"com.apple.slp.daScopeList\" to", daScopeListTemp );
#endif 
                        propertyListIsDirty = true;
                   }

                    if ( daScopeListTemp )
                        free( daScopeListTemp );
                    
                    SLPRegistrar::TheSLPR()->SendRAdminAddedScope( newScope );
                    
                    if ( newScope )
                        free( newScope );
                }
                else
                    status = kSLPNullBufferErr;				// they can't set the scope list to nothing!
            }
            else
                status = SCOPE_LIST_TOO_LONG;
                
            SLPdMessageHeader		returnBuf;
            returnBuf.messageType = messageType;
            returnBuf.messageLength = sizeof(SLPdMessageHeader);
            returnBuf.messageStatus = status;
            
            write( mRequestSD, &returnBuf, returnBuf.messageLength );		// send back our result					

            if ( !status && AreWeADirectoryAgent() )
            {
                ResetStatelessBootTime();
                StartSLPDAAdvertiser( mServerState );					// will update if currently running
            }
        }
        break;
        
#pragma mark case kSLPDeleteScope:
        case kSLPDeleteScope:
        {
            SLPdMessageHeader*	messageHeader = (SLPdMessageHeader*)internalBuffer;
            const char*			daScopeList = SLPGetProperty("com.apple.slp.daScopeList");
            char*				scopeToDelete = NULL;
            char*				daScopeListTemp = NULL;
            OSStatus			status = noErr;
            
            if ( messageHeader->messageLength - sizeof(SLPdMessageHeader) > 0 )
                scopeToDelete = (char*)malloc( (messageHeader->messageLength - sizeof(SLPdMessageHeader)) + 1 );
            
            if ( scopeToDelete )
            {
                memcpy( scopeToDelete, (char*)messageHeader + sizeof(SLPdMessageHeader), messageHeader->messageLength - sizeof(SLPdMessageHeader) );
                scopeToDelete[messageHeader->messageLength - sizeof(SLPdMessageHeader)] = '\0';
            
                daScopeListTemp = list_remove_element( daScopeList, scopeToDelete );
                if ( daScopeListTemp )
                {
                    SLPSetProperty( "com.apple.slp.daScopeList", daScopeListTemp );

                    SLPBoolean 	needToSetOverflow = SLP_FALSE;
                    
                    while ( strlen( daScopeListTemp ) > kMaxScopeListLenForUDP )	// as long as we have a scope list that is too long
                    {
                        // message is too long
                        needToSetOverflow = SLP_TRUE;
                        
                        char* tempPtr = strrchr( daScopeListTemp, ',' );
                        
                        if ( !tempPtr )
                        {
                            free( daScopeListTemp );
#ifdef ENABLE_SLP_LOGGING
                            SLP_LOG( SLP_LOG_DROP, "we can't fit a single scope in our advertisement!" );			// bad error
#endif
                            break;
                        }
                        
                        if ( tempPtr && *tempPtr == ',' )
                            *tempPtr = '\0';				// chop off here and try again
                    }
                
                    if ( needToSetOverflow == SLP_TRUE )
                    {
                        SLPSetProperty( "com.apple.slp.daPrunedScopeList", daScopeListTemp );
#ifdef ENABLE_SLP_LOGGING
                        SLP_LOG( SLP_LOG_RADMIN, "We have a scope list that is longer than can be advertised via multicasting.  Some SLP implementations may see a truncated list." );
#endif
                    }
                    else
                        SLPSetProperty( "com.apple.slp.daPrunedScopeList", NULL );

                    propertyListIsDirty = true;
                    
#ifdef ENABLE_SLP_LOGGING
                    mslplog( SLP_LOG_RADMIN, "Deleting Scope:", scopeToDelete );
                    mslplog( SLP_LOG_RADMIN, "Setting property: \"com.apple.slp.daScopeList\" to", SLPGetProperty("com.apple.slp.daScopeList") );
#endif                  
                    SLPRegistrar::TheSLPR()->RemoveScope( scopeToDelete );
               }
               
                if ( daScopeListTemp )
                    free( daScopeListTemp );
                
                if ( scopeToDelete )
                    free( scopeToDelete );
            }
            else
                status = kSLPNullBufferErr;				// they can't set the scope list to nothing!
                
            SLPdMessageHeader		returnBuf;
            returnBuf.messageType = messageType;
            returnBuf.messageLength = sizeof(SLPdMessageHeader);
            returnBuf.messageStatus = status;
            
            write( mRequestSD, &returnBuf, returnBuf.messageLength );		// send back our result					

            if ( AreWeADirectoryAgent() )
            {
                ResetStatelessBootTime();
                StartSLPDAAdvertiser( mServerState );					// will update if currently running
            }
        }
        break;
        
#pragma mark case kSLPGetLoggingLevel:
        case kSLPGetLoggingLevel:
        {
            char*		returnBuf = NULL;
            UInt32		returnBufLen = 0;
            UInt32		curLogLevel = 0;
            
            // figure out the current log level
            if ( SLPGetProperty("com.apple.slp.traceRegistrations") && !SDstrcasecmp(SLPGetProperty("com.apple.slp.traceRegistrations"), "true") )
                curLogLevel += kLogOptionRegDereg;
                
            if ( SLPGetProperty("com.apple.slp.traceExpirations") && !SDstrcasecmp(SLPGetProperty("com.apple.slp.traceExpirations"), "true") )
                curLogLevel += kLogOptionExpirations;
                
            if ( SLPGetProperty("com.apple.slp.traceServiceRequests") && !SDstrcasecmp(SLPGetProperty("com.apple.slp.traceServiceRequests"), "true") )
                curLogLevel += kLogOptionServiceRequests;
            
            if ( SLPGetProperty("com.apple.slp.traceDAInfoRequests") && !SDstrcasecmp(SLPGetProperty("com.apple.slp.traceDAInfoRequests"), "true") )
                curLogLevel += kLogOptionDAInfoRequests;
            
            if ( SLPGetProperty("com.apple.slp.traceErrors") && !SDstrcasecmp(SLPGetProperty("com.apple.slp.traceErrors"), "true") )
                curLogLevel += kLogOptionErrors;
            
            if ( SLPGetProperty("com.apple.slp.traceDebug") && !SDstrcasecmp(SLPGetProperty("com.apple.slp.traceDebug"), "true") )
                curLogLevel += kLogOptionDebuggingMessages;
            
            if ( SLPGetProperty("com.apple.slp.logAll") && !SDstrcasecmp(SLPGetProperty("com.apple.slp.logAll"), "true") )
                curLogLevel += kLogOptionAllMessages;
            
            status = MakeSLPSetLoggingOptionsDataBuffer( curLogLevel, &returnBufLen, &returnBuf );
            
            if ( !status && returnBuf && returnBufLen > 0 )
            {
                write( mRequestSD, &returnBuf, returnBufLen );		// send back our result	
                
                free(returnBuf);				
            }
        }
        break;
        
#pragma mark case kSLPSetLoggingLevel:
        case kSLPSetLoggingLevel:
        {
            UInt32		newLogLevel;
            
            status = GetSLPDALoggingLevelFromBuffer( internalBuffer, &newLogLevel );
            
            if ( !status )
            {
                if ( newLogLevel & kLogOptionRegDereg )
                {
                    SLPSetProperty( "com.apple.slp.traceRegistrations", "true" );
                    propertyListIsDirty = true;
                    
#ifdef ENABLE_SLP_LOGGING
                    SLP_LOG( SLP_LOG_RADMIN, "Setting property: \"com.apple.slp.traceRegistrations\" to true" );
#endif
                }
                else
                {
                    SLPSetProperty( "com.apple.slp.traceRegistrations", "false" );
                    propertyListIsDirty = true;
                    
#ifdef ENABLE_SLP_LOGGING
                    SLP_LOG( SLP_LOG_RADMIN, "Setting property: \"com.apple.slp.traceRegistrations\" to false" );
#endif
                }
                    
                if ( newLogLevel & kLogOptionExpirations )
                {
                    SLPSetProperty( "com.apple.slp.traceExpirations", "true" );
                    propertyListIsDirty = true;
                    
#ifdef ENABLE_SLP_LOGGING
                    SLP_LOG( SLP_LOG_RADMIN, "Setting property: \"com.apple.slp.traceExpirations\" to true" );
#endif
                }
                else
                {
                    SLPSetProperty( "com.apple.slp.traceExpirations", "false" );
                    propertyListIsDirty = true;
                    
#ifdef ENABLE_SLP_LOGGING
                    SLP_LOG( SLP_LOG_RADMIN, "Setting property: \"com.apple.slp.traceExpirations\" to false" );
#endif
                }
                    
                if ( newLogLevel & kLogOptionServiceRequests )
                {
                    SLPSetProperty( "com.apple.slp.traceServiceRequests", "true" );
                    propertyListIsDirty = true;
                    
#ifdef ENABLE_SLP_LOGGING
                    SLP_LOG( SLP_LOG_RADMIN, "Setting property: \"com.apple.slp.traceServiceRequests\" to true" );
#endif
                }
                else
                {
                    SLPSetProperty( "com.apple.slp.traceServiceRequests", "false" );
                    propertyListIsDirty = true;
                    
#ifdef ENABLE_SLP_LOGGING
                    SLP_LOG( SLP_LOG_RADMIN, "Setting property: \"com.apple.slp.traceServiceRequests\" to false" );
#endif
                }
                   
                if ( newLogLevel & kLogOptionDAInfoRequests )
                {
                    SLPSetProperty( "com.apple.slp.traceDAInfoRequests", "true" );
                    propertyListIsDirty = true;
                    
#ifdef ENABLE_SLP_LOGGING
                    SLP_LOG( SLP_LOG_RADMIN, "Setting property: \"com.apple.slp.traceDAInfoRequests\" to true" );
#endif
                }
                else
                {
                    SLPSetProperty( "com.apple.slp.traceDAInfoRequests", "false" );
                    propertyListIsDirty = true;
                    
#ifdef ENABLE_SLP_LOGGING
                    SLP_LOG( SLP_LOG_RADMIN, "Setting property: \"com.apple.slp.traceDAInfoRequests\" to false" );
#endif
                }
                   
                if ( newLogLevel & kLogOptionErrors )
                {
                    SLPSetProperty( "com.apple.slp.traceErrors", "true" );
                    propertyListIsDirty = true;
                    
#ifdef ENABLE_SLP_LOGGING
                    SLP_LOG( SLP_LOG_RADMIN, "Setting property: \"com.apple.slp.traceErrors\" to true" );
#endif
                }
                else
                {
                    SLPSetProperty( "com.apple.slp.traceErrors", "false" );
                    propertyListIsDirty = true;
                    
#ifdef ENABLE_SLP_LOGGING
                    SLP_LOG( SLP_LOG_RADMIN, "Setting property: \"com.apple.slp.traceErrors\" to false" );
#endif
                }
                   
                if ( newLogLevel & kLogOptionDebuggingMessages )
                {
                    SLPSetProperty( "com.apple.slp.traceDebug", "true" );
                    propertyListIsDirty = true;
                    
#ifdef ENABLE_SLP_LOGGING
                    SLP_LOG( SLP_LOG_RADMIN, "Setting property: \"com.apple.slp.traceDebug\" to true" );
#endif
                }
                else
                {
                    SLPSetProperty( "com.apple.slp.traceDebug", "false" );
                    propertyListIsDirty = true;
                    
#ifdef ENABLE_SLP_LOGGING
                    SLP_LOG( SLP_LOG_RADMIN, "Setting property: \"com.apple.slp.traceDebug\" to false" );
#endif
                }
                   
                if ( newLogLevel & kLogOptionAllMessages )
                {
                    SLPSetProperty( "com.apple.slp.logAll", "true" );
                    propertyListIsDirty = true;
                    
#ifdef ENABLE_SLP_LOGGING
                    SLP_LOG( SLP_LOG_RADMIN, "Setting property: \"com.apple.slp.logAll\" to true" );
#endif
                }
                else
                {
                    SLPSetProperty( "com.apple.slp.logAll", "false" );
                    propertyListIsDirty = true;
                    
#ifdef ENABLE_SLP_LOGGING
                    SLP_LOG( SLP_LOG_RADMIN, "Setting property: \"com.apple.slp.logAll\" to false" );
#endif
                }
            }
        }
        break;
        
#pragma mark case kSLPTurnOnRAdminNotifications:
        case kSLPTurnOnRAdminNotifications:
        {
            SLPdMessageHeader		returnBuf;
            returnBuf.messageType = messageType;
            returnBuf.messageLength = sizeof(SLPdMessageHeader);
            returnBuf.messageStatus = noErr;

            write( mRequestSD, &returnBuf, returnBuf.messageLength );		// send back our result					
            
#ifdef ENABLE_SLP_LOGGING
            SLP_LOG( SLP_LOG_RADMIN, "Enabling ServerAdmin notifications" );
#endif
            SLPRegistrar::TheSLPR()->EnableRAdminNotification();
        }
        break;
        
#pragma mark case kSLPTurnOffRAdminNotifications:
        case kSLPTurnOffRAdminNotifications:
        {
            SLPdMessageHeader		returnBuf;
            returnBuf.messageType = messageType;
            returnBuf.messageLength = sizeof(SLPdMessageHeader);
            returnBuf.messageStatus = noErr;

            write( mRequestSD, &returnBuf, returnBuf.messageLength );		// send back our result					
            
#ifdef ENABLE_SLP_LOGGING
            SLP_LOG( SLP_LOG_RADMIN, "Disabling ServerAdmin notifications" );
#endif
            SLPRegistrar::TheSLPR()->DisableRAdminNotification();
        }
        break;
        
#pragma mark case kSLPGetRegisteredSvcs:
        case kSLPGetRegisteredSvcs:
        {
            SLPdMessageHeader		returnBuf;
            returnBuf.messageType = messageType;
            returnBuf.messageLength = sizeof(SLPdMessageHeader);
            returnBuf.messageStatus = noErr;

            write( mRequestSD, &returnBuf, returnBuf.messageLength );		// send back our result					
            // ServerAdmin now wants us to send all our registered data to it
            SLPRegistrar::TheSLPR()->SendRAdminAllCurrentlyRegisteredServices();
        }
        break;
        
		default:
		break;
	}
    
    if ( propertyListIsDirty )
        SLPWriteConfigFile( kSAConfigFilePath );
}

#ifdef EXTRA_MSGS

#endif /* EXTRA_MSGS */
