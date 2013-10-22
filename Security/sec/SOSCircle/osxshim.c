//
//  osxshim.c
//  sec
//
//  Created by J Osborne on 12/4/12.
//
//

#include <stdbool.h>

typedef void *SOSDataSourceFactoryRef;
typedef void *SOSAccountRef;

// XXX Need to plumb these from security to secd.   If we can.

typedef SOSDataSourceFactoryRef (^AccountDataSourceFactoryBlock)();

bool SOSKeychainAccountSetFactoryForAccount(AccountDataSourceFactoryBlock factory);

bool SOSKeychainAccountSetFactoryForAccount(AccountDataSourceFactoryBlock factory)
{
    return false;
}

SOSAccountRef SOSKeychainAccountGetSharedAccount(void);

SOSAccountRef SOSKeychainAccountGetSharedAccount(void)
{
    return (void*)0;
}
