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
/*
 * Copyright (c) 2002 Apple Computer
 *
 * User Client CLI tool for the Sun GEM Ethernet Controller 
 *
 */

#ifdef UNDEFINED
		To do:
		options:
			continuous			until ^c
			raw data to disk
			Cocoa app
			Log to file
			Reset or clear buffer
#endif // UNDEFINED

//#include <assert.h>
#include <ctype.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFSerialize.h>
#include <IOKit/network/IONetworkLib.h>
#include <IOKit/IOTypes.h>

#include <mach/mach.h>
#include <mach/mach_interface.h>

#include <sys/time.h>
#include <sys/file.h>



	int		DumpLog();
	void	OutputBuffer();
//	FILE*	CreateLogFile();	// make the output file

		// find this elsewhere ...
	kern_return_t	io_connect_method_structureI_structureO(
												mach_port_t				connection,
												int						selector,
												io_struct_inband_t		input,
												mach_msg_type_number_t	inputCnt,
												io_struct_inband_t		output,
												mach_msg_type_number_t	*outputCnt );


	enum	/* request codes to send to the user client:	*/
	{
		kSelectLoopbackMAC			= 0x20,
		kSelectLoopbackPHY			= 0x21,

		kRltkUserCmd_GetLog			= 0x30,		// get entire Rltk ELG buffer
		kRltkUserCmd_GetRegs		= 0x31,		// get all Rltk registers
		kRltkUserCmd_GetOneReg		= 0x32,		// get one particular Rltk register
		kRltkUserCmd_GetTxRing		= 0x33,		// get Tx DMA elements
		kRltkUserCmd_GetRxRing		= 0x34,		// get Rx DMA elements
		kRltkUserCmd_WriteOneReg	= 0x35,		// write one particular Rltk register

		kRltkUserCmd_ReadAllMII	= 0x50,		// read MII registers 0 thru 31
		kRltkUserCmd_ReadMII	= 0x51,		// read one MII register
		kRltkUserCmd_WriteMII	= 0x52		// write one MII register
	};


	typedef struct					/* User Client Request structure:	*/
	{
		UInt32		reqID;			/* kRltkUserCmd_GetLog				*/
		UInt8		*pLogBuffer;	/* address of the 64KB buffer		*/
		UInt32		logBufferSz;	/* size of the 64KB buffer			*/
	} UCRequest;


		/* Globals:	*/

	UCRequest			gUCRequest;


		/* Prototypes:	*/

	io_object_t		getInterfaceWithName( mach_port_t masterPort, char *className );




int main( int argc, char **argv )
{
	if      ( argc == 1 )	return DumpLog();
///	else if ( argc == 3 ) 	return DumpRemoteLog( argc, argv );

	printf( "usage: %s	# to dump RTL8139 log on local machine\n", argv[0] );
	return 1;
}/* end main */


	/* Search the registry for an IONetworkInterface object with	*/
	/* the given name. If a match is found, the object is returned.	*/

io_object_t getInterfaceWithName( mach_port_t masterPort, char *className )
{
	io_iterator_t	ite;
	io_object_t		obj = 0;
	io_name_t		name;
	kern_return_t	rc;
	kern_return_t	kr;


    kr = IORegistryCreateIterator(	masterPort,
									kIOServicePlane,
									true,					/* recursive */
									&ite );
	if ( kr != kIOReturnSuccess )
	{
		printf( "IORegistryCreateIterator() error %08lx\n", (unsigned long)kr);
		return 0;
	}

	while ( (obj = IOIteratorNext( ite )) )
	{
		if ( IOObjectConformsTo( obj, (char*)className ) )
		{
		//	printf( "Found RTL8139 UserClient !!\n" );
			break;
		}
		else
		{
			rc = IOObjectGetClass( obj, name );
			if ( rc == kIOReturnSuccess )
			{
			//	printf( "Skipping class %s\n", name );
			}
		}
		IOObjectRelease( obj );
		obj = 0;
	}

    IORegistryDisposeEnumerator( ite );

    return obj;
}/* end getInterfaceWithName */


FILE* CreateLogFile()
{
    time_t		now;
    struct tm	*lt;
    char		filename[ 100 ];


    time( &now );
    lt = localtime( &now );
    sprintf( filename, "log.%d.%d.%02d%02d.%02d",
            lt->tm_mon+1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec );
    printf( "Writing log to %s\n", filename );

    return fopen( filename, "w" );
}/* end CreateLogFile */


