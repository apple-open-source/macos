/*
 * This file generated automatically from res.xml by c-client.xsl using XSLT.
 * Edit at your peril.
 */

/**
 * @defgroup XCB_Res_API XCB Res API
 * @brief Res XCB Protocol Implementation.
 * @{
 **/

#ifndef __RES_H
#define __RES_H

#include "xcb.h"

#define XCB_RES_MAJOR_VERSION 1
#define XCB_RES_MINOR_VERSION 0
  
extern xcb_extension_t xcb_res_id;

/**
 * @brief xcb_res_client_t
 **/
typedef struct xcb_res_client_t {
    uint32_t resource_base; /**<  */
    uint32_t resource_mask; /**<  */
} xcb_res_client_t;

/**
 * @brief xcb_res_client_iterator_t
 **/
typedef struct xcb_res_client_iterator_t {
    xcb_res_client_t *data; /**<  */
    int               rem; /**<  */
    int               index; /**<  */
} xcb_res_client_iterator_t;

/**
 * @brief xcb_res_type_t
 **/
typedef struct xcb_res_type_t {
    uint32_t resource_type; /**<  */
    uint32_t count; /**<  */
} xcb_res_type_t;

/**
 * @brief xcb_res_type_iterator_t
 **/
typedef struct xcb_res_type_iterator_t {
    xcb_res_type_t *data; /**<  */
    int             rem; /**<  */
    int             index; /**<  */
} xcb_res_type_iterator_t;

/**
 * @brief xcb_res_query_version_cookie_t
 **/
typedef struct xcb_res_query_version_cookie_t {
    unsigned int sequence; /**<  */
} xcb_res_query_version_cookie_t;

/** Opcode for xcb_res_query_version. */
#define XCB_RES_QUERY_VERSION 0

/**
 * @brief xcb_res_query_version_request_t
 **/
typedef struct xcb_res_query_version_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint8_t  client_major; /**<  */
    uint8_t  client_minor; /**<  */
} xcb_res_query_version_request_t;

/**
 * @brief xcb_res_query_version_reply_t
 **/
typedef struct xcb_res_query_version_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint16_t server_major; /**<  */
    uint16_t server_minor; /**<  */
} xcb_res_query_version_reply_t;

/**
 * @brief xcb_res_query_clients_cookie_t
 **/
typedef struct xcb_res_query_clients_cookie_t {
    unsigned int sequence; /**<  */
} xcb_res_query_clients_cookie_t;

/** Opcode for xcb_res_query_clients. */
#define XCB_RES_QUERY_CLIENTS 1

/**
 * @brief xcb_res_query_clients_request_t
 **/
typedef struct xcb_res_query_clients_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
} xcb_res_query_clients_request_t;

/**
 * @brief xcb_res_query_clients_reply_t
 **/
typedef struct xcb_res_query_clients_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint32_t num_clients; /**<  */
    uint8_t  pad1[20]; /**<  */
} xcb_res_query_clients_reply_t;

/**
 * @brief xcb_res_query_client_resources_cookie_t
 **/
typedef struct xcb_res_query_client_resources_cookie_t {
    unsigned int sequence; /**<  */
} xcb_res_query_client_resources_cookie_t;

/** Opcode for xcb_res_query_client_resources. */
#define XCB_RES_QUERY_CLIENT_RESOURCES 2

/**
 * @brief xcb_res_query_client_resources_request_t
 **/
typedef struct xcb_res_query_client_resources_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint32_t xid; /**<  */
} xcb_res_query_client_resources_request_t;

/**
 * @brief xcb_res_query_client_resources_reply_t
 **/
typedef struct xcb_res_query_client_resources_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint32_t num_types; /**<  */
    uint8_t  pad1[20]; /**<  */
} xcb_res_query_client_resources_reply_t;

/**
 * @brief xcb_res_query_client_pixmap_bytes_cookie_t
 **/
typedef struct xcb_res_query_client_pixmap_bytes_cookie_t {
    unsigned int sequence; /**<  */
} xcb_res_query_client_pixmap_bytes_cookie_t;

/** Opcode for xcb_res_query_client_pixmap_bytes. */
#define XCB_RES_QUERY_CLIENT_PIXMAP_BYTES 3

/**
 * @brief xcb_res_query_client_pixmap_bytes_request_t
 **/
typedef struct xcb_res_query_client_pixmap_bytes_request_t {
    uint8_t  major_opcode; /**<  */
    uint8_t  minor_opcode; /**<  */
    uint16_t length; /**<  */
    uint32_t xid; /**<  */
} xcb_res_query_client_pixmap_bytes_request_t;

