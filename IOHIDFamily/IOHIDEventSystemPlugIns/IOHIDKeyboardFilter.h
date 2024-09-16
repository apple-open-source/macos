//
//  IOHIDKeyboardFilter.h
//  IOHIDFamily
//
//  Created by Gopu Bhaskar on 3/10/15.
//
//

#ifndef _IOHIDFamily_IOHIDKeyboardFilter_
#define _IOHIDFamily_IOHIDKeyboardFilter_
#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#if COREFOUNDATION_CFPLUGINCOM_SEPARATE
#include <CoreFoundation/CFPlugInCOM.h>
#endif

#import <Foundation/Foundation.h>

#include <IOKit/hid/IOHIDServiceFilterPlugIn.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <map>
#include <set>

#include "CF.h"
#include "IOHIDUtility.h"


#define kKeyboardOptionMask         ( \
    kIOHIDKeyboardIsRepeat          | \
    kIOHIDKeyboardStickyKeyDown     | \
    kIOHIDKeyboardStickyKeyLocked   | \
    kIOHIDKeyboardStickyKeyUp       | \
    kIOHIDKeyboardStickyKeysOn      | \
    kIOHIDKeyboardStickyKeysOff     )

enum {
    kStickyKeysEnableCount = 5,
    kStickyKeysShiftKeyInterval = 30,
};

enum {
    kStickyKeyState_Reset           = 1 << 0,
    kStickyKeyState_Down            = 1 << 1,
    kStickyKeyState_Locked          = 1 << 2,
    // State where modifier pressed and hold for first time
    kStickyKeyState_Down_Locked     = 1 << 3,
    // State where modifier pressed and hold  and non modifier key was already dispatched at least once.
    kStickyKeyState_Down_Unlocked   = 1 << 4
};

typedef UInt32 StickyKeyState;

#define NX_MODIFIERKEY_COUNT 14

#define MAX_STICKY_KEYS (kHIDUsage_KeyboardRightGUI - kHIDUsage_KeyboardLeftControl + 3)

typedef std::map<Key,Key> KeyMap;

class IOHIDKeyboardFilter;

@interface StickyKeyHandler : NSObject
- (id)initWithFilter:(IOHIDKeyboardFilter *)filter
             service:(IOHIDServiceRef)service;
- (void)removeObserver;
@end

class IOHIDKeyboardFilter
{
public:
    IOHIDKeyboardFilter(CFUUIDRef factoryID);
    ~IOHIDKeyboardFilter();
    HRESULT QueryInterface( REFIID iid, LPVOID *ppv );
    ULONG AddRef();
    ULONG Release();

    SInt32 match(IOHIDServiceRef service, IOOptionBits options);
    IOHIDEventRef filter(IOHIDEventRef event);
    void open(IOHIDServiceRef session, IOOptionBits options);
    void close(IOHIDServiceRef session, IOOptionBits options);
    void registerService(IOHIDServiceRef service);
    void handlePendingStats();
    void scheduleWithDispatchQueue(dispatch_queue_t queue);
    void unscheduleFromDispatchQueue(dispatch_queue_t queue);
    CFTypeRef copyPropertyForClient(CFStringRef key, CFTypeRef client);
    void setPropertyForClient(CFStringRef key, CFTypeRef property, CFTypeRef client);
    void setEventCallback(IOHIDServiceEventCallback callback, void * target, void * refcon);
    void resetStickyKeys();

private:
    static IOHIDServiceFilterPlugInInterface sIOHIDEventSystemStatisticsFtbl;
    IOHIDServiceFilterPlugInInterface *_serviceInterface;
    CFUUIDRef                   _factoryID;
    UInt32                      _refCount;
    SInt32                      _matchScore;

    static IOHIDServiceFilterPlugInInterface sIOHIDKeyboardFilterFtbl;
    static HRESULT QueryInterface( void *self, REFIID iid, LPVOID *ppv );
    static ULONG AddRef( void *self );
    static ULONG Release( void *self );

    static SInt32 match(void * self, IOHIDServiceRef service, IOOptionBits options);
    static IOHIDEventRef filter(void * self, IOHIDEventRef event);

