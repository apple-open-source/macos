/*
  Copyright (c) 1990-1999 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 1999-Oct-05 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, both of these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.cdrom.com/pub/infozip/license.html
*/
/*
 *    vms_pk.c  by Igor Mandrichenko
 *
 *    version 2.0       20-Mar-1993
 *                      Generates PKWARE version of VMS attributes
 *                      extra field according to appnote 2.0.
 *                      Uses low level QIO-ACP interface.
 *    version 2.0-1     10-Apr-1993
 *                      Save ACLs
 *    version 2.1       24-Aug-1993
 *                      By default produce 0x010C extra record ID instead of
 *                      PKWARE's 0x000C. The format is mostly compatible with
 *                      PKWARE.
 *                      Incompatibility (?): zip produces multiple ACE
 *                      fields.
 *    version 2.1-1     Clean extra fields in vms_get_attributes().
 *                      Fixed bug with EOF.
 *    version 2.1-2     15-Sep-1995, Chr. Spieler
 *                      Removed extra fields cleanup from vms_get_attributes().
 *                      This is now done in zipup.c
 *                      Modified (according to UnZip's vms.[ch]) the fib stuff
 *                      for DEC C (AXP,VAX) support.
 *    version 2.2       28-Sep-1995, Chr. Spieler
 *                      Reorganized code for easier maintance of the two
 *                      incompatible flavours (IM style and PK style) VMS
 *                      attribute support.  Generic functions (common to
 *                      both flavours) are now collected in a `wrapper'
 *                      source file that includes one of the VMS attribute
 *                      handlers.
 *                      Made extra block header conforming to PKware's
 *                      specification (extra block header has a length
 *                      of four bytes, two bytes for a signature, and two
 *                      bytes for the length of the block excluding this
 *                      header.
 *    version 2.2-1     19-Oct-1995, Chr. Spieler
 *                      Fixed bug in CRC calculation.
 *                      Use official PK VMS extra field id.
 *    version 2.2-2     21-Nov-1997, Chr. Spieler
 *                      Fixed bug in vms_get_attributes() for directory
 *                      entries (access to uninitialized ioctx record).
 *                      Removed unused second arg for vms_open().
 */

#ifdef VMS                      /* For VMS only ! */

#ifndef __zip_h
#include "zip.h"
#endif

#ifndef __SSDEF_LOADED
#include <ssdef.h>
#endif

#ifndef VMS_ZIP
#define VMS_ZIP
#endif
#include "vms/vms.h"
#include "vms/vmsdefs.h"

#ifndef ERR
#define ERR(x) (((x)&1)==0)
#endif

#ifndef NULL
#define NULL (void*)(0L)
#endif

#ifndef UTIL

static struct PK_info PK_def_info =
{
        ATR$C_RECATTR,  ATR$S_RECATTR,  {0},
        ATR$C_UCHAR,    ATR$S_UCHAR,    {0},
        ATR$C_CREDATE,  ATR$S_CREDATE,  {0},
        ATR$C_REVDATE,  ATR$S_REVDATE,  {0},
        ATR$C_EXPDATE,  ATR$S_EXPDATE,  {0},
        ATR$C_BAKDATE,  ATR$S_BAKDATE,  {0},
        ATR$C_ASCDATES, sizeof(ush),    0,
        ATR$C_UIC,      ATR$S_UIC,      {0},
        ATR$C_FPRO,     ATR$S_FPRO,     {0},
        ATR$C_RPRO,     ATR$S_RPRO,     {0},
        ATR$C_JOURNAL,  ATR$S_JOURNAL,  {0}
};

/* Forward declarations of public functions: */
struct ioctx *vms_open(char *file);
int  vms_read(register struct ioctx *ctx,
              register char *buf, register int size);
int  vms_error(struct ioctx *ctx);
int  vms_rewind(struct ioctx *ctx);
int  vms_get_attributes(struct ioctx *ctx, struct zlist far *z,
                        iztimes *z_utim);
