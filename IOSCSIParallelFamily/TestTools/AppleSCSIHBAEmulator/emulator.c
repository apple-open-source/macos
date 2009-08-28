//-----------------------------------------------------------------------------
//	Includes
//-----------------------------------------------------------------------------

#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <sysexits.h>
#include <stdint.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/scsi/SCSITask.h>
#include <IOKit/scsi/SCSICmds_REPORT_LUNS_Definitions.h>
#include <IOKit/scsi/SCSICommandOperationCodes.h>
#include <IOKit/scsi/SCSICmds_INQUIRY_Definitions.h>
#include <CoreFoundation/CoreFoundation.h>

#include "AppleSCSIEmulatorAdapterUC.h"
#include "AppleSCSIEmulatorDefines.h"

//-----------------------------------------------------------------------------
//	Macros
//-----------------------------------------------------------------------------

#define DEBUG	0

#define DEBUG_ASSERT_COMPONENT_NAME_STRING "Emulator"

#if DEBUG
#define PRINT(x)	printf x
#else
#define PRINT(x)
#endif

#if DEBUG
#define DEBUG_ASSERT_MESSAGE(componentNameString,	\
							 assertionString,		\
							 exceptionLabelString,	\
							 errorString,			\
							 fileName,				\
							 lineNumber,			\
							 errorCode)				\
DebugAssert(componentNameString,					\
					   assertionString,				\
					   exceptionLabelString,		\
					   errorString,					\
					   fileName,					\
					   lineNumber,					\
					   errorCode)					\

static void
DebugAssert ( const char *	componentNameString,
			  const char *	assertionString,
			  const char *	exceptionLabelString,
			  const char *	errorString,
			  const char *	fileName,
			  long			lineNumber,
			  int			errorCode )
{
	
	if ( ( assertionString != NULL ) && ( *assertionString != '\0' ) )
		printf ( "Assertion failed: %s: %s\n", componentNameString, assertionString );
	else
		printf ( "Check failed: %s:\n", componentNameString );
	if ( exceptionLabelString != NULL )
		printf ( "	 %s\n", exceptionLabelString );
	if ( errorString != NULL )
		printf ( "	 %s\n", errorString );
	if ( fileName != NULL )
		printf ( "	 file: %s\n", fileName );
	if ( lineNumber != 0 )
		printf ( "	 line: %ld\n", lineNumber );
	if ( errorCode != 0 )
		printf ( "	 error: %d\n", errorCode );
	
}

#endif	/* DEBUG */

#include <AssertMacros.h>


//-----------------------------------------------------------------------------
//	Constants
//-----------------------------------------------------------------------------

#define kAppleSCSIEmulatorAdapterClassString	"AppleSCSIEmulatorAdapter"
#define kIOSCSIParallelInterfaceDeviceString	"IOSCSIParallelInterfaceDevice"
#define kIOSCSITargetDeviceString				"IOSCSITargetDevice"
#define kIOSCSIHierarchicalLogicalUnitString	"IOSCSIHierarchicalLogicalUnit"

//-----------------------------------------------------------------------------
//	Structures
//-----------------------------------------------------------------------------

#pragma pack(1)

typedef struct EmulatorSCSIInquiryPage00Data
{
	UInt8							page00;
	UInt8							page80;
	UInt8							page83;
} EmulatorSCSIInquiryPage00Data;

typedef struct EmulatorSCSIInquiryPage83Data
{
	SCSICmd_INQUIRY_Page83_Identification_Descriptor	descriptor1;
	UInt8												descriptor1bytes[7];
	SCSICmd_INQUIRY_Page83_Identification_Descriptor	descriptor2;
	UInt8												descriptor2bytes[15];
} EmulatorSCSIInquiryPage83Data;

typedef struct EmulatorSCSIInquiryPage80Data
{
	UInt8							serialBytes[31];
} EmulatorSCSIInquiryPage80Data;

