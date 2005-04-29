#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "uulib/fptools.h"
#include "uulib/uudeview.h"
#include "uulib/uuint.h"

static int
not_here (char *s)
{
    croak("%s not implemented", s);
    return -1;
}

static int
constant (char *name)
{
    errno = 0;
    switch (*name) {
    case 'A':
	if (strEQ(name, "ACT_COPYING")) return UUACT_COPYING;
	if (strEQ(name, "ACT_DECODING")) return UUACT_DECODING;
	if (strEQ(name, "ACT_ENCODING")) return UUACT_ENCODING;
	if (strEQ(name, "ACT_IDLE")) return UUACT_IDLE;
	if (strEQ(name, "ACT_SCANNING")) return UUACT_SCANNING;
    case 'F':
	if (strEQ(name, "FILE_DECODED")) return UUFILE_DECODED;
	if (strEQ(name, "FILE_ERROR")) return UUFILE_ERROR;
	if (strEQ(name, "FILE_MISPART")) return UUFILE_MISPART;
	if (strEQ(name, "FILE_NOBEGIN")) return UUFILE_NOBEGIN;
	if (strEQ(name, "FILE_NODATA")) return UUFILE_NODATA;
	if (strEQ(name, "FILE_NOEND")) return UUFILE_NOEND;
	if (strEQ(name, "FILE_OK")) return UUFILE_OK;
	if (strEQ(name, "FILE_READ")) return UUFILE_READ;
	if (strEQ(name, "FILE_TMPFILE")) return UUFILE_TMPFILE;
	break;
    case 'M':
	if (strEQ(name, "MSG_ERROR")) return UUMSG_ERROR;
	if (strEQ(name, "MSG_FATAL")) return UUMSG_FATAL;
	if (strEQ(name, "MSG_MESSAGE")) return UUMSG_MESSAGE;
	if (strEQ(name, "MSG_NOTE")) return UUMSG_NOTE;
	if (strEQ(name, "MSG_PANIC")) return UUMSG_PANIC;
	if (strEQ(name, "MSG_WARNING")) return UUMSG_WARNING;
    case 'O':
	if (strEQ(name, "OPT_VERSION")) return UUOPT_VERSION;
	if (strEQ(name, "OPT_FAST")) return UUOPT_FAST;
	if (strEQ(name, "OPT_DUMBNESS")) return UUOPT_DUMBNESS;
	if (strEQ(name, "OPT_BRACKPOL")) return UUOPT_BRACKPOL;
	if (strEQ(name, "OPT_VERBOSE")) return UUOPT_VERBOSE;
	if (strEQ(name, "OPT_DESPERATE")) return UUOPT_DESPERATE;
	if (strEQ(name, "OPT_IGNREPLY")) return UUOPT_IGNREPLY;
	if (strEQ(name, "OPT_OVERWRITE")) return UUOPT_OVERWRITE;
	if (strEQ(name, "OPT_SAVEPATH")) return UUOPT_SAVEPATH;
	if (strEQ(name, "OPT_IGNMODE")) return UUOPT_IGNMODE;
	if (strEQ(name, "OPT_DEBUG")) return UUOPT_DEBUG;
	if (strEQ(name, "OPT_ERRNO")) return UUOPT_ERRNO;
	if (strEQ(name, "OPT_PROGRESS")) return UUOPT_PROGRESS;
	if (strEQ(name, "OPT_USETEXT")) return UUOPT_USETEXT;
	if (strEQ(name, "OPT_PREAMB")) return UUOPT_PREAMB;
	if (strEQ(name, "OPT_TINYB64")) return UUOPT_TINYB64;
	if (strEQ(name, "OPT_ENCEXT")) return UUOPT_ENCEXT;
	if (strEQ(name, "OPT_REMOVE")) return UUOPT_REMOVE;
	if (strEQ(name, "OPT_MOREMIME")) return UUOPT_MOREMIME;
	if (strEQ(name, "OPT_DOTDOT")) return UUOPT_DOTDOT;
    case 'R':
	if (strEQ(name, "RET_CANCEL")) return UURET_CANCEL;
	if (strEQ(name, "RET_CONT")) return UURET_CONT;
	if (strEQ(name, "RET_EXISTS")) return UURET_EXISTS;
	if (strEQ(name, "RET_ILLVAL")) return UURET_ILLVAL;
	if (strEQ(name, "RET_IOERR")) return UURET_IOERR;
	if (strEQ(name, "RET_NODATA")) return UURET_NODATA;
	if (strEQ(name, "RET_NOEND")) return UURET_NOEND;
	if (strEQ(name, "RET_NOMEM")) return UURET_NOMEM;
	if (strEQ(name, "RET_OK")) return UURET_OK;
	if (strEQ(name, "RET_UNSUP")) return UURET_UNSUP;
    case 'B':
	if (strEQ(name, "B64_ENCODED")) return B64ENCODED;
	if (strEQ(name, "BH_ENCODED")) return BH_ENCODED;
    case 'P':
	if (strEQ(name, "PT_ENCODED")) return PT_ENCODED;
    case 'Q':
	if (strEQ(name, "QP_ENCODED")) return QP_ENCODED;
    case 'U':
	if (strEQ(name, "UU_ENCODED")) return UU_ENCODED;
    case 'X':
	if (strEQ(name, "XX_ENCODED")) return XX_ENCODED;
    case 'Y':
	if (strEQ(name, "YENC_ENCODED")) return YENC_ENCODED;
    }
    errno = EINVAL;
    return 0;
}

