/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifdef WIN32

#include <windows.h>
#include <dirent.h>
#include <string.h>
#define BOOL short
typedef struct tagWin32GnuDir {
    DIR		dir;
    struct dirent dent;
    char      	namefill[MAX_PATH];
} Win32GnuDir;

typedef Win32GnuDir   * PW32GDIR;

DIR * opendir(path)
char * path;
{
    DIR 	*dir=0;
    char 	fpath[MAX_PATH],full[MAX_PATH];
    LPTSTR	end,put;
    DWORD	fulllen;
    DWORD	fileattr,volattr;
    char	tmp[4];
    strcpy(fpath,path);
    end=fpath;
    while(((end=strchr(fpath,'/'))))
	*end='\\';
    GetFullPathName(fpath,
		    MAX_PATH,
		    full,
		    0);
    if((fileattr=GetFileAttributes(fpath))!=0xffffffff) 
    {
	if (fileattr & FILE_ATTRIBUTE_DIRECTORY)
	{
	    dir=calloc(sizeof(Win32GnuDir),1);
	    strcat(full,"\\*");
	    dir->__data=calloc(sizeof(WIN32_FIND_DATA),1);
	    dir->__fd=FindFirstFile(full,
				    dir->__data);
	    dir->__allocation=dir->__size=sizeof(WIN32_FIND_DATA);
	}
    }
    return(dir);
}



int closedir(dir)
DIR * dir;
{
    int retval=-1;
    if (dir) 
    {
	if (FindClose(dir->__fd) || dir->__fd == 0xffffffff)
	    retval=0;
	free(dir->__data);
	free(dir);
    }
    return(retval);
}

struct dirent *readdir(dir)
DIR * dir;
{
    struct dirent *retdirent=0;
    LPWIN32_FIND_DATA	find;
    DWORD  diridx;
    if 	(dir) 
    {	
	diridx=dir->__offset/sizeof(WIN32_FIND_DATA);
	find=dir->__data;
	if (strlen(find[diridx].cFileName))
	{
	    strcpy(&dir->__entry.d_name,&find[diridx].cFileName);
	    dir->__offset+=sizeof(WIN32_FIND_DATA);
	    if (dir->__offset>=dir->__size) 
	    {
		dir->__size+=sizeof(WIN32_FIND_DATA);
		dir->__allocation=dir->__size;
		find=dir->__data=realloc(find,dir->__size);
		memset(&find[diridx+1],0,sizeof(WIN32_FIND_DATA));
		FindNextFile(dir->__fd,&find[diridx+1]);
	    }
	    retdirent=&dir->__entry;
	    dir->__entry.d_namlen=strlen(dir->__entry.d_name);
	}
    }
    return(retdirent);
}


	

void rewinddir(dirp)
DIR *dirp;
{
    if (dirp)
    {	
	dirp->__offset=0;
    }
}

void seekdir(dirp, pos)
DIR *dirp;
_off_t pos;
{
    if (dirp) 
    {
	pos=(pos/sizeof(WIN32_FIND_DATA))*sizeof(WIN32_FIND_DATA);
	if (pos<dirp->__size)
	    dirp->__offset=pos;
    }
}


_off_t telldir(dirp)
DIR * dirp;
{
    if (dirp)
	return(dirp->__offset);
    else
	return(0xffffffff);
}

#endif
	       

