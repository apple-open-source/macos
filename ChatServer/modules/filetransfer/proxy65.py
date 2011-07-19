"""
License

Copyright (C) 
2002-2004   Dave Smith (dizzyd@jabber.org)
2007-2008   Fabio Forno (xmpp:ff@jabber.bluendo.com)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

$Id: proxy65.py 34 2008-12-14 12:48:21Z fabio.forno@gmail.com $

-------------------------------------------------------------------------

Portions Copyright 2008 Apple, Inc.  All rights reserved.
"""
import os
import sys, socket
sys.path.append('/usr/share/proxy65');
from plistlib import readPlist, writePlist
from twisted.internet import protocol, reactor
from twisted.python import usage, log
from twisted.words.protocols.jabber import component,jid
from twisted.application import app, service, internet
try:
    from twisted.internet.endpoints import _parse; from twisted.application import strports; strports._parse = _parse
except ImportError:
    raise ImportError("Import failed for twisted 10.2 workaround")
import socks5

configPath = '/Library/Preferences/com.apple.Proxy65.plist';
config = {'jid':'',
         'secret':'',
         'rhost':'',
         'rport':'',
         'proxyips':''}

JEP65_GET      = "/iq[@type='get']/query[@xmlns='http://jabber.org/protocol/bytestreams']"
JEP65_ACTIVATE = "/iq[@type='set']/query[@xmlns='http://jabber.org/protocol/bytestreams']/activate"
DISCO_GET      = "/iq[@type='get']/query[@xmlns='http://jabber.org/protocol/disco#info']"


def hashSID(sid, initiator, target):
    import sha
    return sha.new("%s%s%s" % (sid, initiator, target)).hexdigest()

class JEP65Proxy(socks5.SOCKSv5):
    def __init__(self, service):
        socks5.SOCKSv5.__init__(self)
        self.service = service
        self.supportedAuthMechs = [socks5.AUTHMECH_ANON]
        self.supportedAddrs = [socks5.ADDR_DOMAINNAME]
        self.enabledCommands = [socks5.CMD_CONNECT]
        self.addr = ""

    def stopProducing(self):
        self.transport.loseConnection()

    def pauseProducing(self):
        self.transport.stopReading()

    def resumeProducing(self):
        self.transport.startReading()

    # ---------------------------------------------
    # SOCKSv5 subclass
    # ---------------------------------------------    
    def connectRequested(self, addr, port):
        # Check for special connect to the namespace -- this signifies that the client
        # is just checking to ensure it can connect to the streamhost
        if addr == "http://jabber.org/protocol/bytestreams":
            self.connectCompleted(addr, 0)
            self.transport.loseConnection()
            return
            
        # Save addr, for cleanup
        self.addr = addr
        
        # Check to see if the requested address is already
        # activated -- send an error if so
        if self.service.isActive(addr):
            self.sendErrorReply(socks5.REPLY_CONN_NOT_ALLOWED)
            return

        # Add this address to the pending connections
        if self.service.addConnection(addr, self):
            self.connectCompleted(addr, 0)
            self.transport.stopReading()
        else:
            self.sendErrorReply(socks5.REPLY_CONN_REFUSED)

    def connectionLost(self, reason):
        if self.state == socks5.STATE_CONNECT_PENDING:
            self.service.removePendingConnection(self.addr, self)
        else:
            self.transport.unregisterProducer()
            if self.peersock != None:
                self.peersock.peersock = None
                self.peersock.transport.unregisterProducer()
                self.peersock = None
                self.service.removeActiveConnection(self.addr)
        

