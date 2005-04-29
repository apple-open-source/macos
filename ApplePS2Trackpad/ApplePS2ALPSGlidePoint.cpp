/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
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

#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include "ApplePS2ALPSGlidePoint.h"

enum {
    //
    //
    kTapEnabled  = 0x01
};

// =============================================================================
// ApplePS2ALPSGlidePoint Class Implementation
//

#define super IOHIPointing
OSDefineMetaClassAndStructors(ApplePS2ALPSGlidePoint, IOHIPointing);

UInt32 ApplePS2ALPSGlidePoint::deviceType()
{ return NX_EVS_DEVICE_TYPE_MOUSE; };

UInt32 ApplePS2ALPSGlidePoint::interfaceID()
{ return NX_EVS_DEVICE_INTERFACE_BUS_ACE; };

IOItemCount ApplePS2ALPSGlidePoint::buttonCount() { return 2; };
IOFixed     ApplePS2ALPSGlidePoint::resolution()  { return _resolution; };
bool IsItALPS(UInt8 byte0, UInt8 byte1, UInt8 byte2);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool ApplePS2ALPSGlidePoint::init( OSDictionary * properties )
{
    //
    // Initialize this object's minimal state. This is invoked right after this
    // object is instantiated.
    //
    
    if (!super::init(properties))  return false;

    _device                    = 0;
    _interruptHandlerInstalled = false;
    _packetByteCount           = 0;
    _resolution                = (100) << 16; // (100 dpi, 4 counts/mm)
    _touchPadModeByte          = kTapEnabled;

    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

ApplePS2ALPSGlidePoint *
ApplePS2ALPSGlidePoint::probe( IOService * provider, SInt32 * score )
{
	UInt8   byte0, byte1, byte2;
    //
    // The driver has been instructed to verify the presence of the actual
    // hardware we represent. We are guaranteed by the controller that the
    // mouse clock is enabled and the mouse itself is disabled (thus it
    // won't send any asynchronous mouse data that may mess up the
    // responses expected by the commands we send it).
    //

    ApplePS2MouseDevice * device  = (ApplePS2MouseDevice *) provider;
    PS2Request *          request = device->allocateRequest();
    bool                  success = false;
    
    if (!super::probe(provider, score) || !request) return 0;

    //
    // Send an "Identify TouchPad" command and see if the device is
    // a ALPS GlidePoint based on its response.  End the command
    // chain with a "Set Defaults" command to clear all state.
    //

    // ALPS GlidePoint detection
    request->commands[0].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[0].inOrOut  = kDP_SetDefaultsAndDisable;

    // set mouse sample rate 100
    request->commands[1].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[1].inOrOut  = kDP_SetMouseSampleRate;
    request->commands[2].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[2].inOrOut  = 100;

    // Set mouse resolution 0
    request->commands[3].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[3].inOrOut  = kDP_SetMouseResolution;
    request->commands[4].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[4].inOrOut  = 0;

    // 3X set mouse scaling 2 to 1
    request->commands[5].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[5].inOrOut  = kDP_SetMouseScaling2To1;
    request->commands[6].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[6].inOrOut  = kDP_SetMouseScaling2To1;
    request->commands[7].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[7].inOrOut  = kDP_SetMouseScaling2To1;

    // get mouse info
    request->commands[8].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[8].inOrOut  = kDP_GetMouseInformation;
    request->commands[9].command  = kPS2C_ReadDataPort;
    request->commands[9].inOrOut  = 0;
    request->commands[10].command = kPS2C_ReadDataPort;
    request->commands[10].inOrOut = 0;
    request->commands[11].command = kPS2C_ReadDataPort;
    request->commands[11].inOrOut = 0;
    request->commands[12].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[12].inOrOut  = kDP_SetDefaultsAndDisable;
    request->commandsCount = 13;

    device->submitRequestAndBlock(request);

    success = true;
	byte0 = request->commands[9].inOrOut;
	byte1 = request->commands[10].inOrOut;
	byte2 = request->commands[11].inOrOut;
	// IOLog("%s:%s three bytes returned are: [%02x %02x %02x]\n", getName(), __FUNCTION__, byte0, byte1, byte2);
	
    success = IsItALPS(byte0, byte1, byte2);
	_touchPadVersion = (byte2 & 0x0f) << 8 | byte0;
	
    device->freeRequest(request);

    return (success) ? this : 0;

}

bool IsItALPS(UInt8 byte0, UInt8 byte1, UInt8 byte2)
{
	bool	success = false;
	short   i;
	
	#define NUM_SINGLES 9
	static int singles[NUM_SINGLES * 3] ={
		0x33,0x2,0xa,
		0x53,0x2,0x0a,
		0x53,0x2,0x14,
		0x63,0x2,0xa,
		0x63,0x2,0x14,
//		0x73,0x2,0xa,	// 3622947
		0x63,0x2,0x28,
		0x63,0x2,0x3c,
		0x63,0x2,0x50,
		0x63,0x2,0x64};
	#define NUM_DUALS 3
	static int duals[NUM_DUALS * 3]={
		0x20,0x2,0xe,
		0x22,0x2,0xa,
		0x22,0x2,0x14};

	for(i = 0;i < NUM_SINGLES;i++) {
		if((byte0 == singles[i * 3] && (byte1 == singles[i * 3 + 1]) && byte2 == singles[i * 3 + 2]))
		{
			success = true;
			break;
		}
	}
	if(success == false)
	{
		for(i = 0;i < NUM_DUALS;i++)
		{
			if((byte0 == duals[i * 3]) && (byte1 == duals[i * 3 + 1]) && (byte2 == duals[i * 3 + 2]))
			{
				success = true;
				break;
			}
		}
	}
	return success;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool ApplePS2ALPSGlidePoint::start( IOService * provider )
{ 
    UInt64 gesturesEnabled;

    //
    // The driver has been instructed to start. This is called after a
    // successful probe and match.
    //

    if (!super::start(provider)) return false;

    //
    // Maintain a pointer to and retain the provider object.
    //

    _device = (ApplePS2MouseDevice *) provider;
    _device->retain();

    //
    // Announce hardware properties.
    //

    IOLog("ApplePS2Trackpad: ALPS GlidePoint v%d.%d\n",
          (UInt8)(_touchPadVersion >> 8), (UInt8)(_touchPadVersion));

    //
    // Write the TouchPad mode byte value.
    //

    setTapEnable(_touchPadModeByte);

    //
    // Advertise the current state of the tapping feature.
    //

    gesturesEnabled = (_touchPadModeByte == kTapEnabled)
                    ? 1 : 0;
    setProperty("Clicking", gesturesEnabled, sizeof(gesturesEnabled)*8);

    //
    // Must add this property to let our superclass know that it should handle
    // trackpad acceleration settings from user space.  Without this, tracking
    // speed adjustments from the mouse prefs panel have no effect.
    //

    setProperty(kIOHIDPointerAccelerationTypeKey, kIOHIDTrackpadAccelerationType);

    //
    // Install our driver's interrupt handler, for asynchronous data delivery.
    //

    _device->installInterruptAction(this,
        (PS2InterruptAction)&ApplePS2ALPSGlidePoint::interruptOccurred);
    _interruptHandlerInstalled = true;

    //
    // Enable the mouse clock (should already be so) and the mouse IRQ line.
    //

    setCommandByte( kCB_EnableMouseIRQ, kCB_DisableMouseClock );

    //
    // Finally, we enable the trackpad itself, so that it may start reporting
    // asynchronous events.
    //

    setTouchPadEnable(true);

    //
	// Install our power control handler.
	//

	_device->installPowerControlAction( this, (PS2PowerControlAction) 
             &ApplePS2ALPSGlidePoint::setDevicePowerState );
	_powerControlHandlerInstalled = true;

    return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ALPSGlidePoint::stop( IOService * provider )
{
    //
    // The driver has been instructed to stop.  Note that we must break all
    // connections to other service objects now (ie. no registered actions,
    // no pointers and retains to objects, etc), if any.
    //

    assert(_device == provider);

    //
    // Disable the mouse itself, so that it may stop reporting mouse events.
    //

    setTouchPadEnable(false);

    //
    // Disable the mouse clock and the mouse IRQ line.
    //

    setCommandByte( kCB_DisableMouseClock, kCB_EnableMouseIRQ );

    //
    // Uninstall the interrupt handler.
    //

    if ( _interruptHandlerInstalled )  _device->uninstallInterruptAction();
    _interruptHandlerInstalled = false;

    //
    // Uninstall the power control handler.
    //

    if ( _powerControlHandlerInstalled ) _device->uninstallPowerControlAction();
    _powerControlHandlerInstalled = false;

	super::stop(provider);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ALPSGlidePoint::free()
{
    //
    // Release the pointer to the provider object.
    //

    if (_device)
    {
        _device->release();
        _device = 0;
    }

    super::free();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ALPSGlidePoint::interruptOccurred( UInt8 data )
{
    //
    // This will be invoked automatically from our device when asynchronous
    // events need to be delivered. Process the trackpad data. Do NOT issue
    // any BLOCKING commands to our device in this context.
    //
    // Ignore all bytes until we see the start of a packet, otherwise the
    // packets may get out of sequence and things will get very confusing.
    //
    if (_packetByteCount == 0 && ((data == kSC_Acknowledge) || !(data & 0x08)))
    {
        return;
    }

    //
    // Add this byte to the packet buffer. If the packet is complete, that is,
    // we have the three bytes, dispatch this packet for processing.
    //

    _packetBuffer[_packetByteCount++] = data;
    
    if (_packetByteCount == 3)
    {
        dispatchRelativePointerEventWithPacket(_packetBuffer, 3);
        _packetByteCount = 0;
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ALPSGlidePoint::
     dispatchRelativePointerEventWithPacket( UInt8 * packet,
                                             UInt32  packetSize )
{
    //
    // Process the three byte relative format packet that was retreived from the
    // trackpad. The format of the bytes is as follows:
    //
    //  7  6  5  4  3  2  1  0
    // -----------------------
    // YO XO YS XS  1  M  R  L
    // X7 X6 X5 X4 X3 X3 X1 X0  (X delta)
    // Y7 Y6 Y5 Y4 Y3 Y2 Y1 Y0  (Y delta)
    //

    UInt32       buttons = 0;
    SInt32       dx, dy;
    AbsoluteTime now;

    if ( (packet[0] & 0x1) ) buttons |= 0x1;  // left button   (bit 0 in packet)
    if ( (packet[0] & 0x2) ) buttons |= 0x2;  // right button  (bit 1 in packet)
    if ( (packet[0] & 0x4) ) buttons |= 0x4;  // middle button (bit 2 in packet)
    
    dx = ((packet[0] & 0x10) ? 0xffffff00 : 0 ) | packet[1];
    dy = -(((packet[0] & 0x20) ? 0xffffff00 : 0 ) | packet[2]);

    clock_get_uptime(&now);
    
    dispatchRelativePointerEvent(dx, dy, buttons, now);
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ALPSGlidePoint::setTapEnable( bool enable )
{
    //
    // Instructs the trackpad to honor or ignore tapping
    //
	bool success;
    PS2Request * request = _device->allocateRequest();
    if ( !request ) return;

    request->commands[0].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[0].inOrOut  = kDP_SetMouseScaling2To1;
    request->commands[1].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[1].inOrOut  = kDP_SetMouseScaling2To1;
    request->commands[2].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[2].inOrOut  = kDP_SetMouseScaling2To1;
    request->commands[3].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[3].inOrOut  = kDP_SetDefaultsAndDisable;
    request->commands[4].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[4].inOrOut  = kDP_GetMouseInformation;
    request->commands[5].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[5].inOrOut  = kDP_SetDefaultsAndDisable;
    request->commands[6].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[6].inOrOut  = kDP_SetDefaultsAndDisable;

    if (enable)
	{
		request->commands[7].command  = kPS2C_SendMouseCommandAndCompareAck;
    	request->commands[7].inOrOut  = kDP_SetMouseSampleRate;
		request->commands[8].command  = kPS2C_SendMouseCommandAndCompareAck;
    	request->commands[8].inOrOut  = 0x0A;	// 1010 somehow enables tapping
	}
	else
	{
		request->commands[7].command  = kPS2C_SendMouseCommandAndCompareAck;
    	request->commands[7].inOrOut  = kDP_SetMouseResolution;
		request->commands[8].command  = kPS2C_SendMouseCommandAndCompareAck;
    	request->commands[8].inOrOut  = 0x00;
	}

    request->commands[9].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[9].inOrOut  = kDP_SetMouseScaling1To1;
    request->commands[10].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[10].inOrOut  = kDP_SetMouseScaling1To1;
    request->commands[11].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[11].inOrOut  = kDP_SetMouseScaling1To1;
    request->commands[12].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[12].inOrOut  = kDP_SetDefaultsAndDisable;
    request->commands[13].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[13].inOrOut  = kDP_Enable;

    request->commandsCount = 14;
    // _device->submitRequest(request); // asynchronous, auto-free'd
	_device->submitRequestAndBlock(request);

    success = (request->commandsCount == 14);
	if (success)
	{
		setSampleRateAndResolution();
	}

    _device->freeRequest(request);

}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void ApplePS2ALPSGlidePoint::setTouchPadEnable( bool enable )
{
    //
    // Instructs the trackpad to start or stop the reporting of data packets.
    // It is safe to issue this request from the interrupt/completion context.
    //

    PS2Request * request = _device->allocateRequest();
    if ( !request ) return;

    // (mouse enable/disable command)
    request->commands[0].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[0].inOrOut = kDP_SetDefaultsAndDisable;
    request->commands[1].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[1].inOrOut = kDP_SetDefaultsAndDisable;
    request->commands[2].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[2].inOrOut = kDP_SetDefaultsAndDisable;
    request->commands[3].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[3].inOrOut = kDP_SetDefaultsAndDisable;

	// (mouse or pad enable/disable command)
    request->commands[4].command = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[4].inOrOut = (enable)?kDP_Enable:kDP_SetDefaultsAndDisable;
    request->commandsCount = 5;
    _device->submitRequest(request); // asynchronous, auto-free'd
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ALPSGlidePoint::setSampleRateAndResolution( void )
{
	// Following the Synaptics specification, but assuming everywhere it
	// says "Syanptics doesn't do this" implies "ALPS should do this"
	// We set the sample rate to 100, and resolution to 4 counts per mm
	// This should match the Synaptics behavior
    PS2Request * request = _device->allocateRequest();
    if ( !request ) return;

	// From 3.4 of the Synaptics TouchPad Interfacing Guide
    request->commands[0].command  = kPS2C_SendMouseCommandAndCompareAck;
    request->commands[0].inOrOut  = kDP_SetDefaultsAndDisable;
	request->commands[1].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[1].inOrOut = kDP_SetMouseSampleRate;
	request->commands[2].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[2].inOrOut = 100;
	request->commands[3].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[3].inOrOut = kDP_SetMouseResolution;
	request->commands[4].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[4].inOrOut = 2;   // 0x02 = 4 counts per mm
	request->commands[5].command = kPS2C_SendMouseCommandAndCompareAck;
	request->commands[5].inOrOut = kDP_Enable;
	request->commandsCount = 6;
	_device->submitRequestAndBlock(request);

    _device->freeRequest(request);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ALPSGlidePoint::setCommandByte( UInt8 setBits, UInt8 clearBits )
{
    //
    // Sets the bits setBits and clears the bits clearBits "atomically" in the
    // controller's Command Byte.   Since the controller does not provide such
    // a read-modify-write primitive, we resort to a test-and-set try loop.
    //
    // Do NOT issue this request from the interrupt/completion context.
    //

    UInt8        commandByte;
    UInt8        commandByteNew;
    PS2Request * request = _device->allocateRequest();

    if ( !request ) return;

    do
    {
        // (read command byte)
        request->commands[0].command = kPS2C_WriteCommandPort;
        request->commands[0].inOrOut = kCP_GetCommandByte;
        request->commands[1].command = kPS2C_ReadDataPort;
        request->commands[1].inOrOut = 0;
        request->commandsCount = 2;
        _device->submitRequestAndBlock(request);

        //
        // Modify the command byte as requested by caller.
        //

        commandByte    = request->commands[1].inOrOut;
        commandByteNew = (commandByte | setBits) & (~clearBits);

        // ("test-and-set" command byte)
        request->commands[0].command = kPS2C_WriteCommandPort;
        request->commands[0].inOrOut = kCP_GetCommandByte;
        request->commands[1].command = kPS2C_ReadDataPortAndCompare;
        request->commands[1].inOrOut = commandByte;
        request->commands[2].command = kPS2C_WriteCommandPort;
        request->commands[2].inOrOut = kCP_SetCommandByte;
        request->commands[3].command = kPS2C_WriteDataPort;
        request->commands[3].inOrOut = commandByteNew;
        request->commandsCount = 4;
        _device->submitRequestAndBlock(request);

        //
        // Repeat this loop if last command failed, that is, if the
        // old command byte was modified since we first read it.
        //

    } while (request->commandsCount != 4);  

    _device->freeRequest(request);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

IOReturn ApplePS2ALPSGlidePoint::setParamProperties( OSDictionary * dict )
{
    OSNumber * clicking = OSDynamicCast( OSNumber, dict->getObject("Clicking") );

    if ( clicking )
    {    
        UInt8  newModeByteValue = clicking->unsigned32BitValue() & 0x1 ?
                                  kTapEnabled :
                                  0;

        if (_touchPadModeByte != newModeByteValue)
        {
            _touchPadModeByte = newModeByteValue;

            //
            // Write the TouchPad mode byte value.
            //

            setTapEnable(_touchPadModeByte);

            //
            // Advertise the current state of the tapping feature.
            //

            setProperty("Clicking", clicking);
        }
    }

    return super::setParamProperties(dict);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void ApplePS2ALPSGlidePoint::setDevicePowerState( UInt32 whatToDo )
{
    switch ( whatToDo )
    {
        case kPS2C_DisableDevice:
            
            //
            // Disable touchpad.
            //

            setTouchPadEnable( false );
            break;

        case kPS2C_EnableDevice:

            setTapEnable( _touchPadModeByte );


            //
            // Enable the mouse clock (should already be so) and the
            // mouse IRQ line.
            //

            setCommandByte( kCB_EnableMouseIRQ, kCB_DisableMouseClock );

            //
            // Finally, we enable the trackpad itself, so that it may
            // start reporting asynchronous events.
            //

            setTouchPadEnable( true );
            break;
	}
}
