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

/*!
 * @header IOAudioDevice
 */

#ifndef _IOKIT_IOAUDIODMAENGINE_H
#define _IOKIT_IOAUDIODMAENGINE_H

#include <IOKit/IOService.h>

#include <IOKit/audio/IOAudioTypes.h>

class OSDictionary;
class IOAudioDMAEngineUserClient;
class IOAudioDevice;
class IOAudioStream;
class IOAudioControl;
class IOCommandGate;

#define IOAUDIODMAENGINE_DEFAULT_NUM_ERASES_PER_BUFFER	4

/*!
 * @enum IOAudioDMAEngineState
 * @abstract Represents the state of an IOAudioDMAEngine
 * @constant kAudioDMAEngineRunning The IOAudioDMAEngine is currently running - it is transferring data to or 
 *           from the device.
 * @constant kAudioDMAEngineStopped The IOAudioDMAEngine is currently stopped - no activity is occurring.
 */

typedef enum _IOAudioDMAEngineState
{
    kAudioDMAEngineRunning = 0,
    kAudioDMAEngineStopped
} IOAudioDMAEngineState;

/*!
 * @typedef IOAudioDMAEnginePosition
 * @abstract Represents a position in an audio DMA engine based on the block within a loop, and the loop count
 * @field fSampleIndex The sample index within the buffer
 * @field fLoopCount The number of times the ring buffer has looped
 */
typedef struct _IOAudioDMAEnginePosition
{
    UInt32	fSampleFrame;
    UInt32	fLoopCount;
} IOAudioDMAEnginePosition;

/*!
 * @class IOAudioDMAEngine
 * @abstract Abstract base class for a single audio DMA/IO engine.
 * @discussion An audio DMA engine is defined by a single stream and an I/O engine to transfer data to
 *  or from that buffer.  An IOAudioDMAEngine implementation may be used for both input and output, however
 *  an IOAudioDMAEngine instance is only one or the other.
 *
 *  All of the DMA engine attributes are represented by the IOAudioDMAEngine instance: sampleRate, bitDepth, numChannels, direction, buffer size.
 *
 *  The sample buffer is broken up into multiple blocks and has its size specified by the number of blocks and
 *  the block size.
 *
 *  The IOAudioDMAEngine superclass provides a shared status buffer that contains all of the dynamic pieces
 *  of information about the audio engine.   It provides a shared mix buffer used by the Audio Device API
 *  to maintain precision as it mixes multiple audio sources.  The IOAudioDMAEngine superclass runs an erase
 *  head that zeros the sample and mix buffers on any DMA engine that requires it (typically output DMA engines).
 *  It also tracks all clients of the DMA engine and starts and stops the DMA engine as needed.
 *
 *  In order for an audio device to play back or record sound, an IOAudioDMAEngine subclass must be created.
 *  The subclass must initialize all of the necessary hardware resources to prepare for starting the
 *  audio I/O engine.  It typically will perform these tasks in the start() method.  One note is that
 *  the start() method simply tells the IOAudioDMAEngine to get ready to start the I/O engine.  There
 *  are separate startDMAEngine() and stopDMAEngine() methods that the subclass must implement to actually start
 *  the I/O.  A subclass may also implement a stop() method which is called as the driver is being torn
 *  down.  This is typically called in preparation of removing the device from the system for removable
 *  devices.
 *
 *  The following fields in the shared IOAudioDMAEngineStatus struct must be maintained by the subclass
 *  implementation:
 *  <pre>
 *  <t>  fCurrentBlock - the current block being transferred by the audio hardware
 *  <t>  fCurrentLoopCount - the number of times the sample buffer has wrapped around to the beginning
 *  <t>  fLastLoopTime - timestamp of the time that the I/O engine/DAC/ADC looped back to the beginning
 *  </pre>
 *  It is critically important that the fLastLoopTime field be as accurate as possible.  It is the basis for the
 *  entire timer and synchronization mechanism used by the audio system.
 *
 *  Currently, the fCurrentBlock field must be updated by the driver to reflect the current block of the audio engine.
 *  It was originally intended that the DMA engine for the audio device update this field directly.  However,
 *  it is entirely possible that the DMA engine doesn't have the capabilities to do this.  The only way to keep
 *  it updated would be either to throw more interrupts or set up a timer that runs more often.  Neither of those
 *  are very good solutions as they require more work to be done in the kernel.  We are working to remove the
 *  Audio Device API's need for this field and plan to remove it altogether in a future release.  For now,
 *  if the DMA engine doesn't support updating a field like this, it is recommended that the sample buffer
 *  be configured with a single block whose size is the entire buffer.  That way, there is no need to update
 *  the fCurrentBlock field.  It will always be 0.
 *
 *  All of the other fields are maintained by the IOAudioDMAEngine superclass implementation and get set through the
 *  various set...() and _set...() methods provided.  A subclass must make calls to the following set methods
 *  in order to properly configure the DMA engine:
 *  <pre>
 *  <t>    _setNumBlocks()
 *  <t>    _setBlockSize()
 *  <t>    _setSampleRate()
 *  <t>    _setBitDepth()
 *  <t>    _setNumChannels()
 *  <t>    _setDirection()
 *  </pre>
 *  These will typically be done in the init() method and are responsible for the initial configuration and state
 *  of the IOAudioDMAEngine instance.  One note about the naming of the set and get methods: there are a number of set
 *  methods that begin with an underscore.  This indicates that the method simply updates the internal state of
 *  the IOAudioDMAEngine.  Those methods do not actually perform the change - they simply reflect that the change has
 *  been made.  For example _setSampleRate() does not change the sample rate.  It only records that the sample
 *  rate has been changed.
 *
 *  There is an additional set method: _setState().  This is called to reflect that the DMA engine is in a new state.
 *  By default, a newly created IOAudioDMAEngine instance is in the kAudioDMAEngineStopped state.  When the DMA engine
 *  is told to start (through startDMAEngine()), it must change the state to kAudioDMAEngineRunning when it has
 *  finished starting the I/O engine.  This is accomplished through a call to the _setState() method.  When the
 *  state of the DMA engine is being investigated and changed (like what must be done in startDMAEngine() and stopDMAEngine()),
 *  there is a lock that must be held to ensure that the state doesn't change.  The lock is the stateLock instance
 *  variable.
 *
 *  A typical implementation of startDMAEngine() might look like the following:
 *  <pre>
 *  <t>   IOReturn MyAudioDMAEngineSubclass::startDMAEngine()
 *  <t>   {
 *  <t>     assert(stateLock);
 *  <t>     IORecursiveLockLock(stateLock);
 *
 *  <t>     if (getState() == kIOAudioDMAEngineStopped) {
 *  <t>        // Start I/O engine here
 *  <t>        // Enable any interrupts for this DMA engine
 *  <t>        // the interrupt should be triggered at the start of the sample buffer
 *  <t>        // the interrupt handler should increment the fCurrentLoopCount and fLastLoopTime fields
 *
 *  <t>        _setState(kAudioDMAEngineRunning);
 *  <t>      }
 *
 *  <t>      IORecursiveLockUnlock(stateLock);
 *
 *  <t>      return kIOReturnSuccess;
 *  <t>    }
 *  </pre>
 *  Be very careful that the method doesn't return without releasing the lock.  Failure to do so will cause a
 *  potential deadlock situation.
 *
 *  The stopDMAEngine() implementation would look similar.
 */

