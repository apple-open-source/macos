/*
 *  main.cpp
 *  DSNSLPlugins
 *
 *  Created by Kevin Arnold on Fri Feb 22 2002.
 *  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
 *
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "mslpd_store.h"
#include "mslp_dat.h"
#include "mslplib.h"
#include "mslpd.h"

#include "SLPDALocator.h"

void PrintHelpInfo( void );
int IsIPAddress(const char* adrsStr, long *ipAdrs);		// defined in slplib
void our_daadvert_callback(	SLPHandle hSLP, 
                                int iErrCode,
                                struct sockaddr_in sin,
                                const char *pcScopeList,
                                const char *pcAttrList,
                                long lBootTime,
                                void *pvUser );
                                
static SLPInternalError get_da_reply(	
                                        struct sockaddr_in	sin,	/* the address of the DA to use */
                                        char*		pcSend, 
                                        int 		iSize,
                                        SLPHandle	puas,
                                        void*		pvCallback, 
                                        void*		pvUser, 
                                        CBType 		cbt );

SLPBoolean SLPServiceLookupNotifier( SLPHandle hSLP, const char* pcSrvURL, short unsigned int sLifeTime, SLPInternalError errCode, void* pvCookie );

static long		gAddressToLookup =	0;
static char*	gServiceType = NULL;

#define kMaxArgs		6		// [-v] [-l address] | [-a] [-s serviceType]
int main(int argc, char *argv[])
{
    char		address[16] = {0};
    long		ipaddress = 0;
    OSStatus		status = 0;
    
    if ( argc > kMaxArgs || argc <= 1 )
    {
        PrintHelpInfo();
        return -1;
    }
    
    for ( int i=1; i<argc; i++ )		// skip past [0]
    {
        if ( strcmp(argv[i], "-v" ) == 0 )
        {
            SLPSetProperty( "com.apple.slp.logAll", "true" );
        }
        else if ( strcmp(argv[i], "-l") == 0 ) 
        {
            i++;		// increment this as the next attribute is the address
            
            if (argv[i])
            {
                strncpy( address, argv[i], sizeof(address) );
            }
            else
            {
                PrintHelpInfo();
                return -1;
            }
                    
            if ( !IsIPAddress(address, &ipaddress) )
            {
                fprintf( stderr, "You have entered a malformed IP address, it must be of the form X.X.X.X\n" );
                PrintHelpInfo();
                return -1;
            }
                
            gAddressToLookup = ipaddress;
        }
        else if ( strcmp(argv[i], "-a") == 0 )
        {
            gAddressToLookup = 0;
        }
        else if ( strcmp(argv[i], "-s") == 0 )
        {
            i++;		// increment this as the next attribute is the serviceType

            if ( argv[i] )
            {
                gServiceType = argv[i];
            }
            else
            {
                PrintHelpInfo();
                return -1;
            }
        }
        else
        {
            PrintHelpInfo();
            return -1;
        }
    }
    
    if ( gAddressToLookup )
        LocateAndAddDA( gAddressToLookup );
        
    StartSLPDALocator( (void*)our_daadvert_callback, NULL );     
    
    while ( !SLPDALocator::TheSLPDAL()->FinishedFirstLookup() )
        sleep(1);
    
    return status; 
}

void PrintHelpInfo( void )
{
    fprintf( stderr,
            "Usage: slpda_netdetective [-l <da_address>] | [-a] [-s <serviceType>]\n"
            "  -l <da_address> is the address of a directory agent you wish to lookup\n"
            "  -a lookup all directory agents on the network\n"
            "  -s <serviceType is an optional parameter to query the DA(s) about registered services\n" );
}

