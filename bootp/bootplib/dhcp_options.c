/*
 * Copyright (c) 1999-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * dhcp_options.c
 * - routines to parse and access dhcp options
 *   and create new dhcp option areas
 * - handles overloaded areas as well as vendor-specific options
 *   that are encoded using the RFC 2132 encoding
 */

/* 
 * Modification History
 *
 * November 23, 1999	Dieter Siegmund (dieter@apple)
 * - created from objective C version (dhcpOptions.m)
 * - generalized code so that the file, sname and vendor specific
 *   areas could be parsed using the same routines
 * - cleanly separated functions that deal with existing option areas
 *   (dhcpol_*) from those that create new option areas (dhpoa_*).
 */
#import "dhcp.h"

#import <unistd.h>
#import <stdlib.h>
#import <stdio.h>
#import <sys/types.h>
#import <netinet/in.h>
#import <arpa/inet.h>
#import <strings.h>

#import "rfc_options.h"
#import "gen_dhcp_types.h"

#import "dhcp_options.h"
#import "gen_dhcp_parse_table.h"
#import "util.h"

/*
 * Module: dhcpoa (dhcp options area)
 *
 * Purpose:
 *   Types and functions to create new dhcp option areas.
 */

#define DHCPOA_MAGIC	0x11223344


#define MAX_TAG (sizeof(dhcptag_info_table) / sizeof(dhcptag_info_table[0]))
static unsigned char	rfc_magic[] = RFC_OPTIONS_MAGIC;


/*
 * Functions: dhcptag_*, dhcptype_*
 *
 * Purpose:
 *   Functions to deal with dhcp option tag and type information.
 */
/*
 * Function: dhcptype_info
 *
 * Purpose:
 *   Return the type information for the given type.
 */
dhcptype_info_t *
dhcptype_info(dhcptype_t type) 
{
    if (type > dhcptype_last_e)
	return (NULL);
    return dhcptype_info_table + type;
}

/*
 * Function: dhcptype_from_str
 *
 * Purpose:
 *   Convert from a string to the appropriate internal representation
 *   for the given type.  Calls the appropriate strto<type> function.
 */
boolean_t
dhcptype_from_str(unsigned char * str, int type, void * buf, int * len_p,
		  unsigned char * err)
{
    dhcptype_info_t * 	type_info = dhcptype_info(type);

    if (err)
	err[0] = '\0';

    if (type_info == NULL) {
	if (err) {
	    sprintf(err,"type %d unknown", type);
	}
	return (FALSE);
    }

    if (*len_p < type_info->size) {
	if (err) {
	    sprintf(err, "type %s buffer too small (%d < %d)", 
		    type_info->name, *len_p, type_info->size);
	}
	return (FALSE);
    }

    switch (type) {
      case dhcptype_bool_e: {
	  long l = strtol(str, 0, 0);
	  *len_p = type_info->size;
	  *((unsigned char *)buf) = ((l == 0) ? 0 : 1);
	  break;
      }
      case dhcptype_uint8_e: {
	  unsigned long ul = strtoul(str, 0, 0);
	  *len_p = type_info->size;
	  if (ul > 255) {
	      if (err) {
		  sprintf(err, "value %ld too large for %s",
			  ul, type_info->name);
	      }
	      return (FALSE);
	  }
	  *((unsigned char *)buf) = ul;
	  break;
      }
      case dhcptype_uint16_e: {
	  unsigned long ul = strtoul(str, 0, 0);
	  unsigned short us = ul;

	  if (ul > 65535) {
	      if (err) {
		  sprintf(err, "value %ld too large for %s",
			  ul, type_info->name);
	      }
	      return (FALSE);
	  }
	  *((unsigned short *)buf) = htons(us);
	  *len_p = type_info->size;
	  break;
      }
      case dhcptype_int32_e: {
	  long l = strtol(str, 0, 0);
	  *((long *)buf) = htonl(l);
	  *len_p = type_info->size;
	  break;
      }
      case dhcptype_uint32_e: {
	  unsigned long ul = strtoul(str, 0, 0);
	  *((unsigned long *)buf) = htonl(ul);
	  *len_p = type_info->size;
	  break;
      }
      case dhcptype_ip_e: {
	  struct in_addr ip;

	  if (inet_aton(str, &ip) == 0) {
	      if (err) {
		  sprintf(err, "%s not valid ip",
			  str);
	      }
	      return (FALSE);
	  }
	  *((struct in_addr *)buf) = ip;
	  *len_p = type_info->size;
	  break;
      }
      case dhcptype_string_e: {
	  int len = strlen(str);
	  if (*len_p < len) {
	      if (err) {
		  sprintf(err, "string too long");
	      }
	      return (FALSE);
	  }
	  if (len)
	      bcopy(str, buf, len);
	  *len_p = len;
	  break;
      }
      default:
	  if (err) {
	      sprintf(err, "type %s not yet supported",
		      type_info->name);
	  }
	  return (FALSE);
	  break;
    }
    return (TRUE);
}

