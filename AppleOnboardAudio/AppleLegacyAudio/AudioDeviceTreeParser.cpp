/*
 *  AudioDeviceTreeParser.cpp
 *  Apple02Audio
 *
 *  Created by lcerveau on Thu May 31 2001.
 *  Copyright (c) 2001 __CompanyName__. All rights reserved.
 *
 */
 
#include "AudioHardwareCommon.h"
#include "AudioHardwareConstants.h"
#include "AudioDeviceTreeParser.h"
#include "AudioHardwareOutput.h"
#include "AudioHardwareDetect.h"
#include "AudioHardwarePower.h"

#define super OSObject

// aml 5.27.02
#define OVERRIDE_REGISTRY_ZERO_GAIN	1

OSDefineMetaClassAndStructors(AudioDeviceTreeParser, OSObject)


// Creation and destruction of the object
AudioDeviceTreeParser *AudioDeviceTreeParser::createWithEntryProvider(IOService *provider){
    AudioDeviceTreeParser *myAudioDTParser = 0;
    FailIf(!provider, EXIT);    
    
    myAudioDTParser = new AudioDeviceTreeParser;
    
    if(myAudioDTParser) {
        if(!(myAudioDTParser->init(provider))){
            myAudioDTParser->release();
            myAudioDTParser = 0;
        }            
    }

EXIT:
    return myAudioDTParser;
}
    
bool AudioDeviceTreeParser::init(IOService *provider){

    soundEntry = 0;
    if(!super::init())
        return(false);
    
    soundEntry = provider->childFromPath(kSoundEntryName, gIODTPlane);               
    if(!soundEntry) return false;
    
    soundEntry->retain();
    return(true);
}

void AudioDeviceTreeParser::free(){
    soundEntry->release(); 
    super::free();
}

// Information getter for outputs
UInt32 AudioDeviceTreeParser::getNumberOfOutputs(){
    OSData *tempData;
    UInt32 AudioOutputNb = 0;
    
    tempData = OSDynamicCast(OSData, soundEntry->getProperty(kNumOutputsPropName));
    FailIf(!tempData, BAIL);
    AudioOutputNb = *(UInt32 *)(tempData->getBytesNoCopy());
    FailIf(0 == AudioOutputNb,EXIT);

EXIT:
    return AudioOutputNb;
BAIL:
    AudioOutputNb = 0;
    goto EXIT;
}

