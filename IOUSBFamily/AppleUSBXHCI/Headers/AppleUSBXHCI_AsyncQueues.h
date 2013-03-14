#ifndef AppleUSBXHCI_AppleUSBXHCI_AsyncQueues_h
#define AppleUSBXHCI_AppleUSBXHCI_AsyncQueues_h

#include <libkern/c++/OSMetaClass.h>
#include <libkern/c++/OSObject.h>
#include <IOKit/usb/IOUSBCommand.h>
#include <IOKit/usb/IOUSBControllerListElement.h>
#include <IOKit/IODMACommand.h>

#include "XHCI.h"
#include "AppleUSBXHCIUIM.h"

class AppleXHCIAsyncEndpoint;
class AppleUSBXHCI;

#define kFreeTDs                        1
#define kAsyncMaxFragmentSize           PAGE_SIZE*32      // 4K * 32 = 128K this is > the max value for TRB length field 
                                                          // but ::_createTransfer->GenerateNextPhysicalSegment takes care 
                                                          // of the range not crossing 64K boundary
#define kMaxFreeSpaceInRing             2                 // Space for 2 more TDs with multiple of maxTRBs from queued TDs.
#define kAccountForAlignment            2                 // For Event DATA trb & unaligned buffer
#define kMinimumTDs                     1

// AppleXHCIAsyncTransferDescriptors - ATDs
class AppleXHCIAsyncTransferDescriptor : public OSObject
{
    OSDeclareDefaultStructors(AppleXHCIAsyncTransferDescriptor)

public:
	virtual bool	init();
	virtual void    free(void);
    
    void print(int level);
    void reinit();

	IOUSBCommand	*activeCommand;		// Lookup ID across lists.
	UInt32			transferSize;		// Minimum should be maxBurst - 48K for SS or HS
	UInt32			remAfterThisTD;		// the remaining size in the TDs AFTER this one
	IOByteCount     startOffset;		// From IOUSBCommand so we can tell _createTransfer where to start
	UInt32 			trbIndex;			// Say index 27 to 32 for this particular transfer in the ring
	UInt32 			trbCount;			// For example: 20K will take 5 or 6 TRBs
    bool            interruptThisTD;    // 
    bool            fragmentedTD;       // Indicates a fragmented TD. False for transfers < kAsyncMaxFragmentSize
    UInt16          totalTDs;           // filled in the last TD to indicate the total fragments for this transfer
    UInt32          offCOverride;
    UInt32          shortfall;
    UInt16          maxTRBs;            // = (transferSize ÷ 4K pages) + kAccountForAlignment 
    bool            immediateTransfer;
    bool            last;
    UInt8           immediateBuffer[kMaxImmediateTRBTransferSize];
    UInt16          streamID;
    SInt16          completionIndex;
    
    bool            flushed;
    bool            lastFlushedTD;
    bool            lastInRing;
        
    AppleXHCIAsyncEndpoint              *_endpoint;
    AppleXHCIAsyncTransferDescriptor	*_logicalNext;				// the next element in the list
    

    // constructor method
    static AppleXHCIAsyncTransferDescriptor 	*ForEndpoint(AppleXHCIAsyncEndpoint *endpoint);
};

class AppleXHCIAsyncEndpoint : public OSObject
{
    friend class AppleUSBXHCI;
    
    OSDeclareDefaultStructors(AppleXHCIAsyncEndpoint)

protected:
    virtual bool                        init(AppleUSBXHCI *controller, struct ringStruct *ring, UInt16 maxPacketSize, UInt16 maxBurst, UInt16 mult);

public:
    virtual bool                        init();
	virtual void						free(void);
    
    
    void                                validateLists(int level);
	void								print(int level);
	struct ringStruct                   *_ring;						// a.k.a. XHCIRing *
    
    AppleXHCIAsyncTransferDescriptor  	*readyQueue;				// all transfers get chunked and Q'd here	
    AppleXHCIAsyncTransferDescriptor  	*readyEnd;					// end marker
    
    AppleXHCIAsyncTransferDescriptor  	*activeQueue;				// all transfers in HW ring	
    AppleXHCIAsyncTransferDescriptor  	*activeEnd;					// end marker
    
    AppleXHCIAsyncTransferDescriptor  	*doneQueue;					// all transfers which are completed are Q'd here
    AppleXHCIAsyncTransferDescriptor  	*doneEnd;					// end marker
    
    // 
    // Lifecycle of TDs : freeQueue -> readyQueue -> activeQueue ->  doneQueue -> freeQueue
    AppleXHCIAsyncTransferDescriptor  	*freeQueue;					// all free TDs are Q'd here - 
    AppleXHCIAsyncTransferDescriptor  	*freeEnd;					// end marker
        
    UInt32								onReadyQueue;				
    UInt32								onActiveQueue;				
    UInt32								onDoneQueue;				
    UInt32                              onFreeQueue;
    
	bool								_aborting;
    
    UInt32                              _maxPacketSize;
    UInt32                              _maxBurst;
    UInt32                              _mult;
    
    UInt32                              _actualFragmentSize;
    
    AppleUSBXHCI                        *_xhciUIM;

    void PutTDAtHead(AppleXHCIAsyncTransferDescriptor **qStart, AppleXHCIAsyncTransferDescriptor **qEnd, AppleXHCIAsyncTransferDescriptor *pTD, UInt32 *qCount);
    
    void PutTD(AppleXHCIAsyncTransferDescriptor **qStart, AppleXHCIAsyncTransferDescriptor **qEnd, AppleXHCIAsyncTransferDescriptor *pTD, UInt32 *qCount);
    
    AppleXHCIAsyncTransferDescriptor *GetTD(AppleXHCIAsyncTransferDescriptor **qStart, AppleXHCIAsyncTransferDescriptor **qEnd, UInt32 *qCount);
    
    void PutTDonFreeQueue(AppleXHCIAsyncTransferDescriptor *pTD);

    AppleXHCIAsyncTransferDescriptor *GetTDFromFreeQueue(bool allocate = true);

    void PutTDonReadyQueueAtHead(AppleXHCIAsyncTransferDescriptor *pTD);

    void PutTDonReadyQueue(AppleXHCIAsyncTransferDescriptor *pTD);
    
    AppleXHCIAsyncTransferDescriptor *GetTDFromReadyQueue();

    void PutTDonDoneQueue(AppleXHCIAsyncTransferDescriptor *pTD);

    AppleXHCIAsyncTransferDescriptor *GetTDFromDoneQueue();

    void PutTDonActiveQueue(AppleXHCIAsyncTransferDescriptor *pTD);
    
    AppleXHCIAsyncTransferDescriptor *GetTDFromActiveQueue();

    AppleXHCIAsyncTransferDescriptor *GetTDFromActiveQueueWithIndex(UInt16 trbIndex);

    void    MoveTDsFromReadyQToDoneQ(IOUSBCommand *pUSBCommand = NULL);

    // AppleXHCIAsyncTransferDescriptor *FindNearByActiveTD(int deQueueIndex);

    void    MoveAllTDsFromReadyQToDoneQ();
    
    void    MoveAllTDsFromActiveQToDoneQ();
    
    //
    //  Create ATDs by chunking the IOUSBCommand and add them to the readyQueue
    //
    IOReturn    CreateTDs(IOUSBCommand *pUSBCommand, UInt16 streamID = 0, UInt32 offsC = 0, UInt8 immediateTransferSize = kInvalidImmediateTRBTransferSize, UInt8 *immediateBuffer = NULL);
    
    //
    //  Schedule the ATDs from readyQueue to the ring and add them to the HW ring
    //
    void    ScheduleTDs();

    //
    //  Flush, Complete and Schedule more ATDs
    //
    void    ScavengeTDs(AppleXHCIAsyncTransferDescriptor *pActiveTD, IOReturn status, bool complete, bool flush);

    //
    //  Set ATDs in activeQueue as flushed
    //
    void    FlushTDs(IOUSBCommand *pUSBCommand);
    
    //
    //  Flush ATDs in activeQueue with status
    //
    void    FlushTDsWithStatus(IOUSBCommand *pUSBCommand, IOReturn status);

    //
    //  Complete the ATDs that are in the doneQueue
    //
    void    Complete(IOReturn status);
    
    //
    // Abort the ATDs starting from activeQueue -> doneQueue and readyQueue -> doneQueue
    // 
    // Called from UIMDeleteEndpoint
    // Called from ReturnAllTransfers
    //
    IOReturn    Abort();

    //
    // Walk the activeQueue and Update the timeout for the activeCommands in the TDs
    // 
    void UpdateTimeouts(bool abortAll, UInt32 curFrame, bool stopped);  

    //
    // Evaluate and set the IOUSBCommand to have noDataTimeouts or not
    //
    bool NeedTimeouts();
};

#endif