void our_daadvert_callback(	SLPHandle hSLP, 
                                int iErrCode,
                                struct sockaddr_in sin,
                                const char *pcScopeList,
                                const char *pcAttrList,
                                long lBootTime,
                                void *pvUser )
{
    bool	ignoreDA = false;
    
    if ( gAddressToLookup && gAddressToLookup != (long)(sin.sin_addr.s_addr) )
        ignoreDA = true;
    
    if ( !ignoreDA )
    {
        fprintf( stderr, "Found DA [%s]\n\t%s\n\t%s\n", inet_ntoa(sin.sin_addr), pcScopeList, (pcAttrList) ? pcAttrList : "" );

        char*					endPtr = NULL;
		int         			iMTU = strtol(SLPGetProperty("net.slp.MTU"),&endPtr,10);
        char					*pcSendBuf = safe_malloc(iMTU,0,0);
        char					c;
        char*					pcScope = NULL;
        int						offset=0;;   /* This records the reply length. */
//        int						iLast  = 0;   /* used to for ending async callbacks */    
        
        SLPInternalError		err;

//        int 					mSocket = socket(AF_INET, SOCK_DGRAM, 0);

        while( (pcScope = get_next_string(",",pcScopeList,&offset,&c)) )
        {
            if (gServiceType && !(err = generate_srvrqst(pcSendBuf,&iMTU,"en",pcScope, gServiceType,""))) 
            {
                fprintf( stderr, "\tLooking up %s services in %s on %s\n", gServiceType, pcScope, inet_ntoa(sin.sin_addr) );
                err = get_da_reply( sin, pcSendBuf, iMTU, hSLP, (void *)SLPServiceLookupNotifier, NULL, SLPSRVURL_CALLBACK );
            }            
/*
            if (gServiceType && !(err = generate_srvrqst(pcSendBuf,&iMTU,"en",pcScope, gServiceType,""))) 
            {
                if ((err = get_unicast_result(
                                                MAX_UNICAST_WAIT,
                                                mSocket, 
                                                pcSendBuf, 
                                                iSize, 
                                                pcRecvBuf,
                                                RECVMTU, 
                                                &len, 
                                                sin)) != SLP_OK) 
                {
                    fprintf( stderr, "get_reply could not get_da_results from [%s]...: %s",inet_ntoa(sin.sin_addr), slperror(err) );
                    
                    SLPFree(pcRecvBuf);
                    pcRecvBuf = NULL;
                }
                else
                {
                    if (GETFLAGS(pcRecvBuf) & OVERFLOWFLAG) 
                    {
                    
                        SLPFree(pcRecvBuf);
                        pcRecvBuf = NULL;   
                        
                        // set the port to use the SLP port
                        sin.sin_port   = htons(SLP_PORT);
                        if ((err=get_tcp_result(pcSendBuf,iSize, sin, &pcRecvBuf,&len)) != SLP_OK) 
                        {
                            SLPFree(pcRecvBuf);
                            pcRecvBuf = NULL;
        //                    last_one(err, ALL_DONE,pvUser,(SLPHandle)puas,pvCallback,cbt);	  
                            fprintf( stderr, "get_reply overflow, tcp failed from [%s] when locating and adding the DA...: %s",inet_ntoa(sin.sin_addr), slperror(err));
                        }
                    }
                }

                if ( !err )
                    err = process_reply(pcSendBuf, pcRecvBuf, len, &iLast, NULL, SLPDALocator::TheSLPDAL()->GetServerState(), (void *)SLPServiceLookupNotifier, SLPSRVURL_CALLBACK);
            }            
*/            
            SLPFree(pcScope);
        }
    }
}

SLPBoolean SLPServiceLookupNotifier( SLPHandle hSLP, const char* pcSrvURL, short unsigned int sLifeTime, SLPInternalError errCode, void* pvCookie )
{
    SLPBoolean						wantMoreData = SLP_FALSE;
    
    if ( pcSrvURL && errCode == SLP_OK )
    {
        char*	ourURLCopy = (char*)malloc( strlen(pcSrvURL) +1 );
        strcpy( ourURLCopy, pcSrvURL );

        long		numCharsToAdvance = 7;
        
        char*	namePtr = strstr( ourURLCopy, "/?NAME=" );
        
        if ( !namePtr )
        {
            namePtr = strstr( ourURLCopy, "?NAME=" );
            numCharsToAdvance--;
        }
        
        if ( namePtr )
        {
            // ok, this is a hack registration to include the display name
            *namePtr = '\0';						// don't return this as the url
            namePtr += numCharsToAdvance;			// advance to the name
            
            fprintf( stderr, "\t%s\t%s\t\n", namePtr, ourURLCopy );
        }
        else if ( namePtr = strstr( ourURLCopy, ";" ) )
        {
            namePtr[0] = '\0';
            namePtr++;
            
            fprintf( stderr, "\t%s\t%s\t%s\n", "", ourURLCopy, namePtr );
        }
        else
        {
            fprintf( stderr, "\t%s\t%s\t\n", "", ourURLCopy );
        }
        
        wantMoreData = SLP_TRUE;					// still going!
                
        free( ourURLCopy );
    }

    return wantMoreData;
}