OSArray *AudioDeviceTreeParser::createOutputsArray(){
    OSData *tempData;
    OSArray* AudioOutputs = 0;
    UInt32 AudioOutputNb;
    char * theSoundObjects, *thetempObject;
    int parser, startidx, stopidx, startwordidx, stopwordidx, length;
    AudioHardwareOutputInfo myInfo;
    AudioHardwareOutput *theOutput;
    int size;
         
	// get the output numbers and create the detects array
    AudioOutputNb = getNumberOfOutputs();
    FailIf(0 == AudioOutputNb,EXIT);

    AudioOutputs = OSArray::withCapacity(AudioOutputNb);
    FailIf(!AudioOutputs, BAIL);
    
	// get all the sound objects and do the parsing
    tempData = OSDynamicCast(OSData, soundEntry->getProperty(kSoundObjectsPropName));
    FailIf(!tempData, BAIL);

    theSoundObjects = (char *) tempData->getBytesNoCopy();
    size = (int) tempData->getLength();
    parser = -1;
    
    while(size && parser < size) {

        // find the end of string
        ASSIGNSTARTSTRING(startidx, parser);
        NEXTENDOFSTRING(theSoundObjects, parser);
        ASSIGNSTOPSTRING(stopidx, parser);
        thetempObject = theSoundObjects+startidx;
                        
		// do the detect parsing if it is one
        stopwordidx = 0;
        NEXTENDOFWORD(thetempObject, stopwordidx, size);
                        
        if(!bcmp(thetempObject, kOutputObjEntryName, stopwordidx-1)) {
            length = stopidx-startidx;
            startwordidx = ++stopwordidx;
            
            while(stopwordidx < length) {

                NEXTENDOFWORD(thetempObject, stopwordidx, size);
                
                if(!bcmp(thetempObject+startwordidx, kDeviceMaskPropName, stopwordidx-startwordidx)) {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx, size);
                    READWORDASNUMBER(myInfo.deviceMask, thetempObject, startwordidx, stopwordidx);
                } else if(!bcmp(thetempObject+startwordidx, kDeviceMatchPropName, stopwordidx-startwordidx)) {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx, size);
                    READWORDASNUMBER(myInfo.deviceMatch, thetempObject, startwordidx, stopwordidx);
                } else if(!bcmp(thetempObject+startwordidx, kIconIDPropName, stopwordidx-startwordidx)) {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx, size);    
                    READWORDASNUMBER(myInfo.iconID, thetempObject, startwordidx, stopwordidx);
                } else if(!bcmp(thetempObject+startwordidx, kPortConnectionPropName, stopwordidx-startwordidx)) {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx, size);    
                    READWORDASNUMBER(myInfo.portConnection, thetempObject, startwordidx, stopwordidx);
                } else if(!bcmp(thetempObject+startwordidx, kPortTypePropName, stopwordidx-startwordidx)) {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx, size); 
                    READWORDASNUMBER(myInfo.portType, thetempObject, startwordidx, stopwordidx);
                } else if(!bcmp(thetempObject+startwordidx, kNameIDPropName, stopwordidx-startwordidx)) {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx, size);    
                    READWORDASNUMBER(myInfo.nameID, thetempObject, startwordidx, stopwordidx);
                } else if(!bcmp(thetempObject+startwordidx, "param", stopwordidx-startwordidx)) {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx, size);    
                    READWORDASNUMBER(myInfo.param, thetempObject, startwordidx, stopwordidx);
                } else if(!bcmp(thetempObject+startwordidx, kModelPropName, stopwordidx-startwordidx)) {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx); 
                    NEXTENDOFWORD(thetempObject, stopwordidx, size);
                    
                    if(!bcmp(thetempObject+startwordidx, kOutputPortObjName, 8))
                        myInfo.outputKind = kOutputPortTypeClassic;
                    else if(!bcmp(thetempObject+startwordidx, kKiheiSpeakerObjName, 8))
                        myInfo.outputKind = kOutputPortTypeProj5;
                    else if(!bcmp(thetempObject+startwordidx, kWSIntSpeakerObjName, 8))
                        myInfo.outputKind = kOutputPortTypeProj3;
                    else if(!bcmp(thetempObject+startwordidx, kOutputEQPortObjName, 8))
                        myInfo.outputKind = kOutputPortTypeEQ;
                    else 
                        myInfo.outputKind = kOutputPortTypeUnknown;
                }
                ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);                                                
            }
                
            theOutput = AudioHardwareOutput::create(myInfo);
            AudioOutputs->setObject(theOutput);
            CLEAN_RELEASE(theOutput);
            myInfo.deviceMask =0; myInfo.deviceMatch =0;
            myInfo.iconID =0; myInfo.portConnection =0;
            myInfo.portType =0; myInfo.nameID =0;
            myInfo.outputKind = 0; myInfo.param = 0;
        }
    }              
    
EXIT:
    return AudioOutputs;
BAIL:
    goto EXIT;
}

UInt32 AudioDeviceTreeParser::getNumberOfInputs(){
    UInt32 result = 0;
    OSData *tempData;
    
    tempData = OSDynamicCast(OSData, soundEntry->getProperty(kNumInputsPropName));
    if(tempData)
		result = *(UInt32 *)(tempData->getBytesNoCopy());

    return result; 
}

UInt32 AudioDeviceTreeParser::getNumberofInputsWithMuxes(){
    UInt32 result;
    OSData *tempData;
    char * theSoundObjects, *thetempObject;
    int parser, startidx, stopidx, stopwordidx, size;
    
    result = 0;
    tempData = OSDynamicCast(OSData, soundEntry->getProperty(kSoundObjectsPropName));
    FailIf(!tempData, BAIL);

    theSoundObjects = (char *) tempData->getBytesNoCopy();
    size = (int) tempData->getLength();
    parser = -1;

    while(size && parser < size) {
        ASSIGNSTARTSTRING(startidx, parser);
        NEXTENDOFSTRING(theSoundObjects, parser);
        ASSIGNSTOPSTRING(stopidx, parser);
        thetempObject = theSoundObjects+startidx;
                        
		// do the detect parsing if it is one
        stopwordidx = 0;
        NEXTENDOFWORD(thetempObject, stopwordidx, size);
        if(!bcmp(thetempObject, kInputObjEntryName, stopwordidx-1)) 
            result++;
        else if( !bcmp(thetempObject, kMuxObjEntryName, stopwordidx-1))
            result++;
    }

EXIT:
    return result;
BAIL:
    result = 0;
    goto EXIT;
}