typedef struct EmulatorSCSIInquiryPage00
{
	SCSICmd_INQUIRY_Page00_Header	header;
	EmulatorSCSIInquiryPage00Data	data;
} EmulatorSCSIInquiryPage00;

typedef struct EmulatorSCSIInquiryPage80
{
	SCSICmd_INQUIRY_Page80_Header	header;
	EmulatorSCSIInquiryPage80Data	data;
} EmulatorSCSIInquiryPage80;

typedef struct EmulatorSCSIInquiryPage83
{
	SCSICmd_INQUIRY_Page83_Header	header;
	EmulatorSCSIInquiryPage83Data	data;
} EmulatorSCSIInquiryPage83;

#pragma options align=reset


//-----------------------------------------------------------------------------
//	Globals
//-----------------------------------------------------------------------------

SCSICmd_INQUIRY_StandardData gInquiryData =
{
	kINQUIRY_PERIPHERAL_TYPE_DirectAccessSBCDevice,	// PERIPHERAL_DEVICE_TYPE
	0,	// RMB;
	5,	// VERSION
	2,	// RESPONSE_DATA_FORMAT
	sizeof ( SCSICmd_INQUIRY_StandardData ) - 5,	// ADDITIONAL_LENGTH
	0,	// SCCSReserved
	0,	// flags1
	0,	// flags2
	"APPLE",
	"SCSI Emulator",
	"1.0",
};

static EmulatorSCSIInquiryPage00 gInquiryPage00Data =
{
	0,											// PERIPHERAL_DEVICE_TYPE
	kINQUIRY_Page00_PageCode,					// PAGE_CODE
	0,											// RESERVED
	sizeof ( EmulatorSCSIInquiryPage00Data ),	// PAGE_LENGTH
	kINQUIRY_Page00_PageCode,
	kINQUIRY_Page80_PageCode,
	kINQUIRY_Page83_PageCode
};

static EmulatorSCSIInquiryPage80 gInquiryPage80Data =
{
	0,												// PERIPHERAL_DEVICE_TYPE
	kINQUIRY_Page80_PageCode,						// PAGE_CODE
	0,												// RESERVED
	sizeof ( EmulatorSCSIInquiryPage80Data ) + 1,	// PAGE_LENGTH
	'A',
	'P',
	'P',
	'L',
	'E',
	' ',
	'V',
	'i',
	'r',
	't',
	'u',
	'a',
	'l',
	' ',
	'L',
	'U',
	'N',
	'0',
	0
};

static EmulatorSCSIInquiryPage83 gInquiryPage83Data =
{
	0,											// PERIPHERAL_DEVICE_TYPE
	kINQUIRY_Page83_PageCode,					// PAGE_CODE
	0,											// RESERVED
	sizeof ( EmulatorSCSIInquiryPage83Data ),	// PAGE_LENGTH
	{
		{													// Descriptor1
			kINQUIRY_Page83_CodeSetBinaryData,				// CODE_SET
			kINQUIRY_Page83_AssociationTargetDevice | kINQUIRY_Page83_IdentifierTypeFCNameIdentifier, // IDENTIFIER_TYPE
			0x00,											// Reserved
			0x08,											// IDENTIFIER_LENGTH
			0x50											// IDENTIFIER
		},
		{
			0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
		},
		{													// Descriptor2
			kINQUIRY_Page83_CodeSetBinaryData,				// CODE_SET
			kINQUIRY_Page83_IdentifierTypeFCNameIdentifier, // IDENTIFIER_TYPE
			0x00,											// Reserved
			0x10,											// IDENTIFIER_LENGTH
			0x60											// IDENTIFIER
		},
		{
			0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
		}
	},
};


//-----------------------------------------------------------------------------
//		Prototypes
//-----------------------------------------------------------------------------

static void
CreateTargetLUN (
	SCSITargetIdentifier	targetID,
	SCSILogicalUnitNumber	logicalUnit,
	UInt64					capacity,
	boolean_t				unique );