/*
 * Function: dhcptype_to_str
 *
 * Purpose:
 *   Give a string representation to the given type.
 */
boolean_t
dhcptype_to_str(unsigned char * tmp, void * opt, int len, int type,
		unsigned char * err)
{
    switch (type) {
      case dhcptype_bool_e:
	sprintf(tmp, "%d", *((boolean_t *)opt));
	break;
      case dhcptype_uint8_e:
	sprintf(tmp, "%d", *((u_char *)opt));
	break;
      case dhcptype_uint16_e:
	sprintf(tmp, "%d", *((u_int16_t *)opt));
	break;
      case dhcptype_int32_e:
	sprintf(tmp, "%d", *((int32_t *)opt));
	break;
      case dhcptype_uint32_e:
	sprintf(tmp, "%u", *((u_int32_t *)opt));
	break;
      case dhcptype_ip_e:
	sprintf(tmp, IP_FORMAT, IP_LIST((struct in_addr *)opt));
	break;
      case dhcptype_string_e:
	strncpy(tmp, opt, len);
	tmp[len] = '\0';
	break;
      default: {
	  if (err)
	      sprintf(err, "type %d: not supported", type);
	  return (FALSE);
	  break;
      }
    }
    return (TRUE);
}
/*
 * Function: dhcptype_print_simple
 * Purpose:
 *   Display (to stdout) an option with a simple type.
 */
void
dhcptype_print_simple(dhcptype_t type, void * opt, int option_len)
{
    u_char *		option = opt;

    switch (type) {
      case dhcptype_bool_e:
	  printf("%s", *option ? "TRUE" : "FALSE");
	  break;
	
      case dhcptype_ip_e:
	  printf(IP_FORMAT, IP_LIST((struct in_addr *)option));
	  break;
	
      case dhcptype_string_e: {
	  printf("%.*s", option_len, (char *)option);
	  break;
      }
	
      case dhcptype_opaque_e:
	printf("\n");
	print_data(option, option_len);
	break;

      case dhcptype_uint8_e:
	printf("0x%x", *option);
	break;

      case dhcptype_uint16_e:
	printf("0x%x", ntohs(*((unsigned short *)option)));
	break;

      case dhcptype_uint32_e:
	printf("0x%lx", ntohl(*((unsigned long *)option)));
	break;

      case dhcptype_none_e:
      default:
	break;
    }
    return;
}

/*
 * Function: dhcptype_print
 * Purpose:
 *   Display (to stdout) an option with the given type.
 */
void
dhcptype_print(dhcptype_t type, void * option, int option_len)
{
    dhcptype_info_t * 	type_info = dhcptype_info(type);

    if (type_info && type_info->multiple_of != dhcptype_none_e) {
	int 			i;
	int 			number;
	void *			offset;
	int 			size;
	dhcptype_info_t * 	subtype_info;

	subtype_info = dhcptype_info(type_info->multiple_of);
	if (subtype_info == NULL)
	    return;

	size = subtype_info->size;
	number = option_len / size;
	printf("{");
	for (i = 0, offset = option; i < number; i++) {
	    if (i != 0)
		printf(", ");
	    dhcptype_print_simple(type_info->multiple_of, offset, size);
	    offset += size;
	}
	printf("}");
    }
    else
	dhcptype_print_simple(type, option, option_len);
    return;
}

/*
 * Function: dhcptag_print
 * Purpose:
 *   Display (to stdout) the given option.
 * Returns:
 *   TRUE it could be displayed, FALSE otherwise.
 */
boolean_t
dhcptag_print(void * vopt)
{
    u_char *    opt = vopt;
    u_char 	tag = opt[TAG_OFFSET];
    u_char 	option_len = opt[LEN_OFFSET];
    u_char * 	option = opt + OPTION_OFFSET;
    dhcptag_info_t * entry;

    entry = dhcptag_info(tag);
    if (entry == NULL)
	return (FALSE);
    {	
	dhcptype_info_t * type = dhcptype_info(entry->type);
	
	if (type == NULL) {
	    printf("unknown type %d\n", entry->type);
	    return (FALSE);
	}
	printf("%s (%s): ", entry->name, type->name);
	if (tag == dhcptag_dhcp_message_type_e)
	    printf("%s ", dhcp_msgtype_names(*option));
	dhcptype_print(entry->type, option, option_len);
	printf("\n");
    }
    return (TRUE);
}

/*
 * Function: dhcptag_info
 *
 * Purpose:
 *   Return the tag information for the give tag.
 */
