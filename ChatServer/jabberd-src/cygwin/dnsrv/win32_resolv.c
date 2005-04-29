/*
 * Copyright (c) 1983, 1989, 1993
 *    The Regents of the University of California.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */



#include <w32api/windows.h>
#include <w32api/iphlpapi.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <stdlib.h>
#include <sys/unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "win32_resolv.h"


// misc definitions
#define BUF_SIZE 8096
#define DNS_PORT 53
#define UDP_PROTO 17
#define TCP_PROTO 6

// internal error codes
#define RESP_TRYTCP -1
#define RESP_TRYNEXTSERVER -2
#define RESP_UNKNOWNHOST -3


// Possible opcodes for query
#define OPCODE_QUERY 0
#define OPCODE_IQUERY 1
#define OPCODE_STATUS 2



/**
 * Perform a DNS lookup using UDP
 *
 * @param dnsServer DNS Server to look up
 * @param sendPktBuf Buffer containing the packet to send
 * @param sendPktSize Size of the packet to send
 * @param recvPktBuf Buffer to put received packet in
 * @param recvPktSize Size of the recvPktBuf buffer
 * @param expectedId Id counter to identify response packets
 * @return Received buffer size, or <0 on failure
 */
int dnsUdp(struct in_addr dnsServer, 
           char* sendPktBuf, int sendPktSize,
           char* recvPktBuf, int recvPktSize,
           int* expectedId);

/**
 * Perform a DNS lookup using TCP
 *
 * @param dnsServer DNS Server to look up
 * @param sendPktBuf Buffer containing the packet to send
 * @param sendPktSize Size of the packet to send
 * @param recvPktBuf Buffer to put received packet in
 * @param recvPktSize Size of the recvPktBuf buffer
 * @param expectedId Id counter to identify response packets
 * @return Received buffer size, or <0 on failure
 */
int dnsTcp(struct in_addr dnsServer, 
           char* sendPktBuf, int sendPktSize,
           char* recvPktBuf, int recvPktSize,
           int* expectedId);


/**
 * Format a DNS query packet. Id will be set to 0
 *
 * @param dstBuf Where to put the packet
 * @param bufSize Size of the above
 * @param name Name of service to look up
 * @param domain Domain To look it up in
 * @param qClass Class of query
 * @param qType Type of query
 * @return Packet size on success, <0 on failure
 */
int formatPacket(char* dstBuf, int bufSize,
                 const char* name, const char* domain,
                 u_int16_t qClass, u_int16_t qType);


/**
 * Retrieve list of DNS servers known to the machine
 *
 * @param list Updated to point to a buffer of ULONGs. This will be malloced, 
 * so it is up to you to free it when finished. They are already in network 
 * order. Will be NULL terminated.
 * @return 0 on success, nonzero error code on failure
 */
int getDnsServers(struct in_addr** list);

/*
 * ns_name_unpack(msg, eom, src, dst, dstsiz)
 *      Unpack a domain name from a message, source may be compressed.
 * return:
 *      -1 if it fails, or consumed octets if it succeeds.
 */
int ns_name_unpack(const u_char *msg, const u_char *eom, const u_char *src,
                   u_char *dst, size_t dstsiz);

/*
 * ns_name_uncompress(msg, eom, src, dst, dstsiz)
 *      Expand compressed domain name to presentation format.
 * return:
 *      Number of bytes read out of `src', or -1 (with errno set).
 * note:
 *      Root domain returns as "." not "".
 */
int ns_name_uncompress(const u_char *msg, const u_char *eom, const u_char *src,
                       char *dst, size_t dstsiz);

/*
 * ns_name_ntop(src, dst, dstsiz)
 *      Convert an encoded domain name to printable ascii as per RFC1035.
 * return:
 *      Number of bytes written to buffer, or -1 (with errno set)
 * notes:
 *      The root is returned as "."
 *      All other domains are returned in non absolute form
 */
int ns_name_ntop(const u_char *src, char *dst, size_t dstsiz);


