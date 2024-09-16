//
//  utils.h
//  dcerpctest_server
//
//  Created by William Conway on 12/1/23.
//

#ifndef utils_h
#define utils_h

// Prints a description of a given error_status_t code.
// Arguments
//     ecode:   an rpc error_status_t code.
//     routine: Otional routine name which returned the error.
//     ctx:    Optional context about the error.
//     fatal:   Calls exit(1) if not set to zero.
//
void  chk_dce_err(error_status_t, const char *, const char *, unsigned int);

#endif /* utils_h */