int  vms_close(struct ioctx *ctx);

/*---------------*
 |  vms_open()   |
 *---------------*
 |  This routine opens file for reading fetching its attributes.
 |  Returns pointer to file description structure.
 */

struct ioctx *vms_open(file)
char *file;
{
    static struct atrdef        Atr[VMS_MAX_ATRCNT+1];
    static struct NAM           Nam;
    static struct fibdef        Fib;
    static struct dsc$descriptor FibDesc =
        {sizeof(Fib),DSC$K_DTYPE_Z,DSC$K_CLASS_S,(char *)&Fib};
    static struct dsc$descriptor_s DevDesc =
        {0,DSC$K_DTYPE_T,DSC$K_CLASS_S,&Nam.nam$t_dvi[1]};
    static struct dsc$descriptor_s FileName =
        {0,DSC$K_DTYPE_T,DSC$K_CLASS_S,0};
    static char EName[NAM$C_MAXRSS];
    static char RName[NAM$C_MAXRSS];

    struct FAB  Fab;
    register struct ioctx *ctx;
    register struct fatdef *fat;
    int status;
    int i;
    ulg efblk, hiblk;

    if ( (ctx=(struct ioctx *)malloc(sizeof(struct ioctx))) == NULL )
        return NULL;
    ctx -> PKi = PK_def_info;

#define FILL_REQ(ix,id,b)   {       \
    Atr[ix].atr$l_addr = &(b);      \
    Atr[ix].atr$w_type = (id);      \
    Atr[ix].atr$w_size = sizeof(b); \
}
    FILL_REQ(0, ATR$C_RECATTR,  ctx->PKi.ra);
    FILL_REQ(1, ATR$C_UCHAR,    ctx->PKi.uc);
    FILL_REQ(2, ATR$C_REVDATE,  ctx->PKi.rd);
    FILL_REQ(3, ATR$C_EXPDATE,  ctx->PKi.ed);
    FILL_REQ(4, ATR$C_CREDATE,  ctx->PKi.cd);
    FILL_REQ(5, ATR$C_BAKDATE,  ctx->PKi.bd);
    FILL_REQ(6, ATR$C_ASCDATES, ctx->PKi.rn);
    FILL_REQ(7, ATR$C_JOURNAL,  ctx->PKi.jr);
    FILL_REQ(8, ATR$C_RPRO,     ctx->PKi.rp);
    FILL_REQ(9, ATR$C_FPRO,     ctx->PKi.fp);
    FILL_REQ(10,ATR$C_UIC,      ctx->PKi.ui);
    FILL_REQ(11,ATR$C_ACLLENGTH,ctx->acllen);
    FILL_REQ(12,ATR$C_READACL,  ctx->aclbuf);

    Atr[13].atr$w_type = 0;     /* End of ATR list */
    Atr[13].atr$w_size = 0;
    Atr[13].atr$l_addr = (byte *)NULL;

    /* initialize RMS structures, we need a NAM to retrieve the FID */
    Fab = cc$rms_fab;
    Fab.fab$l_fna = file ; /* name of file */
    Fab.fab$b_fns = strlen(file);
    Fab.fab$l_nam = &Nam; /* FAB has an associated NAM */
    Nam = cc$rms_nam;
    Nam.nam$l_esa = EName; /* expanded filename */
    Nam.nam$b_ess = sizeof(EName);
    Nam.nam$l_rsa = RName; /* resultant filename */
    Nam.nam$b_rss = sizeof(RName);

    /* do $PARSE and $SEARCH here */
    status = sys$parse(&Fab);
    if (!(status & 1)) return NULL;

    /* search for the first file.. If none signal error */
    status = sys$search(&Fab);
    if (!(status & 1)) return NULL;

    /* initialize Device name length, note that this points into the NAM
         to get the device name filled in by the $PARSE, $SEARCH services */
    DevDesc.dsc$w_length = Nam.nam$t_dvi[0];

    status = sys$assign(&DevDesc,&ctx->chan,0,0);
    if (!(status & 1)) return NULL;

    FileName.dsc$a_pointer = Nam.nam$l_name;
    FileName.dsc$w_length = Nam.nam$b_name+Nam.nam$b_type+Nam.nam$b_ver;

    /* Initialize the FIB */
    Fib.FIB$L_ACCTL = FIB$M_NOWRITE;
    for (i=0;i<3;i++)
        Fib.FIB$W_FID[i]=Nam.nam$w_fid[i];
    for (i=0;i<3;i++)
        Fib.FIB$W_DID[i]=Nam.nam$w_did[i];

    /* Use the IO$_ACCESS function to return info about the file */
    status = sys$qiow(0,ctx->chan,IO$_ACCESS|IO$M_ACCESS,&ctx->iosb,0,0,
                    &FibDesc,&FileName,0,0,&Atr,0);

    if (ERR(status) || ERR(status = ctx->iosb.status))
    {
        vms_close(ctx);
        return NULL;
    }

    fat = (struct fatdef *)&(ctx -> PKi.ra);