/*
 * Expand compressed domain name 'comp_dn' to full domain name.
 * 'msg' is a pointer to the begining of the message,
 * 'eomorig' points to the first location after the message,
 * 'exp_dn' is a pointer to a buffer of size 'length' for the result.
 * Return size of compressed name or -1 if there was an error.
 */
int dn_expand(const u_char *msg, const u_char *eom, const u_char *src,
              char *dst, int dstsiz);

/*
 * special(ch)
 *      Thinking in noninternationalized USASCII (per the DNS spec),
 *      is this characted special ("in need of quoting") ?
 * return:
 *      boolean.
 */
static int special(int ch);

/*
 * printable(ch)
 *      Thinking in noninternationalized USASCII (per the DNS spec),
 *      is this character visible and not a space when printed ?
 * return:
 *      boolean.
 */
static int printable(int ch);


// list of digits
static const char       digits[] = "0123456789";






/**
 * Retrieve list of DNS servers known to the machine
 *
 * @param list Updated to point to a buffer of ULONGs. This will be malloced, 
 * so it is up to you to free it when finished. They are already in network 
 * order. Will be NULL terminated.
 * @return 0 on success, nonzero error code on failure
 */
int getDnsServers(struct in_addr** list) {
  int err;
  int count;
  ULONG fixedInfoSize;
  PFIXED_INFO fixedInfo;
  PIP_ADDR_STRING addrStr;
  
  // get size of FIXED_INFO structure
  if (!(err = GetNetworkParams(NULL, &fixedInfoSize))) {
    if (err != ERROR_BUFFER_OVERFLOW) {
      return err;
    }
  }
  
  // alloc memory
  if (!(fixedInfo = malloc(fixedInfoSize))) {
    return -ENOMEM;
  }
  
  // Get the network params
  if (!(err = GetNetworkParams(fixedInfo, &fixedInfoSize))) {
    // First of all, count the number of servers
    count = 1;
    addrStr = fixedInfo->DnsServerList.Next;
    while(addrStr) {
      count++;
      addrStr = addrStr->Next;
    }

    // I know this cannot happen, but just in case someone changes the above
    if (count == 0) {
      *list = NULL;
      return 0;
    }

    // Allocate memory to store 'em in
    if (!(*list = (struct in_addr*) malloc(sizeof(struct in_addr) * count+1))) {
      return -ENOMEM;
    }
    
    // Now, copy 'em into the list
    count = 0;
    (*list)[count++].s_addr = 
      inet_addr(fixedInfo->DnsServerList.IpAddress.String);
    addrStr = fixedInfo->DnsServerList.Next;
    while(addrStr) {
      (*list)[count++].s_addr = inet_addr(addrStr->IpAddress.String);
      addrStr = addrStr->Next;
    }
    (*list)[count].s_addr = 0;
  } else {
    free(fixedInfo);
    return err;
  }
  
  // OK!
  free(fixedInfo);
  return 0;
}

/**
 * Format a DNS query packet. Id will be set to 0
 *
 * @param dstBuf Where to put the packet
 * @param bufSize Size of the above
 * @param name Name of service to look up
 * @param domain Domain To look it up in
 * @param qClass Class of query
 * @param qType Type of query
 * @return Packet size on success, <0 on failure
 */
