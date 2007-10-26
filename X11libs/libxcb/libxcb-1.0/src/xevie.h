/*
 * This file generated automatically from xevie.xml by c-client.xsl using XSLT.
 * Edit at your peril.
 */

/**
 * @defgroup XCB_Xevie_API XCB Xevie API
 * @brief Xevie XCB Protocol Implementation.
 * @{
 **/

#ifndef __XEVIE_H
#define __XEVIE_H

#include "xcb.h"

#define XCB_XEVIE_MAJOR_VERSION 1
#define XCB_XEVIE_MINOR_VERSION 0
  
extern xcb_extension_t xcb_xevie_id;

/**
 * @brief xcb_xevie_query_version_cookie_t
 **/
typedef struct xcb_xevie_query_version_cookie_t {
    unsigned int sequence; /**<  */
} xcb_xevie_query_version_cookie_t;

/** Opcode for xcb_xevie_query_version. */
#define XCB_XEVIE_QUERY_VERSION 0

/**
 * @brief xcb_xevie_query_version_request_t
 **/
typedef struct xcb_xevie_query_version_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint16_t client_major_version; /**<  */
    uint16_t client_minor_version; /**<  */
} xcb_xevie_query_version_request_t;

/**
 * @brief xcb_xevie_query_version_reply_t
 **/
typedef struct xcb_xevie_query_version_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint16_t server_major_version; /**<  */
    uint16_t server_minor_version; /**<  */
    uint8_t  pad1[20]; /**<  */
} xcb_xevie_query_version_reply_t;

/**
 * @brief xcb_xevie_start_cookie_t
 **/
typedef struct xcb_xevie_start_cookie_t {
    unsigned int sequence; /**<  */
} xcb_xevie_start_cookie_t;

/** Opcode for xcb_xevie_start. */
#define XCB_XEVIE_START 1

/**
 * @brief xcb_xevie_start_request_t
 **/
typedef struct xcb_xevie_start_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint32_t screen; /**<  */
} xcb_xevie_start_request_t;

/**
 * @brief xcb_xevie_start_reply_t
 **/
typedef struct xcb_xevie_start_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint8_t  pad1[24]; /**<  */
} xcb_xevie_start_reply_t;

/**
 * @brief xcb_xevie_end_cookie_t
 **/
typedef struct xcb_xevie_end_cookie_t {
    unsigned int sequence; /**<  */
} xcb_xevie_end_cookie_t;

/** Opcode for xcb_xevie_end. */
#define XCB_XEVIE_END 2

/**
 * @brief xcb_xevie_end_request_t
 **/
typedef struct xcb_xevie_end_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint32_t cmap; /**<  */
} xcb_xevie_end_request_t;

/**
 * @brief xcb_xevie_end_reply_t
 **/
typedef struct xcb_xevie_end_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint8_t  pad1[24]; /**<  */
} xcb_xevie_end_reply_t;

typedef enum xcb_xevie_datatype_t {
    XCB_XEVIE_DATATYPE_UNMODIFIED,
    XCB_XEVIE_DATATYPE_MODIFIED
} xcb_xevie_datatype_t;

/**
 * @brief xcb_xevie_event_t
 **/
typedef struct xcb_xevie_event_t {
    uint8_t pad0[32]; /**<  */
} xcb_xevie_event_t;

/**
 * @brief xcb_xevie_event_iterator_t
 **/
typedef struct xcb_xevie_event_iterator_t {
    xcb_xevie_event_t *data; /**<  */
    int                rem; /**<  */
    int                index; /**<  */
} xcb_xevie_event_iterator_t;

/**
 * @brief xcb_xevie_send_cookie_t
 **/
typedef struct xcb_xevie_send_cookie_t {
    unsigned int sequence; /**<  */
} xcb_xevie_send_cookie_t;

/** Opcode for xcb_xevie_send. */
#define XCB_XEVIE_SEND 3

/**
 * @brief xcb_xevie_send_request_t
 **/
typedef struct xcb_xevie_send_request_t {
    uint8_t           major_opcode; /**<  */
    uint8_t           minor_opcode; /**<  */
    uint16_t          length; /**<  */
    xcb_xevie_event_t event; /**<  */
    uint32_t          data_type; /**<  */
    uint8_t           pad0[64]; /**<  */
} xcb_xevie_send_request_t;

/**
 * @brief xcb_xevie_send_reply_t
 **/
typedef struct xcb_xevie_send_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint8_t  pad1[24]; /**<  */
} xcb_xevie_send_reply_t;