UInt32 AudioDeviceTreeParser::getNumberOfDetects(){
    UInt32 result = 0;
    OSData *tempData;
    
    tempData = OSDynamicCast(OSData, soundEntry->getProperty(kNumDetectsPropName));
    FailIf(!tempData, BAIL);
    result = *(UInt32 *)(tempData->getBytesNoCopy());
    FailIf(0 == result, BAIL);

EXIT:
    return result; 
BAIL:
    goto EXIT;
}

UInt32 AudioDeviceTreeParser::getNumberOfFeatures(){
    UInt32 result = 0;
 
    return result;
}

SInt16 AudioDeviceTreeParser::getInitOperationType(){
    SInt16 result = 0;
    OSData *tempData;
    char * theSoundObjects, *thetempObject;
    int parser, startidx, stopidx, startwordidx, stopwordidx, length;
    short opkind = 0;
    int size;
         
	// get all the sound objects and do the parsing
    tempData = OSDynamicCast(OSData, soundEntry->getProperty(kSoundObjectsPropName));
    FailIf(!tempData, BAIL);

    theSoundObjects = (char *) tempData->getBytesNoCopy();
	FailIf (NULL == theSoundObjects, BAIL);
    size = (int) tempData->getLength();
    parser = -1;
    
    while(size && parser < size) {

		// find the end of string
        ASSIGNSTARTSTRING(startidx, parser);
        NEXTENDOFSTRING(theSoundObjects, parser);
        ASSIGNSTOPSTRING(stopidx, parser);
        thetempObject = theSoundObjects+startidx;
                        
		// do the detect parsing if there is one
        stopwordidx = 0;
        NEXTENDOFWORD(thetempObject, stopwordidx, size);
                        
        if(!bcmp(thetempObject, "init", stopwordidx-1)) {
            length = stopidx-startidx;
            startwordidx = ++stopwordidx;
            
            while(stopwordidx < length) {
                NEXTENDOFWORD(thetempObject, stopwordidx, size);
                
                if(!bcmp(thetempObject+startwordidx, "operation", stopwordidx-startwordidx)) {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx, size);
                    READWORDASNUMBER(opkind, thetempObject, startwordidx, stopwordidx);
                } else if(!bcmp(thetempObject+startwordidx, "param", stopwordidx-startwordidx)) {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx, size);
                    thetempObject[stopwordidx] = '\0';
                    thetempObject[stopwordidx] = ' ';
                } else if(!bcmp(thetempObject+startwordidx, "param-size", stopwordidx-startwordidx)) {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx, size);    
                    thetempObject[stopwordidx] = '\0';
                    thetempObject[stopwordidx] = ' ';
                }                 
                ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);                                                 
            }
        }
    }              

    result = opkind;
EXIT:
    return result;
BAIL:
    result = 0;   
    goto EXIT;
}

// cheap and dirty way to get the phase inversion feature object.
// Do it this way until we parse all the features directly
bool AudioDeviceTreeParser::getPhaseInversion()
{
    bool result = false;
    OSData *tempData;
    char * theSoundObjects, *thetempObject;
    int parser, startidx, stopidx, startwordidx, stopwordidx, length;
    int size;
    
    // get all the sound objects and do the parsing
    tempData = OSDynamicCast(OSData, soundEntry->getProperty(kSoundObjectsPropName));
    FailIf(!tempData, BAIL);

    theSoundObjects = (char *) tempData->getBytesNoCopy();
    size = (int) tempData->getLength();
    parser = -1;
    
    while(size && parser < size) 
    {
		// find the end of string
        ASSIGNSTARTSTRING(startidx, parser);
        NEXTENDOFSTRING(theSoundObjects, parser);
        ASSIGNSTOPSTRING(stopidx, parser);
        thetempObject = theSoundObjects+startidx;
                        
		// do the feature  parsing if it is one
        stopwordidx = 0;
        NEXTENDOFWORD(thetempObject, stopwordidx, size);
                        
        if(!bcmp(thetempObject, "feature", stopwordidx-1)) 
        {
            length = stopidx-startidx;
            startwordidx = ++stopwordidx;
            
            while(stopwordidx < length) 
            {
                NEXTENDOFWORD(thetempObject, stopwordidx, size);
                
                if(!bcmp(thetempObject+startwordidx, "model", stopwordidx-startwordidx)) 
                {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx, size);
                    if(!bcmp(thetempObject+startwordidx, "PhaseInversion", 8)) 
                    {
                        result = true;
                        // we are done so lets get out of here
                        goto EXIT;
                    }
                 } 
                ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);                                                 
            }
                 
        }
    }              

