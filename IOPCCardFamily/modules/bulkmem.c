/*======================================================================

    PCMCIA Bulk Memory Services

    bulkmem.c 1.38 2000/09/25 19:29:51

    The contents of this file are subject to the Mozilla Public
    License Version 1.1 (the "License"); you may not use this file
    except in compliance with the License. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

    Software distributed under the License is distributed on an "AS
    IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
    implied. See the License for the specific language governing
    rights and limitations under the License.

    The initial developer of the original code is David A. Hinds
    <dahinds@users.sourceforge.net>.  Portions created by David A. Hinds
    are Copyright (C) 1999 David A. Hinds.  All Rights Reserved.

    Contributor:  Apple Computer, Inc.  Portions © 2000 Apple Computer, 
    Inc. All rights reserved.

    Alternatively, the contents of this file may be used under the
    terms of the GNU Public License version 2 (the "GPL"), in which
    case the provisions of the GPL are applicable instead of the
    above.  If you wish to allow the use of your version of this file
    only under the terms of the GPL and not to allow others to use
    your version of this file under the MPL, indicate your decision
    by deleting the provisions above and replace them with the notice
    and other provisions required by the GPL.  If you do not delete
    the provisions above, a recipient may use your version of this
    file under either the MPL or the GPL.
    
======================================================================*/

#include <IOKit/pccard/config.h>
#define __NO_VERSION__
#include <IOKit/pccard/k_compat.h>

#ifdef __LINUX__
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/malloc.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/timer.h>
#endif

#define IN_CARD_SERVICES
#include <IOKit/pccard/cs_types.h>
#include <IOKit/pccard/ss.h>
#include <IOKit/pccard/cs.h>
#include <IOKit/pccard/bulkmem.h>
#include <IOKit/pccard/cistpl.h>
#include "cs_internal.h"

/*======================================================================

    This function handles submitting an MTD request, and retrying
    requests when an MTD is busy.

    An MTD request should never block.
    
======================================================================*/

static int do_mtd_request(memory_handle_t handle, mtd_request_t *req,
			  caddr_t buf)
{
    int ret, tries;
    client_t *mtd;
    socket_info_t *s;
    
    mtd = handle->mtd;
    if (mtd == NULL)
	return CS_GENERAL_FAILURE;
    s = SOCKET(mtd);
    wacquire(&mtd->mtd_req);
    for (ret = tries = 0; tries < 100; tries++) {
	mtd->event_callback_args.mtdrequest = req;
	mtd->event_callback_args.buffer = buf;
	ret = EVENT(mtd, CS_EVENT_MTD_REQUEST, CS_EVENT_PRI_LOW);
	if (ret != CS_BUSY)
	    break;
	switch (req->Status) {
	case MTD_WAITREQ:
	    /* Not that we should ever need this... */
	    wsleeptimeout(&mtd->mtd_req, HZ);
	    break;
	case MTD_WAITTIMER:
	case MTD_WAITRDY:
	    wsleeptimeout(&mtd->mtd_req, req->Timeout*HZ/1000);
	    req->Function |= MTD_REQ_TIMEOUT;
	    break;
	case MTD_WAITPOWER:
	    wsleep(&mtd->mtd_req);
	    break;
	}
#ifndef __MACOSX__
	if (signal_pending(current))
	    printk(KERN_NOTICE "cs: do_mtd_request interrupted!\n");
#endif
    }
    wrelease(&mtd->mtd_req);
    if (tries == 20) {
	printk(KERN_NOTICE "cs: MTD request timed out!\n");
	ret = CS_GENERAL_FAILURE;
    }
    wakeup(&mtd->mtd_req);
    retry_erase_list(&mtd->erase_busy, 0);
    return ret;
} /* do_mtd_request */

/*======================================================================

    This stuff is all for handling asynchronous erase requests.  It
    is complicated because all the retry stuff has to be dealt with
    in timer interrupts or in the card status event handler.

======================================================================*/

static void insert_queue(erase_busy_t *head, erase_busy_t *entry)
{
    DEBUG(2, "cs: adding 0x%p to queue 0x%p\n", entry, head);
    entry->next = head;
    entry->prev = head->prev;
    head->prev->next = entry;
    head->prev = entry;
}

static void remove_queue(erase_busy_t *entry)
{
    DEBUG(2, "cs: unqueueing 0x%p\n", entry);
    entry->next->prev = entry->prev;
    entry->prev->next = entry->next;
}