static void
DestroyTargetLUN (
	SCSITargetIdentifier 	targetID,
	SCSILogicalUnitNumber 	logicalUnit );

static void
DestroyTarget (
	SCSITargetIdentifier 	targetID );

static io_object_t
GetController ( void );

static void
PrintUsage ( void );

static void
PrintController ( io_object_t controller );

static void
PrintTarget ( io_object_t target );

static void
PrintLogicalUnit ( io_object_t logicalUnit );

static void
ReportInventory ( void );


//-----------------------------------------------------------------------------
//		main - Our main entry point
//-----------------------------------------------------------------------------

int
main ( int argc, const char * argv[] )
{
	
	boolean_t		create		= false;
	boolean_t		destroy		= false;
	boolean_t		inventory	= false;
	boolean_t		unique		= true;
	int64_t			targetID	= -1;
	int64_t			lun			= -1;
	uint64_t		size		= 0;
	char			c;
	
	static struct option long_options [ ] =
	{
		{ "target",			required_argument,	0, 't' },
		{ "lun",			required_argument,	0, 'l' },
		{ "size",			required_argument,	0, 's' },
		{ "inventory",		no_argument,		0, 'i' },
        { "create",			no_argument,		0, 'c' },
		{ "destroy",		no_argument,		0, 'd' },
		{ "help",			no_argument,		0, 'h' },
		{ "nounique",		no_argument,		0, 'n' },
		{ 0, 0, 0, 0 }
	};
	
	while ( ( c = getopt_long ( argc, ( char * const * ) argv, "t:l:s:icdhn?", long_options, NULL ) ) != -1 )
	{
		
		switch ( c )
		{
			
			case 't':
			{
				
				targetID = strtoull ( optarg, ( char ** ) NULL, 10 );
				if ( ( targetID > 255 ) || ( targetID == kInitiatorID ) )
				{
					PRINT ( ( "Invalid targetID.\n" ) );
					PrintUsage ( );
					exit ( EX_USAGE );
				}
				
			}
			break;
			
			case 'l':
			{
				
				lun = strtoull ( optarg, ( char ** ) NULL, 10 );
				if ( ( lun > 16383 ) || ( lun == 0 ) )
				{
					PRINT ( ( "Invalid LUN.\n" ) );
					PrintUsage ( );
					exit ( EX_USAGE );
				}
				
			}
			break;
			
			case 's':
			{
				
				char *	expr;
				
				size = strtoull ( optarg, &expr, 10 );
				
				switch ( *expr )
				{
					
					case 'k':
						size *= 1 << 10;
						break;
					
					case 'm':
						size *= 1 << 20;
						break;
					
					case 'g':
						size *= 1 << 30;
						break;
					
					default:
						break;
					
				}
				
				if ( size % 512 )
				{
					PRINT ( ( "Invalid byte count. Must be a multiple of 512 bytes\n" ) );
					PrintUsage ( );
					exit ( EX_USAGE );
				}
				
			}
			break;
			
			case 'i':
			{
				
				if ( ( create ) || ( destroy ) )
				{
					PRINT ( ( "Create, Destroy, and Inventory are mutually exclusive.\n" ) );
					PrintUsage ( );
					exit ( EX_USAGE );
				}
				
				inventory = true;
				
			}
			break;
				
			case 'c':
			{
				
				if ( ( inventory ) || ( destroy ) )
				{
					PRINT ( ( "Create, Destroy, and Inventory are mutually exclusive.\n" ) );
					PrintUsage ( );
					exit ( EX_USAGE );
				}
				
				create = true;
				
			}
			break;
			
			case 'd':
			{
				
				if ( ( create ) || ( inventory ) )
				{
					PRINT ( ( "Create, Destroy, and Inventory are mutually exclusive.\n" ) );
					PrintUsage ( );
					exit ( EX_USAGE );
				}
				
				destroy = true;
				
			}
			break;
			
			case 'n':
			{
				unique = false;
			}
			break;
			
			case 'h':
			default:
			{
				
				PrintUsage ( );
				exit ( EX_USAGE );
				
			}
			break;
			
		}
		
	}
	
	if ( inventory )
	{
		
		ReportInventory ( );
		exit ( 0 );
		
	}
	
	if ( create )
	{
		
		if ( ( targetID == -1 ) || ( lun == -1 ) || ( size == 0 ) )
		{
			
			PrintUsage ( );
			exit ( EX_USAGE );
			
		}
		
		CreateTargetLUN ( targetID, lun, size, unique );
		
	}
	
	else if ( destroy )
	{

		if ( targetID == -1 )
		{
			
			PrintUsage ( );
			exit ( EX_USAGE );
			
		}
		
		if ( lun == -1 )
		{
			DestroyTarget ( targetID );
		}
		
		else
		{
			DestroyTargetLUN ( targetID, lun );
		}
		
	}
	
	else
	{
		
		PrintUsage ( );
		exit ( EX_USAGE );
		
	}
	
	return 0;

}


