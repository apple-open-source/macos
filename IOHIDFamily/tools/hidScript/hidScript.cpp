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
#include <IOKit/hid/IOHIDUserDevice.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <unistd.h>

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
void lua_report_errors(lua_State *L);


class HIDUserDevice {

protected:
  std::string           property_;
  std::vector<uint8_t>  descriptor_;
  IOHIDUserDeviceRef    device_;
  std::vector<uint8_t>  report_;
  std::mutex            report_guard_;

protected:
  
public:
  HIDUserDevice (lua_State* L): device_(NULL){
    
    luabridge::LuaRef val = luabridge::LuaRef::fromStack(L, 2);
    if (val.isString()) {
      property_ = val.cast<std::string>();
    }
    val = luabridge::LuaRef::fromStack(L, 3);
    if (val.isTable()) {
      for (int i = 1 ; i <= val.length(); i++) {
        descriptor_.push_back(val[i].cast<uint8_t>());
      }
    }
    
    CreateDevice ();
    
    LOG ("---------------------------------------------------------\n");
    LOG ("Create HIDUserDevice (%p) device Ref:%p\n", this,  device_);
    LOG ("---------------------------------------------------------\n");
    LOG ("Property:\n");
    LOG ("%s\n", property_.c_str());
    LOG ("---------------------------------------------------------\n");
    if (mVerbose) {
      PrintHIDDescriptor (descriptor_.data(), descriptor_.size());
    }
    LOG ("---------------------------------------------------------\n");
 
   }

  virtual ~HIDUserDevice () {
    LOG ("Destroy HIDUserDevice (%p)\n", this);
    if (device_) {
      CFRelease(device_);
    }
  }

  int SendReport(lua_State* L) {
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
          LOG ("IOHIDUserDeviceHandleReport = 0x%x\n", status);
        }
      }
    }
    return status;
  }
  
  void CreateDevice () {
    CFDataRef   descriptorData    = NULL;
    CFPropertyListRef properties  = NULL;
    CFDataRef   propertyData = CFDataCreate(kCFAllocatorDefault, (const uint8_t*)property_.data(), property_.length());
    require(propertyData, finish);

    properties = CFPropertyListCreateWithData(kCFAllocatorDefault, propertyData, kCFPropertyListMutableContainers, NULL, NULL);
    CFRelease(propertyData);
    require(properties, finish);
    
    descriptorData = CFDataCreate(kCFAllocatorDefault, descriptor_.data(), descriptor_.size());
    require(descriptorData, finish);
    
    CFDictionarySetValue((CFMutableDictionaryRef)properties, CFSTR(kIOHIDReportDescriptorKey), (const void*)descriptorData);
    CFRelease(descriptorData);
    
    device_ = IOHIDUserDeviceCreate(kCFAllocatorDefault, (CFDictionaryRef)properties);

finish:

    if (properties) {
      CFRelease(properties);
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


int main(int argc, const char * argv[]) {
  int status;
  char * scriptFile = NULL;
  int oc;
  
  while ((oc = getopt(argc, (char**)argv, "hvs:")) != -1) {
    switch (oc) {
    case 'h':
        /* handle -v */
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
        .endNamespace()
        .beginClass<HIDUserDevice>("HIDUserDevice")
            .addConstructor<void(*) (lua_State*)>()
            .addFunction("SendReport", &HIDUserDevice::SendReport)
        .endClass();

  status = luaL_dofile(L, scriptFile);
  if (status) {
    lua_report_errors (L);
  }
  return status;
}