EXIT: 
    return result;
BAIL:
    goto EXIT;
    result = 0;   
}

SInt16 AudioDeviceTreeParser::getPowerObjectType(){
    SInt16 result = 0;
    OSData *tempData;
    char * theSoundObjects, *thetempObject;
    int parser, startidx, stopidx, startwordidx, stopwordidx, length;
    int size;

	// get all the sound objects and do the parsing
    tempData = OSDynamicCast(OSData, soundEntry->getProperty(kSoundObjectsPropName));
    FailIf(!tempData, BAIL);

    theSoundObjects = (char *) tempData->getBytesNoCopy();
    size = (int) tempData->getLength();
    parser = -1;
    
    while(size && parser < size) {
		// find the end of string
        ASSIGNSTARTSTRING(startidx, parser);
        NEXTENDOFSTRING(theSoundObjects, parser);
        ASSIGNSTOPSTRING(stopidx, parser);
        thetempObject = theSoundObjects+startidx;
                        
		// do the feature  parsing if it is one
        stopwordidx = 0;
        NEXTENDOFWORD(thetempObject, stopwordidx, size);
                        
        if(!bcmp(thetempObject, "feature", stopwordidx-1)) {
            length = stopidx-startidx;
            startwordidx = ++stopwordidx;
            
            while(stopwordidx < length) {
                NEXTENDOFWORD(thetempObject, stopwordidx, size);
                
                if(!bcmp(thetempObject+startwordidx, "model", stopwordidx-startwordidx)) {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx, size);
                    if(!bcmp(thetempObject+startwordidx, "Proj7PowerControl", 8)) 
                        result= kProj7PowerObject;
                    else if (!bcmp(thetempObject+startwordidx, "Proj10PowerControl", 8)) 
                        result= kProj10PowerObject;
                    else if (!bcmp(thetempObject+startwordidx, "Proj8PowerControl", 8)) 
                        result= kProj8PowerObject;
                    else if (!bcmp(thetempObject+startwordidx, "Proj6PowerControl", 8)) 
                        result= kProj6PowerObject;
                    else if (!bcmp(thetempObject+startwordidx, "Proj14PowerControl", 8)) 
                        result= kProj14PowerObject;
                    else if (!bcmp(thetempObject+startwordidx, "Proj16PowerControl", 8)) 
                        result= kProj16PowerObject;
                 }
                ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);                                                 
            }
                 
        }
    }              

EXIT:
    return result;
BAIL:
    goto EXIT;
    result = 0;   
}

UInt32 AudioDeviceTreeParser::getLayoutID(){
    UInt32 result = 0;
    OSData *tempData;
    
    tempData = OSDynamicCast(OSData, soundEntry->getProperty(kDeviceIDPropName));
    FailIf(!tempData, BAIL);
    result = *(UInt32 *)(tempData->getBytesNoCopy());
    FailIf(0 == result, BAIL);

EXIT:
    return result; 
BAIL:
    goto EXIT;
}

OSArray *AudioDeviceTreeParser::getDSPFeatures(){
 
 return(0);
}

OSArray *AudioDeviceTreeParser::createInputsArray(){
	// this get the inputs, not used because handles everything.
	return(0);
}

//
// aml 4.29.02, returns true if we have HW input gain by checking the current HW model
//
bool AudioDeviceTreeParser::getHasHWInputGain(){
    bool hasHWInputGain = true;
    OSData *tempData;
    
    tempData = OSDynamicCast(OSData, soundEntry->getProperty(kModelPropName));
    FailIf(!tempData, EXIT);

    if((!bcmp(tempData->getBytesNoCopy(), kDacaModelName, tempData->getLength())) || 
        (!bcmp(tempData->getBytesNoCopy(), kTexasModelName, tempData->getLength())) || 
        (!bcmp(tempData->getBytesNoCopy(), kTexas2ModelName, tempData->getLength()))) {
		hasHWInputGain = false;
    } else {
		hasHWInputGain = true;
    }

// aml XXX testing
#if 0    
    if (!bcmp(tempData->getBytesNoCopy(), kDacaModelName, tempData->getLength())) {
        debugIOLog (3, "AudioDeviceTreeParser::getHasHWInputGain - kDacaModelName.");
    } else if (!bcmp(tempData->getBytesNoCopy(), kTexasModelName, tempData->getLength())) {
        debugIOLog (3, "AudioDeviceTreeParser::getHasHWInputGain - kTexasModelName.");
    } else if (!bcmp(tempData->getBytesNoCopy(), kTexas2ModelName, tempData->getLength())) {
        debugIOLog (3, "AudioDeviceTreeParser::getHasHWInputGain - kTexas2ModelName.");
    } else if (!bcmp(tempData->getBytesNoCopy(), kAWACsModelName, tempData->getLength())) {
        debugIOLog (3, "AudioDeviceTreeParser::getHasHWInputGain - kAWACsModelName.");
    } else if (!bcmp(tempData->getBytesNoCopy(), kScreamerModelName, tempData->getLength())) {
        debugIOLog (3, "AudioDeviceTreeParser::getHasHWInputGain - kScreamerModelName.");
    } else if (!bcmp(tempData->getBytesNoCopy(), kBurgundyModelName, tempData->getLength())) {
        debugIOLog (3, "AudioDeviceTreeParser::getHasHWInputGain - kBurgundyModelName.");
    } 
    debugIOLog (3, "AudioDeviceTreeParser::getHasHWInputGain - has HW gain = %d.", hasHWInputGain);
#endif

EXIT:
    return hasHWInputGain;
}

//
// aml 4.26.02, returns a dictionary of types (imic, line, etc.) and their zero dB gain offsets
//
UInt32 AudioDeviceTreeParser::getInternalMicGainOffset(){
    UInt32 zeroGain;

	zeroGain = 0;
#if OVERRIDE_REGISTRY_ZERO_GAIN
	// override the registry entries with these fixed values.
	// values come from open firmware files and OS 9 source code.
	// see AudioHardwareConstants.h for the full list of layouts.
	switch (getLayoutID()) {
		case layout101:
			zeroGain = 0x00090000;	// 9.0 dB
			break;
		case layoutKihei:
			zeroGain = 0x001E0000;	// 30 dB
			break;
		case layoutPismo:
			zeroGain = 0x00138000;	// 19.5 dB 
			break;
		case layoutMercury:
			zeroGain = 0x00090000;	// 9.0 dB 
			break;
		case layoutPerigee:
			zeroGain = 0x001E0000;	// 30 dB
			break;
		case layoutWallStreet:
			zeroGain = 0x00090000;	// 9.0 dB
			break;
		default:
			zeroGain = 0x0;			// 0 dB
			break;
	}

#else // OVERRIDE_REGISTRY_ZERO_GAIN

	OSData *			tempData;
	UInt32				portType;
	char *				theSoundObjects;
	char *				thetempObject;
	int					parser;
	int					startidx;
	int					stopidx;
	int					stopwordidx;
	int					startwordidx;
	int					size;
	int					length;
	UInt32				result;

    result = 0;
    tempData = OSDynamicCast(OSData, soundEntry->getProperty(kSoundObjectsPropName));
    FailIf(!tempData, BAIL);

    theSoundObjects = (char *) tempData->getBytesNoCopy();
    size = (int) tempData->getLength();
    parser = -1;

    while(size && parser < size) {
        ASSIGNSTARTSTRING(startidx, parser);
        NEXTENDOFSTRING(theSoundObjects, parser);
        ASSIGNSTOPSTRING(stopidx, parser);
        thetempObject = theSoundObjects+startidx;
                                                
        stopwordidx = 0;
        NEXTENDOFWORD(thetempObject, stopwordidx);
        //
        // Parse for input or mux objects
        //
        if(!bcmp(thetempObject, kInputObjEntryName, stopwordidx-1) ||
		   !bcmp(thetempObject, kMuxObjEntryName, stopwordidx-1)) {
		   
            length = stopidx-startidx;
            startwordidx = ++stopwordidx;
            
            while(stopwordidx < length) {
                //
                // reset variables
                //
                portType = 0;

                NEXTENDOFWORD(thetempObject, stopwordidx);
                //
                // Parse input objects for input port or mux type (internal mic, external mic, line, etc.)
                //
                if(!bcmp(thetempObject+startwordidx, kPortTypePropName, stopwordidx-startwordidx)) {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx); 
                    READWORDASNUMBER(portType, thetempObject, startwordidx, stopwordidx);

                    NEXTENDOFWORD(thetempObject, stopwordidx);
                     
                    if (portType == kIntMicSource) {
                        //
                        // next word is the zero gain value
                        //
                        ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                      	NEXTENDOFWORD(thetempObject, stopwordidx); 
                        if(!bcmp(thetempObject+startwordidx, kZeroGainPropName, stopwordidx-startwordidx)) {
                            ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                            NEXTENDOFWORD(thetempObject, stopwordidx); 
                            READWORDASNUMBER(zeroGain, thetempObject, startwordidx, stopwordidx);
						}
                    }
                    ASSIGNNEXTWORD(thetempObject, startwordidx, stopwordidx);                                                 
                }                
                ASSIGNNEXTWORD(thetempObject, startwordidx, stopwordidx);                                                 
            }
        } 
    }

#endif // OVERRIDE_REGISTRY_ZERO_GAIN

    return zeroGain;
}

OSArray *AudioDeviceTreeParser::createInputsArrayWithMuxes(){
    // this get the inputs and the muxes there are on
    OSArray *AudioInputs = 0;
    OSData *tempData;
    char * theSoundObjects, *thetempObject;
    int parser, startidx, stopidx, startwordidx, stopwordidx, length;
    UInt32 tlength, muxIndex;
    AudioHardwareInputInfo myInfo;
    UInt32 AudioInputMuxNb, AudioMuxNb;
    AudioHardwareMuxInfo   myMuxInfo;
    AudioHardwareInput *theInput;
    AudioHardwareMux	*theMux;
    bool muxflag = false;
    int size;
    
	muxIndex = 0;
	// get the output numbers and create the detects array
    AudioMuxNb = 0; 
    AudioInputMuxNb = getNumberofInputsWithMuxes();
	debugIOLog (3, "Number of inputs with muxes = %ld", AudioInputMuxNb);

    if( 0 != AudioInputMuxNb)
        AudioInputs = OSArray::withCapacity(AudioInputMuxNb);
	if (!AudioInputs) goto BAIL;	// No need to log, just abort.
    
	// get all the sound objects and do the parsing
    tempData = OSDynamicCast(OSData, soundEntry->getProperty(kSoundObjectsPropName));
    FailIf(!tempData, BAIL);

    theSoundObjects = (char *) tempData->getBytesNoCopy();
    size = (int) tempData->getLength();
    parser = -1;
    
    myInfo.sndHWPort = 0; myInfo.inputPortType = 0;
    myInfo.channels = 0;   myInfo.isOnMuX = muxflag;

    while(size && parser < size) {
		// find the end of string
        ASSIGNSTARTSTRING(startidx, parser);
        NEXTENDOFSTRING(theSoundObjects, parser);
        ASSIGNSTOPSTRING(stopidx, parser);
        thetempObject = theSoundObjects+startidx;
                        
		// do the detect parsing if it is one
        stopwordidx = 0;
        NEXTENDOFWORD(thetempObject, stopwordidx, size);
                        
        if(!bcmp(thetempObject, kInputObjEntryName, stopwordidx-1)) {
            length = stopidx-startidx;
            startwordidx = ++stopwordidx;
            
            while(stopwordidx < length) {

                NEXTENDOFWORD(thetempObject, stopwordidx, size);
                
                if(!bcmp(thetempObject+startwordidx, kPortChannelsPropName, stopwordidx-startwordidx)) {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx, size);
                    READWORDASNUMBER(myInfo.channels, thetempObject, startwordidx, stopwordidx);
                } else if(!bcmp(thetempObject+startwordidx, kPortConnectionPropName, stopwordidx-startwordidx)) {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx, size);
                    READWORDASNUMBER(myInfo.sndHWPort, thetempObject, startwordidx, stopwordidx);
                } else if(!bcmp(thetempObject+startwordidx, kPortTypePropName, stopwordidx-startwordidx)) {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx, size); 
                    READWORDASNUMBER(myInfo.inputPortType, thetempObject, startwordidx, stopwordidx);
                }
                ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);                                                 
            }
            
            if(myInfo.isOnMuX) {
                myInfo.theMuxRef =  OSDynamicCast(AudioHardwareMux, AudioInputs->getObject(muxIndex));
            } else {
                myInfo.theMuxRef = 0;
            }
            
            theInput = AudioHardwareInput::create(myInfo);
            AudioInputs->setObject(theInput);
            CLEAN_RELEASE(theInput);
            myInfo.sndHWPort = 0; myInfo.inputPortType = 0;
            myInfo.channels = 0; 
        } else if(!bcmp(thetempObject, kMuxObjEntryName, stopwordidx-1)) {
            muxflag = true;
            myInfo.isOnMuX = muxflag;
            
			// we know we have 1 mux we should create the array
            length = stopidx-startidx;
            startwordidx = ++stopwordidx;

             while(stopwordidx < length) {

                NEXTENDOFWORD(thetempObject, stopwordidx, size);
                
                if(!bcmp(thetempObject+startwordidx, kModelPropName, stopwordidx-startwordidx)) { //model
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx, size);
                    
                    if(!bcmp(thetempObject+startwordidx, kMuxProgOutName, 6)) 
                        myMuxInfo.MuxPortType = kAudioHardwareMuxPO;
                    else if(!bcmp(thetempObject+startwordidx, kMux101ObjName, 6)) 
                        myMuxInfo.MuxPortType = kAudioHardwareMux101;   
                     else if(!bcmp(thetempObject+startwordidx, kMuxWSObjName, 6)) 
                        myMuxInfo.MuxPortType = kAudioHardwareMuxWS;   
                    else 
                        myMuxInfo.MuxPortType = kAudioHardwareMuxUnknown;
                        
                } else if(!bcmp(thetempObject+startwordidx, kSourceMapPropName, stopwordidx-startwordidx)) {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx, size);
                  //  READWORDASNUMBER(myInfo.sndHWPort, thetempObject, startwordidx, stopwordidx);
                    tlength = sizeof(MuxSourceMap) * 2;
                    thetempObject[stopwordidx] = '\0';
                    getStringAsHexData((char *) &(thetempObject[startwordidx]),(UInt8 *) &(myMuxInfo.MuxPOsources), &tlength);
                    thetempObject[stopwordidx] = ' ';
                } else if(!bcmp(thetempObject+startwordidx, kSourceMapCountPropName, stopwordidx-startwordidx)) {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx, size); 
                    READWORDASNUMBER(myMuxInfo.MuxPOnumSources, thetempObject, startwordidx, stopwordidx);
                }                 
                ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);                  
            }
            
            if(kAudioHardwareMuxUnknown != myMuxInfo.MuxPortType) {
				// we don't add a mux that is unknown
                if (0 == AudioMuxNb)    { // creation of an array of one Mux we should think of more
                    theMux = AudioHardwareMux::create(myMuxInfo);
                    muxIndex = AudioInputs->getCount();
                    if(theMux)
                        AudioInputs->setObject(theMux);
                } else {
                    debugIOLog (3, "There shouldn't be another mux");
                }
            }                
        }
    }              
    
