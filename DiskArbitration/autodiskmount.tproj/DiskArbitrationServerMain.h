/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __DISK_ARB_SERVER_MAIN__
#define __DISK_ARB_SERVER_MAIN__

//------------------------------------------------------------------------

#include <mach/mach.h>

#include <sys/types.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Authorization.h>

#include <IOKit/IOKitLib.h>

#include "DiskArbitrationTypes.h"

#define PID_FILE "/var/run/autodiskmount.pid"


//------------------------------------------------------------------------

void LogErrorMessage(const char *format, ...);

extern int gDebug;

/* Debug warning */

#define dwarning(a) { if (gDebug) { printf a; fflush(stdout); } }

/* Production warning */

#if 1
#define pwarning(a) { LogErrorMessage a; }
#else
#define pwarning(a) { printf a; fflush(stdout); }
#endif

#define DISKSTATE(x)	(x)==kDiskStateIdle ? "Idle" : \
						(x)==kDiskStateNew ? "New" : \
						(x)==kDiskStateNewlyUnmounted ? "NewlyUnmounted" : \
						(x)==kDiskStateNewlyEjected ? "NewlyEjected" : \
						(x)==kDiskStateToBeUnmounted ? "ToBeUnmounted" : \
						(x)==kDiskStateToBeEjected ? "ToBeEjected" : \
(x)==kDiskStateToBeUnmountedAndEjected ? "ToBeUnmountedAndEjected" : \
(x)==kDiskStateWaitingForMountApproval ? "WaitingForMountApproval" : \
(x)==kDiskStateMountApprovalDenied ? "ApprovalDenied" : \
						"<UNKNOWN>"

#define STR(x)			(x) ? (x) : "(null)"

//------------------------------------------------------------------------

void SetBlueBoxBootVolume( int seqno );
int GetBlueBoxBootVolume( void );

//------------------------------------------------------------------------

enum ClientState {
	kClientStateIdle = 0,
	kClientStateNew,
};

typedef enum ClientState ClientState;

/* Steal a bit out of the top of the client->flags word. */

enum
{
        kDiskArbIAmBlueBox 				= 1 << 31, /* Flag set by SetBlueBoxBootVolume */
        kDiskArbClientHandlesUninitializedDisks		= 1 << 30, /* Flag set by client handling uninited disks */
};

struct Client {
        struct Client * next;
	mach_port_t		port;
	pid_t			pid;
	unsigned		flags;
	ClientState		state;
	unsigned		numAcksRequired;
        int			notifyOnDiskTypes;
        int			unrecognizedPriority;
        void *			ackOnUnrecognizedDisk;
        AuthorizationRef	clientAuthRef;
};

typedef struct Client Client;
typedef struct Client * ClientPtr;

//------------------------------------------------------------------------

ClientPtr NewClient( mach_port_t port, unsigned pid, unsigned flags );

void PrintClient(ClientPtr clientPtr);
void PrintClients(void);

ClientPtr LookupClientByPID( pid_t pid );
ClientPtr LookupClientByMachPort( mach_port_t port );

unsigned NumClientsDesiringAsyncNotification( void );
unsigned NumClientsDesiringAsyncNotificationOfType( int type );

//------------------------------------------------------------------------

enum AckState {
	kSendMsg,
	kWaitingForAck,
	kAckReceived,
};

typedef enum AckState AckState;

struct AckValue {
	pid_t			pid;
	AckState		state;
	int				errorCode;
};

typedef struct AckValue AckValue;

struct AckValues {
	int				physicalLength;
	int				logicalLength;
	AckValue	*	ackValues; // ptr to an array
};

typedef struct AckValues AckValues;

extern int currentConsoleUser;

