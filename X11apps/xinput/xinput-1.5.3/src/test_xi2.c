/*
 * Copyright © 2009 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */


#include "xinput.h"
#include <string.h>

extern void print_classes_xi2(Display*, XIAnyClassInfo **classes,
                              int num_classes);

static Window create_win(Display *dpy)
{
    Window win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy), 0, 0, 200,
            200, 0, 0, WhitePixel(dpy, 0));
    Window subwindow = XCreateSimpleWindow(dpy, win, 50, 50, 50, 50, 0, 0,
            BlackPixel(dpy, 0));

    XMapWindow(dpy, subwindow);
    XSelectInput(dpy, win, ExposureMask);
    return win;
}

static void print_deviceevent(XIDeviceEvent* event)
{
    double *val;
    int i;

    printf("    device: %d (%d)\n", event->deviceid, event->sourceid);
    printf("    detail: %d\n", event->detail);
    printf("    flags: %s\n", (event->flags & XIKeyRepeat) ? "repeat" : "");

    printf("    root: %.2f/%.2f\n", event->root_x, event->root_y);
    printf("    event: %.2f/%.2f\n", event->event_x, event->event_y);

    printf("    buttons:");
    for (i = 0; i < event->buttons.mask_len * 8; i++)
        if (XIMaskIsSet(event->buttons.mask, i))
            printf(" %d", i);
    printf("\n");

    printf("    modifiers: locked %#x latched %#x base %#x effective: %#x\n",
            event->mods.locked, event->mods.latched,
            event->mods.base, event->mods.effective);
    printf("    group: locked %#x latched %#x base %#x effective: %#x\n",
            event->group.locked, event->group.latched,
            event->group.base, event->group.effective);
    printf("    valuators:\n");

    val = event->valuators.values;
    for (i = 0; i < event->valuators.mask_len * 8; i++)
        if (XIMaskIsSet(event->valuators.mask, i))
            printf("        %i: %.2f\n", i, *val++);

    printf("    windows: root 0x%lx event 0x%lx child 0x%ld\n",
            event->root, event->event, event->child);
}

static void print_devicechangedevent(Display *dpy, XIDeviceChangedEvent *event)
{
    printf("    device: %d (%d)\n", event->deviceid, event->sourceid);
    printf("    reason: %s\n", (event->reason == XISlaveSwitch) ? "SlaveSwitch" :
                                "DeviceChanged");
    print_classes_xi2(dpy, event->classes, event->num_classes);
}

static void print_hierarchychangedevent(XIHierarchyEvent *event)
{
    int i;
    printf("    Changes happened: %s %s %s %s %s %s %s %s\n",
            (event->flags & XIMasterAdded) ? "[new master]" : "",
            (event->flags & XIMasterRemoved) ? "[master removed]" : "",
            (event->flags & XISlaveAdded) ? "[new slave]" : "",
            (event->flags & XISlaveRemoved) ? "[slave removed]" : "",
            (event->flags & XISlaveAttached) ? "[slave attached]" : "",
            (event->flags & XISlaveDetached) ? "[slave detached]" : "",
            (event->flags & XIDeviceEnabled) ? "[device enabled]" : "",
            (event->flags & XIDeviceDisabled) ? "[device disabled]" : "");

    for (i = 0; i < event->num_info; i++)
    {
        char *use;
        switch(event->info[i].use)
        {
            case XIMasterPointer: use = "master pointer"; break;
            case XIMasterKeyboard: use = "master keyboard"; break;
            case XISlavePointer: use = "slave pointer"; break;
            case XISlaveKeyboard: use = "slave keyboard"; break;
            case XIFloatingSlave: use = "floating slave"; break;
                break;
        }

        printf("    device %d [%s (%d)] is %s\n",
                event->info[i].deviceid,
                use,
                event->info[i].attachment,
                (event->info[i].enabled) ? "enabled" : "disabled");
        if (event->info[i].flags)
        {
            printf("    changes: %s %s %s %s %s %s %s %s\n",
                    (event->info[i].flags & XIMasterAdded) ? "[new master]" : "",
                    (event->info[i].flags & XIMasterRemoved) ? "[master removed]" : "",
                    (event->info[i].flags & XISlaveAdded) ? "[new slave]" : "",
                    (event->info[i].flags & XISlaveRemoved) ? "[slave removed]" : "",
                    (event->info[i].flags & XISlaveAttached) ? "[slave attached]" : "",
                    (event->info[i].flags & XISlaveDetached) ? "[slave detached]" : "",
                    (event->info[i].flags & XIDeviceEnabled) ? "[device enabled]" : "",
                    (event->info[i].flags & XIDeviceDisabled) ? "[device disabled]" : "");
        }
    }
}

static void print_rawevent(XIRawEvent *event)
{
    int i;
    double *val, *raw_val;

    printf("    device: %d\n", event->deviceid);
    printf("    detail: %d\n", event->detail);
    printf("    valuators:\n");

    val = event->valuators.values;
    raw_val = event->raw_values;
    for (i = 0; i < event->valuators.mask_len * 8; i++)
        if (XIMaskIsSet(event->valuators.mask, i))
            printf("         %2d: %.2f (%.2f)\n", i, *val++, *raw_val++);
    printf("\n");
}