//-----------------------------------------------------------------------------
//		CreateTargetLUN - Creates a Logical Unit attached to a target.
//-----------------------------------------------------------------------------

static void
CreateTargetLUN (
	SCSITargetIdentifier	targetID,
	SCSILogicalUnitNumber	logicalUnit,
	UInt64					capacity,
	boolean_t				unique )
{
	
	io_object_t		controller = IO_OBJECT_NULL;
	
	PRINT ( ( "CreateTargetLUN, targetID = %qd, logicalUnit = %qd, capacity = %qd\n", targetID, logicalUnit, capacity ) );
	
	controller = GetController ( );
	if ( controller != IO_OBJECT_NULL )
	{
		
		io_connect_t	connection 	= IO_OBJECT_NULL;
		IOReturn		status		= kIOReturnSuccess;
		
		status = IOServiceOpen (
			controller,
			mach_task_self ( ),
			kSCSIEmulatorAdapterUserClientConnection,
			&connection );
		
		if ( status == kIOReturnSuccess )
		{
			
			EmulatorTargetParamsStruct		target;
			EmulatorLUNParamsStruct			lun;
			size_t							outCount	= 0;
			EmulatorSCSIInquiryPage80		page80		= gInquiryPage80Data;
			EmulatorSCSIInquiryPage83		page83		= gInquiryPage83Data;
			char							serial[32];
			
			bzero ( &target, sizeof ( EmulatorTargetParamsStruct ) );
			bzero ( &lun, sizeof ( EmulatorLUNParamsStruct ) );
			
			lun.logicalUnit 				= logicalUnit;
			lun.capacity 					= capacity;
			
			lun.inquiryData 				= ( mach_vm_address_t ) ( uintptr_t ) &gInquiryData;
			lun.inquiryPage00Data 			= ( mach_vm_address_t ) ( uintptr_t ) &gInquiryPage00Data;
			lun.inquiryPage80Data 			= ( mach_vm_address_t ) ( uintptr_t ) &page80;
			lun.inquiryPage83Data 			= ( mach_vm_address_t ) ( uintptr_t ) &page83;
			
			lun.inquiryDataLength			= sizeof ( gInquiryData );
			lun.inquiryPage00DataLength 	= sizeof ( gInquiryPage00Data );
			lun.inquiryPage80DataLength 	= sizeof ( page80 );
			lun.inquiryPage83DataLength 	= sizeof ( page83 );
			
			target.targetID 				= targetID;
			target.lun						= lun;
			
			PRINT ( ( "lun.inquiryData = %p\n", &gInquiryData ) );
			PRINT ( ( "lun.inquiryData = %p\n", &gInquiryPage00Data ) );
			PRINT ( ( "lun.inquiryData = %p\n", &page80 ) );
			PRINT ( ( "lun.inquiryData = %p\n", &page83 ) );
			
			PRINT ( ( "gInquiryPage00Data = 0x%02x 0x%02x 0x%02x 0x%02x  0x%02x 0x%02x 0x%02x\n",
					   gInquiryPage00Data.header.PERIPHERAL_DEVICE_TYPE,
					   gInquiryPage00Data.header.PAGE_CODE,
					   gInquiryPage00Data.header.RESERVED,
					   gInquiryPage00Data.header.PAGE_LENGTH,
					   gInquiryPage00Data.data.page00,
					   gInquiryPage00Data.data.page80,
					   gInquiryPage00Data.data.page83 ) );
			
			PRINT ( ( "sizeof ( gInquiryData ) = %ld\n", sizeof ( gInquiryData ) ) );
			PRINT ( ( "sizeof ( gInquiryPage00Data ) = %ld\n", sizeof ( gInquiryPage00Data ) ) );
			PRINT ( ( "sizeof ( page80 ) = %ld\n", sizeof ( page80 ) ) );
			PRINT ( ( "sizeof ( page83 ) = %ld\n", sizeof ( page83 ) ) );
			PRINT ( ( "sizeof ( EmulatorTargetParamsStruct ) = %ld\n", sizeof ( EmulatorTargetParamsStruct ) ) );
			PRINT ( ( "sizeof ( EmulatorLUNParamsStruct ) = %ld\n", sizeof ( EmulatorLUNParamsStruct ) ) );
			
			// Fill in LUN information for page 80.
			snprintf ( serial, 32, "APPLE Virtual LUN %qd", logicalUnit );
			bcopy ( serial, ( char * ) &page80.header.PRODUCT_SERIAL_NUMBER, 32 );
			
			if ( unique )
			{
				
				int		fd = -1;
				char	randomBytes[15];
				int		amount;
				
				// Get some random bytes from /dev/random.
				fd = open ( "/dev/random", O_RDONLY, 0 );
				if ( fd == -1 )
				{
					
					PRINT ( ( "Open /dev/random failed\n" ) );
					exit ( -1 );
					
				}
				
				amount = read ( fd, randomBytes, sizeof ( randomBytes ) );
				if ( amount != sizeof ( randomBytes ) )
				{
					
					PRINT ( ( "Reading from /dev/random failed\n" ) );
					exit ( -1 );
					
				}
				
				// Make sure that the page 83 data is unique for this LUN.
				bcopy ( randomBytes, page83.data.descriptor2bytes, sizeof ( page83.data.descriptor2bytes ) );
				
				close ( fd );
				
			}
			
			IOConnectCallStructMethod (
				connection,
				kUserClientCreateLUN,
				&target,
				sizeof ( EmulatorTargetParamsStruct ),
				NULL,
				&outCount );
			
			IOServiceClose ( connection );
			
		}
		
		IOObjectRelease ( controller );
		
	}
	
}