int formatPacket(char* dstBuf, int bufSize,
                 const char* name, const char* domain,
                 u_int16_t qClass, u_int16_t qType) {
  int offset;
  int lastCountOffset;
  HEADER* headerSection;
  char tmpName[NS_MAXDNAME+10];



  // slap the two names together correctly
  if ((name == NULL) && (domain == NULL)) {
    return -1;
  }
  if ((name != NULL) && (domain == NULL)) {
    return -1;
  }
  if (name != NULL) {
    strcpy(tmpName, name);
  }
  if (tmpName[strlen(tmpName)-1] != '.') {
    strcat(tmpName, ".");
  }
  strcat(tmpName, domain);

  // check if buffer is big enough
  if ((sizeof(HEADER) + strlen(tmpName) + 4) > bufSize) {
    return -ENOMEM;
  }

  // zero the buffer
  memset(dstBuf, 0, bufSize);

  // make up DNS header section
  headerSection = (HEADER*) dstBuf;
  headerSection->id = htons(0);
  headerSection->rd = 1;
  headerSection->qdcount = htons(1); // One single query present

  // Fill out the question section
  offset = sizeof(HEADER);

  // We're now doing the name to look up, separated by NULLs
  lastCountOffset = offset;
  offset++; // Keep one byte free for count of first component of name
  strcpy(dstBuf + offset, tmpName);

  // Now, loop through the string we've just copied converting '.' to NULL
  while(dstBuf[offset]) {
    // If we've hit a '.', update the PREVIOUS 
    // counter to the size of the bit BEFORE the '.'
    if (dstBuf[offset] == '.') {
      dstBuf[lastCountOffset] = offset - lastCountOffset -1;
      lastCountOffset = offset;
      dstBuf[offset] = 0;
    }
    
    // next char
    offset++;
  }

  // Finally, need to update the count for the bit betweent the last 
  // dot and the end of string. Note: If the last character is a '.', it will 
  // already have been turned into a NULL
  if (dstBuf[offset - 1] != 0) { 
    // if the last character was NOT '.', normal update is OK
    dstBuf[lastCountOffset] = offset - lastCountOffset -1;
    // And woo! We've already GOT a terminating NULL character from the
    // string copy
    offset++;
  } else {
    // last character was a '.'. Therefore, we should use THAT as the
    // terminating null, and not the extra one which is now present at the 
    // end of the string. Therefore, just don't bother incrementing the count!
  }

  // Add in the type and class
  *((unsigned short*) (dstBuf + offset)) = htons(qType);
  offset+=2;
  *((unsigned short*) (dstBuf + offset)) = htons(qClass);
  offset+=2;

  // Finally, return the length
  return offset;
}



/**
 * Perform a DNS lookup
 *
 * @param name Name of service to look up
 * @param domain Domain To look it up in
 * @param qClass Class of query
 * @param qType Type of query
 * @param dnsServers DNS servers to try
 * @param dstBuffer Where to put received DNS packet
 * @param dstBufferSize Size of dstBuffer
 * @return Size of received packet, or <0 on failure
 */
int dnsLookup(const char* name, const char* domain, 
              u_int16_t qClass, u_int16_t qType,
              struct in_addr* dnsServers, 
              char* dstBuffer, int dstBufferSize) {
  char tmpBuf[BUF_SIZE];
  int pktSize;
  int dnsServerCount;
  int retries;
  int count;
  int expectedId;


  // start off with expectedId 0
  expectedId = 0;
  
  // OK, format the packet & check
  if ((pktSize = formatPacket(tmpBuf, BUF_SIZE,
                              name, domain,
                              qClass, qType)) < 0) {
    return -1;
  }
  
  // retry the lookup
  for(retries=0; retries < 5; retries++) {

    // Now, try each DNS server
    dnsServerCount = 0;
    while(dnsServers[dnsServerCount].s_addr) {
      // try UDP DNS if the packet ain't too big
      if (pktSize <= NS_PACKETSZ) {
        count = dnsUdp(dnsServers[dnsServerCount], 
                       tmpBuf, pktSize,
                       dstBuffer, dstBufferSize,
                       &expectedId);
        switch(count) {
        case RESP_TRYTCP:
        case RESP_TRYNEXTSERVER:
          break;

        case RESP_UNKNOWNHOST:
          return -1;

        default:
          return count;
        }
      } else {
        count = RESP_TRYTCP;
      }

      // try TCP connection?
      if (count == RESP_TRYTCP) {
        count = dnsTcp(dnsServers[dnsServerCount], 
                       tmpBuf, pktSize,
                       dstBuffer, dstBufferSize,
                       &expectedId);
        switch(count) {
        case RESP_TRYTCP:
        case RESP_TRYNEXTSERVER:
          break;
          
        case RESP_UNKNOWNHOST:
          return -1;
          
        default:
          return count;
        }
      }

      // Move on to next server
      dnsServerCount++;
    }
  }

  // if we get here, we have not found a valid address
  return -1;
}


