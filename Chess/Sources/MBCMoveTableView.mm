//
//  MBCMoveTableView.mm
//  MBChess
//
//  Created by Matthias Neeracher on 10/7/10.
//  Copyright 2010 Apple Computer. All rights reserved.
//

#import "MBCMoveTableView.h"

@implementation MBCMoveTableView

- (id)initWithFrame:(NSRect)frameRect
{
	fInGridDrawing	= false;
	
	return [super initWithFrame:frameRect];
}

- (void)drawGridInClipRect:(NSRect)clipRect
{
	fInGridDrawing	= true;
	[super drawGridInClipRect:clipRect];
	fInGridDrawing	= false;
}

- (NSIndexSet *)columnIndexesInRect:(NSRect)rect
{
	//
	// We only want to draw certain columns
	//
	NSIndexSet * origColumns = [super columnIndexesInRect:rect];
	if (fInGridDrawing) {
		NSMutableIndexSet * filtered = [origColumns mutableCopy];
		[filtered removeIndex:2];
		[filtered removeIndex:3];
		[filtered removeIndex:5];
		[filtered removeIndex:6];
		return [filtered autorelease];
	} else {
		return origColumns;
	}
}

@end
