.\" $Xorg: library.ms,v 1.3 2000/08/17 19:42:38 cpqbld Exp $
.\" $XdotOrg: xc/doc/specs/Xi/library.ms,v 1.2 2004/04/23 18:42:18 eich Exp $
.\" Input Extension			now coded to macros.t
.\" edited for DP edits and code consistency w/ core protocol/xlib 4/5/96
.EH ''''
.OH ''''
.EF ''''
.OF ''''
.ps 11
.nr PS 11
\0
.sp 10
.ce 500
.ps 20
\fBX Input Device Extension Library
.ps 12
.sp 2
X Consortium Standard
.sp 1
X Version 11, Release 6.8
.sp 16
.ps 15
\fBMark Patrick\0\0\0\0Ardent Computer
.sp 1
\fBGeorge Sachs\0\0\0\0Hewlett-Packard
.ps 12
.ce 0
.bp
\0
.sp 10
.fi
.LP
Copyright \(co 1989, 1990, 1991 by Hewlett-Packard Company, Ardent Computer. 
.LP
Permission to use, copy, modify, and distribute this documentation for
any purpose and without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.
Ardent, and Hewlett-Packard make no representations about the suitability 
for any purpose of the information in this document.  It is provided \`\`as is''
without express or implied warranty.
.sp 5
Copyright (c) 1989, 1990, 1991, 1992  X Consortium
.LP
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the ``Software''), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
.LP
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
.LP
THE SOFTWARE IS PROVIDED ``AS IS'', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
.LP
Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.
.sp 3
\fIX Window System\fP is a trademark of The Open Group.
.ps
.vs
.bp 1
.EH '\fBX Input Extension Library\fP''\fBX11, Release 6.8\fP'
.OH '\fBX Input Extension Library\fP''\fBX11, Release 6.8\fP'
.EF ''\fB\\\\n(PN\fP''
.OF ''\fB\\\\n(PN\fP''
.\"  Force the heading counter for level 1 to one
.\"
.\"
.\"
.\"  Print table of contents to level 4 headings
.\"
.\"
.\"  Page eject for each level 1 heading
.\"
.\"
.\"  Define Ch to contain the chapter string.
.\"
.ds Ch Input Extension Overview
.\"
.\"
.\"  Pull in the layout macro package.
.\"
.\"
.tr ~
.NH 1
Input Extension Overview
.XS
\*(SN Input Extension Overview
.XE
.LP
This document describes an extension to
the X11 server.  The purpose of this extension is to support the use
of additional input devices beyond the pointer and keyboard devices
defined by the core X protocol.  This first section gives an overview
of the input extension.  The following sections correspond to 
chapters 9, 10, and 11, ``Window and Session Manager Functions'',
``Events'', and ``Event Handling Functions'' of the
``Xlib - C Language Interface'' manual
and describe how to use the input device extension.
.NH 2
Design Approach
.XS
\*(SN Design Approach
.XE
.LP
The design approach of the extension is to define functions
and events analogous to the core functions and events.
This allows extension input devices and events to be individually
distinguishable from each other and from the core input devices and events.
These functions and events make use of a device identifier and support the
reporting of \fIn\fP\^-dimensional motion data as well as other data that
is not currently reportable via the core input events.
.NH 2
Core Input Devices
.XS
\*(SN Core Input Devices
.XE
.LP
The X server core protocol supports two input devices:  a pointer and a
keyboard.  The pointer device has two major functions. 
First, it may be used to generate motion information
that client programs can detect. Second, it may also be used to indicate the
current location and focus of the X keyboard.  To accomplish this, the server 
echoes a cursor at the current position of the X pointer.  Unless the X
keyboard has been explicitly focused, this cursor also shows the current
location and focus of the X keyboard.
.LP
The X keyboard is used to generate input that client programs can detect.
.LP
The X keyboard and X pointer are referred to in this document as 
the \fIcore devices\fP, and the input
events they generate 
.Pn ( KeyPress ,
.PN KeyRelease ,
.PN ButtonPress ,
.PN ButtonRelease ,
and
.PN MotionNotify )
are known as the \fIcore input events\fP.  All other
input devices are referred to as \fIextension input devices\fP, and the 
input events they generate are referred to as \fIextension input events\fP\^.
.NT
This input extension does not change the behavior or functionality of the
core input devices, core events, or core protocol requests, with the
exception of the core grab requests.  These requests may affect the
synchronization of events from extension devices.  See the explanation
in the section titled ``Event Synchronization and Core Grabs.''
.NE
.LP
Selection of the physical devices to be initially used by the server as the 
core devices is left implementation dependent.  Functions are defined that
allow client programs to change which physical devices are used as the
core devices.
.NH 2
Extension Input Devices
.XS
\*(SN Extension Input Devices
.XE
.LP
The input extension controls access to input devices other than the X keyboard
and X pointer.  It allows client programs to select input from these devices 
independently
from each other and independently from the core devices.  Input events from
these devices are of extension types 
.Pn ( DeviceKeyPress ,
.PN DeviceKeyRelease ,
.PN DeviceButtonPress ,
.PN DeviceButtonRelease ,
.PN DeviceMotionNotify ,
and so on) and contain
a device identifier so that events of the same type coming from different
input devices can be distinguished.
.LP
Extension input events are not limited in size by the size of the server
32-byte wire events.  Extension input events
may be constructed by the server sending as many
wire-sized events as necessary to return the information required for
that event.
The library event reformatting routines
are responsible for combining these into one or more client XEvents.
.LP
Any input device that generates key, button, or motion data may be used as
an extension input device.
Extension input devices may have zero or more keys, zero or more buttons,
and may report zero or more axes of motion.  Motion may be reported 
as relative movements from a previous position or as an absolute
position.  All valuators reporting motion information for a given
extension input device must report the same kind of motion information
(absolute or relative).
.LP
This extension is designed to accommodate new types of input devices that
may be added in the future.  The protocol requests that refer to
specific characteristics of input devices organize that information
by \fIinput device classes\fP.  Server implementors may add new
classes of input devices without changing the protocol requests.
.LP
All extension input
devices are treated like the core X keyboard in determining their location
and focus.  The server does not track the location of these devices on an
individual basis and, therefore,
does not echo a cursor to indicate their current location.
Instead, their location is determined by the location of the core X pointer.
Like the core X keyboard, some may be explicitly focused. If they are
not explicitly focused,  their focus
is determined by the location of the core X pointer.
.NH 3
Input Device Classes
.XS
\*(SN Input Device Classes
.XE
.LP
Some of the input extension requests divide input devices into classes
based on their functionality.  This is intended to allow new classes of input
devices to be defined at a later time without changing the semantics of 
these functions.  The following input device classes are currently
defined:
.IP "\s-1KEY\s+1" 1i
The device reports key events.
.IP "\s-1BUTTON\s+1" 1i
The device reports button events.
.IP "\s-1VALUATOR\s+1" 1i
The device reports valuator data in motion events.
.IP "\s-1PROXIMITY\s+1" 1i
The device reports proximity events.
.IP "\s-1FOCUS\s+1" 1i
The device can be focused.
.IP "\s-1FEEDBACK\s+1" 1i
The device supports feedbacks.
.LP
Additional classes may be added in the future.
Functions that support multiple input classes, such as the 
.PN XListInputDevices
function that lists all available input devices,
organize the data they return by input class.  Client programs that
use these functions should not access data unless it matches a 
class defined at the time those clients were compiled.  In this way,
new classes can be added without forcing existing clients that use
these functions to be recompiled.
.NH 2
Using Extension Input Devices
.XS
\*(SN Using Extension Input Devices
.XE
.LP
A client that wishes to access an input device does so through the library
functions defined in the following sections.  A typical sequence of requests
that a client would make is as follows:
.IP \(bu 5
.PN XListInputDevices
\- lists all of the available input devices.  From the 
information returned by this request, determine whether the desired input
device is attached to the server.  For a description of the 
.PN XListInputDevices
request, see the section entitled ``Listing Available Devices.''
.IP \(bu 5
.PN XOpenDevice
\- requests that the server open the device for access by this client.
This request returns an 
.PN XDevice
structure that is used
by most other input extension requests to identify the specified device.
For a description of the 
.PN XOpenDevice
request, see the section entitled ``Enabling and Disabling Extension Devices.''
.IP \(bu 5
Determine the event types and event classes needed to select the desired
input extension events, and identify them when they are received.
This is done via macros whose name corresponds to the desired event, for
example,
.PN DeviceKeyPress .
For a description of these macros,
see the section entitled ``Selecting Extension Device Events.''
.IP \(bu 5
.PN XSelectExtensionEvent
\- selects the desired events from the server.
For a description of the 
.PN XSelextExtensionEvent
request, see the section entitled ``Selecting Extension Device Events.''
.IP \(bu 5
.PN XNextEvent
\- receives the next available event.  This is the core
.PN XNextEvent
function provided by the standard X libarary.
.LP
Other requests are defined to grab and focus extension devices, to
change their key, button, or modifier mappings, to control the
propagation of input extension events, to get motion history from an
extension device, and to send input extension events to another client.
These functions are described in the following sections.
.NH 1
Library Extension Requests
.XS
\*(SN Library Extension Requests
.XE
.LP
Extension input devices are accessed by client programs through the 
use of new protocol requests.
The following requests are provided as extensions to Xlib.  Constants
and structures referenced by these functions may be found in the 
files \fB<X11/extensions/XI.h>\fP and \fB<X11/extensions/XInput.h>\fP,
which are attached to this document as 
Appendix A.
.LP
The library will return \fBNoSuchExtension\fP if an extension request
is made to a server that does not support the input extension.
.LP
Input extension requests cannot be used to access the X keyboard and
X pointer devices.
.NH 2
Window Manager Functions
.XS
\*(SN Window Manager Functions
.XE
.LP
This section discusses the following X Input Extension Window Manager topics:
.IP \(bu 5
Changing the core devices
.IP \(bu 5
Event synchronization and core grabs
.IP \(bu 5
Extension active grabs
.IP \(bu 5
Passively grabbing a key
.IP \(bu 5
Passively grabbing a button
.IP \(bu 5
Thawing a device
.IP \(bu 5
Controlling device focus
.IP \(bu 5
Controlling device feedback
.IP \(bu 5
Ringing a bell on an input device
.IP \(bu 5
Controlling device encoding
.IP \(bu 5
Controlling button mapping
.IP \(bu 5
Obtaining the state of a device
.NH 3
Changing the Core Devices
.XS
\*(SN Changing the Core Devices
.XE
.LP
These functions are provided to change which physical device is used
as the X pointer or X keyboard.
.NT
Using these functions may change the characteristics of the core devices.
The new pointer device may have a different number of buttons from the 
old one, or the new keyboard device may have a different number of
keys or report a different range of keycodes.  Client programs may be
running that depend on those characteristics.  For example, a client
program could allocate an array based on the number of buttons on the
pointer device and then use the button numbers received in button events
as indices into that array.  Changing the core devices could cause
such client programs to behave improperly or to terminate abnormally
if they ignore the 
.PN ChangeDeviceNotify
event generated by these requests.
.NE
.LP
These functions change the X keyboard or X pointer device and generate an
.PN XChangeDeviceNotify
event and a
.PN MappingNotify
event.  
The specified device becomes the
new X keyboard or X pointer device.  The location of the core device
does not change as a result of this request.
.LP
These requests fail and return 
.PN AlreadyGrabbed
if either the specified
device or the core device it would replace are grabbed by some other client.
They fail and return 
.PN GrabFrozen
if either device is frozen by the active grab of another client.
.LP
These requests fail with a
.PN BadDevice
error if the specified device is invalid, has not previously been opened via
.PN XOpenDevice ,
or is
not supported as a core device by the server implementation.
.LP
Once the device has successfully replaced one of the core devices, it
is treated as a core device until it is in turn replaced by another
.PN ChangeDevice
request or until the server terminates.  The termination
of the client that changed the device will not cause it to change back.
Attempts to use the
.PN XCloseDevice
request to close the new core device will fail with a
.PN BadDevice
error.
.sp
.LP
To change which physical device is used as the X keyboard, use the 
.PN XChangeKeyboardDevice
function.
The specified device must support input class 
.PN Keys
(as reported in the
.PN ListInputDevices
request) or the request will fail with a 
.PN BadMatch
error.
.LP
.sM
.FD 0
int XChangeKeyboardDevice\^(\^\fIdisplay\fP\^, \fIdevice\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the desired device.
.LP
.eM
If no error occurs, 
.PN XChangeKeyboardDevice
returns 
.PN Success .
A 
.PN ChangeDeviceNotify
event with the request field set to 
.PN NewKeyboard
is sent to all clients selecting that event.
A 
.PN MappingNotify
event with the request field set to
.PN MappingKeyboard
is sent to all clients.
The requested device becomes the X keyboard, and the old keyboard becomes 
available as an extension input device.
The focus state of the new keyboard is the same as
the focus state of the old X keyboard.
.LP
.PN XChangeKeyboardDevice
can generate
.PN AlreadyGrabbed ,
.PN BadDevice ,
.PN BadMatch ,
and 
.PN GrabFrozen
errors.
.sp
.LP
To change which physical device is used as the X pointer,
use the
.PN XChangePointerDevice
function.
The specified device must support input class 
.PN Valuators
(as reported in the
.PN XListInputDevices
request) and report at least two axes of motion,
or the request will fail with a 
.PN BadMatch
error.
If the specified device reports more than two axes, the two specified in
the xaxis and yaxis arguments will be used.  Data from other
valuators on the device will be ignored.
.LP
If the specified device reports absolute positional information, and the 
server implementation does not allow such a device to be used as the 
X pointer, the request will fail with a 
.PN BadDevice
error.
.sM
.FD 0
int XChangePointerDevice\^(\^\fIdisplay\fP\^, \fIdevice\fP\^, \fIxaxis\fP\^, \fIyaxis\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.br
     int \fIxaxis\fP\^;
.br
     int \fIyaxis\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the desired device.
.IP \fIxaxis\fP 1i
Specifies the zero-based index of the axis to be used as the x-axis of the 
pointer device.
.IP \fIyaxis\fP 1i
Specifies the zero-based index of the axis to be used as the y-axis of the 
pointer device.
.LP
.eM
If no error occurs, 
.PN XChangePointerDevice
returns 
.PN Success .
A 
.PN ChangeDeviceNotify
event with the request field set to 
.PN NewPointer
is sent to all clients selecting that event.
A
.PN MappingNotify
event with the request field set to
.PN MappingPointer
is sent to all clients.
The requested device becomes the X pointer, and the old pointer becomes 
available as an extension input device.
.LP
.PN XChangePointerDevice
can generate
.PN AlreadyGrabbed ,
.PN BadDevice ,
.PN BadMatch ,
and 
.PN GrabFrozen
errors.
.NH 3
Event Synchronization and Core Grabs
.XS
\*(SN Event Synchronization and Core Grabs
.XE
.LP
Implementation of the input extension requires an extension of the
meaning of event synchronization for the core grab requests.  This is
necessary in order to allow window managers to freeze all input devices
with a single request.
.LP
The core grab requests require a pointer_mode and keyboard_mode
argument.  The meaning of these modes is changed by the input extension.
For the
.PN XGrabPointer
and 
.PN XGrabButton
requests, pointer_mode controls synchronization of the pointer device, 
and keyboard_mode controls the synchronization of all other input devices.  
For the 
.PN XGrabKeyboard
and
.PN XGrabKey
requests, pointer_mode controls the synchronization
of all input devices, except the X keyboard, while keyboard_mode controls
the synchronization of the keyboard.  When using one of the core grab
requests, the synchronization of extension devices
is controlled by the mode specified for the device not being grabbed.
.NH 3
Extension Active Grabs
.XS
\*(SN Extension Active Grabs
.XE
.LP
Active grabs of
extension devices are supported via the 
.PN XGrabDevice
function in the same way that core devices are grabbed using the core 
.PN XGrabKeyboard
function, except that an extension input device
is passed as a function parameter.  
The 
.PN XUngrabDevice
function allows a previous active grab for an extension device to be released.
.LP
Passive grabs of buttons and keys on extension devices are supported
via the 
.PN XGrabDeviceButton
and 
.PN XGrabDeviceKey
functions.
These passive grabs are released via the
.PN XUngrabDeviceKey
and
.PN XUngrabDeviceButton
functions.
.sp
.LP
To grab an extension device, use the
.PN XGrabDevice
function.
The device must have previously been opened using the
.PN XOpenDevice
function.
.sM
.FD 0
int XGrabDevice\^(\^\fIdisplay\fP\^, \fIdevice\fP\^, \fIgrab_window\fP\^, \fIowner_events\fP\^, \fIevent_count\fP\^, \fIevent_list\fP\^, 
.br
               \fIthis_device_mode\fP\^, \fIother_device_mode\fP\^, \fItime\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.br
     Window \fIgrab_window\fP\^;
.br
     Bool \fIowner_events\fP\^;
.br
     int \fIevent_count\fP\^;
.br
     XEventClass *\fIevent_list\fP\^;
.br
     int \fIthis_device_mode\fP\^;
.br
     int \fIother_device_mode\fP\^;
.br
     Time \fItime\fP\^;
.FN
.IP "\fIdisplay\fP" 1i
Specifies the connection to the X server.
.IP "\fIdevice\fP" 1i
Specifies the desired device.
.IP "\fIgrab_window\fP" 1i
Specifies the ID of a window associated with the device specified above.
.IP "\fIowner_events\fP" 1i
Specifies a boolean value of either 
.PN True
or 
.PN False .
.IP "\fIevent_count\fP" 1i
Specifies the number of elements in the event_list array.
.IP "\fIevent_list\fP" 1i
Specifies a pointer to a list of event classes that indicate which events
the client wishes to receive.  
These event classes must have been obtained
using the device being grabbed.
.IP "\fIthis_device_mode\fP" 1i
Controls further processing of events from this device.  You can pass one
of these constants: 
.PN GrabModeSync
or 
.PN GrabModeAsync .
.IP "\fIother_device_mode\fP" 1i
Controls further processing of events from all other devices.  You can pass one
of these constants:
.PN GrabModeSync
or 
.PN GrabModeAsync .
.IP "\fItime\fP" 1i
Specifies the time.  This may be either a timestamp expressed in
milliseconds or
.PN CurrentTime .
.LP
.eM
.PN XGrabDevice
actively grabs an extension input device and generates 
.PN DeviceFocusIn
and 
.PN DeviceFocusOut
events.
Further input events from this device are reported only to the grabbing client.
This function overrides any previous active grab by this client for this device.
.LP
The event_list parameter is a pointer to a list of event classes.  This list
indicates which events the client wishes to receive while the grab is active.
If owner_events is 
.PN False ,
input events from this device are reported with respect to 
grab_window and are reported only if specified in event_list.
If owner_events is
.PN True ,
then if a generated event would normally be reported to this client,
it is reported normally.
Otherwise, the event is reported with respect to the grab_window and is only
reported if specified in event_list.
.LP
The this_device_mode argument controls the further processing 
of events from this device, and the other_device_mode argument controls
the further processing of input events from all other devices.
.IP \(bu 5
If the this_device_mode argument is 
.PN GrabModeAsync ,
device event processing continues
normally; if the device is currently frozen by this client, then
processing of device events is resumed.
If the this_device_mode  argument is
.PN GrabModeSync ,
the state of the grabbed device
(as seen by client applications) appears to freeze,
and no further device events are generated by the server until the
grabbing client issues a releasing 
.PN XAllowDeviceEvents
call or until the device grab is released.
Actual device input events are not lost while the device is frozen; they are
simply queued for later processing.
.IP \(bu 5
If the other_device_mode is 
.PN GrabModeAsync ,
event processing from other input devices is unaffected
by activation of the grab.  
If other_device_mode is 
.PN GrabModeSync ,
the state of all devices except the grabbed device
(as seen by client applications) appears to freeze, and no further
events are generated by the server until the grabbing client issues a
releasing 
.PN XAllowEvents
or 
.PN XAllowDeviceEvents
call or until the device grab is released.
Actual events are not lost
while the other devices are frozen; they are simply queued for later
processing.
.LP
.PN XGrabDevice
fails on the following conditions:
.IP \(bu 5
If the device is actively grabbed by some other client, it returns
.PN AlreadyGrabbed .
.IP \(bu 5
If grab_window is not viewable, it returns
.PN GrabNotViewable .
.IP \(bu 5
If the specified time is earlier
than the last-grab-time for the specified device
or later than the current X server time, it returns
.PN GrabInvalidTime .
Otherwise,
the last-grab-time for the specified device is set
to the specified time and 
.PN CurrentTime
is replaced by the current X server time.
.IP \(bu 5
If the device is frozen by an active grab of another client, it returns
.PN GrabFrozen .
.LP
If a grabbed device is closed by a client while an active grab by that 
client is in effect, that active grab will be released. 
Any passive grabs established by that client will be released.
If the device is frozen only by an active grab
of the requesting client, it is thawed.
.LP
.PN XGrabDevice
can generate
.PN BadClass ,
.PN BadDevice ,
.PN BadValue ,
and
.PN BadWindow
errors.
.sp
.LP
To release a grab of an extension device, use the
.PN XUngrabDevice
function.
.LP
.sM
.FD 0
int XUngrabDevice(\fIdisplay\fP\^, \fIdevice\fP\^, \fItime\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.br
     Time \fItime\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the desired device.
.IP \fItime\fP 1i
Specifies the time.  This may be either a timestamp expressed in
milliseconds, or 
.PN CurrentTime .
.LP
.eM
.PN XUngrabDevice
allows a client to release an extension input device and any
queued events if this client has it grabbed from either
.PN XGrabDevice
or
.PN XGrabDeviceKey .
If any other devices are frozen by the grab,
.PN XUngrabDevice
thaws them. 
This function does not release the device and any
queued events if the specified time is earlier than the last-device-grab
time or is later than the current X server time.  It also generates 
.PN DeviceFocusIn
and 
.PN DeviceFocusOut
events.  The X server automatically performs an
.PN XUngrabDevice
if the event window for an active device grab becomes not viewable
or if the client terminates without releasing the grab.
.LP
.PN XUngrabDevice
can generate
.PN BadDevice
errors.
.NH 3
Passively Grabbing a Key
.XS
\*(SN Passively Grabbing a Key
.XE
.LP
To passively grab a single key on an extension device, use
.PN XGrabDeviceKey .
That device must have previously been opened using the
.PN XOpenDevice
function, or the request will fail with a
.PN BadDevice
error.  
If the specified device does not support input class
.PN Keys ,
the request will fail with a
.PN BadMatch
error.
.sM
.FD 0
int XGrabDeviceKey(\fIdisplay\fP\^, \fIdevice\fP\^, \fIkeycode\fP\^, \
\fImodifiers\fP\^, \fImodifier_device\fP\^, \fIgrab_window\fP\^, 
.br
	      \fIowner_events\fP\^, \fIevent_count\fP\^, \fIevent_list\fP\^, \
\fIthis_device_mode\fP\^, \fIother_device_mode\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.br
     int \fIkeycode\fP\^;
.br
     unsigned int \fImodifiers\fP\^;
.br
     XDevice *\fImodifier_device\fP\^;
.br
     Window \fIgrab_window\fP\^;
.br
     Bool \fIowner_events\fP\^;
.br
     int \fIevent_count\fP\^;
.br
     XEventClass *\fIevent_list\fP\^;
.br
     int \fIthis_device_mode\fP\^;
.br
     int \fIother_device_mode\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the desired device.
.IP \fIkeycode\fP 1i
Specifies the keycode of the key that is to be grabbed.  You can pass
either the keycode or 
.PN AnyKey .
.IP \fImodifiers\fP 1i
Specifies the set of keymasks.  This mask is the bitwise inclusive OR
of these keymask bits:  
.PN ShiftMask ,
.PN LockMask ,
.PN ControlMask ,
.PN Mod1Mask ,
.PN Mod2Mask ,
.PN Mod3Mask ,
.PN Mod4Mask ,
and
.PN Mod5Mask .
.IP
You can also pass 
.PN AnyModifier ,
which is equivalent to issuing the grab key request 
for all possible modifier combinations (including the combination
of no modifiers).
.IP \fImodifier_device\fP 1i
Specifies the device whose modifiers are to be used.  If NULL is 
specified, the core X keyboard is used as the modifier_device.
.IP \fIgrab_window\fP 1i
Specifies the ID of a window associated with the device specified above.
.IP \fIowner_events\fP 1i
Specifies a boolean value of either
.PN True
or
.PN False .
.IP \fIevent_count\fP 1i
Specifies the number of elements in the event_list array.
.IP \fIevent_list\fP 1i
Specifies a pointer to a list of event classes that indicate which events
the client wishes to receive.
.IP \fIthis_device_mode\fP 1i
Controls further processing of events from this device.
You can pass one of these constants:
.PN GrabModeSync
or
.PN GrabModeAsync .
.IP \fIother_device_mode\fP 1i
Controls further processing of events from all other devices.
You can pass one of these constants: 
.PN GrabModeSync
or
.PN GrabModeAsync .
.LP
.eM
.PN XGrabDeviceKey
is analogous to the core 
.PN XGrabKey
function.  It creates an
explicit passive grab for a key on an extension device.
The 
.PN XGrabDeviceKey
function establishes a passive grab on a device.
Consequently, in the future, 
.IP \(bu 5
IF the device is not grabbed and the specified key, 
which itself can be a modifier key, is logically pressed
when the specified modifier keys logically are down on the specified
modifier device (and no other keys are down),
.IP \(bu 5
AND no other modifier keys logically are down,
.IP \(bu 5
AND EITHER the grab window is an ancestor of (or is) the focus window
or the grab window is a descendent of the focus window and contains the pointer,
.IP \(bu 5
AND a passive grab on the same device and key combination does not exist on any
ancestor of the grab window,
.IP \(bu 5
THEN the device is actively grabbed, as for
.PN XGrabDevice ,
the last-device-grab time is set to the time at which the key was pressed
(as transmitted in the 
.PN DeviceKeyPress
event), and the 
.PN DeviceKeyPress
event is reported.
.LP
The interpretation of the remaining arguments is as for
.PN XGrabDevice .
The active grab is terminated automatically when the logical state of the
device has the specified key released
(independent of the logical state of the modifier keys).
.LP
Note that the logical state of a device (as seen by means of the X protocol)
may lag the physical state if device event processing is frozen.
.LP
A modifier of 
.PN AnyModifier
is equivalent to issuing the request for all
possible modifier combinations (including the combination of no modifiers).  
It is not required that all modifiers specified have
currently assigned keycodes.
A key of 
.PN AnyKey
is equivalent to issuing
the request for all possible keycodes.  Otherwise, the key must be in
the range specified by min_keycode and max_keycode in the 
information returned by the
.PN XListInputDevices
function.
If it is not within that range, 
.PN XGrabDeviceKey
generates a
.PN BadValue
error.
.LP
.PN XGrabDeviceKey
generates a
.PN BadAccess
error if some other client has issued a 
.PN XGrabDeviceKey
with the same device and key combination on the same window.
When using
.PN AnyModifier
or
.PN AnyKey ,
the request fails completely and the X server generates a
.PN BadAccess
error, and no grabs are established if there is a conflicting grab
for any combination.
.LP
.PN XGrabDeviceKey
returns
.PN Success
upon successful completion of the request.
.LP
.PN XGrabDeviceKey
can generate
.PN BadAccess ,
.PN BadClass ,
.PN BadDevice ,
.PN BadMatch ,
.PN BadValue ,
and
.PN BadWindow
errors.
.sp
.LP
To release a passive grab of a single key on an extension device, use
.PN XUngrabDeviceKey .
.sM
.FD 0
int XUngrabDeviceKey(\fIdisplay\fP\^, \fIdevice\fP\^, \fIkeycode\fP\^, \fImodifiers\fP\^, \fImodifier_device\fP\^, \fIungrab_window\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.br
     int \fIkeycode\fP\^;
.br
     unsigned int \fImodifiers\fP\^;
.br
     XDevice *\fImodifier_device\fP\^;
.br
     Window \fIungrab_window\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the desired device.
.IP \fIkeycode\fP 1i
Specifies the keycode of the key that is to be ungrabbed.  You can pass
either the keycode or
.PN AnyKey .
.IP \fImodifiers\fP 1i
Specifies the set of keymasks.  This mask is the bitwise inclusive OR
of these keymask bits:
.PN ShiftMask ,
.PN LockMask ,
.PN ControlMask , 
.PN Mod1Mask ,
.PN Mod2Mask ,
.PN Mod3Mask ,
.PN Mod4Mask ,
and
.PN Mod5Mask .
.IP 
You can also pass
.PN AnyModifier ,
which is equivalent to issuing the ungrab key 
request for all possible modifier combinations (including the combination
of no modifiers).
.IP \fImodifier_device\fP 1.5i
Specifies the device whose modifiers are to be used.  If NULL is 
specified, the core X keyboard is used as the modifier_device.
.IP \fIungrab_window\fP 1.5i
Specifies the ID of a window associated with the device specified above.
.LP
.eM
.PN XUngrabDeviceKey
is analogous to the core
.PN XUngrabKey
function.  It releases an explicit passive grab for a key 
on an extension input device.
.LP
.PN XUngrabDeviceKey
can generate
.PN BadAlloc ,
.PN BadDevice ,
.PN BadMatch ,
.PN BadValue ,
and 
.PN BadWindow
errors.
.NH 3
Passively Grabbing a Button
.XS
\*(SN Passively Grabbing a Button
.XE
.LP
To establish a passive grab for a single button on an extension device, use
.PN XGrabDeviceButton .
The specified device must have previously been opened using the
.PN XOpenDevice
function, or the request will fail with a 
.PN BadDevice
error.  If the specified device does not support input class
.PN Buttons ,
the request will fail with a 
.PN BadMatch
error.
.sM
.FD 0
int XGrabDeviceButton(\fIdisplay\fP\^, \fIdevice\fP\^, \fIbutton\fP\^, \fImodifiers\fP\^, \fImodifier_device\fP \^, \fIgrab_window\fP\^,
.br
                 \fIowner_events\fP\^, \fIevent_count\fP\^, \fIevent_list\fP\^, \
\fIthis_device_mode\fP\^, \fIother_device_mode\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.br
     unsigned int \fIbutton\fP\^;
.br
     unsigned int \fImodifiers\fP\^;
.br
     XDevice *\fImodifier_device\fP \^;
.br
     Window \fIgrab_window\fP\^;
.br
     Bool \fIowner_events\fP\^;
.br
     int \fIevent_count\fP\^;
.br
     XEventClass *\fIevent_list\fP\^;
.br
     int \fIthis_device_mode\fP\^;
.br
     int \fIother_device_mode\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the desired device.
.IP \fIbutton\fP 1i
Specifies the code of the button that is to be grabbed.  You can pass
either the button or
.PN AnyButton .
.IP \fImodifiers\fP 1i
Specifies the set of keymasks.  This mask is the bitwise inclusive OR
of these keymask bits:
.PN ShiftMask ,
.PN LockMask ,
.PN ControlMask , 
.PN Mod1Mask ,
.PN Mod2Mask ,
.PN Mod3Mask ,
.PN Mod4Mask ,
and
.PN Mod5Mask .
.IP
You can also pass 
.PN AnyModifier ,
which is equivalent to issuing the grab request 
for all possible modifier combinations (including the combination
of no modifiers).
.IP \fImodifier_device\fP 1i
Specifies the device whose modifiers are to be used.  If NULL is 
specified, the core X keyboard is used as the modifier_device.
.IP \fIgrab_window\fP 1i
Specifies the ID of a window associated with the device specified above.
.IP \fIowner_events\fP 1i
Specifies a boolean value of either
.PN True
or
.PN False .
.IP \fIevent_count\fP 1i
Specifies the number of elements in the event_list array.
.IP \fIevent_list\fP 1i
Specifies a list of event classes that indicates which device events are to be 
reported to the client.
.IP \fIthis_device_mode\fP 1i
Controls further processing of events from this device.  You can pass one
of these constants:
.PN GrabModeSync
or
.PN GrabModeAsync .
.IP \fIother_device_mode\fP 1i
Controls further processing of events from all other devices.  You can pass one
of these constants:
.PN GrabModeSync
or
.PN GrabModeAsync .
.LP
.eM
.PN XGrabDeviceButton 
is analogous to the core
.PN XGrabButton
function.  
It creates an explicit passive grab for a button on an extension input device.
Because the server does not track extension devices,
no cursor is specified with this request.
For the same reason, there is no confine_to parameter.
The device must have previously been opened using the
.PN XOpenDevice
function.
.LP
The
.PN XGrabDeviceButton
function establishes a passive grab on a device.
Consequently, in the future, 
.IP \(bu 5
IF the device is not grabbed and the specified button is logically pressed
when the specified modifier keys logically are down 
(and no other buttons or modifier keys are down),
.IP \(bu 5
AND EITHER the grab window is an ancestor of (or is) the focus window
OR the grab window is a descendent of the focus window and contains the pointer,
.IP \(bu 5
AND a passive grab on the same device and button/key combination does not 
exist on any ancestor of the grab window,
.IP \(bu 5
THEN the device is actively grabbed, as for
.PN XGrabDevice ,
the last-grab time is set to the time at which the button was pressed
(as transmitted in the
.PN DeviceButtonPress
event), and the 
.PN DeviceButtonPress
event is reported.
.LP
The interpretation of the remaining arguments is as for 
.PN XGrabDevice .
The active grab is terminated automatically when logical state of the
device has all buttons released (independent of the logical state of 
the modifier keys).
.LP
Note that the logical state of a device (as seen by means of the X protocol)
may lag the physical state if device event processing is frozen.
.LP
A modifier of
.PN AnyModifier
is equivalent to issuing the request for all
possible modifier combinations (including the combination of no
modifiers).  
It is not required that all modifiers specified have
currently assigned keycodes.
A button of
.PN AnyButton
is equivalent to issuing
the request for all possible buttons.
Otherwise, it is not required that the 
specified button be assigned to a physical button.
.LP
.PN XGrabDeviceButton
generates a
.PN BadAccess
error if some other client has issued a 
.PN XGrabDeviceButton
with the same device and button combination on the same window.  
When using
.PN AnyModifier
or
.PN AnyButton ,
the request fails completely and the X server generates a
.PN BadAccess
error and no grabs are
established if there is a conflicting grab for any combination.
.LP
.PN XGrabDeviceButton
can generate
.PN BadAccess , 
.PN BadClass ,
.PN BadDevice ,
.PN BadMatch ,
.PN BadValue ,
and
.PN BadWindow
errors.
.sp
.LP
To release a passive grab of a button on an extension device, use 
.PN XUngrabDeviceButton .
.sM
.FD 0
int XUngrabDeviceButton(\fIdisplay\fP\^, \fIdevice\fP\^, \fIbutton\fP\^, \fImodifiers\fP\^, \fImodifier_device\fP\^, \fIungrab_window\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.br
     unsigned int \fIbutton\fP\^;
.br
     unsigned int \fImodifiers\fP\^;
.br
     XDevice *\fImodifier_device\fP\^;
.br
     Window \fIungrab_window\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the desired device.
.IP \fIbutton\fP 1i
Specifies the code of the button that is to be ungrabbed.  You can pass
either a button or
.PN AnyButton .
.IP \fImodifiers\fP 1i
Specifies the set of keymasks.  This mask is the bitwise inclusive OR
of these keymask bits:
.PN ShiftMask ,
.PN LockMask ,
.PN ControlMask , 
.PN Mod1Mask ,
.PN Mod2Mask ,
.PN Mod3Mask ,
.PN Mod4Mask ,
and
.PN Mod5Mask .
.IP
You can also pass
.PN AnyModifier ,
which is equivalent to issuing the ungrab key 
request for all possible modifier combinations (including the combination
of no modifiers).
.IP \fImodifier_device\fP 1i
Specifies the device whose modifiers are to be used.  If NULL is 
specified, the core X keyboard is used as the modifier_device.
.IP \fIungrab_window\fP 1i
Specifies the ID of a window associated with the device specified above.
.LP
.eM
.PN XUngrabDeviceButton 
is analogous to the core
.PN XUngrabButton
function.  It releases an explicit passive grab for a button
on an extension device.
That device must have previously been opened using the
.PN XOpenDevice
function, or a
.PN BadDevice
error will result.
.LP
A modifier of
.PN AnyModifier
is equivalent to issuing the request for all
possible modifier combinations (including the combination of no
modifiers).  
.LP
.PN XUngrabDeviceButton 
can generate
.PN BadAlloc , 
.PN BadDevice ,
.PN BadMatch ,
.PN BadValue ,
and
.PN BadWindow
errors.
.NH 3
Thawing a Device
.XS
\*(SN Thawing a Device
.XE
.LP
To allow further events to be processed when a device has been frozen, use
.PN XAllowDeviceEvents .
.sM
.FD 0
int XAllowDeviceEvents(\fIdisplay\fP\^, \fIdevice\fP\^, \fIevent_mode\fP\^, \fItime\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.br
     int \fIevent_mode\fP\^;
.br
     Time \fItime\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the desired device.
.IP \fIevent_mode\fP 1i
Specifies the event mode.  You can pass one of these constants:
.PN AsyncThisDevice ,
.PN SyncThisDevice ,
.PN AsyncOtherDevices ,
.PN ReplayThisDevice ,
.PN AsyncAll ,
or
.PN SyncAll .
.IP \fItime\fP 1i
Specifies the time.  This may be either a timestamp expressed in
milliseconds, or
.PN CurrentTime .
.LP
.eM
.PN XAllowDeviceEvents
releases some queued events if the client has caused a device to freeze.
It has no effect if the specified time is earlier than the last-grab 
time of the most recent active grab for the client and device, 
or if the specified time is later than the current X server time.
The following describes the processing that occurs depending on what constant
you pass to the event_mode argument:
.IP \(bu 5
.PN AsyncThisDevice
.IP
If the specified device is frozen by the client, event processing for that 
continues as usual.  If the device is frozen multiple times by the client on 
behalf of multiple separate grabs, 
.PN AsyncThisDevice
thaws for all.
.PN AsyncThisDevice
has no effect if the specified device is not frozen by the 
client, but the device need not be grabbed by the client.
.IP \(bu 5
.PN SyncThisDevice
.IP
If the specified device is frozen and actively grabbed by the client,
event processing for that device continues normally until the next 
key or button event is reported to the client.
At this time, 
the specified device again appears to freeze.
However, if the reported event causes the grab to be released,
the specified device does not freeze.
.PN SyncThisDevice
has no effect if the specified device is not frozen by the client
or is not grabbed by the client.
.IP \(bu 5
.PN ReplayThisDevice
.IP
If the specified device is actively grabbed by the client
and is frozen as the result of an event having been sent to the client
(either from the activation of a 
.PN GrabDeviceButton
or from a previous
.PN AllowDeviceEvents
with mode 
.PN SyncThisDevice , 
but not from a
.PN Grab ),
the grab is released and that event is completely reprocessed.
This time, however, the request ignores any passive grabs at or above 
(toward the root) the grab-window of the grab just released.
The request has no effect if the specified device is not grabbed by the client
or if it is not frozen as the result of an event.
.IP \(bu 5
.PN AsyncOtherDevices
.IP
If the remaining devices are frozen by the client,
event processing for them continues as usual.
If the other devices are frozen multiple times  by the client on behalf of 
multiple separate grabs,
.PN AsyncOtherDevices
``thaws'' for all.
.PN AsyncOtherDevices
has no effect if the devices are not frozen by the client,
but those devices need not be grabbed by the client.
.IP \(bu 5 
.PN SyncAll
.IP
If all devices are frozen by the client,
event processing (for all devices) continues normally until the next
button or key event is reported
to the client for a grabbed device,
at which time the devices again appear to
freeze.  However, if the reported event causes the grab to be released,
then the devices do not freeze (but if any device is still
grabbed, then a subsequent event for it will still cause all devices
to freeze).  
.PN SyncAll
has no effect unless all devices are frozen by the client.
If any device is frozen twice
by the client on behalf of two separate grabs, 
.PN SyncAll
"thaws" for both (but a subsequent freeze for 
.PN SyncAll
will freeze each device only once).
.IP \(bu 5
.PN AsyncAll
.IP
If all devices are frozen by the
client, event processing (for all devices) continues normally.
If any device is frozen multiple times by the client on behalf of multiple
separate grabs, 
.PN AsyncAll
``thaws ''for all.
If any device is frozen twice by the client on behalf of two separate grabs,
.PN AsyncAll
``thaws'' for both.
.PN AsyncAll
has no effect unless all devices are frozen by the client.
.LP
.PN AsyncThisDevice ,
.PN SyncThisDevice ,
and
.PN ReplayThisDevice 
have no effect on the processing of events from the remaining devices.
.PN AsyncOtherDevices
has no effect on the processing of events from the specified device.
When the event_mode is
.PN SyncAll
or
.PN AsyncAll ,
the device parameter is ignored.
.LP
It is possible for several grabs of different devices (by the same 
or different clients) to be active simultaneously.
If a device is frozen on behalf of any grab,
no event processing is performed for the device.
It is possible for a single device to be frozen because of several grabs.
In this case,
the freeze must be released on behalf of each grab before events can 
again be processed.
.LP
.PN XAllowDeviceEvents
can generate
.PN BadDevice
and 
.PN BadValue
errors.
.NH 3
Controlling Device Focus
.XS
\*(SN Controlling Device Focus
.XE
.LP
The current focus window for an extension input device can be 
determined using the
.PN XGetDeviceFocus
function.
Extension devices are focused using the
.PN XSetDeviceFocus
function in the same way that the keyboard is focused using the core
.PN XSetInputFocus
function, except that a device ID is passed as
a function parameter.  One additional focus state,
.PN FollowKeyboard ,
is provided for extension devices.
.LP
To get the current focus state, revert state,
and focus time of an extension device, use
.PN XGetDeviceFocus .
.sM
.FD 0
int XGetDeviceFocus(\fIdisplay\fP\^, \fIdevice\fP\^, \fIfocus_return\fP\^, \
\fIrevert_to_return\fP\^, \fIfocus_time_return\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.br
     Window *\fIfocus_return\fP\^;
.br
     int *\fIrevert_to_return\fP\^;
.br
     Time *\fIfocus_time_return\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the desired device.
.IP \fIfocus_return\fP 1i
Specifies the address of a variable into which the server can return the ID of
the window that contains the device focus or one of the constants
.PN None ,
.PN PointerRoot ,
or
.PN FollowKeyboard .  
.IP \fIrevert_to_return\fP 1i
Specifies the address of a variable into which the server can
return the current revert_to status for the device.
.IP \fIfocus_time_return\fP 1i
Specifies the address of a variable into which the server can
return the focus time last set for the device.
.LP
.eM
.PN XGetDeviceFocus
returns the focus state, the revert-to state,
and the last-focus-time for an extension input device.
.LP
.PN XGetDeviceFocus
can generate
.PN BadDevice
and
.PN BadMatch
errors.
.sp 
.LP
To set the focus of an extension device, use
.PN XSetDeviceFocus .
.sM
.FD 0
int XSetDeviceFocus(\fIdisplay\fP\^, \fIdevice\fP\^, \fIfocus\fP\^, \
\fIrevert_to\fP\^, \fItime\fP\^)
.br
    Display *\fIdisplay\fP\^;
.br
    XDevice *\fIdevice\fP\^;
.br
    Window \fIfocus\fP\^;
.br
    int \fIrevert_to\fP\^;
.br
    Time \fItime\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the desired device.
.IP \fIfocus\fP 1i
Specifies the ID of the window to which the device's focus should be set.
This may be a window ID, or
.PN PointerRoot ,
.PN FollowKeyboard ,
or
.PN None .
.IP \fIrevert_to\fP 1i
Specifies to which window the focus of the device should revert
if the focus window becomes not viewable.  One of the following
constants may be passed:
.PN RevertToParent ,
.PN RevertToPointerRoot ,
.PN RevertToNone ,
or
.PN RevertToFollowKeyboard .
.IP \fItime\fP 1i
Specifies the time.  You can pass either a timestamp, expressed in
milliseconds, or
.PN CurrentTime .
.LP
.eM
.PN XSetDeviceFocus
changes the focus for an extension input device and the 
last-focus-change-time.  It has no effect if the specified 
time is earlier than the last-focus-change-time or is later than the
current X server time.  Otherwise, the last-focus-change-time is set to the
specified time.
This function causes the X server to generate
.PN DeviceFocusIn
and 
.PN DeviceFocusOut
events.
.LP
The action taken by the server when this function is requested depends
on the value of the focus argument:
.IP \(bu 5
If the focus argument is
.PN None ,
all input events from this device will be discarded until a new focus window
is set.  In this case, the revert_to argument is ignored.
.IP \(bu 5
If the focus argument is a window ID, it becomes the focus
window of the device.  If an input event from the device would normally
be reported to this window or to one of its inferiors, the event is 
reported normally.  Otherwise, the event is reported relative to the focus 
window.
.IP \(bu 5
If the focus argument is
.PN PointerRoot ,
the focus window is dynamically taken to be the root window 
of whatever screen the pointer is on at each input event.
In this case, the revert_to argument is ignored.
.IP \(bu 5
If the focus argument is
.PN FollowKeyboard ,   
the focus window is dynamically taken to be the same as the focus
of the X keyboard at each input event.
.LP
The specified focus window must be viewable at the time
.PN XSetDeviceFocus
is called.  Otherwise, it generates a
.PN BadMatch
error.
If the focus window later becomes not viewable,
the X server evaluates the revert_to argument
to determine the new focus window.
.IP \(bu 5
If the revert_to argument is
.PN RevertToParent ,
the focus reverts to the parent (or the closest viewable ancestor),
and the new revert_to value is taken to be
.PN RevertToNone .  
.IP \(bu 5
If the revert_to argument is
.PN RevertToPointerRoot ,
.PN RevertToFollowKeyboard ,
or
.PN RevertToNone ,
the focus reverts to that value.
.LP
When the focus reverts,
the X server generates
.PN DeviceFocusIn
and
.PN DeviceFocusOut
events, but the last-focus-change time is not affected.
.LP
.PN XSetDeviceFocus
can generate
.PN BadDevice ,
.PN BadMatch , 
.PN BadValue ,
and
.PN BadWindow
errors.
.NH 3
Controlling Device Feedback
.XS
\*(SN Controlling Device Feedback
.XE
.LP
To determine the current feedback settings of an extension input device, use
.PN XGetFeedbackControl .
.sM
.FD 0
XFeedbackState * XGetFeedbackControl(\fIdisplay\fP\^, \fIdevice\fP\^, \fInum_feedbacks_return\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.br
     int *\fInum_feedbacks_return\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the desired device.
.IP \fInum_feedbacks_return\fP 1i
Returns the number of feedbacks supported by the device.
.LP
.eM
.PN XGetFeedbackControl
returns a list of
.PN FeedbackState
structures that describe the feedbacks supported by the specified device.
There is an
.PN XFeedbackState
structure for each class of feedback.  These are of 
variable length, but the first three members are common to all.
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	XID class;
	int length;
	XID id;
} XFeedbackState;
.De
.LP
.eM
The common members are as follows:
.IP \(bu 5
The class member identifies the class of feedback.
It may be compared to constants defined in the file
.Pn < X11/extensions/XI.h >.
Currently defined feedback constants include:
.PN KbdFeedbackClass , 
.PN PtrFeedbackClass ,
.PN StringFeedbackClass ,
.PN IntegerFeedbackClass , 
.PN LedFeedbackClass ,
and
.PN BellFeedbackClass .
.IP \(bu 5
The length member specifies the length of the
.PN FeedbackState
structure and can be used by clients to traverse the list.
.IP \(bu 5
The id member uniquely identifies a feedback for a given device and class.
This allows a device to support more than one feedback of the same class.
Other feedbacks of other classes or devices may have the same ID.
.sp
.LP
Those feedbacks equivalent to those
supported by the core keyboard are reported in class
.PN KbdFeedback
using the
.PN XKbdFeedbackState
structure, which is defined as follows:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	XID class;
	int length;
	XID id;
	int click;
	int percent;
	int pitch;
	int duration;
	int led_mask;
	int global_auto_repeat;
	char auto_repeats[32];
} XKbdFeedbackState;
.De
.LP
.eM
The additional members of the
.PN XKbdFeedbackState
structure report the current state of the feedback:
.IP \(bu 5
The click member specifies the key-click volume and has a value in the range
0 (off) to 100 (loud).
.IP \(bu 5
The percent member specifies the bell volume and has a value in the range
0 (off) to 100 (loud).
.IP \(bu 5
The pitch member specifies the bell pitch in Hz. The range of the value is 
implementation-dependent.
.IP \(bu 5
The duration member specifies the duration in milliseconds of the bell.
.IP \(bu 5
The led_mask member is a bit mask that describes the current state of up to 
32 LEDs.  A value of 1 in a bit indicates that the corresponding LED is on.
.IP \(bu 5
The global_auto_repeat member has a value of
.PN AutoRepeatModeOn
or
.PN AutoRepeatModeOff .
.IP \(bu 5
The auto_repeats member is a bit vector.  Each bit set to 1 indicates
that auto-repeat is enabled for the corresponding key.  The vector is
represented as 32 bytes.  Byte N (from 0) contains the bits for keys
8N to 8N + 7, with the least significant bit in the byte representing
key 8N.
.sp
.LP
Those feedbacks equivalent to those
supported by the core pointer are reported in class
.PN PtrFeedback
using the
.PN XPtrFeedbackState
structure, which is defined as follows:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	XID class;
	int length;
	XID id;
	int accelNum;
	int accelDenom;
	int threshold;
} XPtrFeedbackState;
.De
.LP
.eM
The additional members of the
.PN XPtrFeedbackState
structure report the current state of the feedback:
.IP \(bu 5
The accelNum member returns the numerator for the acceleration multiplier.
.IP \(bu 5
The accelDenom member returns the denominator for the acceleration multiplier.
.IP \(bu 5
The accelDenom member returns the threshold for the acceleration.
.sp
.LP
Integer feedbacks are those capable of displaying integer numbers
and reported via the
.PN XIntegerFeedbackState
structure.
The minimum and maximum values that they can display are reported.
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	XID class;
	int length;
	XID id;
	int resolution;
	int minVal;
	int maxVal;
} XIntegerFeedbackState;
.De
.LP
.eM
The additional members of the
.PN XIntegerFeedbackState
structure report the capabilities of the feedback:
.IP \(bu 5
The resolution member specifies the number of digits that the feedback
can display.
.IP \(bu 5
The minVal member specifies the minimum value that the feedback can display.
.IP \(bu 5
The maxVal specifies the maximum value that the feedback can display.
.sp
.LP
String feedbacks are those that can display character information
and are reported via the
.PN XStringFeedbackState
structure.
Clients set these feedbacks by passing a list of 
.PN KeySyms
to be displayed.
The 
.PN XGetFeedbackControl
function returns the
set of key symbols that the feedback can display, as well as the 
maximum number of symbols that can be displayed.
The
.PN XStringFeedbackState
structure is defined as follows:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	XID class;
	int length;
	XID id;
	int max_symbols;
	int  num_syms_supported;
	KeySym *syms_supported;
} XStringFeedbackState;
.De
.LP
.eM
The additional members of the
.PN XStringFeedbackState
structure report the capabilities of the feedback:
.IP \(bu 5
The max_symbols member specifies the maximum number of symbols
that can be displayed.
.IP \(bu 5
The syms_supported member is a pointer to the list of supported symbols.
.IP \(bu 5
The num_syms_supported member specifies the length of the list of supported symbols.
.sp
.LP
Bell feedbacks are those that can generate a sound
and are reported via the
.PN XBellFeedbackState
structure.
Some implementations may support a bell as part of a
.PN KbdFeedback
feedback.  Class
.PN BellFeedback
is provided for implementations that do not choose to do
so and for devices that support multiple feedbacks that can produce sound.
The meaning of the members is the same as that of the corresponding fields in
the 
.PN XKbdFeedbackState
structure.
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	XID class;
	int length;
	XID id;
	int percent;
	int pitch;
	int duration;
} XBellFeedbackState;
.De
.LP
.eM
Led feedbacks are those that can generate a light and are reported via the
.PN XLedFeedbackState
structure.
Up to 32 lights per feedback are supported.  
Each bit in led_mask
corresponds to one supported light, and the corresponding bit in led_values
indicates whether that light is currently on (1) or off (0).
Some implementations may support leds as part of a
.PN KbdFeedback
feedback.
Class 
.PN LedFeedback
is provided for implementations that do not choose to do
so and for devices that support multiple led feedbacks.
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	XID class;
	int length;
	XID id;
	Mask led_values;
	Mask led_mask;
} XLedFeedbackState;
.De
.LP
.eM
.PN XGetFeedbackControl
can generate
.PN BadDevice
and
.PN BadMatch
errors.
.sp
.LP
To free the information returned by the
.PN XGetFeedbackControl
function, use 
.PN XFreeFeedbackList .
.sM
.FD 0
void XFreeFeedbackList(\fIlist\fP\^)
.br
     XFeedbackState *\fIlist\fP\^;
.FN
.IP \fIlist\fP 1i
Specifies the pointer to the 
.PN XFeedbackState
structure returned by
a previous call to
.PN XGetFeedbackControl .
.LP
.eM
.PN XFreeFeedbackList
frees the list of feedback control information.
.sp
.LP
To change the settings of a feedback on an extension device, use
.PN XChangeFeedbackControl .
This function modifies the current control values of the specified feedback
using information passed in the appropriate
.PN XFeedbackControl
structure for the feedback.
Which values are modified depends on the valuemask passed.
.sM
.FD 0
int XChangeFeedbackControl(\fIdisplay\fP\^, \fIdevice\fP\^, \fIvaluemask\fP\^, \
\fIvalue\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.br
     unsigned long \fIvaluemask\fP\^;
.br
     XFeedbackControl *\fIvalue\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the desired device.
.IP \fIvaluemask\fP 1i
Specifies one value for each bit in the mask (least to most significant
bit).  The values are associated with the feedbacks for the specified
device.
.IP \fIvalue\fP 1i
Specifies a pointer to the
.PN XFeedbackControl
structure.
.LP
.eM
.PN XChangeFeedbackControl
controls the device characteristics described by the
.PN XFeedbackControl
structure. 
There is an
.PN XFeedbackControl
structure for each class of feedback.  
These are of variable length, but the first 
three members are common to all and are as follows:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	XID class;
	int length;
	XID id;
} XFeedbackControl;
.De
.LP
.eM
Feedback class 
.PN KbdFeedback
controls feedbacks equivalent to those provided by the core keyboard using the
.PN KbdFeedbackControl
structure, which is defined as follows:.
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	XID class;
	int length;
	XID id;
	int click;
	int percent;
	int pitch;
	int duration;
	int led_mask;
	int led_value;
	int key;
	int auto_repeat_mode;
} XKbdFeedbackControl;
.De
.LP
.eM
This class controls the device characteristics described by the
.PN XKbdFeedbackControl
structure.  These include the key_click_percent,
global_auto_repeat, and individual key auto-repeat.  Valid modes
are
.PN AutoRepeatModeOn ,
.PN AutoRepeatModeOff ,
and
.PN AutoRepeatModeDefault .
.LP
Valid masks are as follows:
.sM
.LP
.TS
lw(.5i) lw(2.5i) lw(.8i).
T{
#define
T}	T{
.PN DvKeyClickPercent
T}	T{
(1L << 0)
T}
T{
#define
T}	T{
.PN DvPercent
T}	T{
(1L << 1)
T}
T{
#define
T}	T{
.PN DvPitch
T}	T{
(1L << 2)
T}
T{
#define
T}	T{
.PN DvDuration
T}	T{
(1L << 3)
T}
T{
#define
T}	T{
.PN DvLed
T}	T{
(1L << 4)
T}
T{
#define
T}	T{
.PN DvLedMode
T}	T{
(1L << 5)
T}
T{
#define
T}	T{
.PN DvKey
T}	T{
(1L << 6)
T}
T{
#define
T}	T{
.PN DvAutoRepeatMode
T}	T{
(1L << 7)
T}
.TE
.eM
.LP
Feedback class
.PN PtrFeedback
controls feedbacks equivalent to those provided by the core pointer using the
.PN PtrFeedbackControl
structure, which is defined as follows:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	XID class;
	int length;
	XID id;
	int accelNum;
	int accelDenom;
	int threshold;
} XPtrFeedbackControl;
.De
.LP
.eM
Which values are modified depends on the valuemask passed.
.LP
Valid masks are as follows:
.sM
.LP
.TS
lw(.5i) lw(2.5i) lw(.8i).
T{
#define
T}	T{
.PN DvAccelnum
T}	T{
(1L << 0)
T}
T{
#define
T}	T{
.PN DvAccelDenom
T}	T{
(1L << 1)
T}
T{
#define
T}	T{
.PN DvThreshold
T}	T{
(1L << 2)
T}
.TE
.LP
.eM
The acceleration, expressed as a fraction, is a multiplier for movement. 
For example,
specifying 3/1 means that the device moves three times as fast as normal.
The fraction may be rounded arbitrarily by the X server.  
Acceleration takes effect only if the device moves more than threshold pixels at
once and applies only to the amount beyond the value in the threshold argument.
Setting a value to -1 restores the default.
The values of the accelNumerator and threshold fields must be nonzero for
the pointer values to be set.
Otherwise, the parameters will be unchanged.
Negative values generate a
.PN BadValue
error, as does a zero value
for the accelDenominator field.
.LP
This request fails with a
.PN BadMatch
error if the specified device is not currently reporting relative motion.
If a device that is capable of reporting both relative and absolute motion
has its mode changed from
.PN Relative
to
.PN Absolute
by an
.PN XSetDeviceMode
request, valuator control values
will be ignored by the server while the device is in that mode.
.LP
Feedback class
.PN IntegerFeedback
controls integer feedbacks displayed on input devices and are
reported via the
.PN IntegerFeedbackControl
structure, which is defined as follows:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	XID class;
	int length;
	XID id;
	int int_to_display;
} XIntegerFeedbackControl;
.De
.LP
.eM
Valid masks are as follows:
.sM
.LP
.TS
lw(.5i) lw(2.5i) lw(.8i).
T{
#define
T}	T{
.PN DvInteger
T}	T{
(1L << 0)
T}
.TE
.LP
.eM
Feedback class
.PN StringFeedback
controls string feedbacks displayed on input devices
and reported via the
.PN StringFeedbackControl
structure, which is defined as follows:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	XID class;
	int length;
	XID id;
	int num_keysyms;
	KeySym *syms_to_display;
} XStringFeedbackControl;
.De
.LP
.eM
Valid masks are as follows:
.sM
.LP
.TS
lw(.5i) lw(2.5i) lw(.8i).
T{
#define
T}	T{
.PN DvString
T}	T{
(1L << 0)
T}
.TE
.LP
.eM
Feedback class
.PN BellFeedback
controls a bell on an input device and is reported via the
.PN BellFeedbackControl
structure, which is defined as follows:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	XID class;
	int length;
	XID id;
	int percent;
	int pitch;
	int duration;
} XBellFeedbackControl;
.De
.LP
.eM
Valid masks are as follows:
.sM
.LP
.TS
lw(.5i) lw(2.5i) lw(.8i).
T{
#define
T}	T{
.PN DvPercent
T}	T{
(1L << 1)
T}
T{
#define
T}	T{
.PN DvPitch
T}	T{
(1L << 2)
T}
T{
#define
T}	T{
.PN DvDuration
T}	T{
(1L << 3)
T}
.TE
.LP
.eM
Feedback class
.PN LedFeedback
controls lights on an input device and are reported via the
.PN LedFeedbackControl
structure, which is defined as follows:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	XID class;
	int length;
	XID id;
	int led_mask;
	int led_values;
} XLedFeedbackControl;
.De
.LP
.eM
Valid masks are as follows:
.sM
.LP
.TS
lw(.5i) lw(2.5i) lw(.8i).
T{
#define
T}	T{
.PN DvLed
T}	T{
(1L << 4)
T}
T{
#define
T}	T{
.PN DvLedMode
T}	T{
(1L << 5)
T}
.TE
.LP
.eM
.PN XChangeFeedbackControl
can generate
.PN BadDevice ,
.PN BadFeedBack ,
.PN BadMatch ,
and
.PN BadValue
errors.
.NH 3
Ringing a Bell on an Input Device
.XS
\*(SN Ringing a Bell on an Input Device
.XE
.LP
To ring a bell on an extension input device, use
.PN XDeviceBell .
.sM
.FD 0
int XDeviceBell(\fIdisplay\fP\^, \fIdevice\fP\^, \fIfeedbackclass\fP\^, \
\fIfeedbackid\fP\^, \fIpercent\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.br
     XID \fIfeedbackclass\fP\^, \fIfeedbackid\fP\^;
.br
     int \fIpercent\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the desired device.
.IP \fIfeedbackclass\fP 1i
Specifies the feedbackclass.  Valid values are
.PN KbdFeedbackClass
and
.PN BellFeedbackClass .
.IP \fIfeedbackid\fP 1i
Specifies the ID of the feedback that has the bell.
.IP \fIpercent\fP 1i
Specifies the volume in the range -100 (quiet) to 100 percent (loud).
.LP
.eM
.PN XDeviceBell
is analogous to the core 
.PN XBell
function.  It rings the specified bell on the specified input device feedback,
using the specified volume.
The specified volume is relative to the base volume for the feedback.
If the value for the percent argument is not in the range -100 to 100
inclusive, a
.PN BadValue
error results.
The volume at which the bell rings when the percent argument is nonnegative is:
.LP
.DS
	  base - [(base * percent) / 100] + percent
.DE
.LP
The volume at which the bell rings
when the percent argument is negative is:
.LP
.DS 
	  base + [(base * percent) / 100]
.DE
.LP
To change the base volume of the bell, use
.PN XChangeFeedbackControl .
.LP
.PN XDeviceBell
can generate
.PN BadDevice
and
.PN BadValue
errors.
.NH 3
Controlling Device Encoding
.XS
\*(SN Controlling Device Encoding
.XE
.LP
To get the key mapping of an extension device that supports input class 
.PN Keys ,
use 
.PN XGetDeviceKeyMapping .
.sM
.FD 0
KeySym * XGetDeviceKeyMapping(\fIdisplay\fP\^, \fIdevice\fP\^, \fIfirst_keycode_wanted\fP\^, \fIkeycode_count\fP\^, 
.br
                     \fIkeysyms_per_keycode_return\fP\^)
.br
      Display *\fIdisplay\fP\^;
.br
      XDevice *\fIdevice\fP\^;
.br
      KeyCode \fIfirst_keycode_wanted\fP\^;
.br
      int \fIkeycode_count\fP\^;
.br
      int *\fIkeysyms_per_keycode_return\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the desired device.
.IP \fIfirst_keycode_wanted\fP 1i
Specifies the first keycode that is to be returned.
.IP \fIkeycode_count\fP 1i
Specifies the number of keycodes that are to be returned.
.IP \fIkeysyms_per_keycode_return\fP 1i
Returns the number of keysyms per keycode.
.LP
.eM
.PN XGetDeviceKeyMapping 
is analogous to the core
.PN XGetKeyboardMapping
function.  
It returns the symbols for the specified number of keycodes for the 
specified extension device.
.LP
.PN XGetDeviceKeyMapping 
returns the symbols for the 
specified number of keycodes for the 
specified extension device, starting with the specified keycode.
The first_keycode_wanted must be greater than or equal to
min-keycode as returned 
by the
.PN XListInputDevices
request (else a 
.PN BadValue
error results). The following value:
.DS
first_keycode_wanted + keycode_count \- 1
.DE
.LP
must be less than or equal to max-keycode as returned 
by the 
.PN XListInputDevices
request (else a 
.PN BadValue
error results).
.LP
The number of elements in the keysyms list is as follows:
.DS
keycode_count * keysyms_per_keycode_return
.DE
And KEYSYM number N (counting from zero) for keycode K has an index
(counting from zero), in keysyms, of the following:
.DS
(K \- first_keycode_wanted) * keysyms_per_keycode_return + N
.DE
.LP
The keysyms_per_keycode_return value is chosen arbitrarily by the server
to be large enough to report all requested symbols.
A special KEYSYM value of
.PN NoSymbol
is used to fill in unused elements for individual keycodes.
.LP
To free the data returned by this function, use
.PN XFree .
.LP
If the specified device has not first been opened by this client via
.PN XOpenDevice ,
this request will fail with a
.PN BadDevice
error.
If that device does not support input class
.PN Keys ,
this request will fail with a 
.PN BadMatch
error.
.LP
.PN XGetDeviceKeyMapping 
can generate
.PN BadDevice ,
.PN BadMatch ,
and
.PN BadValue
errors.
.sp 
.LP
To change the keyboard mapping of an extension device that supports input class 
.PN Keys ,
use 
.PN XChangeDeviceKeyMapping .
.sM
.FD )
int
XChangeDeviceKeyMapping(\fIdisplay\fP\^, \fIdevice\fP\^, \fIfirst_keycode\fP\^, \
\fIkeysyms_per_keycode\fP\^, \fIkeysyms\fP\^,
.br
                     \fInum_codes\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.br
     int \fIfirst_keycode\fP\^;
.br
     int \fIkeysyms_per_keycode\fP\^;
.br
     KeySym *\fIkeysyms\fP\^;
.br
     int \fInum_codes\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the desired device.
.IP \fIfirst_keycode\fP 1i
Specifies the first keycode that is to be changed.
.IP \fIkeysyms_per_keycode\fP 1i
Specifies the keysyms that are to be used.
.IP \fIkeysyms\fP 1i
Specifies a pointer to an array of keysyms.
.IP \fInum_codes\fP 1i
Specifies the number of keycodes that are to be changed.
.LP
.eM
.PN XChangeDeviceKeyMapping
is analogous to the core
.PN XChangeKeyboardMapping
function.  
It defines the symbols for the specified number of keycodes for the 
specified extension keyboard device.
.LP
If the specified device has not first been opened by this client via
.PN XOpenDevice ,
this request will fail with a
.PN BadDevice
error.
If the specified device does not support input class
.PN Keys ,
this request will fail with a
.PN BadMatch
error.
.LP
The number of elements in the keysyms list must be a multiple of
keysyms_per_keycode.  Otherwise,
.PN XChangeDeviceKeyMapping
generates a
.PN BadLength
error.
The specified first_keycode must be greater than or equal to
the min_keycode value returned by the
.PN ListInputDevices
request, or this request will fail with a
.PN BadValue
error.  In addition, if the following expression is not less than
the max_keycode value returned by the
.PN ListInputDevices
request, the request will fail with a
.PN BadValue
error:
.DS
	  first_keycode + (num_codes / keysyms_per_keycode) - 1
.DE
.LP
.PN XChangeDeviceKeyMapping
can generate
.PN BadAlloc ,
.PN BadDevice ,
.PN BadMatch ,
and
.PN BadValue
errors.
.sp
.LP
To obtain the keycodes that are used as modifiers on an 
extension device that supports input class
.PN Keys ,
use
.PN XGetDeviceModifierMapping .
.sM
.FD 0
XModifierKeymap * XGetDeviceModifierMapping(\fIdisplay\fP\^, \fIdevice\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the desired device.
.LP
.eM
.PN XGetDeviceModifierMapping
is analogous to the core
.PN XGetModifierMapping
function.  
The
.PN XGetDeviceModifierMapping
function returns a newly created
.PN XModifierKeymap
structure that contains the keys being used as
modifiers for the specified device.
The structure should be freed after use with
.PN XFreeModifierMapping .
If only zero values appear in the set for any modifier,
that modifier is disabled.
.LP
.PN XGetDeviceModifierMapping
can generate
.PN BadDevice
and
.PN BadMatch
errors.
.sp
.LP
To set which keycodes are to be used as modifiers for an extension device, use
.PN XSetDeviceModifierMapping .
.sM
.FD 0
int XSetDeviceModifierMapping(\fIdisplay\fP\^, \fIdevice\fP\^, \fImodmap\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.br
     XModifierKeymap *\fImodmap\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the desired device.
.IP \fImodmap\fP 1i
Specifies a pointer to the
.PN XModifierKeymap
structure.
.LP
.eM
.PN XSetDeviceModifierMapping
is analogous to the core
.PN XSetModifierMapping
function.  
The 
.PN XSetDeviceModifierMapping
function specifies the keycodes of the keys, if any, 
that are to be used as modifiers.  A zero value means
that no key should be used.  No two arguments can have the same nonzero
keycode value.  Otherwise,
.PN XSetDeviceModifierMapping
generates a
.PN BadValue
error.
There are eight modifiers, and the modifiermap member of the 
.PN XModifierKeymap
structure contains eight sets of max_keypermod 
keycodes, one for each modifier in the order 
.PN Shift ,
.PN Lock ,
.PN Control ,
.PN Mod1 ,
.PN Mod2 ,
.PN Mod3 ,
.PN Mod4 ,
and
.PN Mod5 .
Only nonzero keycodes have meaning in each set, and zero keycodes
are ignored.
In addition, all of the nonzero keycodes must be in the range specified by 
min_keycode and max_keycode reported by the
.PN XListInputDevices
function.
Otherwise,
.PN XSetModifierMapping
generates a
.PN BadValue
error.
No keycode may appear twice in the entire map. 
Otherwise, it generates a
.PN BadValue
error.
.LP
A X server can impose restrictions on how modifiers can be changed, 
for example,
if certain keys do not generate up transitions in hardware or if multiple
modifier keys are not supported.  
If some such restriction is violated, 
the status reply is
.PN MappingFailed ,
and none of the modifiers are changed.
If the new keycodes specified for a modifier differ from those
currently defined and any (current or new) keys for that modifier are
in the logically down state, 
the status reply is
.PN MappingBusy , 
and none of the modifiers are changed.
.PN XSetModifierMapping
generates a
.PN DeviceMappingNotify
event on a
.PN MappingSuccess
status.
.LP
.PN XSetDeviceModifierMapping
can generate
.PN BadAlloc ,
.PN BadDevice ,
.PN BadMatch ,
and
.PN BadValue
errors.
.NH 3
Controlling Button Mapping
.XS
\*(SN Controlling Button Mapping
.XE
.LP
To set the mapping of the buttons on an extension device, use
.PN XSetDeviceButtonMapping .
.sM
.FD 0
int XSetDeviceButtonMapping(\fIdisplay\fP\^, \fIdevice\fP\^, \fImap\fP\^, \fInmap\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.br
     unsigned char \fImap\fP\^[];
.br
     int \fInmap\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the desired device.
.IP \fImap\fP 1i
Specifies the mapping list.
.IP \fInmap\fP 1i
Specifies the number of items in the mapping list.
.LP
.eM
.PN XSetDeviceButtonMapping
sets the mapping of the buttons on an extension device.
If it succeeds, the X server generates a
.PN DeviceMappingNotify
event, and
.PN XSetDeviceButtonMapping
returns
.PN MappingSuccess .
Elements of the list are indexed starting from one.
The length of the list must be the same as
.PN XGetDeviceButtonMapping
would return, or a 
.PN BadValue
error results.
The index is a button number, and the element of the list
defines the effective number.
A zero element disables a button, and elements are not restricted in
value by the number of physical buttons.
However, no two elements can have the same nonzero value, or a
.PN BadValue
error results.
If any of the buttons to be altered are logically in the down state,
.PN XSetDeviceButtonMapping
returns
.PN MappingBusy ,
and the mapping is not changed.
.LP
.PN XSetDeviceButtonMapping
can generate
.PN BadDevice ,
.PN BadMatch ,
and
.PN BadValue
errors.
.sp
.LP
To get the button mapping, use
.PN XGetDeviceButtonMapping .
.sM
.FD 0
int XGetDeviceButtonMapping(\fIdisplay\fP\^, \fIdevice\fP\^, \fImap_return\fP\^, \fInmap\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.br
     unsigned char \fImap_return\fP\^[];
.br
     int \fInmap\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the desired device.
.IP \fImap_return\fP 1i
Specifies the mapping list.
.IP \fInmap\fP 1i
Specifies the number of items in the mapping list.
.LP
.eM
.PN XGetDeviceButtonMapping
returns the current mapping of the specified extension device.
Elements of the list are indexed starting from one.
.PN XGetDeviceButtonMapping
returns the number of physical buttons actually on the pointer.
The nominal mapping for the buttons is the identity mapping: map[i]=i.
The nmap argument specifies the length of the array where the button
mapping is returned, and only the first nmap elements are returned 
in map_return.
.LP
.PN XGetDeviceButtonMapping
can generate
.PN BadDevice
and
.PN BadMatch
errors.
.NH 3
Obtaining the State of a Device
.XS
\*(SN Obtaining the State of a Device
.XE
.LP
To obtain information that describes the state of the keys, buttons, and 
valuators of an extension device, use
.PN XQueryDeviceState .
.sM
.FD 0
XDeviceState * XQueryDeviceState(\fIdisplay\fP\^, \fIdevice\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the desired device.
.LP
.eM
.PN XQueryDeviceState
returns a pointer to an
.PN XDeviceState
structure, which points to a list of
structures that describe the state of the keys, buttons, and valuators 
on the device:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	XID device_id;
	int num_classes;
	XInputClass *data;
} XDeviceState;
.De
.LP
.eM
The structures are of variable length, but the first 
two members are common to all and are as follows:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	unsigned char class;
	unsigned char length;
} XInputClass;
.De
.LP
.eM
The class member contains a class identifier.  This identifier can be
compared with constants defined in the file 
.Pn < X11/extensions/XI.h >.
Currently defined constants are:
.PN KeyClass ,
.PN ButtonClass ,
and
.PN ValuatorClass .
.LP
The length member contains the length of the structure and can be used
by clients to traverse the list.
.LP
The 
.PN XValuatorState
structure describes the current state of the valuators
on the device.
The num_valuators member contains the number of valuators
on the device.
The mode member is a mask whose bits report the data mode
and other state information for the device.
The following bits are currently defined:
.DS 0
.TA .5i 1.5i 2.5i
.ta .5i 1.5i 2.5i
	DeviceMode	1 << 0	Relative = 0, Absolute = 1
	ProximityState	1 << 1	InProximity = 0, OutOfProximity = 1
.DE
The valuators member contains a pointer to an array of integers that
describe the current value of the valuators.
If the mode is 
.PN Relative ,
these values are undefined.
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	unsigned char class;
	unsigned char length;
	unsigned char num_valuators;
	unsigned char mode;
	int *valuators;
} XValuatorState;
.De
.LP
.eM
The
.PN XKeyState
structure describes the current state of the keys
on the device.  Byte N (from 0) contains the
bits for key 8N to 8N + 7 with the least significant bit in the
byte representing key 8N.
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	unsigned char class;
	unsigned char length;
	short num_keys;
	char keys[32];
} XKeyState;
.De
.LP
.eM
The
.PN XButtonState
structure describes the current state of the buttons
on the device.  Byte N (from 0) contains the bits for button 8N to 8N + 7
with the least significant bit in the
byte representing button 8N.
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	unsigned char class;
	unsigned char length;
	short num_buttons;
	char buttons[32];
} XButtonState;
.De
.LP
.eM
.PN XQueryDeviceState
can generate
.PN BadDevice
errors.
.sp
.LP
To free the data returned by this function, use
.PN XFreeDeviceState .
.sM
.FD 0
void XFreeDeviceState(\fIstate\fP\^)
.br
     XDeviceState *state;
.FN
.IP \fIstate\fP 1i
Specifies the pointer to the
.PN XDeviceState
data returned by a previous call to
.PN XQueryDeviceState .
.LP
.eM
.PN XFreeDeviceState
frees the device state data.
.NH 2
Events
.XS
\*(SN Events
.XE
.LP
The input extension creates input events analogous to the core input events.
These extension input events are generated by manipulating one of the
extension input devices.
The remainder of this section discusses the following X Input Extension
event topics:
.IP \(bu 5
Event types
.IP \(bu 5
Event classes
.IP \(bu 5
Event structures
.NH 3
Event Types
.XS
\*(SN Event Types
.XE
.LP
Event types are integer numbers that a client can use to determine what
kind of event it has received.  The client compares the type field of 
the event structure with known event types to make this determination.
.LP
The core input event types are constants and are defined in the header file
.Pn < X11/X.h >.
Extension event types are not constants.  Instead, they
are dynamically allocated by the extension's request to the X server
when the extension is initialized.  Because of this, extension event
types must be obtained by the client from the server.
.LP
The client program determines the event type for an extension event by using
the information returned by the
.PN XOpenDevice
request.
This type can then be used for comparison with the type field
of events received by the client.
.LP
Extension events propagate up the window hierarchy in the same manner
as core events.  If a window is not interested in an extension event, 
it usually propagates to the closest ancestor that is interested,
unless the dont_propagate list prohibits it.
Grabs of extension devices may alter the set of windows that receive a particular
extension event.
.LP
The following table lists the event category and its associated event
type or types.
.TS
lw(2.5i) lw(2.5i).
_
.sp 6p
\fBEvent Category	Event Type\fP
.sp 6p
_
T{
Device key
T}	T{
.PN DeviceKeyPress
T}
	T{
.PN DeviceKeyRelease
T}
.sp 3p
T{
Device motion
T}	T{
.PN DeviceButtonPress
T}
	T{
.PN DeviceButtonRelease
T}
	T{
.PN DeviceMotionNotify
T}
.sp 3p
T{
Device input focus
T}	T{
.PN DeviceFocusIn
T}
	T{
.PN DeviceFocusOut
T}
.sp 3p
T{
Device state notification
T}	T{
.PN DeviceStateNotify
T}
.sp 3p
T{
Device proximity
T}	T{
.PN ProximityIn
T}
	T{
.PN ProximityOut
T}
.sp 3p
T{
Device mapping
T}	T{
.PN DeviceMappingNotify
T}
.sp 3p
T{
Device change
T}	T{
.PN ChangeDeviceNotify
T}
.sp 6p
_
.TE
.NH 3
Event Classes
.XS
\*(SN Event Classes
.XE
.LP
Event classes are integer numbers that are used in the same way as the
core event masks.  They are used by a client program to indicate to the
server which events that client program wishes to receive.
.LP
The core input event masks are constants and are defined in the header file
.Pn < X11/X.h >.
Extension event classes are not constants.  Instead, they are dynamically
allocated by the extension's request to the X server
when the extension is initialized.  Because of this, extension event
classes must be obtained by the client from the server.
.LP
The event class for an extension event and device is obtained from
information returned by the 
.PN XOpenDevice
function. 
This class can then be used in an
.PN XSelectExtensionEvent
request to ask that events of that type from that device be sent to
the client program.
.LP
For
.PN DeviceButtonPress
events, the client may specify whether
or not an implicit passive grab should be done when the button is
pressed.  If the client wants to guarantee that it will receive a
.PN DeviceButtonRelease
event for each
.PN DeviceButtonPress
event it receives, it should specify the
.PN DeviceButtonPressGrab
class in addition to the
.PN DeviceButtonPress
class.
This restricts the client in that only one client at a time
may request
.PN DeviceButtonPress
events from the same device and
window if any client specifies this class.
.LP
If any client has specified the
.PN DeviceButtonPressGrab
class, any requests by any other client that specify the same device
and window and specify either
.PN DeviceButtonPress
or
.PN DeviceButtonPressGrab
will cause an
.PN Access
error to be generated.
.LP
If only the
.PN DeviceButtonPress
class is specified, no implicit passive grab will be done when a button
is pressed on the device.
Multiple clients may use this class to specify the same device and
window combination.
.LP
The client may also select
.PN DeviceMotion
events only when a button is down.
It does this by specifying the event classes 
.PN DeviceButton1Motion
through
.PN DeviceButton5Motion .  
An input device will support only
as many button motion classes as it has buttons.
.NH 3
Event Structures
.XS
\*(SN Event Structures
.XE
.LP
Each extension event type has a corresponding structure declared in
.Pn < X11/extensions/XInput.h >.
All event structures have the following common members:
.IP type 1i
Set to the event type number that uniquely identifies it.  For example,
when the X server reports a
.PN DeviceKeyPress
event to a client application, it sends an
.PN XDeviceKeyPressEvent
structure.
.IP serial 1i
Set from the serial number reported in the protocol but expanded from the
16-bit least significant bits to a full 32-bit value.
.IP send_event 1i
Set to 
.PN True
if the event came from an
.PN XSendEvent
request.
.IP display 1i
Set to a pointer to a structure that defines the display 
on which the event was read.
.LP
Extension event structures report the current position of the X pointer.
In addition, if the device reports motion data and is reporting absolute data,
the current value of any valuators the device contains is also reported.
.NH 4
Device Key Events
.XS
\*(SN Device Key Events
.XE
.LP
Key events from extension devices contain all the information that is
contained in a key event from the X keyboard.  In addition, they contain
a device ID and report the current value of any valuators on the device,
if that device is reporting absolute data.
If data for more than six valuators is being reported, more than one
key event will be sent.
The axes_count member contains the number of axes that are being
reported.  The server sends as many of these events as are
needed to report the device data.  Each event contains the total number
of axes reported in the axes_count member and the first axis reported
in the current event in the first_axis member.
If the device supports input class
.PN Valuators , 
but is not reporting absolute mode data,
the axes_count member  contains zero (0).
.LP
The location reported in 
the x, y and x_root, y_root members is the location of the core X pointer.
.LP
The
.PN XDeviceKeyEvent
structure is defined as follows:
.LP
.sM
.Ds 0
.TA .5i 3i
.ta .5i 3i
typedef struct {
	int type;	/* of event */
	unsigned long serial;	/* # of last request processed */
	Bool send_event;	/* true if from SendEvent request */
	Display *display;	/* Display the event was read from */
	Window window;	/* "event" window reported relative to */
	XID deviceid;
	Window root;	/* root window event occurred on */
	Window subwindow;	/* child window */
	Time time;	/* milliseconds */
	int x, y;	/* x, y coordinates in event window */
	int x_root;	/* coordinates relative to root */
	int y_root;	/* coordinates relative to root */
	unsigned int state;	/* key or button mask */
	unsigned int keycode;	/* detail */
	Bool same_screen;	/* same screen flag */
	unsigned int device_state;	/* device key or button mask */
	unsigned char axes_count;
	unsigned char first_axis;
	int axis_data[6];
} XDeviceKeyEvent;

typedef XDeviceKeyEvent XDeviceKeyPressedEvent;
typedef XDeviceKeyEvent XDeviceKeyReleasedEvent;
.De
.eM
.NH 4
Device Button Events
.XS
\*(SN Device Button Events
.XE
.LP
Button events from extension devices contain all the information that is
contained in a button event from the X pointer.  In addition, they contain
a device ID and report the current value of any valuators on the device
if that device is reporting absolute data.
If data for more than six valuators is being reported, more than one
button event may be sent.
The axes_count member contains the number of axes that are being
reported.  The server sends as many of these events as are
needed to report the device data.  Each event contains the total number
of axes reported in the axes_count member and the first axis reported
in the current event in the first_axis member.
If the device supports input class
.PN Valuators , 
but is not reporting absolute mode data,
the axes_count member contains zero (0).
.LP
The location reported in 
the x, y and x_root, y_root members is the location of the core X pointer.
.LP
.sM
.Ds 0
.TA .5i 3i
.ta .5i 3i
typedef struct {
	int type;	/* of event */
	unsigned long serial;	/* # of last request processed by server */
	Bool send_event;	/* true if from a SendEvent request */
	Display *display;	/* Display the event was read from */
	Window window;	/* "event" window reported relative to */
	XID deviceid;
	Window root;	/* root window that the event occurred on */
	Window subwindow;	/* child window */
	Time time;	/* milliseconds */
	int x, y;	/* x, y coordinates in event window */
	int x_root;	/* coordinates relative to root */
	int y_root;	/* coordinates relative to root */
	unsigned int state;	/* key or button mask */
	unsigned int button;	/* detail */
	Bool same_screen;	/* same screen flag */
	unsigned int device_state;	/* device key or button mask */
	unsigned char axes_count;
	unsigned char first_axis;
	int axis_data[6];
} XDeviceButtonEvent;

typedef XDeviceButtonEvent XDeviceButtonPressedEvent;
typedef XDeviceButtonEvent XDeviceButtonReleasedEvent;
.De
.eM
.NH 4
Device Motion Events
.XS
\*(SN Device Motion Events
.XE
.LP
Motion events from extension devices contain all the information that is
contained in a motion event from the X pointer.  In addition, they contain
a device ID and report the current value of any valuators on the device.
.LP
The location reported in 
the x, y and x_root, y_root members is the location of the core X pointer, 
and so is 2-dimensional.
.LP
Extension motion devices may report motion data for a variable number of 
axes.  
The axes_count member contains the number of axes that are being
reported.  The server sends as many of these events as are
needed to report the device data.  Each event contains the total number
of axes reported in the axes_count member and the first axis reported
in the current event in the first_axis member.
.LP
.sM
.Ds 0
.TA .5i 3i
.ta .5i 3i
typedef struct {
	int type;	/* of event */
	unsigned long serial;	/* # of last request processed by server */
	Bool send_event;	/* true if from a SendEvent request */
	Display *display;	/* Display the event was read from */
	Window window;	/* "event" window reported relative to */
	XID deviceid;
	Window root;	/* root window that the event occurred on */
	Window subwindow;	/* child window */
	Time time;	/* milliseconds */
	int x, y;	/* x, y coordinates in event window */
	int x_root;	/* coordinates relative to root */
	int y_root;	/* coordinates relative to root */
	unsigned int state;	/* key or button mask */
	char is_hint;	/* detail */
	Bool same_screen;	/* same screen flag */
	unsigned int device_state;	/* device key or button mask */
	unsigned char axes_count;
	unsigned char first_axis;
	int axis_data[6];
} XDeviceMotionEvent;
.De
.eM
.NH 4
Device Focus Events
.XS
\*(SN Device Focus Events
.XE
.LP
These events are equivalent to the core focus events.
They contain the same information, with the addition
of a device ID to identify which device has had a focus change,
and a timestamp.
.LP
.PN DeviceFocusIn
and
.PN DeviceFocusOut
events are generated for
focus changes of extension devices in the same manner as core focus
events are generated.
.LP
.sM
.Ds 0
.TA .5i 2.5i
.ta .5i 2.5i
typedef struct {
	int type;	/* of event */
	unsigned long serial;	/* # of last request processed by server */
	Bool send_event;	/* true if this came from a SendEvent request */
	Display *display;	/* Display the event was read from */
	Window window;	/* "event" window it is reported relative to */
	XID deviceid;
	int mode;	/* NotifyNormal, NotifyGrab, NotifyUngrab */
	int detail;
		/*
	 	* NotifyAncestor, NotifyVirtual, NotifyInferior, 
	 	* NotifyNonLinear,NotifyNonLinearVirtual, NotifyPointer,
		* NotifyPointerRoot, NotifyDetailNone 
	 	*/
	Time time;
} XDeviceFocusChangeEvent;

typedef XDeviceFocusChangeEvent XDeviceFocusInEvent;
typedef XDeviceFocusChangeEvent XDeviceFocusOutEvent;
.De
.eM
.NH 4
Device StateNotify Event
.XS
\*(SN Device StateNotify Event
.XE
.LP
This event is analogous to the core keymap event but
reports the current state of the device for each
input class that it supports.
It is generated after every
.PN DeviceFocusIn
event and
.PN EnterNotify
event and is delivered to clients who have selected
.PN XDeviceStateNotify
events.
.LP
If the device supports input class
.PN Valuators ,
the mode member in the
.PN XValuatorStatus
structure is a bitmask that reports the device mode,
proximity state, and other state information.
The following bits are currently defined:
.DS 0
.TA .5i 1.5i
.ta .5i 1.5i
	0x01	Relative = 0, Absolute = 1
	0x02	InProximity = 0, OutOfProximity = 1
.DE
.LP
If the device supports more valuators than can be reported in a single
.PN XEvent ,
multiple
.PN XDeviceStateNotify
events will be generated.
.LP
.sM
.Ds 0
.TA .5i 3i
.ta .5i 3i
typedef struct {
	unsigned char class;
	unsigned char length;
} XInputClass;

typedef struct {
	int type;
	unsigned long serial;	/* # of last request processed by server */
	Bool send_event;	/* true if this came from a SendEvent request */
	Display *display;	/* Display the event was read from */
	Window window;
	XID deviceid;
	Time time;
	int num_classes;
	char data[64];
} XDeviceStateNotifyEvent;	

typedef struct {
	unsigned char class;
	unsigned char length;
	unsigned char num_valuators;
	unsigned char mode;
	int valuators[6];
} XValuatorStatus;

typedef struct {
	unsigned char class;
	unsigned char length;
	short num_keys;
	char keys[32];
} XKeyStatus;

typedef struct {
	unsigned char class;
	unsigned char length;
	short num_buttons;
	char buttons[32];
} XButtonStatus;
.De
.eM
.NH 4
Device Mapping Event
.XS
\*(SN Device Mapping Event
.XE
.LP
This event is equivalent to the core
.PN MappingNotify
event.
It notifies client programs when the mapping of keys,
modifiers, or buttons on an extension device has changed.
.LP
.sM
.Ds 0
.TA .5i 3i
.ta .5i 3i
typedef struct {
	int type;
	unsigned long serial;	
	Bool send_event;
	Display *display;
	Window window;
	XID deviceid;
	Time time;
	int request;
	int first_keycode;
	int count;
} XDeviceMappingEvent;
.De
.eM
.NH 4
ChangeDeviceNotify Event
.XS
\*(SN ChangeDeviceNotify Event
.XE
.LP
This event has no equivalent in the core protocol.  It notifies client
programs when one of the core devices has been changed.
.LP
.sM
.Ds 0
.TA .5i 3i
.ta .5i 3i
typedef struct {
	int type;
	unsigned long serial;
	Bool send_event;
	Display *display;
	Window window;
	XID deviceid;
	Time time;
	int request;
} XChangeDeviceNotifyEvent;
.De
.eM
.NH 4
Proximity Events
.XS
\*(SN Proximity Events
.XE
.LP
These events have no equivalent in the core protocol.  Some input
devices such as graphics tablets or touchscreens may send these
events to indicate that a stylus has moved into or out of contact
with a positional sensing surface.
.LP
The event contains the current value of any valuators on the device
if that device is reporting absolute data.
If data for more than six valuators is being reported, more than one
proximity event may be sent.
The axes_count member contains the number of axes that are being
reported.  The server sends as many of these events as are
needed to report the device data.  Each event contains the total number
of axes reported in the axes_count member and the first axis reported
in the current event in the first_axis member.
If the device supports input class
.PN Valuators , 
but is not reporting absolute mode data,
the axes_count member contains zero (0).
.LP
.sM
.Ds 0
.TA .5i 3i
.ta .5i 3i
typedef struct {
	int type;	/* ProximityIn or ProximityOut */        
	unsigned long serial;	/* # of last request processed by server */
	Bool send_event;	/* true if this came from a SendEvent request */
	Display *display;	/* Display the event was read from */
	Window window;      
	XID deviceid;
	Window root;            
	Window subwindow;      
	Time time;            
	int x, y;            
	int x_root, y_root;  
	unsigned int state;           
	Bool same_screen;     
	unsigned int device_state;	/* device key or button mask */
	unsigned char axes_count;
	unsigned char first_axis;
	int axis_data[6];
} XProximityNotifyEvent;

typedef XProximityNotifyEvent XProximityInEvent;
typedef XProximityNotifyEvent XProximityOutEvent;
.De
.eM
.NH 2
Event Handling Functions
.XS
\*(SN Event Handling Functions
.XE
.LP
This section discusses the X Input Extension
event handling functions that allow you to:
.IP \(bu 5
Determine the extension version
.IP \(bu 5
List the available devices
.IP \(bu 5
Enable and disable extension devices
.IP \(bu 5
Change the mode of a device
.IP \(bu 5
Initialize valuators on an input device
.IP \(bu 5
Get input device controls
.IP \(bu 5
Change input device controls
.IP \(bu 5
Select extension device events
.IP \(bu 5
Determine selected device events
.IP \(bu 5
Control event propogation
.IP \(bu 5
Send an event
.IP \(bu 5
Get motion history
.NH 3
Determining the Extension Version
.XS
\*(SN Determining the Extension Version
.XE
.LP
.sM
.FD 0
XExtensionVersion * XGetExtensionVersion(\fIdisplay\fP\^, \fIname\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     char *\fIname\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIname\fP 1i
Specifies the name of the desired extension.
.LP
.eM
.PN XGetExtensionVersion
allows a client to determine whether a server supports
the desired version of the input extension.
.LP
The 
.PN XExtensionVersion
structure returns information about the version of the extension
supported by the server and is defined as follows:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	Bool present;   
	short major_version;
	short minor_version;
} XExtensionVersion;
.De
.LP
.eM
The major and minor versions can be compared with constants defined in
the header file 
.Pn < X11/extensions/XI.h >.
Each version is a superset of the previous versions.
.LP
You should use 
.PN XFree
to free the data returned by this function.
.NH 3
Listing Available Devices
.XS
\*(SN Listing Available Devices
.XE
.LP
A client program that wishes to access a specific device
must first determine whether that device is connected to the X server.  This
is done through the
.PN XListInputDevices
function, which will return a list of all devices that can be opened
by the X server.  The client program can use one
of the names defined in the
.Pn < X11/extensions/XI.h >
header file in an
.PN XInternAtom 
request to determine the device type of the desired device.  This type 
can then be compared with the device types returned by the 
.PN XListInputDevices
request.
.LP
.sM
.FD 0
XDeviceInfo * XListInputDevices(\fIdisplay\fP\^, \fIndevices\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     int *\fIndevices\fP\^;            /* RETURN */
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIndevices\fP 1i
Specifies the address of a variable into which the server
can return the number of input devices available to the X server.
.LP
.eM
.PN XListInputDevices
allows a client to determine which devices 
are available for X input and information about those devices.
An array of
.PN XDeviceInfo
structures is returned, with one element in the array for each device.
The number of devices is returned in the ndevices argument.
.LP
The X pointer device and X keyboard device are reported, as well as
all available extension input devices.  The use member of the 
.PN XDeviceInfo
structure specifies the current use of the device.
If the value of this member is 
.PN IsXPointer ,
the device is the X pointer device.  If the value is
.PN IsXKeyboard ,
the device is the X keyboard device.  If the value is
.PN IsXExtensionDevice ,
the device is available for use as an extension input device.
.LP
Each
.PN XDeviceInfo
entry contains a pointer to a list of structures 
that describe the characteristics of each class
of input supported by that device.  The num_classes member
contains the number of entries in that list.
.LP
If the device supports input class
.PN Valuators ,
one of the structures pointed to by the
.PN XDeviceInfo
structure will be an
.PN XValuatorInfo
structure.  The axes member of that structure
contains the address of an array of
.PN XAxisInfo
structures.
There is one element in this array for each axis of motion
reported by the device.  The number of elements in this 
array is contained in the num_axes element of the 
.PN XValuatorInfo
structure.
The size of the motion buffer for the device is
reported in the motion_buffer member of the
.PN XValuatorInfo
structure.
.LP
The
.PN XDeviceInfo
structure is defined as follows:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct _XDeviceInfo {
	XID id;        
	Atom type;
	char *name;
	int num_classes;
	int use;
	XAnyClassPtr inputclassinfo;
} XDeviceInfo;
.De
.LP
.eM
The structures pointed to by the
.PN XDeviceInfo
structure are defined as follows:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct _XKeyInfo {
	XID class;
	int length;
	unsigned short min_keycode;
	unsigned short max_keycode;
	unsigned short num_keys;
} XKeyInfo;

typedef struct _XButtonInfo {
	XID class;
	int length;
	short num_buttons;
} XButtonInfo;

typedef struct _XValuatorInfo {
	XID class;
	int length;
	unsigned char num_axes;
	unsigned char mode;
	unsigned long motion_buffer;
	XAxisInfoPtr axes;
} XValuatorInfo;
.De
.LP
.eM
The
.PN XAxisInfo
structure pointed to by the
.PN XValuatorInfo
structure is defined as follows:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct _XAxisInfo {
	int resolution;
	int min_value;
	int max_value;
} XAxisInfo;
.De
.LP
.eM
The following atom names are defined in the 
.Pn < X11/extensions/XI.h >
header file.
.Ds 0
.TA 2i
.ta 2i
\s-1MOUSE	QUADRATURE
TABLET	SPACEBALL
KEYBOARD	DATAGLOVE
TOUCHSCREEN	EYETRACKER
TOUCHPAD	CURSORKEYS
BUTTONBOX	FOOTMOUSE
BARCODE	ID_MODULE
KNOB_BOX	ONE_KNOB
TRACKBALL	NINE_KNOB\s+1
.De
.LP
These names can be used in an
.PN XInternAtom
request to return an atom that can be used for comparison
with the type member of the 
.PN XDeviceInfo
structure.
.LP
.PN XListInputDevices
returns NULL if there are no input devices to list.
.sp
.LP
To free the data returned by 
.PN XListInputDevices ,
use
.PN XFreeDeviceList .
.sp
.LP
.sM
.FD 0
void XFreeDeviceList(\fIlist\fP\^) 
.br
    XDeviceInfo *\fIlist\fP\^; 
.FN
.IP \fIlist\fP 1i
Specifies the pointer to the
.PN XDeviceInfo
array returned by a previous call to
.PN XListInputDevices .
.LP
.eM
.PN XFreeDeviceList
frees the list of input device information.
.NH 3
Enabling and Disabling Extension Devices
.XS
\*(SN Enabling and Disabling Extension Devices
.XE
.LP
Each client program that wishes to access an extension device must request
that the server open that device by calling the
.PN XOpenDevice
function.
.sM
.FD 0
XDevice * XOpenDevice(\fIdisplay\fP\^, \fIdevice_id\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XID \fIdevice_id\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice_id\fP 1i
Specifies the ID that uniquely identifies the device to be opened.
This ID is obtained from the
.PN XListInputDevices
request.
.LP
.eM
.PN XOpenDevice
opens the device for the requesting client and, on success, returns an
.PN XDevice
structure, which is defined as follows:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	XID device_id;
	int num_classes;
	XInputClassInfo *classes;
} XDevice;
.De
.LP
.eM
The
.PN XDevice
structure contains a pointer to an array of
.PN XInputClassInfo
structures.  Each element in that array
contains information about events of a particular input class supported
by the input device.
.LP
The
.PN XInputClassInfo
structure is defined as follows:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	unsigned char input_class;
	unsigned char event_type_base;
} XInputClassInfo;
.De
.LP
.eM
A client program can determine the event
type and event class for a given event by using macros defined by the 
input extension.  The name of the macro corresponds to the desired event,
and the macro is passed the structure that describes the device from which
input is desired, for example:
.LP
.DS 0
.TA .5i
.ta .5i
	DeviceKeyPress(XDevice *device, event_type, event_class)
.DE
.LP
The macro will fill in the values of the event class to be used in an
.PN XSelectExtensionEvent
request to select the event and the event type to be used in comparing
with the event types of events received via
.PN XNextEvent .
.LP
.PN XOpenDevice
can generate
.PN BadDevice
errors.
.sp
.LP
Before terminating, the client program should request that the server close
the device by calling the
.PN XCloseDevice
function.
.sM
.FD 0
int XCloseDevice(\fIdisplay\fP\^, \fIdevice\fP\^)
.br
     Display *display;
.br
     XDevice *device;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the device to be closed.
.LP
.eM
.PN XCloseDevice
closes the device for the requesting client and frees the associated
.PN XDevice
structure.
.LP
A client may open the same extension device more than once.  Requests
after the first successful one return an additional
.PN XDevice
structure
with the same information as the first, but otherwise have no effect.
A single
.PN XCloseDevice
request will terminate that client's access to the device.
.LP
Closing a device releases any active or passive grabs the requesting client
has established.  If the device is frozen only by an active grab of the
requesting client, any queued events are released.
.LP
If a client program terminates without closing a device, the server will
automatically close that device on behalf of the client.  This does not
affect any other clients that may be accessing that device.
.LP
.PN XCloseDevice
can generate
.PN BadDevice
errors.
.NH 3
Changing the Mode of a Device
.XS
\*(SN Changing the Mode of a Device
.XE
.LP
Some devices are capable of reporting either relative or absolute motion
data. 
To change the mode of a device from relative to absolute, use
.PN XSetDeviceMode .
.sM
.FD 0
int XSetDeviceMode(\fIdisplay\fP\^, \fIdevice\fP\^, \fImode\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.br
     int \fImode\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the device whose mode should be changed.
.IP \fImode\fP 1i
Specifies the mode.  You can pass
.PN Absolute
or
.PN Relative .
.LP
.eM
.PN XSetDeviceMode
allows a client to request the server to change the mode of a 
device that is capable of reporting either absolute positional data or relative
motion data.  If the device is invalid or if the client has not previously 
requested that the server open the device via an
.PN XOpenDevice
request, this request will fail with a
.PN BadDevice
error.
If the device does not support input class
.PN Valuators
or if it is not capable of reporting the specified mode,
the request will fail with a 
.PN BadMatch
error.
.LP
This request will fail and return
.PN DeviceBusy
if another client has already opened the device and requested a different mode.
.LP
.PN XSetDeviceMode
can generate
.PN BadDevice ,
.PN BadMatch ,
.PN BadMode ,
and
.PN DeviceBusy
errors.
.NH 3
Initializing Valuators on an Input Device
.XS
\*(SN Initializing Valuators on an Input Device
.XE
.LP
Some devices that report absolute positional data can be initialized to a 
starting value.  Devices that are capable of reporting relative motion or
absolute positional data may require that their valuators be initialized 
to a starting value after the mode of the device is changed to
.PN Absolute .
.sp
.LP
To initialize the valuators on such a device, use 
.PN XSetDeviceValuators .
.sM
.FD 0
Status XSetDeviceValuators(\fIdisplay\fP\^, \fIdevice\fP\^, \fIvaluators\fP\^, \
\fIfirst_valuator\fP\^, \fInum_valuators\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.br
     int *\fIvaluators\fP\^, \fIfirst_valuator\fP\^, \fInum_valuators\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the device whose valuators should be initialized.
.IP \fIvaluators\fP 1i
Specifies the values to which each valuator should be set.
.IP \fIfirst_valuator\fP 1i
Specifies the first valuator to be set.
.IP \fInum_valuators\fP 1i
Specifies the number of valuators to be set.
.LP
.eM
.PN XSetDeviceValuators
initializes the specified valuators on the specified extension
input device.  Valuators are numbered beginning with zero.  Only the valuators
in the range specified by first_valuator and num_valuators are set.  
A
.PN BadValue
error results if the number of valuators supported by the device 
is less than the following expression:
.DS 0
.TA .5i
.ta .5i
	first_valuator + num_valuators
.DE
.LP
If the request succeeds,
.PN Success
is returned.  If the specified device is grabbed by some other client,
the request will fail and a status of
.PN AlreadyGrabbed
will be returned.
.LP
.PN XSetDeviceValuators
can generate
.PN BadDevice ,
.PN BadLength ,
.PN BadMatch ,
and 
.PN BadValue
errors.
.NH 3 
Getting Input Device Controls
.XS
\*(SN Getting Input Device Controls
.XE
.LP
Some input devices support various configuration controls
that can be queried or changed by clients.  The set of supported
controls will vary from one input device to another.  Requests
to manipulate these controls will fail if either the target
X server or the target input device does not support the 
requested device control.
.LP
Each device control has a unique identifier.  Information
passed with each device control varies in length and is mapped
by data structures unique to that device control.
.sp
.LP
To query a device control, use
.PN XGetDeviceControl .
.sM
.FD 0
XDeviceControl * XGetDeviceControl(\fIdisplay\fP\^, \fIdevice\fP\^, \fIcontrol\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.br
     int \fIcontrol\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the device whose configuration control status is to be returned.
.IP \fIcontrol\fP 1i
Identifies the specific device control to be queried.
.LP
.eM
.PN XGetDeviceControl
returns the current state of the specified device control.
If the target X server does not support that device control, a
.PN BadValue
error is returned.
If the specified device does not support that device control, a
.PN BadMatch
error
is returned.
.LP
If the request is successful, a pointer to a generic 
.PN XDeviceState
structure is returned.  The information returned varies according
to the specified control and is mapped by a structure appropriate
for that control.
The first two members are common to all device controls
and are defined as follows:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	XID control;
	int length;
} XDeviceState;
\fP
.De
.LP
.eM
The control may be compared to constants defined in the file
.Pn < X11/extensions/XI.h >.
Currently defined device controls include DEVICE_RESOLUTION.
.LP
The information returned for the DEVICE_RESOLUTION control is
defined in the 
.PN XDeviceResolutionState
structure, which is defined as follows:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	XID control;
	int length;
	int num_valuators;
	int *resolutions;
	int *min_resolutions;
	int *max_resolutions;
} XDeviceResolutionState;
.De
.LP
.eM
This device control returns a list of valuators and the range of 
valid resolutions allowed for each.  Valuators are numbered 
beginning with zero (0).  Resolutions for all valuators on the device are 
returned.  For each valuator i on the device, resolutions[i] returns 
the current setting of the resolution, min_resolutions[i] returns 
the minimum valid setting, and max_resolutions[i] returns the 
maximum valid setting.
.LP
When this control is specified,
.PN XGetDeviceControl
fails with a
.PN BadMatch
error if the specified device has no valuators.
.LP
.PN XGetDeviceControl
can generate
.PN BadMatch
and
.PN BadValue
errors.
.NH 3 
Changing Input Device Controls
.XS
\*(SN Changing Input Device Controls
.XE
.LP
Some input devices support various configuration controls
that can be changed by clients.  Typically, this would be
done to initialize the device to a known state or configuration.
The set of supported controls will vary from one input device 
to another.  Requests to manipulate these controls will fail if 
either the target X server or the target input device does not 
support the requested device control.  Setting the device control 
will also fail if the target input device is grabbed by another 
client or is open by another client and has been set to a conflicting
state.
.LP
Each device control has a unique identifier.  Information
passed with each device control varies in length and is mapped
by data structures unique to that device control.
.sp
.LP
To change a device control, use
.PN XChangeDeviceControl .
.sM
.FD 0
Status XChangeDeviceControl(\fIdisplay\fP\^, \fIdevice\fP\^, \fIcontrol\fP\^, \
\fIvalue\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.br
     int \fIcontrol\fP\^;
.br
     XDeviceControl *\fIvalue\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the device whose configuration control status is to be modified.
.IP \fIcontrol\fP 1i
Identifies the specific device control to be changed.
.IP \fIvalue\fP 1i
Specifies a pointer to an
.PN XDeviceControl
structure that describes which control is to be changed
and how it is to be changed.
.LP
.eM
.PN XChangeDeviceControl
changes the current state of the specified device control.
If the target X server does not support that device control, a
.PN BadValue
error is returned.
If the specified device does not support that device control, a
.PN BadMatch
error is returned.
If another client has the target device grabbed, a status of
.PN AlreadyGrabbed
is returned.
If another client has the device open and has set it to a 
conflicting state, a status of
.PN DeviceBusy
is returned.
If the request fails for any reason, the device control will not
be changed.
.LP
If the request is successful, the device control will be changed
and a status of
.PN Success
is returned.
The information passed varies according to the specified control
and is mapped by a structure appropriate for that control.
The first two members are common to all device controls:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	XID control;
	int length;
} XDeviceControl;
.De
.LP
.eM
The control may be set using constants defined in the 
.Pn < X11/extensions/XI.h >
header file.
Currently defined device controls include DEVICE_RESOLUTION.
.LP
The information that can be changed by the DEVICE_RESOLUTION 
control is defined in the 
.PN XDeviceResolutionControl
structure, which is defined as follows:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	XID control;
	int length;
	int first_valuator;
	int num_valuators;
	int *resolutions;
} XDeviceResolutionControl;
.De
.LP
.eM
This device control changes the resolution of the specified 
valuators on the specified extension input device.  Valuators 
are numbered beginning with zero.  Only the valuators in the range 
specified by first_valuator and num_valuators are set.  A value 
of -1 in the resolutions list indicates that the resolution for 
this valuator is not to be changed.  The num_valuators member
specifies the number of valuators in the resolutions list.
.LP
When this control is specified,
.PN XChangeDeviceControl
fails with a
.PN BadMatch
error if the specified device has no valuators.
If a resolution is specified that is not within the range of valid values 
(as returned by
.PN XGetDeviceControl ),
.PN XChangeDeviceControl
fails with a
.PN BadValue
error.
A
.PN BadValue
error results if the number of valuators supported by the device 
is less than the following expression:
.LP   
.DS 0
.TA .5i
.ta .5i
	first_valuator + num_valuators, 
.DE
.LP  
.PN XChangeDeviceControl
can generate
.PN BadMatch
and
.PN BadValue
errors.
.NH 3
Selecting Extension Device Events
.XS
\*(SN Selecting Extension Device Events
.XE
.LP
To select device input events, use
.PN XSelectExtensionEvent .
The parameters passed are a pointer to 
a list of classes that define the desired event types and devices, a count
of the number of elements in the list, and the ID of the window from which 
events are desired.
.sM
.FD 0
int XSelectExtensionEvent(\fIdisplay\fP\^, \fIwindow\fP\^, \fIevent_list\fP\^, \
\fIevent_count\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     Window \fIwindow\fP\^;
.br
     XEventClass *\fIevent_list\fP\^;
.br
     int \fIevent_count\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIwindow\fP 1i
Specifies the ID of the window from which the client wishes to receive events.
.IP \fIevent_list\fP 1i
Specifies a pointer to an array of event classes
that specify which events are desired.
.IP \fIevent_count\fP 1i
Specifies the number of elements in the event_list.
.LP
.eM
.PN XSelectExtensionEvent
requests the server to send events that match the events and
devices described by the event list and that come from the requested 
window.  
The elements of the
.PN XEventClass
array are the event_class values
obtained by invoking a macro with the pointer to an
.PN XDevice
structure returned by the
.PN XOpenDevice
request.
For example, the
.PN DeviceKeyPress
macro would return the
.PN XEventClass
for
.PN DeviceKeyPress
events from the specified device if it were invoked in the following form:
.DS 0
.TA .5i
.ta .5i
	DeviceKeyPress (XDevice *device, event_type, event_class)
.DE
.LP
Macros are defined for the following event classes: 
.DS 0
.PN DeviceKeyPress
.PN DeviceKeyRelease
.PN DeviceButtonPress
.PN DeviceButtonRelease
.PN DeviceMotionNotify
.PN DeviceFocusIn
.PN DeviceFocusOut
.PN ProximityIn
.PN ProximityOut
.PN DeviceStateNotify
.PN DeviceMappingNotify
.PN ChangeDeviceNotify
.PN DevicePointerMotionHint
.PN DeviceButton1Motion 
.PN DeviceButton2Motion
.PN DeviceButton3Motion, 
.PN DeviceButton4Motion
.PN DeviceButton5Motion
.PN DeviceButtonMotion,
.PN DeviceOwnerGrabButton
.PN DeviceButtonPressGrab
.DE
.LP
To get the next available event from within a client program, use the core
.PN XNextEvent
function.  This returns the next event whether it
came from a core device or an extension device.
.LP
Succeeding
.PN XSelectExtensionEvent
requests using event classes
for the same device as was specified on a previous request will replace
the previous set of selected events from that device with the new set.
.LP
.PN XSelectExtensionEvent
can generate
.PN BadAccess , 
.PN BadClass ,
.PN BadLength ,
and
.PN BadWindow
errors.
.NH 3
Determining Selected Device Events
.XS
\*(SN Determining Selected Device Events
.XE
.LP
To determine which extension events are currently selected from a given
window, use
.PN XGetSelectedExtensionEvents .
.sM
.FD 0
int XGetSelectedExtensionEvents(\fIdisplay\fP\^, \fIwindow\fP\^, \
\fIthis_client_count\fP\^, \fIthis_client\fP\^,
.br
                        \fIall_clients_count\fP\^, \fIall_clients\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     Window \fIwindow\fP\^;
.br
     int *\fIthis_client_count\fP\^;  /* RETURN */
.br
     XEventClass **\fIthis_client\fP\^;       /* RETURN */
.br
     int *\fIall_clients_count\fP\^;  /* RETURN */
.br
     XEventClass **\fIall_clients\fP\^;       /* RETURN */
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIwindow\fP 1i
Specifies the ID of the window from which the client wishes to receive events.
.IP \fIthis_client_count\fP 1i
Returns the number of elements in the this_client list.
.IP \fIthis_client\fP 1i
Returns a list of
.PN XEventClasses
that specify which events are
selected by this client.
.IP \fIall_clients_count\fP 1i
Returns the number of elements in the all_clients list.
.IP \fIall_clients\fP 1i
Returns a list of
.PN XEventClasses
that specify which events are
selected by all clients.
.LP
.eM
.PN XGetSelectedExtensionEvents
returns pointers to two event class arrays.  
One lists the extension events selected by this client from
the specified window.  The other lists the extension events selected by
all clients from the specified window.  This information is analogous
to that returned in your_event_mask and all_event_masks of the
.PN XWindowAttributes
structure when an 
.PN XGetWindowAttributes
request is made.
To free the two arrays returned by this function, use
.PN XFree .
.LP
.PN XGetSelectedExtensionEvents
can generate
.PN BadWindow
errors.
.NH 3
Controlling Event Propagation
.XS
\*(SN Controlling Event Propagation
.XE
.LP
Extension events propagate up the window hierarchy in the same manner
as core events.  If a window is not interested in an extension event, 
it usually propagates to the closest ancestor that is interested,
unless the dont_propagate list prohibits it.
Grabs of extension devices may alter the set of windows that receive a 
particular extension event.
.LP
Client programs may control event propagation through the use
of the following two functions:
.PN XChangeDeviceDontPropagateList
and
.PN XGetDeviceDontPropagateList .  
.LP
.sM
.FD 0
int XChangeDeviceDontPropagateList(\fIdisplay\fP\^, \fIwindow\fP\^, \
\fIevent_count\fP\^, \fIevents\fP\^, \fImode\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     Window \fIwindow\fP\^;
.br
     int \fIevent_count\fP\^;
.br
     XEventClass *\fIevents\fP\^;
.br
     int \fImode\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIwindow\fP 1i
Specifies the desired window.
.IP \fIevent_count\fP 1i
Specifies the number of elements in the events list.
.IP \fIevents\fP 1i
Specifies a pointer to the list of XEventClasses.
.IP \fImode\fP 1i
Specifies the mode.  You can pass
.PN AddToList
or
.PN DeleteFromList .
.LP
.eM
.PN XChangeDeviceDontPropagateList
adds an event to or deletes an event from the do_not_propagate list 
of extension events for the specified window.
There is one list per window, and the list remains for the life of the window.
The list is not altered if a client that changed the list terminates.
.LP
Suppression of event propagation is not allowed for all events.  If a
specified
.PN XEventClass
is invalid because suppression of that event is not allowed, a
.PN BadClass
error results.
.LP
.PN XChangeDeviceDontPropagateList
can generate
.PN BadClass ,
.PN BadMode ,
and
.PN BadWindow
errors.
.sp
.LP
.sM
.FD 0
XEventClass * XGetDeviceDontPropagateList(\fIdisplay\fP\^, \fIwindow\fP\^, \fIevent_count\fP\^)
.br
      Display *\fIdisplay\fP\^;
.br
      Window \fIwindow\fP\^;
.br
      int *\fIevent_count\fP\^;	/*RETURN */
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIwindow\fP 1i
Specifies the desired window.
.IP \fIevent_count\fP 1i
Returns the number of elements in the array returned by this function.
.LP
.eM
.PN XGetDeviceDontPropagateList
allows a client to determine the do_not_propagate list of extension events
for the specified window.
It returns an array of
.PN XEventClass ,
each
.PN XEventClass
representing a device/event type pair.
To free the data returned by this function, use
.PN XFree .
.LP
.PN XGetDeviceDontPropagateList
can generate
.PN BadWindow
errors.
.NH 3
Sending an Event
.XS
\*(SN Sending an Event
.XE
.LP
To send an extension event to another client, use
.PN XSendExtensionEvent .
.sM
.FD 0
int XSendExtensionEvent(\fIdisplay\fP\^, \fIdevice\fP\^, \fIwindow\fP\^, \
\fIpropagate\fP\^, \fIevent_count\fP\^, \fIevent_list\fP\^, \fIevent\fP\^)
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.br
     Window \fIwindow\fP\^;
.br
     Bool \fIpropagate\fP\^;
.br
     int \fIevent_count\fP\^;
.br
     XEventClass *\fIevent_list\fP\^;
.br
     XEvent *\fIevent\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the device whose ID is recorded in the event.
.IP \fIwindow\fP 1i
Specifies the destination window ID.  You can pass a window ID,
.PN PointerWindow
or
.PN InputFocus .
.IP \fIpropagate\fP 1i
Specifies a boolean value that is either
.PN True
or 
.PN False .
.IP \fIevent_count\fP 1i
Specifies the number of elements in the event_list array.
.IP \fIevent_list\fP 1i
Specifies a pointer to an array of
.PN XEventClass .
.IP \fIevent\fP 1i
Specifies a pointer to the event that is to be sent.
.LP
.eM
.PN XSendExtensionEvent
identifies the destination window, determines which clients should receive
the specified event, and ignores any active grabs.
It requires a list of
.PN XEventClass
to be specified.
These are obtained by opening an input device with the
.PN XOpenDevice
request.
.LP
.PN XSendExtensionEvent
uses the window argument to identify the destination window as follows:
.IP \(bu 5
If you pass
.PN PointerWindow ,
the destination window is the window that contains the pointer.
.IP \(bu 5
If you pass
.PN InputFocus
and if the focus window contains the pointer,
the destination window is the window that contains the pointer.
If the focus window does not contain the pointer,
the destination window is the focus window.
.LP
To determine which clients should receive the specified events, 
.PN XSendExtensionEvent
uses the propagate argument as follows:
.IP \(bu 5
If propagate is
.PN False ,
the event is sent to every client selecting
from the destination window
any of the events specified in the event_list array.
.IP \(bu 5
If propagate is
.PN True 
and no clients have selected from the destination window
any of the events specified in the event_list array, the destination is 
replaced with the closest ancestor of destination for which some client
has selected one of the specified events and for which no intervening
window has that event in its do_not_propagate mask.
If no such window exists,
or if the window is an ancestor of the focus window, and 
.PN InputFocus
was originally specified as the destination,
the event is not sent to any clients.  Otherwise, the event is reported to every
client selecting on the final destination any of the events specified
in event_list.
.LP
The event in the
.PN XEvent
structure must be one of the events defined
by the input extension, so that the X server can correctly byte swap the
contents as necessary.  The contents of the event are otherwise unaltered
and unchecked by the X server except to force send_event to
.PN True
in the forwarded event and to set the sequence number in the event correctly.
.LP
.PN XSendExtensionEvent
returns zero if the conversion-to-wire protocol failed;
otherwise, it returns nonzero.
.LP
.PN XSendExtensionEvent
can generate
.PN BadClass ,
.PN BadDevice ,
.PN BadValue ,
and
.PN BadWindow
errors.
.NH 3
Getting Motion History
.XS
\*(SN Getting Motion History
.XE
.LP
.sM
.FD 0
XDeviceTimeCoord * XGetDeviceMotionEvents(\fIdisplay\fP\^, \fIdevice\fP\^, \fIstart\fP\^, \
\fIstop\fP\^, \fInevents_return\fP\^, \fImode_return\fP\^,
.br
                     \fIaxis_count_return\fP\^);
.br
     Display *\fIdisplay\fP\^;
.br
     XDevice *\fIdevice\fP\^;
.br
     Time \fIstart\fP\^, \fIstop\fP\^;
.br
     int *\fInevents_return\fP\^;
.br
     int *\fImode_return\fP\^;
.br
     int *\fIaxis_count_return\fP\^;
.FN
.IP \fIdisplay\fP 1i
Specifies the connection to the X server.
.IP \fIdevice\fP 1i
Specifies the desired device.
.IP \fIstart\fP 1i
Specifies the start time.
.IP \fIstop\fP 1i
Specifies the stop time.
.IP \fInevents_return\fP 1i
Returns the number of positions in the motion buffer returned
for this request.
.IP \fImode_return\fP 1i
Returns the mode of the nevents information.
The mode will be one of the following:
.PN Absolute
or
.PN Relative .
.IP \fIaxis_count_return\fP 1i
Returns the number of axes reported in each of the positions returned.
.LP
.eM
.PN XGetDeviceMotionEvents
returns all positions in the device's motion history buffer
that fall between the specified start and stop times inclusive.
If the start time is in the future or is later than the stop time,
no positions are returned.
.LP
The return type for this function is an
.PN XDeviceTimeCoord
structure, which is defined as follows:
.LP
.sM
.Ds 0
.TA .5i
.ta .5i
typedef struct {
	Time time;
	unsigned int *data;
} XDeviceTimeCoord;
.De
.LP
.eM
The data member is a pointer to an array of data items.
Each item is of type int, and there is one data item
per axis of motion reported by the device.
The number of axes reported by the device is returned in the axis_count variable.
.LP
The value of the data items depends on the mode of the device. 
The mode is returned in the mode variable.  If the 
mode is
.PN Absolute ,
the data items are the raw values generated by the device.
These may be scaled by the client program using the
maximum values that the device can generate for each axis of motion
that it reports.  The maximum value for each axis is reported in
the max_val member of the
.PN XAxisInfo
structure, which is part of the information returned by the
.PN XListInputDevices
request.
.LP
If the mode is
.PN Relative ,
the data items are the relative values generated by the device.
The client program must choose an initial
position for the device and maintain a current position by 
accumulating these relative values.
.LP
Consecutive calls to
.PN XGetDeviceMotionEvents
can return data of different modes, that is, if
some client program has changed the mode of the device via an
.PN XSetDeviceMode
request.
.LP
.PN XGetDeviceMotionEvents
can generate
.PN BadDevice
and
.PN BadMatch
errors.
.sp
.LP
To free the data returned by
.PN XGetDeviceMotionEvents ,
use
.PN XFreeDeviceMotionEvents .
.LP
.sM
.FD 0
void XFreeDeviceMotionEvents(\fIevents\fP\^)
.br
      XDeviceTimeCoord *\fIevents\fP\^;
.FN
.IP \fIevents\fP 1i
Specifies the pointer to the
.PN XDeviceTimeCoord
array returned by a previous call to
.PN XGetDeviceMotionEvents .
.LP
.eM
.PN XFreeDeviceMotionEvents
frees the specified array of motion information.
.\"
.\"
.\"  Appendicies
.\"
.\"
.bp
.ds Ch ~
.sp 1
.ce 3
\s+1\fBAppendix A\fP\s-1
.XS
\*(SN Appendix A
.XE
.LP
The following information is contained in the \fB<X11/extensions/XInput.h>\fP
and \fB<X11/extensions/XI.h>\fP header files:
.DS 0
.cs CW 20
\fC
.ps 8
/* Definitions used by the library and client */

#ifndef _XINPUT_H_
#define _XINPUT_H_

#ifndef _XLIB_H_
#include <X11/Xlib.h>
#endif

#ifndef _XI_H_
#include "XI.h"
#endif

#define _deviceKeyPress		0
#define _deviceKeyRelease	1

#define _deviceButtonPress	0
#define _deviceButtonRelease	1

#define _deviceMotionNotify	0

#define _deviceFocusIn		0
#define _deviceFocusOut		1

#define _proximityIn		0
#define _proximityOut		1

#define _deviceStateNotify	0
#define _deviceMappingNotify	1
#define _changeDeviceNotify	2

#define FindTypeAndClass(d, type, class, classid, offset) \
    { int i; XInputClassInfo *ip; \
    type = 0; class = 0; \
    for (i=0, ip= ((XDevice *) d)->classes; \
	 i< ((XDevice *) d)->num_classes; \
	 i++, ip++) \
	if (ip->input_class == classid) \
	    {type =  ip->event_type_base + offset; \
	     class =  ((XDevice *) d)->device_id << 8 | type;}}

#define DeviceKeyPress(d, type, class) \
    FindTypeAndClass(d, type, class, KeyClass, _deviceKeyPress)

#define DeviceKeyRelease(d, type, class) \
    FindTypeAndClass(d, type, class, KeyClass, _deviceKeyRelease)

#define DeviceButtonPress(d, type, class) \
    FindTypeAndClass(d, type, class, ButtonClass, _deviceButtonPress)

#define DeviceButtonRelease(d, type, class) \
    FindTypeAndClass(d, type, class, ButtonClass, _deviceButtonRelease)

#define DeviceMotionNotify(d, type, class) \
    FindTypeAndClass(d, type, class, ValuatorClass, _deviceMotionNotify)

#define DeviceFocusIn(d, type, class) \
    FindTypeAndClass(d, type, class, FocusClass, _deviceFocusIn)

#define DeviceFocusOut(d, type, class) \
    FindTypeAndClass(d, type, class, FocusClass, _deviceFocusOut)

#define ProximityIn(d, type, class) \
    FindTypeAndClass(d, type, class, ProximityClass, _proximityIn)

#define ProximityOut(d, type, class) \
    FindTypeAndClass(d, type, class, ProximityClass, _proximityOut)

#define DeviceStateNotify(d, type, class) \
    FindTypeAndClass(d, type, class, OtherClass, _deviceStateNotify)

#define DeviceMappingNotify(d, type, class) \
    FindTypeAndClass(d, type, class, OtherClass, _deviceMappingNotify)

#define ChangeDeviceNotify(d, type, class) \
    FindTypeAndClass(d, type, class, OtherClass, _changeDeviceNotify)

#define DevicePointerMotionHint(d, type, class) \
    { class =  ((XDevice *) d)->device_id << 8 | _devicePointerMotionHint;}

#define DeviceButton1Motion(d, type, class) \
    { class =  ((XDevice *) d)->device_id << 8 | _deviceButton1Motion;}

#define DeviceButton2Motion(d, type, class) \
    { class =  ((XDevice *) d)->device_id << 8 | _deviceButton2Motion;}

#define DeviceButton3Motion(d, type, class) \
    { class =  ((XDevice *) d)->device_id << 8 | _deviceButton3Motion;}

#define DeviceButton4Motion(d, type, class) \
    { class =  ((XDevice *) d)->device_id << 8 | _deviceButton4Motion;}

#define DeviceButton5Motion(d, type, class) \
    { class =  ((XDevice *) d)->device_id << 8 | _deviceButton5Motion;}

#define DeviceButtonMotion(d, type, class) \
    { class =  ((XDevice *) d)->device_id << 8 | _deviceButtonMotion;}

#define DeviceOwnerGrabButton(d, type, class) \
    { class =  ((XDevice *) d)->device_id << 8 | _deviceOwnerGrabButton;}

#define DeviceButtonPressGrab(d, type, class) \
    { class =  ((XDevice *) d)->device_id << 8 | _deviceButtonGrab;}

#define NoExtensionEvent(d, type, class) \
    { class =  ((XDevice *) d)->device_id << 8 | _noExtensionEvent;}

#define BadDevice(dpy, error) _xibaddevice(dpy, &error)

#define BadClass(dpy, error) _xibadclass(dpy, &error)

#define BadEvent(dpy, error) _xibadevent(dpy, &error)

#define BadMode(dpy, error) _xibadmode(dpy, &error)

#define DeviceBusy(dpy, error) _xidevicebusy(dpy, &error)

/***************************************************************
 *
 * DeviceKey events.  These events are sent by input devices that
 * support input class Keys.
 * The location of the X pointer is reported in the coordinate
 * fields of the x,y and x_root,y_root fields.
 *
 */

typedef struct 
    {
    int            type;         /* of event */
    unsigned long  serial;       /* # of last request processed */
    Bool           send_event;   /* true if from SendEvent request */
    Display        *display;     /* Display the event was read from */
    Window         window;       /* "event" window reported relative to */
    XID            deviceid;
    Window         root;         /* root window event occured on */
    Window         subwindow;    /* child window */
    Time           time;         /* milliseconds */
    int            x, y;         /* x, y coordinates in event window */
    int            x_root;       /* coordinates relative to root */
    int            y_root;       /* coordinates relative to root */
    unsigned int   state;        /* key or button mask */
    unsigned int   keycode;      /* detail */
    Bool           same_screen;  /* same screen flag */
    unsigned int   device_state; /* device key or button mask */
    unsigned char  axes_count;
    unsigned char  first_axis;
    int            axis_data[6];
    } XDeviceKeyEvent;

typedef XDeviceKeyEvent XDeviceKeyPressedEvent;
typedef XDeviceKeyEvent XDeviceKeyReleasedEvent;

/*******************************************************************
 *
 * DeviceButton events.  These events are sent by extension devices
 * that support input class Buttons.
 *
 */

typedef struct {
    int           type;         /* of event */
    unsigned long serial;       /* # of last request processed by server */
    Bool          send_event;   /* true if from a SendEvent request */
    Display       *display;     /* Display the event was read from */
    Window        window;       /* "event" window reported relative to */
    XID           deviceid;
    Window        root;         /* root window that the event occured on */
    Window        subwindow;    /* child window */
    Time          time;         /* milliseconds */
    int           x, y;         /* x, y coordinates in event window */
    int           x_root;       /* coordinates relative to root */
    int           y_root;       /* coordinates relative to root */
    unsigned int  state;        /* key or button mask */
    unsigned int  button;       /* detail */
    Bool          same_screen;  /* same screen flag */
    unsigned int  device_state; /* device key or button mask */
    unsigned char axes_count;
    unsigned char first_axis;
    int           axis_data[6];
    } XDeviceButtonEvent;

typedef XDeviceButtonEvent XDeviceButtonPressedEvent;
typedef XDeviceButtonEvent XDeviceButtonReleasedEvent;

/*******************************************************************
 *
 * DeviceMotionNotify event.  These events are sent by extension devices
 * that support input class Valuators.
 *
 */

typedef struct 
    {
    int           type;        /* of event */
    unsigned long serial;      /* # of last request processed by server */
    Bool          send_event;  /* true if from a SendEvent request */
    Display       *display;    /* Display the event was read from */
    Window        window;      /* "event" window reported relative to */
    XID           deviceid;
    Window        root;        /* root window that the event occured on */
    Window        subwindow;   /* child window */
    Time          time;        /* milliseconds */
    int           x, y;        /* x, y coordinates in event window */
    int           x_root;      /* coordinates relative to root */
    int           y_root;      /* coordinates relative to root */
    unsigned int  state;       /* key or button mask */
    char          is_hint;     /* detail */
    Bool          same_screen; /* same screen flag */
    unsigned int  device_state; /* device key or button mask */
    unsigned char axes_count;
    unsigned char first_axis;
    int           axis_data[6];
    } XDeviceMotionEvent;

/*******************************************************************
 *
 * DeviceFocusChange events.  These events are sent when the focus
 * of an extension device that can be focused is changed.
 *
 */

typedef struct 
    {
    int           type;       /* of event */
    unsigned long serial;     /* # of last request processed by server */
    Bool          send_event; /* true if from a SendEvent request */
    Display       *display;   /* Display the event was read from */
    Window        window;     /* "event" window reported relative to */
    XID           deviceid;
    int           mode;       /* NotifyNormal, NotifyGrab, NotifyUngrab */
    int           detail;
	/*
	 * NotifyAncestor, NotifyVirtual, NotifyInferior, 
	 * NotifyNonLinear,NotifyNonLinearVirtual, NotifyPointer,
	 * NotifyPointerRoot, NotifyDetailNone 
	 */
    Time                time;
    } XDeviceFocusChangeEvent;

typedef XDeviceFocusChangeEvent XDeviceFocusInEvent;
typedef XDeviceFocusChangeEvent XDeviceFocusOutEvent;

/*******************************************************************
 *
 * ProximityNotify events.  These events are sent by those absolute
 * positioning devices that are capable of generating proximity information.
 *
 */

typedef struct 
    {
    int             type;      /* ProximityIn or ProximityOut */        
    unsigned long   serial;    /* # of last request processed by server */
    Bool            send_event; /* true if this came from a SendEvent request */
    Display         *display;  /* Display the event was read from */
    Window          window;      
    XID	            deviceid;
    Window          root;            
    Window          subwindow;      
    Time            time;            
    int             x, y;            
    int             x_root, y_root;  
    unsigned int    state;           
    Bool            same_screen;     
    unsigned int    device_state; /* device key or button mask */
    unsigned char   axes_count;
    unsigned char   first_axis;
    int             axis_data[6];
    } XProximityNotifyEvent;
typedef XProximityNotifyEvent XProximityInEvent;
typedef XProximityNotifyEvent XProximityOutEvent;

/*******************************************************************
 *
 * DeviceStateNotify events are generated on EnterWindow and FocusIn 
 * for those clients who have selected DeviceState.
 *
 */

typedef struct
    {
    unsigned char	class;
    unsigned char	length;
    } XInputClass;

typedef struct {
    int           type;
    unsigned long serial;       /* # of last request processed by server */
    Bool          send_event;   /* true if this came from a SendEvent request */
    Display       *display;     /* Display the event was read from */
    Window        window;
    XID           deviceid;
    Time          time;
    int           num_classes;
    char	  data[64];
} XDeviceStateNotifyEvent;	

typedef struct {
    unsigned char	class;
    unsigned char	length;
    unsigned char	num_valuators;
    unsigned char	mode;
    int        		valuators[6];
} XValuatorStatus;

typedef struct {
    unsigned char	class;
    unsigned char	length;
    short		num_keys;
    char        	keys[32];
} XKeyStatus;

typedef struct {
    unsigned char	class;
    unsigned char	length;
    short		num_buttons;
    char        	buttons[32];
} XButtonStatus;

/*******************************************************************
 *
 * DeviceMappingNotify event.  This event is sent when the key mapping,
 * modifier mapping, or button mapping of an extension device is changed.
 *
 */

typedef struct {
    int           type;
    unsigned long serial;       /* # of last request processed by server */
    Bool          send_event;   /* true if this came from a SendEvent request */
    Display       *display;     /* Display the event was read from */
    Window        window;       /* unused */
    XID           deviceid;
    Time          time;
    int           request;      /* one of MappingModifier, MappingKeyboard,
                                    MappingPointer */
    int           first_keycode;/* first keycode */
    int           count;        /* defines range of change w. first_keycode*/
} XDeviceMappingEvent;

/*******************************************************************
 *
 * ChangeDeviceNotify event.  This event is sent when an 
 * XChangeKeyboard or XChangePointer request is made.
 *
 */

typedef struct {
    int           type;
    unsigned long serial;       /* # of last request processed by server */
    Bool          send_event;   /* true if this came from a SendEvent request */
    Display       *display;     /* Display the event was read from */
    Window        window;       /* unused */
    XID           deviceid;
    Time          time;
    int           request;      /* NewPointer or NewKeyboard */
} XChangeDeviceNotifyEvent;

/*******************************************************************
 *
 * Control structures for input devices that support input class
 * Feedback.  These are used by the XGetFeedbackControl and 
 * XChangeFeedbackControl functions.
 *
 */

typedef struct {
     XID            class;
     int            length;
     XID            id;
} XFeedbackState;

typedef struct {
    XID     class;
    int     length;
    XID     id;
    int     click;
    int     percent;
    int     pitch;
    int     duration;
    int     led_mask;
    int     global_auto_repeat;
    char    auto_repeats[32];
} XKbdFeedbackState;

typedef struct {
    XID     class;
    int     length;
    XID     id;
    int     accelNum;
    int     accelDenom;
    int     threshold;
} XPtrFeedbackState;

typedef struct {
    XID     class;
    int     length;
    XID     id;
    int     resolution;
    int     minVal;
    int     maxVal;
} XIntegerFeedbackState;

typedef struct {
    XID     class;
    int     length;
    XID     id;
    int     max_symbols;
    int     num_syms_supported;
    KeySym  *syms_supported;
} XStringFeedbackState;

typedef struct {
    XID     class;
    int     length;
    XID     id;
    int     percent;
    int     pitch;
    int     duration;
} XBellFeedbackState;

typedef struct {
    XID     class;
    int     length;
    XID     id;
    int     led_values;
    int     led_mask;
} XLedFeedbackState;

typedef struct {
     XID            class;
     int            length;
     XID	    id;
} XFeedbackControl;

typedef struct {
    XID     class;
    int     length;
    XID     id;
    int     accelNum;
    int     accelDenom;
    int     threshold;
} XPtrFeedbackControl;

typedef struct {
    XID     class;
    int     length;
    XID     id;
    int     click;
    int     percent;
    int     pitch;
    int     duration;
    int     led_mask;
    int     led_value;
    int     key;
    int     auto_repeat_mode;
} XKbdFeedbackControl;

typedef struct {
    XID     class;
    int     length;
    XID     id;
    int     num_keysyms;
    KeySym  *syms_to_display;
} XStringFeedbackControl;

typedef struct {
    XID     class;
    int     length;
    XID     id;
    int     int_to_display;
} XIntegerFeedbackControl;

typedef struct {
    XID     class;
    int     length;
    XID     id;
    int     percent;
    int     pitch;
    int     duration;
} XBellFeedbackControl;

typedef struct {
    XID     class;
    int     length;
    XID     id;
    int     led_mask;
    int     led_values;
} XLedFeedbackControl;

/*******************************************************************
 *
 * Device control structures.
 *
 */

typedef struct {
     XID            control;
     int            length;
} XDeviceControl;

typedef struct {
     XID            control;
     int            length;
     int            first_valuator;
     int            num_valuators;
     int            *resolutions;
} XDeviceResolutionControl;

typedef struct {
     XID            control;
     int            length;
     int            num_valuators;
     int            *resolutions;
     int            *min_resolutions;
     int            *max_resolutions;
} XDeviceResolutionState;

/*******************************************************************
 *
 * An array of XDeviceList structures is returned by the 
 * XListInputDevices function.  Each entry contains information
 * about one input device.  Among that information is an array of 
 * pointers to structures that describe the characteristics of 
 * the input device.
 *
 */

typedef struct _XAnyClassinfo *XAnyClassPtr;

typedef struct _XAnyClassinfo {
    XID 	class;
    int 	length;
    } XAnyClassInfo;

typedef struct _XDeviceInfo *XDeviceInfoPtr;

typedef struct _XDeviceInfo
    {
    XID                 id;        
    Atom                type;
    char                *name;
    int                 num_classes;
    int                 use;
    XAnyClassPtr 	inputclassinfo;
    } XDeviceInfo;

typedef struct _XKeyInfo *XKeyInfoPtr;

typedef struct _XKeyInfo
    {
    XID			class;
    int			length;
    unsigned short      min_keycode;
    unsigned short      max_keycode;
    unsigned short      num_keys;
    } XKeyInfo;

typedef struct _XButtonInfo *XButtonInfoPtr;

typedef struct _XButtonInfo {
    XID		class;
    int		length;
    short 	num_buttons;
    } XButtonInfo;

typedef struct _XAxisInfo *XAxisInfoPtr;

typedef struct _XAxisInfo {
    int 	resolution;
    int 	min_value;
    int 	max_value;
    } XAxisInfo;

typedef struct _XValuatorInfo *XValuatorInfoPtr;

typedef struct	_XValuatorInfo
    {
    XID			class;
    int			length;
    unsigned char       num_axes;
    unsigned char       mode;
    unsigned long       motion_buffer;
    XAxisInfoPtr        axes;
    } XValuatorInfo;


/*******************************************************************
 *
 * An XDevice structure is returned by the XOpenDevice function.  
 * It contains an array of pointers to XInputClassInfo structures.
 * Each contains information about a class of input supported by the
 * device, including a pointer to an array of data for each type of event
 * the device reports.
 *
 */


typedef struct {
        unsigned char   input_class;
        unsigned char   event_type_base;
} XInputClassInfo;

typedef struct {
        XID                    device_id;
        int                    num_classes;
        XInputClassInfo        *classes;
} XDevice;


/*******************************************************************
 *
 * The following structure is used to return information for the 
 * XGetSelectedExtensionEvents function.
 *
 */

typedef struct {
        XEventClass     event_type;
        XID             device;
} XEventList;

/*******************************************************************
 *
 * The following structure is used to return motion history data from 
 * an input device that supports the input class Valuators.
 * This information is returned by the XGetDeviceMotionEvents function.
 *
 */

typedef struct {
        Time   time;
        int    *data;
} XDeviceTimeCoord;


/*******************************************************************
 *
 * Device state structure.
 * This is returned by the XQueryDeviceState request.
 *
 */

typedef struct {
        XID		device_id;
        int		num_classes;
        XInputClass	*data;
} XDeviceState;

/*******************************************************************
 *
 * Note that the mode field is a bitfield that reports the Proximity
 * status of the device as well as the mode.  The mode field should
 * be OR'd with the mask DeviceMode and compared with the values
 * Absolute and Relative to determine the mode, and should be OR'd
 * with the mask ProximityState and compared with the values InProximity
 * and OutOfProximity to determine the proximity state.
 *
 */

typedef struct {
    unsigned char	class;
    unsigned char	length;
    unsigned char	num_valuators;
    unsigned char	mode;
    int        		*valuators;
} XValuatorState;

typedef struct {
    unsigned char	class;
    unsigned char	length;
    short		num_keys;
    char        	keys[32];
} XKeyState;

typedef struct {
    unsigned char	class;
    unsigned char	length;
    short		num_buttons;
    char        	buttons[32];
} XButtonState;

/*******************************************************************
 *
 * Function definitions.
 *
 */

_XFUNCPROTOBEGIN

extern int	XChangeKeyboardDevice(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */
#endif
);

extern int	XChangePointerDevice(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    int			/* xaxis */,
    int			/* yaxis */
#endif
);

extern int	XGrabDevice(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    Window		/* grab_window */,
    Bool		/* ownerEvents */,
    int			/* event count */,
    XEventClass*	/* event_list */,
    int			/* this_device_mode */,
    int			/* other_devices_mode */,
    Time		/* time */
#endif
);

extern int	XUngrabDevice(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    Time 		/* time */
#endif
);

extern int	XGrabDeviceKey(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    unsigned int	/* key */,
    unsigned int	/* modifiers */,
    XDevice*		/* modifier_device */,
    Window		/* grab_window */,
    Bool		/* owner_events */,
    unsigned int	/* event_count */,
    XEventClass*	/* event_list */,
    int			/* this_device_mode */,
    int			/* other_devices_mode */
#endif
);

extern int	XUngrabDeviceKey(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    unsigned int	/* key */,
    unsigned int	/* modifiers */,
    XDevice*		/* modifier_dev */,
    Window		/* grab_window */
#endif
);

extern int	XGrabDeviceButton(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    unsigned int	/* button */,
    unsigned int	/* modifiers */,
    XDevice*		/* modifier_device */,
    Window		/* grab_window */,
    Bool		/* owner_events */,
    unsigned int	/* event_count */,
    XEventClass*	/* event_list */,
    int			/* this_device_mode */,
    int			/* other_devices_mode */
#endif
);

extern int	XUngrabDeviceButton(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    unsigned int	/* button */,
    unsigned int	/* modifiers */,
    XDevice*		/* modifier_dev */,
    Window		/* grab_window */
#endif
);

extern int	XAllowDeviceEvents(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    int			/* event_mode */,
    Time		/* time */
#endif
);

extern int	XGetDeviceFocus(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    Window*		/* focus */,
    int*		/* revert_to */,
    Time*		/* time */
#endif
);

extern int	XSetDeviceFocus(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    Window		/* focus */,
    int			/* revert_to */,
    Time		/* time */
#endif
);

extern XFeedbackState	*XGetFeedbackControl(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    int*		/* num_feedbacks */
#endif
);

extern int	XFreeFeedbackList(
#if NeedFunctionPrototypes
    XFeedbackState*	/* list */
#endif
);

extern int	XChangeFeedbackControl(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    unsigned long	/* mask */,
    XFeedbackControl*	/* f */
#endif
);

extern int	XDeviceBell(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    XID			/* feedbackclass */,
    XID			/* feedbackid */,
    int			/* percent */
#endif
);

extern KeySym	*XGetDeviceKeyMapping(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
#if NeedWidePrototypes
    unsigned int	/* first */,
#else
    KeyCode		/* first */,
#endif
    int			/* keycount */,
    int*		/* syms_per_code */
#endif
);

extern int	XChangeDeviceKeyMapping(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    int			/* first */,
    int			/* syms_per_code */,
    KeySym*		/* keysyms */,
    int			/* count */
#endif
);

extern XModifierKeymap	*XGetDeviceModifierMapping(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */
#endif
);

extern int	XSetDeviceModifierMapping(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    XModifierKeymap*	/* modmap */
#endif
);

extern int	XSetDeviceButtonMapping(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    unsigned char*	/* map[] */,
    int			/* nmap */
#endif
);

extern int	XGetDeviceButtonMapping(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    unsigned char*	/* map[] */,
    unsigned int	/* nmap */
#endif
);

extern XDeviceState	*XQueryDeviceState(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */
#endif
);

extern int	XFreeDeviceState(
#if NeedFunctionPrototypes
    XDeviceState*	/* list */
#endif
);

extern XExtensionVersion *XGetExtensionVersion(
#if NeedFunctionPrototypes
    Display*		/* display */,
    _Xconst char*	/* name */
#endif
);

extern XDeviceInfo	*XListInputDevices(
#if NeedFunctionPrototypes
    Display*		/* display */,
    int*		/* ndevices */
#endif
);

extern int	XFreeDeviceList(
#if NeedFunctionPrototypes
    XDeviceInfo*	/* list */
#endif
);

extern XDevice	*XOpenDevice(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XID			/* id */
#endif
);

extern int	XCloseDevice(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */
#endif
);

extern int	XSetDeviceMode(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    int			/* mode */
#endif
);

extern int	XSetDeviceValuators(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    int*		/* valuators */,
    int			/* first_valuator */,
    int			/* num_valuators */
#endif
);

extern XDeviceControl	*XGetDeviceControl(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    int			/* control */
#endif
);

extern int	XChangeDeviceControl(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    int			/* control */,
    XDeviceControl*	/* d */
#endif
);

extern int	XSelectExtensionEvent(
#if NeedFunctionPrototypes
    Display*		/* display */,
    Window		/* w */,
    XEventClass*	/* event_list */,
    int			/* count */
#endif
);

extern int	XGetSelectedExtensionEvents(
#if NeedFunctionPrototypes
    Display*		/* display */,
    Window		/* w */,
    int*		/* this_client_count */,
    XEventClass**	/* this_client_list */,
    int*		/* all_clients_count */,
    XEventClass**	/* all_clients_list */
#endif
);

extern int	XChangeDeviceDontPropagateList(
#if NeedFunctionPrototypes
    Display*		/* display */,
    Window		/* window */,
    int			/* count */,
    XEventClass*	/* events */,
    int			/* mode */
#endif
);

extern XEventClass	*XGetDeviceDontPropagateList(
#if NeedFunctionPrototypes
    Display*		/* display */,
    Window		/* window */,
    int*		/* count */
#endif
);

extern Status	XSendExtensionEvent(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    Window		/* dest */,
    Bool		/* prop */,
    int			/* count */,
    XEventClass*	/* list */,
    XEvent*		/* event */
#endif
);

extern XDeviceTimeCoord	*XGetDeviceMotionEvents(
#if NeedFunctionPrototypes
    Display*		/* display */,
    XDevice*		/* device */,
    Time		/* start */,
    Time		/* stop */,
    int*		/* nEvents */,
    int*		/* mode */,
    int*		/* axis_count */
#endif
);

extern int	XFreeDeviceMotionEvents(
#if NeedFunctionPrototypes
    XDeviceTimeCoord*	/* events */
#endif
);

extern int	XFreeDeviceControl(
#if NeedFunctionPrototypes
    XDeviceControl*	/* control */
#endif
);

_XFUNCPROTOEND

#endif /* _XINPUT_H_ */

/* Definitions used by the server, library and client */

#ifndef _XI_H_

#define _XI_H_

#define sz_xGetExtensionVersionReq		8
#define sz_xGetExtensionVersionReply		32
#define sz_xListInputDevicesReq			4
#define sz_xListInputDevicesReply		32
#define sz_xOpenDeviceReq			8
#define sz_xOpenDeviceReply			32
#define sz_xCloseDeviceReq			8
#define sz_xSetDeviceModeReq			8
#define sz_xSetDeviceModeReply			32
#define sz_xSelectExtensionEventReq		12
#define sz_xGetSelectedExtensionEventsReq	8
#define sz_xGetSelectedExtensionEventsReply	32
#define sz_xChangeDeviceDontPropagateListReq	12
#define sz_xGetDeviceDontPropagateListReq	8
#define sz_xGetDeviceDontPropagateListReply	32
#define sz_xGetDeviceMotionEventsReq		16
#define sz_xGetDeviceMotionEventsReply		32
#define sz_xChangeKeyboardDeviceReq		8
#define sz_xChangeKeyboardDeviceReply		32
#define sz_xChangePointerDeviceReq		8
#define sz_xChangePointerDeviceReply		32
#define sz_xGrabDeviceReq			20
#define sz_xGrabDeviceReply			32
#define sz_xUngrabDeviceReq			12
#define sz_xGrabDeviceKeyReq			20
#define sz_xGrabDeviceKeyReply			32
#define sz_xUngrabDeviceKeyReq			16
#define sz_xGrabDeviceButtonReq			20
#define sz_xGrabDeviceButtonReply		32
#define sz_xUngrabDeviceButtonReq		16
#define sz_xAllowDeviceEventsReq		12
#define sz_xGetDeviceFocusReq			8
#define sz_xGetDeviceFocusReply			32
#define sz_xSetDeviceFocusReq			16
#define sz_xGetFeedbackControlReq		8
#define sz_xGetFeedbackControlReply		32
#define sz_xChangeFeedbackControlReq		12
#define sz_xGetDeviceKeyMappingReq		8
#define sz_xGetDeviceKeyMappingReply		32
#define sz_xChangeDeviceKeyMappingReq		8
#define sz_xGetDeviceModifierMappingReq		8
#define sz_xSetDeviceModifierMappingReq		8
#define sz_xSetDeviceModifierMappingReply	32
#define sz_xGetDeviceButtonMappingReq		8
#define sz_xGetDeviceButtonMappingReply		32
#define sz_xSetDeviceButtonMappingReq		8
#define sz_xSetDeviceButtonMappingReply		32
#define sz_xQueryDeviceStateReq			8
#define sz_xQueryDeviceStateReply		32
#define sz_xSendExtensionEventReq		16
#define sz_xDeviceBellReq			8
#define sz_xSetDeviceValuatorsReq		8
#define sz_xSetDeviceValuatorsReply		32
#define sz_xGetDeviceControlReq			8
#define sz_xGetDeviceControlReply		32
#define sz_xChangeDeviceControlReq		8
#define sz_xChangeDeviceControlReply		32

#define INAME 			"XInputExtension"

#define XI_KEYBOARD	"KEYBOARD"
#define XI_MOUSE	"MOUSE"
#define XI_TABLET	"TABLET"
#define XI_TOUCHSCREEN	"TOUCHSCREEN"
#define XI_TOUCHPAD	"TOUCHPAD"
#define XI_BARCODE	"BARCODE"
#define XI_BUTTONBOX	"BUTTONBOX"
#define XI_KNOB_BOX	"KNOB_BOX"
#define XI_ONE_KNOB	"ONE_KNOB"
#define XI_NINE_KNOB	"NINE_KNOB"
#define XI_TRACKBALL	"TRACKBALL"
#define XI_QUADRATURE	"QUADRATURE"
#define XI_ID_MODULE	"ID_MODULE"
#define XI_SPACEBALL	"SPACEBALL"
#define XI_DATAGLOVE	"DATAGLOVE"
#define XI_EYETRACKER	"EYETRACKER"
#define XI_CURSORKEYS	"CURSORKEYS"
#define XI_FOOTMOUSE	"FOOTMOUSE"

#define Dont_Check			0
#define XInput_Initial_Release		1
#define XInput_Add_XDeviceBell		2
#define XInput_Add_XSetDeviceValuators	3
#define XInput_Add_XChangeDeviceControl	4

#define XI_Absent		0
#define XI_Present		1

#define XI_Initial_Release_Major		1
#define XI_Initial_Release_Minor		0

#define XI_Add_XDeviceBell_Major		1
#define XI_Add_XDeviceBell_Minor		1

#define XI_Add_XSetDeviceValuators_Major	1
#define XI_Add_XSetDeviceValuators_Minor	2

#define XI_Add_XChangeDeviceControl_Major	1
#define XI_Add_XChangeDeviceControl_Minor	3

#define DEVICE_RESOLUTION	1

#define NoSuchExtension		1

#define COUNT			0
#define CREATE			1

#define NewPointer		0
#define NewKeyboard		1

#define XPOINTER		0
#define XKEYBOARD		1

#define UseXKeyboard		0xFF

#define IsXPointer		0
#define IsXKeyboard		1
#define IsXExtensionDevice	2

#define AsyncThisDevice		0
#define SyncThisDevice		1
#define ReplayThisDevice	2
#define AsyncOtherDevices	3
#define AsyncAll		4
#define SyncAll			5

#define FollowKeyboard 		3
#define RevertToFollowKeyboard 	3

#define DvAccelNum              (1L << 0)
#define DvAccelDenom            (1L << 1)
#define DvThreshold             (1L << 2)

#define DvKeyClickPercent	(1L<<0)
#define DvPercent		(1L<<1)
#define DvPitch			(1L<<2)
#define DvDuration		(1L<<3)
#define DvLed			(1L<<4)
#define DvLedMode		(1L<<5)
#define DvKey			(1L<<6)
#define DvAutoRepeatMode	(1L<<7)

#define DvString                (1L << 0)

#define DvInteger               (1L << 0)

#define DeviceMode              (1L << 0)
#define Relative                0
#define Absolute                1

#define ProximityState          (1L << 1)
#define InProximity             (0L << 1)
#define OutOfProximity          (1L << 1)

#define AddToList               0
#define DeleteFromList          1

#define KeyClass  		0
#define ButtonClass  		1
#define ValuatorClass  		2
#define FeedbackClass  		3
#define ProximityClass  	4
#define FocusClass  		5
#define OtherClass  		6

#define KbdFeedbackClass  	0
#define PtrFeedbackClass  	1
#define StringFeedbackClass  	2
#define IntegerFeedbackClass  	3
#define LedFeedbackClass  	4
#define BellFeedbackClass  	5

#define _devicePointerMotionHint 0
#define _deviceButton1Motion	 1
#define _deviceButton2Motion	 2
#define _deviceButton3Motion	 3
#define _deviceButton4Motion	 4
#define _deviceButton5Motion	 5
#define _deviceButtonMotion	 6
#define _deviceButtonGrab	 7
#define _deviceOwnerGrabButton	 8
#define _noExtensionEvent	 9

#define XI_BadDevice	0
#define XI_BadEvent	1
#define XI_BadMode	2
#define XI_DeviceBusy	3
#define XI_BadClass	4

typedef	unsigned long	XEventClass;

/*******************************************************************
 *
 * Extension version structure.
 *
 */

typedef struct {
        int   	present;
        short	major_version;
        short	minor_version;
} XExtensionVersion;

#endif /* _XI_H_ */
\fP
.DE
.\" print Table of Contents
.if o .bp \" blank page to make count even
.bp 1
.af PN i
.PX