static void
uu_msg_callback (void *cb, char *msg, int level)
{
  dSP;
  
  ENTER; SAVETMPS; PUSHMARK (SP); EXTEND (SP, 2);

  PUSHs (sv_2mortal (newSVpv (msg, 0)));
  PUSHs (sv_2mortal (newSViv (level)));

  PUTBACK; (void) perl_call_sv ((SV *)cb, G_VOID|G_DISCARD); SPAGAIN;
  PUTBACK; FREETMPS; LEAVE;
}

static int
uu_busy_callback (void *cb, uuprogress *uup)
{
  dSP;
  int count;
  int retval;
  
  ENTER; SAVETMPS; PUSHMARK (SP); EXTEND (SP, 6);

  PUSHs (sv_2mortal (newSViv (uup->action)));
  PUSHs (sv_2mortal (newSVpv (uup->curfile, 0)));
  PUSHs (sv_2mortal (newSViv (uup->partno)));
  PUSHs (sv_2mortal (newSViv (uup->numparts)));
  PUSHs (sv_2mortal (newSViv (uup->fsize)));
  PUSHs (sv_2mortal (newSViv (uup->percent)));

  PUTBACK; count = perl_call_sv ((SV *)cb, G_SCALAR); SPAGAIN;

  if (count != 1)
    croak ("busycallback perl callback returned more than one argument");

  retval = POPi;

  PUTBACK; FREETMPS; LEAVE;

  return retval;
}

static char *
uu_fnamefilter_callback (void *cb, char *fname)
{
  dSP;
  int count;
  static char *str;
  
  ENTER; SAVETMPS; PUSHMARK (SP); EXTEND (SP, 1);

  PUSHs (sv_2mortal (newSVpv (fname, 0)));

  PUTBACK; count = perl_call_sv ((SV *)cb, G_SCALAR); SPAGAIN;

  if (count != 1)
    croak ("fnamefilter perl callback returned more than one argument");

  _FP_free(str); str = _FP_strdup (POPp);

  PUTBACK; FREETMPS; LEAVE;

  return str;
}