static void retry_erase(erase_busy_t *busy, u_int cause)
{
    eraseq_entry_t *erase = busy->erase;
    mtd_request_t req;
    client_t *mtd;
    socket_info_t *s;
    int ret;

    DEBUG(2, "cs: trying erase request 0x%p...\n", busy);
    if (busy->next)
	remove_queue(busy);
    req.Function = MTD_REQ_ERASE | cause;
    req.TransferLength = erase->Size;
    req.DestCardOffset = erase->Offset + erase->Handle->info.CardOffset;
    req.MediaID = erase->Handle->MediaID;
    mtd = erase->Handle->mtd;
    s = SOCKET(mtd);
    mtd->event_callback_args.mtdrequest = &req;
    wacquire(&mtd->mtd_req);
    ret = EVENT(mtd, CS_EVENT_MTD_REQUEST, CS_EVENT_PRI_LOW);
    wrelease(&mtd->mtd_req);
    if (ret == CS_BUSY) {
	DEBUG(2, "  Status = %d, requeueing.\n", req.Status);
	switch (req.Status) {
	case MTD_WAITREQ:
	case MTD_WAITPOWER:
	    insert_queue(&mtd->erase_busy, busy);
	    break;
	case MTD_WAITTIMER:
	case MTD_WAITRDY:
	    if (req.Status == MTD_WAITRDY)
		insert_queue(&s->erase_busy, busy);
	    mod_timer(&busy->timeout, jiffies + req.Timeout*HZ/1000);
	    break;
	}
    } else {
	/* update erase queue status */
	DEBUG(2, "  Ret = %d\n", ret);
	switch (ret) {
	case CS_SUCCESS:
	    erase->State = ERASE_PASSED; break;
	case CS_WRITE_PROTECTED:
	    erase->State = ERASE_MEDIA_WRPROT; break;
	case CS_BAD_OFFSET:
	    erase->State = ERASE_BAD_OFFSET; break;
	case CS_BAD_SIZE:
	    erase->State = ERASE_BAD_SIZE; break;
	case CS_NO_CARD:
	    erase->State = ERASE_BAD_SOCKET; break;
	default:
	    erase->State = ERASE_FAILED; break;
	}
	busy->client->event_callback_args.info = erase;
	EVENT(busy->client, CS_EVENT_ERASE_COMPLETE, CS_EVENT_PRI_LOW);
	kfree(busy);
	/* Resubmit anything waiting for a request to finish */
	wakeup(&mtd->mtd_req);
	retry_erase_list(&mtd->erase_busy, 0);
    }
} /* retry_erase */

void retry_erase_list(erase_busy_t *list, u_int cause)
{
    erase_busy_t tmp = *list;

    DEBUG(2, "cs: rescanning erase queue list 0x%p\n", list);
    if (list->next == list)
	return;
    /* First, truncate the original list */
    list->prev->next = &tmp;
    list->next->prev = &tmp;
    list->prev = list->next = list;
    tmp.prev->next = &tmp;
    tmp.next->prev = &tmp;

    /* Now, retry each request, in order. */
    while (tmp.next != &tmp)
	retry_erase(tmp.next, cause);
} /* retry_erase_list */

static void handle_erase_timeout(u_long arg)
{
    DEBUG(0, "cs: erase timeout for entry 0x%lx\n", arg);
    retry_erase((erase_busy_t *)arg, MTD_REQ_TIMEOUT);
}

static void setup_erase_request(client_handle_t handle, eraseq_entry_t *erase)
{
    erase_busy_t *busy;
    region_info_t *info;
    
    if (CHECK_REGION(erase->Handle))
	erase->State = ERASE_BAD_SOCKET;
    else {
	info = &erase->Handle->info;
	if ((erase->Offset >= info->RegionSize) ||
	    (erase->Offset & (info->BlockSize-1)))
	    erase->State = ERASE_BAD_OFFSET;
	else if ((erase->Offset+erase->Size > info->RegionSize) ||
		 (erase->Size & (info->BlockSize-1)))
	    erase->State = ERASE_BAD_SIZE;
	else {
	    erase->State = 1;
	    busy = kmalloc(sizeof(erase_busy_t), GFP_KERNEL);
	    busy->erase = erase;
	    busy->client = handle;
#ifndef __MACOSX__
	    busy->timeout.prev = busy->timeout.next = NULL;
#endif
	    busy->timeout.data = (u_long)busy;
	    busy->timeout.function = &handle_erase_timeout;
	    busy->prev = busy->next = NULL;
	    retry_erase(busy, 0);
	}
    }
} /* setup_erase_request */

/*======================================================================

    MTD helper functions

======================================================================*/

