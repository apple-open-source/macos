#!/usr/bin/python2 
#
# Bonjour register example
#
# Usage: python register.py <serviceName> <serviceType> <port>
#
# e.g. python register.py myservice _myservicetype._tcp 12345


import sys
import time
import bonjour
import select
import socket


def RegisterCallback(sdRef,flags,errorCode,name,regtype,domain,userdata):
    print "Service registered:", name, regtype
    print "userdata = ", userdata

if len(sys.argv) < 4:
    print "Usage: register.py servicename regtype port"
    sys.exit(1)

servicename = sys.argv[1]
regtype = sys.argv[2]
port = int(sys.argv[3])
userdata = None
if len(sys.argv) > 4:
    userdata = sys.argv[4]
    
hostname = socket.gethostname()

# Allocate a service discovery reference and register the specified service
serviceRef = bonjour.AllocateDNSServiceRef()
ret = bonjour.pyDNSServiceRegister(serviceRef,        # DNSServiceRef           *sdRef,
                              0,                    # DNSServiceFlags         flags,         /* may be 0 */
                              0,                    # uint32_t                interfaceIndex,/* may be 0 */
                              servicename,          # const char              *name,         /* may be NULL */
                              regtype,              # const char              *regtype,
                              'local.',             # const char              *domain,       /* may be NULL */
                              hostname,             # const char              *host,         /* may be NULL */
                              port,                 # uint16_t                port,
                              0,                    # uint16_t                txtLen,
                              "",                   # const void              *txtRecord,    /* may be NULL */
                              RegisterCallback,     # DNSServiceRegisterReply callBack,      /* may be NULL */
                              userdata)

if ret != bonjour.kDNSServiceErr_NoError:
    print "error %d returned; exiting" % ret
    sys.exit(ret)


# Get the socket and loop
fd = bonjour.DNSServiceRefSockFD(serviceRef)
while 1:
    ret = select.select([fd],[],[])
    ret = bonjour.DNSServiceProcessResult(serviceRef)

# Deallocate the service discovery ref
bonjour.DNSServiceRefDeallocate(serviceRef)

