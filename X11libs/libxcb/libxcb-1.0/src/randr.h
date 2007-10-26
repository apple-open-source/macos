/*
 * This file generated automatically from randr.xml by c-client.xsl using XSLT.
 * Edit at your peril.
 */

/**
 * @defgroup XCB_RandR_API XCB RandR API
 * @brief RandR XCB Protocol Implementation.
 * @{
 **/

#ifndef __RANDR_H
#define __RANDR_H

#include "xcb.h"
#include "xproto.h"

#define XCB_RANDR_MAJOR_VERSION 1
#define XCB_RANDR_MINOR_VERSION 1
  
extern xcb_extension_t xcb_randr_id;

typedef enum xcb_randr_rotation_t {
    XCB_RANDR_ROTATION_ROTATE_0 = 1,
    XCB_RANDR_ROTATION_ROTATE_90 = 2,
    XCB_RANDR_ROTATION_ROTATE_180 = 4,
    XCB_RANDR_ROTATION_ROTATE_270 = 8,
    XCB_RANDR_ROTATION_REFLECT_X = 16,
    XCB_RANDR_ROTATION_REFLECT_Y = 32
} xcb_randr_rotation_t;

/**
 * @brief xcb_randr_screen_size_t
 **/
typedef struct xcb_randr_screen_size_t {
    int16_t width; /**<  */
    int16_t height; /**<  */
    int16_t mwidth; /**<  */
    int16_t mheight; /**<  */
} xcb_randr_screen_size_t;

/**
 * @brief xcb_randr_screen_size_iterator_t
 **/
typedef struct xcb_randr_screen_size_iterator_t {
    xcb_randr_screen_size_t *data; /**<  */
    int                      rem; /**<  */
    int                      index; /**<  */
} xcb_randr_screen_size_iterator_t;

/**
 * @brief xcb_randr_refresh_rates_t
 **/
typedef struct xcb_randr_refresh_rates_t {
    uint16_t nRates; /**<  */
} xcb_randr_refresh_rates_t;

/**
 * @brief xcb_randr_refresh_rates_iterator_t
 **/
typedef struct xcb_randr_refresh_rates_iterator_t {
    xcb_randr_refresh_rates_t *data; /**<  */
    int                        rem; /**<  */
    int                        index; /**<  */
} xcb_randr_refresh_rates_iterator_t;

/**
 * @brief xcb_randr_query_version_cookie_t
 **/
typedef struct xcb_randr_query_version_cookie_t {
    unsigned int sequence; /**<  */
} xcb_randr_query_version_cookie_t;

/** Opcode for xcb_randr_query_version. */
#define XCB_RANDR_QUERY_VERSION 0

/**
 * @brief xcb_randr_query_version_request_t
 **/
typedef struct xcb_randr_query_version_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint32_t major_version; /**<  */
    uint32_t minor_version; /**<  */
} xcb_randr_query_version_request_t;

/**
 * @brief xcb_randr_query_version_reply_t
 **/
typedef struct xcb_randr_query_version_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint32_t major_version; /**<  */
    uint32_t minor_version; /**<  */
    uint8_t  pad1[16]; /**<  */
} xcb_randr_query_version_reply_t;

/**
 * @brief xcb_randr_set_screen_config_cookie_t
 **/
typedef struct xcb_randr_set_screen_config_cookie_t {
    unsigned int sequence; /**<  */
} xcb_randr_set_screen_config_cookie_t;

/** Opcode for xcb_randr_set_screen_config. */
#define XCB_RANDR_SET_SCREEN_CONFIG 2

/**
 * @brief xcb_randr_set_screen_config_request_t
 **/
typedef struct xcb_randr_set_screen_config_request_t {
    uint8_t         major_opcode; /**<  */
    uint8_t         minor_opcode; /**<  */
    uint16_t        length; /**<  */
    xcb_drawable_t  drawable; /**<  */
    xcb_timestamp_t timestamp; /**<  */
    xcb_timestamp_t config_timestamp; /**<  */
    uint16_t        sizeID; /**<  */
    int16_t         rotation; /**<  */
    uint16_t        rate; /**<  */
    uint8_t         pad0[2]; /**<  */
} xcb_randr_set_screen_config_request_t;

/**
 * @brief xcb_randr_set_screen_config_reply_t
 **/
