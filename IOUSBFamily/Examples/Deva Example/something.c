/*
 * Copyright © 2003-2012 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
/*
* © Copyright 2002 Apple Inc.  All rights reserved.
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


#define PUMPKIN 0

UInt32 lights[]={
    1 << 16,
    1 << 17,
    1 << 18,
    1 << 19,
    1 << 8,
    1 << 9,
    1 << 10,
    1 << 11,
    
    1 << 12,
    1 << 13,
    1 << 14,
    1 << 15,
    
    1 << 0,
    1 << 1,
    1 << 2,
    1 << 3,
    1 << 4,
    1 << 5,
    1 << 6,
    1 << 7
    };

#if PUMPKIN

// This stuff supports the light sequencing in the USB Pumpkin
// entered into the halloween pumpkin carving competition.


#define l0 (1<<0)
#define l1 (1<<1)
#define l2 (1<<2)
#define l3 (1<<3)
#define l4 (1<<4)
#define l5 (1<<5)
#define l6 (1<<6)
#define l7 (1<<7)
#define l8 (1<<8)
#define l9 (1<<9)
#define l10 (1<<10)
#define l11 (1<<11)
#define l12 (1<<12)
#define l13 (1<<13)
#define l14 (1<<14)
#define l15 (1<<15)
#define l16 (1<<16)
#define l17 (1<<17)
#define l18 (1<<18)
#define l19 (1<<19)

#define LUEye l4
#define LLEye l5
#define RLEye l6
#define RUEye l7
#define RMouth l8
#define LMouth l9
#define RBeard l10
#define LBeard l11
#define Triangle l12
#define Circle l13
#define Square l14
#define LEye l18
#define REye l19



typedef struct{
    UInt32 time;
    UInt32 state;
    }lightState;


#define dwellTenths 1

UInt32 constState = Triangle+Circle+Square;

lightState states[]={
    {10, 0},
    {2, LEye+REye},
    {1, 0},
    {3, LEye+REye},
    {1, 0},
    {5, LEye+REye},
    {10, LEye+REye+RMouth+LMouth},
    {2, LEye+REye+LMouth},
    {2, LEye+REye},
    {20, LEye+REye+LUEye+LLEye+RLEye+RUEye},
    {20, LEye+REye+LUEye+LLEye+RLEye+RUEye+RMouth+LMouth},
    {5, LEye+REye+LUEye+LLEye+RLEye+RUEye},
//    {2, LEye+REye+LUEye+LLEye+RLEye+RUEye+RMouth+LMouth},
    {1, LEye+REye+LUEye+LLEye+RLEye+RUEye+RMouth},
    {1, LEye+REye+LUEye+LLEye+RLEye+RUEye+RMouth+LBeard},
    {1, LEye+REye+LUEye+LLEye+RLEye+RUEye+RMouth+RBeard+LBeard},
    {20, LEye+REye+LUEye+LLEye+RLEye+RUEye+RMouth+LMouth+RBeard+LBeard},
    {1, LEye+REye+LUEye+LLEye+RLEye+RUEye+RMouth+LMouth+RBeard+LBeard},

#if 0
    {1, LUEye},
    {1, LLEye},
    {1, RLEye},
    {1, RUEye},
    {1, RMouth},
    {1, LMouth},
    {1, RBeard},
    {1, LBeard},
    {1, Triangle},
    {1, Circle},
    {1, Square},
    {1, LEye},
    {1, REye},
#endif
    };

#define numStates (sizeof(states)/sizeof(lightState))

#endif


void finallyDoSomethingWithThisDevice(IOUSBInterfaceInterface245 **intf)
{
#if PUMPKIN
IOReturn err;
UInt32 IOBits, currState;
UInt32 state, bit;
int i;
void *hBits;
    // Now set all bits output.

    err = DevaSetIoPortsConfig(intf, 0);
    if (kIOReturnSuccess != err)
    {
        printInterpretedError("unable to do SetIoPortsConfig", err);
        return;
    }

    err = DevaWriteIoPorts(intf, 0x000FFFFF, 0x000FFFFF);

    if(kIOReturnSuccess != err)
    {
        printInterpretedError("unable to do WriteIoPorts", err);
        return;
    }

	usleep(100 * 1000);
    
    state = 0;

    while(!quitFlag)
    {
	bit = 1;
	IOBits = 0;
    currState = states[state].state +constState;
	for(i= 0; i<20; i++)
	{
	    if((currState & bit) != 0)
	    {
		IOBits |= lights[i];
	    }
	    bit <<= 1;
	}
    hBits = (void *)IOBits;
	err = DevaWriteIoPorts(intf, IOBits, 0x000FFFFF);

	if(kIOReturnSuccess != err)
	{
	    printInterpretedError("unable to write walking bit", err);
	    return;
	}

	usleep(states[state].time*100 * 1000*dwellTenths);

	if(++state >= numStates)
	{
	    state = 0;
	}

    }
#else
IOReturn err;
UInt32 portBits, IOBits, IOBits1;
UInt32 light, light2;
Boolean dirn;
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
        light = 0;
        light2 = 8;
        dirn= false;
    //      IOBits1 = IOBits;

        do{
            IOBits1 = IOBits;

#if 0
            do{
                portBits <<= 1;
             //   portBits2 >>= 1;
                if((portBits & 0x000FFFFF)== 0)
                {
                    portBits = 1;
                //    portBits2 = (0x000FFFFF+1)/2;
              }
            }while((portBits & ~IOBits) == 0);
#endif
            if( (light >= 7) || (light <= 0) )
            {
               // light = 0;
               // light2 = 19;
               dirn = !dirn;
            }
            light2++;
            if(light2 > 19)
            {
                light2 = 8;
            }
            portBits = lights[light] | lights[light2];
            if(dirn)
            {
                light++;
            }
            else
            {
                light--;
            }

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
#endif

}