class IOAudioDMAEngine : public IOService
{
    OSDeclareAbstractStructors(IOAudioDMAEngine)
    
    friend class IOAudioDMAEngineUserClient;
    friend class IOAudioDevice;

public:
    static const OSSymbol	*gSampleRateWholeNumberKey;
    static const OSSymbol	*gSampleRateFractionKey;
    
    static void initKeys();
    
    static OSDictionary *createDictionaryFromSampleRate(const IOAudioSampleRate *sampleRate, OSDictionary *rateDict = 0);
    static IOAudioSampleRate *createSampleRateFromDictionary(const OSDictionary *rateDict, IOAudioSampleRate *sampleRate = 0);

    UInt32			numSampleFramesPerBuffer;
    UInt32			minSampleFramesPerBuffer;
    UInt32			maxSampleFramesPerBuffer;
    
    /*! @var sampleRate The current sample rate of the DMA engine in samples per second */
    IOAudioSampleRate			sampleRate;

    /*! @var numErasesPerBuffer The number of times the erase head get scheduled to run for each cycle of the DMA engine */
    UInt32			numErasesPerBuffer;
    bool			runEraseHead;
    
    IOAudioDMAEnginePosition	dmaEngineStopPosition;

    bool			isRegistered;
    
    /*! @var state The current state of the IOAudioDMAEngine - running, stopped*/
    IOAudioDMAEngineState		state;

    /*! @var status Represents the current status of the audio I/O engine */
    IOAudioDMAEngineStatus *	status;

    /*! @var audioDevice The IOAudioDevice instance to which the IOAudioDMAEngine belongs */
    IOAudioDevice *		audioDevice;
    
