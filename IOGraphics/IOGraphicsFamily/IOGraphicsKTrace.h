//
//  IOGraphicsKTrace.h
//  IOGraphics
//
//  Created by local on 2/9/17.
//
//

#ifndef IOGraphicsKTrace_h
#define IOGraphicsKTrace_h

#include <sys/kdebug.h>

#include "GTrace.hpp"
#include "GMetric.hpp"


// KTracing and GTracing always enabled.
#pragma mark - IOG_KTRACE
#if GTRACE_REVISION >= 0x1
class GTrace;
extern GTrace *     gGTrace;
extern GMetricsRecorder * gGMetrics;

#ifndef IOG_KTRACE
    #define IOG_KTRACE(_f_, _t_, _t1_, _a1_, _t2_, _a2_, _t3_, _a3_, _t4_, _a4_) \
        do{\
            KERNEL_DEBUG_CONSTANT_RELEASE(IOKDBG_CODE(DBG_IOGRAPHICS, _f_) | _t_, \
                                          _a1_, _a2_, _a3_, _a4_, 0);\
            GTRACE((_f_ & 0x03FF)|((_t_ & 0x3) << 10), _a1_, _t2_, _a2_, _t3_, _a3_, _t4_, _a4_);\
            GMETRIC(_t1_, _t_, GMETRIC_DATA_FROM_FUNC(_f_)); \
        }while(0)
#endif /*IOG_KTRACE*/

#else /*GTRACE_REVISION*/

#warning "**GTrace Disabled**"

#ifndef IOG_KTRACE
    #define IOG_KTRACE(_f_, _t_, _t1_, _a1_, _t2_, _a2_, _t3_, _a3_, _t4_, _a4_) \
        KERNEL_DEBUG_CONSTANT_RELEASE(IOKDBG_CODE(DBG_IOGRAPHICS, _f_) | _t_, \
                                      _a1_, _a2_, _a3_, _a4_, 0);
#endif /*IOG_KTRACE*/

#endif /*GTRACE_REVISION*/


#pragma mark - Ariadne Tracing
// Ariadne tracing IOGraphics (Telemetry) - master control
#ifndef ENABLE_TELEMETRY
#define ENABLE_TELEMETRY                                0
#endif

#ifndef ENABLE_IONDRV_TELEMETRY
#define ENABLE_IONDRV_TELEMETRY                         0
#endif

#if ENABLE_TELEMETRY

// Trace Flags from NVRAM "iogt" property
extern uint32_t gIOGATFlags;


// Trace control
#define TRACE_IODISPLAYWRANGLER                         0x00000001
#define TRACE_IODISPLAY                                 0x00000002
#define TRACE_IODISPLAYCONNECT                          0x00000004
#define TRACE_IOFBCONTROLLER                            0x00000008
#define TRACE_IOFRAMEBUFFER                             0x00000010
#define TRACE_IOFRAMEBUFFERPARAMETERHANDLER             0x00000020
#define TRACE_FRAMEBUFFER                               0x00000040
#define TRACE_APPLEBACKLIGHT                            0x00000080
#define TRACE_IOFBUSERCLIENT                            0x00000100
#define TRACE_IOFBSHAREDUSERCLIENT                      0x00000200
#define TRACE_IOI2INTERFACEUSERCLIENT                   0x00000400
#define TRACE_IOI2INTERFACE                             0x00000800
#define TRACE_IOFBI2INTERFACE                           0x00001000
#define TRACE_IOBOOTFRAMEBUFFER                         0x00002000
#define TRACE_IONDRVFRAMEBUFFER                         0x00004000
#define TRACE_IOACCELERATOR                             0x00008000
#define TRACE_IOACCELERATORUSERCLIENT                   0x00010000
#define TRACE_IOFBDIAGNOSTICUSERCLIENT                  0x00020000

#define TRACE_MASK                                      0x0001FFFF


// Function ID helper
#define MAKE_FID_DEFINE(_cls_,_fnc_)                    _cls_ ## _FID_ ## _fnc_


// KTrace Telemetry - durations
#define IODISPLAYWRANGLER_TELEMETRY_START               1
#define IODISPLAYWRANGLER_TELEMETRY_END                 2
#define IODISPLAY_TELEMETRY_START                       3
#define IODISPLAY_TELEMETRY_END                         4
#define IODISPLAYCONNECT_TELEMETRY_START                5
#define IODISPLAYCONNECT_TELEMETRY_END                  6
#define IOFBCONTROLLER_TELEMETRY_START                  7
#define IOFBCONTROLLER_TELEMETRY_END                    8
#define IOFRAMEBUFFER_TELEMETRY_START                   9
#define IOFRAMEBUFFER_TELEMETRY_END                     10
#define IOFRAMEBUFFERPARAMETERHANDLER_TELEMETRY_START   11
#define IOFRAMEBUFFERPARAMETERHANDLER_TELEMETRY_END     12
#define FRAMEBUFFER_TELEMETRY_START                     13
#define FRAMEBUFFER_TELEMETRY_END                       14
#define APPLEBACKLIGHT_TELEMETRY_START                  15
#define APPLEBACKLIGHT_TELEMETRY_END                    16
#define IOFBUSERCLIENT_TELEMETRY_START                  17
#define IOFBUSERCLIENT_TELEMETRY_END                    18
#define IOFBSHAREDUSERCLIENT_TELEMETRY_START            19
#define IOFBSHAREDUSERCLIENT_TELEMETRY_END              20
#define IOI2INTERFACEUSERCLIENT_TELEMETRY_START         21
#define IOI2INTERFACEUSERCLIENT_TELEMETRY_END           22
#define IOI2INTERFACE_TELEMETRY_START                   23
#define IOI2INTERFACE_TELEMETRY_END                     24
#define IOFBI2INTERFACE_TELEMETRY_START                 25
#define IOFBI2INTERFACE_TELEMETRY_END                   26
#define IOBOOTFRAMEBUFFER_TELEMETRY_START               27
#define IOBOOTFRAMEBUFFER_TELEMETRY_END                 28
#define IONDRVFRAMEBUFFER_TELEMETRY_START               29
#define IONDRVFRAMEBUFFER_TELEMETRY_END                 30
#define IOACCELERATOR_TELEMETRY_START                   31
#define IOACCELERATOR_TELEMETRY_END                     32
#define IOACCELERATORUSERCLIENT_TELEMETRY_START         33
#define IOACCELERATORUSERCLIENT_TELEMETRY_END           34
#define IOFBDIAGNOSTICUSERCLIENT_TELEMETRY_START        35
#define IOFBDIAGNOSTICUSERCLIENT_TELEMETRY_END          36
// - 98 future, 99 reserved


// KTrace Telemetry - events
#define IOGRAPHICS_TELEMETRY_EVENT_RESERVED             100


