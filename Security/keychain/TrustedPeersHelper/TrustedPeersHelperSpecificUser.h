
#if OCTAGON

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class CKContainer;

@interface TPSpecificUser : NSObject <NSSecureCoding, NSCopying>
@property (readonly) NSString* cloudkitContainerName;
@property (readonly) NSString* appleAccountID;
@property (readonly) NSString* altDSID;

@property (readonly) NSString* octagonContextID;

// Might be nil if the account is primary (and therefore matches multiple types of persona)
@property (readonly, nullable) NSString* personaUniqueString;
@property (readonly) BOOL isPrimaryAccount;

- (instancetype)initWithCloudkitContainerName:(NSString*)cloudkitContainerName
                             octagonContextID:(NSString*)octagonContextID
                               appleAccountID:(NSString*)appleAccountID
                                      altDSID:(NSString*)altDSID
                             isPrimaryPersona:(BOOL)isPrimaryPersona
                          personaUniqueString:(NSString* _Nullable)personaUniqueString;

- (CKContainer *)makeCKContainer;
@end


NS_ASSUME_NONNULL_END

#endif // Octagon
