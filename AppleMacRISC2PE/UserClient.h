/*
 *  UserClient.h
 *  AppleMacRISC2PE
 *
 */

#ifndef _USERCLIENT_
#define _USERCLIENT_

// the data structure to pass values between kernel space and user space
typedef struct  regStruct
{
    UInt32 int32_0;
    UInt32 int32_1;
	UInt32 int32_2;
	UInt32 int32_3;
} regStruct;


// the user client method names    
enum
{
    kUserClientOpen,
    kUserClientClose,
    kUserClient_DFS,
    kUserClient_DFSCont,
    kUserClient_DFSStopCont,
    kUserClient_GPUCont,
    kUserClient_GPUStopCont,
    kUserClient_vStep,
    kUserClient_vStepCont,
    kUserClient_vStepStopCont,
    kUserClient_SetGPUPower,
    kNumberOfMethods
};

#endif