// Ariadne Codes
// Telemetry for IODisplayWrangler functions
#define IODISPLAYWRANGLER_ARIADNE_START                 ARIADNEDBG_CODE(DBG_IOGRAPHICS, IODISPLAYWRANGLER_TELEMETRY_START)
#define IODISPLAYWRANGLER_ARIADNE_END                   ARIADNEDBG_CODE(DBG_IOGRAPHICS, IODISPLAYWRANGLER_TELEMETRY_END)
// Telemetry for IODisplay functions
#define IODISPLAY_ARIADNE_START                         ARIADNEDBG_CODE(DBG_IOGRAPHICS, IODISPLAY_TELEMETRY_START)
#define IODISPLAY_ARIADNE_END                           ARIADNEDBG_CODE(DBG_IOGRAPHICS, IODISPLAY_TELEMETRY_END)
// Telemetry for IODisplayConnect functions
#define IODISPLAYCONNECT_ARIADNE_START                  ARIADNEDBG_CODE(DBG_IOGRAPHICS, IODISPLAYCONNECT_TELEMETRY_START)
#define IODISPLAYCONNECT_ARIADNE_END                    ARIADNEDBG_CODE(DBG_IOGRAPHICS, IODISPLAYCONNECT_TELEMETRY_END)
// Telemetry for IOFBController functions
#define IOFBCONTROLLER_ARIADNE_START                    ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOFBCONTROLLER_TELEMETRY_START)
#define IOFBCONTROLLER_ARIADNE_END                      ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOFBCONTROLLER_TELEMETRY_END)
// Telemetry for IOFramebuffer functions
#define IOFRAMEBUFFER_ARIADNE_START                     ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOFRAMEBUFFER_TELEMETRY_START)
#define IOFRAMEBUFFER_ARIADNE_END                       ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOFRAMEBUFFER_TELEMETRY_END)
// Telemetry for IOFramebufferParameterHandler functions
#define IOFRAMEBUFFERPARAMETERHANDLER_ARIADNE_START     ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOFRAMEBUFFERPARAMETERHANDLER_TELEMETRY_START)
#define IOFRAMEBUFFERPARAMETERHANDLER_ARIADNE_END       ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOFRAMEBUFFERPARAMETERHANDLER_TELEMETRY_END)
// Telemetry for vendor functions
#define FRAMEBUFFER_ARIADNE_START                       ARIADNEDBG_CODE(DBG_IOGRAPHICS, FRAMEBUFFER_TELEMETRY_START)
#define FRAMEBUFFER_ARIADNE_END                         ARIADNEDBG_CODE(DBG_IOGRAPHICS, FRAMEBUFFER_TELEMETRY_END)
// Telemetry for AppleBackLight functions
#define APPLEBACKLIGHT_ARIADNE_START                    ARIADNEDBG_CODE(DBG_IOGRAPHICS, APPLEBACKLIGHT_TELEMETRY_START)
#define APPLEBACKLIGHT_ARIADNE_END                      ARIADNEDBG_CODE(DBG_IOGRAPHICS, APPLEBACKLIGHT_TELEMETRY_END)
// Telemetry for IOFramebufferUserClient fucntions
#define IOFBUSERCLIENT_ARIADNE_START                    ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOFBUSERCLIENT_TELEMETRY_START)
#define IOFBUSERCLIENT_ARIADNE_END                      ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOFBUSERCLIENT_TELEMETRY_END)
// Telemetry for IOFramebufferSharedUserClient functions
#define IOFBSHAREDUSERCLIENT_ARIADNE_START              ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOFBSHAREDUSERCLIENT_TELEMETRY_START)
#define IOFBSHAREDUSERCLIENT_ARIADNE_END                ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOFBSHAREDUSERCLIENT_TELEMETRY_END)
// Telemetry for IOFramebufferSharedUserClient functions
#define IOFBDIAGNOSTICUSERCLIENT_ARIADNE_START          ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOFBDIAGNOSTICUSERCLIENT_TELEMETRY_START)
#define IOFBDIAGNOSTICUSERCLIENT_ARIADNE_END            ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOFBDIAGNOSTICUSERCLIENT_TELEMETRY_END)
// Telemetry for IOI2CInterfaceUserClient functions
#define IOI2INTERFACEUSERCLIENT_ARIADNE_START           ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOI2INTERFACEUSERCLIENT_TELEMETRY_START)
#define IOI2INTERFACEUSERCLIENT_ARIADNE_END             ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOI2INTERFACEUSERCLIENT_TELEMETRY_END)
// Telemetry for IOI2CInterface functions
#define IOI2INTERFACE_ARIADNE_START                     ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOI2INTERFACE_TELEMETRY_START)
#define IOI2INTERFACE_ARIADNE_END                       ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOI2INTERFACE_TELEMETRY_END)
// Telemetry for IOFramebufferI2CInterface functions
#define IOFBI2INTERFACE_ARIADNE_START                   ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOFBI2INTERFACE_TELEMETRY_START)
#define IOFBI2INTERFACE_ARIADNE_END                     ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOFBI2INTERFACE_TELEMETRY_END)
// Telemetry for IOBootFramebuffer functions
#define IOBOOTFRAMEBUFFER_ARIADNE_START                 ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOBOOTFRAMEBUFFER_TELEMETRY_START)
#define IOBOOTFRAMEBUFFER_ARIADNE_END                   ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOBOOTFRAMEBUFFER_TELEMETRY_END)
// Telemetry for IONDRVFramebuffer functions
#define IONDRVFRAMEBUFFER_ARIADNE_START                 ARIADNEDBG_CODE(DBG_IOGRAPHICS, IONDRVFRAMEBUFFER_TELEMETRY_START)
#define IONDRVFRAMEBUFFER_ARIADNE_END                   ARIADNEDBG_CODE(DBG_IOGRAPHICS, IONDRVFRAMEBUFFER_TELEMETRY_END)
// Telemetry for IOAccelerator functions
#define IOACCELERATOR_ARIADNE_START                     ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOACCELERATOR_TELEMETRY_START)
#define IOACCELERATOR_ARIADNE_END                       ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOACCELERATOR_TELEMETRY_END)
// Telemetry for IOAcceleratorUserClient functions
#define IOACCELERATORUSERCLIENT_ARIADNE_START           ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOACCELERATORUSERCLIENT_TELEMETRY_START)
#define IOACCELERATORUSERCLIENT_ARIADNE_END             ARIADNEDBG_CODE(DBG_IOGRAPHICS, IOACCELERATORUSERCLIENT_TELEMETRY_END)


