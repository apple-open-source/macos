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
		UInt32		reqID;			/* kGMACUserCmd_GetTxRing			*/
		UInt8		*pBuffer;		/* address of the buffer			*/
		UInt32		bufferSz;		/* size of the buffer				*/
	} UCRequest;




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


	io_object_t		getInterfaceWithName( mach_port_t masterPort, char *className );
	int				DoIt();
	void			OutputBuffer();


		/* Globals:	*/

	UCRequest	gUCRequest;
	UInt8		gBuffer[ 4096 * 2 ];	// a page or 2 should do it.



int main( int argc, char **argv )
{
	if ( argc == 1 )
		return DoIt();

	printf( "usage: %s	# to dump UniNEnet Tx and Tx rings.\n", argv[0] );
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
		printf( "IORegistryCreateIterator() error %08lx\n", (unsigned long)kr );
		return 0;
	}

	while ( (obj = IOIteratorNext( ite )) )
	{
		if ( IOObjectConformsTo( obj, (char*)className ) )
		{
		///	printf( "Found UniNEnet UserClient !!\n" );
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
		return -1;
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

///	printf( "open device succeeded.\n" );

	bzero( gBuffer, sizeof( gBuffer ) );
((UInt32*)gBuffer)[0] = 0x8badf00d;

		/* dump the Tx ring 1st:	*/

	inStruct.reqID		= kGMACUserCmd_GetTxRing;
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
		printf( "\n\n\t Start of Tx ring dump:\n\n" );
		OutputBuffer();
		printf( "\n\t End of Tx ring dump.\n\n" );
	}

		/* Now dump the Rx ring:	*/

	bzero( gBuffer, sizeof( gBuffer ) );

	inStruct.reqID		= kGMACUserCmd_GetRxRing;
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
		printf( "\n\n\t Start of Rx ring dump:\n\n" );
		OutputBuffer();
		printf( "\n\t End of Rx ring dump.\n\n" );
	}

	IOServiceClose( conObj );
//	printf( "Closed device.\n" );

	IOObjectRelease( netif );
    exit( 0 );
}/* end DoIt */


void OutputBuffer()
{
	UInt32		*pe = (UInt32*)gBuffer;		// pointer to element
	UInt8		*pb;						// pointer to BYTE
	UInt32		*pl;						// pointer to LONG
	UInt32		i, index = 0;
	UInt8		swapBuf[ 16 ];				// buffer for endian swapping


	pl = (UInt32*)swapBuf;

	while ( *pe )
	{	pb = (UInt8*)pe;
		for ( i = 0; i < 8; ++i )			// swap 1st 64 bit value
			swapBuf[ 7 - i ] = *pb++;
		for ( i = 0; i < 8; ++i )			// swap 2nd 64 bit value
			swapBuf[ 15 - i ] = *pb++;

		printf( "[%4lx]\t%8lx %8lx   %8lx %8lx\n",	index, pl[0], pl[1], pl[2], pl[3] );
		pe += 4;
		index++;
	}/* end WHILE have elements to dump */

	return;
}/* end OutputBuffer */
