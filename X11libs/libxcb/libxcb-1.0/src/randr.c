/*
 * This file generated automatically from randr.xml by c-client.xsl using XSLT.
 * Edit at your peril.
 */

#include <string.h>
#include <assert.h>
#include "xcbext.h"
#include "randr.h"

xcb_extension_t xcb_randr_id = { "RANDR" };


/*****************************************************************************
 **
 ** void xcb_randr_screen_size_next
 ** 
 ** @param xcb_randr_screen_size_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_randr_screen_size_next (xcb_randr_screen_size_iterator_t *i  /**< */)
{
    --i->rem;
    ++i->data;
    i->index += sizeof(xcb_randr_screen_size_t);
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_randr_screen_size_end
 ** 
 ** @param xcb_randr_screen_size_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_randr_screen_size_end (xcb_randr_screen_size_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    ret.data = i.data + i.rem;
    ret.index = i.index + ((char *) ret.data - (char *) i.data);
    ret.rem = 0;
    return ret;
}


/*****************************************************************************
 **
 ** uint16_t * xcb_randr_refresh_rates_rates
 ** 
 ** @param const xcb_randr_refresh_rates_t *R
 ** @returns uint16_t *
 **
 *****************************************************************************/
 
uint16_t *
xcb_randr_refresh_rates_rates (const xcb_randr_refresh_rates_t *R  /**< */)
{
    return (uint16_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_randr_refresh_rates_rates_length
 ** 
 ** @param const xcb_randr_refresh_rates_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_randr_refresh_rates_rates_length (const xcb_randr_refresh_rates_t *R  /**< */)
{
    return R->nRates;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_randr_refresh_rates_rates_end
 ** 
 ** @param const xcb_randr_refresh_rates_t *R
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_randr_refresh_rates_rates_end (const xcb_randr_refresh_rates_t *R  /**< */)
{
    xcb_generic_iterator_t i;
    i.data = ((uint16_t *) (R + 1)) + (R->nRates);
    i.rem = 0;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** void xcb_randr_refresh_rates_next
 ** 
 ** @param xcb_randr_refresh_rates_iterator_t *i
 ** @returns void
 **
 *****************************************************************************/
 
void
xcb_randr_refresh_rates_next (xcb_randr_refresh_rates_iterator_t *i  /**< */)
{
    xcb_randr_refresh_rates_t *R = i->data;
    xcb_generic_iterator_t child = xcb_randr_refresh_rates_rates_end(R);
    --i->rem;
    i->data = (xcb_randr_refresh_rates_t *) child.data;
    i->index = child.index;
}


/*****************************************************************************
 **
 ** xcb_generic_iterator_t xcb_randr_refresh_rates_end
 ** 
 ** @param xcb_randr_refresh_rates_iterator_t i
 ** @returns xcb_generic_iterator_t
 **
 *****************************************************************************/
 
xcb_generic_iterator_t
xcb_randr_refresh_rates_end (xcb_randr_refresh_rates_iterator_t i  /**< */)
{
    xcb_generic_iterator_t ret;
    while(i.rem > 0)
        xcb_randr_refresh_rates_next(&i);
    ret.data = i.data;
    ret.rem = i.rem;
    ret.index = i.index;
    return ret;
}


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
                         uint32_t          minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_QUERY_VERSION,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_randr_query_version_cookie_t xcb_ret;
    xcb_randr_query_version_request_t xcb_out;
    
    xcb_out.major_version = major_version;
    xcb_out.minor_version = minor_version;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


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
                                   uint32_t          minor_version  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_QUERY_VERSION,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_randr_query_version_cookie_t xcb_ret;
    xcb_randr_query_version_request_t xcb_out;
    
    xcb_out.major_version = major_version;
    xcb_out.minor_version = minor_version;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


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
                               xcb_generic_error_t              **e  /**< */)
{
    return (xcb_randr_query_version_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


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
                             uint16_t          rate  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SET_SCREEN_CONFIG,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_randr_set_screen_config_cookie_t xcb_ret;
    xcb_randr_set_screen_config_request_t xcb_out;
    
    xcb_out.drawable = drawable;
    xcb_out.timestamp = timestamp;
    xcb_out.config_timestamp = config_timestamp;
    xcb_out.sizeID = sizeID;
    xcb_out.rotation = rotation;
    xcb_out.rate = rate;
    memset(xcb_out.pad0, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


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
                                       uint16_t          rate  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SET_SCREEN_CONFIG,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_randr_set_screen_config_cookie_t xcb_ret;
    xcb_randr_set_screen_config_request_t xcb_out;
    
    xcb_out.drawable = drawable;
    xcb_out.timestamp = timestamp;
    xcb_out.config_timestamp = config_timestamp;
    xcb_out.sizeID = sizeID;
    xcb_out.rotation = rotation;
    xcb_out.rate = rate;
    memset(xcb_out.pad0, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


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
                                   xcb_generic_error_t                  **e  /**< */)
{
    return (xcb_randr_set_screen_config_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}


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
                                uint16_t          enable  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SELECT_INPUT,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_select_input_request_t xcb_out;
    
    xcb_out.window = window;
    xcb_out.enable = enable;
    memset(xcb_out.pad0, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


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
                        uint16_t          enable  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_SELECT_INPUT,
        /* isvoid */ 1
    };
    
    struct iovec xcb_parts[4];
    xcb_void_cookie_t xcb_ret;
    xcb_randr_select_input_request_t xcb_out;
    
    xcb_out.window = window;
    xcb_out.enable = enable;
    memset(xcb_out.pad0, 0, 2);
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


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
                           xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_SCREEN_INFO,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_randr_get_screen_info_cookie_t xcb_ret;
    xcb_randr_get_screen_info_request_t xcb_out;
    
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    xcb_ret.sequence = xcb_send_request(c, XCB_REQUEST_CHECKED, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


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
                                     xcb_window_t      window  /**< */)
{
    static const xcb_protocol_request_t xcb_req = {
        /* count */ 2,
        /* ext */ &xcb_randr_id,
        /* opcode */ XCB_RANDR_GET_SCREEN_INFO,
        /* isvoid */ 0
    };
    
    struct iovec xcb_parts[4];
    xcb_randr_get_screen_info_cookie_t xcb_ret;
    xcb_randr_get_screen_info_request_t xcb_out;
    
    xcb_out.window = window;
    
    xcb_parts[2].iov_base = (char *) &xcb_out;
    xcb_parts[2].iov_len = sizeof(xcb_out);
    xcb_parts[3].iov_base = 0;
    xcb_parts[3].iov_len = -xcb_parts[2].iov_len & 3;
    xcb_ret.sequence = xcb_send_request(c, 0, xcb_parts + 2, &xcb_req);
    return xcb_ret;
}


/*****************************************************************************
 **
 ** xcb_randr_screen_size_t * xcb_randr_get_screen_info_sizes
 ** 
 ** @param const xcb_randr_get_screen_info_reply_t *R
 ** @returns xcb_randr_screen_size_t *
 **
 *****************************************************************************/
 
xcb_randr_screen_size_t *
xcb_randr_get_screen_info_sizes (const xcb_randr_get_screen_info_reply_t *R  /**< */)
{
    return (xcb_randr_screen_size_t *) (R + 1);
}


/*****************************************************************************
 **
 ** int xcb_randr_get_screen_info_sizes_length
 ** 
 ** @param const xcb_randr_get_screen_info_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_randr_get_screen_info_sizes_length (const xcb_randr_get_screen_info_reply_t *R  /**< */)
{
    return R->nSizes;
}


/*****************************************************************************
 **
 ** xcb_randr_screen_size_iterator_t xcb_randr_get_screen_info_sizes_iterator
 ** 
 ** @param const xcb_randr_get_screen_info_reply_t *R
 ** @returns xcb_randr_screen_size_iterator_t
 **
 *****************************************************************************/
 
xcb_randr_screen_size_iterator_t
xcb_randr_get_screen_info_sizes_iterator (const xcb_randr_get_screen_info_reply_t *R  /**< */)
{
    xcb_randr_screen_size_iterator_t i;
    i.data = (xcb_randr_screen_size_t *) (R + 1);
    i.rem = R->nSizes;
    i.index = (char *) i.data - (char *) R;
    return i;
}


/*****************************************************************************
 **
 ** int xcb_randr_get_screen_info_rates_length
 ** 
 ** @param const xcb_randr_get_screen_info_reply_t *R
 ** @returns int
 **
 *****************************************************************************/
 
int
xcb_randr_get_screen_info_rates_length (const xcb_randr_get_screen_info_reply_t *R  /**< */)
{
    return (R->nInfo - R->nSizes);
}


/*****************************************************************************
 **
 ** xcb_randr_refresh_rates_iterator_t xcb_randr_get_screen_info_rates_iterator
 ** 
 ** @param const xcb_randr_get_screen_info_reply_t *R
 ** @returns xcb_randr_refresh_rates_iterator_t
 **
 *****************************************************************************/
 
xcb_randr_refresh_rates_iterator_t
xcb_randr_get_screen_info_rates_iterator (const xcb_randr_get_screen_info_reply_t *R  /**< */)
{
    xcb_randr_refresh_rates_iterator_t i;
    xcb_generic_iterator_t prev = xcb_randr_screen_size_end(xcb_randr_get_screen_info_sizes_iterator(R));
    i.data = (xcb_randr_refresh_rates_t *) ((char *) prev.data + XCB_TYPE_PAD(xcb_randr_refresh_rates_t, prev.index));
    i.rem = (R->nInfo - R->nSizes);
    i.index = (char *) i.data - (char *) R;
    return i;
}


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
                                 xcb_generic_error_t                **e  /**< */)
{
    return (xcb_randr_get_screen_info_reply_t *) xcb_wait_for_reply(c, cookie.sequence, e);
}