static int
uu_file_callback (void *cb, char *id, char *fname, int retrieve)
{
  dSP;
  int count;
  int retval;
  SV *xfname = newSVpv ("", 0);
  STRLEN dc;
  
  ENTER; SAVETMPS; PUSHMARK (SP); EXTEND (SP, 3);

  PUSHs (sv_2mortal (newSVpv (id, 0)));
  PUSHs (sv_2mortal (xfname));
  PUSHs (sv_2mortal (newSViv (retrieve)));

  PUTBACK; count = perl_call_sv ((SV *)cb, G_SCALAR); SPAGAIN;

  if (count != 1)
    croak ("filecallback perl callback returned more than one argument");

  strcpy (fname, SvPV (xfname, dc));

  retval = POPi;

  PUTBACK; FREETMPS; LEAVE;

  return retval;
}

static char *
uu_filename_callback (void *cb, char *subject, char *filename)
{
  dSP;
  int count;
  SV *retval;
  STRLEN dc;
  
  ENTER; SAVETMPS; PUSHMARK(SP); EXTEND(SP,3);

  PUSHs(sv_2mortal(newSVpv(subject, 0)));
  PUSHs(filename ? sv_2mortal(newSVpv(filename, 0)) : &PL_sv_undef);

  PUTBACK; count = perl_call_sv ((SV *)cb, G_ARRAY); SPAGAIN;

  if (count > 1)
    croak ("filenamecallback perl callback returned more than one argument");

  if (count)
    {
      _FP_free (filename);

      retval = POPs;

      if (SvOK (retval))
        {
          STRLEN len;
          char *fn = SvPV (retval, len);

          filename = malloc (len + 1);

          if (filename)
            {
              memcpy (filename, fn, len);
              filename[len] = 0;
            }
        }
      else
        filename = 0;
    }

  PUTBACK; FREETMPS; LEAVE;

  return filename;
}

static SV *uu_msg_sv, *uu_busy_sv, *uu_file_sv, *uu_fnamefilter_sv, *uu_filename_sv;

#define FUNC_CB(cb) (void *)(sv_setsv (cb ## _sv, func), cb ## _sv), func ? cb ## _callback : NULL

static int
uu_info_file (void *cb, char *info)
{
  dSP;
  int count;
  int retval;
  
  ENTER; SAVETMPS; PUSHMARK(SP); EXTEND(SP,1);

  PUSHs(sv_2mortal(newSVpv(info,0)));

  PUTBACK; count = perl_call_sv ((SV *)cb, G_SCALAR); SPAGAIN;

  if (count != 1)
    croak ("info_file perl callback returned more than one argument");

  retval = POPi;

  PUTBACK; FREETMPS; LEAVE;

  return retval;
}

static int
uu_opt_isstring (int opt)
{
  switch (opt)
    {
      case UUOPT_VERSION:
      case UUOPT_SAVEPATH:
      case UUOPT_ENCEXT:
         return 1;
      default:
         return 0;
    }
}

static int uu_initialized;

MODULE = Convert::UUlib		PACKAGE = Convert::UUlib		PREFIX = UU

PROTOTYPES: ENABLE

int
constant (name)
	char *		name


void
UUInitialize ()
	CODE:
        if (!uu_initialized)
          {
            int retval;
            
            if ((retval = UUInitialize ()) != UURET_OK)
              croak ("unable to initialize uudeview library (%s)", UUstrerror (retval));
 
            uu_initialized = 1;
          }

void
UUCleanUp ()
	CODE:
       	if (uu_initialized)
          UUCleanUp ();

        uu_initialized = 0; 

SV *
UUGetOption (opt)
	int	opt
        CODE:
	{
                if (opt == UUOPT_PROGRESS)
                  croak ("GetOption(UUOPT_PROGRESS) is not yet implemented");
                else if (uu_opt_isstring (opt))
                  {
        	    char cval[8192];

                    UUGetOption (opt, 0, cval, sizeof cval);
                    RETVAL = newSVpv (cval, 0);
                  }
                else
                  {
                    RETVAL = newSViv (UUGetOption (opt, 0, 0, 0));
                  }
	}
        OUTPUT:
        RETVAL

