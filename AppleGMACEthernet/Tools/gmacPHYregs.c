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
		kGMACUserCmd_GetLog			= 0x30,		// get entire GMAC ELG buffer
		kGMACUserCmd_GetRegs		= 0x31,		// get all GMAC registers
		kGMACUserCmd_GetOneReg		= 0x32,		// get one particular GMAC register
		kGMACUserCmd_GetTxRing		= 0x33,		// get Tx DMA elements
		kGMACUserCmd_GetRxRing		= 0x34,		// get Rx DMA elements
		kGMACUserCmd_WriteOneReg	= 0x35,		// write one particular GMAC register

		kGMACUserCmd_ReadAllMII	= 0x50,		// read MII registers 0 thru 31
		kGMACUserCmd_ReadMII	= 0x51,		// read one MII register
		kGMACUserCmd_WriteMII	= 0x52		// write one MII register
	};


	enum
	{		// operation codes:
		kUsage,
		kReadAll,
		kReadOne,
		kWrite
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
	void			DumpMII(	 UInt16 phyAddr );
	void			DumpOneMII(  UInt16 phyAddr, UInt16 regnum );
	void			WriteOneMII( UInt16 phyAddr, UInt16 regnum, UInt16 value );



int main( int argc, char** argv )
{
	int			i, rc;
	UInt32 		regnum, regval, phyAddr, op;


	phyAddr = 0xFF;
	op		= kReadAll;

	for ( i = 1; i < argc; ++i )
	{
		if ( strcmp( argv[ i ], "-h" ) == 0 )
		{
			op = kUsage;
			break;
		}
		else if ( strcmp( argv[ i ], "-r" ) == 0 )
		{
			if ( ++i >= argc )
			{
				op = kReadAll;
				break;
			}
			rc = sscanf( argv[ i ], "%ld", &regnum );
			if ( rc != 1 || regnum > 31 )
			{
				printf( "Bad register number?\n" );
				return 0;
			}

			op = kReadOne;
			continue;
		}/* end IF -r */

		else if ( strcmp( argv[ i ], "-w" ) == 0 )
		{
			if ( i + 2 >= argc )	/* need to pick up register number and value.	*/
			{
				printf( "Need register number and value for write.\n" );
				return 0;
			}
			++i;
			rc = sscanf( argv[ i ], "%ld", &regnum );
			if ( rc != 1 || regnum > 31 )
			{
				printf( "Bad register number.\n" );
				return 0;
			}
			++i;
			if ( strncmp( argv[ i ], "0x", 2 ) == 0 )		// skip over any leading 0x
				argv[ i ] += 2;
			rc = sscanf( argv[ i ], "%lx", &regval );
			if ( rc != 1 )
			{
				printf( "Bad value.\n" );
				return 0;
			}
			op = kWrite;
			continue;
		}/* end IF -w */

		else if ( strcmp( argv[ i ], "-p" ) == 0 )
		{
			if ( ++i >= argc )	/* need to pick up phy address.	*/
			{
				printf( "PHY address not specified.\n" );
				return 0;
			}
			rc = sscanf( argv[ i ], "%ld", &phyAddr );
			if ( rc != 1 || phyAddr > 31 )
			{
				printf( "Bad PHY address.\n" );
				return 0;
			}
			continue;
		}/* end IF -p */
		else
		{
			op = kUsage;
			break;
		}
	}/* end FOR arguments */

	switch ( op )
	{
	case kReadAll:
		printf( "\n\tReading all PHY registers:\n" );
		DumpMII( phyAddr );
		break;

	case kReadOne:
		printf( "Reading PHY register %ld:\n", regnum );
		DumpOneMII( phyAddr, regnum );
		break;

	case kWrite:
		printf( "Writing PHY register %ld with 0x%lx.\n", regnum, regval );
		WriteOneMII( phyAddr, regnum, regval );
		break;

	default:
		Usage( argv[0] );
		break;
	}

	return 1;
}/* end main */


void Usage( char *myname )
{
	printf( "Usage:\n" );
	printf( "   %s -h             // Display this help text.\n", myname );
	printf( "   %s                // Read all 32 registers from the default PHY address.\n", myname );
	printf( "   %s -r             //       ditto.\n", myname );
	printf( "   %s -r N           // Read just register N from the default PHY address\n", myname );
	printf( "   %s -w N 0xValue   // Write hex value V to register N at the default PHY address\n", myname );
	printf( "   %s -p A           // Read all 32 registers from PHY at address A\n", myname );
	printf( "   %s -p A ...       // Specify PHY at address A and do -r or -w\n", myname );
	printf( "                     // Put -p first if using specific PHY address.\n" );
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


void DumpMII( UInt16 phyAddr )
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
		inStruct.pLogBuffer		= (UInt8*)(phyAddr << 16);
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
			{	if ( (i & 3) == 0 )
					printf( "\n[%2d]",	i );
				printf( "\t0x%04x",		buffer[i] );
			}
			printf( "\n" );
		}
		else printf( "command/request failed 0x%x\n", kr );

		IOServiceClose( conObj );
	}
	else printf( "open device failed 0x%x\n", kr );
	IOObjectRelease( netif );

    printf( "\nEnd of PHY register dump.\n" );
	return;
}/* end DumpMII */


void DumpOneMII( UInt16 phyAddr, UInt16 regnum )
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
	inStruct.pLogBuffer		= (UInt8*)(phyAddr << 16 | r);	// buffer size is really reg number
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
	//	printf( "Read of PHY register successful.\n" );
		printf( "PHY register[ %d ] 0x%04x\n", regnum, buffer[0] );
	}
	else printf( "command/request failed 0x%x\n", kr );

	IOServiceClose( conObj );
	IOObjectRelease( netif );

	return;
}/* end DumpOneMII */


void WriteOneMII( UInt16 phyAddr, UInt16 regnum, UInt16 regval )
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
		inStruct.pLogBuffer		= (UInt8*)(phyAddr << 16 | r);
		inStruct.logBufferSz	= regval;

			/* Set the PHY's register:	*/

		kr = io_connect_method_structureI_structureO(	conObj,
														0,					/* method index	*/
														(char*)&inStruct,
														sizeof( inStruct ),
														dummy,
														&dummySize );
		if ( kr == kIOReturnSuccess )
				printf( "Write of PHY register successful.\n" );
		else printf( "command/request failed 0x%x\n", kr );
		IOServiceClose( conObj );
	}
	else
		printf( "open device failed 0x%x\n", kr );

	IOObjectRelease( netif );
	return;
}/* end WriteOneMII */