#define SWAPW(x)        ( (((x)>>16)&0xFFFF) + ((x)<<16) )

    efblk = SWAPW(fat->fat$l_efblk);
    hiblk = SWAPW(fat->fat$l_hiblk);

    if( efblk == 0 && fat -> fat$w_ffbyte == 0 )
        ctx -> size =
        ctx -> rest = hiblk * 512;
    else
        ctx -> size =
        ctx -> rest = (efblk - 1) * 512 + fat -> fat$w_ffbyte;

    ctx -> status = SS$_NORMAL;
    ctx -> vbn = 1;
    return ctx;
}

#define KByte 1024

/*----------------*
 |   vms_read()   |
 *----------------*
 |   Reads file block by block into the buffer.
 |   Stops on EOF, returns number of bytes actually read.
 |   Note: size of the buffer must be greater than or equal to 512 !
 */

int vms_read(ctx, buf, size)
register struct ioctx *ctx;
register char *buf;
register int size;
{
    int status;
    long nr=0;

    if (ctx -> rest <= 0 || ctx -> status == SS$_ENDOFFILE)
        return 0;               /* Eof */

    if(size <= 0)
        return 0;

    if(size > 16*KByte)
        size = 16*KByte;
    else if(size > 512)
        size &= ~511L;          /* Round to integer number of blocks */

    do
    {
        status = sys$qiow(0, ctx->chan, IO$_READVBLK,
            &ctx->iosb, 0, 0,
            buf, size, ctx->vbn,0,0,0);

        ctx->vbn += size>>9;
        if( size < 512 )
                ++ctx->vbn;

        if ( !ERR(status) )
                status = ctx->iosb.status;

        if ( !ERR(status) || status == SS$_ENDOFFILE )
        {
            register int count;

            if ( status == SS$_ENDOFFILE )
                count = ctx->rest;
            else
                count = ctx->iosb.count;

            size -= count;
            buf  += count;
            nr   += count;
        }
    } while ( !ERR(status) && size > 0 );

    if (!ERR(status))
    {
        ctx -> status = SS$_NORMAL;
        ctx -> rest -= nr;
    }
    else if (status == SS$_ENDOFFILE)
    {
        ctx -> status = SS$_ENDOFFILE;
        ctx -> rest = 0;
    }
    else
    {
        ctx -> status = status;
        ctx -> rest -= nr;
    }
    return nr;
}

/*-----------------*
 |   vms_error()   |
 *-----------------*
 |   Returns whether last operation on the file caused an error
 */

int vms_error(ctx)
struct ioctx *ctx;
{   /* EOF is not actual error */
    return ERR(ctx->status) && (ctx->status != SS$_ENDOFFILE);
}

/*------------------*
 |   vms_rewind()   |
 *------------------*
 |   Rewinds file to the beginning for the next vms_read().
 */

int vms_rewind(ctx)
struct ioctx *ctx;
{
    ctx -> vbn = 1;
    ctx -> rest = ctx -> size;
    return 0;
}