static SLPInternalError get_da_reply(	struct sockaddr_in	sin,	/* the address of the DA to use */
                                        char*		pcSend, 
                                        int 		iSize,
                                        SLPHandle	puas,
                                        void*		pvCallback, 
                                        void*		pvUser, 
                                        CBType 		cbt ) 
{
    char*				pcRecvBuf = NULL;
    int					iLast  = 0;   /* used to for ending async callbacks */
    SLPInternalError				err    = SLP_OK;
	char*				endPtr = NULL;
    int					iMTU   = strtol(SLPGetProperty("net.slp.MTU"),&endPtr,10);
    int					len    = 0;   /* This records the reply length. */
    
    int 				mSocket = socket(AF_INET, SOCK_DGRAM, 0);
    
    while ( 1 )
    {
        /* Handle larger than MTU sized requests */
        if (iSize >= iMTU) 
        {
            pcRecvBuf = NULL; /* make sure we're not pointing at puas->pcRecvBuf */
            if ((err=get_tcp_result(pcSend,iSize, sin,&pcRecvBuf,&len))!= SLP_OK)
            {
                SLPFree(pcRecvBuf);
                pcRecvBuf = NULL;
                fprintf( stderr,"\tget_reply of overflowed result failed\n");
                return err;
            }
        
            /* evokes the callback once */
            err = process_reply(pcSend, pcRecvBuf, len, &iLast, pvUser, puas, pvCallback,cbt);
//            (void) process_reply(NULL, NULL, 0, &iLast, pvUser, puas, pvCallback,cbt);
            
            SLPFree(pcRecvBuf);
            pcRecvBuf = NULL;
            return err;
        }
        
        /*
        * Note: TCP will be used in the section below to handle replies
        * which overflow.  In this case, the request must be small enough
        * to fit into the net.slp.MTU limit or else the special case code
        * above would have been used.
        */
        int tcp_used = 0; /* set if overflow occurs and is handled by tcp */

        fprintf( stderr, "\tget_reply connecting to DA [%s]\n",inet_ntoa(sin.sin_addr));
    
        pcRecvBuf = (char*)malloc(RECVMTU);
        
        if ((err = get_unicast_result(
                                        MAX_UNICAST_WAIT,
                                        mSocket, 
                                        pcSend, 
                                        iSize, 
                                        pcRecvBuf,
                                        RECVMTU, 
                                        &len, 
                                        sin)) != SLP_OK) 
        {
//            iLast = 1;
//            last_one(err, ALL_DONE,pvUser,(SLPHandle)puas,pvCallback,cbt);      
            
            fprintf( stderr, "\tget_reply could not get_da_results from [%s]...: %s\n",inet_ntoa(sin.sin_addr), slperror(err) );
            
            dat_strike_da( NULL, sin );		// this DA was bad, give them a strike and when we return an error, the caller can try again
            
            SLPFree(pcRecvBuf);
            pcRecvBuf = NULL;
            continue; 						// try again
        }
        else
        {
            if (GETFLAGS(pcRecvBuf) & OVERFLOWFLAG) 
            { /* the result overflowed ! */
                SLPFree(pcRecvBuf);
                pcRecvBuf = NULL;   
                
                // set the port to use the SLP port
                sin.sin_port   = htons(SLP_PORT);
                err=get_tcp_result(pcSend,iSize, sin, &pcRecvBuf,&len);
                
                if (err != SLP_OK) 
                    err=get_tcp_result(pcSend,iSize, sin, &pcRecvBuf,&len);		// try once more

                if (err != SLP_OK) 
                {
                    SLPFree(pcRecvBuf);
                    pcRecvBuf = NULL;
//                    last_one(err, ALL_DONE,pvUser,(SLPHandle)puas,pvCallback,cbt);	  
                    fprintf( stderr, "\tget_reply overflow, tcp failed from [%s] when getting a reply...: %s\n",inet_ntoa(sin.sin_addr), slperror(err));
            
                    dat_strike_da( NULL, sin );		// this DA was bad, give them a strike and when we return an error, the caller can try again
            
                    continue; 						// try again
                }
                else
                    fprintf( stderr, "\tget_tcp_result, received %d bytes from [%s]\n", len, inet_ntoa(sin.sin_addr) );
                
                tcp_used = 1;	
            }
            else
                fprintf( stderr, "\tget_unicast_result, received %d bytes from [%s]\n", len, inet_ntoa(sin.sin_addr) );
        }
        /* evokes the callback once */
        if ( !err )
            err = process_reply(pcSend, pcRecvBuf, len, &iLast, pvUser, (SLPHandle) puas, pvCallback, cbt);
    
        if (tcp_used) 
        {
            SLPFree(pcRecvBuf);
            pcRecvBuf = NULL;
        }
    
        /* take care of last call as per api */
        if ( !err )
            (void) process_reply(NULL, NULL, 0, &iLast, pvUser, puas, pvCallback,cbt);

        return err;
    }
}