void OutputBuffer()
{
    char			buffer[ 256 ];
	UInt32			*pl;			// pointer to LONG
	UInt8			*pb;			// pointer to byte
	UInt8			lefty;			// leftmost byte of event
	UInt32			microsec;		// approx microsecond
	UInt32			i, x, p1, p2;


	buffer[ 4 ] = 0;				// for ending 4 ASCII char string.
	pl = (UInt32*)gUCRequest.pLogBuffer;

	x = pl[ 3 ] - pl[ 1 ] - 0x30;	// calc relative offset of next entry to log
	printf( "\n\t%8lx  %8lx  %8lx  %8lx\t[%lx:]\n",	pl[0], pl[1], pl[2], pl[3], x );
	pl += 4;

	printf( "\t%8lx  %8lx  %8lx  %8lx\n",	pl[0], pl[1], pl[2], pl[3] );
	pl += 4;

	printf( "\t%8lx  %8lx  %8lx  %8lx\n",	pl[0], pl[1], pl[2], pl[3] );
	pl += 4;

	for ( i = 3; i < gUCRequest.logBufferSz / 0x10; ++i )
	{
		if ( *pl == 0xDEBEEFED || *pl == 0 )
			break;

		lefty				= (UInt8)(*pl >> 24);
		microsec			= *pl++ & 0x00FFFFFF;
		p1					= *pl++;
		p2					= *pl++;
	//	*(UInt32*)buffer	= *pl++;
		pb					= (UInt8*)pl;
		pl++;
		buffer[0] = pb[3];
		buffer[1] = pb[2];
		buffer[2] = pb[1];
		buffer[3] = pb[0];
		printf( "%8lx:  %3d  %6ld  %8lx  %8lx\t%s\n",	i * 0x10, lefty, microsec, p1, p2, buffer );
	}
	*(UInt32*)gUCRequest.pLogBuffer = 0xFeedBeef;	// get the juices flowing again.
	return;
}/* end OutputBuffer */


int DumpLog()
{
    mach_port_t				masterPort;
    io_object_t				netif;		// network interface
    io_connect_t			conObj;		// connection object
    kern_return_t			kr;
///	FILE					*out;
	UCRequest				inStruct;
	UInt32					outSize = sizeof( UCRequest );


	    // Get master device port

    kr = IOMasterPort( bootstrap_port, &masterPort );
    if ( kr != KERN_SUCCESS )
	{
		printf( "IOMasterPort() failed: %08lx\n", (unsigned long)kr );
		return -1;
    }

//	netif = getInterfaceWithName( masterPort, "RTL8139" );
	netif = getInterfaceWithName( masterPort, "com_apple_driver_RTL8139" );
    if ( !netif )
	{
		printf( "getInterfaceWithName failed.\n" );
    	exit( 0 );
	}

	kr = IOServiceOpen( netif, mach_task_self(), 'Rltk', &conObj );
	if ( kr != kIOReturnSuccess )
	{
		printf( "Open device failed 0x%x\n", kr );
		IOObjectRelease( netif );
		exit( 0 );
	}

//	printf( "open device succeeded.\n" );

	inStruct.reqID = kRltkUserCmd_GetLog;

		/* Now we can get the elg buffer mapped into my space:	*/

	kr = io_connect_method_structureI_structureO(
			conObj,									/* connection object			*/
			0,										/* method index for doRequest	*/
			(void*)&inStruct,						/* input struct					*/
			sizeof( inStruct ),						/* input size					*/
			(void*)&gUCRequest,						/* output struct				*/
			(mach_msg_type_number_t*)&outSize );	/* output size					*/

	if ( kr != kIOReturnSuccess )
	{
		printf( "Request failed 0x%x\n", kr );
	}
	else
	{
	//	printf( "Request allegedly worked - buffer/size: %8lx/%lx\n",
	//				(UInt32)gUCRequest.pLogBuffer, gUCRequest.logBufferSz );

	///	out = CreateLogFile();
	///	if ( out )
		{
			OutputBuffer();
		///	fclose( out );
		}
	}

	IOServiceClose( conObj );
//	printf( "Closed device.\n" );

	IOObjectRelease( netif );
    exit( 0 );
}/* end DumpLog */
