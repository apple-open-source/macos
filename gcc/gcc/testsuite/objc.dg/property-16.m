/* APPLE LOCAL file radar 4660569 */
/* No warning here because accessor methods are INHERITED from NSButton */
/* APPLE LOCAL radar 4899595 */
/* { dg-options "-fno-objc-new-property -mmacosx-version-min=10.5" } */
#include <AppKit/AppKit.h>

@interface NSButton (Properties)
@property NSString *title;
@end

@implementation NSButton (Properties)
@end