class Service(component.Service, protocol.Factory):
    def __init__(self, config):
        self.jid = config["jid"]

        self.activeAddresses = []
        self.listeners = None
        self.pendingConns = {}
        self.activeConns = {}

    def buildProtocol(self, addr):
        return JEP65Proxy(self)

    def componentConnected(self, xmlstream):
        xmlstream.addObserver(JEP65_GET, self.onGetHostInfo)
        xmlstream.addObserver(DISCO_GET, self.onDisco)
        xmlstream.addObserver(JEP65_ACTIVATE, self.onActivateStream)
        
        # we're connected, start the SOCKSv5 listeners
        self.listeners.startService()
        

    def componentDisconnected(self):
        
        self.listeners.stopService()

    def onGetHostInfo(self, iq):
        iq.swapAttributeValues("to", "from")
        iq["type"] = "result"
        iq.query.children = []
        for (ip, port) in self.activeAddresses:
            s = iq.query.addElement("streamhost")
            s["jid"] = self.jid
            s["host"] = ip
            s["port"] = str(port)
        self.send(iq)

    def onDisco(self, iq):
        iq.swapAttributeValues("to", "from")
        iq["type"] = "result"
        iq.query.children = []
        i = iq.query.addElement("identity")
        i["category"] = "proxy"
        i["type"] = "bytestreams"
        i["name"] = "SOCKS5 Bytestreams Service"
        iq.query.addElement("feature")["var"] = "http://jabber.org/protocol/bytestreams"
        self.send(iq)


    def onActivateStream(self, iq):

        try:
            fromJID = jid.internJID(iq["from"]).full()
            activateJID = jid.internJID(unicode(iq.query.activate)).full()
            sid = hashSID(
                iq.query["sid"].encode("utf-8"), fromJID.encode("utf-8"), activateJID.encode("utf-8")
            )
            log.msg("Activation requested for: ", sid)

            # Get list of objects for this sid
            olist = self.pendingConns[sid]

            # Remove sid from pending
            del self.pendingConns[sid]

            # Ensure there are the correct # of participants
            if len(olist) != 2:
                log.msg("Activation for %s failed: insufficient participants", sid)
                # Send an error
                iq.swapAttributeValues("to", "from")
                iq["type"] = "error"
                iq.query.children = []
                e = iq.addElement("error")
                e["code"] = "405"
                e["type"] = "cancel"
                c = e.addElement("not-allowed")
                c["xmlns"] = "urn:ietf:params:xml:ns:xmpp-stanzas"
                self.send(iq)
                
                # Close all connected
                for c in olist:
                    c.transport.loseConnection()
                    
                return

            # Send iq result
            iq.swapAttributeValues("to", "from")
            iq["type"] = "result"
            iq.query.children = []
            self.send(iq)
            
            # Remove sid from pending and mark as active
            assert sid not in self.activeConns
            self.activeConns[sid] = None
        
            # Complete connection
            log.msg("Activating ", sid)
            olist[0].peersock = olist[1]
            olist[1].peersock = olist[0]
            olist[0].transport.registerProducer(olist[1], 0)
            olist[1].transport.registerProducer(olist[0], 0)
        except Exception, e:
            # TODO report the detailed exception
            # Send an error
            iq.swapAttributeValues("to", "from")
            iq["type"] = "error"
            iq.query.children = []
            e = iq.addElement("error")
            e["code"] = "404"
            e["type"] = "cancel"
            c = e.addElement("item-not-found")
            c["xmlns"] = "urn:ietf:params:xml:ns:xmpp-stanzas"
            self.send(iq)

    def isActive(self, address):
        return address in self.activeConns

    def addConnection(self, address, connection):
        log.msg("Adding connection: ", address, connection)
        olist = self.pendingConns.get(address, [])
        if len(olist) <= 1:
            olist.append(connection)
            self.pendingConns[address] = olist
            return True
        else:
            return False

    def removePendingConnection(self, address, connection):
        olist = self.pendingConns[address]
        if len(olist) == 1:
            del self.pendingConns[address]
        else:
            olist.remove(connection)
            self.pendingConns[address] = olist


    def removeActiveConnection(self, address):
        del self.activeConns[address]

def makeService(config):
    # Check for parameters...
    try:
        int(config["rport"], 10)
    except ValueError:
        print "Invalid router port (--rport) provided."
        sys.exit(-1)

    if config["secret"] == None:
        print "Component secret (--secret) is a REQUIRED parameter. Configuration aborted."
        sys.exit(-1)

    if config["proxyips"] == None:
        print "Proxy Network Addresses (--proxyips) is a REQUIRED parameter. Configuration aborted."
        sys.exit(-1)

    # Split and parse the addresses to ensure they are valid
    addresses = config["proxyips"].split(",")
    validAddresses = []
    for a in addresses:
        try:
            ip = None
            port = 7777
            if ":" in a:
                ip, port = a.split(":")
            else:
                ip = a
            socket.inet_pton(socket.AF_INET, ip)            
            validAddresses.append((ip, int(port)))
        except socket.error:
            print "Warning! Not using invalid proxy network address: ", a

    # No valid addresses, no proxy65
    if len(validAddresses) < 1:
        print "0 Proxy Network Addresses (--proxyips) found. Configuration aborted."
        sys.exit(-1)
    
    c = component.buildServiceManager(config["jid"], config["secret"],
                                      ("tcp:%s:%s" % (config["rhost"], config["rport"])))

    proxySvc = Service(config)
    proxySvc.setServiceParent(c)

    # Construct a multi service to hold all the listening
    # services -- the main proxy65.Service object will then
    # just use that to control when the system should be
    # listening
    listeners = service.MultiService()
    for (ip,port) in validAddresses:
        listener = internet.TCPServer(port, proxySvc, interface=ip)
        listener.setServiceParent(listeners)

    # Set the proxy services listeners variable with the
    # new multiservice
    proxySvc.listeners = listeners
    proxySvc.activeAddresses = validAddresses

    return c

# this is the core part of any tac file, the creation of the root-level
# application object
application = service.Application("Proxy65")

try:
    configFromFile = readPlist(configPath)
except:
    print("Failed to find configuration file: %s" % (configPath))
    sys.exit(1)

config.update(configFromFile)

# attach the service to its parent application
service = makeService(config)
service.setServiceParent(application)
