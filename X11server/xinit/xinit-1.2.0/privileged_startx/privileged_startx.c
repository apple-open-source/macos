/* Copyright (c) 2008 Apple Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT
 * HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above
 * copyright holders shall not be used in advertising or otherwise to
 * promote the sale, use or other dealings in this Software without
 * prior written authorization.
 */

#include <string.h>
#include <stdio.h>

#ifndef SCRIPTDIR
#define SCRIPTDIR="/usr/X11/lib/X11/xinit/privileged_startx.d"
#endif

static void usage(const char *prog) {
    fprintf(stderr, "%s: usage\n", prog);
    fprintf(stderr, "    %s [-d [<script dir>]]\n\n", prog);
    fprintf(stderr, "              -d: Passed when called from launchd to denote server-mode.\n");
    fprintf(stderr, "    <script dir>: Directory to use instead of %s\n", SCRIPTDIR);
}

int client_main(void);
int server_main(const char *dir);

int main(int argc, char *argv[]) {

    if(argc == 1) {
        return client_main();
    } else if(!strncmp(argv[1], "-d", 2)) {
        if(argc == 2)
            return server_main(NULL);
        else if(argc == 3)
            return server_main(argv[2]);
    }

    usage(argv[0]);
    return 1;
}
