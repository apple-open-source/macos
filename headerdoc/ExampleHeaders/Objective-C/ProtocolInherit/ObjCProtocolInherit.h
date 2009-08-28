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

/*!
 	@protocol ObjCProtocolOne_A
	@abstract Protocol implemented by validator objects.
	@discussion All validator objects validate--in fact, they can't 
	help themselves. That's what they do. 
*/

@protocol ObjCProtocolOne_A
/*!
 	@method ProtoOneMethodOne:
	@abstract Validates the specified item.
	@param anItem The item to be validated.
*/
- (BOOL)ProtoOneMethodOne:(id <NSValidatedUserInterfaceItem>)anItem;
@end



/*!
 	@protocol ObjCProtocolTwo
	@abstract Protocol implemented by validator objects.
	@discussion All validator objects validate--in fact, they can't 
	help themselves. That's what they do. 
*/

@protocol ObjCProtocolTwo <NSObject>
/*!
 	@method validateUserInterfaceItem:
	@abstract Validates the specified item.
	@param anItem The item to be validated.
*/
- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)anItem;
@end



/*!
 	@protocol ObjCProtocolThree
	@abstract Protocol implemented by validator objects.
	@discussion All validator objects validate--in fact, they can't 
	help themselves. That's what they do. 
*/

@protocol ObjCProtocolThree <NSObject, NSCopying, NSCoding>
/*!
 	@method doSomething:
	@abstract Validates the specified item.
	@param anItem The item to be validated.
*/
- (BOOL)doSomething:(id <NSValidatedUserInterfaceItem>)anItem;
@end



