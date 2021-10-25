//
//  od.m
//  authd
//
//  Copyright © 2021 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <OpenDirectory/OpenDirectory.h>
#import <opendirectory/odcustomfunctions.h>

#import "od.h"
#import "debugging.h"

AUTHD_DEFINE_LOG

bool odUserHasSecureToken(const char *username)
{
    NSError *localError = nil;
    NSString *recordName = [NSString stringWithUTF8String:username];
    if (!recordName) {
        os_log_error(AUTHD_LOG, "Unable to get username for %s", username);
        return false;
    }
    ODNode *node = [ODNode nodeWithSession:(__bridge ODSession *)(kODSessionDefault) type:kODNodeTypeLocalNodes error:&localError];
    if (node == nil) {
        os_log_error(AUTHD_LOG, "OD node failed with %{public}@", localError);
        return false;
    }
    
    NSDictionary *payload = @{ kODHasSecureTokenRecordnameKey : recordName };
    NSDictionary *result = (NSDictionary *)[node customFunction:kODCustomFunctionHasSecureToken payload:payload error:&localError];
    if ((!result) || localError) {
        os_log_error(AUTHD_LOG, "OD failed with %{public}@", localError);
        return false;
    }

    NSNumber *num = result[kODHasSecureTokenResultKey];
    return  num.boolValue == YES;
}
