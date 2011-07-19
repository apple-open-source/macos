//
//  WSLib.h
//  Web Sharing Utility Library
//
//  Created by Al Begley on 4/4/11.
//  Copyright 2011 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

enum {
	WS_STATE_ERROR = 0,
	WS_STATE_STOPPED,
	WS_STATE_RUNNING
};

int WSGetWebSharingState();
int WSSetWebSharingState(int newState);