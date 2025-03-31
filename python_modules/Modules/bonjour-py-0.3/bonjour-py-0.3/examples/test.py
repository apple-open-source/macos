#!/usr/bin/python2

import sys
import bonjour
import select
import time
import threading
import socket

#
# Globals 
#
waitEvent = threading.Event()
resolveEvent = threading.Event()


#
# Callback functions
#

# Register 
def RegisterCallback(sdRef,flags,errorCode,name,regtype,domain,userdata):
    if name == servicename:
        sys.stderr.write( "Service registered\n")
        waitEvent.set()

# Resolve
def ResolveCallback(sdRef,flags,interfaceIndex,
                    errorCode,fullname,hosttarget,
                    port,txtLen,txtRecord,userdata):
    if fullname.split('.')[0] == servicename:
        sys.stderr.write( "\nResolved service %s: host,port=%s,%d\n" % (fullname,host,port) )
        resolveEvent.set()

# Browse
def BrowseCallback(sdRef,flags,interfaceIndex,
             errorCode,serviceName,regtype,
             replyDomain,userdata):
    if serviceName == servicename and flags & bonjour.kDNSServiceFlagsAdd:
        sys.stderr.write( "Found service; resolving...\n")

        waitEvent.set()
        
        sdRef2 = bonjour.AllocateDNSServiceRef()
        ret = bonjour.pyDNSServiceResolve(sdRef2,
                                          0,
                                          0,
                                          serviceName,
                                          regtype,
                                          replyDomain,
                                          ResolveCallback,
                                          None );

        resolveEvent.clear()
        while not resolveEvent.isSet():
            bonjour.DNSServiceProcessResult(sdRef2)


#
# Main program
#


flags = 0
interfaceIndex = 0
servicename = "MyService"
regtype = "_test._tcp"
domain = "local."
host = socket.gethostname()
port = 23
txtRecordLength = 0
txtRecord = ""
userdata = None

sys.stderr.write("Register service: %s\n" % (servicename,))
serviceRef = bonjour.AllocateDNSServiceRef()
ret = bonjour.pyDNSServiceRegister(serviceRef, 
                              flags,                  
                              interfaceIndex,                  
                              servicename,        
                              regtype,            
                              domain,           
                              host,           
                              port,               
                              txtRecordLength,                  
                              txtRecord,               
                              RegisterCallback,   
                              userdata)
if ret != bonjour.kDNSServiceErr_NoError:
    print "register: ret = %d; exiting" % ret
    sys.exit(1)

# Get the socket and loop
fd = bonjour.DNSServiceRefSockFD(serviceRef)
while not waitEvent.isSet():
    ret = select.select([fd],[],[])
    ret = bonjour.DNSServiceProcessResult(serviceRef)


sys.stderr.write("\nBrowsing for service\n")
ret = bonjour.pyDNSServiceBrowse(  serviceRef,  
                              0,                   
                              0,                   
                              regtype,             
                              'local.',            
                              BrowseCallback,      
                              None)                
if ret != bonjour.kDNSServiceErr_NoError:
    print "browse: ret = %d; exiting" % ret
    sys.exit(1)

# Block until service is found                     
fd = bonjour.DNSServiceRefSockFD(serviceRef)
waitEvent.clear()
while not waitEvent.isSet():
    ret = select.select([fd],[],[])
    ret = bonjour.DNSServiceProcessResult(serviceRef)
print "Found service: %s; resolving" % (servicename)

# Block until service is resolved
while not resolveEvent.isSet():
    time.sleep(.5)



# Cleanup
bonjour.DNSServiceRefDeallocate(serviceRef)


