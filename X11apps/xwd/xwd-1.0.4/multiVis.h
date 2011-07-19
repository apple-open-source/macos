/** ------------------------------------------------------------------------
	This file contains routines for manipulating generic lists.
	Lists are implemented with a "harness".  In other words, each
	node in the list consists of two pointers, one to the data item
	and one to the next node in the list.  The head of the list is
	the same struct as each node, but the "item" ptr is used to point
	to the current member of the list (used by the first_in_list and
	next_in_list functions).

Copyright 1994 Hewlett-Packard Co.
Copyright 1996, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

 ------------------------------------------------------------------------ **/

extern int GetMultiVisualRegions(
    Display *, Window, int, int, unsigned int,
    unsigned int, int *, int *, XVisualInfo **, int *,
    OverlayInfo  **, int *, XVisualInfo ***, list_ptr *,
    list_ptr *, int *
); 

extern XImage *ReadAreaToImage(
    Display *, Window, int, int, unsigned int,
    unsigned int, int, XVisualInfo *, int,
    OverlayInfo	*, int, XVisualInfo **, list_ptr,
    list_ptr, int, int
);

extern void initFakeVisual(
    Visual *
);
