/*
 * Copyright (c) 2009, Oracle and/or its affiliates. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "extensions.h"
#include "bigreqscope.h"
#include "lbxscope.h"
#include "randrscope.h"
#include "renderscope.h"
#include "shmscope.h"
#include "wcpscope.h"

/* Extensions we know how to decode */
struct extension_decoders {
    const char *name;
    void (*init_func)(const unsigned char *buf);
};

static const struct extension_decoders decodable_extensions[] = {
    { "BIG-REQUESTS",		InitializeBIGREQ },
    { "LBX",			InitializeLBX },
    { "MIT-SHM",		InitializeMITSHM },
    { "NCD-WinCenterPro",	InitializeWCP },
    { "RANDR",			InitializeRANDR },
    { "RENDER",			InitializeRENDER },
    { "GLX",			InitializeGLX },
   { NULL, NULL } /* List terminator - keep last */
};

/* all extensions we know about */
struct extension_info {
    const char *name;
    unsigned char request;
    unsigned char event;
    unsigned char error;
    long query_seq;	/* sequence id of QueryExtension request */
    struct extension_info *next;
};

struct extension_info *query_list;

struct extension_info *ext_by_request[NUM_EXTENSIONS];

static void
DefineExtNameValue(int type, unsigned char value, const char *extname)
{
    int namelen = strlen(extname) + 1;
    const char *typename = NULL;
    char *exttypename;

    switch (type) {
      case REQUEST:
	typename = "-Request";
	break;
      case REPLY:
	typename = "-Reply";
	break;
      case EVENT:
	typename = "-Event";
	break;
      case ERROR:
	typename = "-Error";
	break;
      case EXTENSION:
	typename = "";
	break;
      default:
	panic("Impossible argument to DefineExtNameValue");
    }
    namelen += strlen(typename);
    exttypename = Malloc(namelen);
    snprintf(exttypename, namelen, "%s%s", extname, typename);
    DefineEValue(&TD[type], (unsigned long) value, exttypename);
}

void
ProcessQueryExtensionRequest(long seq, const unsigned char *buf)
{
    int namelen = IShort(&buf[4]);
    char *extname = Malloc(namelen + 1);
    struct extension_info *qe;

    memcpy(extname, &buf[8], namelen);
    extname[namelen] = '\0';

    for (qe = query_list; qe != NULL; qe = qe->next) {
	if (strcmp(extname, qe->name) == 0) {
	    /* already in list, no need to add */
	    Free(extname);
	    return;
	}
    }

    /* add to list */
    qe = Malloc(sizeof(struct extension_info));
    qe->name = extname;
    qe->request = 0;
    qe->event = 0;
    qe->error = 0;
    qe->query_seq = seq;
    qe->next = query_list;
    query_list = qe;
}

void
ProcessQueryExtensionReply(long seq, const unsigned char *buf)
{
    struct extension_info *qe;
    int i;

    if (buf[8] == 0) {
	/* Extension not present, nothing to record */
	return;
    }

    for (qe = query_list; qe != NULL; qe = qe->next) {
	if (qe->query_seq == seq) {
	    qe->request = buf[9];
	    qe->event = buf[10];
	    qe->error = buf[11];

	    ext_by_request[qe->request - EXTENSION_MIN_REQ] = qe;

	    DefineExtNameValue(EXTENSION, qe->request, qe->name);

	    for (i = 0; decodable_extensions[i].name != NULL ; i++) {
		if (strcmp(qe->name, decodable_extensions[i].name) == 0) {
		    decodable_extensions[i].init_func(buf);
		    break;
		}
	    }
	    if (decodable_extensions[i].name == NULL) {
		/* Not found - initialize values as best we can generically */
		DefineExtNameValue(REQUEST, qe->request, qe->name);
		DefineExtNameValue(REPLY, qe->request, qe->name);
		if (qe->event != 0) {
		    DefineExtNameValue(EVENT, qe->event, qe->name);
		}
		if (qe->error != 0) {
		    DefineExtNameValue(ERROR, qe->error, qe->name);
		}
	    }

	    return;
	}
    }
}

/* Decoding for specific/known extensions */

static extension_decode_req_ptr   ExtensionRequestDecoder[NUM_EXTENSIONS];
static extension_decode_reply_ptr ExtensionReplyDecoder[NUM_EXTENSIONS];
static extension_decode_error_ptr ExtensionErrorDecoder[NUM_EXTENSIONS];
static extension_decode_event_ptr ExtensionEventDecoder[NUM_EXT_EVENTS];
static extension_decode_event_ptr GenericEventDecoder[NUM_EXTENSIONS];