AckValues * NewAckValues( int size );
void FreeAckValues( AckValues * p );
void InitAckValue( AckValues * p, pid_t pid );
void UpdateAckValue( AckValues * p, pid_t pid, int errorCode );
int NumUnsetAckValues( AckValues * p );
int NumUnsetAckValuesForAllDisks( void );
void PrintAckValues( AckValues * p );
AckValue * GetDissenterFromAckValues( AckValues * ackValuesPtr );
AckValue * GetDissenterFromAckValuesForAllDisks( void );
AckValue * GetUnresponderFromAckValues( AckValues * ackValuesPtr );
AckValue * GetUnresponderFromAckValuesForAllDisks( void );

void MakeDeadClientAgreeable( ClientPtr clientPtr );

void StartDiskRegistrationCompleteThread(ClientPtr client);

//------------------------------------------------------------------------

enum DiskState {
	kDiskStateIdle = 0,				/* may be mounted or unmounted */
	kDiskStateNew,					/* not yet probed for mounting */
	kDiskStateNewlyUnmounted,			/* newly unmounted - need to send post-unmount notifications to interested clients */
	kDiskStateNewlyEjected,				/* newly ejected - need to send post-eject notifications to interested clients */
	kDiskStateToBeUnmounted,			/* to be unmounted - waiting for pre-unmount acknowledgements from interested clients */
	kDiskStateToBeEjected,				/* to be ejected - waiting for pre-eject acknowledgements from interested clients */
    kDiskStateToBeUnmountedAndEjected,		/* to be unmounted then ejected - waiting for pre-unmount acknowledgements from interested clients */
    kDiskStateUnrecognized,				/* unrecognized disks can now be ignored */
    kDiskStatePostponed,				/* Disk has appeared, but we aren't yet ready to process it */
    kDiskStateWaitingForMountApproval,
    kDiskStateMountApprovalDenied,
};

typedef enum DiskState DiskState;

enum DiskFamily {
	kDiskFamily_Null = 0,
	kDiskFamily_SCSI,
	kDiskFamily_IDE,
	kDiskFamily_Floppy,
	kDiskFamily_File,
	kDiskFamily_AFP,
};

typedef enum DiskFamily DiskFamily;

struct Disk {
        struct Disk *		next;
        char *			ioBSDName;
        int			ioBSDUnit;
        char *			ioContent;
        char *			ioMediaNameOrNull;
        char *			ioDeviceTreePath;
        char *			mountpoint;
        char *			mountedFilesystemName;
        DiskFamily		family;
        unsigned		flags;
        int			sequenceNumber; // -1 if not applicable, i.e., not mounted
        DiskState		state;
        AckValues	*	ackValues; // NULL, except while processing an unmount/eject request
        int			mountedUser;
        io_object_t		service;
        int			retainingClient;	// gets set when a client retains, 0 when no one retains, only useful to whole disks
        ClientPtr		lastClientAttemptedForUnrecognizedMessages;
        UInt64			ioSize;
        unsigned int		wholeDiskContainsMountedChild:1;  // only useful to whole disks
        unsigned int		wholeDiskHasBeenYanked:1; 	// only useful to the whole disk
        unsigned int		mountAttempted:1;		// gets set when no user is logged in, in "client" mode
        unsigned int		admCreatedMountPoint:1;		// gets set if adm creates the mountpoint - should alleviate problems with deleting someone elses mounts 

        unsigned int		ejectOnLogout:1;		// only gets set from a property in the ioregistry for a mounted disk image
        unsigned int		forceTakeOwnershipOnDisk:1; // gets set from mount and own being true
        unsigned int		approvedForMounting:1; // gets set from mount and own being true

        double			timeAppeared;  // When did we (adm) first see this diskPtr?
        char *			volName;	// what is the real name on the disk so that apps don't have to probe
        unsigned int		unmountOrEjectRequestorUID;

};

typedef struct Disk Disk;
typedef struct Disk * DiskPtr;

//------------------------------------------------------------------------

// Returns kDiskStateToBeUnmountedAndEjected, kDiskStateToBeUnmounted, or kDiskStateToBeEjected
// if there are any such disks.
// Otherwise returns kDiskStateIdle