static int mtd_modify_window(window_handle_t win, mtd_mod_win_t *req)
{
    if ((win == NULL) || (win->magic != WINDOW_MAGIC))
	return CS_BAD_HANDLE;
    win->ctl.flags = MAP_16BIT | MAP_ACTIVE;
    if (req->Attributes & WIN_USE_WAIT)
	win->ctl.flags |= MAP_USE_WAIT;
    if (req->Attributes & WIN_MEMORY_TYPE)
	win->ctl.flags |= MAP_ATTRIB;
    win->ctl.speed = req->AccessSpeed;
    win->ctl.card_start = req->CardOffset;
    win->sock->ss_entry(win->sock->sock, SS_SetMemMap, &win->ctl);
    return CS_SUCCESS;
}

static int mtd_set_vpp(client_handle_t handle, mtd_vpp_req_t *req)
{
    socket_info_t *s;
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    if (req->Vpp1 != req->Vpp2)
	return CS_BAD_VPP;
    s = SOCKET(handle);
    s->socket.Vpp = req->Vpp1;
    if (s->ss_entry(s->sock, SS_SetSocket, &s->socket))
	return CS_BAD_VPP;
    return CS_SUCCESS;
}

static int mtd_rdy_mask(client_handle_t handle, mtd_rdy_req_t *req)
{
    socket_info_t *s;
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    s = SOCKET(handle);
    if (req->Mask & CS_EVENT_READY_CHANGE)
	s->socket.csc_mask |= SS_READY;
    else
	s->socket.csc_mask &= ~SS_READY;
    if (s->ss_entry(s->sock, SS_SetSocket, &s->socket))
	return CS_GENERAL_FAILURE;
    return CS_SUCCESS;
}

int MTDHelperEntry(int func, void *a1, void *a2)
{
    switch (func) {
    case MTDRequestWindow:
	return CardServices(RequestWindow, a1, a2, NULL);
    case MTDReleaseWindow:
	return CardServices(ReleaseWindow, a1, NULL, NULL);
    case MTDModifyWindow:
	return mtd_modify_window(a1, a2); break;
    case MTDSetVpp:
	return mtd_set_vpp(a1, a2); break;
    case MTDRDYMask:
	return mtd_rdy_mask(a1, a2); break;
    default:
	return CS_UNSUPPORTED_FUNCTION; break;
    }
} /* MTDHelperEntry */

/*======================================================================

    This stuff is used by Card Services to initialize the table of
    region info used for subsequent calls to GetFirstRegion and
    GetNextRegion.
    
======================================================================*/

static void setup_regions(client_handle_t handle, int attr,
			  memory_handle_t *list)
{
    int i, code, has_jedec, has_geo;
    u_int offset;
    cistpl_device_t device;
    cistpl_jedec_t jedec;
    cistpl_device_geo_t geo;
    memory_handle_t r;

    DEBUG(1, "cs: setup_regions(0x%p, %d, 0x%p)\n",
	  handle, attr, list);

    code = (attr) ? CISTPL_DEVICE_A : CISTPL_DEVICE;
    if (read_tuple(handle, code, &device) != CS_SUCCESS)
	return;
    code = (attr) ? CISTPL_JEDEC_A : CISTPL_JEDEC_C;
    has_jedec = (read_tuple(handle, code, &jedec) == CS_SUCCESS);
    if (has_jedec && (device.ndev != jedec.nid)) {
#ifdef PCMCIA_DEBUG
	printk(KERN_DEBUG "cs: Device info does not match JEDEC info.\n");
#endif
	has_jedec = 0;
    }
    code = (attr) ? CISTPL_DEVICE_GEO_A : CISTPL_DEVICE_GEO;
    has_geo = (read_tuple(handle, code, &geo) == CS_SUCCESS);
    if (has_geo && (device.ndev != geo.ngeo)) {
#ifdef PCMCIA_DEBUG
	printk(KERN_DEBUG "cs: Device info does not match geometry tuple.\n");
#endif
	has_geo = 0;
    }
    
    offset = 0;
    for (i = 0; i < device.ndev; i++) {
	if ((device.dev[i].type != CISTPL_DTYPE_NULL) &&
	    (device.dev[i].size != 0)) {
	    r = kmalloc(sizeof(*r), GFP_KERNEL);
	    r->region_magic = REGION_MAGIC;
	    r->state = 0;
	    r->dev_info[0] = '\0';
	    r->mtd = NULL;
	    r->info.Attributes = (attr) ? REGION_TYPE_AM : 0;
	    r->info.CardOffset = offset;
	    r->info.RegionSize = device.dev[i].size;
	    r->info.AccessSpeed = device.dev[i].speed;
	    if (has_jedec) {
		r->info.JedecMfr = jedec.id[i].mfr;
		r->info.JedecInfo = jedec.id[i].info;
	    } else
		r->info.JedecMfr = r->info.JedecInfo = 0;
	    if (has_geo) {
		r->info.BlockSize = geo.geo[i].buswidth *
		    geo.geo[i].erase_block * geo.geo[i].interleave;
		r->info.PartMultiple =
		    r->info.BlockSize * geo.geo[i].partition;
	    } else
		r->info.BlockSize = r->info.PartMultiple = 1;
	    r->info.next = *list; *list = r;
	}
	offset += device.dev[i].size;
    }
} /* setup_regions */

