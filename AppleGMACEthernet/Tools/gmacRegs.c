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



	int		DoIt();
	void	OutputBuffer();



		// find this elsewhere ...
	kern_return_t	io_connect_method_structureI_structureO
	(
		mach_port_t				connection,
		int						selector,
		io_struct_inband_t		input,
		mach_msg_type_number_t	inputCnt,
		io_struct_inband_t		output,
		mach_msg_type_number_t	*outputCnt
	);


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
		UInt8		*pBuffer;		/* address of the buffer			*/
		UInt32		bufferSz;		/* size of the buffer				*/
	} UCRequest;


		/* Globals:	*/

	UCRequest		gUCRequest;
	UInt8			gBuffer[ 4096 ];		// a page should fit the returned data.


		/* Prototypes:	*/

	kern_return_t	doRequest(	io_connect_t			connection,
								unsigned char			reqID,
            					void					*inputData,
								unsigned long			inputDataSize,
								void					*outputData,
								mach_msg_type_number_t	*outputDataSize );
	io_object_t		getInterfaceWithName( mach_port_t masterPort, char *className );




int main( int argc, char **argv )
{
	if ( argc != 1 )
	{
		printf( "\nUsage: %s	# to dump UniNEnet registers.\n", argv[0] );
		return 1;
	}

	return DoIt();
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
		printf( "IORegistryCreateIterator() error %08lx\n", (unsigned long)kr );
		return 0;
	}

	while ( (obj = IOIteratorNext( ite )) )
	{
		if ( IOObjectConformsTo( obj, (char*)className ) )
		{
		//	printf( "Found UniNEnet UserClient !!\n" );
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


void OutputBuffer()
{
	UInt32			*pl = (UInt32*)gBuffer;			// pointer to LONG
	UInt8			*pb;							// pointer to Byte
	UInt32			i, length, offset;
	UInt32			endianSwapped;


	printf( "\n\t Start of GMAC register dump:\n\n" );

	while ( (length = *pl++) )
	{
		offset	= *pl++;
		for ( i = 0; i < length; i += sizeof( UInt32 ) )
		{
			pb = (UInt8*)pl;
			endianSwapped = pb[3]<<24 | pb[2]<<16 | pb[1]<<8 | pb[0];
			printf( "%4x\t%08x\n", (unsigned int)offset, (unsigned int)endianSwapped );
			offset += sizeof( UInt32 );
			pl++;
		}/* end FOR each register of set */
		printf( "\n" );
	}/* end WHILE have a set to dump */

	printf( "\tEnd of GMAC register dump (%d bytes).\n\n", (UInt8*)pl - (UInt8*)gBuffer  );
	return;
}/* end OutputBuffer */


int DoIt()
{
    mach_port_t				masterPort;
    io_object_t				netif;		// network interface
    io_connect_t			conObj;		// connection object
    kern_return_t			kr;
	UCRequest				inStruct;
	UInt32					outSize = sizeof( gBuffer );


	    // Get master device port

    kr = IOMasterPort( bootstrap_port, &masterPort );
    if ( kr != KERN_SUCCESS )
	{
		printf( "IOMasterPort() failed: %08lx\n", (unsigned long)kr );
		exit( 0 );
    }

    netif = getInterfaceWithName( masterPort, "UniNEnet" );
    if ( !netif )
	{
		printf( "getInterfaceWithName failed.\n" );
    	exit( 0 );
	}

	kr = IOServiceOpen( netif, mach_task_self(), 'GMAC', &conObj );
	if ( kr != kIOReturnSuccess )
	{
		printf( "open device failed 0x%x\n", kr );
		IOObjectRelease( netif );
		exit( 0 );
	}

//	printf( "open device succeeded.\n" );

	inStruct.reqID		= kGMACUserCmd_GetRegs;
	inStruct.pBuffer	= 0;	// unused
	inStruct.bufferSz	= 0;	// unused

	kr = io_connect_method_structureI_structureO(
			conObj,									/* connection object			*/
			0,										/* method index for doRequest	*/
			(void*)&inStruct,						/* input struct					*/
			sizeof( inStruct ),						/* input size					*/
			(void*)&gBuffer,						/* output buffer				*/
			(mach_msg_type_number_t*)&outSize );	/* output size					*/

	if ( kr != kIOReturnSuccess )
	{
		printf( "Request failed 0x%x\n", kr );
	}
	else
	{
		OutputBuffer();
	}

	IOServiceClose( conObj );
//	printf( "Closed device.\n" );

	IOObjectRelease( netif );
    exit( 0 );
}/* end DoIt */
