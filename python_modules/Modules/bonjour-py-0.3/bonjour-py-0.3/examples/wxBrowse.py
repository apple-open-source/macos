#!/usr/bin/python2

# wxBrowse.py
#
# Use wx to display a browser for services registered on the local net via bonjour
#
#

import struct
import sys
import time
import bonjour
import select
import threading
import socket

import wx
from wxPython.wx import *
app = None
from wx import VERSION as WXVERSION

from serviceTypes import ServiceTypes

PTR_RECORD_TYPE = 12
INTERNET_CLASS_TYPE = 1

SERVICE_ADD = 2

serviceRefDict = {}

class rendBrowser(wxFrame):
    """
    The main frame for the bonjour service browser
    """
    def __init__(self, parent, ID, title):
        wxFrame.__init__(self, parent, ID, title,
                         wxDefaultPosition, wxSize(450, 300))
        
        staticBox = wxStaticBox(self,-1,'')
        
        mainsz = wxStaticBoxSizer( staticBox,wxVERTICAL )
        self.SetSizer( mainsz )
        
        self.tree = wxTreeCtrl(self,-1,style=wxTR_HIDE_ROOT|wxTR_HAS_BUTTONS|wxTR_TWIST_BUTTONS)
        item = self.tree.AddRoot("")
        mainsz.Add(self.tree, 1, wxEXPAND)
        
        EVT_TREE_ITEM_EXPANDING(self,self.tree.GetId(),self.OnExpandItem)
        
        self.resolvingItem = None
        
        self.serviceTypeItems = {}
       
    def OnExpandItem(self,event):
    
        item = event.GetItem()
        if item == self.tree.GetRootItem():
            return
            
        
        data = self.tree.GetPyData(item)
        if not data:    
            return
        self.tree.DeleteChildren(item)
        name,regtype,replyDomain = data
        
        self.resolvingItem = item
        sdRef2 = bonjour.AllocateDNSServiceRef()
        ret = bonjour.pyDNSServiceResolve(sdRef2,
                                          0,
                                          0,
                                          name,
                                          regtype,
                                          replyDomain,
                                          ResolveCallback,
                                          None)

        bonjour.DNSServiceProcessResult(sdRef2)
        
            
    def ResolveIt(self,data):
        hosttarget,port = data
        
        ip = socket.gethostbyname(hosttarget)
        self.tree.AppendItem(self.resolvingItem,"Host = %s (%s)" % (hosttarget,ip))
        self.tree.AppendItem(self.resolvingItem,"Port = %d" % port)
        
    def AddServiceType(self,serviceType):

        if hasattr(wx,'PlatformInfo') and "unicode" in wx.PlatformInfo: 
            serviceTypeStr = unicode(serviceType,'utf-8')
        else:
            serviceTypeStr = unicode(serviceType,'utf-8').encode('ascii','replace')

        serviceType = serviceTypeStr.split('.')[0]
        serviceType = serviceType[1:]
        if serviceType in ServiceTypes.keys():
            serviceTypeName = ServiceTypes[serviceType] + ' (%s) [%d]'%(serviceTypeStr,0)
        else:
            serviceTypeName = serviceTypeStr + '[%d]' % 0
        item = self.tree.AppendItem(self.tree.GetRootItem(),serviceTypeName)
        self.serviceTypeItems[serviceTypeStr] = item
        
        #self.tree.Expand(self.tree.GetRootItem())
        self.tree.Refresh()

    def RemoveServiceType(self,serviceType):
        serviceTypeNoDot = serviceType[:-1]
        if self.serviceTypeItems.has_key(serviceTypeNoDot):
            itemId = self.serviceTypeItems[serviceTypeNoDot]
            name = self.tree.GetItemText(itemId)
            if name == serviceType:
                self.tree.Delete(itemId)
                self.tree.Refresh()

    def AddService(self,serviceName,serviceType,data):
        serviceTypeNoDot = serviceType[:-1]
        if self.serviceTypeItems.has_key(serviceTypeNoDot):
            itemId = self.serviceTypeItems[serviceTypeNoDot]
            item = self.tree.AppendItem(itemId,serviceName)
            self.tree.SetPyData(item,data)
            self.tree.AppendItem(item,'Nothing')

            self._UpdateServiceCount(itemId)

            self.tree.Refresh()
        
    def RemoveService(self,serviceName,serviceType):
        serviceTypeNoDot = serviceType[:-1]
        if self.serviceTypeItems.has_key(serviceTypeNoDot):
            itemId = self.serviceTypeItems[serviceTypeNoDot]
            if WXVERSION[0] <= 2 and WXVERSION[1] <= 4: 
                cookie = 12312       
                (serviceItemId,cookie) = self.tree.GetFirstChild(itemId,cookie)
            else:
                (serviceItemId,cookie) = self.tree.GetFirstChild(itemId)
            while serviceItemId:
                serviceItemName = self.tree.GetItemText(serviceItemId)
                if serviceItemName == serviceName:
                    self.tree.Delete(serviceItemId)
                (serviceItemId,cookie) = self.tree.GetNextChild(itemId,cookie)

            self._UpdateServiceCount(itemId)

            self.tree.Refresh()
            
    def _UpdateServiceCount(self,itemId):
        serviceTypeText = self.tree.GetItemText(itemId)
        try:
            bracketIndex = serviceTypeText.rindex('[')
        except ValueError:
            bracketIndex = serviceTypeText[-1]

        serviceTypeText = serviceTypeText[:bracketIndex]
        serviceTypeText += '[%d]'%(self.tree.GetChildrenCount(itemId,recursively=false))
        self.tree.SetItemText(itemId,serviceTypeText)



