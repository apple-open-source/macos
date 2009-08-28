/*
 * Copyright © 2008 Ian Osgood <iano@quirkster.com>
 * Copyright © 2008 Jamey Sharp <jamey@minilop.net>
 * Copyright © 2008 Josh Triplett <josh@freedesktop.org>
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
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <xcb/xcb.h>
#include "reply_formats.h"
#include "xcb_aux.h"
#include "xcb_event.h"
#include "xcb_atom.h"
#include "xcb_icccm.h"
#include "xcb_wm.h"

static const int TOP = 20;
static const int LEFT = 5;
static const int BOTTOM = 5;
static const int RIGHT = 5;

static const int TEST_THREADS = 1;
static const int TEST_WATCH_ROOT = 1;

static int16_t move_from_x = -1;
static int16_t move_from_y = -1;

static int handleEvent(void *ignored, xcb_connection_t *c, xcb_generic_event_t *e)
{
	return format_event(e);
}

static int handleButtonPressEvent(void *data, xcb_connection_t *c, xcb_button_press_event_t *e)
{
	if(move_from_x != -1 && move_from_y != -1)
	{
		printf("Weird. Got ButtonPress after ButtonPress.\n");
		return 0;
	}
	move_from_x = e->root_x;
	move_from_y = e->root_y;
	return 1;
}

static int handleButtonReleaseEvent(void *data, xcb_connection_t *c, xcb_button_release_event_t *e)
{
	uint32_t values[2];
	if(move_from_x == -1 && move_from_y == -1)
	{
		printf("Weird. Got ButtonRelease without ButtonPress.\n");
		return 0;
	}
	values[0] = /* x */ e->root_x;
	values[1] = /* y */ e->root_y;
	xcb_configure_window(c, e->event, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, values);
	xcb_flush(c);
	move_from_x = -1;
	move_from_y = -1;
	return 1;
}

static int addClientWindow(xcb_window_t child, xcb_window_t parent, xcb_gcontext_t titlegc)
{
	int success;
	client_window_t *record = malloc(sizeof(client_window_t));
	assert(record);
	record->child = child;
	record->parent = parent;
	record->name_len = 0;
	record->name = 0;
	record->titlegc = titlegc;
	success = table_put(byParent, parent, record) &&
		table_put(byChild, child, record);
	assert(success);
	return 1;
}

void reparent_window(xcb_connection_t *c, xcb_window_t child,
		xcb_visualid_t v, xcb_window_t r, uint8_t d,
		int16_t x, int16_t y, uint16_t width, uint16_t height)
{
	xcb_window_t w;
	xcb_drawable_t drawable;
	uint32_t mask = 0;
	uint32_t values[3];
	xcb_screen_t *root = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
	xcb_gcontext_t titlegc;

	w = xcb_generate_id(c);

	mask |= XCB_CW_BACK_PIXEL;
	values[0] = root->white_pixel;

	mask |= XCB_CW_OVERRIDE_REDIRECT;
	values[1] = 1;

	mask |= XCB_CW_EVENT_MASK;
	values[2] = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE
		| XCB_EVENT_MASK_EXPOSURE /* | XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW */;

	printf("Reparenting 0x%08x under 0x%08x.\n", child, w);
	xcb_create_window(c, d, w, r, x, y,
			width + LEFT + RIGHT, height + TOP + BOTTOM,
			/* border_width */ 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, v, mask, values);
	xcb_change_save_set(c, XCB_SET_MODE_INSERT, child);
	xcb_map_window(c, w);

	titlegc = xcb_generate_id(c);

	mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND;
	values[0] = root->black_pixel;
	values[1] = root->white_pixel;
	drawable = w;
	xcb_create_gc(c, titlegc, drawable, mask, values);
	addClientWindow(child, w, titlegc);

	xcb_reparent_window(c, child, w, LEFT - 1, TOP - 1);

	mask = XCB_CW_EVENT_MASK;
	values[0] = XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;
	xcb_change_window_attributes(c, child, mask, values);

	xcb_flush(c);
}

static void redrawWindow(xcb_connection_t *c, client_window_t *client)
{
	xcb_drawable_t d = { client->parent };
	if(!client->name_len)
		return;
	xcb_clear_area(c, 0, d, 0, 0, 0, 0);
	xcb_image_text_8(c, client->name_len, d, client->titlegc,
			LEFT - 1, TOP - 4, client->name);
	xcb_flush(c);
}

static int handleExposeEvent(void *data, xcb_connection_t *c, xcb_expose_event_t *e)
{
	client_window_t *client = table_get(byParent, e->window);
	if(!client || e->count != 0)
		return 1;
	redrawWindow(c, client);
	return 1;
}

static int handleWMNameChange(void *data, xcb_connection_t *c, uint8_t state, xcb_window_t window, xcb_atom_t atom, xcb_get_property_reply_t *prop)
{
	client_window_t *client = table_get(byChild, window);
	printf("WM_NAME change: Window 0x%08x ", window);
	if(!client)
	{
		printf("is not being managed.\n");
		return 0;
	}
	if(client->name)
	{
		printf("was named \"%.*s\"; now ", client->name_len, client->name);
		free(client->name);
	}
	if(!prop)
	{
		client->name_len = 0;
		client->name = 0;
		printf("has no name.\n");
		return 1;
	}

	client->name_len = xcb_get_property_value_length(prop);
	client->name = malloc(client->name_len);
	assert(client->name);
	strncpy(client->name, xcb_get_property_value(prop), client->name_len);
	printf("is named \"%.*s\".\n", client->name_len, client->name);

	redrawWindow(c, client);
	return 1;
}

int main(int argc, char **argv)
{
	xcb_connection_t *c;
	xcb_event_handlers_t evenths;
	xcb_property_handlers_t prophs;
	xcb_window_t root;
	pthread_t event_thread;
        int screen_nbr;
	int i;

	byChild = alloc_table();
	byParent = alloc_table();

	c = xcb_connect(NULL, &screen_nbr);

	xcb_event_handlers_init(c, &evenths);

	for(i = 2; i < 128; ++i)
		xcb_event_set_handler(&evenths, i, handleEvent, 0);
	for(i = 0; i < 256; ++i)
		xcb_event_set_error_handler(&evenths, i, (xcb_generic_error_handler_t) handleEvent, 0);
	xcb_event_set_button_press_handler(&evenths, handleButtonPressEvent, 0);
	xcb_event_set_button_release_handler(&evenths, handleButtonReleaseEvent, 0);
	xcb_event_set_unmap_notify_handler(&evenths, handle_unmap_notify_event, 0);
	xcb_event_set_expose_handler(&evenths, handleExposeEvent, 0);

	xcb_property_handlers_init(&prophs, &evenths);
	xcb_event_set_map_notify_handler(&evenths, handle_map_notify_event, &prophs);
	xcb_watch_wm_name(&prophs, 40, handleWMNameChange, 0);

	if(TEST_THREADS)
	{
		pthread_create(&event_thread, 0, (void *(*)(void *))xcb_event_wait_for_event_loop, &evenths);
	}

	root = xcb_aux_get_screen(c, screen_nbr)->root;

	{
		uint32_t mask = XCB_CW_EVENT_MASK;
		uint32_t values[] = { XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE };
		xcb_change_window_attributes(c, root, mask, values);
	}
	xcb_flush(c);

	manage_existing_windows(c, &prophs, root);

	/* Terminate only when the event loop terminates */
	if(TEST_THREADS)
		pthread_join(event_thread, 0);
	else
		xcb_event_wait_for_event_loop(&evenths);

	exit(0);
	/*NOTREACHED*/
}