/*--------------------------*
 |   vms_get_attributes()   |
 *--------------------------*
 |   Malloc a PKWARE extra field and fill with file attributes. Returns
 |   error number of the ZE_??? class.
 |   If the passed ioctx record "FILE *" pointer is NULL, vms_open() is
 |   called to fetch the file attributes.
 |   When `vms_native' is not set, a generic "UT" type timestamp extra
 |   field is generated instead.
 */

int vms_get_attributes(ctx, z, z_utim)
struct ioctx *ctx;
struct zlist far *z;    /* zip entry to compress */
iztimes *z_utim;
{
    byte    *p, *b;
    struct  PK_header    *h;
    extent  l;
    int     notopened;

    if ( !vms_native )
    {
#ifdef USE_EF_UT_TIME
        /*
         *  A `portable' zipfile entry is created. Create an "UT" extra block
         *  containing UNIX style modification time stamp in UTC, which helps
         *  maintaining the `real' "last modified" time when the archive is
         *  transfered across time zone boundaries.
         */
#  ifdef IZ_CHECK_TZ
        if (!zp_tz_is_valid)
            return ZE_OK;       /* skip silently if no valid TZ info */
#  endif
        if ((b = (uch *)malloc(EB_HEADSIZE+EB_UT_LEN(1))) == NULL)
            return ZE_MEM;

        b[0]  = 'U';
        b[1]  = 'T';
        b[2]  = EB_UT_LEN(1);          /* length of data part of e.f. */
        b[3]  = 0;
        b[4]  = EB_UT_FL_MTIME;
        b[5]  = (byte)(z_utim->mtime);
        b[6]  = (byte)(z_utim->mtime >> 8);
        b[7]  = (byte)(z_utim->mtime >> 16);
        b[8]  = (byte)(z_utim->mtime >> 24);

        z->cext = z->ext = (EB_HEADSIZE+EB_UT_LEN(1));
        z->cextra = z->extra = (char*)b;
#endif /* USE_EF_UT_TIME */

        return ZE_OK;
    }

    notopened = (ctx == (struct ioctx *)NULL);
    if ( notopened && ((ctx = vms_open(z->name)) == (struct ioctx *)NULL) )
        return ZE_OPEN;

    l = PK_HEADER_SIZE + sizeof(ctx->PKi);
    if (ctx->acllen > 0)
        l += PK_FLDHDR_SIZE + ctx->acllen;

    b = (uch *)malloc(l);
    if ( b==NULL )
        return ZE_MEM;

    h = (struct PK_header *)b;
    h->tag = PK_SIGNATURE;
    h->size = l - EB_HEADSIZE;
    p = (h->data);

    /* Copy default set of attributes */
    memcpy(h->data, (char*)&(ctx->PKi), sizeof(ctx->PKi));
    p += sizeof(ctx->PKi);

    if ( ctx->acllen > 0 )
    {
        struct PK_field *f;

        if (dosify)
            zipwarn("file has ACL, may be incompatible with PKUNZIP","");

        f = (struct PK_field *)p;
        f->tag = ATR$C_ADDACLENT;
        f->size = ctx->acllen;
        memcpy((char *)&(f->value[0]), ctx->aclbuf, ctx->acllen);
        p += PK_FLDHDR_SIZE + ctx->acllen;
    }


    h->crc32 = CRCVAL_INITIAL;                  /* Init CRC register */
    h->crc32 = crc32(h->crc32, (uch *)(h->data), l - PK_HEADER_SIZE);

    z->ext = z->cext = l;
    z->extra = z->cextra = (char *)b;

    if (notopened)              /* close "ctx", if we have opened it here */
        vms_close(ctx);

    return ZE_OK;
}

int vms_close(ctx)
struct ioctx *ctx;
{
        sys$dassgn(ctx->chan);
        free(ctx);
        return 0;
}

#endif /* !_UTIL */
#endif /* VMS */
