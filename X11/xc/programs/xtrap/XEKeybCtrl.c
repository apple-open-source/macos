/* $XFree86: xc/programs/xtrap/XEKeybCtrl.c,v 1.1 2001/11/02 23:29:34 dawes Exp $ */
/*****************************************************************************
Copyright 1987, 1988, 1989, 1990, 1991, 1992 by Digital Equipment Corp., 
Maynard, MA

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*****************************************************************************/
/*
 *
 *  CONTRIBUTORS:
 *
 *      Dick Annicchiarico
 *      Robert Chesler
 *      Dan Coutu
 *      Gene Durso
 *      Marc Evans
 *      Alan Jamison
 *      Mark Henry
 *      Ken Miller
 *
 */
#ifndef vms
#include    <signal.h>
#else
#include    <descrip.h>                     /* Character string descriptors */
#include    <dvidef.h>                      /* GETDVI item codes */
#include    <devdef.h>                      /* device independent codes */
#include    <iodef.h>                       /* I/O function codes */
#include    <psldef.h>                      /* PSL definitions */
#include    <ssdef.h>                       /* System service codes */
#include    <stsdef.h>                      /* System status masks and codes */
#include    <ttdef.h>                       /* Terminal specific I/O defs */
#include    <tt2def.h>                      /* Terminal specific I/O defs */
#define CTRL_USER_MODE 3

/*----------*
 *  Macros  *
 *----------*/

#define     $CheckStatus(status)\
            if (!(status & 1)) return(status);

            /*  Allocate a quadword aligned VMS descriptor.
             *  NOTE: This supersedes the $DESCRIPTOR macro in DESCRIP.H.
             *  The only difference is the _align(QUADWORD) term.
             */
#define     $DESCRIPTOR_Q(name, string)\
                struct dsc$descriptor_s _align (QUADWORD) name = \
                { sizeof(string)-1, DSC$K_DTYPE_T, DSC$K_CLASS_S, string }

/*---------------------*
 *  Data Declarations  *
 *---------------------*/

static $DESCRIPTOR_Q (sys_input, "SYS$INPUT:");

static unsigned short
                comm_chan,                      /* Communication channel */
                comm_iosb[4];                   /* I/O status block */

static struct getdvi_itmlst_struct
    {                                           /* $GETDVI item list */
    unsigned short int
                buflen,
                item_code;
    unsigned int
                bufadr,
                retadr,
                eol;
    }  _align (LONGWORD) getdvi_itmlst   = { sizeof(int), 
                                             DVI$_DEVCHAR, 
                                             0,0,0 };

static unsigned int dvi_characteristics,return_length;

static struct exit_handler_blk
    {
    unsigned int    flink;
    void            (*exit_routine)();
    unsigned char   arg_cnt;
    unsigned char   null_byte;
    unsigned short  null_word;
    unsigned int    cond_value;
    } _align (LONGWORD) exit_block;

static unsigned int vms_condition;

#endif /* vms */



#include <X11/extensions/xtraplib.h>
#include <X11/extensions/xtraplibp.h>

#include "XEKeybCtrl.h"

int XEEnableCtrlKeys(void (*rtn)(int))
{
#ifndef vms
    signal(SIGINT, rtn); /* CTRL-C */
    return(1L);

#else /* vms */

    int status;

    /*  
     *  Provide the addresses of the longword to receive device chars
     *  and the return length of the information.
     */
    getdvi_itmlst.bufadr = &dvi_characteristics;
    getdvi_itmlst.retadr = &return_length;

    status = SYS$GETDVIW (0, 0, &sys_input, &getdvi_itmlst, 0, 0, 0, 0);
    $CheckStatus(status);


    /* If we have a terminal device, enable control-c and control-y */
    if (dvi_characteristics & DEV$M_TRM)
    {
        /* Assign a channel to the communication device. */
        status = SYS$ASSIGN ( &sys_input,    /* Device name */
                              &comm_chan,    /* Channel returned */
                              0, 0 );
        $CheckStatus(status);

        status = XEEnableCtrlC(rtn);
        $CheckStatus(status);

        status = XEEnableCtrlY(rtn);
        $CheckStatus(status);
    }
    return (SS$_NORMAL);

#endif /* vms */
}


int XEClearCtrlKeys(void)
{
#ifndef vms
    signal(SIGINT,  SIG_DFL); /* CTRL-C */
    return(1L);
#else  /* vms */
    int status;

    if (dvi_characteristics & DEV$M_TRM) 
    {                 
        status = SYS$DASSGN(comm_chan);
        $CheckStatus(status);
    }
    return (SS$_NORMAL);
#endif /* vms */
}

int XEEnableCtrlC(void (*rtn)(int))
{
#ifndef vms
    signal(SIGINT, rtn); /* CTRL-C */
    return(1);
#else
    int status;

    status = SYS$QIOW ( 0,                  /*  Now set the characteristics */
                        comm_chan,              /* Channel */
                        IO$_SETMODE|IO$M_CTRLCAST, /* Set ctrl_c */
                        comm_iosb,              /* iosb address */
                        0, 0,                   /*  */
                        rtn, 0,                 /* AST routine and param */
                        CTRL_USER_MODE, 0, 0, 0 );
    $CheckStatus(status);

    return (SS$_NORMAL);
#endif /* vms */
}

                                           
int XEEnableCtrlY(void (*rtn)(int))
{
#ifndef vms
    signal(SIGQUIT,rtn); /* CTRL-backslash */
    return(1);
#else /* vms */
    int status;

    status = SYS$QIOW ( 0,                      /* Set characteristics */
                        comm_chan,              /* Channel */
                        IO$_SETMODE|IO$M_CTRLYAST, /* Set ctrl_y */
                        comm_iosb,              /* iosb address */
                        0, 0,                   /* */
                        rtn, 0,                 /* AST routine and param */
                        CTRL_USER_MODE, 0, 0, 0 );
    $CheckStatus(status);

    return (SS$_NORMAL);
#endif /* vms */
}


int XEDeclExitHndlr(void (*rtn)(int))   
{
#ifndef vms
    return(1);  /* no real way for U*IX to do this */
#else /* vms */
    int status;

    /*
     *  The Exit handler routine must accept one argument.
     *  This argument will be the condition that signaled the
     *  the exit handler.
     */
    exit_block.exit_routine = rtn;
    exit_block.arg_cnt    = 1;              /* The condition code is the first argument */
    exit_block.cond_value = &vms_condition; /* Address of condition value written by VMS */
    
    status = SYS$DCLEXH (&exit_block);  /* Set up the condition handler */
    $CheckStatus(status);

    return (SS$_NORMAL);
#endif /* vms */
}