dhcptag_info_t *
dhcptag_info(dhcptag_t tag)
{
    if (tag >= MAX_TAG)
	return (NULL);
    return dhcptag_info_table + tag;
}

/*
 * Function: dhcptag_with_name
 *
 * Purpose:
 *   Given a name, return a corresponding tag.  This comes from
 *   the table, as well as the default name of the option number itself.
 */

dhcptag_t
dhcptag_with_name(unsigned char * name)
{
    dhcptag_t i;

    for (i = 0; i < MAX_TAG; i++) {
	if (dhcptag_info_table[i].name) {
	    if (strcmp(dhcptag_info_table[i].name, name) == 0)
		return (i);
	}
    }
    {
	unsigned int v = strtoul(name, 0, 0);
	if (v <= MAX_TAG)
	    return (v);
    }
    return (-1);
}

/*
 * Function: dhcptag_name
 * Purpose:
 *   Return the name of the tag.
 */
unsigned char *
dhcptag_name(int tag)
{
    dhcptag_info_t * info;

    info = dhcptag_info(tag);
    if (info == NULL)
	return (NULL);
    return (info->name);
}

/*
 * Function: dhcptag_from_strlist
 *
 * Purpose:
 *   Convert from a string list using the appropriate
 *   type conversion for the tag's type.
 */
boolean_t
dhcptag_from_strlist(unsigned char * * slist, int num, 
		     int tag, void * buf, int * len_p,
		     unsigned char * err)
{
    int			i;
    int			n_per_type;
    dhcptag_info_t *	tag_info;
    dhcptype_info_t * 	type_info;
    dhcptype_info_t * 	base_type_info;

    if (err)
	err[0] = '\0';
    tag_info = dhcptag_info(tag);
    if (tag_info == NULL) {
	if (err)
	    sprintf(err, "tag %d unknown", tag);
	return (FALSE);
    }

    type_info = dhcptype_info(tag_info->type);
    if (type_info == NULL) {
	if (err)
	    sprintf(err, "tag %d type %d unknown", tag,
		    tag_info->type);
	return (FALSE);
    }

    if (type_info->multiple_of == dhcptype_none_e)
	return (dhcptype_from_str(slist[0], tag_info->type, 
				  buf, len_p,err));

    base_type_info = dhcptype_info(type_info->multiple_of);
    if (base_type_info == NULL) {
	if (err)
	    sprintf(err, "tag %d base type %d unknown", tag,
		    type_info->multiple_of);
	return (FALSE);
    }

    n_per_type = type_info->size / base_type_info->size;
    if (num & (n_per_type - 1)) {
	if (err)
	    sprintf(err, "type %s not a multiple of %d",
		    type_info->name, n_per_type);
	return (FALSE);
    }

    if ((num * base_type_info->size) > *len_p) /* truncate if necessary */
	num = *len_p / base_type_info->size;

    for (i = 0, *len_p = 0; i < num; i++) {
	int 		l;

	l = base_type_info->size;
	if (dhcptype_from_str(slist[i], type_info->multiple_of, buf, &l,
			      err) == FALSE)
	    return (FALSE);
	buf += l;
	*len_p += l;
    }
    return (TRUE);
}

/*
 * Function: dhcptag_to_str
 *
 * Purpose:
 *   Give a string representation to the given tag.
 * Note:
 *   For tags whose type is a multiple of a type, this routine
 *   only returns the string representation for the first
 *   occurrence.
 */
boolean_t
dhcptag_to_str(unsigned char * tmp, int tag, void * opt,
	       int len, unsigned char * err)
{
    dhcptag_info_t * 	tag_info;
    dhcptype_info_t * 	type_info;
    dhcptype_t		type;

    tag_info = dhcptag_info(tag);
    if (tag_info == NULL)
	return (FALSE);
    type_info = dhcptype_info(tag_info->type);
    if (type_info->multiple_of == dhcptype_none_e)
	type = tag_info->type;
    else
	type = type_info->multiple_of;
    return dhcptype_to_str(tmp, opt, len, type, err);
}

/*
 * Functions: dhcpol_* 
 *
 * Purpose:
 *   Routines to parse/access existing options buffers.
 */
boolean_t
dhcpol_add(dhcpol_t * list, void * element)
{
    return (ptrlist_add((ptrlist_t *)list, element));
}

int
dhcpol_count(dhcpol_t * list)
{
    return (ptrlist_count((ptrlist_t *)list));
}

void *
dhcpol_element(dhcpol_t * list, int i)
{
    return (ptrlist_element((ptrlist_t *)list, i));
}

void
dhcpol_init(dhcpol_t * list)
{
    ptrlist_init((ptrlist_t *)list);
}

