#pragma prototyped

#include <ast.h>
#include <cs.h>
#include <msg.h>
#include "common.h"
#include "code.h"
#include "mem.h"
#include "tbl.h"
#include "exec.h"
#include "cs2l.h"

int C2Lopen (char *name, char *mode, FILE **ifp, FILE **ofp) {
    int fd;

    if ((fd = csopen (name, CS_OPEN_READ)) == -1)
        return -1;
    fcntl (fd, F_SETFD, FD_CLOEXEC);
    *ifp = fdopen (fd, "r"), *ofp = fdopen (fd, "a+");
    return 0;
}

/* LEFTY builtin */
int C2Lreadcsmessage (int argc, lvar_t *argv) {

#if 0 /* not finished yet */
    io_t *p;
    int ioi, n;
    Msg_call_t msg;
    Tobj to;
    int tm;

    ioi = Tgetnumber (argv[0].o);
    if (ioi < 0 || ioi >= ion)
        return L_FAILURE;

    p = &iop[ioi];
    fseek (p->ofp, 0L, 1);
    if ((n = msgrecv (fileno (p->ifp), &msg)) <= 0)
        return L_FAILURE;
    to = Ttable (6);
    tm = Mpushmark (to);
    Tinss (to, "id", Tinteger (MSG_CHANNEL_USR (msg.channel)));
    Tinss (to, "pid", Tinteger (MSG_CHANNEL_SYS (msg.channel)));
    rtno = to;
    Mpopmark (tm);
#endif

    return L_SUCCESS;
}
