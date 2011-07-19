/*
 *	XIint.h - Header definition and support file for the internal
 *	support routines used by the Xi library.
 */

#ifndef _XIINT_H_
#define _XIINT_H_
#include <X11/extensions/XI.h>

extern XExtDisplayInfo *XInput_find_display(Display *);

extern int _XiCheckExtInit(Display *, int, XExtDisplayInfo *);

extern XExtensionVersion *_XiGetExtensionVersion(Display *, _Xconst char *, XExtDisplayInfo *);
extern XExtensionVersion* _XiGetExtensionVersionRequest(Display *dpy, _Xconst char *name, int xi_opcode);
extern Status _xiQueryVersion(Display *dpy, int*, int*, XExtDisplayInfo *);

extern Status _XiEventToWire(
    register Display *		/* dpy */,
    register XEvent *		/* re */,
    register xEvent **		/* event */,
    register int *		/* count */
);

typedef struct _XInputData
{
    XEvent data;
    XExtensionVersion *vers;
} XInputData;


/**
 * Returns the next valid memory block of the given size within the block
 * previously allocated.
 * Use letting pointers inside a struct point to bytes after the same
 * struct, e.g. during protocol parsing etc.
 *
 * Caller is responsible for allocating enough memory.
 *
 * Example:
 *    void *ptr;
 *    struct foo {
 *       int num_a;
 *       int num_b;
 *       int *a;
 *       int *b;
 *    } bar;
 *
 *    ptr = malloc(large_enough);
 *    bar = next_block(&ptr, sizeof(struct foo));
 *    bar->num_a = 10;
 *    bar->num_b = 20;
 *    bar->a = next_block(&ptr, bar->num_a);
 *    bar->b = next_block(&ptr, bar->num_b);
 */
static inline void*
next_block(void **ptr, int size) {
    void *ret = *ptr;

    if (!*ptr)
        return NULL;

    *(unsigned char**)ptr += size;

    return ret;
}

#endif
