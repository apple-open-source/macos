/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Portions of this software have been released under the following terms:
 *
 * (c) Copyright 1989-1993 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989-1993 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989-1993 DIGITAL EQUIPMENT CORPORATION
 *
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 * permission to use, copy, modify, and distribute this file for any
 * purpose is hereby granted without fee, provided that the above
 * copyright notices and this notice appears in all source code copies,
 * and that none of the names of Open Software Foundation, Inc., Hewlett-
 * Packard Company or Digital Equipment Corporation be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Neither Open Software
 * Foundation, Inc., Hewlett-Packard Company nor Digital
 * Equipment Corporation makes any representations about the suitability
 * of this software for any purpose.
 *
 * Copyright (c) 2007, Novell, Inc. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Novell Inc. nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
**
**  NAME
**
**      MESSAGE.C
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**      UUID Generator Tool
**
**  ABSTRACT:
**
**      International error message primitive routines.
**
**  VERSION: DCE 1.0
**
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef UUIDGEN  /* Building for uuidgen, so include whatever's needed from nidl.h. */
#   if defined __STDC__
#       include <limits.h>
#       include <stdlib.h>
#   endif
#   include <uuidmsg.h>
#   define MESSAGE_VERSION      UUIDGEN_MESSAGE_VERSION
#   define MESSAGE_VERSION_USED UUIDGEN_MESSAGE_VERSION_USED
#   define MESSAGE_CATALOG_DIR  "/usr/bin/"
#   define NLSCATVER            UUIDGEN_NLSCATVER
#   define NLSWRONG             UUIDGEN_NLSWRONG
#   ifdef _AIX
#       define NL_VFPRINTF NLvfprintf
#   else
#       define NL_VFPRINTF vfprintf
#   endif
#   define BRANCHCHAR '/'
#else   /* Building for nidl. */
#   include <nidl.h>
#   include <nidlmsg.h>
#   define MESSAGE_VERSION      NIDL_MESSAGE_VERSION
#   define MESSAGE_VERSION_USED NIDL_MESSAGE_VERSION_USED
#   define NLSCATVER            NIDL_NLSCATVER
#   define NLSWRONG             NIDL_NLSWRONG
#endif

#include <stdio.h>
#include <string.h>

#   define MAX_FMT_TEXT   512      /* Max size of formatted output string */
#   ifdef HAVE_NL_TYPES_H
#       include <nl_types.h>
#   else
#       warning Message catalog support disabled
#   endif
#   ifdef __STDC__
#       include <stdarg.h>  /* New! Improved! Method */
#       define VA_START(L, A, T) va_start(L, A)
#   else
#       include <varargs.h> /* Traditional Method */
#       define VA_START(L, A, T) T A; va_start(L); A = va_arg(L,T)
#   endif

#ifdef UUIDGEN
#   ifndef PATH_MAX
#       define PATH_MAX 256
#   endif
#endif

#ifdef HAVE_NL_TYPES_H
    static nl_catd cat_handle;
#endif /* HAVE_NL_TYPES_H */
/*
** Declare an array to hold the default messages.  The text of the messages is
** read from a file generated from the message catalog.
*/
const char *default_messages[] = {
"Internal idl compiler error: Invalid message number",
#include <default_msg.h>
};
static long max_message_number		/* Compute number of messages. */
	= (long)(sizeof(default_messages)/sizeof(char *) - 1);
#   define def_message(id) \
	default_messages[(id<0||id>max_message_number)?0:id]

#include <message.h>
static char     msg_prefix[PATH_MAX+3];


/*
 *  m e s s a g e _ o p e n
 *
 *  Function:   Opens message database.
 */

void message_open
(
    char *image_name __attribute__((unused))
)
{
#ifdef HAVE_NL_TYPES_H
    char cat_name[PATH_MAX] = CATALOG_DIR "idl.cat";

    strlcpy(msg_prefix, "idl: ", sizeof (msg_prefix));

    /*
     * Open the message catalog using the image name.
     */
#ifdef AIX32
    setlocale(LC_ALL, "");
#endif
    cat_handle = catopen(cat_name, 0);

    /* Sucessful open, check version information */
    if (cat_handle != (nl_catd)-1)
    {
          char  *version_text;
          version_text = catgets(cat_handle,CAT_SET,MESSAGE_VERSION,NULL);
          if (version_text != NULL && atoi(version_text) != MESSAGE_VERSION_USED)
          {
              fprintf(stderr, def_message(NLSCATVER),
                  msg_prefix, cat_name, MESSAGE_VERSION_USED, version_text);
              fprintf(stderr, "\n");
              fprintf(stderr, def_message(NLSWRONG), msg_prefix);
              fprintf(stderr, "\n");
          }
    }
#endif /* HAVE_NL_TYPES_H */
    return;
}