// IOGraphics function IDs
// IODisplayWrangler
#define IODW_FID_reserved                               0
#define IODW_FID_serverStart                            1
#define IODW_FID_start                                  2
#define IODW_FID__displayHandler                        3
#define IODW_FID__displayConnectHandler                 4
#define IODW_FID_displayHandler                         5
#define IODW_FID_displayConnectHandler                  6
#define IODW_FID_makeDisplayConnects                    7
#define IODW_FID_destroyDisplayConnects                 8
#define IODW_FID_activityChange                         9
#define IODW_FID_getDisplayConnect                      10
#define IODW_FID_getConnectFlagsForDisplayMode          11
#define IODW_FID_getFlagsForDisplayMode                 12
#define IODW_FID_initForPM                              13
#define IODW_FID_initialPowerStateForDomainState        14
#define IODW_FID_setAggressiveness                      15
#define IODW_FID_setPowerState                          16
#define IODW_FID_getDisplaysPowerState                  17
#define IODW_FID_nextIdleTimeout                        18
#define IODW_FID_activityTickle                         19
#define IODW_FID_copyProperty                           20
#define IODW_FID_setProperties                          21
// IODisplay
#define IOD_FID_reserved                                0
#define IOD_FID_initialize                              1
#define IOD_FID_probe                                   2
#define IOD_FID_getConnection                           3
#define IOD_FID_getGammaTableByIndex                    4
#define IOD_FID_searchParameterHandlers                 5
#define IOD_FID_start                                   6
#define IOD_FID_addParameterHandler                     7
#define IOD_FID_removeParameterHandler                  8
#define IOD_FID_stop                                    9
#define IOD_FID_free                                    10
#define IOD_FID_readFramebufferEDID                     11
#define IOD_FID_getConnectFlagsForDisplayMode           12
#define IOD_FID_getIntegerRange                         13
#define IOD_FID_setForKey                               14
#define IOD_FID_setProperties                           15
#define IOD_FID_updateNumber                            16
#define IOD_FID_addParameter                            17
#define IOD_FID_setParameter                            18
#define IOD_FID__framebufferEvent                       19
#define IOD_FID_framebufferEvent                        20
#define IOD_FID_doIntegerSet                            21
#define IOD_FID_doDataSet                               22
#define IOD_FID_doUpdate                                23
#define IOD_FID_initPowerManagement                     24
#define IOD_FID_setDisplayPowerState                    25
#define IOD_FID_dropOneLevel                            26
#define IOD_FID_makeDisplayUsable                       27
#define IOD_FID_setPowerState                           28
#define IOD_FID_maxCapabilityForDomainState             29
#define IOD_FID_initialPowerStateForDomainState         30
#define IOD_FID_powerStateForDomainState                31
// IODisplayConnect
#define IODC_FID_reserved                               0
#define IODC_FID_initWithConnection                     1
#define IODC_FID_getFramebuffer                         2
#define IODC_FID_getConnection                          3
#define IODC_FID_getAttributeForConnection              4
#define IODC_FID_setAttributeForConnection              5
#define IODC_FID_joinPMtree                             6
// IOFBController
#define IOFBC_FID_reserved                              0
#define IOFBC_FID_withFramebuffer                       1
#define IOFBC_FID_init                                  2
#define IOFBC_FID_unhookFB                              3
#define IOFBC_FID_free                                  4
#define IOFBC_FID_copyController                        5
#define IOFBC_FID_alias                                 6
#define IOFBC_FID_computeState                          7
#define IOFBC_FID_startAsync                            8
#define IOFBC_FID_startThread                           9
#define IOFBC_FID_didWork                               10
#define IOFBC_FID_processConnectChange                  11
#define IOFBC_FID_matchOnlineFramebuffers               12
#define IOFBC_FID_asyncWork                             13
#define IOFBC_FID_checkPowerWork                        14
#define IOFBC_FID_checkConnectionWork                   15
#define IOFBC_FID_messageConnectionChange               16
// IOFramebuffer
#define IOFB_FID_reserved                               0
#define IOFB_FID_StdFBRemoveCursor8                     1
#define IOFB_FID_startThread                            2
#define IOFB_FID_fbLock                                 3
#define IOFB_FID_fbUnlock                               4
#define IOFB_FID_StdFBDisplayCursor                     5
#define IOFB_FID_StdFBRemoveCursor                      6
#define IOFB_FID_RemoveCursor                           7
#define IOFB_FID_DisplayCursor                          8
#define IOFB_FID_SysHideCursor                          9
#define IOFB_FID_SysShowCursor                          10
#define IOFB_FID_CheckShield                            11
#define IOFB_FID_setupCursor                            12
#define IOFB_FID_stopCursor                             13
#define IOFB_FID_extCreateSharedCursor                  14
// 15-16 unused since Sept 2017
#define IOFB_FID_closestDepth                           17
#define IOFB_FID_extGetPixelInformation                 18
#define IOFB_FID_extGetCurrentDisplayMode               19
#define IOFB_FID_extSetStartupDisplayMode               20
#define IOFB_FID_saveGammaTables                        21
#define IOFB_FID_extSetGammaTable                       22
#define IOFB_FID_updateGammaTable                       23
#define IOFB_FID_extSetCLUTWithEntries                  24
#define IOFB_FID_updateCursorForCLUTSet                 25
#define IOFB_FID_deferredCLUTSetInterrupt               26
#define IOFB_FID_deferredCLUTSetTimer                   27
#define IOFB_FID_checkDeferredCLUTSet                   28
#define IOFB_FID_createSharedCursor                     29
#define IOFB_FID_setBoundingRect                        30
#define IOFB_FID_newUserClient                          31
#define IOFB_FID_extGetDisplayModeCount                 32
#define IOFB_FID_extGetDisplayModes                     33
#define IOFB_FID_extGetVRAMMapOffset                    34
#define IOFB_FID__extEntry                              35
#define IOFB_FID__extExit                               36
#define IOFB_FID_extSetBounds                           37
#define IOFB_FID_extValidateDetailedTiming              38
#define IOFB_FID_extSetColorConvertTable                39
#define IOFB_FID_requestTerminate                       40
#define IOFB_FID_terminate                              41
#define IOFB_FID_willTerminate                          42
#define IOFB_FID_didTerminate                           43
#define IOFB_FID_stop                                   44
#define IOFB_FID_free                                   45
#define IOFB_FID_probeAccelerator                       46
#define IOFB_FID_initialize                             47
#define IOFB_FID_copyController                         48
#define IOFB_FID_start                                  49
#define IOFB_FID_deferredMoveCursor                     50
#define IOFB_FID_cursorWork                             51
#define IOFB_FID__setCursorImage                        52
#define IOFB_FID__setCursorState                        53
#define IOFB_FID_transformLocation                      54
#define IOFB_FID_moveCursor                             55
#define IOFB_FID_hideCursor                             56
#define IOFB_FID_showCursor                             57
#define IOFB_FID_updateVBL                              58
#define IOFB_FID_deferredVBLDisable                     59
#define IOFB_FID_getTimeOfVBL                           60
#define IOFB_FID_handleVBL                              61
#define IOFB_FID_resetCursor                            62
#define IOFB_FID_getVBLTime                             63
#define IOFB_FID_getBoundingRect                        64
#define IOFB_FID_getNotificationSemaphore               65
#define IOFB_FID_extSetCursorVisible                    66
#define IOFB_FID_extSetCursorPosition                   67
#define IOFB_FID_transformCursor                        68
#define IOFB_FID_extSetNewCursor                        69
#define IOFB_FID_convertCursorImage                     70
#define IOFB_FID_deliverDisplayModeDidChangeNotification    71
#define IOFB_FID_saveFramebuffer                        72
#define IOFB_FID_restoreFramebuffer                     73
#define IOFB_FID_handleEvent                            74
#define IOFB_FID_notifyServer                           75
#define IOFB_FID_getIsUsable                            76
#define IOFB_FID_postWake                               77
#define IOFB_FID_pmSettingsChange                       78
#define IOFB_FID_systemWork                             79
#define IOFB_FID_copyDisplayConfig                      80
#define IOFB_FID_serverAckTimeout                       81
#define IOFB_FID_StdFBRemoveCursor32                    82
#define IOFB_FID_isWakingFromHibernateGfxOn             83
#define IOFB_FID_checkPowerWork                         84
#define IOFB_FID_extEndConnectionChange                 85
#define IOFB_FID_extProcessConnectionChange             86
#define IOFB_FID_processConnectChange                   87
#define IOFB_FID_updateOnline                           88
#define IOFB_FID_displaysOnline                         89
#define IOFB_FID_matchFramebuffer                       90
#define IOFB_FID_setPowerState                          91
#define IOFB_FID_powerStateWillChangeTo                 92
#define IOFB_FID_powerStateDidChangeTo                  93
#define IOFB_FID_updateDisplaysPowerState               94
#define IOFB_FID_delayedEvent                           95
#define IOFB_FID_resetClamshell                         96
#define IOFB_FID_displayOnline                          97
#define IOFB_FID_clamshellState                         98
#define IOFB_FID_agcMessage                             99
#define IOFB_FID_muxPowerMessage                        100
#define IOFB_FID_systemPowerChange                      101
#define IOFB_FID_deferredSpeedChangeEvent               102
#define IOFB_FID_setAggressiveness                      103
#define IOFB_FID_getAggressiveness                      104
#define IOFB_FID_serverAcknowledgeNotification          105
#define IOFB_FID_extAcknowledgeNotification             106
#define IOFB_FID_extRegisterNotificationPort            107
#define IOFB_FID__getApertureRange                      108
#define IOFB_FID_writePrefs                             109
#define IOFB_FID_connectChangeInterrupt                 110
#define IOFB_FID_assignGLIndex                          111
#define IOFB_FID_open                                   112
#define IOFB_FID_setTransform                           113
#define IOFB_FID_getTransform                           114
#define IOFB_FID_selectTransform                        115
#define IOFB_FID_probeAll                               116
#define IOFB_FID_requestProbe                           117
#define IOFB_FID_initFB                                 118
#define IOFB_FID_postOpen                               119
#define IOFB_FID_callPlatformFunction                   120
#define IOFB_FID_message                                121
#define IOFB_FID_getControllerWorkLoop                  122
#define IOFB_FID_getGraphicsSystemWorkLoop              123
#define IOFB_FID_getWorkLoop                            124
#define IOFB_FID_setCaptured                            125
#define IOFB_FID_setDimDisable                          126
#define IOFB_FID_getDimDisable                          127
#define IOFB_FID_setNextDependent                       128
#define IOFB_FID_getNextDependent                       129
#define IOFB_FID_close                                  130
#define IOFB_FID_getApertureRangeWithLength             131
#define IOFB_FID_getVRAMRange                           132
#define IOFB_FID_setUserRanges                          133
#define IOFB_FID_setBackingFramebuffer                  134
#define IOFB_FID_switchBackingFramebuffer               135
#define IOFB_FID_findConsole                            136
#define IOFB_FID_setupForCurrentConfig                  137
#define IOFB_FID_getConfigMode                          138
#define IOFB_FID_setVBLTiming                           139
#define IOFB_FID_doSetup                                140
#define IOFB_FID_suspend                                141
#define IOFB_FID_extSetDisplayMode                      142
#define IOFB_FID_doSetDisplayMode                       143
#define IOFB_FID_checkMirrorSafe                        144
#define IOFB_FID_extSetMirrorOne                        145
#define IOFB_FID_setWSAAAttribute                       146
#define IOFB_FID_extSetAttribute                        147
#define IOFB_FID_extGetAttribute                        148
#define IOFB_FID_extGetInformationForDisplayMode        149
#define IOFB_FID_setDisplayAttributes                   150
#define IOFB_FID_extSetProperties                       151
#define IOFB_FID_setAttribute                           152
#define IOFB_FID_readClamshellState                     153
#define IOFB_FID_clamshellHandler                       154
#define IOFB_FID_getAttribute                           155
#define IOFB_FID_setNumber                              156
#define IOFB_FID_serializeInfo                          157
#define IOFB_FID_addFramebufferNotification             158
#define IOFB_FID_deliverFramebufferNotification         159
#define IOFB_FID_enableController                       160
#define IOFB_FID_isConsoleDevice                        161
#define IOFB_FID_setDisplayMode                         162
#define IOFB_FID_setApertureEnable                      163
#define IOFB_FID_setStartupDisplayMode                  164
#define IOFB_FID_getStartupDisplayMode                  165
#define IOFB_FID_setCLUTWithEntries                     166
#define IOFB_FID_setGammaTable2                         167
#define IOFB_FID_setGammaTable                          168
#define IOFB_FID_getTimingInfoForDisplayMode            169
#define IOFB_FID_validateDetailedTiming                 170
#define IOFB_FID_setDetailedTimings                     171
#define IOFB_FID_getConnectionCount                     172
#define IOFB_FID_setAttributeExt                        173
#define IOFB_FID_getAttributeExt                        174
#define IOFB_FID_setAttributeForConnectionExt           175
#define IOFB_FID_getAttributeForConnectionExt           176
#define IOFB_FID_getAttributeForConnectionParam         177
#define IOFB_FID_setAttributeForConnectionParam         178
#define IOFB_FID_setAttributeForConnection              179
#define IOFB_FID_getAttributeForConnection              180
#define IOFB_FID_setCursorImage                         181
#define IOFB_FID_setCursorState                         182
#define IOFB_FID_flushCursor                            183
#define IOFB_FID_registerForInterruptType               184
#define IOFB_FID_unregisterInterrupt                    185
#define IOFB_FID_setInterruptState                      186
#define IOFB_FID_getAppleSense                          187
#define IOFB_FID_connectFlags                           188
#define IOFB_FID_setDDCClock                            189
#define IOFB_FID_setDDCData                             190
#define IOFB_FID_readDDCClock                           191
#define IOFB_FID_readDDCData                            192
#define IOFB_FID_enableDDCRaster                        193
#define IOFB_FID_hasDDCConnect                          194
#define IOFB_FID_getDDCBlock                            195
#define IOFB_FID_doI2CRequest                           196
#define IOFB_FID_stopDDC1SendCommand                    197
#define IOFB_FID_i2cReadData                            198
#define IOFB_FID_i2cWriteData                           199
#define IOFB_FID_waitForDDCDataLine                     200
#define IOFB_FID_readDDCBlock                           201
#define IOFB_FID_i2cStart                               202
#define IOFB_FID_i2cStop                                203
#define IOFB_FID_i2cSendAck                             204
#define IOFB_FID_i2cSendNack                            205
#define IOFB_FID_i2cWaitForAck                          206
#define IOFB_FID_i2cSendByte                            207
#define IOFB_FID_i2cReadByte                            208
#define IOFB_FID_i2cWaitForBus                          209
#define IOFB_FID_i2cReadDDCciData                       210
#define IOFB_FID_i2cRead                                211
#define IOFB_FID_i2cWrite                               212
#define IOFB_FID_i2cSend9Stops                          213
#define IOFB_FID_setPreferences                         214
#define IOFB_FID_copyPreferences                        215
#define IOFB_FID_copyPreference                         216
#define IOFB_FID_getIntegerPreference                   217
#define IOFB_FID_setPreference                          218
#define IOFB_FID_setIntegerPreference                   219
#define IOFB_FID_getTransformPrefs                      220
#define IOFB_FID_diagnoseReport                         221
#define IOFB_FID_extDiagnose                            222
#define IOFB_FID_extReservedB                           223
#define IOFB_FID_extReservedC                           224
#define IOFB_FID_extReservedD                           225
#define IOFB_FID_extReservedE                           226
#define IOFB_FID___Report                               227
#define IOFB_FID_dpInterruptProc                        228
#define IOFB_FID_dpInterrupt                            229
#define IOFB_FID_dpProcessInterrupt                     230
#define IOFB_FID_dpUpdateConnect                        231
#define IOFB_FID_StdFBDisplayCursor555                  232
#define IOFB_FID_StdFBDisplayCursor8P                   233
#define IOFB_FID_StdFBDisplayCursor8G                   234
#define IOFB_FID_StdFBDisplayCursor30Axxx               235
#define IOFB_FID_StdFBDisplayCursor32Axxx               236
#define IOFB_FID_StdFBRemoveCursor16                    237
#define IOFB_FID_deliverFramebufferNotificationCallout  238
#define IOFB_FID_waitQuietController                    239
#define IOFB_FID_addFramebufferNotificationWithOptions  240
#define IOFB_FID_deliverGroupNotification               241
#define IOFB_FID_disableNotifiers                       242
#define IOFB_FID_cleanupNotifiers                       243
#define IOFB_FID_initNotifiers                          244
#define IOFB_FID_attach                                 245
#define IOFB_FID_extCopySharedCursor                    246
#define IOFB_FID_newDiagnosticUserClient                247
#define IOFB_FID_extSetHibernateGammaTable              248
#define IOFB_FID_closeNoSys                             249
#define IOFB_FID_clamshellOfflineShouldChange           250

