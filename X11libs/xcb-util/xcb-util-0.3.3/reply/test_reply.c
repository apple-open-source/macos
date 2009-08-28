/*
 * Copyright © 2008 Ian Osgood  <iano@quirkster.com>
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

#include "xcb_reply.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void fontinfo_handler(void *data, xcb_connection_t *c, xcb_generic_reply_t *rg, xcb_generic_error_t *eg)
{
	xcb_list_fonts_with_info_reply_t *rep = (xcb_list_fonts_with_info_reply_t *) rg;
	if(rep)
	{
		int len = xcb_list_fonts_with_info_name_length(rep);
		if(len)
			printf("(+%u) Font \"%.*s\"\n",
					(unsigned int) rep->replies_hint,
					len, xcb_list_fonts_with_info_name(rep));
		else
		{
			pthread_mutex_lock(&lock);
			pthread_cond_broadcast(&cond);
			pthread_mutex_unlock(&lock);
			printf("End of font list.\n");
		}
	}
	if(eg)
		printf("Error from ListFontsWithInfo: %d\n", eg->error_code);
}

int main(int argc, char **argv)
{
	int count = 10;
	char *pattern = "*";
	xcb_connection_t *c = xcb_connect(NULL, NULL);
	xcb_reply_handlers_t h;
        xcb_reply_handlers_init(c, &h);

	if(argc > 1)
		count = atoi(argv[1]);
	if(argc > 2)
		pattern = argv[2];
	
	xcb_reply_add_handler(&h, xcb_list_fonts_with_info(c, count, strlen(pattern), pattern).sequence, fontinfo_handler, 0);
	pthread_mutex_lock(&lock);
	xcb_reply_start_thread(&h);
	pthread_cond_wait(&cond, &lock);
	xcb_reply_stop_thread(&h);
	pthread_mutex_unlock(&lock);

	xcb_disconnect(c);
	exit(0);
}