/**
 * @brief xcb_xevie_select_input_cookie_t
 **/
typedef struct xcb_xevie_select_input_cookie_t {
    unsigned int sequence; /**<  */
} xcb_xevie_select_input_cookie_t;

/** Opcode for xcb_xevie_select_input. */
#define XCB_XEVIE_SELECT_INPUT 4

/**
 * @brief xcb_xevie_select_input_request_t
 **/
typedef struct xcb_xevie_select_input_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint32_t event_mask; /**<  */
} xcb_xevie_select_input_request_t;

/**
 * @brief xcb_xevie_select_input_reply_t
 **/
typedef struct xcb_xevie_select_input_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint8_t  pad1[24]; /**<  */
} xcb_xevie_select_input_reply_t;


/*****************************************************************************
 **
 ** xcb_xevie_query_version_cookie_t xcb_xevie_query_version
 ** 
 ** @param xcb_connection_t *c
 ** @param uint16_t          client_major_version
 ** @param uint16_t          client_minor_version
 ** @returns xcb_xevie_query_version_cookie_t
 **
 *****************************************************************************/
 
xcb_xevie_query_version_cookie_t
xcb_xevie_query_version (xcb_connection_t *c  /**< */,
                         uint16_t          client_major_version  /**< */,
                         uint16_t          client_minor_version  /**< */);


/*****************************************************************************
 **
 ** xcb_xevie_query_version_cookie_t xcb_xevie_query_version_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint16_t          client_major_version
 ** @param uint16_t          client_minor_version
 ** @returns xcb_xevie_query_version_cookie_t
 **
 *****************************************************************************/
 
xcb_xevie_query_version_cookie_t
xcb_xevie_query_version_unchecked (xcb_connection_t *c  /**< */,
                                   uint16_t          client_major_version  /**< */,
                                   uint16_t          client_minor_version  /**< */);


/*****************************************************************************
 **
 ** xcb_xevie_query_version_reply_t * xcb_xevie_query_version_reply
 ** 
 ** @param xcb_connection_t                  *c
 ** @param xcb_xevie_query_version_cookie_t   cookie
 ** @param xcb_generic_error_t              **e
 ** @returns xcb_xevie_query_version_reply_t *
 **
 *****************************************************************************/
 
xcb_xevie_query_version_reply_t *
xcb_xevie_query_version_reply (xcb_connection_t                  *c  /**< */,
                               xcb_xevie_query_version_cookie_t   cookie  /**< */,
                               xcb_generic_error_t              **e  /**< */);


/*****************************************************************************
 **
 ** xcb_xevie_start_cookie_t xcb_xevie_start
 ** 
 ** @param xcb_connection_t *c
 ** @param uint32_t          screen
 ** @returns xcb_xevie_start_cookie_t
 **
 *****************************************************************************/
 
xcb_xevie_start_cookie_t
xcb_xevie_start (xcb_connection_t *c  /**< */,
                 uint32_t          screen  /**< */);


/*****************************************************************************
 **
 ** xcb_xevie_start_cookie_t xcb_xevie_start_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint32_t          screen
 ** @returns xcb_xevie_start_cookie_t
 **
 *****************************************************************************/
 
xcb_xevie_start_cookie_t
xcb_xevie_start_unchecked (xcb_connection_t *c  /**< */,
                           uint32_t          screen  /**< */);


/*****************************************************************************
 **
 ** xcb_xevie_start_reply_t * xcb_xevie_start_reply
 ** 
 ** @param xcb_connection_t          *c
 ** @param xcb_xevie_start_cookie_t   cookie
 ** @param xcb_generic_error_t      **e
 ** @returns xcb_xevie_start_reply_t *
 **
 *****************************************************************************/
 
xcb_xevie_start_reply_t *
xcb_xevie_start_reply (xcb_connection_t          *c  /**< */,
                       xcb_xevie_start_cookie_t   cookie  /**< */,
                       xcb_generic_error_t      **e  /**< */);


/*****************************************************************************
 **
 ** xcb_xevie_end_cookie_t xcb_xevie_end
 ** 
 ** @param xcb_connection_t *c
 ** @param uint32_t          cmap
 ** @returns xcb_xevie_end_cookie_t
 **
 *****************************************************************************/
 
xcb_xevie_end_cookie_t
xcb_xevie_end (xcb_connection_t *c  /**< */,
               uint32_t          cmap  /**< */);


/*****************************************************************************
 **
 ** xcb_xevie_end_cookie_t xcb_xevie_end_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint32_t          cmap
 ** @returns xcb_xevie_end_cookie_t
 **
 *****************************************************************************/
 
