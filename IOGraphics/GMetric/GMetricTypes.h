/*
 * CONFIDENTIAL
 * Copyright (c) 2017-2018 Apple Inc. as an unpublished work.
 * All Rights Reserved.
 */
//  Created by Jérémy Tran on 2017-08-16

#ifndef GMetricTypes_h
#define GMetricTypes_h

#include <stdint.h>
#include <os/base.h>
#include <sys/kdebug.h>

#define kGMetricMaximumLineCount        8192 // @64b == 512k
#define kGMetricDefaultLineCount        2048 // @64b == 128K

/*!
 gmetric_command_t
 */
OS_ENUM(gmetric_command, uint64_t,
    kGMetricCmdEnable                   = 'Enbl',
    kGMetricCmdDisable                  = 'Dsbl',
    kGMetricCmdStart                    = 'Strt',
    kGMetricCmdStop                     = 'Stop',
    kGMetricCmdReset                    = 'Rset',
    kGMetricCmdFetch                    = 'Ftch',
);

/*!
 gmetric_event_t
 */
OS_ENUM(gmetric_event, uint8_t,
    kGMETRICS_EVENT_SIGNAL              = 0b00, // DBG_FUNC_NONE
    kGMETRICS_EVENT_START               = 0b01, // DBG_FUNC_START
    kGMETRICS_EVENT_END                 = 0b10, // DBG_FUNC_END
    kGMETRICS_EVENT_ERROR               = 0b11,
);
_Static_assert(kGMETRICS_EVENT_SIGNAL == DBG_FUNC_NONE, "Enum value mismatch");
_Static_assert(kGMETRICS_EVENT_START == DBG_FUNC_START, "Enum value mismatch");
_Static_assert(kGMETRICS_EVENT_END == DBG_FUNC_END, "Enum value mismatch");

/*!
 gmetric_marker_t
 */
OS_ENUM(gmetric_marker, uint8_t,
    kGMETRICS_MARKER_NONE               = 0,
    kGMETRICS_MARKER_SLEEP,
    kGMETRICS_MARKER_ENTERING_SLEEP,
    kGMETRICS_MARKER_EXITING_SLEEP,
    kGMETRICS_MARKER_DOZE,
    kGMETRICS_MARKER_DARKWAKE,
    kGMETRICS_MARKER_WAKE,
    kGMETRICS_MARKER_ENTERING_WAKE,
    kGMETRICS_MARKER_EXITING_WAKE,
    kGMETRICS_MARKER_BOOT
);

/*!
 gmetric_domain_t
 */
OS_ENUM(gmetric_domain, uint64_t,
    kGMETRICS_DOMAIN_NONE               = UINT64_C(0),
    kGMETRICS_DOMAIN_GENERAL            = UINT64_C(1) <<  0,  // General - non specific
    kGMETRICS_DOMAIN_SYSTEM             = UINT64_C(1) <<  1,  // SystWork
    kGMETRICS_DOMAIN_GPUWRANGLER        = UINT64_C(1) <<  2,  // Future
    kGMETRICS_DOMAIN_AGC                = UINT64_C(1) <<  3,  // MUX
    kGMETRICS_DOMAIN_DISPLAYWRANGLER    = UINT64_C(1) <<  4,  // IODisplayWrangler
    kGMETRICS_DOMAIN_FBCONBTROLLER      = UINT64_C(1) <<  5,  // FBController
    kGMETRICS_DOMAIN_FRAMEBUFFER        = UINT64_C(1) <<  6,  // IOFramebuffer
    kGMETRICS_DOMAIN_NDRV               = UINT64_C(1) <<  7,  // IONDRVFramebuffer
    kGMETRICS_DOMAIN_VENDOR             = UINT64_C(1) <<  8,  // down call to vendor
    kGMETRICS_DOMAIN_IODISPLAY          = UINT64_C(1) <<  9,  // IODisplay
    kGMETRICS_DOMAIN_IODISPLAYPH        = UINT64_C(1) << 10,  // IODisplayParameterHandler
    kGMETRICS_DOMAIN_BACKLIGHT          = UINT64_C(1) << 11,  // IOBacklightDisplay
    kGMETRICS_DOMAIN_BOOTFB             = UINT64_C(1) << 12,  // BootFramebuffer
    kGMETRICS_DOMAIN_I2C                = UINT64_C(1) << 13,  // IOI2C
    kGMETRICS_DOMAIN_USERCLIENT         = UINT64_C(1) << 14,  // IOUserClient
    kGMETRICS_DOMAIN_CONSOLE            = UINT64_C(1) << 15,  // Console
    kGMETRICS_DOMAIN_DISPLAYMODE        = UINT64_C(1) << 16,  // Display Mode
    kGMETRICS_DOMAIN_GAMMA              = UINT64_C(1) << 17,  // Gamma
    kGMETRICS_DOMAIN_DISPLAYPORT        = UINT64_C(1) << 18,  // DP
    kGMETRICS_DOMAIN_CONNECTION         = UINT64_C(1) << 19,  // Connection probe
    kGMETRICS_DOMAIN_DARKWAKE           = UINT64_C(1) << 20,  // Darkwake
    kGMETRICS_DOMAIN_WAKE               = UINT64_C(1) << 21,  // Wake
    kGMETRICS_DOMAIN_DOZE               = UINT64_C(1) << 22,  // Doze
    kGMETRICS_DOMAIN_SLEEP              = UINT64_C(1) << 23,  // Sleep
    kGMETRICS_DOMAIN_HIBERNATE          = UINT64_C(1) << 24,  // Hibernate
    kGMETRICS_DOMAIN_CLAMSHELL          = UINT64_C(1) << 25,  // Clamshell
    kGMETRICS_DOMAIN_POWER              = UINT64_C(1) << 26,  // Power
    kGMETRICS_DOMAIN_INITIALIZATION     = UINT64_C(1) << 27,  // Start/Init
    kGMETRICS_DOMAIN_TERMINATION        = UINT64_C(1) << 28,  // Stop/Terminate

    kGMETRICS_DOMAIN_ALL                = (UINT64_C(1) << 47)-1, // 47 bits

    // Flag indicating that GMetric system is enabled, set other domain bits to
    // initiate recording.
    kGMETRICS_DOMAIN_ENABLED            = kGMETRICS_DOMAIN_ALL+1
);

