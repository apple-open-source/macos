
#if OCTAGON

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol OTTooManyPeersAdapter
- (BOOL)shouldPopDialog; // Whether we should pop the dialog at all
- (unsigned long)getLimit; // The number of peers needed to pop the dialog
- (void)popDialogWithCount:(unsigned long)count limit:(unsigned long)limit; // actually pop the dialog
@end

@interface OTTooManyPeersActualAdapter : NSObject <OTTooManyPeersAdapter>
@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON
