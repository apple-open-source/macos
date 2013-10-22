//
//  SOSTestDataSource.h
//  sec
//
//  Created by Michael Brouwer on 9/28/12.
//
//

#ifndef _SEC_SOSTestDataSource_H_
#define _SEC_SOSTestDataSource_H_

#include <SecureObjectSync/SOSAccount.h>

//
// MARK: Data Source Functions
//
SOSDataSourceRef SOSTestDataSourceCreate(void);

CFMutableDictionaryRef SOSTestDataSourceGetDatabase(SOSDataSourceRef data_source);

SOSMergeResult SOSTestDataSourceAddObject(SOSDataSourceRef data_source, SOSObjectRef object, CFErrorRef *error);
bool SOSTestDataSourceDeleteObject(SOSDataSourceRef data_source, CFDataRef key, CFErrorRef *error);

//
// MARK: Data Source Factory Functions
//

SOSDataSourceFactoryRef SOSTestDataSourceFactoryCreate(void);
void SOSTestDataSourceFactoryAddDataSource(SOSDataSourceFactoryRef factory, CFStringRef name, SOSDataSourceRef ds);

SOSObjectRef SOSDataSourceCreateGenericItemWithData(SOSDataSourceRef ds, CFStringRef account, CFStringRef service, bool is_tomb, CFDataRef data);
SOSObjectRef SOSDataSourceCreateGenericItem(SOSDataSourceRef ds, CFStringRef account, CFStringRef service);

#endif /* _SEC_SOSTestDataSource_H_ */
