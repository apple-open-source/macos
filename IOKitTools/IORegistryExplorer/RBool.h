/* RBool.h created by epeyton on Sat 15-Jan-2000 */

#import <AppKit/AppKit.h>

@interface RBool : NSObject
{
    BOOL	_isBool;
}

- (id)initWithBool:(BOOL)yn;
- (void)setBool:(BOOL)yn;
- (BOOL)isBool;

@end
