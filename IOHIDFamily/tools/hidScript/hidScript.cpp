//
//  luabindingtest.cpp
//  M8
//
//  Created by YG on 7/20/15.
//  Copyright © 2015 Yevgen Goryachok. All rights reserved.
//

#include <iostream>
#include <vector>
#include <thread>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOMessage.h>
#include <IOKit/hid/IOHIDUserDevice.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <unistd.h>
#include <os/assumes.h>
#include <os/variant_private.h>

#include <AssertMacros.h>
extern "C"  {
  #include "lua/src/lua.h"
  #include "lua/src/lauxlib.h"
  #include "lua/src/lualib.h"
  #include "IOHIDReportDescriptorParser.h"
}

#include "LuaBridge/LuaBridge.h"

bool mVerbose = false;

#define LOG(fmt, ...) if (mVerbose) printf (fmt, ##__VA_ARGS__)

void usage ();
void lua_usleep (int us);
void lua_StartRunLoop ();
void lua_StopRunLoop ();
void lua_TimedRunLoop (double seconds);


void lua_report_errors(lua_State *L);


class HIDUserDevice {

protected:
    
    IOHIDUserDeviceRef    device_;
    std::vector<uint8_t>  report_;
    std::mutex            report_guard_;
    CFDictionaryRef       deviceProperty_;
    luabridge::LuaRef     setReportCallback_;
    luabridge::LuaRef     getReportCallback_;
    lua_State             *L_;
    
protected:
  
public:
    HIDUserDevice (lua_State* L): device_(NULL), setReportCallback_(L), getReportCallback_(L) {
    
      L_ = L;
      
      luabridge::LuaRef val = luabridge::LuaRef::fromStack(L, 2);
      
      std::vector<uint8_t>  descriptor;
      std::string           properties;
      uint32_t              options = 0;
      
      if (val.isString()) {
          properties = val.cast<std::string>();
      }
      
      val = luabridge::LuaRef::fromStack(L, 3);
      if (val.isTable()) {
          for (int i = 1 ; i <= val.length(); i++) {
              descriptor.push_back(val[i].cast<uint8_t>());
          }
      }
      
      val = luabridge::LuaRef::fromStack(L, 4);
      if (val.isNumber()) {
          options = val.cast<uint32_t>();
      }
      
      createDevice (properties, descriptor, options);
    
      LOG ("---------------------------------------------------------\n");
      LOG ("Create HIDUserDevice (%p) device Ref:%p\n", this,  device_);
      LOG ("---------------------------------------------------------\n");
      LOG ("Property:\n");
      LOG ("%s\n", properties.c_str());
      LOG ("---------------------------------------------------------\n");
      if (mVerbose) {
          PrintHIDDescriptor (descriptor.data(), (uint32_t)descriptor.size());
      }
      LOG ("---------------------------------------------------------\n");
   }

    virtual ~HIDUserDevice () {
        LOG ("Destroy HIDUserDevice (%p)\n", this);
        if (device_) {
            CFRelease(device_);
        }
    }

    
    
    void setSetReportCallback (lua_State* L) {
        setReportCallback_ = luabridge::LuaRef::fromStack(L, 2);
        os_assert(setReportCallback_.isFunction(), "setSetReportCallback: Lua object type:%d , expected function", setReportCallback_.type());
        if (device_) {
            IOHIDUserDeviceRegisterSetReportCallback(device_, HandleSetReportCallbackStatic, this);
        }
    }

    void setGetReportCallback (lua_State* L) {
        getReportCallback_ = luabridge::LuaRef::fromStack(L, 2);
        os_assert(getReportCallback_.isFunction(), "setGetReportCallback: Lua object type:%d , expected function", getReportCallback_.type());
        if (device_) {
            IOHIDUserDeviceRegisterGetReportWithReturnLengthCallback (device_, HandleGetReportCallbackStatic, this);
        }
    }