//-----------------------------------------------------------------------------
//		DestroyTargetLUN - Destroys a Logical Unit attached to a target.
//-----------------------------------------------------------------------------

static void
DestroyTargetLUN (
	SCSITargetIdentifier 	targetID,
	SCSILogicalUnitNumber 	logicalUnit )
{
	
	io_object_t		controller = IO_OBJECT_NULL;
	
	PRINT ( ( "DestroyTargetLUN, targetID = %qd, logicalUnit = %qd\n", targetID, logicalUnit ) );
	
	controller = GetController ( );
	if ( controller != IO_OBJECT_NULL )
	{
		
		io_connect_t	connection 	= IO_OBJECT_NULL;
		IOReturn		status		= kIOReturnSuccess;
		
		status = IOServiceOpen (
			controller,
			mach_task_self ( ),
			kSCSIEmulatorAdapterUserClientConnection,
			&connection );
		
		if ( status == kIOReturnSuccess )
		{
			
			uint32_t	outCount = 0;
			uint64_t	params[2];
			
			params[0] = targetID;
			params[1] = logicalUnit;
			
			IOConnectCallScalarMethod (
				connection,
				kUserClientDestroyLUN,
				( const uint64_t * ) params,
				2,
				NULL,
				&outCount );
			
			IOServiceClose ( connection );
			
		}
		
		IOObjectRelease ( controller );
		
	}
	
}


