##============================================================================
##
##     License:
##
##     This library is free software; you can redistribute it and/or
##     modify it under the terms of the GNU General Public
##     License as published by the Free Software Foundation; either
##     version 2 of the License, or (at your option) any later version.
##
##     This library is distributed in the hope that it will be useful,
##     but WITHOUT ANY WARRANTY; without even the implied warranty of
##     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
##     General Public License for more details.
##
##     You should have received a copy of the GNU General Public
##     License along with this library; if not, write to the Free Software
##     Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  021-1307
##     USA
##
##     Copyright (C) 2002-2004 Dave Smith (dizzyd@jabber.org)
##     Portions (c) Copyright 2005 Apple Computer, Inc.
##
## $Id: proxy65.py,v 1.6 2005/03/05 06:48:21 cjalbert Exp $
##============================================================================

from twisted.internet import protocol, reactor
from twisted.python import usage, log
from twisted.protocols.jabber import component
from twisted.application import app, service, internet
import sys, socket
import socks5

JEP65_GET      = "/iq[@type='get']/query[@xmlns='http://jabber.org/protocol/bytestreams']"
JEP65_ACTIVATE = "/iq[@type='set']/query[@xmlns='http://jabber.org/protocol/bytestreams']/activate"
DISCO_GET      = "/iq[@type='get']/query[@xmlns='http://jabber.org/protocol/disco#info']"


def hashSID(sid, initiator, target):
    import sha
    # logmsg = "sid=%s, initiator=%s, target=%s" % (sid, initiator, target,)
    # log.msg(logmsg.encode('ascii', 'backslashreplace'))
    source = "%s%s%s" % (sid, initiator, target)
    return sha.new(source.encode('utf-8')).hexdigest()
    #return sha.new("%s%s%s" % (sid, initiator, target)).hexdigest()

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
        # Initiator and target are UTF-8 strings, so unicode() should be used instead of str()
        # sid = hashSID(iq.query["sid"], iq["from"], str(iq.query.activate))
        sid = hashSID(iq.query["sid"], iq["from"], unicode(iq.query.activate))
        log.msg("Activation requested for: ", sid)

        if sid in self.pendingConns:
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
                c = e.addElement("condition")
                c["xmlns"] = "urn:ietf:params:xml:ns:xmpp-stanzas"
                c.addElement("not-allowed")
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
        else:
            # Send an error
            iq.swapAttributeValues("to", "from")
            iq["type"] = "error"
            iq.query.children = []
            e = iq.addElement("error")
            e["code"] = "404"
            e["type"] = "cancel"
            c = e.addElement("condition")
            c["xmlns"] = "urn:ietf:params:xml:ns:xmpp-stanzas"
            c.addElement("item-not-found")
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


class Options(usage.Options):
    optParameters = [('jid', None, 'proxy65'),
                     ('secret', None, None),
                     ('rhost', None, '127.0.0.1'),
                     ('rport', None, '6000'),
                     ('proxyips', None, None)]




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
    host_addresses = config["proxyips"].split(",")
    validAddresses = []
    ip = "localhost"
    port = "7777"
    for a in host_addresses:
        try:
            if ":" in a:
                ip, port = a.split(":")
            else:
                ip = a
            #socket.inet_pton(socket.AF_INET, ip)            
            validAddresses.append((ip, int(port)))
        except socket.error:
            print "Warning! Not using invalid proxy network address: ", a

    # No valid addresses, no proxy65
    if len(validAddresses) < 1:
        print "0 Proxy Network Addresses (--proxyip) found. Configuration aborted."
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
    addresses= []
    addresses.append(("0.0.0.0", int(port)))
    for (ip,port) in addresses:
        listener = internet.TCPServer(port, proxySvc, interface=ip)
        listener.setServiceParent(listeners)
    
    # Set the proxy services listeners variable with the
    # new multiservice
    proxySvc.listeners = listeners
    proxySvc.activeAddresses = validAddresses

    return c
    
    