// IOFramebufferParameterHandler
#define IOFBPH_FID_reserved                             0
#define IOFBPH_FID_withFramebuffer                      1
#define IOFBPH_FID_free                                 2
#define IOFBPH_FID_setDisplay                           3
#define IOFBPH_FID_displayModeChange                    4
#define IOFBPH_FID_doIntegerSet                         5
#define IOFBPH_FID_doDataSet                            6
#define IOFBPH_FID_doUpdate                             7
// vendor implementations
#define FB_FID_reserved                                 0
#define FB_FID_getApertureRange                         1
#define FB_FID_getVRAMRange                             2
#define FB_FID_handleEvent                              3
#define FB_FID_enableController                         4
#define FB_FID_getPixelFormats                          5
#define FB_FID_getDisplayModeCount                      6
#define FB_FID_availables_slot_7                        7
#define FB_FID_getDisplayModes                          8
#define FB_FID_getInformationForDisplayMode             9
#define FB_FID_getPixelFormatsForDisplayMode            10
#define FB_FID_getPixelInformation                      11
#define FB_FID_getCurrentDisplayMode                    12
#define FB_FID_setDisplayMode                           13
#define FB_FID_setApertureEnable                        14
#define FB_FID_setStartupDisplayMode                    15
#define FB_FID_getStartupDisplayMode                    16
#define FB_FID_setCLUTWithEntries                       17
#define FB_FID_setGammaTable                            18
#define FB_FID_setAttribute                             19
#define FB_FID_getAttribute                             20
#define FB_FID_getTimingInfoForDisplayMode              21
#define FB_FID_validateDetailedTiming                   22
#define FB_FID_setDetailedTimings                       23
#define FB_FID_getConnectionCount                       24
#define FB_FID_setAttributeForConnection                25
#define FB_FID_getAttributeForConnection                26
#define FB_FID_convertCursorImage                       27
#define FB_FID_setCursorImage                           28
#define FB_FID_setCursorState                           29
#define FB_FID_flushCursor                              30
#define FB_FID_getAppleSense                            31
#define FB_FID_connectFlags                             32
#define FB_FID_setDDCClock                              33
#define FB_FID_setDDCData                               34
#define FB_FID_readDDCData                              35
#define FB_FID_enableDDCRaster                          36
#define FB_FID_hasDDCConnect                            37
#define FB_FID_getDDCBlock                              38
#define FB_FID_registerForInterruptType                 39
#define FB_FID_unregisterInterrupt                      40
#define FB_FID_doI2CRequest                             41
#define FB_FID_diagnoseReport                           42
#define FB_FID_setGammaTable2                           43
// AppleBackLight
#define ABL_FID_reserved                                0
#define ABL_FID_probe                                   1
#define ABL_FID_start                                   2
#define ABL_FID_stop                                    3
#define ABL_FID_initPowerManagement                     4
#define ABL_FID_setPowerState                           5
#define ABL_FID_fadeAbort                               6
#define ABL_FID_fadeWork                                7
#define ABL_FID_makeDisplayUsable                       8
#define ABL_FID_maxCapabilityForDomainState             9
#define ABL_FID_initialPowerStateForDomainState         10
#define ABL_FID_powerStateForDomainState                11
#define ABL_FID_doIntegerSet                            12
#define ABL_FID_doUpdate                                13
#define ABL_FID_updatePowerParam                        14
#define ABL_FID__deferredEvent                          15
#define ABL_FID_framebufferEvent                        16
// IOFramebufferUserClient
#define IOFBUC_FID_reserved                             0
#define IOFBUC_FID_withTask                             1
#define IOFBUC_FID_start                                2
#define IOFBUC_FID_registerNotificationPort             3
#define IOFBUC_FID_getNotificationSemaphore             4
#define IOFBUC_FID_clientClose                          5
#define IOFBUC_FID_getService                           6
#define IOFBUC_FID_clientMemoryForType                  7
#define IOFBUC_FID_setProperties                        8
#define IOFBUC_FID_externalMethod                       9
#define IOFBUC_FID_connectClient                        10
#define IOFBUC_FID_rpcEnter                             11
#define IOFBUC_FID_rpcLeave                             12
#define IOFBUC_FID_terminate                            13
#define IOFBUC_FID_requestTerminate                     14
#define IOFBUC_FID_willTerminate                        15
#define IOFBUC_FID_didTerminate                         16
#define IOFBUC_FID_finalize                             17
#define IOFBUC_FID_stop                                 18
#define IOFBUC_FID_free                                 19
// IOFramebufferSharedUserClient
#define IOFBSUC_FID_reserved                            0
#define IOFBSUC_FID_withTask                            1
#define IOFBSUC_FID_start                               2
#define IOFBSUC_FID_free                                3
#define IOFBSUC_FID_release                             4
#define IOFBSUC_FID_getService                          5
#define IOFBSUC_FID_clientMemoryForType                 6
#define IOFBSUC_FID_getNotificationSemaphore            7
#define IOFBSUC_FID_rpcEnter                            8
#define IOFBSUC_FID_rpcLeave                            9
#define IOFBSUC_FID_terminate                          10
#define IOFBSUC_FID_requestTerminate                   11
#define IOFBSUC_FID_willTerminate                      12
#define IOFBSUC_FID_didTerminate                       13
#define IOFBSUC_FID_finalize                           14
#define IOFBSUC_FID_stop                               15
// IOFramebufferDiagnosticUserClient
#define IOFBDUC_FID_reserved                            0
#define IOFBDUC_FID_client                              1
#define IOFBDUC_FID_start                               2
#define IOFBDUC_FID_clientClose                         3
#define IOFBDUC_FID_externalMethod                      4
#define IOFBDUC_FID_rpcEnter                            5
#define IOFBDUC_FID_rpcLeave                            6
#define IOFBDUC_FID_requestTerminate                    7
#define IOFBDUC_FID_willTerminate                       8
#define IOFBDUC_FID_didTerminate                        9
#define IOFBDUC_FID_stop                               10
// IOI2CInterfaceUserClient
#define IOI2CUC_FID_reserved                            0
#define IOI2CUC_FID_withTask                            1
#define IOI2CUC_FID_start                               2
#define IOI2CUC_FID_clientClose                         3
#define IOI2CUC_FID_getService                          4
#define IOI2CUC_FID_getTargetAndMethodForIndex          5
#define IOI2CUC_FID_setProperties                       6
#define IOI2CUC_FID_extAcquireBus                       7
#define IOI2CUC_FID_extReleaseBus                       8
#define IOI2CUC_FID_extIO                               9
#define IOI2CUC_FID_willTerminate                      10
#define IOI2CUC_FID_didTerminate                       11
#define IOI2CUC_FID_requestTerminate                   12
#define IOI2CUC_FID_terminate                          13
#define IOI2CUC_FID_finalize                           14
#define IOI2CUC_FID_stop                               15
#define IOI2CUC_FID_free                               16
// IOI2CInterface
#define IOI2C_FID_reserved                              0
#define IOI2C_FID_registerI2C                           1
#define IOI2C_FID_newUserClient                         2
// IOFramebufferI2CInterface
#define IOFBI2C_FID_reserved                            0
#define IOFBI2C_FID_create                              1
#define IOFBI2C_FID_withFramebuffer                     2
#define IOFBI2C_FID_start                               3
#define IOFBI2C_FID_startIO                             4
#define IOFBI2C_FID_willTerminate                       5
#define IOFBI2C_FID_didTerminate                        6
#define IOFBI2C_FID_requestTerminate                    7
#define IOFBI2C_FID_terminate                           8
#define IOFBI2C_FID_finalize                            9
#define IOFBI2C_FID_stop                               10
#define IOFBI2C_FID_free                               11
// IOBootFramebuffer
#define IOBFB_FID_reserved                              0
#define IOBFB_FID_probe                                 1
#define IOBFB_FID_getPixelFormats                       2
#define IOBFB_FID_getDisplayModeCount                   3
#define IOBFB_FID_getDisplayModes                       4
#define IOBFB_FID_getInformationForDisplayMode          5
#define IOBFB_FID_getPixelFormatsForDisplayMode         6
#define IOBFB_FID_getPixelInformation                   7
#define IOBFB_FID_getCurrentDisplayMode                 8
#define IOBFB_FID_getApertureRange                      9
#define IOBFB_FID_isConsoleDevice                       10
#define IOBFB_FID_setGammaTable                         11
#define IOBFB_FID_setCLUTWithEntries                    12
// IONDRVFramebuffer
#define IONDRVFB_FID_reserved                           0
#define IONDRVFB_FID_probe                              1
#define IONDRVFB_FID_setProperties                      2
#define IONDRVFB_FID_stop                               3
#define IONDRVFB_FID_start                              4
#define IONDRVFB_FID_isConsoleDevice                    5
#define IONDRVFB_FID_enableController                   6
#define IONDRVFB_FID__videoJackStateChangeHandler       7
#define IONDRVFB_FID__avProbeAction                     8
#define IONDRVFB_FID__probeAction                       9
#define IONDRVFB_FID_requestProbe                       10
#define IONDRVFB_FID_getVRAMRange                       11
#define IONDRVFB_FID__undefinedSymbolHandler            12
#define IONDRVFB_FID_undefinedSymbolHandler             13
#define IONDRVFB_FID_free                               14
#define IONDRVFB_FID_registerForInterruptType           15
#define IONDRVFB_FID_unregisterInterrupt                16
#define IONDRVFB_FID_setInterruptState                  17
#define IONDRVFB_FID_VSLNewInterruptService             18
#define IONDRVFB_FID_VSLDisposeInterruptService         19
#define IONDRVFB_FID_VSLDoInterruptService              20
#define IONDRVFB_FID_VSLPrepareCursorForHardwareCursor  21
#define IONDRVFB_FID_setCursorImage                     22
#define IONDRVFB_FID_setCursorState                     23
#define IONDRVFB_FID_doDriverIO                         24
#define IONDRVFB_FID__doControl                         25
#define IONDRVFB_FID__doStatus                          26
#define IONDRVFB_FID_extControl                         27
#define IONDRVFB_FID_extStatus                          28
#define IONDRVFB_FID_doControl                          29
#define IONDRVFB_FID_doStatus                           30
#define IONDRVFB_FID_checkDriver                        31
#define IONDRVFB_FID_setInfoProperties                  32
#define IONDRVFB_FID_iterateAllModes                    33
#define IONDRVFB_FID_mapDepthIndex                      34
#define IONDRVFB_FID_getResInfoForDetailed              35
#define IONDRVFB_FID_getResInfoForArbMode               36
#define IONDRVFB_FID_getResInfoForMode                  37
#define IONDRVFB_FID_setDetailedTiming                  38
#define IONDRVFB_FID_validateDisplayMode                39
#define IONDRVFB_FID_getCurrentConfiguration            40
#define IONDRVFB_FID_setupForCurrentConfig              41
#define IONDRVFB_FID_makeSubRange                       42
#define IONDRVFB_FID_getApertureRange                   43
#define IONDRVFB_FID_findVRAM                           44
#define IONDRVFB_FID_getPixelFormats                    45
#define IONDRVFB_FID_getDisplayModeCount                46
#define IONDRVFB_FID_getDisplayModes                    47
#define IONDRVFB_FID_validateDetailedTiming             48
#define IONDRVFB_FID_setDetailedTimings                 49
#define IONDRVFB_FID_getInformationForDisplayMode       50
#define IONDRVFB_FID_getPixelFormatsForDisplayMode      51
#define IONDRVFB_FID_getPixelInformation                52
#define IONDRVFB_FID_getTimingInfoForDisplayMode        53
#define IONDRVFB_FID_getCurrentDisplayMode              54
#define IONDRVFB_FID_setDisplayMode                     55
#define IONDRVFB_FID_setStartupDisplayMode              56
#define IONDRVFB_FID_getStartupDisplayMode              57
#define IONDRVFB_FID_setApertureEnable                  58
#define IONDRVFB_FID_setCLUTWithEntries                 59
#define IONDRVFB_FID_setGammaTable                      60
#define IONDRVFB_FID_setMirror                          61
#define IONDRVFB_FID_setAttribute                       62
#define IONDRVFB_FID_getAttribute                       63
#define IONDRVFB_FID_getConnectionCount                 64
#define IONDRVFB_FID_createI2C                          65
#define IONDRVFB_FID_doI2CRequest                       66
#define IONDRVFB_FID_displayI2CPower                    67
#define IONDRVFB_FID_getOnlineState                     68
#define IONDRVFB_FID_ndrvGetSetFeature                  69
#define IONDRVFB_FID_setConnectionFlags                 70
#define IONDRVFB_FID_setAttributeForConnection          71
#define IONDRVFB_FID_searchOfflineMode                  72
#define IONDRVFB_FID_processConnectChange               73
#define IONDRVFB_FID_getAttributeForConnection          74
#define IONDRVFB_FID_getAppleSense                      75
#define IONDRVFB_FID_connectFlags                       76
#define IONDRVFB_FID_hasDDCConnect                      77
#define IONDRVFB_FID_getDDCBlock                        78
#define IONDRVFB_FID_initForPM                          79
#define IONDRVFB_FID_maxCapabilityForDomainState        80
#define IONDRVFB_FID_initialPowerStateForDomainState    81
#define IONDRVFB_FID_powerStateForDomainState           82
#define IONDRVFB_FID_ndrvSetDisplayPowerState           83
#define IONDRVFB_FID_ndrvUpdatePowerState               84
#define IONDRVFB_FID_ndrvSetPowerState                  85
#define IONDRVFB_FID_setGammaTable2                     86
// IOAccelerator
#define IOA_FID_reserved                                0
#define IOA_FID_createAccelID                           1
#define IOA_FID_retainAccelID                           2
#define IOA_FID_releaseAccelID                          3
// IOAccelerationUserClient
#define IOAUC_FID_reserved                              0
#define IOAUC_FID_initWithTask                          1
#define IOAUC_FID_start                                 2
#define IOAUC_FID_free                                  3
#define IOAUC_FID_clientClose                           4
#define IOAUC_FID_stop                                  5
#define IOAUC_FID_getTargetAndMethodForIndex            6
#define IOAUC_FID_extCreate                             7
#define IOAUC_FID_extDestroy                            8

