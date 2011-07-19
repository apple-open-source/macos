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
**  NAME:
**
**      files.c
**
**  FACILITY:
**
**      Interface Definition Language (IDL) Compiler
**
**  ABSTRACT:
**
**  IDL file manipulation routines.
**
**  VERSION: DCE 1.0
**
*/

#include <sys/types.h>
#include <sys/stat.h>

#include <nidl.h>
#include <files.h>
#include <unistd.h>
#include "message.h"

/*
**  Default filespec; only good for one call to FILE_parse.
*/
char const *FILE_def_filespec = NULL;

/*
**  F I L E _ o p e n
**
**  Opens an existing file for read access.
*/

boolean FILE_open               /* Returns TRUE on success */
(
    char        *filespec,      /* [in] Filespec */
    FILE        **fid           /*[out] File handle; ==NULL on FALSE status */
)
{
    if ((*fid = fopen(filespec, "r")) == NULL)
    {
        idl_error_list_t errvec[2];
        errvec[0].msg_id = NIDL_OPENREAD;
        errvec[0].arg[0] = filespec;
        errvec[1].msg_id = NIDL_SYSERRMSG;
        errvec[1].arg[0] = strerror(errno);
        error_list(2, errvec, TRUE);
    }

    return TRUE;
}

/*
**  F I L E _ c r e a t e
**
**  Creates and opens a new file for write access.
*/

boolean FILE_create             /* Returns TRUE on success */
(
    char        *filespec,      /* [in] Filespec */
    FILE        **fid           /*[out] File handle; ==NULL on FALSE status */
)
{
#define MODE_WRITE "w"

    if ((*fid = fopen(filespec, MODE_WRITE)) == NULL)
    {
        idl_error_list_t errvec[2];
        errvec[0].msg_id = NIDL_OPENWRITE;
        errvec[0].arg[0] = filespec;
        errvec[1].msg_id = NIDL_SYSERRMSG;
        errvec[1].arg[0] = strerror(errno);
        error_list(2, errvec, TRUE);
    }

    return TRUE;
}

/*
**  F I L E _ l o o k u p
**
**  Looks for the specified file first in the working directory,
**  and then in the list of specified directories.
**
**  Returns:    TRUE if file was found, FALSE otherwise
*/

boolean FILE_lookup             /* Returns TRUE on success */
(
    char const  *filespec,      /* [in] Filespec */
    char const  * const *idir_list,    /* [in] Array of directories to search */
                                /*      NULL => just use filespec */
    struct stat *stat_buf,      /*[out] Stat buffer - see stat.h */
    char        *lookup_spec,   /*[out] Filespec of found file (on success) */
	size_t lookup_spec_len		/* [in] len of lookup_spec */
)
{
#ifdef HASDIRTREE
    int     i;

    /*
     * First try the filespec by itself.
     */
    if (stat(filespec, stat_buf) != -1)
    {
        strlcpy(lookup_spec, filespec, lookup_spec_len);
        return TRUE;
    }

    /*
     * Fail if idir_list is null.
     */
    if (idir_list == NULL)
        return FALSE;

    /*
     * Lookup other pathnames using the directories in the idir_list.
     */
    for (i = 0; idir_list[i]; i++)
    {
        if (FILE_form_filespec(filespec, idir_list[i], (char *)NULL,
                               (char *)NULL, lookup_spec, lookup_spec_len)
            &&  stat(lookup_spec, stat_buf) != -1)
            return TRUE;
    }

    /*
     * On Unix-like filesystems, make another pass over the idir_list if the
     * search filespec has a directory name, prepending each idir to the search
     * filespec.  For example, importing "y/z.idl" will match "/x/y/z.idl" if
     * -I/x is on the command line.
     */
    if (*filespec != BRANCHCHAR && FILE_has_dir_info(filespec))
    {
        for (i = 0; idir_list[i]; i++)
        {
            sprintf(lookup_spec, "%s%c%s", idir_list[i], BRANCHCHAR, filespec);
            if (stat(lookup_spec, stat_buf) != -1)
                return TRUE;
        }
    }
#else
    error(NIDL_FNUNIXONLY, __FILE__, __LINE__);
#endif

    return FALSE;
}

