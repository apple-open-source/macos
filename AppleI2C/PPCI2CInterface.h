/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * Interface definition for the keylargo I2C interface
 *
 * HISTORY
 *
 */

#ifndef _PPCI2CINTERFACE_H
#define _PPCI2CINTERFACE_H 

#include <IOKit/IOTypes.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/IOService.h>

// IMPORTANT NOTE:
// This driver provides the basic functionality to communicate with
// devices on the i2c bus. To "talk" with a device the developer needs
// first to find the i2c interface. This is very easy since the driver
// does a registerService. Once the client has a reference to this
// driver it needs to follow the following important rule:
// HOLD THE BUS FOR AS SHORT AS POSSIBLE !!! Since each client may need
// to access to the bus in a different way it is not "polite" to hold the
// bus for a long time. For example opening the i2c bus in ::start and closing
// it in ::stop is very bad (and most of all it will not work, to try to
// enforce corectness in using the driver I make sure that the tread that
// opens is the same that uses the other functionalities).
// This is a good way to use the bus:
// open i2c
// setup the bus in the way you need to use it
// do the the write/read (or block of writes and reads)
// close the 12c.
// the sequence open/setup/use/close should be as compact as possible
// (it is great if it is concertrated all in the same function) and
// it MUST be all in the same thread.

class PPCI2CInterface : public IOService
{

    OSDeclareDefaultStructors(PPCI2CInterface)

private:
    // These are the possible states the driver can be in:
    typedef enum {
    ki2cStateIdle        = 0,
    ki2cStateWaitingForIADDR,
    ki2cStateWaitingForIDATA,
    ki2cStateWaitingForISTOP,
    ki2cStateWaitingForISTART,
    } PPCI2CState;

    // Constansts for the mode register:
    typedef enum {
        kPortMask        = 0x0F  //
    }I2CPort;

    typedef enum {
        kDumbMode        = 0x00, // 
        kStandardMode    = 0x01, //
        kStandardSubMode = 0x02, //
        kCombinedMode    = 0x03, //
        kModeMask        = 0x03  //
    } I2CMode;

    typedef enum {
        k100KhzMode      = 0x00, //
        k50KhzMode       = 0x01, //
        k25KhzMode       = 0x02, //
        kReservedMode    = 0x03, //
        kSpeedMask       = 0x03  //
    } I2CSpeed;

    enum {
        I2CPortShift = 4
    };

    enum {
        I2CModeShift = 2
    };

    enum {
        I2CSpeedShift = 0
    };

    // Constants for the Control register
    typedef enum {
        kClrCNTRL        = 0x00, // 0 -> Clears all the control bits
        kAakCNTRL        = 0x01, // 1 -> AAK sent, 0 -> not AAK sent
        kXAddrCNTRL      = 0x02, // when set -> transmit address phase (not used by manual mode)
        kStopCNTRL       = 0x04, // when set -> transmit stop condition
        kStartCNTRL      = 0x08, // when set -> transmit start condition (manual mode only)
        kCNTRLMask       = 0x0F  // Masks all the control bits
    } I2CControl;

    enum {
        I2CControlShift = 0
    };

    // Constants for the STATUS register
    typedef enum {
        kBusySTATUS          = 0x01, // 1 -> busy
        kLastAakSTATUS       = 0x02, // value of last AAK bit
        kLastReadWriteSTATUS = 0x04, // value of last R/W bit transmitted
        kIsdaSTATUS          = 0x08, // data line SDA
        kSclSTATUS           = 0x10, // clock line SCL
        kSTATUSMask          = 0x1F  // Mask all the status bits
    } I2CStatus;

    enum {
        I2CStatusShift = 0
    };

    // Constants for the ISR register
    typedef enum {
        kIDataISR            = 0x01, // Data Byte Sent or Received Interrupt
        kIAddrISR            = 0x02, // Address Phase Sent Interrupt
        kIStopISR            = 0x04, // Stop Condition Sent Interrupt
        kIStartISR           = 0x08, // Start Condition Sent Interrupt
        kISRMask             = 0x0F
    } I2CInterruptStatus;