// Telemetry Macros
/*
 Macro Parameters are:
 param 1: ARIADNE encoded class/subclass
 param 2: IOGraphics Function ID
 param 3: Exit code if function returns a code, else implementation specific
 param 4: Implementation specific.
 param 5: Implementation specific.
 */
// IODisplayWrangler
#define IODW_START(_fID_,args...)                       do{if(gIOGATFlags&TRACE_IODISPLAYWRANGLER){KERNEL_DEBUG_CONSTANT_RELEASE(IODISPLAYWRANGLER_ARIADNE_START,MAKE_FID_DEFINE(IODW,_fID_),## args,0);}}while(0)
#define IODW_END(_fID_,args...)                         do{if(gIOGATFlags&TRACE_IODISPLAYWRANGLER){KERNEL_DEBUG_CONSTANT_RELEASE(IODISPLAYWRANGLER_ARIADNE_END,MAKE_FID_DEFINE(IODW,_fID_),## args,0);}}while(0)

// IODisplay
#define IOD_START(_fID_,args...)                        do{if(gIOGATFlags&TRACE_IODISPLAY){KERNEL_DEBUG_CONSTANT_RELEASE(IODISPLAY_ARIADNE_START,MAKE_FID_DEFINE(IOD,_fID_),## args,0);}}while(0)
#define IOD_END(_fID_,args...)                          do{if(gIOGATFlags&TRACE_IODISPLAY){KERNEL_DEBUG_CONSTANT_RELEASE(IODISPLAY_ARIADNE_END,MAKE_FID_DEFINE(IOD,_fID_),## args,0);}}while(0)

