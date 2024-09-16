//
//  test_utils.c
//  DCERPCTests
//
//  Created by William Conway on 12/1/23.
//

#include "test_utils.h"
#include <stdlib.h>
#include <string.h>

int
get_client_rpc_binding(
    rpc_binding_handle_t * binding_handle,
    const char * hostname,
    const char * protocol,
    const char * endpoint)
{
    unsigned_char_p_t string_binding = NULL;
    error_status_t status;

  /*
   * create a string binding given the parameters and
   * resolve it into a full binding handle
   */

    rpc_string_binding_compose(NULL,
                   (unsigned_char_p_t)protocol,
                   (unsigned_char_p_t)hostname,
                   (unsigned_char_p_t)endpoint,
                   NULL,
                   &string_binding,
                   &status);
    chk_dce_err(status, "rpc_string_binding_compose()", "get_client_rpc_binding", 0);
    if (status != error_status_ok) {
        return (0);
    }
    
    rpc_binding_from_string_binding(string_binding,
                                    binding_handle,
                                    &status);
    chk_dce_err(status, "rpc_binding_from_string_binding()", "get_client_rpc_binding", 0);
    if (status != error_status_ok) {
        return (0);
    }

    printf("fully resolving binding for server is: %s\n", string_binding);
    rpc_string_free(&string_binding, &status);
    chk_dce_err(status, "rpc_string_free()", "get_client_rpc_binding", 0);
    if (status != error_status_ok) {
        return (0);
    }

  return 1;
}

void
chk_dce_err(error_status_t ecode, const char *routine, const char *ctx, unsigned int fatal)
{
  dce_error_string_t errstr;
  int error_status;

  if (ecode != error_status_ok)
    {
       dce_error_inq_text(ecode, errstr, &error_status);
        if (error_status == error_status_ok) {
            printf("chk_dce_err: routine: %s, ctx: %s, rpc_code = 0x%x, error_desc: %s\n",
                   routine ? routine : "n/a", ctx ? ctx : "n/a", ecode, errstr);
        } else
            printf("chk_dce_err: routine: %s ctx: %s rpc_code = 0x%x, error_desc: <not available>\n",
                   routine ? routine : "n/a", ctx ? ctx : "n/a", ecode);

        if (fatal) {
            printf("chk_dce_err: exiting on fatal error\n");
            exit(1);
        }
    }
}

bool stringsAreReversed(const char *str1, const char *str2)
{
    size_t i, j, slen1, slen2;
    bool areReversed = true;
    
    if (str1 == NULL || str2 == NULL) {
        return false;
    }
    
    slen1 = strlen(str1);
    slen2 = strlen(str2);
    
    if (slen1 != slen2) {
        printf("stringsAreReversed: length mismatch, slen1: %lu != slen2: %lu\n", slen1, slen2);
        return false;
    }
    
    // Walk both arrays in opposing directions, comparing each character.
    // This way we can display the offset/position of the offending character
    // if we do encounter a mismatch.
    for (i = 0, j = slen1 - 1; i < slen1; i++, j--) {
        if (str1[i] != str2[j]) {
            
            printf("\tstringsAreNotReversed: str1: %s, str2: %s, str1 pos: %lu, str2 pos: %lu",
                   str1, str2, i, j);
            areReversed = false;
            break;
        }
    }
    
    return areReversed;
}
