#include <sys/param.h>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pwd.h>
#include <grp.h>
#include <err.h>
#include <sysexits.h>

#include <netsmb/smb_lib.h>
#include <netsmb/smb_conn.h>

#include "common.h"

#define	DEFBIT(bit)	{bit, #bit}

