//
//  MBCStretchyTextCell.mm
//  MBChess
//
//  Created by Matthias Neeracher on 10/7/10.
//  Copyright 2010 Apple Computer. All rights reserved.
//

#import "MBCStretchyTextCell.h"


@implementation MBCStretchyTextCell

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
	if ([[self stringValue] length] > 2) {
		cellFrame.origin.x		-= 50;
		cellFrame.size.width	+= 100;
	} else {
		cellFrame.origin.x		-= 3;
		cellFrame.size.width	+= 6;
	}
	[super drawInteriorWithFrame:cellFrame inView:controlView];
}

@end
