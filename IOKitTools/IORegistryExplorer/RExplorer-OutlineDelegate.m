/* RExplorer-OutlineDelegate.m created by epeyton on Tue 11-Jan-2000 */

#import "RExplorer-OutlineDelegate.h"

@implementation RExplorer (OutlineDelegate)

- (BOOL)outlineView:(NSOutlineView *)outlineView shouldSelectItem:(id)item
{
    [inspectorText setString:[item description]];
    [inspectorText display];
    return YES;
}

- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item
{
    if (item == nil) {
        // root
        return [[currentSelectedItemDict allValues] objectAtIndex:index];
    } else {
        //id newItem = [currentSelectedItemDict objectForKey:item];
        if ([item isKindOfClass:[NSArray class]]) {
            if ([item count]) {
                return [item objectAtIndex:index];
            }
        } else if ([item isKindOfClass:[NSDictionary class]]) {
            if ([item count]) {
                return [[item allValues] objectAtIndex:index];
            }
        }
        return item;
    }
    return nil;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
    //id newItem = [currentSelectedItemDict objectForKey:item];
    if ([item isKindOfClass:[NSArray class]]) {
        if ([item count] > 0) {
            return YES;
        }
    } else if ([item isKindOfClass:[NSDictionary class]]) {
        if ([[item allKeys] count] > 0) {
            return YES;
        }
    }
    return NO;
}

- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
    if (item == nil) {
         // root
         //NSLog(@"%@", currentSelectedItemDict);
        return [[currentSelectedItemDict allKeys] count];
    } else {
        //id newItem = [currentSelectedItemDict objectForKey:item];

    if ([item isKindOfClass:[NSArray class]] || [item isKindOfClass:[NSDictionary class]]) {
            return [item count];
        }
    }
    return 0;
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
    if (tableColumn == keyColumn) {
        id parentObject = (id)NSMapGet(_parentMap, item);

        if (!parentObject) {
            parentObject = currentSelectedItemDict;
        }

        if ([parentObject isKindOfClass:[NSArray class]]) {
            return [NSNumber numberWithInt:[parentObject indexOfObject:item]];
        }
        if ([parentObject isKindOfClass:[NSDictionary class]]) {
            //int index = [[parentObject allValues] indexOfObject:item];
            //return [[parentObject allKeys] objectAtIndex:index];
            id obj = (id)NSMapGet(_keyMap, item);
            return obj;
        }

        return item;
    } if (tableColumn == typeColumn) {
        // Return an NSNumber with the index of the selected item in the popup of classes.
        id obj = @"";

        if ([item isKindOfClass:[NSDictionary class]]) {
            obj = @"Dictionary";
        } else if ([item isKindOfClass:[NSArray class]]) {
            obj = @"Array";
        } else if ([item isKindOfClass:[NSString class]]) {
            obj = @"String";
        } else if ([item isKindOfClass:[NSData class]]) {
            obj = @"Data";
        } else if ([item isKindOfClass:[NSDate class]]) {
            obj = @"Date";
        //} else if (CFGetTypeID(item) == CFBooleanGetTypeID()) {
        } else if ([item isKindOfClass:[RBool class]]) {
            obj = @"Boolean";
        } else if ([item isKindOfClass:[NSNumber class]]) {
            obj = @"Number";
        }
        return obj;
    } else {

        if ([item isKindOfClass:[NSArray class]]) {
            return [NSString stringWithFormat:@"(%d Objects)", [item count]];
        } else if ([item isKindOfClass:[NSDictionary class]]) {
            return [NSString stringWithFormat:@"(%d Key/Value Pairs)", [item count]];
        } else if ([item isKindOfClass:[NSData class]]) {
            return [self CFDataShow:item];
            //return [item description];
        } /*else if (CFGetTypeID(item) == CFBooleanGetTypeID()) {
            if ([[item description] isEqualToString:@"0"]) {
                return @"No";
            } else {
                return @"Yes";
            }
        } */
        
        return [item description];
    }
    return @"";

}

- (NSString *)CFDataShow:(CFDataRef) object
{
    UInt32        asciiNormalCount = 0;
    UInt32        asciiSymbolCount = 0;
    const UInt8 * bytes;
    CFIndex       index;
    CFIndex       length;

    NSMutableString *newString = [NSMutableString string];

    [newString appendString:@"<"];
    length = CFDataGetLength(object);
    bytes  = CFDataGetBytePtr(object);

    //
    // This algorithm detects ascii strings, or a set of ascii strings, inside a
    // stream of bytes.  The string, or last string if in a set, needn't be null
    // terminated.  High-order symbol characters are accepted, unless they occur
    // too often (80% of characters must be normal).  Zero padding at the end of
    // the string(s) is valid.  If the data stream is only one byte, it is never
    // considered to be a string.
    //

    for (index = 0; index < length; index++)  // (scan for ascii string/strings)
    {
        if (bytes[index] == 0)       // (detected null in place of a new string,
        {                            //  ensure remainder of the string is null)
            for (; index < length && bytes[index] == 0; index++) { }

            break;          // (either end of data or a non-null byte in stream)
        }
        else                         // (scan along this potential ascii string)
        {
            for (; index < length; index++)
            {
                if (isprint(bytes[index]))
                    asciiNormalCount++;
                else if (bytes[index] >= 128 && bytes[index] <= 254)
                    asciiSymbolCount++;
                else
                    break;
            }

            if (index < length && bytes[index] == 0)          // (end of string)
                continue;
            else             // (either end of data or an unprintable character)
                break;
        }
    }

    if ((asciiNormalCount >> 2) < asciiSymbolCount)    // (is 80% normal ascii?)
        index = 0;
    else if (length == 1)                                 // (is just one byte?)
        index = 0;

    if (index >= length && asciiNormalCount) // (is a string or set of strings?)
    {
        Boolean quoted = FALSE;

        for (index = 0; index < length; index++)
        {
            if (bytes[index])
            {
                if (quoted == FALSE)
                {
                    quoted = TRUE;
                    if (index)
                        [newString appendString:@",\""];
                    else
                        [newString appendString:@"\""];
                }
                [newString appendFormat:@"%c", bytes[index]];
            }
            else
            {
                if (quoted == TRUE)
                {
                    quoted = FALSE;
                    [newString appendString:@"\""];
                }
                else
                    break;
            }
        }
        if (quoted == TRUE)
            [newString appendString:@"\""];
    }
    else                                  // (is not a string or set of strings)
    {
        for (index = 0; index < length; index++)
            [newString appendFormat:@"%02x", bytes[index]];
    }

    [newString appendString:@">"];
    return [[newString copy] autorelease];
}


@end