/*
**  F I L E _ f o r m _ f i l e s p e c
**
**  Forms a file specification from the specified components.
*/

boolean FILE_form_filespec      /* Returns TRUE on success */
(                               /* For all [in] args, NULL => none */
    char const  *in_filespec,   /* [in] Filespec (full or partial) */
    char const  *dirspec,       /* [in] Directory; used if in_filespec */
                                /*      doesn't have directory field */
    char const  *type,          /* [in] Filetype; used if in_filespec */
                                /*      doesn't have filetype field */
    char const  *rel_filespec,  /* [in] Related filespec; fields are used to */
                                /*      fill in missing components after */
                                /*      applying in_filespec, dir, type */
    char        *out_filespec,   /*[out] Full filespec formed */
	size_t		out_filespec_len /* [in] len of out_filespec */
)
{
    char const *dir = NULL;        /* Directory specified */
    char       in_dir[PATH_MAX];   /* Directory part of in_filespec */
    char       in_name[PATH_MAX];  /* Filename part of in_filespec */
    char       in_type[PATH_MAX];  /* Filetype part of in_filespec */
    char       rel_dir[PATH_MAX];  /* Directory part of rel_filespec */
    char       rel_name[PATH_MAX]; /* Filename part of rel_filespec */
    char       rel_type[PATH_MAX]; /* Filetype part of rel_filespec */
    char const *res_dir;           /* Resultant directory */
    char const *res_name;          /* Resultant filename */
    char const *res_type;          /* Resultant filetype */

    in_dir[0]   = '\0';
    in_name[0]  = '\0';
    in_type[0]  = '\0';
    rel_dir[0]  = '\0';
    rel_name[0] = '\0';
    rel_type[0] = '\0';
    res_dir     = "";
    res_name    = "";
    res_type    = "";

    /* Parse in_filespec into its components. */
    if (in_filespec != NULL && in_filespec[0] != '\0')
    {
        /*
         * Setup the related or file type global FILE_def_filespec such that
         * any file lookup is handled appropriately in FILE_parse.
         */
        if (rel_filespec)
            FILE_def_filespec = rel_filespec;
        else if (type)
            FILE_def_filespec = type;

        if (!FILE_parse(in_filespec, in_dir, sizeof (in_dir), in_name, sizeof(in_name), in_type, sizeof(in_type)))
            return FALSE;
    }

    if (dir == NULL)
	dir = dirspec;

    /* Parse rel_filespec into its components. */
    if (rel_filespec != NULL && rel_filespec[0] != '\0')
        if (!FILE_parse(rel_filespec, rel_dir, sizeof(rel_dir), rel_name, sizeof(rel_name), rel_type, sizeof(rel_type)))
            return FALSE;

    /* Apply first valid of in_dir, dir, or rel_dir. */
    if (in_dir[0] != '\0')
        res_dir = in_dir;
    else if (dir != NULL && dir[0] != '\0')
        res_dir = dir;
    else if (rel_dir[0] != '\0')
        res_dir = rel_dir;

    /* Apply first valid of in_name, rel_name. */
    if (in_name[0] != '\0')
        res_name = in_name;
    else if (rel_name[0] != '\0')
        res_name = rel_name;

    /* Apply first valid of in_type, type, rel_type.  Note that rel_type is
     * only applied if in_filespec is null.
     */
    if (in_type[0] != '\0')
        res_type = in_type;
    else if (type != NULL && type[0] != '\0')
        res_type = type;
    else if (rel_type[0] != '\0')
        res_type = rel_type;

#ifdef HASDIRTREE

    /* Concatenate the result. */

    out_filespec[0] = '\0';

    if (res_dir[0] != '\0')
    {
        strlcat(out_filespec, res_dir, out_filespec_len);
        strlcat(out_filespec, BRANCHSTRING, out_filespec_len);
    }

    if (res_name[0] != '\0')
        strlcat(out_filespec, res_name, out_filespec_len);

    if (res_type[0] != '\0')
        strlcat(out_filespec, res_type, out_filespec_len); /* The '.' is part of the filetype */

    return TRUE;

#else
    error(NIDL_FNUNIXONLY, __FILE__, __LINE__);
#endif
}

