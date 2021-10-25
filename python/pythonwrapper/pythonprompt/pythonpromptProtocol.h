//
//  pythonpromptProtocol.h
//  pythonprompt
//
//  Created by Andy Kaplan on 6/4/21.
//

#import <Foundation/Foundation.h>

@protocol pythonpromptProtocol

- (void)prompt:(int)pid withNewBinPath:(NSString *)newBinPath withOldBinPath:(NSString *)oldBinPath withReply:(void (^)(NSString *))reply;
    
@end