    IOWorkLoop 			*workLoop;
    IOCommandGate		*commandGate;

    OSSet 			*inputStreams;
    OSSet			*outputStreams;
    OSSet			*userClients;

    /*!
     * @var deviceStartedDMAEngine Used by the IOAudioDevice to determine responsibility for shutting
     *  the DMA engine down when it is no longer needed.
     */
    bool			deviceStartedDMAEngine;

    virtual void _setNumSampleFramesPerBuffer(UInt32 numSampleFrames);
    virtual UInt32 getNumSampleFramesPerBuffer();

    /*!
     * @function _setState
     * @abstract Used to indicate that the DMA engine is in the specified state
     * @discussion This method simply sets the internal state of the DMA engine to the specified state.  It does not
     *  affect a change to the state.  It does however keep other internal state-related attributes consistent.
     *  For example, it enables or disables the timer as needed when the state changes to running or stopped.
     * @param newState The state the DMA engine is in
     * @result Returns the old state
     */
    virtual IOAudioDMAEngineState _setState(IOAudioDMAEngineState newState);

    /*!
     * @function _setSampleRate
     * @abstract Records the sample rate of the DMA engine
     * @discussion  This method must be called during initialization of a new DMA engine to record the DMA engine's
     *  initial sample rate.  It also is intended to be used to record changes to the sample rate during use.
     *  Currently changing sample rates after the DMA engine has been started is not supported.
     *  It may require that the sample buffers be re-sized.  This will be available in an upcoming release.
     * @param newSampleRate The sample rate of the DMA engine in samples per second
     */
    virtual void _setSampleRate(const IOAudioSampleRate *newSampleRate);

    /*!
     * @function _setSampleLatency
     * @abstract Sets the sample latency for the DMA engine
     * @discussion The sample latency represents the number of samples ahead of the playback head
     *  that it is safe to write into the sample buffer.  The audio device API will never write
     *  closer to the playback head than the number of samples specified.  For input DMA engines
     *  the number of samples is behind the record head.
     */
    virtual void _setSampleLatency(UInt32 numSamples);

    /*!
     * @function _setErases
     * @abstract Tells the DMA engine whether or not to run the erase head
     * @discussion By default, output DMA engines run the erase head and input DMA engines do not.  This method can
     *  be called after _setDirection() is called in order to change the default behavior.
     * @param runEraseHead The DMA engine will run the erase head if this value is true
     */
    virtual void _setRunEraseHead(bool runEraseHead);

    /*!
     * @function clientClosed
     * @abstract Called automatically when a user client closes its connection to the DMA engine
     * @discussion This method decrements the number of connections to the DMA engine and if they reach
     *  zero, the DMA engine is called with a call to stopDMAEngine().  This method should not be called directly.
     * @param client The user client that has disconnected.
     */
    virtual void clientClosed(IOAudioDMAEngineUserClient *client);

    /*!
     * @function addTimer
     * @abstract Enables the timer event for the DMA engine
     * @discussion There is a timer event needed by the IOAudioDMAEngine for processing the erase head
     *  and performing flushing operations. When the timer fires, the method timerFired() is ultimately
     *  called which in turn calls performErase() and performFlush().  This is called automatically
     *  to enable the timer event for this DMA engine.  It is called by _setState() when the DMA engine state
     *  is set to kAudioDMAEngineRunning.  When the timer is no longer needed, removeTimer() is called.
     *  There is no need to call this directly.  
     */
    virtual void addTimer();

    /*!
     * @function removeTimer
     * @abstract Disables the timer event for the DMA engine
     * @discussion  This method is called automatically to disable the timer event for this DMA engine.
     *  There is need to call it directly.  This method is called by _setState() when the DMA engine state
     *  is changed from kAudioDMAEngineRunning to one of the stopped states.
     */
    virtual void removeTimer();

public:
    /*!
     * @function init
     * @abstract Performs initialization of a newly allocated IOAudioDMAEngine.
     * @discussion This method is responsible for initialization of all of the general attributes of
     *  a new IOAudioDMAEngine.  It allocates all of the locks, initializes instance variables to their default
     *  values and allocates the shared status buffer.  Subclasses will likely want to override this method
     *  and do all of their common initialization in their implementation.  They do need to be sure to call
     *  IOAudioDMAEngine's implementation of init and pay attention to the return value.
     * @param properties The default properties for the IOAudioDMAEngine
     * @result Returns true if successful
     */
    virtual bool init(OSDictionary *properties);