typedef struct xcb_randr_set_screen_config_reply_t {
    uint8_t         response_type; /**<  */
    uint8_t         status; /**<  */
    uint16_t        sequence; /**<  */
    uint32_t        length; /**<  */
    xcb_timestamp_t new_timestamp; /**<  */
    xcb_timestamp_t config_timestamp; /**<  */
    xcb_window_t    root; /**<  */
    uint16_t        subpixel_order; /**<  */
    uint8_t         pad0[10]; /**<  */
} xcb_randr_set_screen_config_reply_t;

typedef enum xcb_randr_set_config_t {
    XCB_RANDR_SET_CONFIG_SUCCESS = 0,
    XCB_RANDR_SET_CONFIG_INVALID_CONFIG_TIME = 1,
    XCB_RANDR_SET_CONFIG_INVALID_TIME = 2,
    XCB_RANDR_SET_CONFIG_FAILED = 3
} xcb_randr_set_config_t;

/** Opcode for xcb_randr_select_input. */
#define XCB_RANDR_SELECT_INPUT 4

/**
 * @brief xcb_randr_select_input_request_t
 **/
typedef struct xcb_randr_select_input_request_t {
    uint8_t      major_opcode; /**<  */
    uint8_t      minor_opcode; /**<  */
    uint16_t     length; /**<  */
    xcb_window_t window; /**<  */
    uint16_t     enable; /**<  */
    uint8_t      pad0[2]; /**<  */
} xcb_randr_select_input_request_t;

/**
 * @brief xcb_randr_get_screen_info_cookie_t
 **/
typedef struct xcb_randr_get_screen_info_cookie_t {
    unsigned int sequence; /**<  */
} xcb_randr_get_screen_info_cookie_t;

/** Opcode for xcb_randr_get_screen_info. */
#define XCB_RANDR_GET_SCREEN_INFO 5

/**
 * @brief xcb_randr_get_screen_info_request_t
 **/
typedef struct xcb_randr_get_screen_info_request_t {
    uint8_t      major_opcode; /**<  */
    uint8_t      minor_opcode; /**<  */
    uint16_t     length; /**<  */
    xcb_window_t window; /**<  */
} xcb_randr_get_screen_info_request_t;

/**
 * @brief xcb_randr_get_screen_info_reply_t
 **/
typedef struct xcb_randr_get_screen_info_reply_t {
    uint8_t         response_type; /**<  */
    uint8_t         rotations; /**<  */
    uint16_t        sequence; /**<  */
    uint32_t        length; /**<  */
    xcb_window_t    root; /**<  */
    xcb_timestamp_t timestamp; /**<  */
    xcb_timestamp_t config_timestamp; /**<  */
    uint16_t        nSizes; /**<  */
    uint16_t        sizeID; /**<  */
    int16_t         rotation; /**<  */
    uint16_t        rate; /**<  */
    uint16_t        nInfo; /**<  */
    uint8_t         pad0[2]; /**<  */
} xcb_randr_get_screen_info_reply_t;

typedef enum xcb_randr_sm_t {
    XCB_RANDR_SM_SCREEN_CHANGE_NOTIFY = 1
} xcb_randr_sm_t;

/** Opcode for xcb_randr_screen_change_notify. */
#define XCB_RANDR_SCREEN_CHANGE_NOTIFY 0

/**
 * @brief xcb_randr_screen_change_notify_event_t
 **/
typedef struct xcb_randr_screen_change_notify_event_t {
    uint8_t         response_type; /**<  */
    uint8_t         rotation; /**<  */
    uint16_t        sequence; /**<  */
    xcb_timestamp_t timestamp; /**<  */
    xcb_timestamp_t config_timestamp; /**<  */
    xcb_window_t    root; /**<  */
    xcb_window_t    request_window; /**<  */
    uint16_t        sizeID; /**<  */
    uint16_t        subpixel_order; /**<  */
    int16_t         width; /**<  */
    int16_t         height; /**<  */
    int16_t         mwidth; /**<  */
    int16_t         mheight; /**<  */
} xcb_randr_screen_change_notify_event_t;


/*****************************************************************************
 **
 ** void xcb_randr_screen_size_next
 ** 
 ** @param xcb_randr_screen_size_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_randr_screen_size_next (xcb_randr_screen_size_iterator_t *i  /**< */);


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_randr_screen_size_end
 ** 
 ** @param xcb_randr_screen_size_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_randr_screen_size_end (xcb_randr_screen_size_iterator_t i  /**< */);


/*****************************************************************************
 **
 ** uint16_t * xcb_randr_refresh_rates_rates
 ** 
 ** @param const xcb_randr_refresh_rates_t *R
 ** @returns uint16_t *
 **
 *****************************************************************************/
 