/*
**  F I L E _ p a r s e
**
**  Parses a specified pathanme into individual components.
*/

boolean FILE_parse              /* Returns TRUE on success */
(
    char const  *filespec,      /* [in] Filespec */
    char        *dir,           /*[i,o] Directory portion; NULL =>don't want */
    size_t	dir_len,		/*[i] len of dir */
    char        *name,          /*[i,o] Filename portion;  NULL =>don't want */
    size_t	name_len ATTRIBUTE_UNUSED,		/*[i] len of name */
    char        *type,          /*[i,o] File type (ext);   NULL =>don't want */
    size_t	type_len		/*[i] len of type */
)
{
#if defined(HASDIRTREE)
    FILE_k_t    filekind;       /* File kind */
    char const  *pn;
    int         pn_len,
                leaf_len;
    int         i,
                j;
    int         leaf_start,
                ext_start;
    int         dir_end,
                leaf_end;
    boolean     slash_seen,
                dot_seen;

    /* Init return values. */
    if (dir)
        dir[0] = '\0';
    if (name)
        name[0] = '\0';
    if (type)
        type[0] = '\0';

    /*
     * If the filespec has BRANCHCHAR do special case check to see if pathname
     * is a directory to prevent directory /foo/bar from being interpreted as
     * directory /foo file bar.
     */
    if (strchr(filespec, BRANCHCHAR)
        &&  FILE_kind(filespec, &filekind)
        &&  filekind == file_dir)
    {
        strlcpy(dir, filespec, dir_len);
        return TRUE;
    }

    /*
     *  Scan backwards looking for a BRANCHCHAR -
     *  If not found, then no directory was specified.
     */
    pn = filespec;
    pn_len = strlen(pn);
    slash_seen = FALSE;
    dir_end = -1;
    leaf_start = 0;
    dot_seen = FALSE;

    /*
     * For temporary VMS support, until full file capabilities are in place,
     * look for the defined BRANCHCHAR or a colon, which is indicative of a
     * device name or logical name.  Device and directory information is
     * collectively returned as the dir argument.
     */
    for (i = pn_len - 1; i >= 0; i--)
        if (pn[i] == BRANCHCHAR
#if BRANCHAR == '\\'
            || pn[i] == '/'
#endif
           )
        {
            /*
             * On VMS, the BRANCHCHAR is considered part of the directory.
             */
            leaf_start = i + 1;
            dir_end = i > 0 ? i : 1;
            slash_seen = TRUE;
            break;
        }

    if (dir)
    {
        if (slash_seen)
        {
            strncpy(dir, pn, dir_end);
            dir[dir_end] = '\0';
        }
        else
            dir[0] = '\0';
    }

    /*
     *  Start scanning from the BRANCHCHAR for a '.' to find the leafname.
     */
    ext_start = pn_len;
    leaf_end = pn_len;

    for (j = pn_len; j > leaf_start; --j)
        if (pn[j] == '.')
        {
            leaf_end = j - 1;
            ext_start = j;      /* Extension includes the '.' */
            dot_seen = TRUE;
            break;
        }

    if (leaf_end >= dir_end + 1)
    {
        leaf_len = dot_seen ? leaf_end - leaf_start + 1 : leaf_end - leaf_start;
        if (name)
        {
            strncpy(name, &pn[leaf_start], leaf_len);
            name[leaf_len] = '\0';
        }

        if (!dot_seen)
        {
            if (type)
                type[0] = '\0';
            return TRUE;
        }
        else
        {
        if (type)
            strlcpy(type, &pn[ext_start], type_len);
        }
    }

    return TRUE;

#else
    error(NIDL_FNUNIXONLY, __FILE__, __LINE__);
    return FALSE;
#endif
}


/*
**  F I L E _ h a s _ d i r _ i n f o
**
**  Returns:    TRUE if filespec includes directory information.
*/

boolean FILE_has_dir_info
(
    char const  *filespec       /* [in] Filespec */
)
{
    char    dir[PATH_MAX];      /* Directory part of filespec */

    if (!FILE_parse(filespec, dir, sizeof(dir), (char *)NULL, 0, (char *)NULL, 0))
        return FALSE;

    return (dir[0] != '\0');
}

