/*
	ObjCTestHeader.h
	Application Kit
	Copyright (c) 1994-1997, Apple Computer, Inc.
	All rights reserved.
*/

/*! @header ObjCTestHeader.h
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
 	@class ObjCClassAlpha
	@abstract ObjCClassAlpha provides a visual grouping element.
	@discussion An NSBox object is a simple NSView that can do two things: 
	It can draw a border around itself, and it can title itself. 
*/

@interface ObjCClassAlpha : NSObject
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
 	@method classOneMethodOne
	@abstract Returns the receiver's border type.
	@discussion Returns the receiver's border type. Border types are defined 
	in NSView.h. Currently, the following border types are defined:

		* NSNoBorder
		
		* NSLineBorder
		
		* NSBezelBorder
		
		* NSGrooveBorder

	By default, an NSBox's border type is NSGrooveBorder.
*/
- (NSBorderType)classOneMethodOne;

/*!
 	@method classOneMethodTwo
	@abstract Returns a constant representing the title position.
*/
- (NSTitlePosition)classOneMethodTwo;

@end

/*!
 	@class ObjCClassBeta
	@abstract ObjCClassBeta provides a visual grouping element.
	@discussion An NSBox object is a simple NSView that can do two things: It can draw a border around itself, and it can title itself. 
*/

@interface ObjCClassBeta : NSObject
{
    /*All instance variables are private*/
    struct __bFlags {
		NSBorderType	borderType:2;
		NSTitlePosition	titlePosition:3;
		unsigned int	transparent:1;
        unsigned int	boxType:2;
        unsigned int	_RESERVED:24;
    } _bFlags;
    id			_unused;
}

/*!
 	@method classTwoMethodOne
	@abstract Returns the receiver's border type.
	@discussion Returns the receiver's border type. Border types are defined in NSView.h. Currently, the following border types are defined:

		* NSNoBorder
		
		* NSLineBorder
		
		* NSBezelBorder
		
		* NSGrooveBorder

	By default, an NSBox's border type is NSGrooveBorder.
*/
- (NSBorderType)classTwoMethodOne;

/*!
 	@method classTwoMethodTwo
	@abstract Returns a constant representing the title position.
*/
- (NSTitlePosition)classTwoMethodTwo;

/*!
 	@method classTwoMethodThree
	@abstract Sets the border type to aType, which must be a valid border type. 
	@discussion Border types are defined in NSView.h. Currently, the following border types are defined:

		* NSNoBorder
		* NSLineBorder
		* NSBezelBorder
		* NSGrooveBorder

	By default, an NSBox's border type is NSGrooveBorder.
	
	@param aType The specified type.
*/
- (void)classTwoMethodThree:(NSBorderType)aType;

/*!
 	@method classTwoMethodFour:bogusOne:bogusTwo
	@abstract Sets the title position and includes two bogus args for 
	          testing purposes
	@param bogusOne Fictive at best.
	@param bogusTwo This argument is rarely used.
*/
- (void)classTwoMethodFour:(NSTitlePosition)aPosition bogusOne:(NSView *)bogusOne bogusTwo:(NSView *)bogusTwo;

@end