/*
 *  m e s s a g e _ c l o s e
 *
 *  Function:   Closes message database.
 */

void message_close
(
    void
)

{
#ifdef HAVE_NL_TYPES_H
    if (cat_handle != (nl_catd)-1) catclose(cat_handle);
#endif
    return;
}


/*
 *  m e s s a g e _ p r i n t
 *
 *  Function:   Fetches message from database, then formats and prints message.
 *
 *  Inputs:     msgid - message ID
 *              [arg1,...,arg5] - Optional arguments for message formatting
 *
 *  Outputs:    message printed to stderr.
 */

void vmessage_print
(long msgid, va_list arglist)
{
    char format[MAX_FMT_TEXT];     /* Format string */

#ifdef HAVE_NL_TYPES_H
    /*
     * Output message prefix on all errors that identify the input file,
     * or on every line for UUIDGEN
     */
    format[0]='\0';
    switch (msgid)
    {
#ifndef UUIDGEN
        case NIDL_EOF:
        case NIDL_EOFNEAR:
        case NIDL_SYNTAXNEAR:
        case NIDL_FILESOURCE:
        case NIDL_LINEFILE:
#else
        default:
#endif
            strlcpy(format, msg_prefix, sizeof (format));
    }

    strlcat(format,catgets(cat_handle, CAT_SET, msgid, def_message(msgid)), sizeof(format));
    strlcat(format,"\n", sizeof(format));
#else
    snprintf(format, sizeof(format), "%s%s\n", msg_prefix, def_message(msgid));
#endif /* HAVE_NL_TYPES_H */
    NL_VFPRINTF(stderr, format, arglist);
}

void message_print
#ifdef __STDC__
(long msgid, ...)
#else
(va_alist) va_dcl
#endif
{
    va_list arglist;

    VA_START(arglist, msgid, long);
    vmessage_print (msgid, arglist);
    va_end(arglist);
}


#ifndef UUIDGEN
/*
 *  m e s s a g e _ s p r i n t
 *
 *  Function:   Fetches message from database and formats message.
 *
 *  Inputs:     str - Address of buffer for formatted message
 *              msgid - message ID
 *              [arg1,...,arg5] - Optional arguments for message formatting
 *
 *  Outputs:    str
 */

void message_sprint
(
    char *str,
	size_t str_len,
    long msgid,
    char *arg1,
    char *arg2,
    char *arg3,
    char *arg4,
    char *arg5
)
{
    char *msg_text;     /* Ptr to message text (storage owned by catgets) */

#ifdef HAVE_NL_TYPES_H
    msg_text = catgets(cat_handle, CAT_SET, msgid, def_message(msgid));
#else
    msg_text = def_message(msgid);
#endif /* HAVE_NL_TYPES_H */
    /*
     * Output message prefix on all errors that identify the input file
     */
    switch (msgid)
    {
        case NIDL_EOF:
        case NIDL_EOFNEAR:
        case NIDL_SYNTAXNEAR:
        case NIDL_FILESOURCE:
        case NIDL_LINEFILE:
            strlcpy(str,msg_prefix,str_len);         /* Add prefix to messages */
            str +=  strlen(msg_prefix);
            break;
    }

    NL_SPRINTF(str, msg_text, arg1, arg2, arg3, arg4, arg5);
}

/*
 *  m e s s a g e _ f p r i n t
 *
 *  Function:   Fetches message from database, then formats and prints message.
 *
 *  Inputs:     fid - file handle of file for output message
 *              msgid - message ID
 *              [arg1,...,arg5] - Optional arguments for message formatting
 *
 *  Outputs:    message printed to file indicated by fid not including
 *		any system-dependant prefix information such as the compiler
 *		executable name, facility, severity, etc.
 */

void message_fprint
(
    FILE *fid,
    long msgid,
    char *arg1,
    char *arg2,
    char *arg3,
    char *arg4,
    char *arg5
)
{
    char            str[MAX_FMT_TEXT];     /* Formatted message text */
    char *msg_text;     /* Ptr to message text (storage owned by catgets) */

#ifdef HAVE_NL_TYPES_H
    msg_text = catgets(cat_handle, CAT_SET, msgid, def_message(msgid));
#else
    msg_text = def_message(msgid);
#endif /* HAVE_NL_TYPES_H */
    NL_SPRINTF(str, msg_text, arg1, arg2, arg3, arg4, arg5);
    fprintf(fid, "%s\n", str);
}

#endif
