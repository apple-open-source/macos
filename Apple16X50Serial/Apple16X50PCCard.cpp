/*
Copyright (c) 1997-2002 Apple Computer, Inc. All rights reserved.
Copyright (c) 1994-1996 NeXT Software, Inc.  All rights reserved.
 
IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. (“Apple”) in consideration of your agreement to the following terms, and your use, installation, modification or redistribution of this Apple software constitutes acceptance of these terms.  If you do not agree with these terms, please do not use, install, modify or redistribute this Apple software.

In consideration of your agreement to abide by the following terms, and subject to these terms, Apple grants you a personal, non-exclusive license, under Apple’s copyrights in this original Apple software (the “Apple Software”), to use, reproduce, modify and redistribute the Apple Software, with or without modifications, in source and/or binary forms; provided that if you redistribute the Apple Software in its entirety and without modifications, you must retain this notice and the following text and disclaimers in all such redistributions of the Apple Software.  Neither the name, trademarks, service marks or logos of Apple Computer, Inc. may be used to endorse or promote products derived from the Apple Software without specific prior written permission from Apple.  Except as expressly stated in this notice, no other rights or licenses, express or implied, are granted by Apple herein, including but not limited to any patent rights that may be infringed by your derivative works or by other works in which the Apple Software may be incorporated.

The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. 

IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * ApplePCI16X50Serial.cpp
 * This file contains the implementation of device driver for a 16650-family
 * serial device connected as a PCCard device.  This subclass provides only the routines
 * necessary to detect and initialize the hardware, as well as map the
 * device registers and field interrupts.  All other functions are handled
 * by the client-class "com_apple_driver_16X50UARTSync".
 * 
 * 2002-02-15	dreece	I/O Kit port, based on NeXT drvISASerialPort DriverKit driver.
 */

#include "Apple16X50PCCard.h"
#include <IOKit/serial/IOSerialKeys.h>

#define MAX_WINDOWS		((UInt32)10)
#define MAX_UARTS_PER_WINDOW	((UInt32)4)

#define WINDOW_OFFSET_TO_REFCON(win,off) ( (void *)( ((win)&0x07)|((off)&0x18) ) )
#define REFCON_TO_OFFSET(ref)		( (UInt32)( ((UInt32)ref) & 0x18 ) )
#define REFCON_TO_WINDOW(ref)		( (UInt32)( ((UInt32)ref) & 0x07 ) )

#define super Apple16X50BusInterface
OSDefineMetaClassAndStructors(com_apple_driver_16X50PCCard, com_apple_driver_16X50BusInterface)

IOService *Apple16X50PCCard::probe(IOService *provider, SInt32 *score)
{
    char buf[80];

    OffLine = true;
    Provider = OSDynamicCast(IOPCCard16Device, provider);
    if (!Provider) {
        IOLog ("Apple16X50PCCard: Attached to non-IOPCCard16Device provider!  Failing probe()\n");
        return NULL;
    }
    if (!super::probe(provider, score)) return NULL;

    OSNumber *socketNumber=OSDynamicCast(OSNumber, Provider->getProperty("SocketNumber"));
    if (socketNumber) {
        InterfaceInstance = socketNumber->unsigned32BitValue();
        sprintf(buf, "PCCard Socket=%d", (int)InterfaceInstance);
        setProperty(kLocationKey, buf);
    } else {
        InterfaceInstance = 0;
        setProperty(kLocationKey, "PCCard Socket");
    }
    Location = OSDynamicCast(OSString, getProperty(kLocationKey))->getCStringNoCopy();
    
    sprintf(buf, "Apple16X50PCCard%d", (int)InterfaceInstance);
    setName(buf);
    DEBUG_IOLog("%s::probe(%p)\n", Name, Provider);

    OSArray *propArray=OSDynamicCast(OSArray, Provider->getProperty(kIOPCCardVersionOneMatchKey));
    if (propArray) {
        OSString *str=OSDynamicCast(OSString, propArray->getObject(0)); //If present, this will be the vendor name
        if (str)
            IOLog("%s: Card Vendor is \"%s\"\n", Name, str->getCStringNoCopy());
        str=OSDynamicCast(OSString, propArray->getObject(1)); //If present, this will be the product name
        if (str) {
            InterfaceBaseName=str->getCStringNoCopy();  // this will be displayed in NetworkPrefs
            IOLog("%s: Card Name is \"%s\"\n", Name, InterfaceBaseName);
        }
    }
    
    return this;
}

