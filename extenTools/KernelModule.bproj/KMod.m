/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#import <ProjectBuilder/PBProjectType.h>
#import <ProjectBuilder/PBProjectFiles.h>

#define EXPANSION_DELIMITER	'$'

@interface KMProgressPanel : NSObject
{
}

// the delegate is responsible to provide UI feedback
+ progressPanel;
@end


@implementation KMProgressPanel
+ progressPanel;
{
    return nil;
}

@end


@interface KModProjectType : PBProjectType
{
}

@end

@implementation KModProjectType

static BOOL expandVariablesInTemplateFile
     (NSString *in, NSString *out, NSDictionary *dictionary)
{
    FILE *input, *output;
    int ch;
    char buffer[1024];

    if (!(input = fopen([in cString], "r")))
       return NO;
    if (!(output = fopen([out cString], "w")))
    {
        fclose(input);
        return NO;
    }

    while ((ch = getc(input)) != EOF)
    {
        if (EXPANSION_DELIMITER != ch)
            putc(ch, output);
        else if (EXPANSION_DELIMITER == (ch = getc(input))) // Check for comment
            do {
                ch = getc(input);
            } while ('\n' != ch && EOF != ch);
        else
        {
            NSString *value, *key;
            char *ptr = buffer;

            buffer[0] = '\0';
            while (EOF != ch && EXPANSION_DELIMITER != ch && '\n' != ch) {
                if (ptr >= &buffer[sizeof(buffer)]-1)
                    break;
                *ptr++ = ch;
                ch = getc(input);
            }
            *ptr++ = '\0';
            if (ptr >= &buffer[sizeof(buffer)])
                NSLog(@"expanding %@: Key is too long %s", in, buffer);
            else if (EOF == ch)
                NSLog(@"expanding %@: Unexpected EOF: %s", in, buffer);
            else if ('\n' == ch)
                NSLog(@"expanding %@: New line in key: %s", in, buffer);
            else {
                key = [NSString stringWithCString: buffer];
                if (nil == (value = [dictionary objectForKey:key]))
                    NSLog(@"expanding %@: variable has no value: %@", in, key);
                else
                    fprintf(output, "%s", [value cString]);
            }
        }
    }
    fclose(input);
    fclose(output);
    return YES;
}

- (void) customizeNewProject: (PBProject *) project
{
    NSString *projectDir = [project projectDir];
    NSMutableDictionary *variables;
    NSString *inFile, *outFile , *inFileName , *outFileName;

    /* Now customise the current project */
    /* Create the dictionary that provides interesting info */
    variables = [NSMutableDictionary dictionary];
    [variables setObject:[project projectName] forKey:@"PRODUCTNAME"];
    [variables setObject: [[NSDate date]
        descriptionWithCalendarFormat:@"%Y-%m-%dT%H:%M:%SZ"
            timeZone: [NSTimeZone timeZoneWithAbbreviation: @"GMT" ] locale:nil]
        forKey:@"DATE"];
    [variables setObject:NSFullUserName() forKey:@"USERNAME"];

    outFileName = @"CustomInfo.xml";
    inFileName = [outFileName stringByAppendingString: @".template"];
    outFile = [projectDir stringByAppendingPathComponent: outFileName];
    inFile = [projectDir stringByAppendingPathComponent: inFileName];
    expandVariablesInTemplateFile(inFile, outFile, variables);
    [project addFile: outFileName key: @"OTHER_SOURCES"];
    [[NSFileManager defaultManager] removeFileAtPath: inFile handler:nil];
}

@end
