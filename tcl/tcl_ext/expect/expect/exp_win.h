/* exp_win.h - window support

Written by: Don Libes, NIST, 10/25/93

This file is in the public domain.  However, the author and NIST
would appreciate credit if you use this file or parts of it.
*/

#include <tcl.h> /* For _ANSI_ARGS_ */

int exp_window_size_set   _ANSI_ARGS_ ((int fd));
int exp_window_size_get   _ANSI_ARGS_ ((int fd));

void  exp_win_rows_set    _ANSI_ARGS_ ((char* rows));
int   exp_win_rows_get    _ANSI_ARGS_ ((void));
void  exp_win_columns_set _ANSI_ARGS_ ((char* columns));
int   exp_win_columns_get _ANSI_ARGS_ ((void));

void  exp_win2_rows_set    _ANSI_ARGS_ ((int fd, char* rows));
int   exp_win2_rows_get    _ANSI_ARGS_ ((int fd));
void  exp_win2_columns_set _ANSI_ARGS_ ((int fd, char* columns));
int   exp_win2_columns_get _ANSI_ARGS_ ((int fd));
