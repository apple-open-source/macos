/* RBool.m created by epeyton on Sat 15-Jan-2000 */

#import "RBool.h"

@implementation RBool

- (id)initWithBool:(BOOL)yn
{
    _isBool = yn;
    return [super init];
}

- (void)setBool:(BOOL)yn
{
    _isBool = yn;
}

- (BOOL)isBool
{
    return _isBool;
}

- (NSString *)description
{
    return (_isBool?NSLocalizedString(@"Yes", @""):NSLocalizedString(@"No", @""));
}

@end