// IODisplayConnect
#define IODC_START(_fID_,args...)                       do{if(gIOGATFlags&TRACE_IODISPLAYCONNECT){KERNEL_DEBUG_CONSTANT_RELEASE(IODISPLAYCONNECT_ARIADNE_START,MAKE_FID_DEFINE(IODC,_fID_),## args,0);}}while(0)
#define IODC_END(_fID_,args...)                         do{if(gIOGATFlags&TRACE_IODISPLAYCONNECT){KERNEL_DEBUG_CONSTANT_RELEASE(IODISPLAYCONNECT_ARIADNE_END,MAKE_FID_DEFINE(IODC,_fID_),## args,0);}}while(0)

// IOFBController
#define IOFBC_START(_fID_,args...)                      do{if(gIOGATFlags&TRACE_IOFBCONTROLLER){KERNEL_DEBUG_CONSTANT_RELEASE(IOFBCONTROLLER_ARIADNE_START,MAKE_FID_DEFINE(IOFBC,_fID_),## args,0);}}while(0)
#define IOFBC_END(_fID_,args...)                        do{if(gIOGATFlags&TRACE_IOFBCONTROLLER){KERNEL_DEBUG_CONSTANT_RELEASE(IOFBCONTROLLER_ARIADNE_END,MAKE_FID_DEFINE(IOFBC,_fID_),## args,0);}}while(0)

