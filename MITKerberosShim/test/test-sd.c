/*
 * Copyright (c) 2008-2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2008-2010 Apple Inc. All rights reserved.
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


#include <Kerberos/Kerberos.h>
#include <string.h>


int
main(int argc, char **argv)
{
    krb5_context kcontext = NULL;
    krb5_keytab kt = NULL;
    krb5_keytab_entry entry;
    krb5_kt_cursor cursor = NULL;
    krb5_error_code krb5_err;
    int matched = 0;
    
    char svc_name[] = "host";
    int svc_name_len = strlen (svc_name);
    
    
    krb5_err = krb5_init_context(&kcontext);
    if (krb5_err) { goto Error; }
    krb5_err = krb5_kt_default(kcontext, &kt);
    if (krb5_err) { goto Error; }
    krb5_err = krb5_kt_start_seq_get(kcontext, kt, &cursor);
    if (krb5_err) { goto Error; }
    while ((matched == 0) && (krb5_err = krb5_kt_next_entry(kcontext, kt, &entry, &cursor)) == 0) {
	
	krb5_data *nameData = krb5_princ_name (kcontext, entry.principal);
	if (NULL != nameData->data && svc_name_len == nameData->length
	    && 0 == strncmp (svc_name, nameData->data, nameData->length)) {
	    matched = 1;
	}
        
	krb5_free_keytab_entry_contents(kcontext, &entry);
    }
    
    krb5_err = krb5_kt_end_seq_get(kcontext, kt, &cursor);
 Error:
    if (NULL != kt) { krb5_kt_close (kcontext, kt); }
    if (NULL != kcontext) { krb5_free_context (kcontext); }
    // Return 0 if we got match or -1 if err or no match
    return (0 != krb5_err) ? -1 : matched ? 0 : -1;
}        

