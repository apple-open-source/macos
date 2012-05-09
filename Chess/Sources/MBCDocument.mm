/*
	File:		MBCDocument.mm
	Contains:	Pseudo-document, used only for loading and saving
	Version:	1.0
	Copyright:	© 2003-2011 by Apple Computer, Inc., all rights reserved.
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
