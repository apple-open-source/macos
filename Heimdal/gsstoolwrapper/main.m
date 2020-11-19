/*
* Copyright (c) 2006 Kungliga Tekniska Högskolan
* (Royal Institute of Technology, Stockholm, Sweden).
* All rights reserved.
*
* Portions Copyright (c) 2020 Apple Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* 3. Neither the name of KTH nor the names of its contributors may be
*    used to endorse or promote products derived from this software without
*    specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
* OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#import <Foundation/Foundation.h>


int main(int argc, const char * argv[]) {
    @autoreleasepool {

	if (argc != 2)
	{
	    printf("gsstoolwrapper [audit token file name] \n");
	    printf("This program is a quick way to get a \"real\" audit token to pass to gsstool for testing. The pid from it has to be alive for it to be valid for gss use. The intended use is to run the app and then bacground it.  It will stay alive so that gsstool can use the audit token to confirm that delegation is working correctly based on the current configuration.  If the wrapper tool is assigned per app vpn, then gss tool should be able to confirm delegation is working correctly.\n");
	}
	
	NSString *tempFilePath = [NSString stringWithCString:argv[1] encoding:NSUTF8StringEncoding];
	
	audit_token_t self_token;
	mach_msg_type_number_t token_size = TASK_AUDIT_TOKEN_COUNT;
	kern_return_t kr = task_info(mach_task_self(), TASK_AUDIT_TOKEN,
	(integer_t *)&self_token, &token_size);
	if (kr != KERN_SUCCESS) {
	    printf("Failed to get own token");
	    return 1;
	}
	
	NSData *tokenData = [NSData dataWithBytesNoCopy:&self_token length:sizeof(audit_token_t)];
	
	if (![tokenData writeToFile:tempFilePath atomically:YES]) {
	    printf("Failed to write out token");
	    return 1;
	}

	[[NSRunLoop currentRunLoop] run];

    }
    return 0;
}
