#include <IOKit/scsi/SCSICmds_INQUIRY_Definitions.h>
#include <IOKit/scsi/SCSICmds_MODE_Definitions.h>
#include <IOKit/scsi/SCSICmds_REQUEST_SENSE_Defs.h>

static void
PrintSCSICmds_INQUIRY_Sizes ( void );

static void
PrintSCSICmds_MODE_Sizes ( void );

static void
PrintSCSICmds_REQUEST_SENSE_Sizes ( void );


int
main ( int argc, const char * argv[] )
{
	
	printf ( "SAM Structure Size Tester\n\n" );
	
	PrintSCSICmds_INQUIRY_Sizes ( );
	PrintSCSICmds_MODE_Sizes ( );
	PrintSCSICmds_REQUEST_SENSE_Sizes ( );
	
	return 0;
	
}


static void
PrintSCSICmds_INQUIRY_Sizes ( void )
{

	printf ( "INQUIRY sizes\n" );
	
	// Standard INQUIRY data (page 0x00)
	printf ( "SCSICmd_INQUIRY_StandardData = %ld\n", ( UInt32 ) sizeof ( SCSICmd_INQUIRY_StandardData ) );
	printf ( "SCSICmd_INQUIRY_StandardDataAll = %ld\n", ( UInt32 ) sizeof ( SCSICmd_INQUIRY_StandardDataAll ) );
	
	// INQUIRY Vital Products Pages
	printf ( "SCSICmd_INQUIRY_Page00_Header = %ld\n", ( UInt32 ) sizeof ( SCSICmd_INQUIRY_Page00_Header ) );
	printf ( "SCSICmd_INQUIRY_Page83_Header = %ld\n", ( UInt32 ) sizeof ( SCSICmd_INQUIRY_Page83_Header ) );
	printf ( "SCSICmd_INQUIRY_Page83_Identification_Descriptor = %ld\n", ( UInt32 ) sizeof ( SCSICmd_INQUIRY_Page83_Identification_Descriptor ) );
	
	printf ( "\n" );
	
}

static void
PrintSCSICmds_MODE_Sizes ( void )
{
	
	printf ( "MODE_SENSE and MODE_SELECT sizes\n" );
	
	// Mode parameter headers
	printf ( "SPCModeParameterHeader6 = %ld\n", ( UInt32 ) sizeof ( SPCModeParameterHeader6 ) );
	printf ( "SPCModeParameterHeader10 = %ld\n", ( UInt32 ) sizeof ( SPCModeParameterHeader10 ) );
	
	// Mode parameter block descriptors
	printf ( "ModeParameterBlockDescriptor = %ld\n", ( UInt32 ) sizeof ( ModeParameterBlockDescriptor ) );
	printf ( "DASDModeParameterBlockDescriptor = %ld\n", ( UInt32 ) sizeof ( DASDModeParameterBlockDescriptor ) );
	printf ( "LongLBAModeParameterBlockDescriptor = %ld\n", ( UInt32 ) sizeof ( LongLBAModeParameterBlockDescriptor ) );
	
	// Mode Page format header
	printf ( "ModePageFormatHeader = %ld\n", ( UInt32 ) sizeof ( ModePageFormatHeader ) );
	
	// SPC Mode pages
	printf ( "SPCModePagePowerCondition = %ld\n", ( UInt32 ) sizeof ( SPCModePagePowerCondition ) );
	
	// SBC Mode pages
	printf ( "SBCModePageFormatDevice = %ld\n", ( UInt32 ) sizeof ( SBCModePageFormatDevice ) );
	printf ( "SBCModePageRigidDiskGeometry = %ld\n", ( UInt32 ) sizeof ( SBCModePageRigidDiskGeometry ) );
	printf ( "SBCModePageFlexibleDisk = %ld\n", ( UInt32 ) sizeof ( SBCModePageFlexibleDisk ) );
	printf ( "SBCModePageCaching = %ld\n", ( UInt32 ) sizeof ( SBCModePageCaching ) );
	
	printf ( "\n" );
	
}

static void
PrintSCSICmds_REQUEST_SENSE_Sizes ( void )
{

	printf ( "REQUEST_SENSE sizes\n" );
	
	// Standard REQUEST_SENSE data
	printf ( "SCSI_Sense_Data = %ld\n", ( UInt32 ) sizeof ( SCSI_Sense_Data ) );
	
	printf ( "\n" );
	
}