static void print_enterleave(XILeaveEvent* event)
{
    char *mode, *detail;
    int i;

    printf("    device: %d (%d)\n", event->deviceid, event->sourceid);
    printf("    windows: root 0x%lx event 0x%lx child 0x%ld\n",
            event->root, event->event, event->child);
    switch(event->mode)
    {
        case XINotifyNormal:       mode = "NotifyNormal"; break;
        case XINotifyGrab:         mode = "NotifyGrab"; break;
        case XINotifyUngrab:       mode = "NotifyUngrab"; break;
        case XINotifyWhileGrabbed: mode = "NotifyWhileGrabbed"; break;
        case XINotifyPassiveGrab:  mode = "NotifyPassiveGrab"; break;
        case XINotifyPassiveUngrab:mode = "NotifyPassiveUngrab"; break;
    }
    switch (event->detail)
    {
        case XINotifyAncestor: detail = "NotifyAncestor"; break;
        case XINotifyVirtual: detail = "NotifyVirtual"; break;
        case XINotifyInferior: detail = "NotifyInferior"; break;
        case XINotifyNonlinear: detail = "NotifyNonlinear"; break;
        case XINotifyNonlinearVirtual: detail = "NotifyNonlinearVirtual"; break;
        case XINotifyPointer: detail = "NotifyPointer"; break;
        case XINotifyPointerRoot: detail = "NotifyPointerRoot"; break;
        case XINotifyDetailNone: detail = "NotifyDetailNone"; break;
    }
    printf("    mode: %s (detail %s)\n", mode, detail);
    printf("    flags: %s %s\n", event->focus ? "[focus]" : "",
                                 event->same_screen ? "[same screen]" : "");
    printf("    buttons:");
    for (i = 0; i < event->buttons.mask_len * 8; i++)
        if (XIMaskIsSet(event->buttons.mask, i))
            printf(" %d", i);
    printf("\n");

    printf("    modifiers: locked %#x latched %#x base %#x effective: %#x\n",
            event->mods.locked, event->mods.latched,
            event->mods.base, event->mods.effective);
    printf("    group: locked %#x latched %#x base %#x effective: %#x\n",
            event->group.locked, event->group.latched,
            event->group.base, event->group.effective);

    printf("    root x/y:  %.2f / %.2f\n", event->root_x, event->root_y);
    printf("    event x/y: %.2f / %.2f\n", event->event_x, event->event_y);

}

static void print_propertyevent(Display *display, XIPropertyEvent* event)
{
    char *changed;
    char *name;

    if (event->what == XIPropertyDeleted)
        changed = "deleted";
    else if (event->what == XIPropertyCreated)
        changed = "created";
    else
        changed = "modified";
    name = XGetAtomName(display, event->property);
    printf("     property: %ld '%s'\n", event->property, name);
    printf("     changed: %s\n", changed);

    XFree(name);
}
void
test_sync_grab(Display *display, Window win)
{
    int loop = 3;
    int rc;
    XIEventMask mask;

    /* Select for motion events */
    mask.deviceid = XIAllDevices;
    mask.mask_len = 2;
    mask.mask = calloc(2, sizeof(char));
    XISetMask(mask.mask, XI_ButtonPress);

    if ((rc = XIGrabDevice(display, 2,  win, CurrentTime, None, GrabModeSync,
                           GrabModeAsync, False, &mask)) != GrabSuccess)
    {
        fprintf(stderr, "Grab failed with %d\n", rc);
        return;
    }
    free(mask.mask);

    XSync(display, True);
    XIAllowEvents(display, 2, SyncPointer, CurrentTime);
    XFlush(display);

    printf("Holding sync grab for %d button presses.\n", loop);

    while(loop--)
    {
        XIEvent ev;

        XNextEvent(display, (XEvent*)&ev);
        if (ev.type == GenericEvent && ev.extension == xi_opcode )
        {
            XIDeviceEvent *event = (XIDeviceEvent*)&ev;
            print_deviceevent(event);
            XIAllowEvents(display, 2, SyncPointer, CurrentTime);
        }
    }

    XIUngrabDevice(display, 2, CurrentTime);
    printf("Done\n");
}

static const char* type_to_name(int evtype)
{
    const char *name;

    switch(evtype) {
        case XI_DeviceChanged:    name = "DeviceChanged";       break;
        case XI_KeyPress:         name = "KeyPress";            break;
        case XI_KeyRelease:       name = "KeyRelease";          break;
        case XI_ButtonPress:      name = "ButtonPress";         break;
        case XI_ButtonRelease:    name = "ButtonRelease";       break;
        case XI_Motion:           name = "Motion";              break;
        case XI_Enter:            name = "Enter";               break;
        case XI_Leave:            name = "Leave";               break;
        case XI_FocusIn:          name = "FocusIn";             break;
        case XI_FocusOut:         name = "FocusOut";            break;
        case XI_HierarchyChanged: name = "HierarchyChanged";    break;
        case XI_PropertyEvent:    name = "PropertyEvent";       break;
        case XI_RawKeyPress:      name = "RawKeyPress";         break;
        case XI_RawKeyRelease:    name = "RawKeyRelease";       break;
        case XI_RawButtonPress:   name = "RawButtonPress";      break;
        case XI_RawButtonRelease: name = "RawButtonRelease";    break;
        case XI_RawMotion:        name = "RawMotion";           break;
        default:
                                  name = "unknown event type"; break;
    }
    return name;
}


