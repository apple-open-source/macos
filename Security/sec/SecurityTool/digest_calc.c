//
//  digest_calc.c
//  Digest calculation command for SecurityTool
//
//  Created by John Kelley on 4/27/11.
//  Copyright 2011 Apple, Inc. All rights reserved.
//

#include "builtin_commands.h"

#include <stdlib.h>
#include <strings.h>

#include <SecurityTool/readline.h>

#include <corecrypto/ccsha1.h>
#include <corecrypto/ccsha2.h>

extern int command_digest(int argc, char * const *argv)
{
    int result = 1;
    uint8_t *data = NULL;
    size_t data_len;
    const struct ccdigest_info *di;
    unsigned char *digest = NULL;
    unsigned long i,j;
    
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
    
    digest = malloc(di->output_size);
    if (!digest)
        goto exit;
    
    for (i = 2; i < (unsigned int)argc; ++i)
    {
        printf("%s(%s)= ", argv[1], argv[i]);
        if (read_file( argv[i], &data, &data_len) != 0 || !data)
        {
            printf("error reading file\n");
            continue;
        }
        ccdigest(di, data_len, data, digest);
    
        for (j = 0; j < di->output_size; j++)
            printf("%02x", digest[j]);
        printf("\n");
        free(data);
        data = NULL;
    }
    result = 0;
    
exit:
    if(data)
        free(data);
    if (digest)
        free(digest);
    
    return result;
}
