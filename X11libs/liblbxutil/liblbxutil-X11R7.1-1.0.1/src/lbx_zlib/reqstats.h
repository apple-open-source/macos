/* $Xorg: reqstats.h,v 1.4 2001/02/09 02:04:05 xorgcvs Exp $ */

/*

Copyright 1996  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/* $XFree86: xc/lib/lbxutil/lbx_zlib/reqstats.h,v 1.2 2001/08/27 19:01:07 dawes Exp $ */

#ifdef LBXREQSTATS
struct ReqStats {
    int count;
    unsigned long comp_bytes;
    unsigned long uncomp_bytes;
    int delta_count;
    unsigned long pre_delta_bytes;
    unsigned long post_delta_bytes;
};
#endif

extern void InitLbxReqStats(void);
