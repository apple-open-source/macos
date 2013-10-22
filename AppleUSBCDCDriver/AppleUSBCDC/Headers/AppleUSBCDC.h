/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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
	
	/*AppleUSBCDC.h - This file contains the class definition for the	*/
	/* USB Communication Device Class (CDC) driver			 	*/

class AppleUSBCDC : public IOService
{
    OSDeclareDefaultStructors(AppleUSBCDC);			// Constructor & Destructor stuff

private:
    bool			fTerminate;				// Are we being terminated (ie the device was unplugged)
    bool			fStopping;				// Are we being "stopped"
//	UInt8			fCDCInterfaceNumber;	// CDC interface number (the first one found)
	UInt8			fConfig;				// The current Configuration value
	IOCommandGate	*fCommandGate;
    IOWorkLoop		*fWorkLoop;

public:

    IOUSBDevice			*fpDevice;
    UInt8			fbmAttributes;
    UInt8			fCacheEaddr[6];
	UInt8			fDevClass;
	UInt8			fDevSubClass;
	UInt8			fDevProtocol;
	bool			fIAD;					// Set if we're looking at a device with an IAD descriptor
    UInt8           bFirstInterface;        // First Interface if MBIM
    UInt8           bInterfaceCount;        // Number of Interfaces that make up this function (Per the Spec Number of **contiguous** interfaces that are associated with this function.)
	
	UInt8			fDataInterfaceNumber;	// Data interface number (if there's only one)

        // IOKit methods
		
	virtual IOService	*probe(IOService *provider, SInt32 *score);
    virtual bool		start(IOService *provider);
    virtual void		free(void);
    virtual void		stop(IOService *provider);
    virtual IOReturn 	message(UInt32 type, IOService *provider,  void *argument = 0);
    virtual IOReturn	setProperties( OSObject * properties );
	static IOReturn		setPropertiesAction( OSObject *owner,void *arg1, void *arg2,void *arg3,void	*arg4 );
    virtual IOReturn	setPropertiesWL( OSObject * properties );
    virtual IOCommandGate *getCommandGate() const;
												
        // CDC Driver Methods
	
    UInt8			Asciihex_to_binary(char c);
    virtual IOUSBDevice		*getCDCDevice(void);
	bool			checkDevice(IOUSBDevice *theDevice);
    bool			initDevice(UInt8 numConfigs);
	virtual IOReturn		reInitDevice(void);
    bool			checkACM(IOUSBInterface *Comm, UInt8 cInterfaceNumber, UInt8 dataInterfaceNum);
    bool			checkECM(IOUSBInterface *Comm, UInt8 cInterfaceNumber, UInt8 dataInterfaceNum);
    bool			checkWMC(IOUSBInterface *Comm, UInt8 cInterfaceNumber, UInt8 dataInterfaceNum);
    bool			checkDMM(IOUSBInterface *Comm, UInt8 cInterfaceNumber, UInt8 dataInterfaceNum);
	bool			checkMBIM(IOUSBInterface *Comm, UInt8 cInterfaceNumber, UInt8 dataInterfaceNum);
    virtual bool	confirmDriver(UInt8 subClass, UInt8 dataInterface);
	virtual bool	confirmControl(UInt8 subClass, IOUSBInterface *CInterface);

}; /* end class  */