//-----------------------------------------------------------------------------
//		DestroyTarget - Destroys a target.
//-----------------------------------------------------------------------------

static void
DestroyTarget (
	SCSITargetIdentifier 	targetID )
{
	
	io_object_t		controller = IO_OBJECT_NULL;
	
	PRINT ( ( "DestroyTarget, targetID = %qd\n", targetID ) );
	
	controller = GetController ( );
	if ( controller != IO_OBJECT_NULL )
	{
		
		io_connect_t	connection 	= IO_OBJECT_NULL;
		IOReturn		status		= kIOReturnSuccess;
		
		status = IOServiceOpen (
			controller,
			mach_task_self ( ),
			kSCSIEmulatorAdapterUserClientConnection,
			&connection );
		
		if ( status == kIOReturnSuccess )
		{
			
			uint32_t	outCount = 0;
			uint64_t	params[1];
			
			params[0] = targetID;
			
			IOConnectCallScalarMethod (
				connection,
				kUserClientDestroyTarget,
				( const uint64_t * ) params,
				1,
				NULL,
				&outCount );
			
			IOServiceClose ( connection );
			
		}
		
		IOObjectRelease ( controller );
		
	}
	
}


//-----------------------------------------------------------------------------
//		GetController - Gets the controller object.
//-----------------------------------------------------------------------------

static io_object_t
GetController ( void )
{
	
	io_object_t		controller = IO_OBJECT_NULL;
	
	controller = IOServiceGetMatchingService (
		kIOMasterPortDefault,
		IOServiceMatching ( kAppleSCSIEmulatorAdapterClassString ) );
	
	return controller;
	
}


//-----------------------------------------------------------------------------
//		ReportInventory - Reports the target/lun inventory
//-----------------------------------------------------------------------------

static void
ReportInventory ( void )
{
	
	io_object_t		controller = IO_OBJECT_NULL;
	
	controller = GetController ( );
	if ( controller != IO_OBJECT_NULL )
	{
		
		IOReturn				result		= kIOReturnSuccess;
		io_iterator_t			iterator	= IO_OBJECT_NULL;
		io_registry_entry_t		child		= IO_OBJECT_NULL;
		
		PrintController ( controller );
		
		// Look for devices on this bus
		result = IORegistryEntryCreateIterator ( controller, kIOServicePlane, kNilOptions, &iterator );
		if ( result != kIOReturnSuccess )
		{
			goto ErrorExit;
		}
		
		child = IOIteratorNext ( iterator );
		
		while ( child != IO_OBJECT_NULL ) 
		{
			
			if ( IOObjectConformsTo ( child, kIOSCSIParallelInterfaceDeviceString ) )
			{
				
				io_iterator_t	iterator2 = IO_OBJECT_NULL;
				
				result = IORegistryEntryCreateIterator ( child, kIOServicePlane, kNilOptions, &iterator2 );
				if ( result == kIOReturnSuccess )
				{
					
					io_registry_entry_t		grandchild = IO_OBJECT_NULL;
					
					grandchild = IOIteratorNext ( iterator2 );
					
					while ( grandchild != IO_OBJECT_NULL )
					{
						
						if ( IOObjectConformsTo ( grandchild, kIOSCSITargetDeviceString ) )
						{
							
							io_iterator_t	iterator3 = IO_OBJECT_NULL;
							
							PrintTarget ( grandchild );
							
							result = IORegistryEntryCreateIterator ( grandchild, kIOServicePlane, kNilOptions, &iterator3 );
							if ( result == kIOReturnSuccess )
							{
								
								io_registry_entry_t		greatgrandchild = IO_OBJECT_NULL;
								
								greatgrandchild = IOIteratorNext ( iterator3 );
								
								while ( greatgrandchild != IO_OBJECT_NULL )
								{
									
									if ( IOObjectConformsTo ( greatgrandchild, kIOSCSIHierarchicalLogicalUnitString ) )
									{
										
										PrintLogicalUnit ( greatgrandchild );
										
									}

									IOObjectRelease ( greatgrandchild );
									greatgrandchild = IOIteratorNext ( iterator3 );
								
								}
								
								IOObjectRelease ( iterator3 );
								
							}
							
							printf ( "-------------------------------------------------------------------------------\n" );
							
						}
					
						IOObjectRelease ( grandchild );
						grandchild = IOIteratorNext ( iterator2 );
						
					}
					
					IOObjectRelease ( iterator2 );
					
				}
				
			}
			
			IOObjectRelease ( child );
			child = IOIteratorNext ( iterator );
			
		}
		
		IOObjectRelease ( iterator );
		
	}
	
	else
	{
		
		printf ( "No AppleSCSIEmulatorAdapter class found, please make sure the kext is loaded and try again.\n" );
		
	}

ErrorExit:
	
	
	IOObjectRelease ( controller );
	
}


