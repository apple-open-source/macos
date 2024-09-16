/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
 */

#ifndef DirBlock_h
#define DirBlock_h

#import "DirItem.h"

@interface DirBlock : NSObject

@property uint64_t offsetInVolume; /* Offset within the file system */

-(instancetype)initInDir:(DirItem *)dirItem;

-(void)releaseBlock;

/** Reads the specified dir block number (which is relative to volume start) into the dir block. */
-(NSError *)readDirBlockNum:(uint64_t)dirBlockNumberInVolume;

/** Reads the specified dir block number (which is relative to dir start) into the dir block. */
-(NSError *)readRelativeDirBlockNum:(uint32_t)dirBlockIdxInDir;

/** Returns a pointer to the dir block's data, at the given offset. */
-(void *)getBytesAtOffset:(uint64_t)offsetInDirBlock;

-(NSError *)setBytes:(NSData *)data
            atOffset:(uint64_t)offsetInDirBlock;

/** Write the whole dir block to disk. */
-(NSError *)writeToDisk;

/** Write a part of the dir block to disk. */
-(NSError *)writeToDiskFromOffset:(uint64_t)offsetInDirBlock
						   length:(uint64_t)lengthToWrite;

@end




#endif /* DirBlock_h */