    enum {
        I2CInterruptStatusShift = 0
    };

    // Constants for the IER register
    typedef enum {
        kEDataIER           = 0x01, // Enable Data Byte Sent or Received Interrupt
        kEAddrIER           = 0x02, // Enable AAddress Phase Sent Interrupt
        kEStopIER           = 0x04, // Enable Stop Condition Sent Interrupt
        kEStartIER          = 0x08, // Enable Start Condition Sent Interrupt
        kIERMask            = 0x0F
    } I2CInterruptEnable;

    enum {
        I2CInterruptEnableShift = 0
    };

    // Constants for the Address register
    enum I2CAddress {
        kADDRMask           = 0x7F   //
    };

    typedef enum {
        kWriteADDR          = 0x00, //
        kReadADDR           = 0x01, //
        kRWMask             = 0x01  //
    } I2CRWMode;

    enum {
        I2CAddressShift     = 1
    };

    enum {
        I2CRWShift          = 0
    };

    // redefine the types so it makes easyer to handle
    // new i2c if they have wider registers.
    typedef UInt8 *I2CRegister;
    typedef UInt32 *I2CLongRegister;

    // The ioblock where we have the i2c registers:
    IOMemoryMap *i2cRegisterMap;

    // This is the name of the resource exported by this driver:
    char resourceName[64];

    // These are the keylargo registers to access to the
    // i2c bus:
    I2CRegister mode;                  // Configure the transmission mode of the i2c cell and the databit rate.
    I2CRegister control;               // Holds the 4 bits used to start the operations on the i2c interface.
    I2CRegister status;                // Status bits for the i2 cell and the i2c interface.
    I2CRegister ISR;                   // Holds the status bits for the interrupt conditions.
    I2CRegister IER;                   // Eneables the bits that allow the four interrupt status conditions.
    I2CRegister address;               // Holds the 7 bits address and the R/W bit.
    I2CRegister subAddr;               // the 8bit subaddress..
    I2CRegister data;                  // the byte to sents or the last byte received

    // Remebers the provider so when we work as interrupt-driven we can enable and disable the
    // interrupts:
    IOService *myProvider;
    
    // Keeps track of the success (or faliture) of the last transfer:
    bool transferWasSuccesful;

    // When the driver is not in polling mode (so it is interrupt driven) the
    // following assures that all the transactions are syncronous.
    volatile semaphore_t mySync;
    
    // This is a parameter used in memory cells and useless for
    // the mac-io.
    UInt8 portSelect;

    // This is the current state for the driver:
    PPCI2CState currentState;

    // This interface does not need to be attached to an interrrupt. (it is obvoiusly
    // better to be, but it is not NECESSARY). When it is not attached to an interrupt
    // it works in polling mode. The following bool flag sets the default behavior.
    bool pollingMode;

    // Only one driver a time can use the I2C bus
    // so this lock provides mutual exclusive accesses. To ensure that
    // the the same thread can access the service after the lock has been
    // obrained I am going to use a recursive lock:
    IORecursiveLock *mutexLock;

protected:
    // Chaches the last mode set (I would not do this, but each access to getMode requires a mask and a shift):
    I2CMode lastMode;
    
    // pointer to the data to be transfered
    UInt8 *dataBuffer;
    
    // and the number of bytes still to transfer
    UInt16 nBytes;

    // the current transfer address:
    UInt8 currentAddress;

    // the current transfer subAddress:
    UInt8 currentSubaddress;
    
    // Direction of the data
    bool isReading;

    // prints the content of the registers:
    void dumpI2CRegisters();
    
    // Given the base of the i2c registers inits all the registers.
    void SetI2CBase(UInt8 *baseAddress, UInt8 steps);

    // Returns the mask to use with the register:
    UInt8 shiftedMask(UInt8 mask, UInt8 shift);

    // Returns the complement of the mask
    UInt8 shiftedCompMask(UInt8 mask, UInt8 shift);

    // Generic read and write for register fields:
    UInt8 readRegisterField(I2CRegister, UInt8, UInt8);
    void writeRegisterField(I2CRegister, UInt8, UInt8, UInt8);

    // Intermediate methods to access to each field of all the registers:
 
    // Mode register:
    void setPort(UInt8);
    UInt8 getPort();
    void setMode(I2CMode);
    I2CMode getMode();
    void setSpeed(I2CSpeed);
    I2CSpeed getSpeed();

    // Control register
    void setControl(I2CControl);
    I2CControl getControl();

    // Status register
    void setStatus(I2CStatus);
    I2CStatus getStatus();

    // Interrupt status
    void setInterruptStatus(I2CInterruptStatus);
    I2CInterruptStatus getInterruptStatus();

    // Interrupt enable
    void setInterruptEnable(I2CInterruptEnable);
    I2CInterruptEnable setInterruptEnable();

    // Address Register:
    void setAddressRegister(UInt8, I2CRWMode);
    void setAddress(UInt8);
    UInt8 getAddress();

    void setReadWrite(I2CRWMode);
    I2CRWMode getReadWrite();

    // SubAddress register
    void setSubAddress(UInt8);
    UInt8 getSubAddress();

    // Data register
    void setData(UInt8);
    UInt8 getData();

    // handles the hardware interrupts for the i2c:
    static void handleHardwareInterrupt(OSObject *target, void *refCon, IOService *nub, int source);
    
    // methods to setup and abort a transfer:
    // (inheriting classes must call the parten method)
    virtual bool setAddressAndDirection();
    virtual bool abortTransfer();
   
    // Waits for the completion of a read or write
    // operation:
    bool waitForCompletion();

    // Each mode requires a specific interrupt handler (since the states are different for each mode)
    // so here it is the one for the Standard + SubAddress mode:
    bool i2cStandardSubModeInterrupts(UInt8 interruptStatus);

    // Initialize the address of the registers and the registers themselfs:
     virtual bool initI2CBus(UInt8 *baseAddress, UInt8 steps);

    // These instead set the speed:
    virtual bool setKhzSpeed(UInt);

    // The interrupt handler:
    // (inheriting classes must call the parent method)
    virtual bool handleI2CInterrupt();
    
public:
    // the standard inherits from IOService:
    virtual void free();
    virtual bool start(IOService*);

    // Starts the use of the interface:
    virtual bool openI2CBus(UInt8 port);
    
    // These are to setup the mode for the I2C bus:
    virtual bool setPollingMode(bool);
    virtual void setDumbMode();
    virtual void setStandardMode();
    virtual void setStandardSubMode();
    virtual void setCombinedMode();

    // Test to read the values set by the funtions above:
    virtual bool isInDumbMode();
    virtual bool isInStandardMode();
    virtual bool isInStandardSubMode();
    virtual bool isInCombinedMode();

    // These instead returns the speed:
    virtual UInt getKhzSpeed();
    
    // Writes a block of data at a given address:
    virtual bool writeI2CBus(UInt8 address, UInt8 subAddress, UInt8 *data, UInt16 len);

    // Reads a block of data at a given address:
    virtual bool readI2CBus(UInt8 address, UInt8 subAddress, UInt8 *data, UInt16 len);

    // End using the interface:
    virtual bool closeI2CBus();

    // Returns the name of the resource depending from
    // where the interface attaches:
    // the two most common names are:
    // PPCI2CInterface.i2c-uni-n for the i2c to uni-n
    // PPCI2CInterface.i2c-mac-io for the i2c to mac-io
    // but look in the OF device tree to find out the
    // extension of the i2c bus you are interested in.
    virtual const char * getResourceName();
};

#endif //_PPCI2CINTERFACE_H
