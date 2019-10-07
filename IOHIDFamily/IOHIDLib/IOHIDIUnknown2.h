//
//  IOHIDIUnknown2.h
//  IOHIDLib
//
//  Created by dekom on 11/14/17.
//

#ifndef IOHIDIUnknown_h
#define IOHIDIUnknown_h

#include <IOKit/IOCFPlugIn.h>
#import <Foundation/Foundation.h>

@interface IOHIDIUnknown2 : NSObject {
    IUnknownVTbl *_vtbl;
}

- (HRESULT)queryInterface:(REFIID)uuidBytes
             outInterface:(LPVOID _Nonnull * _Nonnull)outInterface;

@end

@interface IOHIDPlugin : IOHIDIUnknown2 {
    IOCFPlugInInterface *_plugin;
}

- (IOReturn)probe:(NSDictionary * _Nonnull)properties
          service:(io_service_t)service
         outScore:(SInt32 * _Nonnull)outScore;

- (IOReturn)start:(NSDictionary * _Nonnull)properties
          service:(io_service_t)service;

- (IOReturn)stop;

@end

#endif /* IOHIDIUnknown_h */
