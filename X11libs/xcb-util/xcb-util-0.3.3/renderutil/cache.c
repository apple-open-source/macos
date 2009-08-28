/* Copyright © 2006 Jamey Sharp.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 * Except as contained in this notice, the names of the authors or their
 * institutions shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization from the authors.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "xcb_renderutil.h"
#include <stdlib.h>
#include <pthread.h>

typedef struct connection_cache {
    struct connection_cache      *next;		/* keep a linked list */
    xcb_connection_t                *c;		/* which display this is */
    xcb_render_query_version_reply_t     *version;
    xcb_render_query_pict_formats_reply_t *formats;
} connection_cache;

static struct {
    pthread_mutex_t   lock;
    connection_cache *head;           /* start of the list */
    connection_cache *cur;            /* most recently used */
} connections = { PTHREAD_MUTEX_INITIALIZER };

/*
 * If the server is missing support for any of the required depths on
 * any screen, tell the application that Render is not present.
 */

#define DEPTH_MASK(d)	(1 << ((d) - 1))
    
/*
 * Render requires support for depth 1, 4, 8, 24 and 32 pixmaps
 */

#define REQUIRED_DEPTHS	(DEPTH_MASK(1) | \
			 DEPTH_MASK(4) | \
			 DEPTH_MASK(8) | \
			 DEPTH_MASK(24) | \
			 DEPTH_MASK(32))

/* Test each depth not explicitly advertised to see if pixmap creation
 * succeeds: if it does, that depth is usable. */
static int
pixmap_depths_usable (xcb_connection_t *c, uint32_t missing, xcb_pixmap_t pixmap, xcb_drawable_t root)
{
    xcb_void_cookie_t create_cookie[32] = { { 0 } };
    xcb_void_cookie_t free_cookie[32]   = { { 0 } };
    int d;
    int success = 1;
    for (d = 1; d <= 32; d++)
	if (missing & DEPTH_MASK(d))
	{
	    create_cookie[d - 1] = xcb_create_pixmap_checked (c, d, pixmap, root, 1, 1);
	    free_cookie[d - 1] = xcb_free_pixmap_checked (c, pixmap);
	    if (!create_cookie[d - 1].sequence || !free_cookie[d - 1].sequence)
	    {
		success = 0;
		break;
	    }
	}
    for (d = 0; d < 32; d++)
	if (create_cookie[d].sequence || free_cookie[d].sequence)
	{
	    xcb_generic_error_t *create_error = xcb_request_check (c, create_cookie[d]);
	    xcb_generic_error_t *free_error = xcb_request_check (c, free_cookie[d]);
	    success = success && !create_error;
	    free(create_error);
	    free(free_error);
	}
    return success;
}

static int
has_required_depths (xcb_connection_t *c)
{
    xcb_screen_iterator_t screens;
    xcb_pixmap_t pixmap = { -1 };
    for (screens = xcb_setup_roots_iterator(xcb_get_setup(c)); screens.rem; xcb_screen_next(&screens))
    {
	xcb_depth_iterator_t depths;
	uint32_t missing = REQUIRED_DEPTHS;
	xcb_drawable_t root;

	for (depths = xcb_screen_allowed_depths_iterator(screens.data); depths.rem; xcb_depth_next(&depths))
	    missing &= ~DEPTH_MASK(depths.data->depth);
	if (!missing)
	    continue;

	/*
	 * Ok, this is ugly.  It should be sufficient at this
	 * point to just return false, but Xinerama is broken at
	 * this point and only advertises depths which have an
	 * associated visual.  Of course, the other depths still
	 * work, but the only way to find out is to try them.
	 */
	if (pixmap == -1)
	    pixmap = xcb_generate_id(c);
	root = screens.data->root;
	if (!pixmap_depths_usable (c, missing, pixmap, root))
	    return 0;
    }
    return 1;
}

static connection_cache *
find_or_create_display (xcb_connection_t *c)
{
    connection_cache *info;
    xcb_render_query_version_cookie_t version_cookie;
    xcb_render_query_pict_formats_cookie_t formats_cookie;
    int present;

    /*
     * look for display in list
     */
    for (info = connections.head; info; info = info->next)
        if (info->c == c) {
            connections.cur = info;     /* cache most recently used */
	    return info;
        }

    /*
     * don't already have this display: add it.
     */
    info = malloc (sizeof (connection_cache));
    if (!info)
	return NULL;
    info->c = c;

    version_cookie = xcb_render_query_version(c, 0, 10);
    formats_cookie = xcb_render_query_pict_formats(c);
    xcb_flush(c);
    present = has_required_depths (c);
    info->version = xcb_render_query_version_reply(c, version_cookie, 0);
    info->formats = xcb_render_query_pict_formats_reply(c, formats_cookie, 0);

    if (!present || !info->version || !info->formats)
    {
	free(info->version);
	info->version = 0;
	free(info->formats);
	info->formats = 0;
    }
    /* Check for the lack of sub-pixel data */
    else if (info->version->major_version == 0 && info->version->minor_version < 6)
	info->formats->num_subpixel = 0;

    /*
     * now, chain it onto the list
     */
    info->next = connections.head;
    connections.head = info;
    connections.cur = info;

    return info;
}

static connection_cache *
find_display (xcb_connection_t *c)
{
    connection_cache *info;

    /*
     * see if this was the most recently accessed display
     */
    if ((info = connections.cur) && info->c == c) 
        return info;

    pthread_mutex_lock(&connections.lock);
    info = find_or_create_display (c);
    pthread_mutex_unlock(&connections.lock);
    return info;
}

const xcb_render_query_version_reply_t *
xcb_render_util_query_version (xcb_connection_t *c)
{
    connection_cache *info = find_display (c);
    if (!info)
	return 0;
    return info->version;
}

const xcb_render_query_pict_formats_reply_t *
xcb_render_util_query_formats (xcb_connection_t *c)
{
    connection_cache *info = find_display (c);
    if (!info)
	return 0;
    return info->formats;
}

int
xcb_render_util_disconnect (xcb_connection_t *c)
{
    connection_cache **prev, *cur = NULL;
    pthread_mutex_lock(&connections.lock);
    for(prev = &connections.head; *prev; prev = &(*prev)->next)
	if((*prev)->c == c)
	{
	    cur = *prev;
	    *prev = cur->next;
	    if(cur == connections.cur)
		connections.cur = NULL;  /* flush cache */
	    free(cur->version);
	    free(cur->formats);
	    free(cur);
	    break;
	}
    pthread_mutex_unlock(&connections.lock);
    return cur != NULL;
}
