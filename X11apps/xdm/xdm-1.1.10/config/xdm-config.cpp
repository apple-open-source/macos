!
!
!
!
!
#ifndef XDMCONFIGDIR
#define XDMCONFIGDIR XDMDIR
#endif
#ifndef XDMSCRIPTDIR
#define XDMSCRIPTDIR XDMDIR
#endif
DisplayManager.authDir:	XDMXAUTHDIR
DisplayManager.errorLogFile:	XDMLOGDIR/xdm.log
DisplayManager.pidFile:		XDMPIDDIR/xdm.pid
DisplayManager.keyFile:		XDMCONFIGDIR/xdm-keys
DisplayManager.servers:		XDMCONFIGDIR/Xservers
DisplayManager.accessFile:	XDMCONFIGDIR/Xaccess
DisplayManager*resources:	XDMCONFIGDIR/Xresources
DisplayManager.willing:		SU nobody -c XDMSCRIPTDIR/Xwilling
! All displays should use authorization, but we cannot be sure
! X terminals may not be configured that way, so they will require
! individual resource settings.
DisplayManager*authorize:	true
!
DisplayManager*chooser:		CHOOSERPATH
DisplayManager*startup:		XDMSCRIPTDIR/Xstartup
DisplayManager*session:		XDMSCRIPTDIR/Xsession
DisplayManager*reset:		XDMSCRIPTDIR/Xreset
DisplayManager*authComplain:	true
! The following three resources set up display :0 as the console.
DisplayManager._0.setup:	XDMSCRIPTDIR/Xsetup_0
DisplayManager._0.startup:	XDMSCRIPTDIR/GiveConsole
DisplayManager._0.reset:	XDMSCRIPTDIR/TakeConsole
#ifdef XPM
DisplayManager*loginmoveInterval:	10
#endif /* XPM */
! SECURITY: do not listen for XDMCP or Chooser requests
! Comment out this line if you want to manage X terminals with xdm
DisplayManager.requestPort:	0