int
UUSetOption (opt, val)
	int	opt
        SV *	val
        CODE:
	{
                STRLEN dc;

                if (uu_opt_isstring (opt))
                  RETVAL = UUSetOption (opt, 0, SvPV (val, dc));
                else
                  RETVAL = UUSetOption (opt, SvIV (val), (void *)0);
	}
        OUTPUT:
        RETVAL

char *
UUstrerror (errcode)
	int	errcode

void
UUSetMsgCallback (func = 0)
	SV *	func
	CODE:
	UUSetMsgCallback (FUNC_CB(uu_msg));

void
UUSetBusyCallback (func = 0,msecs = 1000)
	SV *	func
        long	msecs
	CODE:
	UUSetBusyCallback (FUNC_CB(uu_busy), msecs);

void
UUSetFileCallback (func = 0)
	SV *	func
	CODE:
	UUSetFileCallback (FUNC_CB(uu_file));

void
UUSetFNameFilter (func = 0)
	SV *	func
	CODE:
	UUSetFNameFilter (FUNC_CB(uu_fnamefilter));

void
UUSetFileNameCallback (func = 0)
	SV *	func
	CODE:
	UUSetFileNameCallback (FUNC_CB(uu_filename));

char *
UUFNameFilter (fname)
	char *	fname

void
UULoadFile (fname, id = 0, delflag = 0, partno = -1)
	char *	fname
	char *	id
	int	delflag
        int	partno
        PPCODE:
	{	
	        int count;
                
	        XPUSHs (sv_2mortal (newSViv (UULoadFileWithPartNo (fname, id, delflag, partno, &count))));
                if (GIMME_V == G_ARRAY)
                  XPUSHs (sv_2mortal (newSViv (count)));
	}

int
UUSmerge (pass)
	int	pass

int
UUQuickDecode(datain,dataout,boundary,maxpos)
	FILE *	datain
	FILE *	dataout
	char *	boundary
	long	maxpos

int
UUEncodeMulti(outfile,infile,infname,encoding,outfname,mimetype,filemode)
	FILE *	outfile
	FILE *	infile
	char *	infname
	int	encoding
	char *	outfname
	char *	mimetype
	int	filemode

int
UUEncodePartial(outfile,infile,infname,encoding,outfname,mimetype,filemode,partno,linperfile)
	FILE *	outfile
	FILE *	infile
	char *	infname
	int	encoding
	char *	outfname
	char *	mimetype
	int	filemode
	int	partno
	long	linperfile

int
UUEncodeToStream(outfile,infile,infname,encoding,outfname,filemode)
	FILE *	outfile
	FILE *	infile
	char *	infname
	int	encoding
	char *	outfname
	int	filemode

int
UUEncodeToFile(infile,infname,encoding,outfname,diskname,linperfile)
	FILE *	infile
	char *	infname
	int	encoding
	char *	outfname
	char *	diskname
	long	linperfile

int
UUE_PrepSingle(outfile,infile,infname,encoding,outfname,filemode,destination,from,subject,isemail)
	FILE *	outfile
	FILE *	infile
	char *	infname
	int	encoding
	char *	outfname
	int	filemode
	char *	destination
	char *	from
	char *	subject
	int	isemail

int
UUE_PrepPartial(outfile,infile,infname,encoding,outfname,filemode,partno,linperfile,filesize,destination,from,subject,isemail)
	FILE *	outfile
	FILE *	infile
	char *	infname
	int	encoding
	char *	outfname
	int	filemode
        int	partno
        long	linperfile
        long	filesize
	char *	destination
	char *	from
	char *	subject
	int	isemail

uulist *
UUGetFileListItem (num)
	int	num

MODULE = Convert::UUlib		PACKAGE = Convert::UUlib::Item

