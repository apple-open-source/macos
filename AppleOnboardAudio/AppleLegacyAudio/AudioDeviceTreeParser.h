/*
 *  AudioDeviceTreeParser.h
 *  Apple02Audio
 *
 *  Created by lcerveau on Thu May 31 2001.
 *  Copyright (c) 2001 __CompanyName__. All rights reserved.
 *
 */

#ifndef _AUDIODEVICETREEPARSER_H
#define _AUDIODEVICETREEPARSER_H

#include "AudioHardwareCommon.h"
#include "AudioHardwareUtilities.h"

class AudioDeviceTreeParser : public OSObject {
    OSDeclareDefaultStructors(AudioDeviceTreeParser);

public :
        
    static AudioDeviceTreeParser *createWithEntryProvider(IOService *provider); 
    
    UInt32 getNumberOfOutputs();
    UInt32 getNumberOfInputs();
    UInt32 getNumberOfDetects();
    UInt32 getNumberOfFeatures();
    UInt32 getNumberofInputsWithMuxes();
    UInt32 getLayoutID();
    UInt32 getInternalMicGainOffset();     	// aml 4.26.02
    bool getHasHWInputGain();     			// aml 4.29.02
    
    SInt16 getPowerObjectType();
    bool getPhaseInversion();
    SInt16 getInitOperationType();
    OSArray *getDSPFeatures(); 
    OSArray *createOutputsArray(); 
    OSArray *createInputsArray(); 
    OSArray *createInputsArrayWithMuxes();
    OSArray *createDetectsArray();     
    
protected:
    IORegistryEntry *soundEntry;
    bool init(IOService *provider);
    void free();
    
	// Utilities to parse the registry
    UInt8 convertAsciiToHexData(char ascii);
    UInt32 getStringAsNumber(char *string);
    bool getStringAsHexData( char *string, UInt8 *value, UInt32 *size );

};

#endif
