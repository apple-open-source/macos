/*
 * Copyright (c) 2013 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2013 Apple Inc. All rights reserved.
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
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "kuser_locl.h"
#include "kcc-commands.h"
#include "heimcred.h"

/*
 *
 */

int
dump_credentials(struct dump_credentials_options *opt, int argc, char **argv)
{
    CFDictionaryRef query;
    CFArrayRef array;
    CFIndex n, count;
    
    const void *keys[] = { kHEIMAttrType };
    void *values[1] = { NULL };
    
    
    if (opt->type_string == NULL)
	values[0] = (void *)kHEIMTypeKerberos;
    else if (strcasecmp(opt->type_string, "Kerberos") == 0)
	values[0] = (void *)kHEIMTypeKerberos;
    else if (strcasecmp(opt->type_string, "NTLM") == 0)
	values[0] = (void *)kHEIMTypeNTLM;
    else if (strcasecmp(opt->type_string, "Configuration") == 0)
	values[0] = (void *)kHEIMTypeConfiguration;
    else if (strcasecmp(opt->type_string, "Generic") == 0)
	values[0] = (void *)kHEIMTypeGeneric;
    else if (strcasecmp(opt->type_string, "Schema") == 0)
	values[0] = (void *)kHEIMTypeSchema;
    else {
	printf("unknown type; %s\n", opt->type_string);
	return 1;
    }
	
    query = CFDictionaryCreate(NULL, keys, (const void **)values, sizeof(keys)/sizeof(keys[0]), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (query == NULL)
	errx(1, "out of memory");


    array = HeimCredCopyQuery(query);
    CFRelease(query);
    if (array == NULL) {
	printf("no credentials\n");
	return 0;
    }
    
    count = CFArrayGetCount(array);
    for (n = 0; n < count; n++) {
	HeimCredRef cred = (HeimCredRef)CFArrayGetValueAtIndex(array, n);

	CFShow(cred);
	
	if (opt->verbose_flag) {
	    CFDictionaryRef attrs;

	    attrs = HeimCredCopyAttributes(cred, NULL, NULL);
	    if (attrs) {
		CFShow(attrs);
		CFRelease(attrs);
	    }
	}
	
    }

    return 0;
}
