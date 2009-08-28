/*
 * Copyright © 2008 Julien Danjou <julien@danjou.info>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the names of the authors or
 * their institutions shall not be used in advertising or otherwise to
 * promote the sale, use or other dealings in this Software without
 * prior written authorization from the authors.
 */

#include <stdlib.h>
#include <string.h>

#include "xcb_property.h"

xcb_get_property_cookie_t
xcb_get_any_property(xcb_connection_t *c, uint8_t del, xcb_window_t window, xcb_atom_t name, uint32_t long_len)
{
    static const xcb_atom_t type = XCB_GET_PROPERTY_TYPE_ANY;

    return xcb_get_property(c, del, window, name, type, 0, long_len);
}

xcb_get_property_cookie_t
xcb_get_any_property_unchecked(xcb_connection_t *c,
                                                             uint8_t del,
                                                             xcb_window_t window,
                                                             xcb_atom_t name,
                                                             uint32_t long_len)
{
    return xcb_get_property_unchecked(c, del, window, name, XCB_GET_PROPERTY_TYPE_ANY, 0, long_len);
}

static int
call_handler(xcb_connection_t *c, uint8_t state, xcb_window_t window, xcb_atom_t atom, xcb_property_handler_t *h)
{
    xcb_get_property_reply_t *propr = 0;
    int                     ret;

    if(state != XCB_PROPERTY_DELETE)
    {
        xcb_get_property_cookie_t cookie = xcb_get_any_property(c, 0, window, atom, h->long_len);
        propr = xcb_get_property_reply(c, cookie, 0);
    }
    ret = h->handler(h->data, c, state, window, atom, propr);
    free(propr);
    return ret;
}

int
xcb_property_changed(xcb_property_handlers_t *prophs, uint8_t state, xcb_window_t window, xcb_atom_t atom)
{
    xcb_connection_t *c = xcb_event_get_xcb_connection(xcb_property_get_event_handlers(prophs));
    xcb_property_handler_node_t *cur;

    for(cur = prophs->head; cur; cur = cur->next)
        if(cur->name == atom)
            return call_handler(c, state, window, atom, &cur->h);

    if(prophs->def.handler)
        return call_handler(c, state, window, atom, &prophs->def);

    return 0;
}

static int
handle_property_notify_event(void *data, xcb_connection_t *c, xcb_property_notify_event_t *e)
{
    xcb_property_handlers_t *prophs = data;
    uint8_t state = e->state;
    xcb_window_t window = e->window;
    xcb_atom_t atom = e->atom;

    return xcb_property_changed(prophs, state, window, atom);
}

void
xcb_property_handlers_init(xcb_property_handlers_t *prophs, xcb_event_handlers_t *evenths)
{
    memset(prophs, 0, sizeof(prophs));
    prophs->evenths = evenths;
    xcb_event_set_property_notify_handler(evenths, handle_property_notify_event, prophs);
}

void
xcb_property_handlers_wipe(xcb_property_handlers_t *prophs)
{
    xcb_property_handler_node_t *node, *next;

    for(node = prophs->head; node; node = next)
    {
            next = node->next;
            free(node);
    }
}

xcb_event_handlers_t *
xcb_property_get_event_handlers(xcb_property_handlers_t *prophs)
{
    return prophs->evenths;
}

static inline void
set_prop_handler(xcb_property_handler_t *cur, uint32_t long_len, xcb_generic_property_handler_t handler, void *data)
{
    cur->long_len = long_len;
    cur->handler = handler;
    cur->data = data;
}

uint8_t
xcb_property_set_handler(xcb_property_handlers_t *prophs, xcb_atom_t name, uint32_t long_len, xcb_generic_property_handler_t handler, void *data)
{
    xcb_property_handler_node_t *cur = malloc(sizeof(xcb_property_handler_node_t));
    if(!cur)
        return 0;
    cur->next = prophs->head;
    cur->name = name;
    set_prop_handler(&cur->h, long_len, handler, data);
    prophs->head = cur;
    return 1;
}

uint8_t
xcb_property_set_default_handler(xcb_property_handlers_t *prophs, uint32_t long_len, xcb_generic_property_handler_t handler, void *data)
{
    set_prop_handler(&prophs->def, long_len, handler, data);
    return 1;
}