    static void open(void * self, IOHIDServiceRef inService, IOOptionBits options);
    static void close(void * self, IOHIDServiceRef inSession, IOOptionBits options);

    static void scheduleWithDispatchQueue(void * self, dispatch_queue_t queue);
    static void unscheduleFromDispatchQueue(void * self, dispatch_queue_t queue);


    static CFTypeRef copyPropertyForClient(void * self,CFStringRef key,CFTypeRef client);
    static void setPropertyForClient(void * self,CFStringRef key, CFTypeRef property, CFTypeRef client);
    static void defaultEventCallback (void * target, void *  refcon, void * sender, IOHIDEventRef event, IOOptionBits options);

    IOHIDServiceRef           _service;
    IOHIDServiceEventCallback _eventCallback;
    void *                    _eventTarget;
    void *                    _eventContext;
    static void setEventCallback(void * self, IOHIDServiceEventCallback callback, void * target, void * refcon);


    bool _unifiedKeyMapping;
    std::map<Key,KeyAttribute> _activeKeys;
    std::set<Key>   _liftedKeys;
    KeyMap          _modifiersKeyMap;
    KeyMap          _fnFunctionUsageMapKeyMap;
    std::map<uint32_t, KeyMap> *_activeModifiedKeyMaps;
    std::map<uint32_t, KeyMap> _legacyModifiedKeyMaps;
    std::map<uint32_t, KeyMap> _unifiedModifiedKeyMaps;
    KeyMap          _fnKeyboardUsageMapKeyMap;
    KeyMap          _numLockKeyboardUsageMapKeyMap;
    KeyMap          _userKeyMap;
    uint32_t        _fnKeyMode;
    uint32_t        _supportedModifiers;

    bool            isNumLockMode();

    Key             remapKey(Key key);

    IOHIDEventRef   _slowKeysSlowEvent;
    UInt32          _slowKeysDelayMS;

    UInt32          _stickyKeysShiftKeyCount;
    StickyKeyState  _stickyKeyState[MAX_STICKY_KEYS];
    boolean_t       _stickyKeyToggle;
    boolean_t       _stickyKeyOn;
    boolean_t       _stickyKeyDisable;

    // Keyboard Event Repeats
    IOHIDEventRef   _keyRepeatEvent;
    uint64_t        _keyRepeatInitialDelayMS;
    uint64_t        _keyRepeatDelayMS;

    IOHIDEventRef   _delayedCapsLockEvent;
    UInt32          _capsLockDelayMS;
    SInt32          _capsLockDelayOverrideMS;

    uint32_t        _numLockOn;

    IOHIDEventRef   _delayedEjectKeyEvent;
    UInt32          _ejectKeyDelayMS;

    uint32_t        _mouseKeyActivationEnable;
    uint32_t        _mouseKeyActivationCount;

    dispatch_source_t _ejectKeyDelayTimer;

    dispatch_source_t _mouseKeyActivationResetTimer;

    IOPMConnection  _powerConnect;

    dispatch_block_t _pmInitBlock;


    IOHIDEventRef   _delayedLockKeyEvent;
    UInt32          _lockKeyDelayMS;
    dispatch_source_t _lockKeyDelayTimer;

    boolean_t       _capsLockState;
    boolean_t       _capsLockLEDState;
    boolean_t       _capsLockLEDInhibit;
    boolean_t       _capsLockDarkWakeLEDInhibit;
    CFStringRef     _capsLockLED;

    NSNumber        *_restoreState;
    NSNumber        *_locationID;

    StickyKeyHandler *_stickyKeyHandler;

    boolean_t       _doNotDisturbSupported;

    IOHIDEventRef processStickyKeys(IOHIDEventRef event);
    void setStickyKeyState(UInt32 usagePage, UInt32 usage, StickyKeyState state);
    StickyKeyState getStickyKeyState(UInt32 usagePage, UInt32 usage);

    dispatch_queue_t  _queue;
    dispatch_source_t _stickyKeysShiftResetTimer;
    dispatch_source_t _slowKeysTimer;
    dispatch_source_t _keyRepeatTimer;
    dispatch_source_t _capsLockDelayTimer;