/*======================================================================

    This is tricky.  When get_first_region() is called by Driver
    Services, we initialize the region info table in the socket
    structure.  When it is called by an MTD, we can just scan the
    table for matching entries.
    
======================================================================*/

static int match_region(client_handle_t handle, memory_handle_t list,
			region_info_t *match)
{
    while (list != NULL) {
	if (!(handle->Attributes & INFO_MTD_CLIENT) ||
	    (strcmp(handle->dev_info, list->dev_info) == 0)) {
	    *match = list->info;
	    return CS_SUCCESS;
	}
	list = list->info.next;
    }
    return CS_NO_MORE_ITEMS;
} /* match_region */

int get_first_region(client_handle_t handle, region_info_t *rgn)
{
    socket_info_t *s = SOCKET(handle);
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    
    if ((handle->Attributes & INFO_MASTER_CLIENT) &&
	(!(s->state & SOCKET_REGION_INFO))) {
	setup_regions(handle, 0, &s->c_region);
	setup_regions(handle, 1, &s->a_region);
	s->state |= SOCKET_REGION_INFO;
    }

    if (rgn->Attributes & REGION_TYPE_AM)
	return match_region(handle, s->a_region, rgn);
    else
	return match_region(handle, s->c_region, rgn);
} /* get_first_region */

int get_next_region(client_handle_t handle, region_info_t *rgn)
{
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    return match_region(handle, rgn->next, rgn);
} /* get_next_region */

/*======================================================================

    Connect an MTD with a memory region.
    
======================================================================*/

int register_mtd(client_handle_t handle, mtd_reg_t *reg)
{
    memory_handle_t list;
    socket_info_t *s;
    
    if (CHECK_HANDLE(handle))
	return CS_BAD_HANDLE;
    s = SOCKET(handle);
    if (reg->Attributes & REGION_TYPE_AM)
	list = s->a_region;
    else
	list = s->c_region;
    DEBUG(1, "cs: register_mtd(0x%p, '%s', 0x%x)\n",
	  handle, handle->dev_info, reg->Offset);
    while (list) {
	if (list->info.CardOffset == reg->Offset) break;
	list = list->info.next;
    }
    if (list && (list->mtd == NULL) &&
	(strcmp(handle->dev_info, list->dev_info) == 0)) {
	list->info.Attributes = reg->Attributes;
	list->MediaID = reg->MediaID;
	list->mtd = handle;
	handle->mtd_count++;
	return CS_SUCCESS;
    } else
	return CS_BAD_OFFSET;
} /* register_mtd */

/*======================================================================

    Erase queue management functions
    
======================================================================*/

int register_erase_queue(client_handle_t *handle, eraseq_hdr_t *header)
{
    eraseq_t *queue;

    if ((handle == NULL) || CHECK_HANDLE(*handle))
	return CS_BAD_HANDLE;
    queue = kmalloc(sizeof(*queue), GFP_KERNEL);
    if (!queue) return CS_OUT_OF_RESOURCE;
    queue->eraseq_magic = ERASEQ_MAGIC;
    queue->handle = *handle;
    queue->count = header->QueueEntryCnt;
    queue->entry = header->QueueEntryArray;
    *handle = (client_handle_t)queue;
    return CS_SUCCESS;
} /* register_erase_queue */

int deregister_erase_queue(eraseq_handle_t eraseq)
{
    int i;
    if (CHECK_ERASEQ(eraseq))
	return CS_BAD_HANDLE;
    for (i = 0; i < eraseq->count; i++)
	if (ERASE_IN_PROGRESS(eraseq->entry[i].State)) break;
    if (i < eraseq->count)
	return CS_BUSY;
    eraseq->eraseq_magic = 0;
    kfree(eraseq);
    return CS_SUCCESS;
} /* deregister_erase_queue */