// IOFramebuffer
#define IOFB_START(_fID_,args...)                       do{if(gIOGATFlags&TRACE_IOFRAMEBUFFER){KERNEL_DEBUG_CONSTANT_RELEASE(IOFRAMEBUFFER_ARIADNE_START,MAKE_FID_DEFINE(IOFB,_fID_),## args,0);}}while(0)
#define IOFB_END(_fID_,args...)                         do{if(gIOGATFlags&TRACE_IOFRAMEBUFFER){KERNEL_DEBUG_CONSTANT_RELEASE(IOFRAMEBUFFER_ARIADNE_END,MAKE_FID_DEFINE(IOFB,_fID_),## args,0);}}while(0)

// IOFramebufferParameterHandler
#define IOFBPH_START(_fID_,args...)                     do{if(gIOGATFlags&TRACE_IOFRAMEBUFFERPARAMETERHANDLER){KERNEL_DEBUG_CONSTANT_RELEASE(IOFRAMEBUFFERPARAMETERHANDLER_ARIADNE_START,MAKE_FID_DEFINE(IOFBPH,_fID_),## args,0);}}while(0)
#define IOFBPH_END(_fID_,args...)                       do{if(gIOGATFlags&TRACE_IOFRAMEBUFFERPARAMETERHANDLER){KERNEL_DEBUG_CONSTANT_RELEASE(IOFRAMEBUFFERPARAMETERHANDLER_ARIADNE_END,MAKE_FID_DEFINE(IOFBPH,_fID_),## args,0);}}while(0)

// Vendor framebuffer
#define FB_START(_fID_,args...)                         do{if(gIOGATFlags&TRACE_FRAMEBUFFER){KERNEL_DEBUG_CONSTANT_RELEASE(FRAMEBUFFER_ARIADNE_START,MAKE_FID_DEFINE(FB,_fID_),## args,0);}}while(0)
#define FB_END(_fID_,args...)                           do{if(gIOGATFlags&TRACE_FRAMEBUFFER){KERNEL_DEBUG_CONSTANT_RELEASE(FRAMEBUFFER_ARIADNE_END,MAKE_FID_DEFINE(FB,_fID_),## args,0);}}while(0)

// AppleBackLight
#define ABL_START(_fID_,args...)                        do{if(gIOGATFlags&TRACE_APPLEBACKLIGHT){KERNEL_DEBUG_CONSTANT_RELEASE(APPLEBACKLIGHT_ARIADNE_START,MAKE_FID_DEFINE(ABL,_fID_),## args,0);}}while(0)
#define ABL_END(_fID_,args...)                          do{if(gIOGATFlags&TRACE_APPLEBACKLIGHT){KERNEL_DEBUG_CONSTANT_RELEASE(APPLEBACKLIGHT_ARIADNE_END,MAKE_FID_DEFINE(ABL,_fID_),## args,0);}}while(0)

// IOFramebufferUserClient
#define IOFBUC_START(_fID_,args...)                     do{if(gIOGATFlags&TRACE_IOFBUSERCLIENT){KERNEL_DEBUG_CONSTANT_RELEASE(IOFBUSERCLIENT_ARIADNE_START,MAKE_FID_DEFINE(IOFBUC,_fID_),## args,0);}}while(0)
#define IOFBUC_END(_fID_,args...)                       do{if(gIOGATFlags&TRACE_IOFBUSERCLIENT){KERNEL_DEBUG_CONSTANT_RELEASE(IOFBUSERCLIENT_ARIADNE_END,MAKE_FID_DEFINE(IOFBUC,_fID_),## args,0);}}while(0)

// IOFramebufferSharedUserClient
#define IOFBSUC_START(_fID_,args...)                    do{if(gIOGATFlags&TRACE_IOFBSHAREDUSERCLIENT){KERNEL_DEBUG_CONSTANT_RELEASE(IOFBSHAREDUSERCLIENT_ARIADNE_START,MAKE_FID_DEFINE(IOFBSUC,_fID_),## args,0);}}while(0)
#define IOFBSUC_END(_fID_,args...)                      do{if(gIOGATFlags&TRACE_IOFBSHAREDUSERCLIENT){KERNEL_DEBUG_CONSTANT_RELEASE(IOFBSHAREDUSERCLIENT_ARIADNE_END,MAKE_FID_DEFINE(IOFBSUC,_fID_),## args,0);}}while(0)

// IOFramebufferDiagnosticUserClient
#define IOFBDUC_START(_fID_,args...)                    do{if(gIOGATFlags&TRACE_IOFBDIAGNOSTICUSERCLIENT){KERNEL_DEBUG_CONSTANT_RELEASE(IOFBDIAGNOSTICUSERCLIENT_ARIADNE_START,MAKE_FID_DEFINE(IOFBDUC,_fID_),## args,0);}}while(0)
#define IOFBDUC_END(_fID_,args...)                      do{if(gIOGATFlags&TRACE_IOFBDIAGNOSTICUSERCLIENT){KERNEL_DEBUG_CONSTANT_RELEASE(IOFBDIAGNOSTICUSERCLIENT_ARIADNE_END,MAKE_FID_DEFINE(IOFBDUC,_fID_),## args,0);}}while(0)