void
dhcpol_free(dhcpol_t * list)
{
    ptrlist_free((ptrlist_t *)list);
}

boolean_t
dhcpol_concat(dhcpol_t * list, dhcpol_t * extra)
{
    return (ptrlist_concat((ptrlist_t *)list, (ptrlist_t *)extra));
}

/*
 * Function: dhcpol_parse_buffer
 *
 * Purpose:
 *   Parse the given buffer into DHCP options, returning the
 *   list of option pointers in the given dhcpol_t.
 *   Parsing continues until we hit the end of the buffer or
 *   the end tag.
 */
boolean_t
dhcpol_parse_buffer(dhcpol_t * list, void * buffer, int length,
		    unsigned char * err)
{
    int			len;
    unsigned char *	scan;
    unsigned char	tag;

    if (err)
	err[0] = '\0';

    dhcpol_init(list);

    len = length;
    tag = dhcptag_pad_e;
    for (scan = (unsigned char *)buffer; tag != dhcptag_end_e && len > 0; ) {

	tag = scan[TAG_OFFSET];

	switch (tag) {
	  case dhcptag_end_e:
	      dhcpol_add(list, scan); /* remember that it was terminated */
	      scan++;
	      len--;
	      break;
	  case dhcptag_pad_e: /* ignore pad */
	      scan++;
	      len--;
	      break;
	  default: {
	      unsigned char	option_len = scan[LEN_OFFSET];
	    
	      dhcpol_add(list, scan);
	      len -= (option_len + 2);
	      scan += (option_len + 2);
	      break;
	  }
	}
    }
    if (len < 0) {
	/* ran off the end */
	if (err)
	    sprintf(err, "parse failed near tag %d", tag);
	dhcpol_free(list);
	return (FALSE);
    }
    return (TRUE);
}

/*
 * Function: dhcpol_find
 *
 * Purpose:
 *   Finds the first occurence of the given option, and returns its
 *   length and the option data pointer.
 *
 *   The optional start parameter allows this function to 
 *   return the next start point so that successive
 *   calls will retrieve the next occurence of the option.
 *   Before the first call, *start should be set to 0.
 */
void *
dhcpol_find(dhcpol_t * list, int tag, int * len_p, int * start)
{
    int 	i = 0;

    if (tag == dhcptag_end_e || tag == dhcptag_pad_e)
	return (NULL);

    if (start)
	i = *start;

    for (; i < dhcpol_count(list); i++) {
	unsigned char * option = dhcpol_element(list, i);
	
	if (option[TAG_OFFSET] == tag) {
	    if (len_p)
		*len_p = option[LEN_OFFSET];
	    if (start)
		*start = i + 1;
	    return (option + OPTION_OFFSET);
	}
    }
    return (NULL);
}

/*
 * Function: dhcpol_get
 * 
 * Purpose:
 *   Accumulate all occurences of the given option into a
 *   malloc'd buffer, and return its length.  Used to get
 *   all occurrences of a particular option in a single
 *   data area.
 * Note:
 *   Use free() to free the returned data area.
 */
void *
dhcpol_get(dhcpol_t * list, int tag, int * len_p)
{
    int 	i;
    void *	data = NULL;
    int		data_len = 0;

    if (tag == dhcptag_end_e || tag == dhcptag_pad_e)
	return (NULL);

    for (i = 0; i < dhcpol_count(list); i++) {
	unsigned char * option = dhcpol_element(list, i);
	
	if (option[TAG_OFFSET] == tag) {
	    int len = option[LEN_OFFSET];

	    if (data_len == 0) {
		data = malloc(len);
	    }
	    else {
		data = realloc(data, data_len + len);
	    }
	    bcopy(option + OPTION_OFFSET, data + data_len, len);
	    data_len += len;
	}
    }
    *len_p = data_len;
    return (data);
}

/*
 * Function: dhcpol_parse_packet
 *
 * Purpose:
 *    Parse the option areas in the DHCP packet.
 *    Verifies that the packet has the right magic number,
 *    then parses and accumulates the option areas.
 *    First the pkt->dp_options is parsed.  If that contains
 *    the overload option, it parses pkt->dp_file if specified,
 *    then parses pkt->dp_sname if specified.
 */
