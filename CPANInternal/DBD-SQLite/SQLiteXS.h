
#ifndef _SQLITEXS_H
#define _SQLITEXS_H   1

/************************************************************************
    DBI Specific Stuff - Added by Matt Sergeant
 ************************************************************************/
#define PERL_POLLUTE
#define PERL_NO_GET_CONTEXT
#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>
#include "ppport.h"

#define NEED_DBIXS_VERSION 93
#include <DBIXS.h>
#include "dbdimp.h"
#include "dbivport.h"
#include <dbd_xsh.h>

#include "sqlite3.h"

#endif