xcb_xevie_end_cookie_t
xcb_xevie_end_unchecked (xcb_connection_t *c  /**< */,
                         uint32_t          cmap  /**< */);


/*****************************************************************************
 **
 ** xcb_xevie_end_reply_t * xcb_xevie_end_reply
 ** 
 ** @param xcb_connection_t        *c
 ** @param xcb_xevie_end_cookie_t   cookie
 ** @param xcb_generic_error_t    **e
 ** @returns xcb_xevie_end_reply_t *
 **
 *****************************************************************************/
 
xcb_xevie_end_reply_t *
xcb_xevie_end_reply (xcb_connection_t        *c  /**< */,
                     xcb_xevie_end_cookie_t   cookie  /**< */,
                     xcb_generic_error_t    **e  /**< */);


/*****************************************************************************
 **
 ** void xcb_xevie_event_next
 ** 
 ** @param xcb_xevie_event_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_xevie_event_next (xcb_xevie_event_iterator_t *i  /**< */);


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_xevie_event_end
 ** 
 ** @param xcb_xevie_event_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_xevie_event_end (xcb_xevie_event_iterator_t i  /**< */);


/*****************************************************************************
 **
 ** xcb_xevie_send_cookie_t xcb_xevie_send
 ** 
 ** @param xcb_connection_t  *c
 ** @param xcb_xevie_event_t  event
 ** @param uint32_t           data_type
 ** @returns xcb_xevie_send_cookie_t
 **
 *****************************************************************************/
 
xcb_xevie_send_cookie_t
xcb_xevie_send (xcb_connection_t  *c  /**< */,
                xcb_xevie_event_t  event  /**< */,
                uint32_t           data_type  /**< */);


/*****************************************************************************
 **
 ** xcb_xevie_send_cookie_t xcb_xevie_send_unchecked
 ** 
 ** @param xcb_connection_t  *c
 ** @param xcb_xevie_event_t  event
 ** @param uint32_t           data_type
 ** @returns xcb_xevie_send_cookie_t
 **
 *****************************************************************************/
 
xcb_xevie_send_cookie_t
xcb_xevie_send_unchecked (xcb_connection_t  *c  /**< */,
                          xcb_xevie_event_t  event  /**< */,
                          uint32_t           data_type  /**< */);


/*****************************************************************************
 **
 ** xcb_xevie_send_reply_t * xcb_xevie_send_reply
 ** 
 ** @param xcb_connection_t         *c
 ** @param xcb_xevie_send_cookie_t   cookie
 ** @param xcb_generic_error_t     **e
 ** @returns xcb_xevie_send_reply_t *
 **
 *****************************************************************************/
 
xcb_xevie_send_reply_t *
xcb_xevie_send_reply (xcb_connection_t         *c  /**< */,
                      xcb_xevie_send_cookie_t   cookie  /**< */,
                      xcb_generic_error_t     **e  /**< */);


/*****************************************************************************
 **
 ** xcb_xevie_select_input_cookie_t xcb_xevie_select_input
 ** 
 ** @param xcb_connection_t *c
 ** @param uint32_t          event_mask
 ** @returns xcb_xevie_select_input_cookie_t
 **
 *****************************************************************************/
 
xcb_xevie_select_input_cookie_t
xcb_xevie_select_input (xcb_connection_t *c  /**< */,
                        uint32_t          event_mask  /**< */);


/*****************************************************************************
 **
 ** xcb_xevie_select_input_cookie_t xcb_xevie_select_input_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint32_t          event_mask
 ** @returns xcb_xevie_select_input_cookie_t
 **
 *****************************************************************************/
 
xcb_xevie_select_input_cookie_t
xcb_xevie_select_input_unchecked (xcb_connection_t *c  /**< */,
                                  uint32_t          event_mask  /**< */);


/*****************************************************************************
 **
 ** xcb_xevie_select_input_reply_t * xcb_xevie_select_input_reply
 ** 
 ** @param xcb_connection_t                 *c
 ** @param xcb_xevie_select_input_cookie_t   cookie
 ** @param xcb_generic_error_t             **e
 ** @returns xcb_xevie_select_input_reply_t *
 **
 *****************************************************************************/
 
xcb_xevie_select_input_reply_t *
xcb_xevie_select_input_reply (xcb_connection_t                 *c  /**< */,
                              xcb_xevie_select_input_cookie_t   cookie  /**< */,
                              xcb_generic_error_t             **e  /**< */);


#endif

/**
 * @}
 */