DiskState AreWeBusyForDisk( DiskPtr diskPtr );

//------------------------------------------------------------------------

DiskPtr NewDisk(	char * ioBSDName,
					int ioBSDUnit,
					char * ioContentOrNull,
					DiskFamily family,
					char * mountpoint,
					char * ioMediaNameOrNull,
					char * ioDeviceTreePathOrNull,
                                        io_object_t	service,
                                        int	mountingUIDFromDevice,
					unsigned flags,
					UInt64 size );

void FreeDisk( DiskPtr diskPtr );

void PrintDisks(void);

void DiskSetMountpoint( DiskPtr diskPtr, const char * mountpoint );
void DiskSetVolumeName( DiskPtr diskPtr, const char * volName );

DiskPtr LookupDiskByIOBSDName( char * ioBSDName );

int UnmountDisk( DiskPtr diskPtr, int forceUnmount );

int UnmountAllPartitions( DiskPtr diskPtr, int forceUnmount );

int EjectDisk( DiskPtr diskPtr );

void SetStateForOnePartition( DiskPtr diskPtr, DiskState newState );
void SetStateForAllPartitions( DiskPtr diskPtr, DiskState newState );

int NumPartitionsMountedFromThisDisk( DiskPtr diskPtr ); /* inclusive */
int NumPartitionsToBeUnmountedAndEjectedFromThisDisk( DiskPtr diskPtr ); /* inclusive */

DiskPtr LookupWholeDiskForThisPartition( DiskPtr diskPtr );

typedef void (*LookupWholeDisksForThisPartitionApplierFunction)( DiskPtr diskPtr );
void LookupWholeDisksForThisPartition( io_registry_entry_t service, LookupWholeDisksForThisPartitionApplierFunction applier );

DiskPtr LookupWholeDiskToBeEjected( void );

boolean_t IsWhole( DiskPtr diskPtr );
boolean_t IsNetwork( DiskPtr diskPtr );

//------------------------------------------------------------------------

void PrepareToSendPreUnmountMsgs( );
void PrepareToSendPreEjectMsgs( );
void PrepareToSendPreMountMsgsForDisk( DiskPtr diskPtr );

void SendUnrecognizedDiskMsgs(mach_port_t port, char *devname, char *fstype, char *deviceType, int isWritable, int isRemovable, int isWhole );
void SendUnrecognizedDiskArbitrationMsgs(mach_port_t port, char *devname, char *fstype, char *deviceType, int isWritable, int isRemovable, int isWhole, int diskType );

void SendDiskChangedMsgs(char *devname, char *newMountpoint, char *newVolumeName, int flags, int success);

void SendDiskWillBeCheckedMessages( DiskPtr disk );

void SendCallFailedMessage(ClientPtr clientPtr, DiskPtr diskPtr, int failedType, int error);
void SendCallSucceededMessage(ClientPtr clientPtr, DiskPtr diskPtr, int succeededType);

int SendDiskAppearedMsgs( void );
void SendPreMountMsgsForDisk( DiskPtr diskPtr );
void SendPreUnmountMsgsForDisk( DiskPtr diskPtr );
void SendPreEjectMsgsForDisk( DiskPtr diskPtr );
void SendUnmountPostNotifyMsgsForOnePartition( char * ioBSDName, int errorCode, pid_t pid );
void SendEjectPostNotifyMsgsForOnePartition( char * ioBSDName, int errorCode, pid_t pid );
void SendEjectPostNotifyMsgsForAllPartitions( DiskPtr diskPtr, int errorCode, pid_t pid );
void SendBlueBoxBootVolumeUpdatedMsgs( void );
void SendCompletedMsgs( int messageType, int newDisks );

void SendClientWasDisconnectedMsg(ClientPtr Client);

void CompleteEjectForDisk( DiskPtr diskPtr );
void CompleteUnmountForDisk( DiskPtr diskPtr );

char *mountPath( void );
void cleanUpAfterFork(int fdp[]);
boolean_t
do_exec(const char * dir, const char * argv[], int * result, char * * output);


