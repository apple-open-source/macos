#
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
# 
# Permission to use, copy, modify, distribute, and sell this software and its
# documentation for any purpose is hereby granted without fee, provided that
# the above copyright notice appear in all copies and that both that
# copyright notice and this permission notice appear in supporting
# documentation.
# 
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
# 
# Except as contained in this notice, the name of the copyright holders shall
# not be used in advertising or otherwise to promote the sale, use or
# other dealings in this Software without prior written authorization
# from the copyright holders.
# 

libmandir = $(LIB_MAN_DIR)

LIB_MAN_DIR_SUFFIX = $(LIB_MAN_DIR:@mandir@/man%=%)

libman_PRE = \
	XAllowDeviceEvents.man \
	XChangeDeviceDontPropagateList.man \
	XChangeKeyboardDevice.man \
	XChangePointerDevice.man \
	XDeviceBell.man \
	XGetDeviceControl.man \
	XGetDeviceKeyMapping.man \
	XGetDeviceModifierMapping.man \
	XGetDeviceMotionEvents.man \
	XGetExtensionVersion.man \
	XGetFeedbackControl.man \
	XGrabDevice.man \
	XGrabDeviceButton.man \
	XGrabDeviceKey.man \
	XListInputDevices.man \
	XOpenDevice.man \
	XQueryDeviceState.man \
	XSelectExtensionEvent.man \
	XSendExtensionEvent.man \
	XSetDeviceButtonMapping.man \
	XSetDeviceFocus.man \
	XSetDeviceMode.man \
	XSetDeviceValuators.man \
	XListDeviceProperties.man \
	XGetDeviceProperty.man

BUILT_SOURCES = shadows.DONE

EXTRA_DIST = $(libman_PRE)

CLEANFILES = $(libman_DATA) $(BUILT_SOURCES)

libman_DATA = $(libman_PRE:man=@LIB_MAN_SUFFIX@) \
	      $(all_shadows:=.@LIB_MAN_SUFFIX@)

SED = sed

# Strings to replace in man pages
XORGRELSTRING = @PACKAGE_STRING@
  XORGMANNAME = X Version 11

SUFFIXES = .$(LIB_MAN_SUFFIX) .man

.man.$(LIB_MAN_SUFFIX):
	sed 's/__xorgversion__/"$(XORGRELSTRING)" "$(XORGMANNAME)"/;  s/__LIB_MAN_SUFFIX__/$(LIB_MAN_SUFFIX)/' < $< > $@

# Generate man page shadow files (Replaces InstallManPageAliases from Imake)

all_shadows =                                \
    $(XGetDeviceControl_shadows)             \
    $(XGetFeedbackControl_shadows)           \
    $(XGetDeviceKeyMapping_shadows)          \
    $(XGetDeviceModifierMapping_shadows)     \
    $(XChangeDeviceDontPropagateList_shadows) \
    $(XGetDeviceMotionEvents_shadows)        \
    $(XGrabDeviceButton_shadows)             \
    $(XGrabDeviceKey_shadows)                \
    $(XGrabDevice_shadows)                   \
    $(XListInputDevices_shadows)             \
    $(XOpenDevice_shadows)                   \
    $(XSetDeviceButtonMapping_shadows)       \
    $(XSetDeviceFocus_shadows)               \
    $(XSelectExtensionEvent_shadows)         \
    $(XGetDeviceProperty_shadows)


XGetDeviceControl_shadows =                 \
    XChangeDeviceControl

XGetFeedbackControl_shadows =               \
    XChangeFeedbackControl

XGetDeviceKeyMapping_shadows =              \
    XChangeDeviceKeyMapping

XGetDeviceModifierMapping_shadows =         \
    XSetDeviceModifierMapping

XChangeDeviceDontPropagateList_shadows =    \
    XGetDeviceDontPropagateList

XGetDeviceMotionEvents_shadows =            \
    XDeviceTimeCoord

XGrabDeviceButton_shadows =                 \
    XUngrabDeviceButton

XGrabDeviceKey_shadows =                    \
    XUngrabDeviceKey

XGrabDevice_shadows =                       \
    XUngrabDevice

XListInputDevices_shadows =                 \
    XFreeDeviceList

XOpenDevice_shadows =                       \
    XCloseDevice

XSetDeviceButtonMapping_shadows =           \
    XGetDeviceButtonMapping

XSetDeviceFocus_shadows =                   \
    XGetDeviceFocus

XSelectExtensionEvent_shadows =             \
    XGetSelectedExtensionEvents

XGetDeviceProperty_shadows =                \
    XDeleteDeviceProperty \
    XChangeDeviceProperty

shadows.DONE:
	-rm -f $(all_shadows:=.@LIB_MAN_SUFFIX@)
	(for i in $(XGetDeviceControl_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XGetDeviceControl.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XGetFeedbackControl_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XGetFeedbackControl.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XGetDeviceKeyMapping_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XGetDeviceKeyMapping.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XGetDeviceModifierMapping_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XGetDeviceModifierMapping.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XChangeDeviceDontPropagateList_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XChangeDeviceDontPropagateList.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XGetDeviceMotionEvents_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XGetDeviceMotionEvents.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XGrabDeviceButton_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XGrabDeviceButton.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XGrabDeviceKey_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XGrabDeviceKey.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XGrabDevice_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XGrabDevice.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XListInputDevices_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XListInputDevices.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XOpenDevice_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XOpenDevice.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XSetDeviceButtonMapping_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XSetDeviceButtonMapping.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XSetDeviceFocus_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XSetDeviceFocus.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XSelectExtensionEvent_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XSelectExtensionEvent.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
	(for i in $(XGetDeviceProperty_shadows:=.@LIB_MAN_SUFFIX@) ; do \
	 echo .so man$(LIB_MAN_DIR_SUFFIX)/XGetDeviceProperty.$(LIB_MAN_SUFFIX) > $$i; \
	 done)
