 /*
  * Extended IP address range matching.
  * 
  * Enable use of this package with -DADDRESS_RANGE
  *
  * In addition to patterns like 1.2.3.4 and 1.2.3.4/255.255.255.0,
  * this allows patterns like 1.2.3, 1.2.3.4-1.2.3.7, 1.2.3.4/24,
  * 1.2.3.4/C, 1.2.3.0-1.2.5.0/255.255.255.0, + (==all hosts), etc.  
  *
  * Author: Douglas Davidson, Apple Computer, Inc.
  */

#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include "tcpd.h"

#define QUAD_SEP_CHAR '.'
#define RANGE_SEP_CHAR '-'
#define MASK_SEP_CHAR '/'
#define ANY_HOST_CHAR '+'

#define address_range_min(a,b) (((a)<(b))?(a):(b))
#define address_range_max(a,b) (((a)<(b))?(b):(a))
#define address_range_ispart(a) ((a)<256)
#define address_range_isshift(a) ((a)<=32)
#define address_range_ignore(a) (isspace(a)||(QUAD_SEP_CHAR==a)||(RANGE_SEP_CHAR==a)||(MASK_SEP_CHAR==a))
#define address_range_address(a, b, c, d) ((a<<24)+(b<<16)+(c<<8)+d)
#define address_range_netmask(a) ((a>0)?(INADDR_NONE<<(32-a)):(INADDR_ANY))

static unsigned int address_range_value(start, end, address_ptr, netmask_ptr)
char 		*start;
char 		*end;
unsigned long  	*address_ptr;
unsigned long  	*netmask_ptr;
{
    unsigned int retval = 0, a = 0, b = 0, c = 0, d = 0, bits = 32;
    int n = 0, length;
    unsigned long address = INADDR_NONE, netmask = INADDR_ANY;
    while (start <= end && address_range_ignore(*(end-1))) end--;
    while (start <= end && address_range_ignore(*start)) start++;
    length = end - start;
    if ((length > 6 && 4 == sscanf(start, "%3u . %3u . %3u . %3u%n", &a, &b, &c, &d, &n) && n == length && address_range_ispart(a) && address_range_ispart(b) && address_range_ispart(c) && address_range_ispart(d)) ||
        (length > 4 && 3 == sscanf(start, "%3u . %3u . %3u%n", &a, &b, &c, &n) && n == length && address_range_ispart(a) && address_range_ispart(b) && address_range_ispart(c)) ||
        (length > 2 && 2 == sscanf(start, "%3u . %3u%n", &a, &b, &n) && n == length && address_range_ispart(a) && address_range_ispart(b)) ||
        (length > 0 && 1 == sscanf(start, "%3u%n", &a, &n) && n == length && address_range_ispart(a)) ||
        (length == 1 && ANY_HOST_CHAR == *start)) {
        address = address_range_address(a, b, c, d);
        bits = (d != 0) ? 32 : ((c != 0) ? 24 : ((b != 0) ? 16 : ((a != 0) ? 8 : 0)));
        netmask = address_range_netmask(bits);
        if (address_ptr) *address_ptr = address;
        if (netmask_ptr) *netmask_ptr = netmask;
        retval = 1;
    }
    return retval;
}

static unsigned int address_range_maskvalue(start, end, netmask_ptr)
char 		*start;
char 		*end;
unsigned long  	*netmask_ptr;
{
    unsigned int retval = 0, bits = 32;
    int n = 0, length;
    unsigned long netmask = INADDR_ANY;
    char c;
    while (start <= end && address_range_ignore(*(end-1))) end--;
    while (start <= end && address_range_ignore(*start)) start++;
    length = end - start;
    if (length > 0) {
        retval = 1;
        c = toupper(*start);
        if (1 == length && 'A' == c) {
            netmask = address_range_netmask(8);
        } else if (1 == length && 'B' == c) {
            netmask = address_range_netmask(16);
        } else if (1 == length && 'C' == c) {
            netmask = address_range_netmask(24);
        } else if (1 == sscanf(start, "%3u%n", &bits, &n) && n == length && address_range_isshift(bits)) {
            netmask = address_range_netmask(bits);
        } else {
            retval = address_range_value(start, end, &netmask, NULL);
        }
        if (retval && netmask_ptr) *netmask_ptr = netmask;
    }
    return retval;
}

static char *address_range_find(start, end, c)
char 			*start;
char 			*end;
char 			c;
{
    char *cp;
    if (start < end) {
        for (cp = start; cp < end; cp++) {
            if (*cp == c) return cp;
        }
    }
    return NULL;
}

unsigned int address_range(string, range_ptr)
char 			*string;
address_range_ptr_t 	range_ptr;
{
    char *start = string, *end = string+strlen(string), *range_separator, *mask_separator;
    unsigned int retval = 1;
    unsigned long lower_address = INADDR_NONE, upper_address = INADDR_NONE, lower_mask = INADDR_ANY, upper_mask = INADDR_ANY, netmask = INADDR_ANY;
    while (start <= end && address_range_ignore(*(end-1))) end--;
    while (start <= end && address_range_ignore(*start)) start++;
    range_separator = (char *)address_range_find(start, end, RANGE_SEP_CHAR);
    if (!range_separator) range_separator = start-1;
    mask_separator = (char *)address_range_find(range_separator+1, end, MASK_SEP_CHAR);
    if (!mask_separator) mask_separator = end;
    if (retval && mask_separator > range_separator+1) {
        retval = address_range_value(range_separator+1, mask_separator, &upper_address, &upper_mask);
    }
    if (retval && range_separator > start) {
        retval = address_range_value(start, range_separator, &lower_address, &lower_mask);
    } else {
        lower_address = upper_address;
        lower_mask = upper_mask;
    }
    if (retval && end > mask_separator+1) {
        retval = address_range_maskvalue(mask_separator+1, end, &netmask);
    } else {
        netmask = lower_mask | upper_mask;
    }
    if (retval && range_ptr) {
        lower_address &= netmask;
        upper_address &= netmask;
        range_ptr->lower_address = address_range_min(lower_address, upper_address);
        range_ptr->upper_address = address_range_max(lower_address, upper_address);
        range_ptr->netmask = netmask;
    }
    return retval;
}

unsigned int address_range_match(range, address)
address_range_t		range;
unsigned long 		address;
{
    unsigned long masked_address;
    masked_address = address & range.netmask;
    return (range.lower_address <= masked_address && masked_address <= range.upper_address);
}