class MyApp(wxApp):

    def OnInit(self):
        self.frame = rendBrowser(NULL, -1, "Bonjour Service Browser")

        self.frame.Show(true)
        self.SetTopWindow(self.frame)
        return true

def ResolveCallback(sdRef,flags,interfaceIndex,
                    errorCode,fullname,hosttarget,
                    port,txtLen,txtRecord,userdata):
    app.frame.ResolveIt( (hosttarget,port) )

def BrowseCallback(sdRef,flags,interfaceIndex,
             errorCode,serviceName,regtype,
             replyDomain,userdata):
             
    global app
    if flags & SERVICE_ADD:
        #print "Service added: ", serviceName, regtype
        
        if hasattr(wx,'PlatformInfo') and "unicode" in wx.PlatformInfo: 
            serviceDisplayName = unicode(serviceName,'utf-8')
        else:
            serviceDisplayName = unicode(serviceName,'utf-8').encode('ascii','replace')
        app.frame.AddService(serviceDisplayName,regtype,(serviceName,regtype,replyDomain))

        
    elif flags == 0:
        #print "Service removed: ", serviceName, regtype
        app.frame.RemoveService(serviceName,regtype)
    
    


############################################################################
#
# Browse for service instances registered on the network
#
############################################################################

def BrowseLoop():
    """
    Loop selecting file descriptors to process browse events
    """
    global serviceRefDict
    while 1:
        #print "serviceRefDict = ", serviceRefDict.keys()
        if not serviceRefDict:
            time.sleep(1)
            continue
        ret = select.select(serviceRefDict.keys(),[],[],1)
        #print "ret = ", ret
        for fd in ret[0]:
            serviceRef = serviceRefDict[fd]
            bonjour.DNSServiceProcessResult(serviceRef)



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
    # This is very ugly; should understand resource records better
    st = struct.unpack('%dc' % (rdlen-1), rdata)
    l1 = ord(st[0])
    l2 = ord(st[l1+1])
    typ = ''.join(st[1:l1+1])
    klass = ''.join(st[l1+2:l1+l2+2])
    serviceType = typ + '.' + klass

    if flags & SERVICE_ADD:
    
        # Add the service type to the browser
        app.frame.AddServiceType(serviceType)

        serviceRef = bonjour.AllocateDNSServiceRef()
        ret = bonjour.pyDNSServiceBrowse(  serviceRef,   # DNSServiceRef         *sdRef,
                                      0,                    # DNSServiceFlags       flags,                        
                                      0,                    # uint32_t              interfaceIndex,               
                                      serviceType,          # const char            *regtype,                     
                                      'local.',             # const char            *domain,    /* may be NULL */ 
                                      BrowseCallback,       # DNSServiceBrowseReply callBack,    
                                      None)                 
        if ret:
            print "ret = %d; exiting" % ret
            sys.exit(1)
        fd = bonjour.DNSServiceRefSockFD(serviceRef)
        serviceRefDict[fd] = serviceRef
    elif flags == 0:
        app.frame.RemoveServiceType(serviceType)

def BrowseServices():

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



def main():
    import os
    global app
    global allTypes
    app = MyApp(0)
    
    # Start a separate thread to loop/handle browse responses
    t = threading.Thread(target=BrowseServices)
    t.start()
    t2 = threading.Thread(target=BrowseLoop)
    t2.start()

    # Start main wx loop
    app.MainLoop()
    
    os._exit(0)


if __name__ == "__main__":
    main()
