//
//  Applicant.h
//  Security
//
//  Created by J Osborne on 3/7/13.
//  Copyright (c) 2013 Apple Inc. All Rights Reserved.
//

#import <Foundation/Foundation.h>
#include "SecureObjectSync/SOSPeerInfo.h"

typedef enum {
    ApplicantWaiting,
    ApplicantOnScreen,
    ApplicantRejected,
    ApplicantAccepted,
} ApplicantUIState;

@interface Applicant : NSObject
@property (readwrite) ApplicantUIState applicantUIState;
@property (readonly) NSString *applicantUIStateName;
@property (readwrite) SOSPeerInfoRef rawPeerInfo;
@property (readonly) NSString *name;
@property (readonly) NSString *idString;
@property (readonly) NSString *deviceType;
-(id)initWithPeerInfo:(SOSPeerInfoRef) peerInfo;
-(NSString *)description;
-(void)dealloc;
@end