//-----------------------------------------------------------------------------
//		PrintController - Dump controller information
//-----------------------------------------------------------------------------

static void
PrintController ( io_object_t controller )
{
	
	CFNumberRef			number;
	CFDictionaryRef		dict;
	
	printf ( "Controller\n" );
	
	dict = ( CFDictionaryRef ) IORegistryEntrySearchCFProperty ( controller, kIOServicePlane, CFSTR ( kIOPropertyProtocolCharacteristicsKey ), kCFAllocatorDefault, kIORegistryIterateRecursively );
	if ( dict != NULL )
	{
		
		number = ( CFNumberRef ) CFDictionaryGetValue ( dict, CFSTR ( kIOPropertySCSIDomainIdentifierKey ) );
		if ( number != NULL )
		{
			
			int		domain = 0;
			
			CFNumberGetValue ( number, kCFNumberIntType, &domain );
			printf ( "\tDomain ID: %d\n", domain );
			
		}

		CFRelease ( dict );
		dict = NULL;
		
	}
	
}


//-----------------------------------------------------------------------------
//		PrintTarget - Dump target information
//-----------------------------------------------------------------------------

static void
PrintTarget ( io_object_t target )
{
	
	SCSITargetIdentifier	targetID = 0;
	CFNumberRef				number;
	CFDictionaryRef			dict;
	
	printf ( "-------------------------------------------------------------------------------\n" );
	
	dict = ( CFDictionaryRef ) IORegistryEntrySearchCFProperty ( target, kIOServicePlane, CFSTR ( kIOPropertyProtocolCharacteristicsKey ), kCFAllocatorDefault, kIORegistryIterateRecursively );
	if ( dict != NULL )
	{
		
		number = ( CFNumberRef ) CFDictionaryGetValue ( dict, CFSTR ( kIOPropertySCSITargetIdentifierKey ) );
		if ( number != NULL )
		{
			
			CFNumberGetValue ( number, kCFNumberSInt64Type, &targetID );
			printf ( "\nTargetDevice@%qd\n", targetID );
			
		}

		CFRelease ( dict );
		dict = NULL;
		
	}
	
}


//-----------------------------------------------------------------------------
//		PrintLogicalUnit - Dump logical unit information
//-----------------------------------------------------------------------------

