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

#include <assert.h>
#include <stdlib.h>

#include "xcb_event.h"

void
xcb_event_handlers_init(xcb_connection_t *c, xcb_event_handlers_t *evenths)
{
    evenths->c = c;
}

xcb_connection_t *
xcb_event_get_xcb_connection(xcb_event_handlers_t *evenths)
{
    return evenths->c;
}

static xcb_event_handler_t *
get_event_handler(xcb_event_handlers_t *evenths, int event)
{
    assert(event < 256);
    event &= XCB_EVENT_RESPONSE_TYPE_MASK;
    assert(event >= 2);
    return &evenths->event[event - 2];
}

static xcb_event_handler_t *
get_error_handler(xcb_event_handlers_t *evenths, int error)
{
    assert(error >= 0 && error < 256);
    return &evenths->error[error];
}

int
xcb_event_handle(xcb_event_handlers_t *evenths, xcb_generic_event_t *event)
{
    xcb_event_handler_t *eventh = 0;
    assert(event->response_type != 1);

    if(event->response_type == 0)
        eventh = get_error_handler(evenths, ((xcb_generic_error_t *) event)->error_code);
    else
        eventh = get_event_handler(evenths, event->response_type);

    if(eventh->handler)
        return eventh->handler(eventh->data, evenths->c, event);
    return 0;
}

void
xcb_event_wait_for_event_loop(xcb_event_handlers_t *evenths)
{
    xcb_generic_event_t *event;
    while((event = xcb_wait_for_event(evenths->c)))
    {
        xcb_event_handle(evenths, event);
        free(event);
    }
}

void
xcb_event_poll_for_event_loop(xcb_event_handlers_t *evenths)
{
    xcb_generic_event_t *event;
    while ((event = xcb_poll_for_event(evenths->c)))
    {
        xcb_event_handle(evenths, event);
        free(event);
    }
}

static void
set_handler(xcb_generic_event_handler_t handler, void *data, xcb_event_handler_t *place)
{
    xcb_event_handler_t eventh = { handler, data };
    *place = eventh;
}

void
xcb_event_set_handler(xcb_event_handlers_t *evenths, int event, xcb_generic_event_handler_t handler, void *data)
{
    set_handler(handler, data, get_event_handler(evenths, event));
}

void
xcb_event_set_error_handler(xcb_event_handlers_t *evenths, int error, xcb_generic_error_handler_t handler, void *data)
{
    set_handler((xcb_generic_event_handler_t) handler, data, get_error_handler(evenths, error));
}
