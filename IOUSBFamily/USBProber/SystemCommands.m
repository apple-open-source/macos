/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

#import "SystemCommands.h"

AuthorizationRef myAuthorizationRef;

@implementation systemCommands

+(NSString *)kmodstatWithAuth:(BOOL)needsAuth
{
    if (needsAuth) {
        if([[authorization sharedInstance] authenticate])
        {
            char* args[1];
            FILE* kmodstatPipe; 
            NSMutableData* outputData;
            NSMutableData* tempData;
            OSStatus err = 0;
            int len = 0;
         
            myAuthorizationRef = [[authorization sharedInstance] returnAuthorizationRef];
                              
            //the arguments parameter to AuthorizationExecuteWithPrivileges is
            //a NULL terminated array of C string pointers.
            args[0] =NULL;
            
            err = AuthorizationExecuteWithPrivileges(myAuthorizationRef,"/usr/sbin/kmodstat",0,nil,&kmodstatPipe);
            outputData = [NSMutableData data];
            tempData = [NSMutableData dataWithLength:512];
            do
            {
                [tempData setLength:512];
                //read the output of the task using fread or any other
                //standard FILE function.
                //You should also be able to write to the task if needed 
                //(but don't if it's not needed, or you'll get a SIGPIPE).
                len = fread([tempData mutableBytes],1,512,kmodstatPipe);
                if(len>0)
                {
                    [tempData setLength:len];
                    [outputData appendData:tempData];        
                }
            } while(len==512);
        
        
            if(err)
            {
                NSLog(@"Error %d executing /usr/sbin/kmodstat",err);
                NSBeep();
            }
            
            return [[NSString alloc]initWithData:outputData encoding:NSASCIIStringEncoding];

        }
        else
        {
            return @"";
        }
    }
    else {
        NSTask *kextstat=[[NSTask alloc] init];
        NSPipe *pipe=[[NSPipe alloc] init];
        NSData *resultData;
        NSString *resultString;
        
        [kextstat setLaunchPath:@"/usr/sbin/kextstat"];
        [kextstat setStandardOutput:pipe];
        
        [kextstat launch];

        resultData = [[pipe fileHandleForReading] readDataToEndOfFile];
        resultString = [[NSString alloc] initWithData:resultData encoding:NSASCIIStringEncoding];

        [kextstat release];
        [pipe release];
        
        return [resultString autorelease];
    }
}

+(NSString *)grep:(NSString *)inputString arguments:(NSArray *)greparguments
{
    NSString *resultString;
    NSPipe *inputPipe = [[NSPipe alloc] init];
    NSPipe *outputPipe = [[NSPipe alloc] init];
    NSTask *grepTask = [[NSTask alloc] init];
    NSData *inputStringAsData = [inputString dataUsingEncoding:NSASCIIStringEncoding];
    NSData *resultData;

    [grepTask setLaunchPath:@"/usr/bin/grep"];
    [grepTask setStandardInput:inputPipe];
    [grepTask setStandardOutput:outputPipe];
    [grepTask setArguments:greparguments];

    [grepTask launch];
    
    [[inputPipe fileHandleForWriting] writeData:inputStringAsData];
    [[inputPipe fileHandleForWriting] closeFile];
    resultData = [[outputPipe fileHandleForReading] readDataToEndOfFile];
    resultString = [[NSString alloc] initWithData:resultData encoding:NSASCIIStringEncoding];

    [grepTask release];
    [inputPipe release];
    [outputPipe release];
    return [resultString autorelease];
}

+(NSString *)awk:(NSString *)inputString arguments:(NSArray *)awkarguments
{
    NSString *resultString;
    NSPipe *inputPipe = [[NSPipe alloc] init];
    NSPipe *outputPipe = [[NSPipe alloc] init];
    NSTask *awkTask = [[NSTask alloc] init];
    NSData *inputStringAsData = [inputString dataUsingEncoding:NSASCIIStringEncoding];
    NSData *resultData;

    [awkTask setLaunchPath:@"/usr/bin/awk"];
    [awkTask setStandardInput:inputPipe];
    [awkTask setStandardOutput:outputPipe];
    [awkTask setArguments:awkarguments];

    [awkTask launch];

    [[inputPipe fileHandleForWriting] writeData:inputStringAsData];
    [[inputPipe fileHandleForWriting] closeFile];
    resultData = [[outputPipe fileHandleForReading] readDataToEndOfFile];
    resultString = [[NSString alloc] initWithData:resultData encoding:NSASCIIStringEncoding];

    [awkTask release];
    [inputPipe release];
    [outputPipe release];
    return [resultString autorelease];
}
@end