int
rename (item, newname)
	uulist *item
	char *	newname
        CODE:
        RETVAL = UURenameFile (item, newname);
	OUTPUT:
        RETVAL

int
decode_temp (item)
	uulist *item
        CODE:
        RETVAL = UUDecodeToTemp (item);
	OUTPUT:
        RETVAL

int
remove_temp (item)
	uulist *item
        CODE:
        RETVAL = UURemoveTemp (item);
	OUTPUT:
        RETVAL

int
decode (item, target = 0)
	uulist *item
	char *	target
        CODE:
        RETVAL = UUDecodeFile (item, target);
	OUTPUT:
        RETVAL

void
info (item, func)
	uulist *item
	SV *	func
        CODE:
        UUInfoFile (item,(void *)func, uu_info_file);

short
state(li)
	uulist *li
        CODE:
        RETVAL = li->state;
        OUTPUT:
        RETVAL

short
mode(li,newmode=0)
	uulist *li
        short	newmode
        CODE:
        if (newmode)
	  li->mode = newmode;
        RETVAL = li->mode;
        OUTPUT:
        RETVAL

short
uudet(li)
	uulist *li
        CODE:
        RETVAL = li->uudet;
        OUTPUT:
        RETVAL

long
size(li)
	uulist *li
        CODE:
        RETVAL = li->size;
        OUTPUT:
        RETVAL

char *
filename (li, newfilename = 0)
	uulist *li
        char *	newfilename
        CODE:
        if (newfilename)
	  {
            _FP_free (li->filename);
	    li->filename = _FP_strdup (newfilename);
          }
        RETVAL = li->filename;
        OUTPUT:
        RETVAL

char *
subfname (li)
	uulist *li
        CODE:
        RETVAL = li->subfname;
        OUTPUT:
        RETVAL

char *
mimeid (li)
	uulist *li
        CODE:
        RETVAL = li->mimeid;
        OUTPUT:
        RETVAL

char *
mimetype (li)
	uulist *li
        CODE:
        RETVAL = li->mimetype;
        OUTPUT:
        RETVAL

char *
binfile (li)
	uulist *li
        CODE:
        RETVAL = li->binfile;
        OUTPUT:
        RETVAL

# methods accessing internal data(!)

void
parts (li)
	uulist *li
        PPCODE:
	{
        	struct _uufile *p = li->thisfile;

                while (p)
                  {
                    HV *pi = newHV ();

                    hv_store (pi, "partno"  , 6, newSViv (p->partno)         , 0);

                    if (p->filename)
                      hv_store (pi, "filename", 8, newSVpv (p->filename, 0)    , 0);
                    if(p->subfname)
                      hv_store (pi, "subfname", 8, newSVpv (p->subfname, 0)    , 0);
                    if(p->mimeid)
                      hv_store (pi, "mimeid"  , 6, newSVpv (p->mimeid  , 0)    , 0);
                    if(p->mimetype)
                      hv_store (pi, "mimetype", 8, newSVpv (p->mimetype, 0)    , 0);
                    if (p->data->subject)
                      hv_store (pi, "subject" , 7, newSVpv (p->data->subject,0), 0);
                    if (p->data->origin)
                      hv_store (pi, "origin"  , 6, newSVpv (p->data->origin ,0), 0);
                    if (p->data->sfname)
                      hv_store (pi, "sfname"  , 6, newSVpv (p->data->sfname ,0), 0);

                    XPUSHs (sv_2mortal (newRV_noinc ((SV *)pi)));

                    p = p->NEXT;
                  }
        }

BOOT:
  uu_msg_sv		= newSVsv(&PL_sv_undef);
  uu_busy_sv		= newSVsv(&PL_sv_undef);
  uu_file_sv		= newSVsv(&PL_sv_undef);
  uu_fnamefilter_sv	= newSVsv(&PL_sv_undef);
  uu_filename_sv	= newSVsv(&PL_sv_undef);