boolean_t
dhcpol_parse_packet(dhcpol_t * options, struct dhcp * pkt, int len,
		    unsigned char * err)
{
    dhcpol_init(options);	/* make sure it's empty */

    if (err)
	err[0] = '\0';

    if (len < (sizeof(*pkt) + RFC_MAGIC_SIZE)) {
	if (err) {
	    sprintf(err, "packet is too short: %d < %d",
		    len, (int)sizeof(*pkt) + RFC_MAGIC_SIZE);
	}
	return (FALSE);
    }
    if (bcmp(pkt->dp_options, rfc_magic, RFC_MAGIC_SIZE)) {
	if (err)
	    sprintf(err, "missing magic number");
	return (FALSE);
    }
    if (dhcpol_parse_buffer(options, pkt->dp_options + RFC_MAGIC_SIZE,
			    len - sizeof(*pkt) - RFC_MAGIC_SIZE, err) == FALSE)
	return (FALSE);
    { /* get overloaded options */
	unsigned char *	overload;
	int		overload_len;

	overload = (unsigned char *)
	    dhcpol_find(options, dhcptag_option_overload_e, 
				&overload_len, NULL);
	if (overload && overload_len == 1) { /* has overloaded options */
	    dhcpol_t	extra;

	    dhcpol_init(&extra);
	    if (*overload == DHCP_OVERLOAD_FILE
		|| *overload == DHCP_OVERLOAD_BOTH) {
		if (dhcpol_parse_buffer(&extra, pkt->dp_file, 
					 sizeof(pkt->dp_file), NULL)) {
		    dhcpol_concat(options, &extra);
		    dhcpol_free(&extra);
		}
	    }
	    if (*overload == DHCP_OVERLOAD_SNAME
		|| *overload == DHCP_OVERLOAD_BOTH) {
		if (dhcpol_parse_buffer(&extra, pkt->dp_sname, 
					 sizeof(pkt->dp_sname), NULL)) {
		    dhcpol_concat(options, &extra);
		    dhcpol_free(&extra);
		}
	    }
	}
    }
    return (TRUE);
}

/*
 * Function: dhcpol_parse_vendor
 *
 * Purpose:
 *   Given a set of options, find the vendor specific option(s)
 *   and parse all of them into a single option list.
 *  
 * Return value:
 *   TRUE if vendor specific options existed and were parsed succesfully,
 *   FALSE otherwise.
 */
boolean_t
dhcpol_parse_vendor(dhcpol_t * vendor, dhcpol_t * options,
		    unsigned char * err)
{
    dhcpol_t		extra;
    boolean_t		ret = FALSE;
    int 		start = 0;

    if (err)
	err[0] = '\0';

    dhcpol_init(vendor);
    dhcpol_init(&extra);

    for (;;) {
	void *		data;
	int		len;

	data = dhcpol_find(options, dhcptag_vendor_specific_e, &len, &start);
	if (data == NULL) {
	    break; /* out of for */
	}

	if (dhcpol_parse_buffer(&extra, data, len, err) == FALSE) {
	    goto failed;
	}

	if (dhcpol_concat(vendor, &extra) == FALSE) {
	    if (err)
		sprintf(err, "dhcpol_concat() failed at %d\n", start);
	    goto failed;
	}
	dhcpol_free(&extra);
	ret = TRUE;
    }
    if (ret == FALSE) {
	if (err)
	    strcpy(err, "missing vendor specific options");
    }
    return (ret);

 failed:
    dhcpol_free(vendor);
    dhcpol_free(&extra);
    return (FALSE);
}

void
dhcpol_print(dhcpol_t * list)
{
    int 		i;

    printf("Options count is %d\n", dhcpol_count(list));
    for (i = 0; i < dhcpol_count(list); i++) {
	unsigned char * option = dhcpol_element(list, i);

	if (dhcptag_print(option) == FALSE) 
	    printf("undefined tag %d len %d\n", option[TAG_OFFSET], 
		   option[LEN_OFFSET]);
    }
}

/*
 * Module: dhcpoa
 *
 * Purpose:
 *   Types and functions to create new dhcp option areas.
 */

/*
 * Function: dhcpoa_{init_common, init_no_end, init}
 *
 * Purpose:
 *   Initialize an option area structure so that it can be used
 *   in calling the dhcpoa_* routines.
 */

static void
dhcpoa_init_common(dhcpoa_t * oa_p, void * buffer, int size, int reserve)
{
    bzero(buffer, size);	/* fill option area with pad tags */

    bzero(oa_p, sizeof(*oa_p));
    oa_p->oa_magic = DHCPOA_MAGIC;
    oa_p->oa_buffer = buffer;
    oa_p->oa_size = size;
    oa_p->oa_reserve = reserve;
}

void
dhcpoa_init_no_end(dhcpoa_t * oa_p, void * buffer, int size)
{
    dhcpoa_init_common(oa_p, buffer, size, 0);
    return;
}

int
dhcpoa_size(dhcpoa_t * oa_p)
{
    return (oa_p->oa_size);
}

void
dhcpoa_init(dhcpoa_t * oa_p, void * buffer, int size)
{
    /* initialize the area, reserve space for the end tag */
    dhcpoa_init_common(oa_p, buffer, size, 1);
    return;
}

