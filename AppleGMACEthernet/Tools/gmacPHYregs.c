/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*
 * Copyright (c) 2002 Apple Computer
 *
 * User Client CLI tool for the Sun GEM Ethernet Controller 
 *
 */


#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include <arpa/inet.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFSerialize.h>
#include <IOKit/network/IONetworkLib.h>

#include <mach/mach.h>
#include <mach/mach_interface.h>
#include <netinet/in.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/file.h>


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
		kGMACUserCmd_GetLog		= 0x30,		// get entire GMAC ELG buffer
		kGMACUserCmd_GetRegs	= 0x31,		// get GMAC registers
		kGMACUserCmd_GetPHY		= 0x32,		// get PHY  registers
		kGMACUserCmd_GetTxRing	= 0x33,		// get Tx DMA elements
		kGMACUserCmd_GetRxRing	= 0x34,		// get Rx DMA elements

		kGMACUserCmd_ReadAllMII	= 0x50,		// read MII registers 0 thru 31
		kGMACUserCmd_ReadMII	= 0x51,		// read one MII register
		kGMACUserCmd_WriteMII	= 0x52		// write one MII register
	};


	typedef struct					/* User Client Request structure:	*/
	{
		UInt32		reqID;			/* kGMACUserCmd_GetLog				*/
		UInt8		*pLogBuffer;	/* buffer addr - really reg #		*/
		UInt32		logBufferSz;	/*  buffer size - really value		*/
	} UCRequest;


		/* Globals:	*/

	UCRequest			gUCRequest;


		/* Prototypes:	*/

	io_object_t		getInterfaceWithName( mach_port_t masterPort, char *className );

	void			Usage( char *myname );
	void			DumpMII();
	void			DumpOneMII(  UInt16 regnum );
	void			WriteOneMII( UInt16 regnum, UInt16 value );



int main( int argc, char ** argv )
{
	if ( argc == 1 )
	{
		Usage( argv[0] );
		return 0;
	}

	if ( argc == 2 && strcmp( argv[1], "-r" ) == 0 )
	{
		DumpMII();		// read all
		return 0;
	}

	if ( argc == 3 && strcmp( argv[1], "-r" ) == 0 )
	{
		int		rc;
		UInt32	regnum;
		rc = sscanf( argv[2], "%ld", &regnum );
		if ( rc == 1 && regnum < 32 )
		{
			printf( "reading mii register %ld\n", regnum );
			DumpOneMII( regnum );
		}
		else
			printf( "bad register number?\n" );
		return 0;
	}

	if ( argc == 4 && strcmp( argv[1], "-w" ) == 0 )
	{
		int		rc;
		UInt32 	regnum;
		UInt32	regval;
		rc = sscanf( argv[2], "%ld", &regnum );
		if ( rc == 1 && regnum < 32 )
		{
			if ( strncmp( argv[3], "0x", 2) == 0 )		// skip over any leading 0x
				argv[3] += 2;
			rc = sscanf( argv[3], "%lx", &regval );
			if ( rc == 1 )
			{
				printf( "writing mii register %ld with 0x%lx\n", regnum, regval );
				WriteOneMII( regnum, regval );
			}
			else
				printf( "bad value?\n" );
		}
		else
			printf( "bad register number?\n" );
		return 0;
	}

	Usage( argv[0] );
	return 1;
}/* end main */


void Usage( char *myname )
{
	printf( "Usage:\n" );
	printf( "\t%s -r		// read all 32 MII registers\n", myname );
	printf( "\t%s -r N	// read just MII register N\n", myname );
	printf( "\t%s -w N 0xValue	// write hex value V to register N\n", myname );
	printf( "\n" );
	return;
}/* end Usage */


io_object_t getInterfaceWithName( mach_port_t masterPort, char *className )
{
	io_object_t		obj = 0;
	io_iterator_t	ite;
	io_name_t		name;
	kern_return_t	kr;


    kr = IORegistryCreateIterator(	masterPort,
									kIOServicePlane,
									true,					/* recursive */
									&ite );

    if ( kr != kIOReturnSuccess )
	{
        printf( "IORegistryCreateIterator() error %08lx\n", (UInt32)kr );
        return 0;
    }

    while ( (obj = IOIteratorNext( ite )) )
	{
        if (IOObjectConformsTo( obj, (char*)className) )
		{
		// printf( "Found our userClient '%s' !!\n", className );
			break;
		}
		else
		{
			kr = IOObjectGetClass( obj, name );
			if ( kr == kIOReturnSuccess )
			{
			//	printf( "Skipping class %s\n", name);
			}
		}
        IOObjectRelease( obj );
        obj = 0;
	}

    IORegistryDisposeEnumerator( ite );

    return obj;
}/* end getInterfaceWithName */