uint16_t *
xcb_randr_refresh_rates_rates (const xcb_randr_refresh_rates_t *R  /**< */);


/*****************************************************************************
 **
 ** int xcb_randr_refresh_rates_rates_length
 ** 
 ** @param const xcb_randr_refresh_rates_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_randr_refresh_rates_rates_length (const xcb_randr_refresh_rates_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_randr_refresh_rates_rates_end
 ** 
 ** @param const xcb_randr_refresh_rates_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_randr_refresh_rates_rates_end (const xcb_randr_refresh_rates_t *R  /**< */);


/*****************************************************************************
 **
 ** void xcb_randr_refresh_rates_next
 ** 
 ** @param xcb_randr_refresh_rates_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_randr_refresh_rates_next (xcb_randr_refresh_rates_iterator_t *i  /**< */);


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_randr_refresh_rates_end
 ** 
 ** @param xcb_randr_refresh_rates_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_randr_refresh_rates_end (xcb_randr_refresh_rates_iterator_t i  /**< */);


/*****************************************************************************
 **
 ** xcb_randr_query_version_cookie_t xcb_randr_query_version
 ** 
 ** @param xcb_connection_t *c
 ** @param uint32_t          major_version
 ** @param uint32_t          minor_version
 ** @returns xcb_randr_query_version_cookie_t
 **
 *****************************************************************************/
 
xcb_randr_query_version_cookie_t
xcb_randr_query_version (xcb_connection_t *c  /**< */,
                         uint32_t          major_version  /**< */,
                         uint32_t          minor_version  /**< */);


/*****************************************************************************
 **
 ** xcb_randr_query_version_cookie_t xcb_randr_query_version_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint32_t          major_version
 ** @param uint32_t          minor_version
 ** @returns xcb_randr_query_version_cookie_t
 **
 *****************************************************************************/
 
xcb_randr_query_version_cookie_t
xcb_randr_query_version_unchecked (xcb_connection_t *c  /**< */,
                                   uint32_t          major_version  /**< */,
                                   uint32_t          minor_version  /**< */);


/*****************************************************************************
 **
 ** xcb_randr_query_version_reply_t * xcb_randr_query_version_reply
 ** 
 ** @param xcb_connection_t                  *c
 ** @param xcb_randr_query_version_cookie_t   cookie
 ** @param xcb_generic_error_t              **e
 ** @returns xcb_randr_query_version_reply_t *
 **
 *****************************************************************************/
 
xcb_randr_query_version_reply_t *
xcb_randr_query_version_reply (xcb_connection_t                  *c  /**< */,
                               xcb_randr_query_version_cookie_t   cookie  /**< */,
                               xcb_generic_error_t              **e  /**< */);


/*****************************************************************************
 **
 ** xcb_randr_set_screen_config_cookie_t xcb_randr_set_screen_config
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_drawable_t    drawable
 ** @param xcb_timestamp_t   timestamp
 ** @param xcb_timestamp_t   config_timestamp
 ** @param uint16_t          sizeID
 ** @param int16_t           rotation
 ** @param uint16_t          rate
 ** @returns xcb_randr_set_screen_config_cookie_t
 **
 *****************************************************************************/
 
xcb_randr_set_screen_config_cookie_t
xcb_randr_set_screen_config (xcb_connection_t *c  /**< */,
                             xcb_drawable_t    drawable  /**< */,
                             xcb_timestamp_t   timestamp  /**< */,
                             xcb_timestamp_t   config_timestamp  /**< */,
                             uint16_t          sizeID  /**< */,
                             int16_t           rotation  /**< */,
                             uint16_t          rate  /**< */);


/*****************************************************************************
 **
 ** xcb_randr_set_screen_config_cookie_t xcb_randr_set_screen_config_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_drawable_t    drawable
 ** @param xcb_timestamp_t   timestamp
 ** @param xcb_timestamp_t   config_timestamp
 ** @param uint16_t          sizeID
 ** @param int16_t           rotation
 ** @param uint16_t          rate
 ** @returns xcb_randr_set_screen_config_cookie_t
 **
 *****************************************************************************/
 
xcb_randr_set_screen_config_cookie_t
xcb_randr_set_screen_config_unchecked (xcb_connection_t *c  /**< */,
                                       xcb_drawable_t    drawable  /**< */,
                                       xcb_timestamp_t   timestamp  /**< */,
                                       xcb_timestamp_t   config_timestamp  /**< */,
                                       uint16_t          sizeID  /**< */,
                                       int16_t           rotation  /**< */,
                                       uint16_t          rate  /**< */);


