#import <TargetConditionals.h>

#if TARGET_OS_OSX

#import <Cocoa/Cocoa.h>
@interface ViewController : NSViewController
@end

#else

#import <UIKit/UIKit.h>
@interface ViewController : UIViewController
@end

#endif
