/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#import "systemCommands.h"

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
        NSFileHandle *handle;
        
        [kextstat setLaunchPath:@"/usr/sbin/kextstat"];
        [kextstat setStandardOutput:pipe];
        handle=[pipe fileHandleForReading];
        [kextstat launch];
        [pipe release];
        [kextstat release];

        return [[NSString alloc] initWithData:[handle readDataToEndOfFile] encoding:NSASCIIStringEncoding];
    }
}

+(NSString *)grep:(NSString *)inputString arguments:(NSArray *)greparguments
{
            NSPipe *inputPipe=[[NSPipe alloc] init];
            NSPipe *outputPipe=[[NSPipe alloc] init];
            NSTask *grep=[[NSTask alloc] init];
            NSFileHandle *handle;
            NSData *grepdata;

            [grep setLaunchPath:@"/usr/bin/egrep"];

            [grep setStandardInput:inputPipe];
            [grep setStandardOutput:outputPipe];
            [grep setArguments:greparguments];
            
            handle=[outputPipe fileHandleForReading];

            grepdata = [inputString dataUsingEncoding:NSASCIIStringEncoding];
            
            [[inputPipe fileHandleForWriting] writeData:grepdata];
            [[inputPipe fileHandleForWriting] closeFile];
            [grep launch];
            
            [grep release];
            [inputPipe release];
            [outputPipe release];
                        
            return [[NSString alloc] initWithData:[handle readDataToEndOfFile] encoding:NSASCIIStringEncoding];
}

+(NSString *)awk:(NSString *)inputString arguments:(NSArray *)awkarguments
{
    NSPipe *inputPipe=[[NSPipe alloc] init];
    NSPipe *outputPipe=[[NSPipe alloc] init];
    NSTask *awk=[[NSTask alloc] init];
    NSFileHandle *handle;
    NSData *awkdata;

    [awk setLaunchPath:@"/usr/bin/awk"];

    [awk setStandardInput:inputPipe];
    [awk setStandardOutput:outputPipe];
    [awk setArguments:awkarguments];

    handle=[outputPipe fileHandleForReading];

    awkdata = [inputString dataUsingEncoding:NSASCIIStringEncoding];

    [[inputPipe fileHandleForWriting] writeData:awkdata];
    [[inputPipe fileHandleForWriting] closeFile];
    [awk launch];

    [awk release];
    [inputPipe release];
    [outputPipe release];

    return [[NSString alloc] initWithData:[handle readDataToEndOfFile] encoding:NSASCIIStringEncoding];
}
@end