/*****************************************************************************
 **
 ** xcb_randr_set_screen_config_reply_t * xcb_randr_set_screen_config_reply
 ** 
 ** @param xcb_connection_t                      *c
 ** @param xcb_randr_set_screen_config_cookie_t   cookie
 ** @param xcb_generic_error_t                  **e
 ** @returns xcb_randr_set_screen_config_reply_t *
 **
 *****************************************************************************/
 
xcb_randr_set_screen_config_reply_t *
xcb_randr_set_screen_config_reply (xcb_connection_t                      *c  /**< */,
                                   xcb_randr_set_screen_config_cookie_t   cookie  /**< */,
                                   xcb_generic_error_t                  **e  /**< */);


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_randr_select_input_checked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @param uint16_t          enable
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_randr_select_input_checked (xcb_connection_t *c  /**< */,
                                xcb_window_t      window  /**< */,
                                uint16_t          enable  /**< */);


/*****************************************************************************
 **
 ** xcb_void_cookie_t xcb_randr_select_input
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @param uint16_t          enable
 ** @returns xcb_void_cookie_t
 **
 *****************************************************************************/
 
xcb_void_cookie_t
xcb_randr_select_input (xcb_connection_t *c  /**< */,
                        xcb_window_t      window  /**< */,
                        uint16_t          enable  /**< */);


/*****************************************************************************
 **
 ** xcb_randr_get_screen_info_cookie_t xcb_randr_get_screen_info
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_randr_get_screen_info_cookie_t
 **
 *****************************************************************************/
 
xcb_randr_get_screen_info_cookie_t
xcb_randr_get_screen_info (xcb_connection_t *c  /**< */,
                           xcb_window_t      window  /**< */);


/*****************************************************************************
 **
 ** xcb_randr_get_screen_info_cookie_t xcb_randr_get_screen_info_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param xcb_window_t      window
 ** @returns xcb_randr_get_screen_info_cookie_t
 **
 *****************************************************************************/
 
xcb_randr_get_screen_info_cookie_t
xcb_randr_get_screen_info_unchecked (xcb_connection_t *c  /**< */,
                                     xcb_window_t      window  /**< */);


/*****************************************************************************
 **
 ** xcb_randr_screen_size_t * xcb_randr_get_screen_info_sizes
 ** 
 ** @param const xcb_randr_get_screen_info_reply_t *R
 ** @returns xcb_randr_screen_size_t *
 **
 *****************************************************************************/
 
xcb_randr_screen_size_t *
xcb_randr_get_screen_info_sizes (const xcb_randr_get_screen_info_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** int xcb_randr_get_screen_info_sizes_length
 ** 
 ** @param const xcb_randr_get_screen_info_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_randr_get_screen_info_sizes_length (const xcb_randr_get_screen_info_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_randr_screen_size_iterator_t xcb_randr_get_screen_info_sizes_iterator
 ** 
 ** @param const xcb_randr_get_screen_info_reply_t *R
 ** @returns xcb_randr_screen_size_iterator_t
 **
 *****************************************************************************/
 
xcb_randr_screen_size_iterator_t
xcb_randr_get_screen_info_sizes_iterator (const xcb_randr_get_screen_info_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** int xcb_randr_get_screen_info_rates_length
 ** 
 ** @param const xcb_randr_get_screen_info_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_randr_get_screen_info_rates_length (const xcb_randr_get_screen_info_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_randr_refresh_rates_iterator_t xcb_randr_get_screen_info_rates_iterator
 ** 
 ** @param const xcb_randr_get_screen_info_reply_t *R
 ** @returns xcb_randr_refresh_rates_iterator_t
 **
 *****************************************************************************/
 
xcb_randr_refresh_rates_iterator_t
xcb_randr_get_screen_info_rates_iterator (const xcb_randr_get_screen_info_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_randr_get_screen_info_reply_t * xcb_randr_get_screen_info_reply
 ** 
 ** @param xcb_connection_t                    *c
 ** @param xcb_randr_get_screen_info_cookie_t   cookie
 ** @param xcb_generic_error_t                **e
 ** @returns xcb_randr_get_screen_info_reply_t *
 **
 *****************************************************************************/
 
xcb_randr_get_screen_info_reply_t *
xcb_randr_get_screen_info_reply (xcb_connection_t                    *c  /**< */,
                                 xcb_randr_get_screen_info_cookie_t   cookie  /**< */,
                                 xcb_generic_error_t                **e  /**< */);


#endif

/**
 * @}
 */
