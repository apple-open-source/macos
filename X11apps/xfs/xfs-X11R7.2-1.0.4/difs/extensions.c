/* $Xorg: extensions.c,v 1.4 2001/02/09 02:05:42 xorgcvs Exp $ */
/*
 * font server extensions
 */
/*
 
Copyright 1990, 1991, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

 * Copyright 1990, 1991 Network Computing Devices;
 * Portions Copyright 1987 by Digital Equipment Corporation 
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Network Computing Devices, or Digital
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 *
 * NETWORK COMPUTING DEVICES, AND DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES,
 * OR DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 */
/* $XFree86: xc/programs/xfs/difs/extensions.c,v 1.6 2001/08/01 00:45:04 tsi Exp $ */

#include	<X11/fonts/FSproto.h>
#include	"misc.h"
#include	"clientstr.h"
#include	"extentst.h"
#include	"difs.h"
#include	"dispatch.h"

#define	EXTENSION_BASE	128
#define	EXTENSION_EVENT_BASE	64
#define	LAST_EVENT	128
#define	LAST_ERROR	255

static ExtensionEntry **extensions = (ExtensionEntry **) NULL;

int         lastEvent = EXTENSION_EVENT_BASE;
static int  lastError = FirstExtensionError;
static int  NumExtensions = 0;


ExtensionEntry *
AddExtension(
    char       *name,
    int         num_events,
    int         num_errors,
    int         (*main_proc) (ClientPtr),
    int         (*smain_proc) (ClientPtr),
    void        (*closedown_proc) (struct _ExtensionEntry *),
    unsigned short (*minorop_proc) (ClientPtr))
{
    int         i;
    ExtensionEntry *ext,
              **newexts;

    if (!main_proc || !smain_proc || !closedown_proc || !minorop_proc)
	return ((ExtensionEntry *) 0);
    if ((lastEvent + num_events > LAST_EVENT) ||
	    (unsigned) (lastError + num_errors > LAST_ERROR))
	return ((ExtensionEntry *) 0);
    ext = (ExtensionEntry *) fsalloc(sizeof(ExtensionEntry));
    if (!ext)
	return ((ExtensionEntry *) 0);
    ext->name = (char *) fsalloc(strlen(name) + 1);
    ext->num_aliases = 0;
    ext->aliases = (char **) NULL;
    if (!ext->name) {
	fsfree(ext);
	return ((ExtensionEntry *) 0);
    }
    strcpy(ext->name, name);
    i = NumExtensions;
    newexts = (ExtensionEntry **) fsrealloc(extensions,
					 (i + 1) * sizeof(ExtensionEntry *));
    if (!newexts) {
	fsfree(ext->name);
	fsfree(ext);
	return ((ExtensionEntry *) 0);
    }
    NumExtensions++;
    extensions = newexts;
    extensions[i] = ext;
    ext->index = i;
    ext->base = i + EXTENSION_BASE;
    ext->CloseDown = closedown_proc;
    ext->MinorOpcode = minorop_proc;
    ProcVector[i + EXTENSION_BASE] = main_proc;
    SwappedProcVector[i + EXTENSION_BASE] = smain_proc;
    if (num_events) {
	ext->eventBase = lastEvent;
	ext->eventLast = lastEvent + num_events;
	lastEvent += num_events;
    } else {
	ext->eventBase = 0;
	ext->eventLast = 0;
    }
    if (num_errors) {
	ext->errorBase = lastError;
	ext->errorLast = lastError + num_errors;
	lastError += num_errors;
    } else {
	ext->errorBase = 0;
	ext->errorLast = 0;
    }
    return ext;
}

Bool
AddExtensionAlias(char *alias, ExtensionEntry *ext)
{
    char       *name;
    char      **aliases;

    aliases = (char **) fsrealloc(ext->aliases,
				  (ext->num_aliases + 1) * sizeof(char *));
    if (!aliases)
	return FALSE;
    ext->aliases = aliases;
    name = (char *) fsalloc(strlen(alias) + 1);
    if (!name)
	return FALSE;
    strcpy(name, alias);
    ext->aliases[ext->num_aliases++] = name;
    return TRUE;
}

unsigned short
StandardMinorOpcode(ClientPtr client)
{
    return ((fsReq *) client->requestBuffer)->data;
}

