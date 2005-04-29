LIBRARY Xrandr
VERSION LIBRARY_VERSION
EXPORTS
#ifndef __UNIXOS2__
XRRCurrentConfig
#endif
 XRRFindDisplay
#ifndef __UNIXOS2__
XRRFreeScreenInfo
#endif
 XRRGetScreenInfo
 XRRQueryExtension
 XRRQueryVersion
 XRRRootToScreen
 XRRRotations
#ifndef __UNIXOS2__
XRRScreenChangeSelectInput
#endif
 XRRSetScreenConfig
 XRRSizes
 XRRTimes
#ifndef __UNIXOS2__
XRRVisualIDToVisual
XRRVisualToDepth
#else
XRRConfigCurrentConfiguration
XRRConfigSizes
XRRConfigRotations
XRRSelectInput
XRRFreeScreenConfigInfo
XRRUpdateConfiguration
XRRConfigCurrentRate
XRRConfigRates
XRRSetScreenConfigAndRate
#endif  /* __UNIXOS2__
/* $XFree86: xc/lib/Xrandr/Xrandr-def.cpp,v 1.2 2003/03/25 04:18:12 dawes Exp $ */
