/*
	File:		MBCDocument.mm
	Contains:	Pseudo-document, used only for loading and saving
	Version:	1.0
	Copyright:	© 2003 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Matthias Neeracher    x43683

	Writers:

		(MN)	Matthias Neeracher

	Change History (most recent first):

		$Log: MBCDocument.mm,v $
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

	[self addWindowController:[controller windowController]];

	return self;
}

- (void) close
{
	[self removeWindowController:[[self windowControllers] lastObject]];
	[super close];
}

- (void) doClose:(id)arg
{
	[self close];
}

- (BOOL) loadDataRepresentation:(NSData *)docData ofType:(NSString *)docType
{
	NSPropertyListFormat	format;
	BOOL res = [fController 	
				   loadGame:[self fileName] fromDict:
					   [NSPropertyListSerialization 
						   propertyListFromData: docData 
						   mutabilityOption: NSPropertyListImmutable
						   format: &format
						   errorDescription:nil]];
	[self performSelector:@selector(doClose:) withObject:nil afterDelay:0.010];

	return res;
}

- (BOOL)writeToFile:(NSString *)fileName ofType:(NSString *)docType
{
	BOOL res;

	if ([docType isEqualToString:@"moves"])
		res = [fController saveMovesTo:fileName];
	else
		res = [super writeToFile:fileName ofType:docType];

	[self performSelector:@selector(doClose:) withObject:nil afterDelay:0.010];

	return res;
}

- (NSData *)dataRepresentationOfType:(NSString *)aType
{
	return [NSPropertyListSerialization 
			   dataFromPropertyList:[fController saveGameToDict]
			   format: NSPropertyListXMLFormat_v1_0
			   errorDescription:nil];
}

@end

// Local Variables:
// mode:ObjC
// End:
