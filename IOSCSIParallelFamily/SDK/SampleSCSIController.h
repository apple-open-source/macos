// Sample SCSI Parallel Interface Controller Driver

#include <IOKit/scsi/IOSCSIParallelInterfaceController.h>

class SampleSCSIController : public IOSCSIParallelInterfaceController
{
public:
	// The ExecuteParallelTask call is made by the client to submit a SCSIParallelTask
	// for execution.
	// -->the actual return value should not be void but maybe a SCSI Service response
	virtual SCSIServiceResponse ExecuteParallelTask( SCSIParallelTask * parallelRequest );

protected:
	// ---- Methods to retrieve the HBA specifics. ----
	// These methods will not be called before the InitializeController call,
	// and will not be called after the TerminateController call.  But in the
	// interval between those calls, they shall report the correct requested
	// information.
	
	// This method will be called to determine the SCSI Device Identifer that
	// the Initiator is assigned for this HBA.
	virtual SCSIDeviceIdentifier	ReportInitiatorIdentifier( void );

	// This method will be called to determine the value of the highest SCSI
	// Device Identifer supported by the HBA.  This value will be used to
	// determine the last ID to process
	virtual SCSIDeviceIdentifier	ReportHighestSupportedDeviceID( void );
	
	// This method will be called to retrieve the maximum number of tasks that
	// the HBA can process during the same 
	virtual UInt32					ReportMaximumTaskCount( void );
	
	// This method is used to retireve the amount of memory that will be allocated
	// in the SCSI Parallel Task for HBA specific use.
	virtual UInt32					ReportHBASpecificDataSize( void );
	
	virtual bool	InitializeController( void );
	virtual bool	TerminateController( void );

	virtual bool	StartController( void );
	virtual void	StopController( void );
	
	// ---- Suspend and Resume Methods for the subclass ----
	// The SuspendServices method
	virtual bool	SuspendServices( void );
	
	// The ResumeServices method
	virtual void	ResumeServices( void );
	
	// ---- Support for Interrupt notification
	// The HandleInterruptRequest is used to notifiy an HBA specific subclass that an 
	// interrupt request needs to be handled.
	virtual void	HandleInterruptRequest( void );
	
	// All necessary supporting methods for handling interrupts 
}
