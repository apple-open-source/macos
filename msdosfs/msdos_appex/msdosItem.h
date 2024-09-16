/*
 * Copyright (c) 2023-2024 Apple Inc. All rights reserved.
 */

#ifndef msdosItem_h
#define msdosItem_h

#import <Foundation/Foundation.h>
#import <FSKit/FSKit.h>
#import "ExtensionCommon.h"
#import "msdosVolume.h"
#import "direntry.h"
#import "FATItem.h"
#import "DirItem.h"

NS_ASSUME_NONNULL_BEGIN

@interface MsdosDirEntryData : DirEntryData

@property uint64_t dosDirEntryOffsetInDirBlock; /* offset in dir block of the dosdirentry data */
@property uint64_t dosDirEntryDirBlockNum; /* dir block number (in volume) which holds the dosdirentry data */

@end

@interface MsdosDirItem: DirItem

/*
 * Instead of iterate the directory for every shortname looking for the next
 * available generation number, use a single counter for the entire directory.
 * If we wrap around, iterate the directory next time.
 */
@property uint32_t maxShortNameIndex;

@end


@interface MsdosFileItem : FileItem

-(void)waitForWrites;

@end

NS_ASSUME_NONNULL_END
#endif /* msdosItem_h */
