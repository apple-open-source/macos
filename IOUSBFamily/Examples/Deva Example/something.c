/*
* © Copyright 2002 Apple Computer, Inc. All rights reserved.
*
* IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. (“Apple”) in 
* consideration of your agreement to the following terms, and your use, installation, 
* modification or redistribution of this Apple software constitutes acceptance of these
* terms.  If you do not agree with these terms, please do not use, install, modify or 
* redistribute this Apple software.
*
* In consideration of your agreement to abide by the following terms, and subject to these 
* terms, Apple grants you a personal, non exclusive license, under Apple’s copyrights in this 
* original Apple software (the “Apple Software”), to use, reproduce, modify and redistribute 
* the Apple Software, with or without modifications, in source and/or binary forms; provided 
* that if you redistribute the Apple Software in its entirety and without modifications, you 
* must retain this notice and the following text and disclaimers in all such redistributions 
* of the Apple Software.  Neither the name, trademarks, service marks or logos of Apple 
* Computer, Inc. may be used to endorse or promote products derived from the Apple Software 
* without specific prior written permission from Apple. Except as expressly stated in this 
* notice, no other rights or licenses, express or implied, are granted by Apple herein, 
* including but not limited to any patent rights that may be infringed by your derivative 
* works or by other works in which the Apple Software may be incorporated.
* 
* The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, 
* EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-
* INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE 
* SOFTWARE OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. 
*
* IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL 
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS 
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, 
* REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND 
* WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR 
* OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>

#include <IOKit/usb/IOUSBLib.h>
#include "something.h"
#include "deva.h"
#include "printInterpretedError.h"

#include <unistd.h>

static Boolean quitFlag = false;	// Set by the interrupt routine to stop us.

void stopDoingSomething(void)	// Call this to stop the action.
{
    quitFlag = true;
}

void finallyDoSomethingWithThisDevice(IOUSBInterfaceInterface **intf)
{
IOReturn err;
UInt32 portBits, IOBits, IOBits1;

    // First set all ports to input
    
    err = DevaSetIoPortsConfig(intf, 0x000FFFFF);
    
    if (kIOReturnSuccess != err)
    {
        printInterpretedError("unable to do bulk write", err);
        return;
    }

    // Next read IO bits, switches which are on should be 1 everything else 0

    err = DevaReadIoPorts(intf, &IOBits);
    
    if (kIOReturnSuccess != err)
    {
        printInterpretedError("unable to do ReadIoPorts", err);
        return;
    }
    
    printf("Port IO read gives: %08lx\n", IOBits);

    // Now set all free bits output.
    
    err = DevaSetIoPortsConfig(intf, IOBits);
    if (kIOReturnSuccess != err) 
    {
        printInterpretedError("unable to do SetIoPortsConfig", err);
        return;
    }

    
    while(1)
    {
    // Now set all free bits to 1.

        err = DevaWriteIoPorts(intf, 0x000FFFFF, 0x000FFFFF);

        if(kIOReturnSuccess != err)
        {
            printInterpretedError("unable to do WriteIoPorts", err);
            return;
        }

//sleep(9);
        sleep(1);
        portBits = 0;
    //      IOBits1 = IOBits;
        
        do{
            IOBits1 = IOBits;
            do{
                portBits <<= 1;
                if((portBits & 0x000FFFFF)== 0)
                {
                    portBits = 1;
                }
            }while((portBits & ~IOBits) == 0);

            err = DevaWriteIoPorts(intf, portBits, 0x000FFFFF);
    
            if(kIOReturnSuccess != err)
            {
                printInterpretedError("unable to write walking bit", err);
                return;
            }

//              sleep(1);
            usleep(100 * 1000);

            err = DevaWriteIoPorts(intf, 0, 0x000FFFFF);
    
            if(kIOReturnSuccess != err)
            {
                printInterpretedError("unable to write zero bits", err);
                return;
            }

        // set all to input
            err = DevaSetIoPortsConfig(intf, 0x000FFFFF);
            
            if (kIOReturnSuccess != err)
            {
                printInterpretedError("unable to do bulk write", err);
                return;
            }
        
            if(quitFlag) return;	// We've been interupted
        
            // Next read IO bits, switches which are on should be 1 everything else 0
    
            err = DevaReadIoPorts(intf, &IOBits);
            
            if (kIOReturnSuccess != err)
            {
                printInterpretedError("unable to do ReadIoPorts", err);
                return;
            }            
//     printf("Port IO read 2 gives: %08lx (old %08lx)\n", IOBits1, IOBits);
            // Now set all free bits output.
            
            err = DevaSetIoPortsConfig(intf, IOBits);
            if (kIOReturnSuccess != err) 
            {
                printInterpretedError("unable to do SetIoPortsConfig", err);
                return;
            }

        } while(IOBits1 == IOBits);   
    }


}