/**
 * @brief xcb_res_query_client_pixmap_bytes_reply_t
 **/
typedef struct xcb_res_query_client_pixmap_bytes_reply_t {
    uint8_t  response_type; /**<  */
    uint8_t  pad0; /**<  */
    uint16_t sequence; /**<  */
    uint32_t length; /**<  */
    uint32_t bytes; /**<  */
    uint32_t bytes_overflow; /**<  */
} xcb_res_query_client_pixmap_bytes_reply_t;


/*****************************************************************************
 **
 ** void xcb_res_client_next
 ** 
 ** @param xcb_res_client_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_res_client_next (xcb_res_client_iterator_t *i  /**< */);


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_res_client_end
 ** 
 ** @param xcb_res_client_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_res_client_end (xcb_res_client_iterator_t i  /**< */);


/*****************************************************************************
 **
 ** void xcb_res_type_next
 ** 
 ** @param xcb_res_type_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_res_type_next (xcb_res_type_iterator_t *i  /**< */);


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_res_type_end
 ** 
 ** @param xcb_res_type_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_res_type_end (xcb_res_type_iterator_t i  /**< */);


/*****************************************************************************
 **
 ** xcb_res_query_version_cookie_t xcb_res_query_version
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           client_major
 ** @param uint8_t           client_minor
 ** @returns xcb_res_query_version_cookie_t
 **
 *****************************************************************************/
 
xcb_res_query_version_cookie_t
xcb_res_query_version (xcb_connection_t *c  /**< */,
                       uint8_t           client_major  /**< */,
                       uint8_t           client_minor  /**< */);


/*****************************************************************************
 **
 ** xcb_res_query_version_cookie_t xcb_res_query_version_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint8_t           client_major
 ** @param uint8_t           client_minor
 ** @returns xcb_res_query_version_cookie_t
 **
 *****************************************************************************/
 
xcb_res_query_version_cookie_t
xcb_res_query_version_unchecked (xcb_connection_t *c  /**< */,
                                 uint8_t           client_major  /**< */,
                                 uint8_t           client_minor  /**< */);


/*****************************************************************************
 **
 ** xcb_res_query_version_reply_t * xcb_res_query_version_reply
 ** 
 ** @param xcb_connection_t                *c
 ** @param xcb_res_query_version_cookie_t   cookie
 ** @param xcb_generic_error_t            **e
 ** @returns xcb_res_query_version_reply_t *
 **
 *****************************************************************************/
 
xcb_res_query_version_reply_t *
xcb_res_query_version_reply (xcb_connection_t                *c  /**< */,
                             xcb_res_query_version_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */);


/*****************************************************************************
 **
 ** xcb_res_query_clients_cookie_t xcb_res_query_clients
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_res_query_clients_cookie_t
 **
 *****************************************************************************/
 
xcb_res_query_clients_cookie_t
xcb_res_query_clients (xcb_connection_t *c  /**< */);


/*****************************************************************************
 **
 ** xcb_res_query_clients_cookie_t xcb_res_query_clients_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @returns xcb_res_query_clients_cookie_t
 **
 *****************************************************************************/
 
xcb_res_query_clients_cookie_t
xcb_res_query_clients_unchecked (xcb_connection_t *c  /**< */);


/*****************************************************************************
 **
 ** xcb_res_client_t * xcb_res_query_clients_clients
 ** 
 ** @param const xcb_res_query_clients_reply_t *R
 ** @returns xcb_res_client_t *
 **
 *****************************************************************************/
 