    void dispatchStickyKeys(int stateMask);
    UInt32 processStickyKeyUp(UInt32 usagePage, UInt32 usage, UInt32 &flags);
    UInt32 processStickyKeyDown(UInt32 usagePage, UInt32 usage, UInt32 &flags);
    void processStickyKeys(void);
    void processShiftKey(void);
    void updateStickyKeysState(StickyKeyState from, StickyKeyState to);
    void startStickyKey ();
    void stopStickyKey ();

    IOHIDEventRef processSlowKeys(IOHIDEventRef event);
    void dispatchSlowKey(void);
    void resetSlowKey(void);

    IOHIDEventRef processKeyRepeats(IOHIDEventRef event, uint64_t keyRepeatInitialDelayMS, uint64_t keyRepeatDelayMS);
    void dispatchKeyRepeat(void);

    void processCapsLockState(IOHIDEventRef event);
    void setCapsLockState(boolean_t state, CFTypeRef client);
    void updateCapslockLED(CFTypeRef client);

    IOHIDEventRef processCapsLockDelay(IOHIDEventRef event);
    void dispatchCapsLock(void);
    void resetCapsLockDelay(void);
    
    /*!
     * @function allowKeyRemapping
     * @abstract Checks the attempted remapping for restricted characters and if the process can bypass those restrictions
     *
     * @param   property    the property containing the remapping being set
     * @param   client         object representing client which is attempting to remap
     * @result  True if remapping should succeed, false if insufficient permissions should cause remapping failure
     *
     */
    bool allowRemapping(CFTypeRef property, CFTypeRef client);
    bool hasTCCPermissions(IOHIDEventSystemConnectionRef client);

    KeyMap createMapFromArrayOfPairs(CFArrayRef mappings);
    KeyMap createMapFromStringMap(CFStringRef mappings);
    IOHIDEventRef processKeyMappings(IOHIDEventRef event);

    uint32_t getKeyboardID ();
    uint32_t getKeyboardID (uint16_t productID, uint16_t vendorID);

    IOHIDEventRef   processEjectKeyDelay(IOHIDEventRef event);
    void dispatchEjectKey(void);
    void resetEjectKeyDelay(void);
    IOHIDEventRef processMouseKeys(IOHIDEventRef event);
    static kern_return_t setHIDSystemParam(CFStringRef key, uint32_t property);
    //------------------------------------------------------------------------------
    // IOHIDKeyboardFilter::powerNotificationCallback
    //------------------------------------------------------------------------------
    static void powerNotificationCallback (void * refcon, IOPMConnection connection, IOPMConnectionMessageToken token, IOPMCapabilityBits eventDescriptor);
    void powerNotificationCallback (IOPMConnection connection, IOPMConnectionMessageToken token, IOPMCapabilityBits eventDescriptor);

    void setDoNotDisturbState();
    bool isModifiersPressed ();
    uint32_t getActiveModifiers();
    void dispatchLockKey(void);
    void resetLockKeyDelay(void);
    IOHIDEventRef processLockKeyDelay(IOHIDEventRef event);

    void setEjectKeyProperty(uint32_t keyboardID);
    bool isDelayedEvent(IOHIDEventRef event);
    bool isKeyPressed (Key key);
    void serialize (CFMutableDictionaryRef  dict) const;
    CFMutableArrayRefWrap serializeMapper (const KeyMap &mapper) const;
    CFMutableArrayRefWrap serializeModifierMappings(const std::map<uint32_t, KeyMap> &maps) const;

    void processKeyState (IOHIDEventRef event);
    void processModifiedKeyState (IOHIDEventRef event);
    void resetModifiedKeyState();
    void resetModifiedKeyState(KeyMap map);
    void dispatchEventCopy(IOHIDEventRef event);
private:

    IOHIDKeyboardFilter();
    IOHIDKeyboardFilter(const IOHIDKeyboardFilter &);
    IOHIDKeyboardFilter &operator=(const IOHIDKeyboardFilter &);
};


#endif /* defined(_IOHIDFamily_IOHIDKeyboardFilter_) */