dhcpoa_ret_t
dhcpoa_vendor_add(dhcpoa_t * oa_p, dhcpoa_t * vendor_oa_p,
		  dhcptag_t tag, int len, void * option)
{
    int			freespace;
    dhcpoa_ret_t	ret = dhcpoa_success_e;

    oa_p->oa_err[0] = '\0';

    if (len > (DHCP_OPTION_SIZE_MAX - OPTION_OFFSET)) {
	sprintf(vendor_oa_p->oa_err, "tag %d option %d > %d", tag, len, 
		DHCP_OPTION_SIZE_MAX - OPTION_OFFSET);
	return (dhcpoa_failed_e);
    }
    freespace = dhcpoa_freespace(vendor_oa_p) - OPTION_OFFSET;
    if (freespace < len) {
	/* add the vendor-specific options to the packet to make room */
	ret = dhcpoa_add(oa_p, dhcptag_vendor_specific_e,
			 dhcpoa_used(vendor_oa_p),
			 dhcpoa_buffer(vendor_oa_p));
	if (ret != dhcpoa_success_e) {
	    sprintf(vendor_oa_p->oa_err, "tag %d option %d > %d",
		    tag, len, freespace);
	    goto failed;
	}
	freespace = dhcpoa_freespace(oa_p) - OPTION_OFFSET;
	if (freespace > dhcpoa_size(vendor_oa_p)) {
	    freespace = dhcpoa_size(vendor_oa_p);
	}
	dhcpoa_init_no_end(vendor_oa_p, dhcpoa_buffer(vendor_oa_p),
			   freespace);
    }
    ret = dhcpoa_add(vendor_oa_p, tag, len, option);

 failed:
    return (ret);
}


/*
 * Function: dhcpoa_add
 *
 * Purpose:
 *   Add an option to the option area.
 */
dhcpoa_ret_t
dhcpoa_add(dhcpoa_t * oa_p, dhcptag_t tag, int len, void * option)
{

    oa_p->oa_err[0] = '\0';

    if (len > DHCP_OPTION_SIZE_MAX) {
	sprintf(oa_p->oa_err, "tag %d option %d > %d", tag, len, 
		DHCP_OPTION_SIZE_MAX);
	return (dhcpoa_failed_e);
    }

    if (oa_p->oa_magic != DHCPOA_MAGIC) {
	strcpy(oa_p->oa_err, "dhcpoa_t not initialized - internal error!!!");
	return (dhcpoa_failed_e);
    }

    if (oa_p->oa_end_tag) {
	strcpy(oa_p->oa_err, "attempt to add data after end tag");
	return (dhcpoa_failed_e);
    }

    switch (tag) {
      case dhcptag_end_e:
	if ((oa_p->oa_offset + 1) > oa_p->oa_size) {
	    /* this can't happen since we're careful to leave space */
	    sprintf(oa_p->oa_err, "can't add end tag %d > %d",
		    oa_p->oa_offset + oa_p->oa_reserve, oa_p->oa_size);
	    return (dhcpoa_failed_e);
	}
	((u_char *)oa_p->oa_buffer)[oa_p->oa_offset + TAG_OFFSET] = tag;
	oa_p->oa_offset++;
	oa_p->oa_end_tag = TRUE;
	break;

      case dhcptag_pad_e:
	/* 1 for pad tag */
	if ((oa_p->oa_offset + oa_p->oa_reserve + 1) > oa_p->oa_size) {
	    sprintf(oa_p->oa_err, "can't add pad tag %d > %d",
		    oa_p->oa_offset + oa_p->oa_reserve + 1, oa_p->oa_size);
	    return (dhcpoa_full_e);
	}
	((u_char *)oa_p->oa_buffer)[oa_p->oa_offset + TAG_OFFSET] = tag;
	oa_p->oa_offset++;
	break;

      default:
	/* 2 for tag/len */
	if ((oa_p->oa_offset + len + 2 + oa_p->oa_reserve) > oa_p->oa_size) {
	    sprintf(oa_p->oa_err, "can't add tag %d (%d > %d)", tag,
		    oa_p->oa_offset + len + 2 + oa_p->oa_reserve, 
		    oa_p->oa_size);
	    return (dhcpoa_full_e);
	}
	((u_char *)oa_p->oa_buffer)[oa_p->oa_offset + TAG_OFFSET] = tag;
	((u_char *)oa_p->oa_buffer)[oa_p->oa_offset + LEN_OFFSET] = len;
	if (len) {
	    bcopy(option, (u_char *)oa_p->oa_buffer 
		  + (OPTION_OFFSET + oa_p->oa_offset), len);
	}
	oa_p->oa_prev_last = oa_p->oa_last;
	oa_p->oa_last = oa_p->oa_offset;
	oa_p->oa_offset += len + OPTION_OFFSET;
	break;
    }
    oa_p->oa_option_count++;
    return (dhcpoa_success_e);
}