    /*!
     * @function free
     * @abstract Frees all of the resources allocated by the IOAudioDMAEngine.
     * @discussion Do not call this directly.  This is called automatically by the system when the instance's
     *  refcount goes to 0.  To decrement the refcount, call release() on the object.
     */
    virtual void free();

    /*!
     * @function start
     * @abstract A simple cover method for start(IOService *, IOAudioDevice *) that assumes the provider
     *  is the IOAudioDevice.
     * @discussion Subclasses will want to override start(IOService *, IOAudioDevice *) rather than this
     *  one.
     * @param provider The service provider for the IOAudioDMAEngine (the IOAudioDevice in this case)
     * @result Returns true if the IOAudioDMAEngine was successfully started.
     */
    virtual bool start(IOService *provider);

    /*!
     * @function start
     * @abstract Standard IOKit start() routine called to start an IOService
     * @discussion This method is called in order to prepare the IOAudioDMAEngine for use.  It does NOT
     *  mean that the audio I/O engine itself should be started.  This implementation verifies that the
     *  IOAudioDevice is present and creates the streams.  The methods _setBlockSize() and
     *  _setNumBlocks() must be called before this method so that the sample buffer size is known.
     *  Subclasses will definitely want to override this method and call the superclass' implementation.
     *  It is in their implementation that they should prepare the I/O engine to be started.  Subclasses
     *  must pay attention to the return of the superclass' implementation and return false if the superclass does.
     * @param provider The service provider for the IOAudioDMAEngine
     * @param device The IOAudioDevice that owns this DMA engine
     * @result Returns true if the IOAudioDMAEngine was successfully started.
     */
    virtual bool start(IOService *provider, IOAudioDevice *device);
    
    virtual IOWorkLoop *getWorkLoop();
    virtual IOCommandGate *getCommandGate();
    
    virtual bool initHardware(IOService *provider);

    /*!
     * @function stop
     * @abstract Called to stop the service and prepare for the driver to be terminated.
     * @discussion This method is called before the driver is terminated and usually means that the device
     *  has been removed from the system.
     * @param provider The service provider for the IOAudioDMAEngine
     */
    virtual void stop(IOService *provider);

    virtual void registerService(IOOptionBits options = 0);

    /*!
     * @function newUserClient
     * @abstract Called to request a new user client object for this service
     * @discussion This method is called automatically by IOKit when a user process attempts
     *  to connect to this service.  It allocates a new IOAudioDMAEngineUserClient object and increments
     *  the number of connections for this DMA engine.  If this is the first user client for this IOAudioDMAEngine,
     *  it calls startDMAEngine().  There is no need to call this method directly.
     * @param task The task requesting the new user client
     * @param securityID Optional security paramater passed in by the client - ignored
     * @param type Optional user client type passed in by the client - ignored
     * @param handler The new IOUserClient * must be stored in this param on a successful completion
     * @result Returns kIOReturnSuccess on success.  May also result kIOReturnError or kIOReturnNoMemory.
     */
    virtual IOReturn newUserClient(task_t task, void *securityID, UInt32 type, IOUserClient **handler);


    virtual bool addAudioStream(IOAudioStream *stream);
    virtual void removeAudioStream(IOAudioStream *stream);
    
    virtual void resetStatusBuffer();
    virtual void clearAllSampleBuffers();
    
