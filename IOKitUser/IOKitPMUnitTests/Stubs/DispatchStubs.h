//
//  DispatchStubs.h
//  IOKitUser
//
//  Created by Faramola Isiaka on 7/21/21.
//

#ifndef DispatchStubs_h
#define DispatchStubs_h

#include <dispatch/dispatch.h>

dispatch_time_t getCurrentTime(void);
void advanceTime(dispatch_time_t newTime);
void clearTime(void);
void DispatchStubsTeardown(void);

#endif /* DispatchStubs_h */
