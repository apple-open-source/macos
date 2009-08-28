/*
	Trains.h
	Application Kit
	Copyright (c) 1994-1997, Apple Computer, Inc.
	All rights reserved.
*/

/*! @header Trains.h
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

typedef enum {
    NSBoxPrimary	= 0,	// default
    NSBoxSecondary	= 1,
    NSBoxSeparator	= 2,
    NSBoxOldStyle	= 3	// use border type
} NSBoxType;


/*!
 	@class Trains_2
	@abstract NSBox provides a visual grouping element.
	@discussion An NSBox object is a simple NSView that can do two things: 
	It can draw a border around itself, and it can title itself. 
*/

@interface Trains_2 : Vehicles
{
    /*All instance variables are private*/
    id                  _titleCell;
    id                  _contentView;
    NSSize              _offsets;
    NSRect              _borderRect;
    NSRect              _titleRect;
    id			_unused;
}

/*!
 	@method trainsMethodOne
	@abstract Returns the receiver's border type.
	@discussion Returns the receiver's border type. Border types are defined 
	in NSView.h. Currently, the following border types are defined:

		* NSNoBorder
		
		* NSLineBorder
		
		* NSBezelBorder
		
		* NSGrooveBorder

	By default, an Train's border type is NSGrooveBorder.
*/
- (NSBorderType)trainsMethodOne;


/*!
 	@method trainsMethodTwo
	@abstract Returns a constant representing the title position.
*/
- (NSTitlePosition)trainsMethodTwo;

/*! 
    @method dateWithString:calendarFormat:   
	@abstract Creates and returns a calendar date initialized with the date 
     specified in the string description. 
	@discussion Test method three in class one. 
	@param description  A string specifying the date.
	@param format  Conversion specifiers similar to those used in strftime().
	@result  Returns the newly initialized date object or nil on error.
*/
+ (id)dateWithString:(NSString *)description calendarFormat:(NSString *)format;


@end


/*!
 	@category Trains_2(TrainsCanFly)
	@abstract The TrainsCanFly category adds levitation methods to the Train class.
	@discussion Methods declared in the TrainsCanFly category of the Train class can only be used with properly equiped Train objects.   
*/

@interface Trains_2(TrainsCanFly)
/*!
 	@method levitateToHeight:
	@abstract Raises the train specified number of centimeters
	@param height The number of centimeters to levitate.
*/
- (void)levitateToHeight:(float)height;
@end

/*!
 	@category Trains_2(TrainsUnderWater)
	@abstract The TrainsUnderWater category adds submarine methods to the Train class.
	@discussion Methods declared in the TrainsUnderWater category of the Train class can only be used with properly equiped Train objects and appropriate infrastructure.   
*/

@interface Trains_2(TrainsUnderWater)
/*!
 	@method enterChunnelOnTrack:
	@abstract Directs the train to Channel Tunnel
	@param num Track number.
*/
- (void)enterChunnelOnTrack:(int)num;
@end