unsigned short
MinorOpcodeOfRequest(ClientPtr client)
{
    unsigned char major;

    major = ((fsReq *) client->requestBuffer)->reqType;
    if (major < EXTENSION_BASE)
	return 0;
    major -= EXTENSION_BASE;
    if (major >= NumExtensions)
	return 0;
    return (*extensions[major]->MinorOpcode) (client);
}

void
CloseDownExtensions(void)
{
    int         i,
                j;

    for (i = NumExtensions - 1; i >= 0; i--) {
	(*extensions[i]->CloseDown) (extensions[i]);
	NumExtensions = i;
	fsfree(extensions[i]->name);
	for (j = extensions[i]->num_aliases; --j >= 0;)
	    fsfree(extensions[i]->aliases[j]);
	fsfree(extensions[i]->aliases);
	fsfree(extensions[i]);
    }
    fsfree(extensions);
    extensions = (ExtensionEntry **) NULL;
    lastEvent = EXTENSION_EVENT_BASE;
    lastError = FirstExtensionError;
}

void
InitExtensions(void)
{
}

int
ProcQueryExtension(ClientPtr client)
{
    fsQueryExtensionReply reply;
    int         i,
                j;

    REQUEST(fsQueryExtensionReq);

    REQUEST_AT_LEAST_SIZE(fsQueryExtensionReq);

    reply.type = FS_Reply;
    reply.length = SIZEOF(fsQueryExtensionReply) >> 2;
    reply.major_opcode = 0;
    reply.sequenceNumber = client->sequence;

    if (!NumExtensions) {
	reply.present = fsFalse;
    } else {
	for (i = 0; i < NumExtensions; i++) {
	    if ((strlen(extensions[i]->name) == stuff->nbytes) &&
		    !strncmp((char *) &stuff[1], extensions[i]->name,
			     (int) stuff->nbytes))
		break;
	    for (j = extensions[i]->num_aliases; --j >= 0;) {
		if ((strlen(extensions[i]->aliases[j]) == stuff->nbytes) &&
		      !strncmp((char *) &stuff[1], extensions[i]->aliases[j],
			       (int) stuff->nbytes))
		    break;
	    }
	    if (j >= 0)
		break;
	}
	if (i == NumExtensions) {
	    reply.present = fsFalse;
	} else {
	    reply.present = fsTrue;
	    reply.major_opcode = extensions[i]->base;
	    reply.first_event = extensions[i]->eventBase;
	    reply.first_error = extensions[i]->errorBase;
	}

    }
    WriteReplyToClient(client, SIZEOF(fsQueryExtensionReply), &reply);
    return client->noClientException;
}

int
ProcListExtensions(ClientPtr client)
{
    fsListExtensionsReply reply;
    char       *bufptr,
               *buffer;
    int         total_length = 0;

    REQUEST(fsListExtensionsReq);
    REQUEST_SIZE_MATCH(fsListExtensionsReq);

    reply.type = FS_Reply;
    reply.nExtensions = NumExtensions;
    reply.length = SIZEOF(fsListExtensionsReply) >> 2;
    reply.sequenceNumber = client->sequence;
    buffer = NULL;

    if (NumExtensions) {
	int         i,
	            j;

	for (i = 0; i < NumExtensions; i++) {
	    total_length += strlen(extensions[i]->name) + 1;
	    reply.nExtensions += extensions[i]->num_aliases;
	    for (j = extensions[i]->num_aliases; --j >= 0;)
		total_length += strlen(extensions[i]->aliases[j]) + 1;
	}
	reply.length += (total_length + 3) >> 2;
	buffer = bufptr = (char *) ALLOCATE_LOCAL(total_length);
	if (!buffer) {
	    SendErrToClient(client, FSBadAlloc, NULL);
	    return FSBadAlloc;
	}
	for (i = 0; i < NumExtensions; i++) {
	    int         len;

	    *bufptr++ = len = strlen(extensions[i]->name);
	    memmove( bufptr, extensions[i]->name, len);
	    bufptr += len;
	    for (j = extensions[i]->num_aliases; --j >= 0;) {
		*bufptr++ = len = strlen(extensions[i]->aliases[j]);
		memmove( bufptr, extensions[i]->aliases[j], len);
		bufptr += len;
	    }
	}
    }
    WriteReplyToClient(client, SIZEOF(fsListExtensionsReply), &reply);
    if (total_length) {
	WriteToClient(client, total_length, buffer);
	DEALLOCATE_LOCAL(buffer);
    }
    return client->noClientException;
}
