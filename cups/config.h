/* config.h.  Generated automatically by configure.  */
/*
 * "$Id: config.h,v 1.20.2.1 2002/12/13 22:54:07 jlovell Exp $"
 *
 *   Configuration file for the Common UNIX Printing System (CUPS).
 *
 *   @configure_input@
 *
 *   Copyright 1997-2002 by Easy Software Products.
 *
 *   These coded instructions, statements, and computer programs are the
 *   property of Easy Software Products and are protected by Federal
 *   copyright law.  Distribution and use rights are outlined in the file
 *   "LICENSE.txt" which should have been included with this file.  If this
 *   file is missing or damaged please contact Easy Software Products
 *   at:
 *
 *       Attn: CUPS Licensing Information
 *       Easy Software Products
 *       44141 Airport View Drive, Suite 204
 *       Hollywood, Maryland 20636-3111 USA
 *
 *       Voice: (301) 373-9600
 *       EMail: cups-info@cups.org
 *         WWW: http://www.cups.org
 */

/*
 * Version of software...
 */

#define CUPS_SVERSION	"CUPS v1.1.15-2"


/*
 * Default user and group...
 */

#define CUPS_DEFAULT_USER "daemon"
#define CUPS_DEFAULT_GROUP "admin"


/*
 * Where are files stored?
 */

#define CUPS_LOCALEDIR "/usr/share/locale"
#define CUPS_SERVERROOT "/private/etc/cups"
#define CUPS_SERVERBIN "/usr/libexec/cups"
#define CUPS_DOCROOT "/usr/share/doc/cups"
#define CUPS_REQUESTS "/private/var/spool/cups"
#define CUPS_LOGDIR "/private/var/log/cups"
#define CUPS_DATADIR "/usr/share/cups"
#define CUPS_FONTPATH "/usr/share/cups/fonts"


/*
 * What is the format string for strftime?
 */

#define CUPS_STRFTIME_FORMAT "%c"


/*
 * Do we have various image libraries?
 */

/* #undef HAVE_LIBPNG */
#define HAVE_LIBZ 1
/* #undef HAVE_LIBJPEG */
/* #undef HAVE_LIBTIFF */


/*
 * Does this machine store words in big-endian (MSB-first) order?
 */

#define WORDS_BIGENDIAN 1


/*
 * Which directory functions and headers do we use?
 */

#define HAVE_DIRENT_H 1
/* #undef HAVE_SYS_DIR_H */
/* #undef HAVE_SYS_NDIR_H */
/* #undef HAVE_NDIR_H */


/*
 * Do we have PAM stuff?
 */

#ifndef HAVE_LIBPAM
#define HAVE_LIBPAM 0
#endif /* !HAVE_LIBPAM */

/* #undef HAVE_PAM_PAM_APPL_H */


/*
 * Do we have <shadow.h>?
 */

/* #undef HAVE_SHADOW_H */


/*
 * Do we have <crypt.h>?
 */

/* #undef HAVE_CRYPT_H */


/*
 * Use <string.h>, <strings.h>, or both?
 */

#define HAVE_STRING_H 1
#define HAVE_STRINGS_H 1


/*
 * Do we have the strXXX() functions?
 */

#define HAVE_STRDUP 1
#define HAVE_STRCASECMP 1
#define HAVE_STRNCASECMP 1
#define HAVE_STRLCAT 1
#define HAVE_STRLCPY 1


/*
 * Do we have the vsyslog() function?
 */

#define HAVE_VSYSLOG 1


/*
 * Do we have the (v)snprintf() functions?
 */

#define HAVE_SNPRINTF 1
#define HAVE_VSNPRINTF 1


/*
 * What signal functions to use?
 */

/* #undef HAVE_SIGSET */
#define HAVE_SIGACTION 1


/*
 * What wait functions to use?
 */

#define HAVE_WAITPID 1
#define HAVE_WAIT3 1


/*
 * Do we have the mallinfo function and malloc.h?
 */

/* #undef HAVE_MALLINFO */
/* #undef HAVE_MALLOC_H */


/*
 * Do we have the OpenSSL library?
 */

#define HAVE_LIBSSL 1


/*
 * Do we have the OpenSLP library?
 */

/* #undef HAVE_LIBSLP */


/*
 * Do we have <sys/ioctl.h>?
 */

#define HAVE_SYS_IOCTL_H 1


/*
 * Do we have mkstemp() and/or mkstemps()?
 */

#define HAVE_MKSTEMP 1
#define HAVE_MKSTEMPS 1


/*
 * Does the "tm" structure contain the "tm_gmtoff" member?
 */

#define HAVE_TM_GMTOFF 1


/*
 * Do we have rresvport()?
 */

#define HAVE_RRESVPORT 1


/*
 * Do we have getifaddrs()?
 */

#define HAVE_GETIFADDRS 1


/*
 * Do we have the <sys/sockio.h> header file?
 */

#define HAVE_SYS_SOCKIO_H 1


/*
 * Does the sockaddr structure contain an sa_len parameter?
 */

/* #undef HAVE_STRUCT_SOCKADDR_SA_LEN */


/*
 * End of "$Id: config.h,v 1.20.2.1 2002/12/13 22:54:07 jlovell Exp $".
 */
