/*
	File:		MBCDocument.h
	Contains:	Pseudo-document, used only for loading and saving
	Version:	1.0
	Copyright:	© 2003-2010 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCDocument.h,v $
		Revision 1.4  2010/10/08 17:40:53  neerache
		<rdar://problem/8518971> Chess asks about saving game when logging out
		
		Revision 1.3  2010/09/23 20:30:53  neerache
		Switch to newer document data loading methods
		
		Revision 1.2  2010/01/18 19:20:39  neerache
		<rdar://problem/7297328> Deprecated methods in Chess, part 2
		
		Revision 1.1  2003/04/02 18:41:01  neeri
		Support saving games
		
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