int requestingClientHasPermissionToModifyDisk(pid_t pid, DiskPtr diskPtr, char *right);

kern_return_t DiskArbUnmountAndEjectRequest_async_rpc (
        mach_port_t server, pid_t pid,
        DiskArbDiskIdentifier diskIdentifier,
                                                       unsigned flags);

int DiskTypeForDisk(DiskPtr diskPtr);
ClientPtr GetNextClientForDisk(DiskPtr diskPtr);

int authorizationAllowedForEvent(ClientPtr client, char *event);

//------------------------------------------------------------------------

int DiskArbitrationServerMain(int argc, char* argv[]);

kern_return_t EnableDeathNotifications(mach_port_t port);

//------------------------------------------------------------------------

typedef struct {
    boolean_t	verbose;
    boolean_t	debug;
    DiskPtr	Disks;
    unsigned	NumDisks;
    int			NumDisksAddedOrDeleted; // incremented each time a disk is added or removed as a flag for debug output
    ClientPtr	Clients;
    unsigned	NumClients;
    int 		readOnlyBoot;
} GlobalStruct;

extern GlobalStruct g;

#define INTERNAL_MSG		10  // some random number I picked :)

//------------------------------------------------------------------------

// this structure's parts are significantly overloaded - see each thread for actual usage ...

typedef struct {
    int			diskAppearedType;
    mach_port_t	port;
    char		*ioBSDName;		// stores volume for DiskChanged
    unsigned	flags;
    char		*mountpoint;		// stores newMountpoint for DiskChanged
    int			pid;
    char		*ioDeviceTreePath;
    char		*ioContent;		// stores
    int			sequenceNumber;
    int			diskType;
    int			isWritable;
    int			isRemovable;
    int			isWhole;
    double		timeAppeared;
    char 		*fsName;
    char		*fsType;
} DiskThreadRecord;

DiskThreadRecord * NewDiskThreadRecord();

//------------------------------------------------------------------------

// Display Messages for Uninitialized disks

extern CFStringRef yankedHeader;
extern CFStringRef yankedMessage;
extern CFStringRef unrecognizedHeader;
extern CFStringRef unrecognizedHeaderNoInitialize;
extern CFStringRef unrecognizedMessage;
extern CFStringRef ejectString;
extern CFStringRef ignoreString;
extern CFStringRef initString;
extern CFStringRef launchString;
extern CFStringRef someDisk;
extern CFStringRef mountOrFsckFailed;
extern CFStringRef unknownString;
extern CFStringRef mountOrFsckFailedWithDiskUtility;


#define YANKED_MESSAGE CFSTR("")
#define UNINITED_MESSAGE CFSTR("")
#define UNINITED_HEADER CFSTR("You have inserted a disk containing no volumes that Mac OS X can read.  To use the unreadable volumes, click Initialize.  To continue with the disk inserted, click Ignore.")
#define UNINITED_HEADER_NO_INIT CFSTR("You have inserted a disk containing no volumes that Mac OS X can read.  To continue with the disk inserted, click Ignore.")
#define YANKED_HEADER CFSTR("The device you removed was not properly put away. Data might have been lost or damaged. Before disconnecting a device, you should select its icon in the Finder and choose Eject from the File menu.")
#define EJECT_BUTTON CFSTR("Eject")
#define IGNORE_BUTTON CFSTR("Ignore")
#define INIT_BUTTON CFSTR("Initialize...")
#define LAUNCH_BUTTON CFSTR("Launch Disk Utility...")
#define A_DISK CFSTR("A disk attempting to mount as ")
#define MOUNT_FAILED_WITH_DU CFSTR(" has failed verification or has failed to mount.  Please use Disk Utility to check the disk and correct any errors.")
#define MOUNT_FAILED CFSTR(" has failed verification or has failed to mount.")
#define UNKNOWN_STRING CFSTR("Unknown")

#endif
