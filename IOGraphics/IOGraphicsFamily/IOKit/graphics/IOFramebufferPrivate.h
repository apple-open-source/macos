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

    void setupCursor(void);
    void stopCursor( void );
    IOReturn doSetup( bool full );
    void findConsole(void);
    IOReturn createSharedCursor( int shmemVersion,
                                        int maxWidth, int maxHeight );
    IOReturn setBoundingRect( IOGBounds * bounds );
    IOReturn setUserRanges( void );
    IOReturn getConnectFlagsForDisplayMode(
                    IODisplayModeID mode, IOIndex connection, UInt32 * flags );

    static inline void StdFBDisplayCursor( IOFramebuffer * inst );
    static inline void StdFBRemoveCursor( IOFramebuffer * inst );
    static inline void RemoveCursor( IOFramebuffer * inst );
    static inline void DisplayCursor( IOFramebuffer * inst );
    static inline void SysHideCursor( IOFramebuffer * inst );
    static inline void SysShowCursor( IOFramebuffer * inst );
    static inline void CheckShield( IOFramebuffer * inst );
    inline IOOptionBits _setCursorImage( UInt32 frame );
    inline IOReturn _setCursorState( SInt32 x, SInt32 y, bool visible );
    static void cursorWork( OSObject * p0, IOInterruptEventSource * evtSrc, int intCount );

    static void StdFBDisplayCursor8P(
                                    IOFramebuffer * inst,
                                    StdFBShmem_t *shmem,
                                    volatile unsigned char *vramPtr,
                                    unsigned int cursStart,
                                    unsigned int vramRow,
                                    unsigned int cursRow,
                                    int width,
                                    int height );

    static void StdFBDisplayCursor8G(
                                    IOFramebuffer * inst,
                                    StdFBShmem_t *shmem,
                                    volatile unsigned char *vramPtr,
                                    unsigned int cursStart,
                                    unsigned int vramRow,
                                    unsigned int cursRow,
                                    int width,
                                    int height );

    static void StdFBDisplayCursor555(
                                    IOFramebuffer * inst,
                                    StdFBShmem_t *shmem,
                                    volatile unsigned short *vramPtr,
                                    unsigned int cursStart,
                                    unsigned int vramRow,
                                    unsigned int cursRow,
                                    int width,
                                    int height );

    static void StdFBDisplayCursor30Axxx(
                                    IOFramebuffer * inst,
                                    StdFBShmem_t *shmem,
                                    volatile unsigned int *vramPtr,
                                    unsigned int cursStart,
                                    unsigned int vramRow,
                                    unsigned int cursRow,
                                    int width,
                                    int height );

    static void StdFBDisplayCursor32Axxx(
                                    IOFramebuffer * inst,
                                    StdFBShmem_t *shmem,
                                    volatile unsigned int *vramPtr,
                                    unsigned int cursStart,
                                    unsigned int vramRow,
                                    unsigned int cursRow,
                                    int width,
                                    int height );

    static void StdFBRemoveCursor8(
                                    IOFramebuffer * inst,
                                    StdFBShmem_t *shmem,
                                    volatile unsigned char *vramPtr,
                                    unsigned int vramRow,
                                    int width,
                                    int height );
    static void StdFBRemoveCursor16(
                                    IOFramebuffer * inst,
                                    StdFBShmem_t *shmem,
                                    volatile unsigned short *vramPtr,
                                    unsigned int vramRow,
                                    int width,
                                    int height );

    static void StdFBRemoveCursor32(
                                    IOFramebuffer * inst,
                                    StdFBShmem_t *shmem,
                                    volatile unsigned int *vramPtr,
                                    unsigned int vramRow,
                                    int width,
                                    int height );

    static void deferredMoveCursor(IOFramebuffer * inst);

    static void deferredCLUTSetInterrupt( OSObject * owner,
                                          IOInterruptEventSource * evtSrc, int intCount );
    static void deferredSpeedChangeEvent( OSObject * owner,
                                              IOInterruptEventSource * evtSrc, int intCount );

    static IOReturn pmSettingsChange(OSObject *target, const OSSymbol *type,
                                                 OSObject *val, uintptr_t refcon);

    static void systemWork(OSObject * owner,
                                        IOInterruptEventSource * evtSrc, int intCount);
	static void controllerAsyncWork(OSObject * owner,
                                        IOInterruptEventSource * evtSrc, int intCount);
	static void startControllerThread(struct IOFBController * controller);
	static void controllerDidWork(IOFBController * controller, IOOptionBits work);
    static void startAsync(IOFBController * controller, uint32_t asyncWork);
    static IOFBController * aliasController(IOFBController * controller);
	static void serverAckTimeout(OSObject * owner, IOTimerEventSource * sender);

	static uint32_t controllerState(IOFBController * controller);

    static IOOptionBits checkPowerWork(IOFBController * controller, IOOptionBits state);
    static IOOptionBits checkConnectionWork(IOFBController * controller, 
                                                IOOptionBits state );
    IOOptionBits checkPowerWork(IOOptionBits state);

    void checkDeferredCLUTSet( void );
    void updateCursorForCLUTSet( void );
    IOReturn updateGammaTable(  UInt32 channelCount, UInt32 dataCount,
                                UInt32 dataWidth, const void * data, bool immediate = false );

    static void dpInterruptProc(OSObject * target, void * ref);
    static void dpInterrupt(OSObject * owner, IOTimerEventSource * sender);
    void dpProcessInterrupt(void);
    void dpUpdateConnect(void);
        

    static void delayedEvent(thread_call_param_t p0, thread_call_param_t p1);
    static void resetClamshell(uint32_t delay);

    static void deferredVBLDisable(OSObject * owner,
                                   IOInterruptEventSource * evtSrc, int intCount);
    static void updateVBL(OSObject * owner, IOTimerEventSource * sender);
    static void deferredCLUTSetTimer(OSObject * owner, IOTimerEventSource * sender);
    static void handleVBL(IOFramebuffer * inst, void * ref);
    bool getTimeOfVBL(AbsoluteTime * deadline, uint32_t frames);


    static void writePrefs( OSObject * owner, IOTimerEventSource * sender );
    static void connectChangeInterrupt( IOFramebuffer * inst, void * ref );
    void setNextDependent( IOFramebuffer * dependent );
    IOFramebuffer * getNextDependent( void );
    void setCaptured( bool isCaptured );
    void setDimDisable( bool dimDisable );
    bool getDimDisable( void );
    IOReturn notifyServer( UInt8 state );
    IOReturn _extEntry(bool system, bool allowOffline, const char * where);
    void     _extExit(bool system, IOReturn result, const char * where);
    bool getIsUsable(void);
	void initFB(void);
    IOReturn postOpen(void);
    IOReturn postWake(void);
    static void checkConnectionChange(IOFBController * controller );
    static void messageConnectionChange(IOFBController * controller );
    static IOReturn processConnectChange(IOFBController * controller, IOOptionBits mode);
	IOReturn matchFramebuffer(void);
    static IOReturn matchController(IOFBController * controller);

    IOReturn extProcessConnectionChange(void);
    IOReturn extEndConnectionChange(void);
    IOReturn processConnectChange(IOOptionBits mode);
    bool suspend(bool now);
    bool updateOnline(void);
    void displaysOnline(bool nowOnline);


    static void startThread(bool highPri);
    static void sleepWork( void * arg );
    static void clamshellWork( thread_call_param_t p0, thread_call_param_t p1 );
    void saveFramebuffer(void);
    IOReturn restoreFramebuffer(IOIndex event);

    IOReturn deliverDisplayModeDidChangeNotification( void );

    static IOReturn systemPowerChange( void * target, void * refCon,
                                    UInt32 messageType, IOService * service,
                                    void * messageArgument, vm_size_t argSize );
    static IOReturn agcMessage( void * target, void * refCon,
                                    UInt32 messageType, IOService * service,
                                    void * messageArgument, vm_size_t argSize );
	static IOReturn muxPowerMessage(UInt32 messageType);

    static bool clamshellHandler( void * target, void * ref,
                                       IOService * resourceService, IONotifier * notifier );
    static void readClamshellState(void);

    static IOReturn probeAll( IOOptionBits options );

    IOReturn selectTransform( UInt64 newTransform, bool generateChange );
    void setTransform( UInt64 newTransform, bool generateChange );
    UInt64 getTransform( void );
    IOReturn checkMirrorSafe( UInt32 value, IOFramebuffer * other );
    void transformLocation(StdFBShmem_t * shmem, IOGPoint * cursorLoc, IOGPoint * transformLoc);
    void transformCursor(StdFBShmem_t * shmem, IOIndex frame);

    bool deepFramebuffer( IOPixelInformation * pixelInfo );
    bool validFramebuffer( IOPixelInformation * pixelInfo );
    IOIndex closestDepth(IODisplayModeID mode, IOPixelInformation * pixelInfo);
    IOReturn setDisplayAttributes(OSObject * data);
    IOReturn doSetDisplayMode(IODisplayModeID displayMode, IOIndex depth);
	OSData * getConfigMode(IODisplayModeID mode, const OSSymbol * sym);

	static void saveGammaTables(void);

    // --

    static IOReturn extCreateSharedCursor(OSObject * target, void * reference, IOExternalMethodArguments * args);
    static IOReturn extGetPixelInformation(OSObject * target, void * reference, IOExternalMethodArguments * args);
    static IOReturn extGetCurrentDisplayMode(OSObject * target, void * reference, IOExternalMethodArguments * args);
    static IOReturn extSetStartupDisplayMode(OSObject * target, void * reference, IOExternalMethodArguments * args);
    static IOReturn extSetGammaTable(OSObject * target, void * reference, IOExternalMethodArguments * args);
    static IOReturn extGetDisplayModeCount(OSObject * target, void * reference, IOExternalMethodArguments * args);
    static IOReturn extGetDisplayModes(OSObject * target, void * reference, IOExternalMethodArguments * args);
    static IOReturn extSetDisplayMode(OSObject * target, void * reference, IOExternalMethodArguments * args);
    static IOReturn extGetInformationForDisplayMode(OSObject * target, void * reference, IOExternalMethodArguments * args);

    static IOReturn extGetVRAMMapOffset(OSObject * target, void * reference, IOExternalMethodArguments * args);
    static IOReturn extSetBounds(OSObject * target, void * reference, IOExternalMethodArguments * args);

    static IOReturn extSetNewCursor(OSObject * target, void * reference, IOExternalMethodArguments * args);
    static IOReturn extSetCursorVisible(OSObject * target, void * reference, IOExternalMethodArguments * args);
    static IOReturn extSetCursorPosition(OSObject * target, void * reference, IOExternalMethodArguments * args);
    static IOReturn extSetColorConvertTable(OSObject * target, void * reference, IOExternalMethodArguments * args);
    static IOReturn extSetCLUTWithEntries(OSObject * target, void * reference, IOExternalMethodArguments * args);
    static IOReturn extSetAttribute(OSObject * target, void * reference, IOExternalMethodArguments * args);
    static IOReturn extGetAttribute(OSObject * target, void * reference, IOExternalMethodArguments * args);
    IOReturn extSetMirrorOne(uint32_t value, IOFramebuffer * other);
    static IOReturn extValidateDetailedTiming(OSObject * target, void * reference, IOExternalMethodArguments * args);
	void serverAcknowledgeNotification(void);
    static IOReturn extAcknowledgeNotification(OSObject * target, void * reference, IOExternalMethodArguments * args);

    // --

    IOReturn extSetProperties( OSDictionary * dict );
    IOReturn extRegisterNotificationPort( 
                    mach_port_t         port,
                    UInt32              type,
                    UInt32              refCon );

