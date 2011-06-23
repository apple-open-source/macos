//
//  PortStatusGatherer.h
//  USBProber
//
//  Created by Russvogel on 10/14/10.
//  Copyright 2010 Apple. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "OutlineViewNode.h"


@class PortStatusGatherer;

@protocol PortStatusGathererListener <NSObject>

@end

@interface PortStatusGatherer : NSObject 
{
    id      _listener;

    OutlineViewNode *       _rootNode;
}

- (IOReturn) gatherStatus;

- initWithListener:(id <PortStatusGathererListener>)listener rootNode:(OutlineViewNode *)rootNode;

@end
