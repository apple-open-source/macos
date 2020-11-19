#import "ViewController.h"

#if TARGET_OS_OSX

@implementation ViewController
- (void)viewDidLoad {
    [super viewDidLoad];
}
- (void)setRepresentedObject:(id)representedObject {
    [super setRepresentedObject:representedObject];
}
@end

#else

@implementation ViewController
- (void)viewDidLoad {
    [super viewDidLoad];
}
- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
 }
@end

#endif
