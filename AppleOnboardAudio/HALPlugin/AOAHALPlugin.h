/*
 *  AOAHALPlugin.h
 *  AppleOnboardAudio
 *
 *  Created by cerveau on Mon May 28 2001.
 *  Copyright (c) 2001 __CompanyName__. All rights reserved.
 *
 */

    //set of properties and explanation

/* In order to fully use the capabilities of the hardware there is a need for extra
properties : we will define the following enhancement
    - usage mode : when on this allows better management of the devices
        This is a boolean with 0 = kAOAUsageModeStandard, and 1 = kAOAUsageModeExpert
    - Boot beep control : when in expert mode this allow separate control of the boot 
      beep : volume between 0 or 7. Otherwise boot beep level is done by the adjustement
      of volume an mute
    - iSub : this allow the detection and control of an iSub : it corresponds to a 
      structure boolean : present or not/ float 0..1 volume (when active)
    - Physical outputs : allow to get the number of physical outputs for separate
      control . Each physical output is describing itself by different characteristics
        - relative volume : this is between 0 and 1 : it is then translated to 
         0 -255 to allow fine control of the 
        - absolute volume : this is equal to system volum * relative volume
        - mute state : tell if the input is mute or not
        - mute dependancy : index of output that is linked to that one : for 
        example 
        - speaker : gives either a generic speaker or if an Apple one allow 
        it
        - Output Effect : this is a dictionnary with Bass, Treble, EQ
          EQ is given as a structure of 
        - Every stuff is passed as CF data
    - Input effect

    - when the plugin loads the PLugin save Preferences with each of these values for a user
    
*/


#ifndef __AOAHALPLUGIN__
#define __AOAHALPLUGIN__

#include <CoreAudio/AudioDriverPlugIn.h>

//#include <CoreAudio/AudioDriverPlugin.h>
enum {
	kAOAPropertyHeadphoneExclusive				= 'hpex',
	kAOAPropertySelectionsReference				= 'selr',
	kAOAPropertyPowerState						= 'powr'
};

#endif