public:
    void fbLock( void );
    void fbUnlock( void );

    void displayOnline( IODisplay * display, SInt32 delta, uint32_t options );
    static void updateDisplaysPowerState(void);
    static IOOptionBits clamshellState( void );
    static IOReturn setPreferences( IOService * props, OSDictionary * prefs );
    static OSObject * copyPreferences( void );
    OSObject * copyPreference( class IODisplay * display, const OSSymbol * key );
    bool getIntegerPreference( IODisplay * display, const OSSymbol * key, UInt32 * value );
    bool setPreference( class IODisplay * display, const OSSymbol * key, OSObject * value );
    bool setIntegerPreference( IODisplay * display, const OSSymbol * key, UInt32 value );
    void getTransformPrefs( IODisplay * display );
    IOReturn flushParameters(void);
    IOReturn getAttributeForConnectionParam(IOIndex connectIndex, 
                                            IOSelect attribute, uintptr_t * value);
    IOReturn setAttributeForConnectionParam(IOIndex connectIndex,
                                            IOSelect attribute, uintptr_t value);

protected:

    IOReturn stopDDC1SendCommand(IOIndex bus, IOI2CBusTiming * timing);
    void waitForDDCDataLine(IOIndex bus, IOI2CBusTiming * timing, UInt32 waitTime);

    IOReturn readDDCBlock(IOIndex bus, IOI2CBusTiming * timing, UInt8 deviceAddress, UInt8 startAddress, UInt8 * data);
    IOReturn i2cReadDDCciData(IOIndex bus, IOI2CBusTiming * timing, UInt8 deviceAddress, UInt8 count, UInt8 *buffer);
    IOReturn i2cReadData(IOIndex bus, IOI2CBusTiming * timing, UInt8 deviceAddress, UInt8 count, UInt8 * buffer);
    IOReturn i2cWriteData(IOIndex bus, IOI2CBusTiming * timing, UInt8 deviceAddress, UInt8 count, UInt8 * buffer);

    void i2cStart(IOIndex bus, IOI2CBusTiming * timing);
    void i2cStop(IOIndex bus, IOI2CBusTiming * timing);
    void i2cSendAck(IOIndex bus, IOI2CBusTiming * timing);
    void i2cSendNack(IOIndex bus, IOI2CBusTiming * timing);
    IOReturn i2cWaitForAck(IOIndex bus, IOI2CBusTiming * timing);
    void i2cSendByte(IOIndex bus, IOI2CBusTiming * timing, UInt8 data);
    IOReturn i2cReadByte(IOIndex bus, IOI2CBusTiming * timing, UInt8 *data);
    IOReturn i2cWaitForBus(IOIndex bus, IOI2CBusTiming * timing);
    IOReturn i2cRead(IOIndex bus, IOI2CBusTiming * timing, UInt8 deviceAddress, UInt8 numberOfBytes, UInt8 * data);
    IOReturn i2cWrite(IOIndex bus, IOI2CBusTiming * timing, UInt8 deviceAddress, UInt8 numberOfBytes, UInt8 * data);
    void i2cSend9Stops(IOIndex bus, IOI2CBusTiming * timing);

    // retired: serverPendingAck, configPending, connectChange
