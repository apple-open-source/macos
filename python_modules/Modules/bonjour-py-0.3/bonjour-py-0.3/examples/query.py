#!/usr/bin/python
#
# Bonjour meta query sample
#
# Queries for all Bonjour-advertised service types
# 
# Usage: python query.py
#

import sys
import bonjour
import select
import struct

PTR_RECORD_TYPE = 12
INTERNET_CLASS_TYPE = 1
SERVICE_ADD = 2


############################################################################
#
# Browse for service types registered on the network
#
############################################################################


# Callback for service query
def QueryCallback(sdRef,flags,interfaceIndex,
                    errorCode,fullname,rrtype,
                    rrclass,rdlen,rdata,
                    ttl,userdata):
    # Resource records are encoded in length/data pairs
    # so we unpack the whole thing and then handle each
    # pair individually
    print 'rdlen, rdata', rdlen, rdata
    st = struct.unpack('%dc' % (rdlen-1), rdata)
    l1 = ord(st[0])
    l2 = ord(st[l1+1])
    typ = ''.join(st[1:l1+1])
    klass = ''.join(st[l1+2:l1+l2+2])
    serviceType = typ + '.' + klass

    if flags & SERVICE_ADD:
        print serviceType
    
    elif flags == 0:
        pass#print 'Service type removed:', serviceType


# Allocate a service discovery ref and browse for the specified service type
serviceRef = bonjour.AllocateDNSServiceRef()
ret   = bonjour.pyDNSServiceQueryRecord(serviceRef,
                                    0,  # no flags
                                    0,  # all network interfaces
                                    "_services._dns-sd._udp.local.",  # meta-query record name
                                    PTR_RECORD_TYPE,
                                    INTERNET_CLASS_TYPE, 
                                    QueryCallback,  # callback function ptr
                                    None)
if ret != bonjour.kDNSServiceErr_NoError:
    print "ret = %d; exiting" % ret
    sys.exit(1)

# Get socket descriptor and loop                       
fd = bonjour.DNSServiceRefSockFD(serviceRef)
while 1:
    ret = select.select([fd],[],[])
    ret = bonjour.DNSServiceProcessResult(serviceRef)

# Deallocate the service discovery ref
bonjour.DNSServiceRefDeallocate(serviceRef)



