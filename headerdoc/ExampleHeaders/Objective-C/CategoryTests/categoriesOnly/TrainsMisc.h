/*
	TrainsMisc.h
	Application Kit
	Copyright (c) 1994-1997, Apple Computer, Inc.
	All rights reserved.
*/

/*! @header TrainsMisc.h
    @discussion This header is used to exercise HeaderDoc's ability to generate documentation from Objective-C headers.   It includes declarations for C API and Objective-C classes, protocols, categories, and methods. The code is definitely NOT part of Cocoa, although some declarations have been snagged from Cocoa headers.
*/


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
 	@category Trains(Performance)
	@abstract The Performance category instruments the Train class for performance analysis.
	@discussion Methods declared in the Performance category of the Trains class are typically used during development--to analyze and improve performance.   
*/

@interface Trains(Performance)
/*!
 	@method canYouGoAnyFaster
	@abstract Queries the Train for its capabilities
*/
- (BOOL)canYouGoAnyFaster;
@end

