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

import javax.management.Notification;

import org.opennms.protocols.snmp.SnmpPduPacket;
import org.opennms.protocols.snmp.SnmpPduTrap;

/**
 * <tt>TrapFactory </tt> takes care of translation of Notifications into
 * SNMP V1 and V2 traps
 *
 * Trap-PDU ::=
 * [4]
 * IMPLICIT SEQUENCE {
 *    enterprise          -- type of object generating
 *                        -- trap, see sysObjectID in [5]
 *    OBJECT IDENTIFIER,
 * agent-addr         -- address of object generating
 *    NetworkAddress, -- trap
 * generic-trap       -- generic trap type
 *    INTEGER {
 *        coldStart(0),
 *        warmStart(1),
 *        linkDown(2),
 *        linkUp(3),
 *        authenticationFailure(4),
 *        egpNeighborLoss(5),
 *        enterpriseSpecific(6)
 *    },
 * specific-trap   -- specific code, present even
 *    INTEGER,     -- if generic-trap is not
 *                 -- enterpriseSpecific
 * time-stamp      -- time elapsed between the last
 *    TimeTicks,   -- (re)initialization of the network
 *                 -- entity and the generation of the
 *                   trap
 * variable-bindings -- "interesting" information
 *    VarBindList
 * }
 * 
 * @version $Revision: 1.1.2.1 $
 *
 * @author  <a href="mailto:spol@intracom.gr">Spyros Pollatos</a>
 * @author  <a href="mailto:andd@intracom.gr">Dimitris Andreadis</a>
**/
public interface TrapFactory
{
   /**
    * Sets the name of the file containing the notification/trap mappings,
    * the uptime clock and the trap counter
   **/ 
   public void set(String notificationMapResName, Clock uptime, Counter count);
   
   /**
    * Performs all the required initialisation in order for the mapper to 
    * commence operation (e.g. reading of the resource file)
   **/    
   public void start()
      throws Exception;  
   
   /**
    * Translates a Notification to an SNMP V2 trap.
    *
    * @param the notification to be translated
   **/
   public SnmpPduPacket generateV2Trap(Notification n) 
      throws MappingFailedException;

   /**
    * Traslates a Notification to an SNMP V1 trap.
    *
    * @param the notification to be translated
   **/
   public SnmpPduTrap generateV1Trap(Notification n) 
      throws MappingFailedException;
          
} // TrapFactory
