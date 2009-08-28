/*
Copyright (c) 2002-2003 by Juliusz Chroboczek

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
/* $XdotOrg: xc/programs/fonttosfnt/fonttosfnt.c,v 1.4 2003/12/19 02:16:36 dawes Exp $ */
/* $XFree86: xc/programs/fonttosfnt/fonttosfnt.c,v 1.3 2003/07/08 15:39:49 tsi Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fonttosfnt.h"

int verbose_flag = 0;
int reencode_flag = 1;
int glyph_flag = 2;
int metrics_flag = 1;
int crop_flag = 1;
int bit_aligned_flag = 1;

static void
usage(void)
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, 
            "fonttosfnt [ -v ] [ -c ] [ -b ] [ -r ] [ -g n ] [ -m n ] -o font.ttf "
            "[ -- ] font ...\n");
}

int
main(int argc, char **argv)
{
    int i;
    int rc;
    char *output = NULL;
    FontPtr font;

    i = 1;
    while(i < argc) {
        if(argv[i][0] != '-')
            break;

        if(argv[i][1] == 'o') {
            if(argv[i][2] == '\0') {
                output = sprintf_alloc("%s", argv[i + 1]);
                i += 2;
            } else {
                output = sprintf_alloc("%s", argv[i] + 2);
                i++;
            }
        } else if(strcmp(argv[i], "-v") == 0) {
            verbose_flag = 1;
            i++;
        } else if(strcmp(argv[i], "-c") == 0) {
            crop_flag = 0;
            i++;
        } else if(strcmp(argv[i], "-b") == 0) {
            bit_aligned_flag = 0;
            i++;
        } else if(strcmp(argv[i], "-r") == 0) {
            reencode_flag = 0;
            i++;
        } else if(strcmp(argv[i], "-g") == 0) {
            if(argc <= i + 1) {
                usage();
                exit(1);
            }
            glyph_flag = atoi(argv[i + 1]);
            i += 2;
        } else if(strcmp(argv[i], "-m") == 0) {
            if(argc <= i + 1) {
                usage();
                exit(1);
            }
            metrics_flag = atoi(argv[i + 1]);
            i += 2;
        } else if(strcmp(argv[i], "--") == 0) {
            i++;
            break;
        } else {
            usage();
            exit(1);
        }
    }

    if(output == NULL) {
        usage();
        exit(1);
    }

    font = makeFont();

    while(i < argc) {
        rc = readFile(argv[i], font);
        if(rc != 0)
            exit(1);
        i++;
    }

    writeFile(output, font);
    return 0;
}