void DumpMII()
{
	mach_port_t				masterPort;
	io_object_t				netif;
	io_connect_t			conObj;
	kern_return_t			kr;
	int						i;
	UCRequest				inStruct;
	UInt16					buffer[ 33 ];
	mach_msg_type_number_t	bufferSize = sizeof( buffer );


		// Get master device port

    kr = IOMasterPort( bootstrap_port, &masterPort );
    if ( kr != KERN_SUCCESS )
	{
		printf( "IOMasterPort() failed: %08lx\n", (UInt32)kr );
		return;
    }

    netif = getInterfaceWithName( masterPort, "UniNEnet" );
    if ( !netif )
		return;

//	printf( "netif=0x%x\n", netif );
	kr = IOServiceOpen( netif, mach_task_self(), 'GMAC', &conObj );
	if ( kr == kIOReturnSuccess )
	{
		inStruct.reqID			= kGMACUserCmd_ReadAllMII;
		inStruct.pLogBuffer		= 0;
		inStruct.logBufferSz	= 0;

			/* Finally get the data:	*/

		kr = io_connect_method_structureI_structureO(	conObj,
														0,					/* method index	*/
														(char*)&inStruct,
														sizeof( inStruct ),
														(UInt8*)&buffer,
														&bufferSize );

		if ( kr == kIOReturnSuccess )
		{
		//	printf( "read all MII worked\n" );
			for ( i = 0 ; i < 32; i++ )
				printf( "[%2d] 0x%04x\n",	i, buffer[i] );
		}
		else printf( "command/request failed 0x%x\n", kr );

		IOServiceClose( conObj );
	}
	else printf( "open device failed 0x%x\n", kr );
	IOObjectRelease( netif );

    printf( "\nEnd of PHY register dump.\n" );
	return;
}/* end DumpMII */


void DumpOneMII( UInt16 regnum )
{
	mach_port_t				masterPort;
	io_object_t				netif;
	io_connect_t			conObj;
	kern_return_t			kr;
	UCRequest				inStruct;
	UInt32					r = regnum;
	UInt16					buffer[ 3 ];		// 1 needed, just testing outsize
	mach_msg_type_number_t	bufferSize = sizeof( buffer );


	    // Get master device port

    kr = IOMasterPort( bootstrap_port, &masterPort );
    if ( kr != KERN_SUCCESS )
	{
		printf( "IOMasterPort() failed: %08lx\n", (unsigned long)kr );
		return;
    }

    netif = getInterfaceWithName( masterPort, "UniNEnet" );
    if ( !netif )
		return;

//	printf( "netif=0x%x\n", netif );
	kr = IOServiceOpen( netif, mach_task_self(), 'GMAC', &conObj );
	if ( kr != kIOReturnSuccess )
	{
		printf( "open device failed 0x%x\n", kr );
		return;
	}

	inStruct.reqID			= kGMACUserCmd_ReadMII;
	inStruct.pLogBuffer		= (UInt8*)r;	// buffer size is really reg number
	inStruct.logBufferSz	= 0;			// unused.

		/* Get the datum:	*/

	kr = io_connect_method_structureI_structureO(	conObj,
													0,					/* method index	*/
													(char*)&inStruct,
													sizeof( inStruct ),
													(UInt8*)buffer,
													&bufferSize );
	if ( kr == kIOReturnSuccess )
	{
		printf( "Read of MII worked.\n" );
		printf( "PHY register[ %d ] 0x%04x\n", regnum, buffer[0] );
	}
	else printf( "command/request failed 0x%x\n", kr );

	IOServiceClose( conObj );
	IOObjectRelease( netif );

	return;
}/* end DumpOneMII */


void WriteOneMII( UInt16 regnum, UInt16 regval )
{
	mach_port_t				masterPort;
	io_object_t				netif;
	io_connect_t			conObj;
	kern_return_t			kr;
	UCRequest				inStruct;
	UInt32					r = regnum;
	UInt8					dummy[ 9 ];		// 1 needed, just testing outsize
	mach_msg_type_number_t	dummySize;


		// Get master device port

    kr = IOMasterPort( bootstrap_port, &masterPort );
    if ( kr != KERN_SUCCESS )
	{
		printf( "IOMasterPort() failed: %08lx\n", (UInt32)kr );
		return;
    }

    netif = getInterfaceWithName( masterPort, "UniNEnet" );
    if ( !netif )
		return;
//	printf( "netif=0x%x\n", netif );

	kr = IOServiceOpen( netif, mach_task_self(), 'GMAC', &conObj );
	if ( kr == kIOReturnSuccess )
	{
		inStruct.reqID			= kGMACUserCmd_WriteMII;
		inStruct.pLogBuffer		= (UInt8*)r;
		inStruct.logBufferSz	= regval;

			/* Set the PHY's register:	*/

		kr = io_connect_method_structureI_structureO(	conObj,
														0,					/* method index	*/
														(char*)&inStruct,
														sizeof( inStruct ),
														dummy,
														&dummySize );
		if ( kr == kIOReturnSuccess )
				printf( "write of MII worked.\n" );
		else printf( "command/request failed 0x%x\n", kr );
		IOServiceClose( conObj );
	}
	else
		printf( "open device failed 0x%x\n", kr );

	IOObjectRelease( netif );
	return;
}/* end WriteOneMII */