    void scheduleWithRunloop (lua_State* L __unused) {
        if (device_) {
            IOHIDUserDeviceScheduleWithRunLoop(device_, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
        }
    }

    void unscheduleFromRunloop (lua_State* L __unused) {
        if (device_) {
            IOHIDUserDeviceUnscheduleFromRunLoop(device_, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
        }
    }

    
    int sendReport (lua_State* L) {
        if (device_ == NULL) {
            return kIOReturnDeviceError;
        }
        IOReturn status = kIOReturnBadArgument;
        luabridge::LuaRef report = luabridge::LuaRef::fromStack(L, 2);
        if (report.isTable()) {
            std::lock_guard<std::mutex> lock(report_guard_);
            report_.clear();
            for (int i = 1 ; i <= report.length(); i++) {
                report_.push_back(report[i].cast<uint8_t>());
            }
            if (report_.size()) {
                status = IOHIDUserDeviceHandleReport(device_, report_.data(), report_.size());
                if (status != kIOReturnSuccess) {
                    LOG ("IOHIDUserDeviceHandleReport:0x%x\n", status);
                }
            }
        }
        return status;
    }
    
    static IOReturn HandleSetReportCallbackStatic (void * _Nullable refcon, IOHIDReportType type, uint32_t reportID, uint8_t * report, CFIndex reportLength) {
        HIDUserDevice * self = static_cast<HIDUserDevice *>(refcon);
        return self->handleSetReportCallback(type, reportID , report, reportLength);
    }

    int handleSetReportCallback (IOHIDReportType type , uint32_t reportID,  uint8_t * report, CFIndex reportLength) {
        LOG ("HIDUserDevice::handleSetReportCallback: type:%d reportID:%d reportLength:%ld \n", type, reportID, reportLength);
        luabridge::LuaRef luaReportData  = luabridge::newTable (L_);
        for (CFIndex index = 0; index < reportLength; index++) {
            luaReportData [index + 1] = report[index];
        }
        setReportCallback_ ((int)type, (int)reportID, luaReportData);
        return kIOReturnSuccess;
    }

    static IOReturn HandleGetReportCallbackStatic (void * _Nullable refcon, IOHIDReportType type, uint32_t reportID, uint8_t * report, CFIndex * reportLength) {
        HIDUserDevice * self = static_cast<HIDUserDevice *>(refcon);
        return self->handleGetReportCallback(type, reportID , report, reportLength);
    }

    int handleGetReportCallback (IOHIDReportType type , uint32_t reportID,  uint8_t * report, CFIndex * reportLength) {
        luabridge::LuaRef luaReportData  = luabridge::newTable (L_);
        getReportCallback_ ((int)type, (int)reportID, luaReportData);
        *reportLength = luaReportData.length();
        LOG ("handleGetReportCallback: type:%d reportID:%d report:%p reportLength:%ld \n", type, reportID, report, *reportLength );
        for (CFIndex index = 0; index < *reportLength ; index++) {
            report[index] = luaReportData[index + 1].cast<uint8_t>();
        }
        return kIOReturnSuccess;
    }

    int waitForEventService (lua_State* L) {
        IONotificationPortRef port = NULL;
        io_service_t service = IO_OBJECT_NULL;
        io_object_t  notification = IO_OBJECT_NULL;
        kern_return_t status;
        IOServiceInterestCallback callback;
        dispatch_semaphore_t semphore = NULL;
        CFTypeRef value = NULL;
        
        int result = -1;
        
        luabridge::LuaRef timeout = luabridge::LuaRef::fromStack(L, 2);
        if (!timeout.isNumber()) {
            return result;
        }
        
        require(device_, exit);
        
        port = IONotificationPortCreate(kIOMasterPortDefault);
        require(port, exit);
        
        IONotificationPortSetDispatchQueue (port, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
        
        service = IOHIDUserDeviceCopyService(device_);
        require(service, exit);
        
        semphore = dispatch_semaphore_create(0);
        
        callback = [](void * refcon, io_service_t service __unused , uint32_t  messageType, void * messageArgument __unused) -> void {
            if (kIOMessageServicePropertyChange != messageType) {
                return;
            }

            CFTypeRef value = IORegistryEntryCreateCFProperty(service, CFSTR (kIOHIDDeviceOpenedByEventSystemKey), kCFAllocatorDefault, 0);
            if (value && CFEqual(value, kCFBooleanTrue)) {
                dispatch_semaphore_t semphore =  (dispatch_semaphore_t) refcon;
                dispatch_semaphore_signal(semphore);
            }
        };
        
        status = IOServiceAddInterestNotification (port,
                                                   service,
                                                   kIOGeneralInterest,
                                                   callback,
                                                   semphore,
                                                   &notification
                                                   );
        if (status) {
            LOG("ERROR: IOServiceAddInterestNotification:%x\n", status);
            exit(1);
        }
        
        value = IORegistryEntryCreateCFProperty(service, CFSTR (kIOHIDDeviceOpenedByEventSystemKey), kCFAllocatorDefault, 0);
        if (value && CFEqual(value, kCFBooleanTrue)) {
            LOG("Device already opened by event system\n");
            dispatch_semaphore_signal(semphore);
        }
        
        result = (int) dispatch_semaphore_wait (semphore, dispatch_time(DISPATCH_TIME_NOW, timeout.cast<int>() * USEC_PER_SEC));
        
    exit:
        
        if (value) {
            CFRelease(value);
        }
        if (port) {
            IONotificationPortSetDispatchQueue (port, NULL);
            IONotificationPortDestroy(port);
            dispatch_barrier_sync(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{});
        }
        if (notification) {
            IOObjectRelease(notification);
        }
        if (service) {
            IOObjectRelease(service);
        }
        if (semphore) {
            dispatch_release(semphore);
        }
        return result;
    }
    
    void createDevice (std::string & properties, std::vector<uint8_t> & descriptor, uint32_t options) {
        CFDataRef   descriptorData    = NULL;
        CFPropertyListRef propertiesDict  = NULL;
        CFDataRef   propertyData = CFDataCreate(kCFAllocatorDefault, (const uint8_t*)properties.data(), properties.length());
        require(propertyData, finish);

        propertiesDict = CFPropertyListCreateWithData(kCFAllocatorDefault, propertyData, kCFPropertyListMutableContainers, NULL, NULL);
        CFRelease(propertyData);
        require(propertiesDict, finish);
        
        descriptorData = CFDataCreate(kCFAllocatorDefault, descriptor.data(), descriptor.size());
        require(descriptorData, finish);
        
        CFDictionarySetValue((CFMutableDictionaryRef)propertiesDict, CFSTR(kIOHIDReportDescriptorKey), (const void*)descriptorData);
        CFRelease(descriptorData);
        
        device_ = IOHIDUserDeviceCreateWithOptions(kCFAllocatorDefault, (CFDictionaryRef)propertiesDict, options);
        require_action(device_, finish, printf ("ERROR!IOHIDUserDeviceCreate=NULL\n"));

    finish:
        
        if (propertiesDict) {
            CFRelease(propertiesDict);
        }
    }
};

void lua_report_errors(lua_State *L) {
    printf("-- %s\n", lua_tostring(L, -1));
    lua_pop(L, 1); // remove error message
}

void usage () {
    printf("\n");
    printf("usage: hidScript [-v] -s file -- [script arguments] \n\n");
    printf("\t-s    <file>\t: lua scrip file\n");
    printf("\t-v          \t: verbose\n");
    printf("\texample:\n");
    printf("\t  sudo hidScript -s test.lua -- arg1 arg2 arg3 \n");

}

void lua_usleep (int us) {
    usleep(us);
}

void lua_StartRunLoop () {
    CFRunLoopRun();
}

void lua_StopRunLoop () {
    CFRunLoopStop(CFRunLoopGetMain());
}

void lua_TimedRunLoop (double seconds) {
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, seconds, false);
}

int main(int argc, const char * argv[]) {
    int status;
    char * scriptFile = NULL;
    int oc;
    
    if(!os_variant_allows_internal_security_policies(NULL)) {
        std::cerr << argv[0] << " is not allowed to run with the current security policies."
                  << std::endl;
        return EXIT_FAILURE;
    }
  
    while ((oc = getopt(argc, (char**)argv, "hvs:")) != -1) {
        switch (oc) {
            case 'h':
                /* handle -h */
                usage();
                exit(0);
                break;
            case 'v':
                /* handle -v */
                mVerbose = true;
                break;
            case 's':
                /* handle -s, get arg value from optarg */
                scriptFile = optarg;
                break;
            default:
                usage ();
                exit(1);
            }
    }
    
    if (scriptFile == NULL) {
        usage ();
        exit(1);
    }
    // create a Lua state
    lua_State* L = luaL_newstate();

    // load standard libs
    luaL_openlibs(L);

    luabridge::LuaRef lua_arg  = luabridge::newTable (L);
    //lua_argv[0] = scriptFile;
    for (int index = optind; index < argc; index++) {
        lua_arg [index - optind] = argv[index];
    }
  
    luabridge::setGlobal(L, lua_arg , "arg");
  
    luabridge::getGlobalNamespace(L)
        .beginNamespace("util")
          .addFunction ("usleep", lua_usleep)
          .addFunction ("StartRunLoop", lua_StartRunLoop)
          .addFunction ("StopRunLoop", lua_StopRunLoop)
          .addFunction ("TimedRunLoop", lua_TimedRunLoop)
        .endNamespace()
        .beginClass<HIDUserDevice>("HIDUserDevice")
            .addConstructor<void(*) (lua_State*)>()
            .addFunction("SendReport", &HIDUserDevice::sendReport)
            .addFunction("SetSetReportCallback", &HIDUserDevice::setSetReportCallback)
            .addFunction("SetGetReportCallback", &HIDUserDevice::setGetReportCallback)
            .addFunction("ScheduleWithRunloop", &HIDUserDevice::scheduleWithRunloop)
            .addFunction("UnscheduleFromRunloop", &HIDUserDevice::unscheduleFromRunloop)
            .addFunction("WaitForEventService", &HIDUserDevice::waitForEventService)
        .endClass();
    status = luaL_dofile(L, scriptFile);
    if (status) {
        lua_report_errors (L);
    }
    return status;
}