EXIT:
    return AudioInputs;
BAIL:
    goto EXIT;
}

OSArray *AudioDeviceTreeParser::createDetectsArray(){
    OSArray* AudioDetects;
    OSData *tempData;
    char * theSoundObjects, *thetempObject;
    int parser, startidx, stopidx, startwordidx, stopwordidx, length;
    short detectidx;
    AudioHardwareDetect *theDetect;
    AudioHardwareDetectInfo myInfo;
    int size;

    myInfo.detectKind =0; myInfo.bitMask = 0;
    myInfo.bitMatch = 0; myInfo.device = 0;

	// get the detect numbers and create the detects array
    AudioDetects = OSArray::withCapacity(getNumberOfDetects());
    
	// get the complete Sound objects to parse them
    tempData = OSDynamicCast(OSData, soundEntry->getProperty(kSoundObjectsPropName));
    FailIf(!tempData, BAIL);

    theSoundObjects = (char *) tempData->getBytesNoCopy();
    size = (int) tempData->getLength();
    parser = -1; detectidx =0;
    
    while(size && parser < size) {

		// find the end of string
        ASSIGNSTARTSTRING(startidx, parser);
        NEXTENDOFSTRING(theSoundObjects, parser);
        ASSIGNSTOPSTRING(stopidx, parser);
        thetempObject = theSoundObjects+startidx;
                        
		// do the detect parsing if it is one
        stopwordidx = 0;
        NEXTENDOFWORD(thetempObject, stopwordidx, size);
                        
        if(!bcmp(thetempObject, kDetectObjEntryName, stopwordidx-1)) {
        
            length = stopidx-startidx;
            startwordidx = ++stopwordidx;
            
            while(stopwordidx < length) {
                NEXTENDOFWORD(thetempObject, stopwordidx, size);
    
                if(!bcmp(thetempObject+startwordidx, kBitMaskPropName, stopwordidx-startwordidx)) {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx, size);
                    READWORDASNUMBER(myInfo.bitMask, thetempObject, startwordidx, stopwordidx);
                } else if(!bcmp(thetempObject+startwordidx, kBitMatchPropName, stopwordidx-startwordidx)) {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx, size);
                    READWORDASNUMBER(myInfo.bitMatch, thetempObject, startwordidx, stopwordidx);
                } else if(!bcmp(thetempObject+startwordidx, kDevicePropName, stopwordidx-startwordidx)) {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx, size);
                    READWORDASNUMBER(myInfo.device, thetempObject, startwordidx, stopwordidx);
                } else if(!bcmp(thetempObject+startwordidx, kModelPropName, stopwordidx-startwordidx)) {
                    ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
                    NEXTENDOFWORD(thetempObject, stopwordidx, size);
                    if(!bcmp(thetempObject+startwordidx, kAnyInDetectObjName, 6))
                        myInfo.detectKind = kAudioHardwareDetectAnyInSense;
                    else if(!bcmp(thetempObject+startwordidx, kInSenseBitsDetectObjName, 6)) 
                        myInfo.detectKind = kAudioHardwareDetectInSense;
                    else if(!bcmp(thetempObject+startwordidx, kGPIODetectObjName, 6)) 
                        myInfo.detectKind = kAudioHardwareDetectGPIO;
                    else if(!bcmp(thetempObject+startwordidx, kGPIOGenericDetectObjName, 6)) 
                        myInfo.detectKind = kAudioHardwareGenericDetectGPIO;
                    else
                        myInfo.detectKind = kAudioHardwareDetectUnknown;
                }
                ASSIGNNEXTWORD(thetempObject,startwordidx, stopwordidx);
            }
            
            theDetect = AudioHardwareDetect::create(detectidx, myInfo);
            AudioDetects->setObject(theDetect);
            CLEAN_RELEASE(theDetect);
            
            myInfo.detectKind =0; myInfo.bitMask = 0;
            myInfo.bitMatch = 0; myInfo.device = 0;
            detectidx++;
        }
    }              

