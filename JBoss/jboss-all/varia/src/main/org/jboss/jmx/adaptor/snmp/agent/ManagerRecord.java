/*
 * Copyright (c) 2003,  Intracom S.A. - www.intracom.com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This package and its source code is available at www.jboss.org
**/
package org.jboss.jmx.adaptor.snmp.agent;

import java.io.Serializable;
import java.net.InetAddress;
import java.net.SocketException;

import org.opennms.protocols.snmp.SnmpHandler;
import org.opennms.protocols.snmp.SnmpSession;
import org.opennms.protocols.snmp.SnmpPeer;
import org.opennms.protocols.snmp.SnmpParameters;
import org.opennms.protocols.snmp.SnmpSMI;
import org.opennms.protocols.snmp.SnmpSyntax;
import org.opennms.protocols.snmp.SnmpPduPacket;

import org.jboss.logging.Logger;

/**
 * <tt>ManagerRecord</tt> is a class that is used as a key
 * to uniquely identify subscribing managers.
 *
 * @version $Revision: 1.1.2.2 $
 *
 * @author  <a href="mailto:spol@intracom.gr">Spyros Pollatos</a>
 * @author  <a href="mailto:andd@intracom.gr">Dimitris Andreadis</a>
**/
class ManagerRecord
   implements Serializable, SnmpHandler
{
   /** The logger object */
   private static final Logger log = Logger.getLogger(ManagerRecord.class);
   
   /** SNMP parameter for number of retries */
   private int retries = 10;
   
   /** SNMP parameter for timeout */
   private int timeout = 5000;
         
   /** Subscription target IP address */
   private InetAddress address;
   
   /** Subscription target port */
   private int port;
   
   /** The local port to use */
   private int localPort;
   
   /** Subscription native SNMP version */
   private int version;
   
   /** The read community string*/
   private final String readCommunity = "public";
   
   /** The session to the manager*/
   private transient SnmpSession session;

   /**
    * Creates key for the specified values. <P>
    *
    * @param address the manager's IP adddress
    * @param port the manager listening port
    * @param version the session's native SNMP version        
   **/ 
   public ManagerRecord(InetAddress address, int port, int localPort, int version)
      throws BadSnmpVersionException
   {
      this.address = address;
      this.port = port;
      this.localPort = localPort;
      
      switch (version) {
         case SnmpAgentService.SNMPV1:
         case SnmpAgentService.SNMPV2:
            this.version = version;
            break;
            
         default:    
            throw new BadSnmpVersionException("Bad SNMP Version: " + version);    
      }
   }
      
   /**
    *
   **/
   public InetAddress getAddress()
   {
      return this.address;
   }

   /**
    *
   **/        
   public int getPort()
   {
      return this.port;
   }

   /**
    *
   **/
   public int getLocalPort()
   {
      return this.localPort;
   }
   
   /**
    *
   **/        
   public int getVersion()
   {
      return this.version;
   }
        
   public void openSession()
      throws SocketException
   {
      // Create the SNMP session to the manager
      SnmpPeer peer = new SnmpPeer(this.address);
      peer.setPort(this.port);
      peer.setServerPort(this.localPort);
      peer.setRetries(this.retries);
      peer.setTimeout(this.timeout);
            
      SnmpParameters parameters = peer.getParameters();
            
      switch(this.version) {                
         case SnmpAgentService.SNMPV1:
            parameters.setVersion(SnmpSMI.SNMPV1);
            break;
         
         case SnmpAgentService.SNMPV2:
            parameters.setVersion(SnmpSMI.SNMPV2);
            break;
         default:
            parameters.setVersion(SnmpSMI.SNMPV1);
      }
            
      parameters.setReadCommunity(this.readCommunity);
      peer.setParameters(parameters);
            
      this.session = new SnmpSession(peer);
      this.session.setDefaultHandler(this);
   }
        
   /**
    * Close the session to the manager
   **/ 
   public void closeSession()
   {
      this.session.close();
   }
        
   /**
    * Returns the session to the manager
   **/ 
   public SnmpSession getSession()
   {
      return this.session;
   }        
                
   /**
    * Comparison operator. Keys are considered equal if all address, port
    * and version are identical
    *
    * @param o the key to be compared with
   **/              
   public boolean equals(Object o)
   {
      if (!(o instanceof ManagerRecord))
         return false;
            
      ManagerRecord other = (ManagerRecord)o;
            
      return (this.port == other.port &&
              this.address.equals(other.address) &&
              this.version == other.version);
   }

   /**
    * Hash generator
   **/                      
   public int hashCode()
   {
      return toString().hashCode();    
   }    
        
   /**
    *
   **/                              
   public String toString()
   {
      return new String(this.address + ":" + this.port +
                        " (" + this.version + ")" );    
   }
        
   /**
    * Stubs to be filled in if we are not only to send traps
   **/
   public void snmpInternalError(SnmpSession session, int err, SnmpSyntax pdu)
   {
      log.error("ManagerRecord::snmpInternalError, code: " + err);
   }
        
   public void snmpTimeoutError(SnmpSession session, SnmpSyntax pdu)
   {
      log.error("ManagerRecord::snmpTimeoutError");
   }
    
   public void snmpReceivedPdu(SnmpSession session, int cmd, SnmpPduPacket pdu)
   {
      log.error("ManagerRecord::snmpReceivedPdu");
   }
        
} // class ManagerRecord    