xcb_res_client_t *
xcb_res_query_clients_clients (const xcb_res_query_clients_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** int xcb_res_query_clients_clients_length
 ** 
 ** @param const xcb_res_query_clients_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_res_query_clients_clients_length (const xcb_res_query_clients_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_res_client_iterator_t xcb_res_query_clients_clients_iterator
 ** 
 ** @param const xcb_res_query_clients_reply_t *R
 ** @returns xcb_res_client_iterator_t
 **
 *****************************************************************************/
 
xcb_res_client_iterator_t
xcb_res_query_clients_clients_iterator (const xcb_res_query_clients_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_res_query_clients_reply_t * xcb_res_query_clients_reply
 ** 
 ** @param xcb_connection_t                *c
 ** @param xcb_res_query_clients_cookie_t   cookie
 ** @param xcb_generic_error_t            **e
 ** @returns xcb_res_query_clients_reply_t *
 **
 *****************************************************************************/
 
xcb_res_query_clients_reply_t *
xcb_res_query_clients_reply (xcb_connection_t                *c  /**< */,
                             xcb_res_query_clients_cookie_t   cookie  /**< */,
                             xcb_generic_error_t            **e  /**< */);


/*****************************************************************************
 **
 ** xcb_res_query_client_resources_cookie_t xcb_res_query_client_resources
 ** 
 ** @param xcb_connection_t *c
 ** @param uint32_t          xid
 ** @returns xcb_res_query_client_resources_cookie_t
 **
 *****************************************************************************/
 
xcb_res_query_client_resources_cookie_t
xcb_res_query_client_resources (xcb_connection_t *c  /**< */,
                                uint32_t          xid  /**< */);


/*****************************************************************************
 **
 ** xcb_res_query_client_resources_cookie_t xcb_res_query_client_resources_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint32_t          xid
 ** @returns xcb_res_query_client_resources_cookie_t
 **
 *****************************************************************************/
 
xcb_res_query_client_resources_cookie_t
xcb_res_query_client_resources_unchecked (xcb_connection_t *c  /**< */,
                                          uint32_t          xid  /**< */);


/*****************************************************************************
 **
 ** xcb_res_type_t * xcb_res_query_client_resources_types
 ** 
 ** @param const xcb_res_query_client_resources_reply_t *R
 ** @returns xcb_res_type_t *
 **
 *****************************************************************************/
 
xcb_res_type_t *
xcb_res_query_client_resources_types (const xcb_res_query_client_resources_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** int xcb_res_query_client_resources_types_length
 ** 
 ** @param const xcb_res_query_client_resources_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_res_query_client_resources_types_length (const xcb_res_query_client_resources_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_res_type_iterator_t xcb_res_query_client_resources_types_iterator
 ** 
 ** @param const xcb_res_query_client_resources_reply_t *R
 ** @returns xcb_res_type_iterator_t
 **
 *****************************************************************************/
 
xcb_res_type_iterator_t
xcb_res_query_client_resources_types_iterator (const xcb_res_query_client_resources_reply_t *R  /**< */);


/*****************************************************************************
 **
 ** xcb_res_query_client_resources_reply_t * xcb_res_query_client_resources_reply
 ** 
 ** @param xcb_connection_t                         *c
 ** @param xcb_res_query_client_resources_cookie_t   cookie
 ** @param xcb_generic_error_t                     **e
 ** @returns xcb_res_query_client_resources_reply_t *
 **
 *****************************************************************************/
 
xcb_res_query_client_resources_reply_t *
xcb_res_query_client_resources_reply (xcb_connection_t                         *c  /**< */,
                                      xcb_res_query_client_resources_cookie_t   cookie  /**< */,
                                      xcb_generic_error_t                     **e  /**< */);


/*****************************************************************************
 **
 ** xcb_res_query_client_pixmap_bytes_cookie_t xcb_res_query_client_pixmap_bytes
 ** 
 ** @param xcb_connection_t *c
 ** @param uint32_t          xid
 ** @returns xcb_res_query_client_pixmap_bytes_cookie_t
 **
 *****************************************************************************/
 
xcb_res_query_client_pixmap_bytes_cookie_t
xcb_res_query_client_pixmap_bytes (xcb_connection_t *c  /**< */,
                                   uint32_t          xid  /**< */);


/*****************************************************************************
 **
 ** xcb_res_query_client_pixmap_bytes_cookie_t xcb_res_query_client_pixmap_bytes_unchecked
 ** 
 ** @param xcb_connection_t *c
 ** @param uint32_t          xid
 ** @returns xcb_res_query_client_pixmap_bytes_cookie_t
 **
 *****************************************************************************/
 
xcb_res_query_client_pixmap_bytes_cookie_t
xcb_res_query_client_pixmap_bytes_unchecked (xcb_connection_t *c  /**< */,
                                             uint32_t          xid  /**< */);


/*****************************************************************************
 **
 ** xcb_res_query_client_pixmap_bytes_reply_t * xcb_res_query_client_pixmap_bytes_reply
 ** 
 ** @param xcb_connection_t                            *c
 ** @param xcb_res_query_client_pixmap_bytes_cookie_t   cookie
 ** @param xcb_generic_error_t                        **e
 ** @returns xcb_res_query_client_pixmap_bytes_reply_t *
 **
 *****************************************************************************/
 
xcb_res_query_client_pixmap_bytes_reply_t *
xcb_res_query_client_pixmap_bytes_reply (xcb_connection_t                            *c  /**< */,
                                         xcb_res_query_client_pixmap_bytes_cookie_t   cookie  /**< */,
                                         xcb_generic_error_t                        **e  /**< */);


#endif

/**
 * @}
 */