// IOI2CInterfaceUserClient
#define IOI2CUC_START(_fID_,args...)                    do{if(gIOGATFlags&TRACE_IOI2INTERFACEUSERCLIENT){KERNEL_DEBUG_CONSTANT_RELEASE(IOI2INTERFACEUSERCLIENT_ARIADNE_START,MAKE_FID_DEFINE(IOI2CUC,_fID_),## args,0);}}while(0)
#define IOI2CUC_END(_fID_,args...)                      do{if(gIOGATFlags&TRACE_IOI2INTERFACEUSERCLIENT){KERNEL_DEBUG_CONSTANT_RELEASE(IOI2INTERFACEUSERCLIENT_ARIADNE_END,MAKE_FID_DEFINE(IOI2CUC,_fID_),## args,0);}}while(0)

// IOI2CInterface
#define IOI2C_START(_fID_,args...)                      do{if(gIOGATFlags&TRACE_IOI2INTERFACE){KERNEL_DEBUG_CONSTANT_RELEASE(IOI2INTERFACE_ARIADNE_START,MAKE_FID_DEFINE(IOI2C,_fID_),## args,0);}}while(0)
#define IOI2C_END(_fID_,args...)                        do{if(gIOGATFlags&TRACE_IOI2INTERFACE){KERNEL_DEBUG_CONSTANT_RELEASE(IOI2INTERFACE_ARIADNE_END,MAKE_FID_DEFINE(IOI2C,_fID_),## args,0);}}while(0)

// IOFramebufferI2CInterface
#define IOFBI2C_START(_fID_,args...)                    do{if(gIOGATFlags&TRACE_IOFBI2INTERFACE){KERNEL_DEBUG_CONSTANT_RELEASE(IOFBI2INTERFACE_ARIADNE_START,MAKE_FID_DEFINE(IOFBI2C,_fID_),## args,0);}}while(0)
#define IOFBI2C_END(_fID_,args...)                      do{if(gIOGATFlags&TRACE_IOFBI2INTERFACE){KERNEL_DEBUG_CONSTANT_RELEASE(IOFBI2INTERFACE_ARIADNE_END,MAKE_FID_DEFINE(IOFBI2C,_fID_),## args,0);}}while(0)

// IOBootFramebuffer
#define IOBFB_START(_fID_,args...)                      do{if(gIOGATFlags&TRACE_IOBOOTFRAMEBUFFER){KERNEL_DEBUG_CONSTANT_RELEASE(IOBOOTFRAMEBUFFER_ARIADNE_START,MAKE_FID_DEFINE(IOBFB,_fID_),## args,0);}}while(0)
#define IOBFB_END(_fID_,args...)                        do{if(gIOGATFlags&TRACE_IOBOOTFRAMEBUFFER){KERNEL_DEBUG_CONSTANT_RELEASE(IOBOOTFRAMEBUFFER_ARIADNE_END,MAKE_FID_DEFINE(IOBFB,_fID_),## args,0);}}while(0)

// IOAccelerator
#define IOA_START(_fID_,args...)                        do{if(gIOGATFlags&TRACE_IOACCELERATOR){KERNEL_DEBUG_CONSTANT_RELEASE(IOACCELERATOR_ARIADNE_START,MAKE_FID_DEFINE(IOA,_fID_),## args,0);}}while(0)
#define IOA_END(_fID_,args...)                          do{if(gIOGATFlags&TRACE_IOACCELERATOR){KERNEL_DEBUG_CONSTANT_RELEASE(IOACCELERATOR_ARIADNE_END,MAKE_FID_DEFINE(IOA,_fID_),## args,0);}}while(0)

// IOAcceleratorUserClient
#define IOAUC_START(_fID_,args...)                      do{if(gIOGATFlags&TRACE_IOACCELERATORUSERCLIENT){KERNEL_DEBUG_CONSTANT_RELEASE(IOACCELERATORUSERCLIENT_ARIADNE_START,MAKE_FID_DEFINE(IOAUC,_fID_),## args,0);}}while(0)
#define IOAUC_END(_fID_,args...)                        do{if(gIOGATFlags&TRACE_IOACCELERATORUSERCLIENT){KERNEL_DEBUG_CONSTANT_RELEASE(IOACCELERATORUSERCLIENT_ARIADNE_END,MAKE_FID_DEFINE(IOAUC,_fID_),## args,0);}}while(0)


#if ENABLE_IONDRV_TELEMETRY

// IONDRVFramebuffer
#define IONDRVFB_START(_fID_,args...)                   do{if(gIOGATFlags&TRACE_IONDRVFRAMEBUFFER){KERNEL_DEBUG_CONSTANT_RELEASE(IONDRVFRAMEBUFFER_ARIADNE_START,MAKE_FID_DEFINE(IONDRVFB,_fID_),## args,0);}}while(0)
#define IONDRVFB_END(_fID_,args...)                     do{if(gIOGATFlags&TRACE_IONDRVFRAMEBUFFER){KERNEL_DEBUG_CONSTANT_RELEASE(IONDRVFRAMEBUFFER_ARIADNE_END,MAKE_FID_DEFINE(IONDRVFB,_fID_),## args,0);}}while(0)

#else /* ENABLE_IONDRV_TELEMETRY */

#define IONDRVFB_START(_fID_,args...)
#define IONDRVFB_END(_fID_,args...)

#endif /* ENABLE_IONDRV_TELEMETRY */


#else /* #if ENABLE_TELEMETRY */


#define IODW_START(_fID_,args...)
#define IODW_END(_fID_,args...)                         

#define IOD_START(_fID_,args...)
#define IOD_END(_fID_,args...)                          

#define IODC_START(_fID_,args...)
#define IODC_END(_fID_,args...)

#define IOFBC_START(_fID_,args...)
#define IOFBC_END(_fID_,args...)

#define IOFB_START(_fID_,args...)
#define IOFB_END(_fID_,args...)                         

#define IOFBPH_START(_fID_,args...)
#define IOFBPH_END(_fID_,args...)

#define FB_START(_fID_,args...)
#define FB_END(_fID_,args...)

#define ABL_START(_fID_,args...)
#define ABL_END(_fID_,args...)

#define IOFBUC_START(_fID_,args...)
#define IOFBUC_END(_fID_,args...)

#define IOFBSUC_START(_fID_,args...)
#define IOFBSUC_END(_fID_,args...)

#define IOFBDUC_START(_fID_,args...)
#define IOFBDUC_END(_fID_,args...)

#define IOI2CUC_START(_fID_,args...)
#define IOI2CUC_END(_fID_,args...)

#define IOI2C_START(_fID_,args...)
#define IOI2C_END(_fID_,args...)  

#define IOFBI2C_START(_fID_,args...)  
#define IOFBI2C_END(_fID_,args...)       

#define IOBFB_START(_fID_,args...) 
#define IOBFB_END(_fID_,args...)      

#define IOA_START(_fID_,args...)
#define IOA_END(_fID_,args...)

#define IOAUC_START(_fID_,args...)
#define IOAUC_END(_fID_,args...)

#define IONDRVFB_START(_fID_,args...)
#define IONDRVFB_END(_fID_,args...)


#endif /* #if ENABLE_TELEMETRY */


#endif /* IOGraphicsKTrace_h */

