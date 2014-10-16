/*
 * Copyright (c) 2011,2013-2014 Apple Inc. All Rights Reserved.
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


#include "builtin_commands.h"

#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include <SecurityTool/readline.h>

#include <corecrypto/ccsha1.h>
#include <corecrypto/ccsha2.h>

#include <AssertMacros.h>

extern int command_digest(int argc, char * const *argv)
{
    int result = 1, fd;
    const struct ccdigest_info *di;
    unsigned char *digest = NULL;
    unsigned long i,j;
    size_t nr = 0, totalBytes = 0;
    char data [getpagesize()];
    
    if (argc < 3)
        return 2; /* Return 2 triggers usage message. */
    
    if (strcasecmp("sha1", argv[1]) == 0)
    {
        //printf("Calculating sha1\n");
        di = ccsha1_di();
    }
    else if (strcasecmp("sha256", argv[1]) == 0)
    {
        //printf("Calculating sha256\n");
        di = ccsha256_di();
    }
    else if (strcasecmp("sha512", argv[1]) == 0)
    {
        //printf("Calculating sha256\n");
        di = ccsha512_di();
        
    }
    else
        return 2; /* Return 2 triggers usage message. */
    
    ccdigest_di_decl(di, ctx);
    ccdigest_init(di, ctx);
    
    digest = malloc(di->output_size);
    require_quiet(digest, exit);
    
    for (i = 2; i < (unsigned int)argc; ++i)
    {
        printf("%s(%s)= ", argv[1], argv[i]);
        if ((fd = inspect_file_and_size(argv[i], NULL)) == -1)
        {
            printf("error reading file\n");
            continue;
        }
        totalBytes = 0;
        while((nr = pread(fd, data, sizeof(data), totalBytes)) > 0){
            ccdigest_update(di, ctx, nr, data);
            totalBytes += nr;
        }
    
        ccdigest_final(di, ctx, digest);

        for (j = 0; j < di->output_size; j++)
            printf("%02x", digest[j]);
        printf("\n");
    }
    result = 0;
    
exit:
    if (digest)
        free(digest);
    
    return result;
}
