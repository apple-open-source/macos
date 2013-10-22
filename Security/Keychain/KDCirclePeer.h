//
//  KDCirclePeer.h
//  Security
//
//  Created by J Osborne on 2/25/13.
//
//

#import <Foundation/Foundation.h>

@interface KDCirclePeer : NSObject

@property (readonly) NSString *name;
@property (readonly) NSString *idString;
@property (readonly) id peerObject;

-(id)initWithPeerObject:(id)peerObject;

@end