void
InitializeExtensionDecoder (int Request, extension_decode_req_ptr reqd,
			    extension_decode_reply_ptr repd)
{
    if ((Request > EXTENSION_MAX_REQ) || (Request < EXTENSION_MIN_REQ)) {
	char errmsg[128];

	snprintf(errmsg, sizeof(errmsg), "Failed to register decoder"
		 " for invalid extension request code %d.", Request);
	warn(errmsg);
	return;
    }
    ExtensionRequestDecoder[Request - EXTENSION_MIN_REQ] = reqd;
    ExtensionReplyDecoder[Request - EXTENSION_MIN_REQ] = repd;
}

void
InitializeExtensionErrorDecoder(int Error, extension_decode_error_ptr errd)
{
    if ((Error > EXTENSION_MAX_ERR) || (Error < EXTENSION_MIN_ERR)) {
	char errmsg[128];

	snprintf(errmsg, sizeof(errmsg), "Failed to register decoder"
		 " for invalid extension error code %d.", Error);
	warn(errmsg);
	return;
    }
    ExtensionErrorDecoder[Error - EXTENSION_MIN_ERR] = errd;
}

void
InitializeExtensionEventDecoder(int Event, extension_decode_event_ptr evd)
{
    if ((Event > EXTENSION_MAX_EV) || (Event < EXTENSION_MIN_EV)) {
	char errmsg[128];

	snprintf(errmsg, sizeof(errmsg), "Failed to register decoder"
		 " for invalid extension event code %d.", Event);
	warn(errmsg);
	return;
    }
    ExtensionEventDecoder[Event - EXTENSION_MIN_EV] = evd;
}


void
InitializeGenericEventDecoder(int Request, extension_decode_event_ptr evd)
{
    if ((Request > EXTENSION_MAX_REQ) || (Request < EXTENSION_MIN_REQ)) {
	char errmsg[128];

	snprintf(errmsg, sizeof(errmsg), "Failed to register decoder"
		 " for invalid generic extension event code %d.", Request);
	warn(errmsg);
	return;
    }
    GenericEventDecoder[Request - EXTENSION_MIN_REQ] = evd;
}

void
ExtensionRequest (FD fd, const unsigned char *buf, short Request)
{
    extension_decode_req_ptr decode_req = NULL;

    if ((Request <= EXTENSION_MAX_REQ) && (Request >= EXTENSION_MIN_REQ)) {
	decode_req = ExtensionRequestDecoder[Request - EXTENSION_MIN_REQ];
    }

    if (decode_req != NULL) {
	decode_req(fd, buf);
    } else {
        ExtendedRequest(fd, buf);
	ReplyExpected(fd, Request);
    }
}

void
ExtensionReply (FD fd, const unsigned char *buf,
		short Request, short RequestMinor)
{
    extension_decode_reply_ptr decode_reply = NULL;

    if ((Request <= EXTENSION_MAX_REQ) && (Request >= EXTENSION_MIN_REQ)) {
	decode_reply = ExtensionReplyDecoder[Request - EXTENSION_MIN_REQ];
    }

    if (decode_reply != NULL) {
	decode_reply(fd, buf, RequestMinor);
    } else {
	UnknownReply(buf);
    }
}

void
ExtensionError (FD fd, const unsigned char *buf, short Error)
{
    extension_decode_error_ptr decode_error = NULL;

    if ((Error <= EXTENSION_MAX_ERR) && (Error >= EXTENSION_MIN_ERR)) {
	decode_error = ExtensionErrorDecoder[Error - EXTENSION_MIN_ERR];
    }

    if (decode_error != NULL) {
	decode_error(fd, buf);
    } else {
	UnknownError(buf);
    }
}

void
ExtensionEvent (FD fd, const unsigned char *buf, short Event)
{
    extension_decode_event_ptr decode_event = NULL;

    if ((Event <= EXTENSION_MAX_EV) && (Event >= EXTENSION_MIN_EV)) {
	decode_event = ExtensionEventDecoder[Event - EXTENSION_MIN_EV];
    } else if (Event == Event_Type_Generic) {
	int Request = IByte (&buf[1]);
	if ((Request <= EXTENSION_MAX_REQ) && (Request >= EXTENSION_MIN_REQ)) {
	    decode_event = GenericEventDecoder[Request - EXTENSION_MIN_REQ];
	}
    }

    if (decode_event != NULL) {
	decode_event(fd, buf);
    } else if (Event == Event_Type_Generic) {
	UnknownGenericEvent(buf);
    } else {
	UnknownEvent(buf);
    }
}
