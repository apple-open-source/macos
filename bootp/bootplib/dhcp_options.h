
/*
 * dhcp_options.h
 * - routines to parse and access dhcp options
 *   and create new dhcp option areas
 */

/*
 * Modification History
 *
 * November 23, 1999	Dieter Siegmund (dieter@apple)
 * - created
 */
#import <mach/boolean.h>
#import "ptrlist.h"
#import "dhcp.h"
#import "gen_dhcp_tags.h"
#import "gen_dhcp_types.h"

/*
 * DHCP_OPTION_SIZE_MAX
 * - the largest size that an option can be (limited to an 8-bit quantity)
 */
#define DHCP_OPTION_SIZE_MAX	255

#define TAG_OFFSET	0
#define LEN_OFFSET	1
#define OPTION_OFFSET	2

/*
 * Module: dhcpoa (dhcp options area)
 *
 * Purpose:
 *   Types and functions to create new dhcp option areas.
 */

/*
 * Type: dhcpoa_t
 *
 * Purpose:
 *   To record information about a dhcp option data area.
 */
typedef struct {
    u_long	oa_magic;	/* magic number to ensure it's been init'd */
    void *	oa_buffer;	/* data area to hold options */
    int		oa_size;	/* size of buffer */
    int		oa_offset;	/* offset of next option to write */
    boolean_t	oa_end_tag;	/* to mark when options are terminated */
    int		oa_last;	/* the offset of the last option added */
    int		oa_prev_last;	/* the offset of the option previous to last */
    int		oa_option_count;/* number of options present */
    char	oa_err[256];	/* error string */
} dhcpoa_t;

#define DHCPOA_MAGIC	0x11223344

/* 
 * Type:dhcpoa_ret_t
 *
 * Purpose:
 *  outine return codes 
 */
typedef enum {
    dhcpoa_success_e = 0,
    dhcpoa_failed_e,
    dhcpoa_full_e,
} dhcpoa_ret_t;

void
dhcpoa_init(dhcpoa_t * opt, void * buffer, int size);

dhcpoa_ret_t
dhcpoa_add(dhcpoa_t * buf, dhcptag_t tag, int len, void * option);

dhcpoa_ret_t
dhcpoa_add_from_strlist(dhcpoa_t * buf, dhcptag_t tag, 
			unsigned char * * strlist, int strlist_len);

dhcpoa_ret_t
dhcpoa_add_from_str(dhcpoa_t * buf, dhcptag_t tag, 
		    unsigned char * str);

dhcpoa_ret_t
dhcpoa_add_dhcpmsg(dhcpoa_t * buf, dhcp_msgtype_t msgtype);

unsigned char *
dhcpoa_err(dhcpoa_t * buf);

int
dhcpoa_used(dhcpoa_t * buf);

int
dhcpoa_count(dhcpoa_t * buf);


/*
 * Module: dhcpol (dhcp options list)
 *
 * Purpose:
 *   Routines to parse and retrieve dhcp options.
 */

typedef ptrlist_t dhcpol_t;

void			dhcpol_init(dhcpol_t * list);
void			dhcpol_free(dhcpol_t * list);
int			dhcpol_count(dhcpol_t * list);
boolean_t		dhcpol_add(dhcpol_t * list, void * element);
void *			dhcpol_element(dhcpol_t * list, int i);
boolean_t		dhcpol_concat(dhcpol_t * list, dhcpol_t * extra);
boolean_t		dhcpol_parse_buffer(dhcpol_t * list, void * buffer, 
					    int length, unsigned char * err);
void *			dhcpol_find(dhcpol_t * list, int tag, int * len_p, 
				    int * start);
void *			dhcpol_get(dhcpol_t * list, int tag, int * len_p);
boolean_t		dhcpol_parse_packet(dhcpol_t * options, 
					    struct dhcp * pkt, int len,
					    unsigned char * err);
boolean_t		dhcpol_parse_vendor(dhcpol_t * vendor, 
					    dhcpol_t * options,
					    unsigned char * err);
void			dhcpol_print(dhcpol_t * list);

/*
 * Functions: dhcptype_*, dhcptag_*
 *
 * Purpose:
 *   Get tag and type information as well as do conversions.
 */

dhcptype_info_t *	dhcptype_info(dhcptype_t type);
boolean_t		dhcptype_from_str(unsigned char * str, 
					  int type, void * buf, int * len_p,
					  unsigned char * err);
boolean_t		dhcptype_to_str(unsigned char * tmp, void * opt, 
					int len, int type, 
					unsigned char * err);
void			dhcptype_print_simple(dhcptype_t type, 
					      void * opt, int option_len);
void			dhcptype_print(dhcptype_t type, void * option, 
				       int option_len);

dhcptag_info_t *	dhcptag_info(dhcptag_t tag);
dhcptag_t		dhcptag_with_name(unsigned char * name);
unsigned char *		dhcptag_name(int tag);
boolean_t		dhcptag_from_strlist(unsigned char * * slist, 
					     int num, int tag, void * buf, 
					     int * len_p, 
					     unsigned char * err);
boolean_t		dhcptag_to_str(unsigned char * tmp, int tag, 
				       void * opt, int len, 
				       unsigned char * err);
boolean_t		dhcptag_print(void * vopt);

