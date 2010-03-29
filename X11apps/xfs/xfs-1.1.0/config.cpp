XCOMM X font server configuration file
XCOMM See xfs(__appmansuffix__) man page for more information.

clone-self = on
use-syslog = off
catalogue = DEFAULTFONTPATH
error-file = FSERRORS
XCOMM in decipoints
default-point-size = 120
default-resolutions = 75,75,100,100

#ifdef FONTCACHE
XCOMM font cache control, specified in KB
cache-hi-mark = 2048
cache-low-mark = 1433
cache-balance = 70
#endif /* FONTCACHE */