/**
 * Perform a DNS lookup using UDP
 *
 * @param dnsServer DNS Server to look up
 * @param sendPktBuf Buffer containing the packet to send
 * @param sendPktSize Size of the packet to send
 * @param recvPktBuf Buffer to put received packet in
 * @param recvPktSize Size of the recvPktBuf buffer
 * @param expectedId Id counter to identify response packets
 * @return Received buffer size, or <0 on failure
 */
int dnsUdp(struct in_addr dnsServer, 
           char* sendPktBuf, int sendPktSize,
           char* recvPktBuf, int recvPktSize,
           int* expectedId) {
  int sockFd;
  struct sockaddr_in server;
  struct sockaddr_in local;
  struct sockaddr_in from;
  int count;
  int tmp;
  struct pollfd sockPollFd;
  HEADER* header;


  // setup sockaddr for local machine
  memset(&local, 0, sizeof(struct sockaddr_in));
  local.sin_family = AF_INET;
  local.sin_port = 0;
  local.sin_addr.s_addr = INADDR_ANY;

  // setup sockaddr for remote server
  memset(&server, 0, sizeof(struct sockaddr_in));
  server.sin_family = AF_INET;
  server.sin_port = htons(DNS_PORT);
  server.sin_addr.s_addr = dnsServer.s_addr;

  // Create UDP socket 
  if ((sockFd = socket(AF_INET, SOCK_DGRAM, UDP_PROTO)) < 0) {
    return RESP_TRYNEXTSERVER;
  }

  // bind input socket to local machine
  if (bind(sockFd, 
           (struct sockaddr*) &local, sizeof(struct sockaddr_in)) < 0) {
    close(sockFd);
    return RESP_TRYNEXTSERVER;
  }

  // set the expectedId in the packet
  ((HEADER*) sendPktBuf)->id = htons(*expectedId);
  (*expectedId)++;

  // send the packet to the DNS server
  if (sendto(sockFd, 
             sendPktBuf, sendPktSize,
             0,
             (struct sockaddr*) &server,
             sizeof(struct sockaddr_in)) < 0) {
    close(sockFd);
    return RESP_TRYNEXTSERVER;
  }

  // setup sockaddr for received packet
  memset(&from, 0, sizeof(from));
  from.sin_family = AF_INET;
  tmp = sizeof(from);
  memset(recvPktBuf, 0, recvPktSize);


  // This is where we listen for connections
  while(1) {

    // Wait for a response
    sockPollFd.fd = sockFd;
    sockPollFd.events = POLLIN | POLLERR;
    sockPollFd.revents = 0;
    if (poll(&sockPollFd, 1, 7000) <= 0) {
      close(sockFd);
      return RESP_TRYNEXTSERVER;
    }
    
    // OK, check we have got some data
    if (sockPollFd.revents != POLLIN) {
      close(sockFd);
      return RESP_TRYNEXTSERVER;
    }    
    
    // read it
    if ((count = recvfrom(sockFd,
                          recvPktBuf,
                          recvPktSize,
                          0,
                          (struct sockaddr*) &from,
                          &tmp)) < 0) {
      close(sockFd);
      return RESP_TRYNEXTSERVER;
    }
    
    // check packet is big enough
    header = (HEADER*) recvPktBuf;
    if (count < sizeof(HEADER)) {
      // packet too small. try next server
      close(sockFd);
      return RESP_TRYNEXTSERVER;
    }
    
    // incorrect ID => old response. start listening again
    if (ntohs(header->id) != ((*expectedId) - 1)) {
      continue;
    }

    // packet truncated. try tcp
    if (header->tc) {
      close(sockFd);
      return RESP_TRYTCP;
    }

    // not a response. try next server
    if (!header->qr) {
      close(sockFd);
      return RESP_TRYNEXTSERVER;
    }

    // unknown host. exit lookup
    if (header->rcode == RCODE_NAMEERROR) {
      close(sockFd);
      return RESP_UNKNOWNHOST;
    }
  
    // error. try next server
    if (header->rcode != RCODE_OK) {
      close(sockFd);
      return RESP_TRYNEXTSERVER;
    }
  
    // There were no actual records! try next server
    if (header->ancount == 0) {
      close(sockFd);
      return RESP_TRYNEXTSERVER;
    }   

    // If we get here, we are OK! exit the loop
    break;
  }

  // OK! return the count
  close(sockFd);
  return count;
}



