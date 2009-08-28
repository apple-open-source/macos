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
#include <xcb/xcbext.h>

#include "xcb_reply.h"

void
xcb_reply_handlers_init(xcb_connection_t *c, xcb_reply_handlers_t *r)
{
    static const pthread_mutex_t proto_lock = PTHREAD_MUTEX_INITIALIZER;
    static const pthread_cond_t proto_cond = PTHREAD_COND_INITIALIZER;
    r->lock = proto_lock;
    r->cond = proto_cond;
    r->c = c;
    r->head = NULL;
}

xcb_connection_t *
xcb_reply_get_xcb_connection(xcb_reply_handlers_t *h)
{
    return h->c;
}

static void
insert_handler(xcb_reply_handlers_t *h, struct xcb_reply_node *cur)
{
    struct xcb_reply_node **prev = &h->head;
    while(*prev && (*prev)->request < cur->request)
        prev = &(*prev)->next;
    cur->next = *prev;
    *prev = cur;
}

static void
remove_handler(xcb_reply_handlers_t *h, struct xcb_reply_node *cur)
{
    struct xcb_reply_node **prev = &h->head;
    while(*prev && (*prev)->request < cur->request)
        prev = &(*prev)->next;
    if(!(*prev) || (*prev)->request != cur->request)
        return;
    *prev = cur->next;
    free(cur);
}

static int
process_replies(xcb_reply_handlers_t *h, int block)
{
    int handled = 0;
    pthread_mutex_lock(&h->lock);
    pthread_cleanup_push((void (*)(void *)) pthread_mutex_unlock, &h->lock);
    while(1)
    {
        struct xcb_reply_node *cur = h->head;
        xcb_generic_error_t *error;
        void *reply;
        pthread_mutex_unlock(&h->lock);
        pthread_cleanup_push((void (*)(void *)) pthread_mutex_lock, &h->lock);
        if(block)
            reply = xcb_wait_for_reply(h->c, cur->request, &error);
        else if(!xcb_poll_for_reply(h->c, cur->request, &reply, &error))
            return handled;
        if(reply || error)
        {
            cur->handler(cur->data, h->c, reply, error);
            cur->handled = 1;
            free(reply);
            free(error);
        }
        handled |= cur->handled;
        pthread_cleanup_pop(1);
        if(!reply)
            remove_handler(h, cur);
        if(!h->head)
        {
            if(block)
                pthread_cond_wait(&h->cond, &h->lock);
            else
                break;
        }
    }
    pthread_cleanup_pop(1);
    return handled;
}

static void *
reply_thread(void *h)
{
    process_replies(h, 1);
    return 0;
}

void
xcb_reply_start_thread(xcb_reply_handlers_t *h)
{
    pthread_create(&h->thread, 0, reply_thread, h);
}

void
xcb_reply_stop_thread(xcb_reply_handlers_t *h)
{
    pthread_cancel(h->thread);
    pthread_join(h->thread, 0);
}

void
xcb_reply_add_handler(xcb_reply_handlers_t *h, unsigned int request, xcb_generic_reply_handler_t handler, void *data)
{
    struct xcb_reply_node *cur = malloc(sizeof(struct xcb_reply_node));
    cur->request = request;
    cur->handler = handler;
    cur->data = data;
    cur->handled = 0;

    pthread_mutex_lock(&h->lock);
    insert_handler(h, cur);
    pthread_cond_broadcast(&h->cond);
    pthread_mutex_unlock(&h->lock);
}