static void
PrintLogicalUnit ( io_object_t logicalUnit )
{
	
	CFNumberRef				number	= NULL;
	CFDictionaryRef			dict	= NULL;
	CFStringRef				string	= NULL;
	
	dict = ( CFDictionaryRef ) IORegistryEntrySearchCFProperty ( logicalUnit, kIOServicePlane, CFSTR ( kIOPropertyProtocolCharacteristicsKey ), kCFAllocatorDefault, kIORegistryIterateRecursively );
	if ( dict != NULL )
	{
		
#if	USE_LUN_BYTES
	
		CFDataRef	data = NULL;

		data = ( CFDataRef ) CFDictionaryGetValue ( dict, CFSTR ( kIOPropertySCSILogicalUnitBytesKey ) );
		if ( data != NULL )
		{
			
			const UInt8 *	ptr = ( UInt8 * ) CFDataGetBytePtr ( data );
			printf ( "\nLogicalUnit: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
					 ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7] );
			
		}
		
#else
	
		number = ( CFNumberRef ) CFDictionaryGetValue ( dict, CFSTR ( kIOPropertySCSILogicalUnitNumberKey ) );
		if ( number != NULL )
		{
			
			UInt64	LUN = 0;
			
			CFNumberGetValue ( number, kCFNumberLongLongType, &LUN );
			
			printf ( "\nLogicalUnit: 0x%qd\n", LUN );
			
		}
		
#endif	/* USE_LUN_BYTES */
		
		CFRelease ( dict );
		dict = NULL;
		
	}

	// Describe the device type
	string = ( CFStringRef ) IORegistryEntrySearchCFProperty ( logicalUnit, kIOServicePlane, CFSTR ( kIOPropertySCSIVendorIdentification ), kCFAllocatorDefault, kIORegistryIterateRecursively );
	if ( string != NULL )
	{
		
		printf ( "\tVendor: %s\n", CFStringGetCStringPtr ( string, kCFStringEncodingMacRoman ) );
		CFRelease ( string );
		string = NULL;
		
	}

	// Describe the device type
	string = ( CFStringRef ) IORegistryEntrySearchCFProperty ( logicalUnit, kIOServicePlane, CFSTR ( kIOPropertySCSIProductIdentification ), kCFAllocatorDefault, kIORegistryIterateRecursively );
	if ( string != NULL )
	{
		
		printf ( "\tProduct: %s\n", CFStringGetCStringPtr ( string, kCFStringEncodingMacRoman ) );
		CFRelease ( string );
		string = NULL;
		
	}

	// Describe the device type
	string = ( CFStringRef ) IORegistryEntrySearchCFProperty ( logicalUnit, kIOServicePlane, CFSTR ( kIOPropertySCSIProductRevisionLevel ), kCFAllocatorDefault, kIORegistryIterateRecursively );
	if ( string != NULL )
	{
		
		printf ( "\tProduct Revision Level: %s\n", CFStringGetCStringPtr ( string, kCFStringEncodingMacRoman ) );
		CFRelease ( string );
		string = NULL;
		
	}


	// Describe the device type
	number = ( CFNumberRef )IORegistryEntrySearchCFProperty ( logicalUnit, kIOServicePlane, CFSTR ( kIOPropertySCSIPeripheralDeviceType ), kCFAllocatorDefault, kIORegistryIterateRecursively );
	if ( number != NULL )
	{
		
		int		pdt = 0;
		
		CFNumberGetValue ( number, kCFNumberIntType, &pdt );
		printf ( "\tPeripheral Device Type: %d\n", pdt );
		CFRelease ( number );
		number = NULL;
		
	}
	
}


//-----------------------------------------------------------------------------
//		PrintUsage - Prints usage string
//-----------------------------------------------------------------------------

static void
PrintUsage ( void )
{
	
	printf ( "Usage: emulator [--create, -c] [--destroy, -d] [--inventory, -i] [--target, -t] [--lun, -l] [--unique, -u] [--size, -s]\n" );
	printf ( "       --create and --destroy are mutually exclusive\n" );
	printf ( "       --target accepts targetIDs in the rang of [0...14][16...255]. ID 15 is reserved for the initiator.\n" );
	printf ( "       --lun accepts LUNs in the range of [1...16383] inclusive.\n" );
	printf ( "       --size can be in bytes, kilobytes, megabytes, or gigabytes, suffix usage similar to dd\n" );
	printf ( "       --unique is used to specify if the logical unit being created has a unique identifier in INQUIRY VPD Page 83h. If --unique is not used, the default (shared) INQUIRY VPD Page 83h identifier will be used\n" );
	fflush ( stdout );
	
}