/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header NSStringEscPath
 */


#import <Foundation/Foundation.h>

/*!
 * @category NSString(NSStringEscPath)
 * A category on NSString that provides
 * on extra method for handling paths.
 */
@interface NSString (NSStringEscPath)

/*!
    @method     escapablePathFromArray:
    @abstract   Takes an array and turns it into an escaped path NSString
    @discussion Returns and escaped string that can be re-parsed if necessary
*/
+ (NSString *)escapablePathFromArray:(NSArray *)inArray;

/*!
 * @method unescapedPathComponents
 * @abstract Like pathComponents, but will honor escaped slashes.
 * @discussion Performs the same operation as pathComponents
 *		but it will honor escaped slashes for allowing the use
 *		of forward slashes in a name.
 *
 *		Example: @"/path/to/file/namewith\/slash"
 *		will be split into this array:
 *
 *		(@"/", @"path", @"to", @"file", @"namewith/slash")
 *		instead of the way pathComponents would do it:
 *
 *		(@"/", @"path", @"to", @"file", @"namewith\", @"slash")
 *
 * @result Array of NSStrings of the path components.
 */
- (NSArray *)unescapedPathComponents;

/*!
    @method     unescapedString
    @abstract   This gets the string in a standard form for use by removing escape characters
    @discussion Unescapes all the components and returns a clean string for normal use
*/

- (NSString *)unescapedString;

/*!
    @method     escapedString
    @abstract   Escapes the string for use by routines
    @discussion Escapes the string 
*/
- (NSString *)escapedString;

/*!
    @method     shortcut for stringByReplacingPercentEscapesUsingEncoding:
    @abstract   shortcut for calling stringByReplacingPercentEscapesUsingEncoding, for legacy calls
    @discussion shortcut for calling stringByReplacingPercentEscapesUsingEncoding, for legacy calls
*/

- (NSString *)urlEncoded;

@end
