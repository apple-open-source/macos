/*
 *  AppleDallasDriver.h
 *  AppleDallasDriver
 *
 *  Created by Keith Cox on Tue Jul 17 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */
#include <IOKit/IOService.h>

enum {
    kDallasDeviceFamilySpeaker = 1,
    kDallasDeviceFamilyPowerSupply = 2
};

enum {
	kRetryCountSeed				=	1000
};

enum DallasFamilyID {
	kDeviceFamilyUndefined		=	0,
	kDeviceFamilySpeaker		=	1,
	kDeviceFamilyPowerSupply	=	2
};

enum DallasSpeakerID {
	kDallasSpeakerRESERVED		=	0,
	kSpeakerModel_P43			=	1,
	kSpeakerModel_P74			=	2,
	kSpeakerModel_Q5			=	3
};

enum DallasROMAddress {
	kDallasIDAddress			=	0
};

struct SpkrID {
	UInt8		deviceFamily;
	UInt8		deviceType;
};
typedef struct SpkrID SpkrID;
typedef SpkrID *SpkrIDPtr;

struct DallasID {
	UInt8		deviceFamily;
	UInt8		deviceType;
	UInt8		deviceSubType;
	UInt8		deviceReserved;
};
typedef struct DallasID DallasID;
typedef DallasID *DallasIDPtr;

class AppleDallasDriver : public IOService
{
OSDeclareDefaultStructors(AppleDallasDriver)
public:
	virtual bool init(OSDictionary *dictionary = 0);
	virtual void free(void);
	virtual IOService *probe(IOService *provider, SInt32 *score);
	virtual bool start(IOService *provider);
	virtual void stop(IOService *provider);
	
	virtual bool readDataROM( UInt8 *bEEPROM, int dallasAddress, int size );	//	this is a maximum of 32 bytes
	virtual bool readSerialNumberROM(UInt8 *bROM);								//	this is a 64 bit value
	virtual bool readApplicationRegister(UInt8 *bAppReg);						//	this is a 64 bit value
	
	virtual bool getSpeakerID (UInt8 *bEEPROM);

protected:
    IODeviceMemory  *gpioRegMem;
};
