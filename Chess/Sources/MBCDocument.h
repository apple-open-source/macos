/*
	File:		MBCDocument.h
	Contains:	Pseudo-document, used only for loading and saving
	Version:	1.0
	Copyright:	© 2003-2010 by Apple Computer, Inc., all rights reserved.
*/

#import <Cocoa/Cocoa.h>

@class MBCController;

@interface MBCDocument : NSDocument
{
	MBCController *	fController;
}

+ (NSURL *)casualGameSaveLocation;
- (id) init;
- (id) initWithController:(MBCController *)controller;

@end

// Local Variables:
// mode:ObjC
// End:
