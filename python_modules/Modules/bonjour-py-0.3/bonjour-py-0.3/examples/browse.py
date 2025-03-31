#!/usr/bin/python2
#
# Bonjour browsing sample
# 
# Usage: python browse.py <serviceType>
#
# e.g.  python browse.py _daap._tcp


import sys
import bonjour
import select


# Callback for service resolving
def ResolveCallback(sdRef,flags,interfaceIndex,
                    errorCode,fullname,hosttarget,
                    port,txtLen,txtRecord,userdata):
    print "Service: ", fullname, hosttarget, port, "  flags: ", flags
    print "   userdata = ", userdata
    print " - ", unicode(fullname,'utf-8').encode('ascii','replace')

# Callback for service browsing
def BrowseCallback(sdRef,flags,interfaceIndex,
             errorCode,serviceName,regtype,
             replyDomain,
             userdata):
    if flags & bonjour.kDNSServiceFlagsAdd:
        sdRef2 = bonjour.AllocateDNSServiceRef()
        ret = bonjour.pyDNSServiceResolve(sdRef2,
                                          0,
                                          0,
                                          serviceName,
                                          regtype,
                                          replyDomain,
                                          ResolveCallback,
                                          None );

        bonjour.DNSServiceProcessResult(sdRef2)
        
    elif flags == 0:
        print "Service removed: ", serviceName, regtype
    

# Allocate a service discovery ref and browse for the specified service type
serviceRef = bonjour.AllocateDNSServiceRef()
ret = bonjour.pyDNSServiceBrowse(  serviceRef,   # DNSServiceRef         *sdRef,
                              0,                    # DNSServiceFlags       flags,                        
                              0,                    # uint32_t              interfaceIndex,               
                              sys.argv[1],          # const char            *regtype,                     
                              'local.',             # const char            *domain,    /* may be NULL */ 
                              BrowseCallback,       # DNSServiceBrowseReply callBack,                     
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