    static IOReturn addUserClientAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4);
    static IOReturn removeUserClientAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4);
    static IOReturn detachUserClientsAction(OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4);
    
    virtual IOReturn addUserClient(IOAudioDMAEngineUserClient *newUserClient);
    virtual IOReturn removeUserClient(IOAudioDMAEngineUserClient *userClient);
    virtual IOReturn detachUserClients();
    
    virtual void detachAudioStreams();
    
    virtual UInt32 getCurrentSampleFrame() = 0;

    /*!
     * @function startDMAEngine
     * @abstract Called to start the audio I/O engine
     * @discussion This method is called automatically when the DMA engine is placed into use the first time.
     *  This must be overridden by the subclass.  No call to the superclass' implementation is
     *  necessary.  The subclass' implementation must start up the audio I/O engine.  This includes any DMA
     *  engine that needs to be started as well as any interrupts that need to be enabled.  Upon successfully
     *  starting the engine, the subclass' implementation must call _setState(kAudioDMAEngineRunning).  If
     *  it has also checked the state using getState() earlier in the implementation, the stateLock must be
     *  acquired for the entire initialization process (using IORecursiveLockLock(stateLock) and
     *  IORecursiveLockUnlock(stateLock)) to ensure that the state remains consistent.  See the general class
     *  comments for an example.
     * @result Must return kIOReturnSuccess on a successful start of the engine.
     */
    virtual IOReturn startDMAEngine();

    /*!
     * @function stopDMAEngine
     * @abstract Called to stop the audio I/O engine
     * @discussion This method is called automatically when the last client disconnects from this DMA engine.
     *  It must be overridden by the subclass.  No call to the superclass' implementation is necessary.
     *  The subclass' implementation must stop the audio I/O engine.  The DMA engine (if it exists) should
     *  be stopped and any interrupts disabled.  Upon successfully stopping the engine, the subclass must call
     *  _setState(kAudioDMAEngineStopped).  If it has also checked the state using getState() earlier in the
     *  implementation, the stateLock must be acquired for the entire initialization process (using
     *  IORecursiveLockLock(stateLock) and IORecursiveLockUnlock(stateLock)) to ensure that the state remains
     *  consistent.
     * @result Must return kIOReturnSuccess on a successful stop of the engine.
     */
    virtual IOReturn stopDMAEngine();

    /*! 
     * @function getState
     * @abstract Returns the current state of the IOAudioDMAEngine
     * @discussion If this method is called in preparation for calling _setState(), the stateLock must
     *  be acquired before the first call to getState() and held until after the last call to _setState().
     *  Be careful not to return from the code acquiring the lock while the lock is being held.  That
     *  will cause a deadlock situation.
     * @result The current state of the IOAudioDMAEngine: kAudioDMAEngineRunning, kAudioDMAEngineStopped
     */
    virtual IOAudioDMAEngineState getState();

    /*!
     * @function getSampleRate Returns the sample rate of the IOAudioDMAEngine in samples per second
     */
    virtual const IOAudioSampleRate *getSampleRate();

    /*!
     * @function getErases Returns true if the DMA engine will run the erase head when the DMA engine is running.
     */
    virtual bool getRunEraseHead();

    /*!
     * @function getStatus Returns a pointer to the shared status buffer
     */
    virtual const IOAudioDMAEngineStatus *getStatus();

    /*!
     * @function timerCallback
     * @abstract A static method used as a callback for the IOAudioDevice timer services
     * @discussion This method implements the IOAudioDevice::TimerEvent type.
     * @param arg1 The IOAudioDMAEngine that is the target of the event
     * @param device The IOAudioDevice that sent the timer event
     */
    static void timerCallback(OSObject *arg1, IOAudioDevice *device);

    /*!
     * @function timerFired
     * @abstract Called by timerCallback to indicate the timer has fired
     * @discussion This method calls performErase() and performFlush() to do erase head processing and
     *  DMA engine flushing each time the timer event fires.
     */
    virtual void timerFired();

    /*!
     * @function getTimerInterval
     * @abstract Called to get the timer interval for use by the timer event
     * @discussion This method is called each timer the timer event is enabled through addTimer().  The default
     *  implementation is set to return a value such that the timer event runs n times each cycle of the audio
     *  engine through the sample buffer.  The value n is stored as the instance variable: numErasesPerBuffer.
     *  The default value of numErasesPerBuffer is set to IOAUDIODMAENGINE_DEFAULT_NUM_ERASES_PER_BUFFER which is 4.
     *  A subclass may change the value of numErasesPerBuffer or override getTimerInterval.  If it is overridden,
     *  the subclass should call the superclass' implementation, compare its interval with the superclass' and
     *  return the smaller of the two.
     * @result Returns the interval for the timer event
     */
    virtual AbsoluteTime getTimerInterval();

    /*!
     * @function performErase
     * @abstract Called to perform erase head processing
     * @discussion This method is called automatically each time the timer event fires and erases the sample
     *  buffer and mix buffer from the previous location up to the current location of the audio engine.
     */
    virtual void performErase();

    /*!
     * @function performFlush
     * @abstract Called to perform the flush operation
     * @discussion This method is called automatically each time the timer event fires.  It stops the DMA engine
     *  if there are no more clients and the audio engine is passed the latest flush ending position.
     */
    virtual void performFlush();
    
    virtual IOReturn mixAndClip(const void *sourceBuf, void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream);
    virtual IOReturn clipToOutputStream(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream);
    virtual IOReturn convertFromInputStream(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream);
    virtual IOReturn convertFromInputStream_NoWrap(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream);

    virtual IOReturn performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate);
    
    virtual void sendFormatChangeNotification(IOAudioStream *audioStream);

    virtual bool addDefaultAudioControl(IOAudioControl *defaultAudioControl);
    virtual void removeDefaultAudioControl(IOAudioControl *defaultAudioControl);
};

#endif /* _IOKIT_IOAUDIODMAENGINE_H */
