/* RExplorer-BrowserDelegate.m created by epeyton on Tue 11-Jan-2000 */

#import "RExplorer-BrowserDelegate.h"

@implementation RExplorer (BrowserDelegate)

// browser delegation

- (int)browser:(NSBrowser *)sender numberOfRowsInColumn:(int)column
{
    if (sender == browser) {

	if (column)
            return [[self childArrayAtColumn:column] count];
	else
	    return 1;

    } else if (sender == planeBrowser) {
        return [[[self propertiesForRegEntry:registryDict] objectForKey:@"IORegistryPlanes"] count];
    }
    return 0;
}

- (void)browser:(NSBrowser *)sender willDisplayCell:(id)cell atRow:(int)row column:(int)column
{
    if (sender == browser) {
        id object = nil;

        id name = nil;

	if (column)
            object = [[self childArrayAtColumn:column] objectAtIndex:row];
	else
            object = registryDict;

        name = [object objectForKey:@"name"];

        if (!name) {
            name = [object objectForKey:@"IOClass"];
        }

        if (!name) {
            name = [[object allKeys] objectAtIndex:0];
        }

        if (!name) {
            name = object;
        }

        [cell setStringValue:name];
        [cell setLeaf:(![[object objectForKey:@"children"] count])];
    } else if (sender == planeBrowser) {
        id planesDict = [[self propertiesForRegEntry:registryDict] objectForKey:@"IORegistryPlanes"];

        [cell setStringValue:[[[planesDict allKeys] sortedArrayUsingSelector:@selector(compare:)] objectAtIndex:row]];
        [cell setLeaf:YES];
    }
    return;
}

- (BOOL)browser:(NSBrowser *)sender selectRow:(int)row inColumn:(int)column
{

    return YES;
}



@end