/**
 * Perform a DNS lookup using TCP
 *
 * @param dnsServer DNS Server to look up
 * @param sendPktBuf Buffer containing the packet to send
 * @param sendPktSize Size of the packet to send
 * @param recvPktBuf Buffer to put received packet in
 * @param recvPktSize Size of the recvPktBuf buffer
 * @param expectedId Id counter to identify response packets
 * @return Received buffer size, or <0 on failure
 */
int dnsTcp(struct in_addr dnsServer, 
           char* sendPktBuf, int sendPktSize,
           char* recvPktBuf, int recvPktSize,
           int* expectedId) {
  int sockFd;
  struct sockaddr_in server;
  struct sockaddr_in local;
  int count;
  struct pollfd sockPollFd;
  char tmpBuf[10];
  HEADER* header;



  // setup sockaddr for local machine
  memset(&local, 0, sizeof(struct sockaddr_in));
  local.sin_family = AF_INET;
  local.sin_port = 0;
  local.sin_addr.s_addr = INADDR_ANY;

  // setup sockaddr for remote server
  memset(&server, 0, sizeof(struct sockaddr_in));
  server.sin_family = AF_INET;
  server.sin_port = htons(DNS_PORT);
  server.sin_addr.s_addr = dnsServer.s_addr;
  
  // Create TCP socket 
  if ((sockFd = socket(AF_INET, SOCK_STREAM, TCP_PROTO)) < 0) {
    return RESP_TRYNEXTSERVER;
  }

  // bind input socket to local machine
  if (bind(sockFd, 
           (struct sockaddr*) &local, sizeof(struct sockaddr_in)) < 0) {
    close(sockFd);
    return RESP_TRYNEXTSERVER;
  }

  // connect to the remote server
  if (connect(sockFd,
              (struct sockaddr*) &server, sizeof(struct sockaddr_in)) < 0) {
    close(sockFd);
    return RESP_TRYNEXTSERVER;
  }
  
  // set the expectedId in the packet
  ((HEADER*) sendPktBuf)->id = htons(*expectedId);
  (*expectedId)++;

  // send the size to the DNS server
  *((unsigned short*) tmpBuf) = htons(sendPktSize);
  if (send(sockFd, 
           tmpBuf, 2,
           0) < 0) {
    close(sockFd);
    return RESP_TRYNEXTSERVER;
  } 

  // send the packet to the DNS server
  if (send(sockFd, 
           sendPktBuf, sendPktSize,
           0) < 0) {
    close(sockFd);
    return RESP_TRYNEXTSERVER;
  }

  // This is where we listen for connections
  while(1) {

    // Wait for a response
    sockPollFd.fd = sockFd;
    sockPollFd.events = POLLIN | POLLERR;
    sockPollFd.revents = 0;
    if (poll(&sockPollFd, 1, 7000) <= 0) {
      close(sockFd);
      return RESP_TRYNEXTSERVER;
    }
    
    // OK, check we have got some data
    if (sockPollFd.revents != POLLIN) {
      close(sockFd);
      return RESP_TRYNEXTSERVER;
    }    
    
    // read it
    if ((count = recv(sockFd,
                      recvPktBuf,
                      recvPktSize,
                      0)) < 0) {
      close(sockFd);
      return RESP_TRYNEXTSERVER;
    }
    
    // check packet is big enough
    header = (HEADER*) (recvPktBuf + 2);
    if (count < sizeof(HEADER)) {
      // packet too small. try next server
      close(sockFd);
      return RESP_TRYNEXTSERVER;
    }
    
    // incorrect ID => old response. start listening again
    if (ntohs(header->id) != ((*expectedId) - 1)) {
      continue;
    }
    
    // packet truncated. try next server
    if (header->tc) {
      close(sockFd);
      return RESP_TRYNEXTSERVER;
    }
    
    // not a response. try next server
    if (!header->qr) {
      close(sockFd);
      return RESP_TRYNEXTSERVER;
    }

    // unknown host. exit lookup
    if (header->rcode == RCODE_NAMEERROR) {
      close(sockFd);
      return RESP_UNKNOWNHOST;
    }
    
    // error. try next server
    if (header->rcode != RCODE_OK) {
      close(sockFd);
      return RESP_TRYNEXTSERVER;
    }
    
    // There were no actual records! try next server
    if (header->ancount == 0) {
      close(sockFd);
      return RESP_TRYNEXTSERVER;
    }   
    
    // If we get here, we are OK. Terminate the loop
    break;
  }

  // need to copy the packet BACK two bytes, 
  // since it has the size of it prepended
  memcpy(recvPktBuf, recvPktBuf+2, count-2);

  // OK! return the count
  close(sockFd);
  return count - 2;
}




