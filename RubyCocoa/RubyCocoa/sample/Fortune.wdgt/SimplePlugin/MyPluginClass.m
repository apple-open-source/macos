/* This is the default source file for new Cocoa bundles. */

/* You can either fill in code here or remove this and create or add new files. */

#import <RubyCocoa/RBRuntime.h>

@interface MyPluginClass : NSObject
{}
@end

@implementation MyPluginClass
+ (void) load
{
  static int loaded = 0;
  if (! loaded) {
    RBBundleInit("load_MyPluginClass.rb", self, nil);
    loaded = 1;
  }
}
@end
