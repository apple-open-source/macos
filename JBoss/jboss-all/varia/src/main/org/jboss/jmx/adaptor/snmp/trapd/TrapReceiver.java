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
package org.jboss.jmx.adaptor.snmp.trapd;

import java.net.InetAddress;

import org.opennms.protocols.snmp.SnmpTrapHandler;
import org.opennms.protocols.snmp.SnmpTrapSession;
import org.opennms.protocols.snmp.SnmpOctetString;
import org.opennms.protocols.snmp.SnmpPduPacket;
import org.opennms.protocols.snmp.SnmpPduRequest;
import org.opennms.protocols.snmp.SnmpVarBind;
import org.opennms.protocols.snmp.SnmpPduTrap;

import org.jboss.logging.Logger;

/**
 * Implements an SNMP trap reception engine
 *
 * @version $Revision: 1.1.2.2 $
 *
 * @author  <a href="mailto:spol@intracom.gr">Spyros Pollatos</a>
 * @author  <a href="mailto:andd@intracom.gr">Dimitris Andreadis</a>
**/
public class TrapReceiver
    implements SnmpTrapHandler
{
    /** target logger */
    protected final Logger log;
    
   /**
    * Public CTOR
    *
    * @param log the logger used to output info messages
   **/
   public TrapReceiver(Logger log)
   {
      this.log = log; 
   }
    
   /**
    * Receives and logs information about SNMPv2 traps.
    *
    * @param session   the trap session that received the PDU
    * @param agent     the address of the remote sender
    * @param port      the remote port where the pdu was transmitted from
    * @param community the decoded community string
    * @param pdu       the decoded v2 trap pdu
   **/
   public void snmpReceivedTrap(SnmpTrapSession session, InetAddress agent, 
                                int port, SnmpOctetString community,
                                SnmpPduPacket pdu)
   {
      if (log.isDebugEnabled()) {
         StringBuffer sbuf = new StringBuffer();
        
         sbuf.append("V2 Trap from ").append(agent.toString());
         sbuf.append(" on port ").append(port);
         sbuf.append("\nPDU command......... ").append(pdu.getCommand());
         sbuf.append("\nPDU Length.......... ").append(pdu.getLength());
         sbuf.append("\nCommunity string.... ").append(community.toString());
		
         if(pdu instanceof SnmpPduRequest) {
            SnmpPduRequest spdu = (SnmpPduRequest) pdu;
         
            sbuf.append("\nPDU Error Status.... ").append(spdu.getErrorStatus());
            sbuf.append("\nPDU Error Index..... ").append(spdu.getErrorIndex());
            sbuf.append("\n");
         }
	
         for (int i = 0; i < pdu.getLength(); i++ ) {
            SnmpVarBind vb = pdu.getVarBindAt(i);
         
            sbuf.append("Varbind[").append(i).append("] := ");
            sbuf.append(vb.getName().toString()).append(" --> ");
            sbuf.append(vb.getValue().toString()).append("\n");		
         }
         log.debug(sbuf.toString());
      }
   } // snmpReceivedTrap

   /**
    * Receives and logs information about SNMPv1 traps.
    *
     * @param session   the trap session that received the PDU
    * @param agent     the address of the remote sender
    * @param port      the remote port where the pdu was transmitted from
    * @param community the decoded community string
    * @param pdu       the decoded v1 trap pdu
   **/
   public void snmpReceivedTrap(SnmpTrapSession session, InetAddress agent, 
                                int port, SnmpOctetString community,
                                SnmpPduTrap pdu)
   {
      if (log.isDebugEnabled()) {
         StringBuffer sbuf = new StringBuffer();
        
         sbuf.append("V1 Trap from agent ").append(agent.toString());
         sbuf.append(" on port ").append(port);
         sbuf.append("\nIP Address......... ").append(pdu.getAgentAddress());
         sbuf.append("\nEnterprise Id...... ").append(pdu.getEnterprise());
         sbuf.append("\nGeneric ........... ").append(pdu.getGeneric());
         sbuf.append("\nSpecific .......... ").append(pdu.getSpecific());
         sbuf.append("\nTimeStamp ......... ").append(pdu.getTimeStamp());
         sbuf.append("\nLength............. ").append(pdu.getLength());
         sbuf.append("\nCommunity string... ").append(community.toString());
         sbuf.append("\n");
	
         for (int i = 0; i < pdu.getLength(); i++ ) {
            SnmpVarBind vb = pdu.getVarBindAt(i);
         
            sbuf.append("Varbind[").append(i).append("] := ");
            sbuf.append(vb.getName().toString()).append(" --> ");
            sbuf.append(vb.getValue().toString()).append("\n");			
         }
         log.debug(sbuf.toString());
      }
   } // snmpReceivedTrap
	
   /**
    * Processes session errors.
    *
    * @param session the trap session in error
    * @param error   the error condition
    * @param ref     the reference object, if any
   **/
   public void snmpTrapSessionError(SnmpTrapSession session,
                                    int error, Object ref)
   {
      StringBuffer sbuf = new StringBuffer();
           
      if(ref != null) {
          sbuf.append("Session error (").append(error).append(") reference: ");
          sbuf.append(ref.toString());
      }
      else {
           sbuf.append("Session error (").append(error).append(")");
      }
	
      try {
         if(error == SnmpTrapSession.ERROR_EXCEPTION)
            session.raise();    
      }
      catch (Throwable e) {
          sbuf.append(" <").append(e).append(">"); 
      }
       
      log.error(sbuf.toString());
       
   } // snmpTrapSessionError
    
} // class TrapReceiver
