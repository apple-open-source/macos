#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "ppport.h"
#include "portable.h"


#define MAX_INITIAL_VALUELEN_VARNAME "File::ExtAttr::MAX_INITIAL_VALUELEN"
                                        /* Richard, fixme! */


MODULE = File::ExtAttr        PACKAGE = File::ExtAttr		

PROTOTYPES: ENABLE


int 
_setfattr (path, attrname, attrvalueSV, flags = 0)
         const char *path
         const char *attrname
         SV * attrvalueSV
         HV * flags
    PREINIT:
        STRLEN slen;
        char * attrvalue;
        int rc;

    CODE:
        attrvalue = SvPV(attrvalueSV, slen);
        rc = portable_setxattr(path, attrname, attrvalue, slen, flags);
        if (rc < 0)
          errno = -rc;
        RETVAL = (rc == 0);

    OUTPUT: 
        RETVAL


int 
_fsetfattr (fd, attrname, attrvalueSV, flags = 0)
         int fd
         const char *attrname
         SV * attrvalueSV
         HV * flags
    PREINIT:
        STRLEN slen;
        char * attrvalue;
        int rc;

    CODE:
        attrvalue = SvPV(attrvalueSV, slen);
        rc = portable_fsetxattr(fd, attrname, attrvalue, slen, flags);
        if (rc < 0)
          errno = -rc;
        RETVAL = (rc == 0);

    OUTPUT: 
        RETVAL


SV *
_getfattr(path, attrname, flags = 0)
        const char *path
        const char *attrname
        HV * flags
   PREINIT:
        char * attrvalue;
        int attrlen;
        ssize_t buflen;

   CODE:
        buflen = portable_lenxattr(path, attrname, flags);
        if (buflen <= 0)
	  buflen = SvIV(get_sv(MAX_INITIAL_VALUELEN_VARNAME, FALSE));

        attrvalue = NULL;
        Newz(1, attrvalue, buflen, char);

        attrlen = portable_getxattr(path, attrname, attrvalue, buflen, flags);
        if (attrlen < 0){

            //key not found, just return undef
            if(errno == ENOATTR){
                Safefree(attrvalue);
                errno = -attrlen;
                XSRETURN_UNDEF;

            //return undef
            }else{
                Safefree(attrvalue);
                errno = -attrlen;
                XSRETURN_UNDEF;
            }
        }

        RETVAL = newSVpv(attrvalue, attrlen);
        Safefree(attrvalue);

    OUTPUT:
        RETVAL


SV *
_fgetfattr(fd, attrname, flags = 0)
        int fd
        const char *attrname
        HV * flags
   PREINIT:
        char * attrvalue;
        int attrlen;
        ssize_t buflen;

   CODE:
        buflen = portable_flenxattr(fd, attrname, flags);
        if (buflen <= 0)
	  buflen = SvIV(get_sv(MAX_INITIAL_VALUELEN_VARNAME, FALSE));

        attrvalue = NULL;
        Newz(1, attrvalue, buflen, char);

        attrlen = portable_fgetxattr(fd, attrname, attrvalue, buflen, flags);
        if (attrlen < 0){

            //key not found, just return undef
            if(errno == ENOATTR){
                Safefree(attrvalue);
                errno = -attrlen;
                XSRETURN_UNDEF;

            //return undef
            }else{
                Safefree(attrvalue);
                errno = -attrlen;
                XSRETURN_UNDEF;
            }
        }

        RETVAL = newSVpv(attrvalue, attrlen);
        Safefree(attrvalue);

    OUTPUT:
        RETVAL


int 
_delfattr (path, attrname, flags = 0)
        const char *path
        const char *attrname
        HV * flags
    PREINIT:
        int rc;

    CODE:
        rc = portable_removexattr(path, attrname, flags);
        if (rc < 0)
          errno = -rc;
        RETVAL = (rc == 0);
    
    OUTPUT: 
        RETVAL


int 
_fdelfattr (fd, attrname, flags = 0)
        int fd
        const char *attrname
        HV * flags
    PREINIT:
        int rc;

    CODE:
        rc = portable_fremovexattr(fd, attrname, flags);
        if (rc < 0)
          errno = -rc;
        RETVAL = (rc == 0);
    
    OUTPUT: 
        RETVAL

void
_listfattr (path, fd, flags = 0)
        const char *path
        int fd
        HV * flags
    PREINIT:
        ssize_t size, ret;
        char *namebuf = NULL;
        char *nameptr;

    PPCODE:
        if(fd == -1)
            size = portable_listxattr(path, NULL, 0, flags);
        else
            size = portable_flistxattr(fd, NULL, 0, flags);

        if (size < 0)
        {
            errno = -(int) size;
            XSRETURN_UNDEF;
        } else if (size == 0)
        {
            XSRETURN_EMPTY;
        }

        namebuf = malloc(size);

        if (fd == -1)
            ret = portable_listxattr(path, namebuf, size, flags);
        else
            ret = portable_flistxattr(fd, namebuf, size, flags);

        // There could be a race condition here, if someone adds a new
        // attribute between the two listxattr calls. However it just means we
        // might return ERANGE.

        if (ret < 0)
        {
            free(namebuf);
            errno = -ret;
            XSRETURN_UNDEF;
        } else if (ret == 0)
        {
            free(namebuf);
            XSRETURN_EMPTY;
        }

        nameptr = namebuf;

        while(nameptr < namebuf + ret)
        {
          char *endptr = nameptr;
          while(*endptr++ != '\0');

          // endptr will now point one past the end..

          XPUSHs(sv_2mortal(newSVpvn(nameptr, endptr - nameptr - 1)));

          // nameptr could now point past the end of namebuf
          nameptr = endptr;
        }

        free(namebuf);

void
_listfattrns (path, fd, flags = 0)
        const char *path
        int fd
        HV * flags
    PREINIT:
        ssize_t size, ret;
        char *namebuf = NULL;
        char *nameptr;

    PPCODE:
        if(fd == -1)
            size = portable_listxattrns(path, NULL, 0, flags);
        else
            size = portable_flistxattrns(fd, NULL, 0, flags);

        if (size < 0)
        {
            errno = -(int) size;
            XSRETURN_UNDEF;
        } else if (size == 0)
        {
            XSRETURN_EMPTY;
        }

        namebuf = malloc(size);

        if (fd == -1)
            ret = portable_listxattrns(path, namebuf, size, flags);
        else
            ret = portable_flistxattrns(fd, namebuf, size, flags);

        // There could be a race condition here, if someone adds a new
        // attribute between the two listxattr calls. However it just means we
        // might return ERANGE.

        if (ret < 0)
        {
            free(namebuf);
            errno = -ret;
            XSRETURN_UNDEF;
        } else if (ret == 0)
        {
            free(namebuf);
            XSRETURN_EMPTY;
        }

        nameptr = namebuf;

        while(nameptr < namebuf + ret)
        {
          char *endptr = nameptr;
          while(*endptr++ != '\0');

          // endptr will now point one past the end..

          XPUSHs(sv_2mortal(newSVpvn(nameptr, endptr - nameptr - 1)));

          // nameptr could now point past the end of namebuf
          nameptr = endptr;
        }

        free(namebuf);