#define GMETRIC_DOMAIN_FROM_POWER_STATE(_state_, _prevState_) ({ \
    const auto _gm_dps_state_ = _state_; \
    const auto _gm_dps_prev_ = _prevState_; \
    gmetric_domain_t _gm_dps_ret_ = kGMETRICS_DOMAIN_NONE; \
    if (_gm_dps_state_ == 0) \
        _gm_dps_ret_ = kGMETRICS_DOMAIN_SLEEP; \
    else if (_gm_dps_state_ == 2) \
        _gm_dps_ret_ = kGMETRICS_DOMAIN_WAKE; \
    else if (_gm_dps_state_ != 1) \
        _gm_dps_ret_ = kGMETRICS_DOMAIN_NONE; \
    else if (_gm_dps_prev_ > _gm_dps_state_) /* state == 1 & prev > cur */ \
        _gm_dps_ret_ = kGMETRICS_DOMAIN_DOZE; \
    else                                     /* state == 1 & prev <= cur */ \
        _gm_dps_ret_ = kGMETRICS_DOMAIN_DARKWAKE; \
    _gm_dps_ret_; \
})

// TODO(gvdl): Copy and paste error waiting to happen, fix me.
#define GMETRIC_MARKER_FROM_POWER_STATE(_state_, _prevState_) ({ \
    const auto _gm_mps_state_ = _state_; \
    const auto _gm_mps_prev_ = _prevState_; \
    gmetric_marker_t _gm_mps_ret_ = kGMETRICS_MARKER_NONE; \
    if (_gm_mps_state_ == 0) \
        _gm_mps_ret_ = kGMETRICS_MARKER_SLEEP; \
    else if (_gm_mps_state_ == 2) \
        _gm_mps_ret_ = kGMETRICS_MARKER_WAKE; \
    else if (_gm_mps_state_ != 1) \
        _gm_mps_ret_ = kGMETRICS_MARKER_NONE; \
    else if (_gm_mps_prev_ > _gm_mps_state_) /* state == 1 & prev > cur */ \
        _gm_mps_ret_ = kGMETRICS_MARKER_DOZE; \
    else                                     /* state == 1 & prev <= cur */ \
        _gm_mps_ret_ = kGMETRICS_MARKER_DARKWAKE; \
    _gm_mps_ret_; \
})

#define GMETRIC_FUNC_FROM_DATA(_data_)     ((_data_ >> 16) & 0xFFFF)
#define GMETRIC_DATA_FROM_FUNC(_func_)     ((_func_ & 0xFFFF) << 16)

#define GMETRIC_MARKER_FROM_DATA(_data_)   (_data_   & 0xFFFF)
#define GMETRIC_DATA_FROM_MARKER(_marker_) (_marker_ & 0xFFFF)

#pragma pack(push, 1)
typedef struct {
    uint64_t    type:2;     // gmetric_event_t
    uint64_t    cpu:14;     // CPU number
    uint64_t    domain:48;  // gmetric_domain_t
} gmetric_header_t;

typedef struct {
    gmetric_header_t        header;
    uint64_t                tid;        // Thread ID
    uint64_t                timestamp;  // Event timestamp
    uint64_t                data;       // Type-dependent data.
} gmetric_entry_t;

typedef struct {
    uint32_t fEntriesCount;
    uint32_t fCopiedCount;
} gmetric_buffer_header_t;
#pragma pack(pop)

/*!
 @struct gmetric_buffer_t

 @abstract
 Static buffer of gmetric_entry_t data.

 @discussion
 This structure should be used when using the UserClient to retrieve the metrics
 buffer.

 While the buffer passed in will be filled as much as possible, it is possible
 that the length is insufficient to return all of the data. In which case the
 fEntriesCount field will be returned and a new buffer can be provided.
 */
typedef struct {
    union {
        gmetric_buffer_header_t fHeader;
        gmetric_entry_t         fPadding;
    };
    gmetric_entry_t             fEntries[];
} gmetric_buffer_t;

#endif /* GMetricTypes_h */