bool Apple16X50PCCard::start(IOService *provider)
{
    if (!super::start(provider)) return false;
    DEBUG_IOLog("%s::start(%p)\n", Name, Provider);

    static const IOPMPowerState myPowerStates[ kIOPCCard16DevicePowerStateCount ] =
    {
        { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, 0, IOPMSoftSleep, IOPMSoftSleep, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 1, IOPMPowerOn, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
    };

    // initialize our PM superclass variables
    PMinit();
    // register as the controlling driver
    registerPowerDriver(this, (IOPMPowerState *)myPowerStates, kIOPCCard16DevicePowerStateCount);
    // add ourselves into the PM tree
    Provider->joinPMtree(this);
    // set current pm state
    changePowerStateTo(kIOPCCard16DeviceOnState);

    
    // Find and decompose all function extension tuples...
    client_handle_t handle = Provider->getCardServicesHandle();
    tuple_t tuple;
    cisparse_t parse;
    u_char buf[64];

    tuple.DesiredTuple = CISTPL_FUNCE;
    tuple.Attributes = 0;
    tuple.TupleData = buf;
    tuple.TupleDataMax = sizeof(buf);
    tuple.TupleOffset = 0;
    if (Provider->cardServices(GetFirstTuple, (void*)handle, (void*)&tuple) == CS_SUCCESS) do {
        if (Provider->cardServices(GetTupleData, (void*)handle, (void*)&tuple) != CS_SUCCESS)
            continue;
        if (Provider->cardServices(ParseTuple, (void*)handle, (void*)&tuple, (void*)&parse) != CS_SUCCESS)
            continue;
        switch (parse.funce.type&0xf) {
            case CISTPL_FUNCE_SERIAL_IF	:
            case CISTPL_FUNCE_SERIAL_CAP :
                DEBUG_IOLog("%s: Serial Port ", Name);
                break;

            case CISTPL_FUNCE_SERIAL_IF_DATA :
            case CISTPL_FUNCE_SERIAL_CAP_DATA :
            case CISTPL_FUNCE_SERIAL_SERV_DATA :
                DEBUG_IOLog("%s: Data Modem ", Name);
                Modem = true;
                break;

            case CISTPL_FUNCE_SERIAL_IF_FAX :
            case CISTPL_FUNCE_SERIAL_CAP_FAX :
            case CISTPL_FUNCE_SERIAL_SERV_FAX :
                DEBUG_IOLog("%s: Fax Modem ", Name);
                break;

            case CISTPL_FUNCE_SERIAL_IF_VOICE :
            case CISTPL_FUNCE_SERIAL_CAP_VOICE :
            case CISTPL_FUNCE_SERIAL_SERV_VOICE :
                DEBUG_IOLog("%s: Voice Modem ", Name);
                break;

            default :
                DEBUG_IOLog("%s: Unknown(%d) ", Name, parse.funce.type&0xf);
                break;
        }
#ifdef DEBUG
        switch (parse.funce.type&0xf) {
            case CISTPL_FUNCE_SERIAL_CAP :
            case CISTPL_FUNCE_SERIAL_CAP_DATA :
            case CISTPL_FUNCE_SERIAL_CAP_FAX :
            case CISTPL_FUNCE_SERIAL_CAP_VOICE : {
                cistpl_modem_cap_t *z = (cistpl_modem_cap_t*)parse.funce.data;
                IOLog("Capabilities: ");
                IOLogByte((z->flow),"flow",0,0,0,"Trans","RTS","CTS","RXO","TXO");
                IOLog(" cmd_buf=%d rx_buf=%d tx_buf=%d\n", (z->cmd_buf+1)*4,
                      (z->rcv_buf_0 + (z->rcv_buf_1<<8) + (z->rcv_buf_2<<16)),
                      (z->xmit_buf_0 + (z->xmit_buf_1<<8) + (z->xmit_buf_2<<16))
                      );
                break; }

            case CISTPL_FUNCE_SERIAL_IF	:
            case CISTPL_FUNCE_SERIAL_IF_DATA :
            case CISTPL_FUNCE_SERIAL_IF_FAX :
            case CISTPL_FUNCE_SERIAL_IF_VOICE : {
                cistpl_serial_t *z = (cistpl_serial_t*) &(parse.funce.data);
                IOLog("Interface: uart=");
                switch (z->uart_type) {
                    case CISTPL_SERIAL_UART_8250  : IOLog("8250");	break;
                    case CISTPL_SERIAL_UART_16450 : IOLog("16450");	break;
                    case CISTPL_SERIAL_UART_16550 : IOLog("16550");	break;
                    case CISTPL_SERIAL_UART_8251  : IOLog("8251");	break;
                    case CISTPL_SERIAL_UART_8530  : IOLog("8530");	break;
                    case CISTPL_SERIAL_UART_85230 : IOLog("85230");	break;
                    default : IOLog("Unknown(%d)", z->uart_type);	break;
                }
                IOLogByte((z->uart_cap_0)," par",0,0,0,0,"E","O","M","S");
                IOLogByte((z->uart_cap_1)," bits",0,"2","M","1","8","7","6","5");
                IO_putc('\n');
                break; }

            case CISTPL_FUNCE_SERIAL_SERV_DATA : {
                cistpl_data_serv_t *z = (cistpl_data_serv_t*) parse.funce.data;
                IOLog("Services: baud=%d", (z->max_data_1 + ((z->max_data_0&0x7)<<8))*75);
                IOLogByte((z->modulation_0)," mod0","V.26bis","V.26","V.22bis","B212A","V.22A&B","V.23","V.21","B103");
                IOLogByte((z->modulation_1)," mod1",0,0,0,"V.27bis","V.29","V.32","V.32bis","V.34");
                IOLogByte((z->error_control)," edac",0,0,0,0,0,0,"V.42/LAPM","MNP2-4");
                IOLogByte((z->compression)," comp",0,0,0,0,0,0,"MNP-5","V.42bis");
                IOLogByte((z->cmd_protocol)," cmd",0,"DMCL","V.25A","V.25bis","MNP-AT","AT3","AT2","AT1");
                IOLogByte((z->escape)," esc",0,0,0,0,0,"prog","+++","BRK");
                IOLogByte((z->encrypt)," crypto",0,0,0,0,0,0,0,0);
                IOLogByte((z->misc_features)," misc",0,0,0,0,0,0,0,"CID");
                IO_putc('\n');
                break; }

            case CISTPL_FUNCE_SERIAL_SERV_FAX : {
                cistpl_fax_serv_t *z = (cistpl_fax_serv_t*) parse.funce.data;
                IOLog("Services: Class=%d baud=%d", parse.funce.type>>4, (z->max_data_1 + ((z->max_data_0&0x7)<<8))*75);
                IOLogByte((z->modulation)," mod",0,0,0,"V.33","V.17","V.29","V.27ter","V.21-C2");
                IOLogByte((z->encrypt)," crypto",0,0,0,0,0,0,0,0);
                IOLogByte((z->features_0)," features0","Passwd","FileTran","Poll","Vreq","ECM","T.6","T.4","T.3");
                IOLogByte((z->features_1)," features1",0,0,0,0,0,0,0,0);
                IO_putc('\n');
                break; }

            default :
                IO_putc('\n');
                break;
        }
#endif
    } while (Provider->cardServices(GetNextTuple, (void*)handle, (void*)&tuple) == CS_SUCCESS);

    if (!Provider->configure()) goto fail;
    WindowCount = min(Provider->getWindowCount(),10);
    if (WindowCount < 1) goto fail;

    if (Map) delete [] Map;
    Map = new (IOMemoryMap*)[WindowCount];
    if (!Map) goto fail;

    UARTInstance=0;
    OffLine = Stopped = false;
    for (unsigned win=0; win < WindowCount; win++) {
        UInt32 attributes, width=0;
        if (Provider->getWindowType(win) != IOPCCARD16_IO_WINDOW)
            continue;
        if (Provider->getWindowAttributes(win, &attributes))
            width = (attributes & IO_DATA_PATH_WIDTH == IO_DATA_PATH_WIDTH_8) ? 8 : 16;
        Map[win] = Provider->mapDeviceMemoryWithIndex(win);
        if (!Map[win]) continue; // try the next map
        DEBUG_IOLog("%s::start() IO window=%d physical=%p virtual=%p length=%d width=%d, attributes=%p\n",
                    Name, win, (void*)Map[win]->getPhysicalAddress(), (void*)Map[win]->getVirtualAddress(),
                    (int)Map[win]->getLength(), (int)width, (void*)attributes);
        if (Map[win] && (Map[win]->getLength()==kREG_Size) && (probeUART(WINDOW_OFFSET_TO_REFCON(win,0))))
            break; // found an UART, lets exit
        else
            RELEASE(Map[win]);
    }
    if (!UARTInstance) goto fail;
    

    if (Modem) {
        setProperty(kIOTTYBaseNameKey, "pccard-modem");
        if (!InterfaceBaseName)
            InterfaceBaseName="PCCard Modem"; // this will be displayed in NetworkPrefs
        IOLog("%s: Identified Modem in %s\n", Name, Location);
    } else {
        setProperty(kIOTTYBaseNameKey, "pccard-serial");
        if (!InterfaceBaseName)
            InterfaceBaseName="PCCard Serial Adapter"; // this will be displayed in NetworkPrefs
        IOLog("%s: Identified Serial Port in %s\n", Name, Location);
    }
    startUARTs();
    return true;

fail:
    stop(Provider);
    return false;
}

void Apple16X50PCCard::stop(IOService *provider)
{
    DEBUG_IOLog("%s::stop(%p)\n", Name, Provider);
    PMstop();	// take ourselves out of PM tree
    goOffLine();
    Stopped=true;
    // unmap the windows
    super::stop(provider);
    if (Map) {
        for (UInt32 win=0; win<WindowCount; win++)
            RELEASE(Map[win]);
        delete [] Map;
        Map=NULL;
    }
    Provider->unconfigure();	// release this device's enabler
}

void Apple16X50PCCard::free()
{
    DEBUG_IOLog("%s::free()\n", Name);
    super::free();
}

Apple16X50UARTSync *Apple16X50PCCard::
probeUART(void* refCon, Apple16X50UARTSync *uart, OSDictionary *properties)
{
    char buf[80];

    uart = super::probeUART(refCon, uart, properties);
    if (!uart) return false;

    sprintf(buf, "%s Window=%d Offset=%d", Location, (int)REFCON_TO_WINDOW(refCon), (int)REFCON_TO_OFFSET(refCon));
    uart->setProperty(kLocationKey, buf);

    return uart;
}

void Apple16X50PCCard::goOnLine()
{
    DEBUG_IOLog("%s::goOnLine() Stopped=%s OffLine=%s\n", Name, BOOLSTR(Stopped), BOOLSTR(OffLine));
    if (Stopped) { // if we are stopped, then we cannot go back online
        goOffLine();
        return;
    }
    if (!OffLine) return; // we already are on-line
    OffLine=false;
    
    UARTInstance=0;
    for (unsigned win=0; win < WindowCount; win++) {
        if (Map[win] && (Map[win]->getLength()==kREG_Size) && (probeUART(WINDOW_OFFSET_TO_REFCON(win,0))))
            break; // found an UART, lets exit
        else
            RELEASE(Map[win]);
    }
    startUARTs();
    if (InterruptSource) InterruptSource->enable();
}

void Apple16X50PCCard::goOffLine()
{
    DEBUG_IOLog("%s::goOffLine() Stopped=%s OffLine=%s\n", Name, BOOLSTR(Stopped), BOOLSTR(OffLine));
    
    if (OffLine) return; // we already are
    OffLine = true;
    
    if (InterruptSource) InterruptSource->disable();
    for (UInt32 i=0; i<MAX_UARTS; i++) {
        if (UART[i]) {
            UART[i]->terminate(kIOServiceRequired | kIOServiceTerminate | kIOServiceSynchronous);
            RELEASE(UART[i]);
        }
    }
}

UInt8 Apple16X50PCCard::getReg(UInt32 reg, void *refCon)
{
    register UInt8 val;
    if (OffLine) {
        val=0xff;
        DEBUG_IOLog("getReg(%d) while offline!\n", (int)reg);
    } else
        val = Provider->ioRead8(reg+REFCON_TO_OFFSET(refCon), Map[REFCON_TO_WINDOW(refCon)]);
    return (val);
}

void Apple16X50PCCard::setReg(UInt32 reg, UInt8 val, void *refCon)
{
    if (!OffLine)
        Provider->ioWrite8(reg+REFCON_TO_OFFSET(refCon), val, Map[REFCON_TO_WINDOW(refCon)]);
#ifdef DEBUG
    else
        DEBUG_IOLog("setReg(%d,%d) while offline!\n", (int)reg, val);
#endif

}

int Apple16X50PCCard::
setPowerState(unsigned long powerState, IOService *whatDevice)
{
    switch (powerState) {

    case kIOPCCard16DeviceOnState:
    case kIOPCCard16DeviceDozeState:
        if (Stopped) {
            DEBUG_IOLog("%s::setPowerState() setting power state to on, but I'm stop()ped!!!\n", Name);
            break;
        }
        DEBUG_IOLog("%s::setPowerState() setting power state to on\n", Name);
        goOnLine();
        break;

    case kIOPCCard16DeviceOffState:
        DEBUG_IOLog("%s::setPowerState() setting power state to off\n", Name);
        goOffLine();
        break;
	
    default:
        DEBUG_IOLog("%s::setPowerState() unknown state=%d?\n", Name, (int)powerState);
	break;
    }

    return IOPMAckImplied;
}

IOReturn Apple16X50PCCard::
message(UInt32 type, IOService *sender, void *argument)
{
	 
    if (type != kIOPCCardCSEventMessage)
        return super::message(type, sender, argument);

    UInt32 cs_event = (UInt32) argument;

    switch (cs_event) {
        case CS_EVENT_EJECTION_REQUEST:
            DEBUG_IOLog("%s::message() Card Eject Requested\n", Name);
        case CS_EVENT_CARD_REMOVAL:
            DEBUG_IOLog("%s::message() Card Removed\n", Name);
            Stopped=true;
            goOffLine();
        default:
            break;
    }

    return kIOReturnSuccess;
}

