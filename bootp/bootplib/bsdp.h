
/*
 * bsdp.h
 * - Boot Server Discovery Protocol (BSDP) definitions
 */

/*
 * Modification History
 *
 * Dieter Siegmund (dieter@apple.com)		November 24, 1999
 * - created
 */

#import <mach/boolean.h>

typedef u_int16_t bsdp_priority_t;
typedef u_int16_t bsdp_version_t;
typedef u_int32_t bsdp_image_id_t;

#define BSDP_IMAGE_ID_NULL	((bsdp_image_id_t)0)

#define BSDP_PRIORITY_MIN	((bsdp_priority_t) 0)
#define BSDP_PRIORITY_MAX	((bsdp_priority_t) 65535)

#define BSDP_PRIORITY_BASE	((bsdp_priority_t) 32768)

#define BSDP_VENDOR_CLASS_ID	"AAPLBSDPC"
#define BSDP_VERSION		((unsigned short)0x0100)

typedef enum {
    bsdptag_message_type_e 		= 1,
    bsdptag_version_e 			= 2,
    bsdptag_server_identifier_e		= 3,
    bsdptag_server_priority_e		= 4,
    bsdptag_reply_port_e		= 5,
    bsdptag_boot_image_list_e		= 6,
    bsdptag_default_boot_image_e	= 7,
    bsdptag_selected_boot_image_e	= 8,

    /* bounds */
    bsdptag_first_e			= 1,
    bsdptag_last_e			= 8,
} bsdptag_t;

static __inline__ const char *
bsdptag_name(bsdptag_t tag)
{
    static char * names[] = {
	NULL,
	"message type",			/* 1 */
	"version",			/* 2 */
	"server identifier",		/* 3 */
	"server priority",		/* 4 */
	"reply port",			/* 5 */
	"boot image list",		/* 6 */
	"default boot image",		/* 7 */
	"selected boot image",		/* 8 */
    };
    if (tag >= bsdptag_first_e && tag <= bsdptag_last_e)
	return (names[tag]);
    return (NULL);
}

typedef enum {
    bsdp_msgtype_none_e				= 0,
    bsdp_msgtype_list_e 			= 1,
    bsdp_msgtype_select_e 			= 2,
    bsdp_msgtype_failed_e			= 3,
} bsdp_msgtype_t;

static __inline__ unsigned char *
bsdp_msgtype_names(bsdp_msgtype_t type)
{
    unsigned char * names[] = {
	"<none>",
	"LIST",
	"SELECT",
	"FAILED",
    };
    if (type >= bsdp_msgtype_none_e && type <= bsdp_msgtype_failed_e)
	return (names[type]);
    return ("<unknown>");
}

/*
 * Function: bsdp_parse_class_id
 *
 * Purpose:
 *   Parse the given option into the arch and system identifier
 *   fields.
 *   
 *   The format is "AAPLBSDPC/<arch>/<system_id>" for client-generated
 *   requests and "AAPLBSDPC" for server-generated responses.
 */
static __inline__ boolean_t
bsdp_parse_class_id(void * buf, int buf_len, unsigned char * arch, 
		    unsigned char * sysid)
{
    int		len;
    u_char * 	scan;

    *arch = '\0';
    *sysid = '\0';

    len = strlen(BSDP_VENDOR_CLASS_ID);
    if (buf_len < len || memcmp(buf, BSDP_VENDOR_CLASS_ID, len))
	return (FALSE); /* not a BSDP class identifier */
    
    buf_len -= len;
    scan = (u_char *)buf + len;
    if (buf_len == 0)
	return (TRUE); /* server-generated */

    if (*scan != '/')
	return (FALSE);

    for (scan++, buf_len--; buf_len && *scan != '/'; scan++, buf_len--) {
	*arch++ = *scan;
    }
    *arch = '\0';
    if (*scan == '/') {
	for (scan++, buf_len--; buf_len; scan++, buf_len--) {
	    *sysid++ = *scan;
	}
	*sysid = '\0';
    }
    return (TRUE);
}

