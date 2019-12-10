//
//  CKKSPBFileStorage.m
//

#import "keychain/ckks/CKKSPBFileStorage.h"

@interface CKKSPBFileStorage ()
@property NSURL *storageFile;
@property Class<CKKSPBCodable> storageClass;
@property id<CKKSPBCodable> protobufStorage;
@end

@implementation CKKSPBFileStorage

- (CKKSPBFileStorage *)initWithStoragePath:(NSURL *)storageFile
                              storageClass:(Class<CKKSPBCodable>) storageClass
{
    if ((self = [super init]) == nil) {
        return nil;
    }
    self.storageFile = storageFile;
    self.storageClass = storageClass;

    NSData *data = [NSData dataWithContentsOfURL:storageFile];
    if (data != nil) {
        self.protobufStorage = [[self.storageClass alloc] initWithData:data];
    }
    /* if not storage, or storage is corrupted, this function will return a empty storage */
    if (self.protobufStorage == nil) {
        self.protobufStorage = [[self.storageClass alloc] init];
    }

    return self;
}

- (id _Nullable)storage
{
    __block id storage;
    @synchronized (self) {
        storage = self.protobufStorage;
    }
    return storage;
}

- (void)setStorage:(id _Nonnull)storage
{
    @synchronized (self) {
        id<CKKSPBCodable> c = storage;
        NSData *data = c.data;
        [data writeToURL:self.storageFile atomically:YES];
        self.protobufStorage = [[self.storageClass alloc] initWithData:data];
    }
}


@end
