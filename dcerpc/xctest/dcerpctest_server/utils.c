//
//  utils.c
//  dcerpctest_server
//
//  Created by William Conway on 12/1/23.
//

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <compat/dcerpc.h>
#include "utils.h"

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