int
test_xi2(Display	*display,
         int	argc,
         char	*argv[],
         char	*name,
         char	*desc)
{
    XIEventMask mask;
    Window win;

    list(display, argc, argv, name, desc);
    win = create_win(display);

    /* Select for motion events */
    mask.deviceid = XIAllDevices;
    mask.mask_len = XIMaskLen(XI_RawMotion);
    mask.mask = calloc(mask.mask_len, sizeof(char));
    XISetMask(mask.mask, XI_ButtonPress);
    XISetMask(mask.mask, XI_ButtonRelease);
    XISetMask(mask.mask, XI_KeyPress);
    XISetMask(mask.mask, XI_KeyRelease);
    XISetMask(mask.mask, XI_Motion);
    XISetMask(mask.mask, XI_DeviceChanged);
    XISetMask(mask.mask, XI_Enter);
    XISetMask(mask.mask, XI_Leave);
    XISetMask(mask.mask, XI_FocusIn);
    XISetMask(mask.mask, XI_FocusOut);
    XISetMask(mask.mask, XI_HierarchyChanged);
    XISetMask(mask.mask, XI_PropertyEvent);
    XISelectEvents(display, win, &mask, 1);
    XMapWindow(display, win);
    XSync(display, False);

    {
        XIGrabModifiers modifiers[] = {{0, 0}, {0, 0x10}, {0, 0x1}, {0, 0x11}};
        int nmods = sizeof(modifiers)/sizeof(modifiers[0]);

        mask.deviceid = 2;
        memset(mask.mask, 0, 2);
        XISetMask(mask.mask, XI_KeyPress);
        XISetMask(mask.mask, XI_KeyRelease);
        XISetMask(mask.mask, XI_ButtonPress);
        XISetMask(mask.mask, XI_ButtonRelease);
        XISetMask(mask.mask, XI_Motion);
        XIGrabButton(display, 2, 1, win, None, GrabModeAsync, GrabModeAsync,
                False, &mask, nmods, modifiers);
        XIGrabKeycode(display, 3, 24 /* q */, win, GrabModeAsync, GrabModeAsync,
                False, &mask, nmods, modifiers);
        XIUngrabButton(display, 3, 1, win, nmods - 2, &modifiers[2]);
        XIUngrabKeycode(display, 3, 24 /* q */, win, nmods - 2, &modifiers[2]);
    }

    mask.deviceid = XIAllMasterDevices;
    memset(mask.mask, 0, 2);
    XISetMask(mask.mask, XI_RawKeyPress);
    XISetMask(mask.mask, XI_RawKeyRelease);
    XISetMask(mask.mask, XI_RawButtonPress);
    XISetMask(mask.mask, XI_RawButtonRelease);
    XISetMask(mask.mask, XI_RawMotion);
    XISelectEvents(display, DefaultRootWindow(display), &mask, 1);

    free(mask.mask);

    {
        XEvent event;
        XMaskEvent(display, ExposureMask, &event);
        XSelectInput(display, win, 0);
    }

    /*
    test_sync_grab(display, win);
    */

    while(1)
    {
        XEvent ev;
        XGenericEventCookie *cookie = (XGenericEventCookie*)&ev.xcookie;
        XNextEvent(display, (XEvent*)&ev);

        if (XGetEventData(display, cookie) &&
            cookie->type == GenericEvent &&
            cookie->extension == xi_opcode)
        {
            printf("EVENT type %d (%s)\n", cookie->evtype, type_to_name(cookie->evtype));
            switch (cookie->evtype)
            {
                case XI_DeviceChanged:
                    print_devicechangedevent(display, cookie->data);
                    break;
                case XI_HierarchyChanged:
                    print_hierarchychangedevent(cookie->data);
                    break;
                case XI_RawKeyPress:
                case XI_RawKeyRelease:
                case XI_RawButtonPress:
                case XI_RawButtonRelease:
                case XI_RawMotion:
                    print_rawevent(cookie->data);
                    break;
                case XI_Enter:
                case XI_Leave:
                case XI_FocusIn:
                case XI_FocusOut:
                    print_enterleave(cookie->data);
                    break;
                case XI_PropertyEvent:
                    print_propertyevent(display, cookie->data);
                    break;
                default:
                    print_deviceevent(cookie->data);
                    break;
            }
        }

        XFreeEventData(display, cookie);
    }

    XDestroyWindow(display, win);

    return EXIT_SUCCESS;
}