/**
 * Query domain
 *
 * @param name Name of service to query for
 * @param domain Name of domain service is in
 * @param class Class of query
 * @param type Type of record to retrieve
 * @param dstBuffer Space to put received packet
 * @param dstLength Size of received packet buffer
 * @return Size of packet received, or < 0 on failure
 */
int res_querydomain(const char *name,
                    const char *domain,
                    int class, int type,
                    u_char *dstBuffer,
                    int dstLength) {
  struct in_addr* dnsServers;
  int pktSize;

  // find the list of DNS servers on this machine
  if (getDnsServers(&dnsServers)) {
    return -1;
  }
  
  // do the lookup
  pktSize = 
    dnsLookup(name, domain, class, type, 
              dnsServers, dstBuffer, dstLength);
  free(dnsServers);
  
  // Finally return the value
  return(pktSize);
}




// -----------------------------------------------------------
// Everything below this line is taken from bind v8.2.3
// See copyright notices above

/*
 * Expand compressed domain name 'comp_dn' to full domain name.
 * 'msg' is a pointer to the begining of the message,
 * 'eomorig' points to the first location after the message,
 * 'exp_dn' is a pointer to a buffer of size 'length' for the result.
 * Return size of compressed name or -1 if there was an error.
 */
int
dn_expand(const u_char *msg, const u_char *eom, const u_char *src,
          char *dst, int dstsiz)
{
        int n = ns_name_uncompress(msg, eom, src, dst, (size_t)dstsiz);

        if (n > 0 && dst[0] == '.')
                dst[0] = '\0';
        return (n);
}


/*
 * ns_name_ntop(src, dst, dstsiz)
 *      Convert an encoded domain name to printable ascii as per RFC1035.
 * return:
 *      Number of bytes written to buffer, or -1 (with errno set)
 * notes:
 *      The root is returned as "."
 *      All other domains are returned in non absolute form
 */
int
ns_name_ntop(const u_char *src, char *dst, size_t dstsiz) {
        const u_char *cp;
        char *dn, *eom;
        u_char c;
        u_int n;

        cp = src;
        dn = dst;
        eom = dst + dstsiz;

        while ((n = *cp++) != 0) {
                if ((n & NS_CMPRSFLGS) != 0) {
                        /* Some kind of compression pointer. */
                        errno = EMSGSIZE;
                        return (-1);
                }
                if (dn != dst) {
                        if (dn >= eom) {
                                errno = EMSGSIZE;
                                return (-1);
                        }
                        *dn++ = '.';
                }
                if (dn + n >= eom) {
                        errno = EMSGSIZE;
                        return (-1);
                }
                for ((void)NULL; n > 0; n--) {
                        c = *cp++;
                        if (special(c)) {
                                if (dn + 1 >= eom) {
                                        errno = EMSGSIZE;
                                        return (-1);
                                }
                                *dn++ = '\\';
                                *dn++ = (char)c;
                        } else if (!printable(c)) {
                                if (dn + 3 >= eom) {
                                        errno = EMSGSIZE;
                                        return (-1);
                                }
                                *dn++ = '\\';
                                *dn++ = digits[c / 100];
                                *dn++ = digits[(c % 100) / 10];
                                *dn++ = digits[c % 10];
                        } else {
                                if (dn >= eom) {
                                        errno = EMSGSIZE;
                                        return (-1);
                                }
                                *dn++ = (char)c;
                        }
                }
        }
        if (dn == dst) {
                if (dn >= eom) {
                        errno = EMSGSIZE;
                        return (-1);
                }
                *dn++ = '.';
        }
        if (dn >= eom) {
                errno = EMSGSIZE;
                return (-1);
        }
        *dn++ = '\0';
        return (dn - dst);
}