/*
 * Function: dhcpoa_add_from_strlist
 *
 * Purpose:
 *   Convert the string list into an option with the specified tag.
 * Note:
 *   This only works for type representations that we know about.
 */
dhcpoa_ret_t
dhcpoa_add_from_strlist(dhcpoa_t * oa_p, dhcptag_t tag, 
			 unsigned char * * strlist, int strlist_len)
{
    int 		len;
    char		tmp[DHCP_OPTION_SIZE_MAX];

    len = sizeof(tmp);

    if (dhcptag_from_strlist(strlist, strlist_len, tag, tmp, &len,
			     oa_p->oa_err) == FALSE) {
	return (dhcpoa_failed_e);
    }
    return (dhcpoa_add(oa_p, tag, len, tmp));
}

/*
 * Function: dhcpoa_add_from_str
 *
 * Purpose:
 *   Convert the string into an option with the specified tag.
 */
dhcpoa_ret_t
dhcpoa_add_from_str(dhcpoa_t * oa_p, dhcptag_t tag, 
		     unsigned char * str)
{
    return (dhcpoa_add_from_strlist(oa_p, tag, &str, 1));
}

/*
 * Function: dhcpoa_add_dhcpmsg
 *
 * Purpose:
 *   Add a dhcp message option to the option area.
 */
dhcpoa_ret_t
dhcpoa_add_dhcpmsg(dhcpoa_t * oa_p, dhcp_msgtype_t msgtype)
{
    unsigned char m = (unsigned char)msgtype;
    return (dhcpoa_add(oa_p, dhcptag_dhcp_message_type_e,
			sizeof(m), &m));
}

/*
 * Function: dhcpoa_err
 *
 * Purpose:
 *   Return an error message for the last error that occcurred.
 */
unsigned char *
dhcpoa_err(dhcpoa_t * oa_p)
{
    if (oa_p == NULL || oa_p->oa_magic != DHCPOA_MAGIC)
	return ("<bad parameter>");
    return (oa_p->oa_err);
}


int
dhcpoa_used(dhcpoa_t * oa_p)
{
    if (oa_p == NULL || oa_p->oa_magic != DHCPOA_MAGIC)
	return 0;
    return (oa_p->oa_offset);
}

int
dhcpoa_freespace(dhcpoa_t * oa_p)
{
    int	freespace;

    if (oa_p == NULL || oa_p->oa_magic != DHCPOA_MAGIC) {
	return 0;
    }
    freespace = oa_p->oa_size - oa_p->oa_offset - oa_p->oa_reserve;
    if (freespace < 0) {
	freespace = 0;
    }
    return (freespace);
}

int
dhcpoa_count(dhcpoa_t * oa_p)
{
    if (oa_p == NULL || oa_p->oa_magic != DHCPOA_MAGIC)
	return 0;
    return (oa_p->oa_option_count);
}

void *
dhcpoa_buffer(dhcpoa_t * oa_p) 
{
    if (oa_p == NULL || oa_p->oa_magic != DHCPOA_MAGIC)
      return (NULL);
    return (oa_p->oa_buffer);
}

#ifdef TEST_DHCP_OPTIONS
char test_empty[] = {
    99, 130, 83, 99,
    255,
};

char test_simple[] = {
    99, 130, 83, 99,
    1, 4, 255, 255, 252, 0,
    3, 4, 17, 202, 40, 1,
    255,
};

char test_vendor[] = {
    99, 130, 83, 99,
    1, 4, 255, 255, 252, 0,
    3, 4, 17, 202, 40, 1,
    43, 6, 1, 4, 1, 2, 3, 4,
    43, 6, 1, 4, 1, 2, 3, 4,
    255,
};

