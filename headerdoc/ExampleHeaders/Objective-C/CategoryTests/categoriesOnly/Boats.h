/*
	Boats.h
	Application Kit
	Copyright (c) 1994-1997, Apple Computer, Inc.
	All rights reserved.
*/

/*! @header Boats.h
    @discussion This header is used to exercise HeaderDoc's ability to generate documentation from Objective-C headers.   It includes declarations for C API and Objective-C classes, protocols, categories, and methods. The code is definitely NOT part of Cocoa, although some declarations have been snagged from Cocoa headers.
*/

#import <AppKit/NSView.h>

@class NSFont;

/*!
	@typedef NSTitlePosition
	Constants that represent title positions.
    @abstract Abstract for this API.
    @discussion Discussion that applies to the entire typedef'd enum.
    @constant NSNoTitle No title.
    @constant NSAboveTop Description of second constant.
    @constant NSAtTop Description of third constant.
*/
typedef enum _NSTitlePosition {
    NSNoTitle				= 0,
    NSAboveTop				= 1,
    NSAtTop				= 2,
    NSBelowTop				= 3,
    NSAboveBottom			= 4,
    NSAtBottom				= 5,
    NSBelowBottom			= 6
} NSTitlePosition;

/*!
 	@category Boats (BoatsCanFly)
	@abstract The BoatsCanFly category adds levitation methods to the Boat class.
	@discussion Methods declared in the BoatsCanFly category of the Boats class can only be used with properly equiped Boat objects.   
*/

@interface Boats (BoatsCanFly)
/*!
 	@method levitateToHeight:
	@abstract Raises the boat specified number of centimeters
	@param height The number of centimeters to levitate.
*/
- (void)levitateToHeight:(float)height;
@end