/*
 * ns_name_uncompress(msg, eom, src, dst, dstsiz)
 *      Expand compressed domain name to presentation format.
 * return:
 *      Number of bytes read out of `src', or -1 (with errno set).
 * note:
 *      Root domain returns as "." not "".
 */
int
ns_name_uncompress(const u_char *msg, const u_char *eom, const u_char *src,
                   char *dst, size_t dstsiz)
{
        u_char tmp[NS_MAXCDNAME];
        int n;
        
        if ((n = ns_name_unpack(msg, eom, src, tmp, sizeof tmp)) == -1)
                return (-1);
        if (ns_name_ntop(tmp, dst, dstsiz) == -1)
                return (-1);
        return (n);
}


/*
 * ns_name_unpack(msg, eom, src, dst, dstsiz)
 *      Unpack a domain name from a message, source may be compressed.
 * return:
 *      -1 if it fails, or consumed octets if it succeeds.
 */
int
ns_name_unpack(const u_char *msg, const u_char *eom, const u_char *src,
               u_char *dst, size_t dstsiz)
{
        const u_char *srcp, *dstlim;
        u_char *dstp;
        int n, len, checked;

        len = -1;
        checked = 0;
        dstp = dst;
        srcp = src;
        dstlim = dst + dstsiz;
        if (srcp < msg || srcp >= eom) {
                errno = EMSGSIZE;
                return (-1);
        }
        /* Fetch next label in domain name. */
        while ((n = *srcp++) != 0) {
                /* Check for indirection. */
                switch (n & NS_CMPRSFLGS) {
                case 0:
                        /* Limit checks. */
                        if (dstp + n + 1 >= dstlim || srcp + n >= eom) {
                                errno = EMSGSIZE;
                                return (-1);
                        }
                        checked += n + 1;
                        *dstp++ = n;
                        memcpy(dstp, srcp, n);
                        dstp += n;
                        srcp += n;
                        break;

                case NS_CMPRSFLGS:
                        if (srcp >= eom) {
                                errno = EMSGSIZE;
                                return (-1);
                        }
                        if (len < 0)
                                len = srcp - src + 1;
                        srcp = msg + (((n & 0x3f) << 8) | (*srcp & 0xff));
                        if (srcp < msg || srcp >= eom) {  /* Out of range. */
                                errno = EMSGSIZE;
                                return (-1);
                        }
                        checked += 2;
                        /*
                         * Check for loops in the compressed name;
                         * if we've looked at the whole message,
                         * there must be a loop.
                         */
                        if (checked >= eom - msg) {
                                errno = EMSGSIZE;
                                return (-1);
                        }
                        break;

                default:
                        errno = EMSGSIZE;
                        return (-1);                    /* flag error */
                }
        }
        *dstp = '\0';
        if (len < 0)
                len = srcp - src;
        return (len);
}

/*
 * special(ch)
 *      Thinking in noninternationalized USASCII (per the DNS spec),
 *      is this characted special ("in need of quoting") ?
 * return:
 *      boolean.
 */
static int special(int ch) {
        switch (ch) {
        case 0x22: /* '"' */
        case 0x2E: /* '.' */
        case 0x3B: /* ';' */
        case 0x5C: /* '\\' */
        /* Special modifiers in zone files. */
        case 0x40: /* '@' */
        case 0x24: /* '$' */
                return (1);
        default:
                return (0);
        }
}

/*
 * printable(ch)
 *      Thinking in noninternationalized USASCII (per the DNS spec),
 *      is this character visible and not a space when printed ?
 * return:
 *      boolean.
 */
static int printable(int ch) {
        return (ch > 0x20 && ch < 0x7f);
}
