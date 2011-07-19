/*
	File:		MBCDocument.mm
	Contains:	Pseudo-document, used only for loading and saving
	Version:	1.0
	Copyright:	© 2003-2011 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCDocument.mm,v $
		Revision 1.13  2011/04/29 16:39:03  neerache
		<rdar://problem/9347429> 11A444b: Chess relaunches with Linen in background
		
		Revision 1.12  2010/10/08 17:40:53  neerache
		<rdar://problem/8518971> Chess asks about saving game when logging out
		
		Revision 1.11  2010/10/08 17:13:17  neerache
		<rdar://problem/8527777> Bundled chess program gives invalid PGN
		
		Revision 1.10  2010/09/23 20:30:53  neerache
		Switch to newer document data loading methods
		
		Revision 1.9  2010/09/23 20:24:03  neerache
		Autosave casual games
		
		Revision 1.8  2010/04/24 01:57:10  neerache
		<rdar://problem/7641028> TAL: Chess doesn't reload my game
		
		Revision 1.7  2010/01/18 19:20:39  neerache
		<rdar://problem/7297328> Deprecated methods in Chess, part 2
		
		Revision 1.6  2008/11/20 23:13:11  neerache
		<rdar://problem/5937079> Chess Save menu items/dialogs are incorrect
		<rdar://problem/6328581> Update Chess copyright statements
		
		Revision 1.5  2007/03/02 07:40:46  neerache
		Revise document handling & saving <rdar://problems/3776337&4186113>
		
		Revision 1.4  2003/08/11 22:55:41  neerache
		Loading was unreliable (RADAR 2811246)
		
		Revision 1.3  2003/07/03 08:12:51  neerache
		Use sheets for saving (RADAR 3093283)
		
		Revision 1.2  2003/04/24 23:22:02  neeri
		Implement persistent preferences, tweak UI
		
		Revision 1.1  2003/04/02 18:41:01  neeri
		Support saving games
		
*/

#import "MBCDocument.h"
#import "MBCController.h"

@implementation MBCDocument

- (id) init
{
	return [self initWithController:[MBCController controller]];
}

- (id) initWithController:(MBCController *)controller
{
	[super init];

	fController	= controller;

	return self;
}

- (void) close
{
	[super close];
}

- (BOOL)readFromData:(NSData *)docData ofType:(NSString *)docType error:(NSError **)outError
{
	NSPropertyListFormat	format;
	NSDictionary *			gameData = 
		[NSPropertyListSerialization propertyListWithData:docData options:0 format:&format error:outError];
	[fController performSelectorOnMainThread:@selector(loadGame:) 
				 withObject:gameData waitUntilDone:YES];

	return YES;
}

- (NSArray *)writableTypesForSaveOperation:(NSSaveOperationType)saveOperation
{
	//
	// Don't filter out PGN, even though we're not an editor for that type
	//
	return [[self class] writableTypes];
}

- (BOOL)writeToURL:(NSURL *)fileURL ofType:(NSString *)docType error:(NSError **)outError
{
	BOOL res;

	if ([docType isEqualToString:@"com.apple.chess.pgn"]) {
		res = [fController saveMovesTo:[fileURL path]];
	} else {
		res = [super writeToURL:fileURL ofType:docType error:outError];
		if (res) {
			//
			// If we saved in a non-default path, remove casual game save
			//
			NSURL * casual = [MBCDocument casualGameSaveLocation];
			if (![fileURL isEqual:casual])
				[[NSFileManager defaultManager]
					removeItemAtURL:casual error:NULL];
		}
	}

	return res;
}

- (NSData *)dataRepresentationOfType:(NSString *)aType
{
	return [NSPropertyListSerialization 
			   dataFromPropertyList:[fController saveGameToDict]
			   format: NSPropertyListXMLFormat_v1_0
			   errorDescription:nil];
}

+ (NSURL *)casualGameSaveLocation
{
	NSString *	savePath = [[[NSFileManager defaultManager] currentDirectoryPath] stringByAppendingPathComponent:@"Casual.game"];
	
	return [NSURL fileURLWithPath:savePath];
}

- (void)canCloseDocumentWithDelegate:(id)delegate shouldCloseSelector:(SEL)shouldCloseSelector contextInfo:(void *)contextInfo
{
	if (![self fileURL]) {
		NSError*	outError;
		[self saveToURL:[MBCDocument casualGameSaveLocation] ofType:@"com.apple.chess.game" forSaveOperation:NSSaveAsOperation error:&outError];
	}
	[super canCloseDocumentWithDelegate:delegate shouldCloseSelector:shouldCloseSelector contextInfo:contextInfo];
}

- (BOOL)readFromURL:(NSURL *)absoluteURL ofType:(NSString *)typeName error:(NSError **)outError
{
	BOOL loaded = [super readFromURL:absoluteURL ofType:typeName error:outError];
	if (loaded && [absoluteURL isEqual:[MBCDocument casualGameSaveLocation]])
		[self setFileURL:nil];
	
	return loaded;
}

@end

// Local Variables:
// mode:ObjC
// End:
