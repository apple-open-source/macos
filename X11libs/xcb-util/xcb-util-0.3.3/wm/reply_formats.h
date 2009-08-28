/*
 * Copyright (C) 2001-2002 Bart Massey and Jamey Sharp.
 * All Rights Reserved.  See the file COPYING in this directory
 * for licensing information.
 */

#ifndef REPLY_FORMATS_H
#define REPLY_FORMATS_H

#include <xcb/xcb.h>

int format_get_window_attributes_reply(xcb_window_t wid, xcb_get_window_attributes_reply_t *reply);
int format_get_geometry_reply(xcb_window_t wid, xcb_get_geometry_reply_t *reply);
int format_query_tree_reply(xcb_window_t wid, xcb_query_tree_reply_t *reply);
int format_event(xcb_generic_event_t *e);

#if 0 /* not yet ready */
int formatButtonReleaseEvent(void *data, xcb_connection_t *c, xcb_button_release_event_t *event);
int formatEnterNotifyEvent(void *data, xcb_connection_t *c, xcb_enter_notify_event_t *event);
int formatExposeEvent(void *data, xcb_connection_t *c, xcb_expose_event_t *event);
int formatDestroyNotifyEvent(void *data, xcb_connection_t *c, xcb_destroy_notify_event_t *event);
int formatUnmapNotifyEvent(void *data, xcb_connection_t *c, xcb_unmap_notify_event_t *event);
int formatMapNotifyEvent(void *data, xcb_connection_t *c, xcb_map_notify_event_t *event);
int formatReparentNotifyEvent(void *data, xcb_connection_t *c, xcb_reparent_notify_event_t *event);
int formatConfigureNotifyEvent(void *data, xcb_connection_t *c, xcb_configure_notify_event_t *event);
int formatGravityNotifyEvent(void *data, xcb_connection_t *c, xcb_gravity_notify_event_t *event);
int formatCirculateNotifyEvent(void *data, xcb_connection_t *c, xcb_circulate_notify_event_t *event);
int formatClientMessageEvent(void *data, xcb_connection_t *c, xcb_client_message_event_t *event);
#endif

#endif /* REPLY_FORMATS_H */
