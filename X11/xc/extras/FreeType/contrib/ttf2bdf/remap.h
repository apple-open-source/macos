/*
 * Copyright 1996, 1997, 1998, 1999 Computing Research Labs,
 * New Mexico State University
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COMPUTING RESEARCH LAB OR NEW MEXICO STATE UNIVERSITY BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
 * OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _h_remap
#define _h_remap

/*
 * Id: remap.h,v 1.4 1999/05/03 17:07:04 mleisher Exp $
 */

#ifdef __cplusplus
extern "C" {
#endif

extern int ttf2bdf_load_map(
#ifdef __STDC__
    FILE *in
#endif
);

extern void ttf2bdf_free_map(
#ifdef __STDC__
    void
#endif
);

extern int ttf2bdf_remap(
#ifdef __STDC__
    unsigned short *code
#endif
);

extern void ttf2bdf_remap_charset(
#ifdef __STDC__
    char **registry_name,
    char **encoding_name
#endif
);

#ifdef __cplusplus
}
#endif

#endif /* _h_remap */