EXIT:
    return AudioDetects;
BAIL:
    goto EXIT;
 
}
    
UInt8 AudioDeviceTreeParser::convertAsciiToHexData(char ascii) {
    UInt8 subvalue;
    subvalue = 0;
    
    if((ascii >= '0') && (ascii <= '9'))
        subvalue = '0';
    else if ((ascii >= 'a') && (ascii <= 'f'))
        subvalue = 'a'- 10;
    else if ((ascii >= 'A') && (ascii <= 'F'))
        subvalue = 'A'-10;
    
    return ascii-subvalue;
}

UInt32 AudioDeviceTreeParser::getStringAsNumber(char *string){
    SInt32 number, multiplier;
    char *valueStr;
    
    valueStr = string;
    number = 0;
    if (('0' == *valueStr) && ('x' == *(valueStr+1))) { // this is an hex number
        valueStr+=2;
        while(*valueStr)
            number = number*16+convertAsciiToHexData(*valueStr++);
    } else {
        if ('-' == *valueStr) {
            multiplier = -1;
            valueStr++;
        } else 
            multiplier = 1;
            
        while(*valueStr)
            number = (number *10)+ convertAsciiToHexData(*valueStr++);
        number *= multiplier;
    }

    return number;
}

bool AudioDeviceTreeParser::getStringAsHexData( char *string, UInt8 *value, UInt32 *size )
{
	UInt32	length;
	bool	err = true;	
        length = 0;
		
        while (*string && (length < *size)) {
            *value++ = (convertAsciiToHexData(*string++) << 4) |convertAsciiToHexData(*string++);
            length++;
        }
		
        *size = length;	
	return err;
}