char test_no_end[] = {
    0x63, 0x82, 0x53, 0x63, 0x35, 0x01, 0x05, 0x36, 
    0x04, 0xc0, 0xa8, 0x01, 0x01, 0x33, 0x04, 0x80,
    0x00, 0x80, 0x00, 0x01, 0x04, 0xff, 0xff, 0xff,
    0x00, 0x03, 0x04, 0xc0, 0xa8, 0x01, 0x01, 0x06,
    0x0c, 0x18, 0x1a, 0xa3, 0x21, 0x18, 0x1a, 0xa3,
    0x20, 0x18, 0x5e, 0xa3, 0x21, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

char test_too_short[] = {
    0x1 
};
struct test {
    char * 		name;
    char *		data;
    int			len;
    boolean_t		result;
};

struct test tests[] = {
    { "empty", test_empty, sizeof(test_empty), TRUE },
    { "simple", test_simple, sizeof(test_simple), TRUE },
    { "vendor", test_vendor, sizeof(test_vendor), TRUE },
    { "no_end", test_no_end, sizeof(test_no_end), TRUE },
    { "too_short", test_too_short, sizeof(test_too_short), FALSE },
    { NULL, NULL, 0, FALSE },
};


char test_string_253[253] = "test string 253 characters long (zero padded)";

static char buf[2048];
static char vend_buf[255];

int
main()
{
    int 	i;
    dhcpol_t 	options;
    dhcpol_t	vendor_options;
    char	error[256];
    struct dhcp * pkt = (struct dhcp *)buf;

    dhcpol_init(&options);
    dhcpol_init(&vendor_options);

    for (i = 0; tests[i].name; i++) {
	printf("\nTest %d: ", i);
	bcopy(tests[i].data, pkt->dp_options, tests[i].len);
	if (dhcpol_parse_packet(&options, pkt, 
				sizeof(*pkt) + tests[i].len,
				error) != tests[i].result) {
	    printf("test '%s' FAILED\n", tests[i].name);
	    if (tests[i].result == TRUE) {
		printf("error message returned was %s\n", error);
	    }
	}
	else {
	    printf("test '%s' PASSED\n", tests[i].name);
	    if (tests[i].result == FALSE) {
		printf("error message returned was %s\n", error);
	    }
	}
	dhcpol_print(&options);
	dhcpol_free(&options);
    }
    {
	struct in_addr	iaddr;
	dhcpoa_t	opts;
	dhcpoa_t	vend_opts;
	char		err[256];
	unsigned char * str;

	dhcpoa_init(&opts, buf, sizeof(buf));
	dhcpoa_init_no_end(&vend_opts, vend_buf, sizeof(vend_buf));
	
	iaddr.s_addr = inet_addr("255.255.252.0");
	if (dhcpoa_add(&opts, dhcptag_subnet_mask_e, sizeof(iaddr),
		       &iaddr) == dhcpoa_failed_e) {
	    printf("couldn't add subnet mask, %s\n", dhcpoa_err(&opts));
	    exit(1);
	}
	str = "17.202.40.1";
	if (dhcpoa_add_from_str(&opts, dhcptag_router_e, str) 
	    != dhcpoa_success_e) {
	    printf("couldn't add router, %s\n", dhcpoa_err(&opts));
	    exit(1);
	}
	if (dhcpoa_add_from_str(&opts, dhcptag_host_name_e, "siegdi5") 
	    != dhcpoa_success_e) {
	    printf("couldn't add hostname tag, %s\n", dhcpoa_err(&opts));
	    exit(1);
	}
	if (dhcpoa_add_from_str(&opts, dhcptag_domain_name_e, "apple.com") 
	    != dhcpoa_success_e) {
	    printf("couldn't add domain name tag, %s\n", dhcpoa_err(&opts));
	    exit(1);
	}
	for (i = 0; i < 253; i++) {
	    if (dhcpoa_vendor_add(&opts, &vend_opts, i+1, 
				  i,
				  test_string_253) != dhcpoa_success_e) {
		printf("couldn't add vendor option, %s\n", 
		       dhcpoa_err(&vend_opts));
		break;
	    }
	}
	if (dhcpoa_used(&vend_opts) > 0) {
	    if (dhcpoa_add(&opts, dhcptag_vendor_specific_e,
			   dhcpoa_used(&vend_opts), dhcpoa_buffer(&vend_opts))
		!= dhcpoa_success_e) {
		printf("couldn't add vendor options, %s\n", dhcpoa_err(&opts));
		exit(1);
	    }
	}
	if (dhcpoa_add(&opts, dhcptag_end_e, 0, 0) != dhcpoa_success_e) {
	    printf("couldn't add end tag, %s\n", dhcpoa_err(&opts));
	    exit(1);
	}
	if (dhcpol_parse_buffer(&options, buf, sizeof(buf), err)
	    == FALSE) {
	    printf("parse buffer failed, %s\n", err);
	    exit(1);
	}
	printf("\nTesting dhcpoa\n");
	dhcpol_print(&options);
	{
	    struct in_addr * iaddr;
	    iaddr = (struct in_addr *)
		dhcpol_find(&options, dhcptag_router_e, 0, 0);
	    if (iaddr == NULL) {
		printf("can't find router option\n");
	    }
	    else {
		printf("Found router option %s\n", inet_ntoa(*iaddr));
	    }
	}
	if (dhcpol_parse_vendor(&vendor_options, &options, NULL)) {
	    printf("vendor parsed ok\n");
	}
	else {
	    printf("parse vendor failed\n");
	}
	dhcpol_free(&options);
	dhcpol_free(&vendor_options);
    }
    exit(0);
}
#endif TEST_DHCP_OPTIONS