/*
**  F I L E _ i s _ c w d
**
**  Returns:    TRUE if filespec is equivalent to the current working directory.
*/

boolean FILE_is_cwd
(
    char        *filespec       /* [in] Filespec */
)
{
    char    *cwd;       /* Current working directory */
    char    *twd;       /* Temp working directory = filespec argument */
    boolean result;     /* Function result */

    /* Null filespec => current working directory. */
    if (filespec[0] == '\0')
        return TRUE;

    /* Get current working directory. */
    cwd = getcwd((char *)NULL, PATH_MAX);
    if (cwd == NULL)
        return FALSE;

    /* chdir to the passed directory filespec. */
    if (chdir(filespec) != 0)
    {
        /* Can chdir; probably a bogus directory. */
        free(cwd);
        return FALSE;
    }

    /*
     * Again get current working directory - this gets us the passed
     * directory filespec in a "normallized form".
     */
    twd = getcwd((char *)NULL, PATH_MAX);
    if (twd == NULL)
    {
        free(cwd);
        return FALSE;
    }

    if (strcmp(cwd, twd) == 0)
        result = TRUE;
    else
    {
        /* Not current working directory; be sure to chdir back to original! */
        result = FALSE;
        chdir(cwd);
    }

    /* Free storage malloc'ed by getcwd(). */
    free(cwd);
    free(twd);

    return result;
}

/*
**  F I L E _ k i n d
**
**  Returns whether a pathname is a directory, a file, or something else.
*/

boolean FILE_kind               /* Returns TRUE on success */
(
    char const  *filespec,      /* [in] Filespec */
    FILE_k_t    *filekind       /*[out] File kind (on success) */
)
{
    struct stat fileinfo;

    if (stat(filespec, &fileinfo) == -1)
        return FALSE;

    switch (fileinfo.st_mode & S_IFMT)
    {
    case S_IFDIR:
        *filekind = file_dir;
        break;

    case S_IFREG:
        *filekind = file_file;
        break;

    default:
        *filekind = file_special;
    }

    return TRUE;
}

/*
**  F I L E _ c o n t a i n s _ e v _ r e f
**
**  Scans a pathname to see if it contains an environment variable reference.
*/

boolean FILE_contains_ev_ref    /* Returns TRUE if filespec contains an */
                                /* environment variable reference */
(
    STRTAB_str_t    fs_id       /* [in] Filespec stringtable ID */
)
{
    char const  *pn;
    unsigned int         i;

    STRTAB_str_to_string(fs_id, &pn);

    for (i = 0; i < strlen(pn) - 1; i++)
        if (pn[i] == '$' && pn[i + 1] == '(')
            return TRUE;

    return FALSE;
}

/*
**  F I L E _ e x e c u t e _ c m d
**
**  This routine executes the specified command string with
**  the specified parameters.  All error output goes to the
**  default output/error device.
*/

int FILE_execute_cmd
(
    char    *cmd_string,        /* command to execute */
    char    *p1,                /* parameter1 */
    char    *p2,                /* parameter2 */
    long    msg_id              /* Optional msg_id to output */
)
{
    char    *cmd;       /* Command derived from inputs */
    int     status;
	size_t	cmd_len = 0;

    /* Alloc space and create command string */
	cmd_len = strlen(cmd_string) + strlen(p1) + strlen(p2) + 3;
    cmd = NEW_VEC (char, cmd_len);
    cmd[0] = '\0';
    strlcat(cmd, cmd_string, cmd_len);
    strlcat(cmd, " ", cmd_len);
    strlcat(cmd, p1, cmd_len);
    strlcat(cmd, " ", cmd_len);
    strlcat(cmd, p2, cmd_len);

    /* Output a message, if msg_id specified is non-zero */
    if (msg_id != 0)
        message_print(msg_id, (char*)cmd);

    /* Execute the command, errors to default output device */
    status = system(cmd);

    /* Free the command string */
    FREE(cmd);

    return status;
}

/*
**  F I L E _ d e l e t e
**
**  This routine deletes the file specified by the filename
**  string specified.
*/

void FILE_delete
(
    char    *filename
)
{
    unlink (filename);
}
/* preserve coding style vim: set tw=78 sw=4 : */
