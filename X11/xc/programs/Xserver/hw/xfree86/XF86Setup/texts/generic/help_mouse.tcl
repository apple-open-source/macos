# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/texts/generic/help_mouse.tcl,v 1.2 1998/04/05 15:30:29 robin Exp $
#
set message \
{ First select the protocol for your mouse using 'p', then if needed, change the device
 name.  If applicable, also set the baud rate (1200 should work).  Avoid moving the
 mouse or pressing buttons before the correct protocol has been selected.  Press 'a'
 to apply the changes and try moving your mouse around.  If the mouse pointer does
 not move properly, try a different protocol or device name.

   Once the mouse is moving properly, test that the various buttons also work correctly.
 If you have a three button mouse and the middle button does not work, try the buttons
 labeled ChordMiddle and Emulate3Buttons.

   Note: the `Logitech' protocol is only used by older Logitech mice.  Most current
 models use the `Microsoft' or `MouseMan' protocol.

       Key    Function
     ------------------------------------------------------
        a  -  Apply changes
        b  -  Change to next baud rate
        c  -  Toggle the ChordMiddle button
        d  -  Toggle the ClearDTR button
        e  -  Toggle the Emulate3button button
        l  -  Select the next resolution
        n  -  Set the name of the device
        p  -  Select the next protocol
        r  -  Toggle the ClearRTS button
        s  -  Increase the sample rate
        t  -  Increase the 3-button emulation timeout
        3  -  Set buttons to 3
        4  -  Set buttons to 4
        5  -  Set buttons to 5
     ------------------------------------------------------
 You can also use Tab, and Shift-Tab to move around and then use Enter to activate
 the selected button.
 
 See the documentation for more information
}