int check_erase_queue(eraseq_handle_t eraseq)
{
    int i;
    if (CHECK_ERASEQ(eraseq))
	return CS_BAD_HANDLE;
    for (i = 0; i < eraseq->count; i++)
	if (eraseq->entry[i].State == ERASE_QUEUED)
	    setup_erase_request(eraseq->handle, &eraseq->entry[i]);
    return CS_SUCCESS;
} /* check_erase_queue */

/*======================================================================

    Look up the memory region matching the request, and return a
    memory handle.
    
======================================================================*/

int open_memory(client_handle_t *handle, open_mem_t *open)
{
    socket_info_t *s;
    memory_handle_t region;
    
    if ((handle == NULL) || CHECK_HANDLE(*handle))
	return CS_BAD_HANDLE;
    s = SOCKET(*handle);
    if (open->Attributes & MEMORY_TYPE_AM)
	region = s->a_region;
    else
	region = s->c_region;
    while (region) {
	if (region->info.CardOffset == open->Offset) break;
	region = region->info.next;
    }
#ifdef __BEOS__
    if (region->dev_info[0] != '\0') {
	char n[80];
	struct module_info *m;
	sprintf(n, MTD_MODULE_NAME("%s"), region->dev_info);
	if (get_module(n, &m) != B_OK)
	    dprintf("cs: could not find MTD module %s\n", n);
    }
#endif
    if (region && region->mtd) {
	*handle = (client_handle_t)region;
	DEBUG(1, "cs: open_memory(0x%p, 0x%x) = 0x%p\n",
	      handle, open->Offset, region);
	return CS_SUCCESS;
    } else
	return CS_BAD_OFFSET;
} /* open_memory */

/*======================================================================

    Close a memory handle from an earlier call to OpenMemory.
    
    For the moment, I don't think this needs to do anything.
    
======================================================================*/

int close_memory(memory_handle_t handle)
{
    DEBUG(1, "cs: close_memory(0x%p)\n", handle);
    if (CHECK_REGION(handle))
	return CS_BAD_HANDLE;
#ifdef __BEOS__
    if (handle->dev_info[0] != '\0') {
	char n[80];
	sprintf(n, MTD_MODULE_NAME("%s"), handle->dev_info);
	put_module(n);
    }
#endif
    return CS_SUCCESS;
} /* close_memory */

/*======================================================================

    Read from a memory device, using a handle previously returned
    by a call to OpenMemory.
    
======================================================================*/

int read_memory(memory_handle_t handle, mem_op_t *req, caddr_t buf)
{
    mtd_request_t mtd;
    if (CHECK_REGION(handle))
	return CS_BAD_HANDLE;
    if (req->Offset >= handle->info.RegionSize)
	return CS_BAD_OFFSET;
    if (req->Offset+req->Count > handle->info.RegionSize)
	return CS_BAD_SIZE;
    
    mtd.SrcCardOffset = req->Offset + handle->info.CardOffset;
    mtd.TransferLength = req->Count;
    mtd.MediaID = handle->MediaID;
    mtd.Function = MTD_REQ_READ;
    if (req->Attributes & MEM_OP_BUFFER_KERNEL)
	mtd.Function |= MTD_REQ_KERNEL;
    return do_mtd_request(handle, &mtd, buf);
} /* read_memory */

/*======================================================================

    Write to a memory device, using a handle previously returned by
    a call to OpenMemory.
    
======================================================================*/

int write_memory(memory_handle_t handle, mem_op_t *req, caddr_t buf)
{
    mtd_request_t mtd;
    if (CHECK_REGION(handle))
	return CS_BAD_HANDLE;
    if (req->Offset >= handle->info.RegionSize)
	return CS_BAD_OFFSET;
    if (req->Offset+req->Count > handle->info.RegionSize)
	return CS_BAD_SIZE;
    
    mtd.DestCardOffset = req->Offset + handle->info.CardOffset;
    mtd.TransferLength = req->Count;
    mtd.MediaID = handle->MediaID;
    mtd.Function = MTD_REQ_WRITE;
    if (req->Attributes & MEM_OP_BUFFER_KERNEL)
	mtd.Function |= MTD_REQ_KERNEL;
    return do_mtd_request(handle, &mtd, buf);
} /* write_memory */

/*======================================================================

    This isn't needed for anything I could think of.

    macosx - this is used to directly map memory, instead of "copying"
    it.  yes, "copy" is a really bad name in this case.  if the
    hardware doesn't support memory mapping then this code should call
    read_memory to do the "copy".
    
======================================================================*/

int copy_memory(memory_handle_t handle, copy_op_t *req)
{
    if (CHECK_REGION(handle))
	return CS_BAD_HANDLE;
    return CS_UNSUPPORTED_FUNCTION;
}

