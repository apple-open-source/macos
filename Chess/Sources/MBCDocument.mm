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
		Revision 1.5.2.1  2007/03/31 03:47:35  neerache
		Make document/save system work without UI changes <rdar://problem/4186113>
		
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

	return res;
}

- (BOOL)writeToURL:(NSURL *)fileURL ofType:(NSString *)docType error:(NSError **)outError
{
	BOOL res;

	if ([docType isEqualToString:@"moves"])
		res = [fController saveMovesTo:[fileURL path]];
	else
		res = [super writeToURL:fileURL ofType:docType error:outError];

	return res;
}

- (NSData *)dataRepresentationOfType:(NSString *)aType
{
	return [NSPropertyListSerialization 
			   dataFromPropertyList:[fController saveGameToDict]
			   format: NSPropertyListXMLFormat_v1_0
			   errorDescription:nil];
}

- (BOOL)shouldRunSavePanelWithAccessoryView
{
	return NO;
}

@end

// Local Variables:
// mode:ObjC
// End:
