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
    void setupCursor( IOPixelInformation * info );
    void stopCursor( void );
    IOReturn doSetup( bool full );
    void findConsole(void);
    IOReturn createSharedCursor( int shmemVersion,
					int maxWidth, int maxHeight );
    IOReturn setBoundingRect( IOGBounds * bounds );
    IOReturn setUserRanges( void );
    IOReturn getConnectFlagsForDisplayMode(
                    IODisplayModeID mode, IOIndex connection, UInt32 * flags );

    IOReturn beginSystemSleep( void * ackRef );

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

    static void StdFBDisplayCursor444(
                                    IOFramebuffer * inst,
                                    StdFBShmem_t *shmem,
                                    volatile unsigned short *vramPtr,
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

    static void StdFBDisplayCursor32xxxA(
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
    static void deferredInterrupt( OSObject * owner,
                                    IOInterruptEventSource * evtSrc, int intCount );
    static void deferredCLUTSetInterrupt( OSObject * owner,
                                          IOInterruptEventSource * evtSrc, int intCount );
    static void deferredSpeedChangeEvent( OSObject * owner,
					      IOInterruptEventSource * evtSrc, int intCount );
    void checkDeferredCLUTSet( void );
    void updateCursorForCLUTSet( void );
    IOReturn updateGammaTable(	UInt32 channelCount, UInt32 dataCount,
				UInt32 dataWidth, void * data );

    static void dpInterruptProc(OSObject * target, void * ref);
    static void dpInterrupt(OSObject * owner, IOTimerEventSource * sender);
    void dpProcessInterrupt(void);
    void dpUpdateConnect(void);
	
    static void handleVBL(IOFramebuffer * inst, void * ref);
    static void writePrefs( OSObject * owner, IOTimerEventSource * sender );
    static void connectChangeInterrupt( IOFramebuffer * inst, void * ref );
    static void connectChangeDelayedInterrupt( OSObject * owner, IOTimerEventSource * sender );
    void checkConnectionChange( bool message = true );
    void setNextDependent( IOFramebuffer * dependent );
    IOFramebuffer * getNextDependent( void );
    void setCaptured( bool isCaptured );
    void setDimDisable( bool dimDisable );
    bool getDimDisable( void );
    IOReturn notifyServer( UInt8 state );
    void notifyServerAll( UInt8 state );
    IOReturn extEntry(void);
    void wakeServerState(UInt8 state);
    bool getIsUsable( void );
    IOReturn postOpen( void );
    static void startThread(bool highPri);
    static void sleepWork( void * arg );
    static void clamshellWork( thread_call_param_t p0, thread_call_param_t p1 );
    IOOptionBits checkPowerWork( void );
    IOReturn postWake( IOOptionBits state );
    IOReturn deliverDisplayModeDidChangeNotification( void );
    static IOReturn systemPowerChange( void * target, void * refCon,
                                    UInt32 messageType, IOService * service,
                                    void * messageArgument, vm_size_t argSize );
    static bool clamshellHandler( void * target, void * ref,
   				       IOService * resourceService );
    static void clamshellProbeAction( OSObject * owner, IOTimerEventSource * sender );

    static IOReturn probeAll( IOOptionBits options );

    IOReturn selectTransform( UInt64 newTransform, bool generateChange );
    void setTransform( UInt64 newTransform, bool generateChange );
    UInt64 getTransform( void );
    IOReturn checkMirrorSafe( UInt32 value, IOFramebuffer * other );
    void transformLocation(StdFBShmem_t * shmem, IOGPoint * cursorLoc, IOGPoint * transformLoc);
    void transformCursor(StdFBShmem_t * shmem, IOIndex frame);

    bool deepFramebuffer( IOPixelInformation * pixelInfo );
    bool validFramebuffer( IOPixelInformation * pixelInfo );

    // --

    IOReturn extCreateSharedCursor( int shmemVersion,
					int maxWidth, int maxHeight );
    IOReturn extGetPixelInformation( 
	IODisplayModeID displayMode, IOIndex depth,
	IOPixelAperture aperture, IOPixelInformation * pixelInfo );
    IOReturn extGetCurrentDisplayMode( 
                            IODisplayModeID * displayMode, IOIndex * depth );
    IOReturn extSetStartupDisplayMode( 
                            IODisplayModeID displayMode, IOIndex depth );
    IOReturn extSetGammaTable( 
                            UInt32 channelCount, UInt32 dataCount,
                            UInt32 dataWidth, void * data );
    IOReturn extGetDisplayModeCount( IOItemCount * count );
    IOReturn extGetDisplayModes( IODisplayModeID * allModes,
					IOByteCount * size );
    IOReturn extSetDisplayMode( IODisplayModeID displayMode,
                                    IOIndex depth );
    IOReturn extGetInformationForDisplayMode( 
		IODisplayModeID mode, void * info, IOByteCount length );

    IOReturn extGetVRAMMapOffset( IOPixelAperture aperture,
				 IOByteCount * offset );
    IOReturn extSetBounds( IOGBounds * bounds );

    IOReturn extSetNewCursor( void * cursor, IOIndex frame,
					IOOptionBits options );
    IOReturn extSetCursorVisible( bool visible );
    IOReturn extSetCursorPosition( SInt32 x, SInt32 y );
    IOReturn extSetColorConvertTable( UInt32 select,
                                        UInt8 * data, IOByteCount length );
    IOReturn extSetCLUTWithEntries( UInt32 index, IOOptionBits options,
                            IOColorEntry * colors, IOByteCount inputCount );
    IOReturn extSetAttribute(
            IOSelect attribute, UInt32 value, IOFramebuffer * other );
    IOReturn extGetAttribute( 
            IOSelect attribute, UInt32 * value, IOFramebuffer * other );
    IOReturn getDefaultMode( 
                        IOIndex connection, IODisplayModeID * mode, IOIndex * depth);
    IOReturn extValidateDetailedTiming( 
                void * description, void * outDescription,
                IOByteCount inSize, IOByteCount * outSize );
    IOReturn extRegisterNotificationPort( 
                    mach_port_t 	port,
                    UInt32		type,
                    UInt32		refCon );
    IOReturn extAcknowledgeNotification( void );
    IOReturn extSetProperties( OSDictionary * dict );

public:

    static void clamshellEnable( SInt32 delta );
    static IOOptionBits clamshellState( void );
    static IOReturn setPreferences( IOService * props, OSDictionary * prefs );
    static OSObject * copyPreferences( void );
    OSObject * copyPreference( class IODisplay * display, const OSSymbol * key );
    bool getIntegerPreference( IODisplay * display, const OSSymbol * key, UInt32 * value );
    bool setPreference( class IODisplay * display, const OSSymbol * key, OSObject * value );
    bool setIntegerPreference( IODisplay * display, const OSSymbol * key, UInt32 value );
    void getTransformPrefs( IODisplay * display );
    IOReturn flushParameters(void);

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
