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

import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.Set;
import java.util.HashSet;
import java.util.Collections;
import java.util.Iterator;
import java.io.InputStreamReader;

import javax.management.Notification;

import org.opennms.protocols.snmp.SnmpIPAddress;
import org.opennms.protocols.snmp.SnmpObjectId;
import org.opennms.protocols.snmp.SnmpOctetString;
import org.opennms.protocols.snmp.SnmpPduRequest;
import org.opennms.protocols.snmp.SnmpPduPacket;
import org.opennms.protocols.snmp.SnmpPduTrap;
import org.opennms.protocols.snmp.SnmpTimeTicks;
import org.opennms.protocols.snmp.SnmpVarBind;
import org.opennms.protocols.snmp.SnmpCounter64;

import org.jboss.logging.Logger;

import org.jboss.jmx.adaptor.snmp.config.manager.Manager;
import org.jboss.jmx.adaptor.snmp.config.manager.ManagerList;

/**
 * <tt>TrapEmitter</tt> is a class that manages SNMP trap emission.
 *
 * Currently, it allows to send V1 or V2 traps to one or more subscribed SNMP
 * managers defined by their IP address, listening port number and expected
 * SNMP version.
 *
 * @version $Revision: 1.1.2.2 $
 *
 * @author  <a href="mailto:spol@intracom.gr">Spyros Pollatos</a>
 * @author  <a href="mailto:andd@intracom.gr">Dimitris Andreadis</a>
**/
public class TrapEmitter
{
   /** The logger object */
   private static final Logger log = Logger.getLogger(TrapEmitter.class);
   
   /** Reference to the utilised trap factory*/
   private TrapFactory trapFactory = null;
   
   /** The actual trap factory to instantiate */
   private String trapFactoryClassName = null;

   /** The managers resource name */
   private String managersResName = null;
   
   /** The notification map resource name */
   private String notificationMapResName = null;
   
   /** Provides trap count */
   private Counter trapCount = null;
   
   /** Uptime clock */
   private Clock uptime = null;
   
   /** Holds the manager subscriptions. Accessed through synch'd wrapper */
   private Set managers = Collections.synchronizedSet(new HashSet());  
    
   /**
    * Builds a TrapEmitter object for sending SNMP V1 or V2 traps. <P>
   **/
   public TrapEmitter(String trapFactoryClassName,
                      Counter trapCount,
                      Clock uptime,
                      String managersResName,
                      String notificationMapResName)
   {
      this.trapFactoryClassName = trapFactoryClassName;
      this.trapCount = trapCount;
      this.uptime = uptime;
      this.managersResName = managersResName;
      this.notificationMapResName = notificationMapResName;
   }
    
   /**
    * Complete emitter initialisation
   **/               
   public void start()
      throws Exception
   {
      // Load persisted manager subscriptions
      load();
      
      // Instantiate the trap factory
      this.trapFactory = (TrapFactory) Class.forName(this.trapFactoryClassName, true, this.getClass().getClassLoader()).newInstance();
      
      // Initialise
      this.trapFactory.set(this.notificationMapResName,
                           this.uptime,
                           this.trapCount);
      
      // Start the trap factory
      this.trapFactory.start();
   }
    
   /**
    * Perform shutdown
   **/
   public void stop()
      throws Exception
   {
      synchronized(this.managers) {

         // Recycle open sessions to managers
         Iterator i = this.managers.iterator();
         
         while (i.hasNext()) {
            ManagerRecord s = (ManagerRecord)i.next();
            s.closeSession();    
         }
            
         // Drop all held manager records
         this.managers.clear();
      }
   }
    
   /**
    * Intercepts the notification and after translating it to a trap sends it
    * along.
    *
    * @param n notification to be sent
    * @throws Exception if an error occurs during the preparation or
    * sending of the trap
   **/    
   public void send(Notification n)
      throws Exception
   {
      // Beeing paranoid
      synchronized(this.trapFactory) {
         if(this.trapFactory == null) {
            log.error("Received notifications before trap factory set. Discarding.");
            return;     
         }
      }
           
      // Cache the translated notification
      SnmpPduTrap v1TrapPdu = null; 
      SnmpPduPacket v2TrapPdu = null; 
       
      // Send trap. Synchronise on the subscription collection while 
      // iterating 
      synchronized(this.managers) {
            
         // Iterate over sessions and emit the trap on each one
         Iterator i = this.managers.iterator();
         while (i.hasNext()) {
            ManagerRecord s = (ManagerRecord)i.next();       

            try {
               switch (s.getVersion()) {
                  case SnmpAgentService.SNMPV1:
                     if (v1TrapPdu == null)
                        v1TrapPdu = this.trapFactory.generateV1Trap(n);
                            
                     // Advance the trap counter
                     this.trapCount.advance();
                            
                     // Send
                     s.getSession().send(v1TrapPdu);
                     break;
                  
                  case SnmpAgentService.SNMPV2:
                     if (v2TrapPdu == null)
                        v2TrapPdu = this.trapFactory.generateV2Trap(n);
                     
                     // Advance the trap counter
                     this.trapCount.advance();
                            
                     // Send
                     s.getSession().send(v2TrapPdu);
                     break;
                     
                  default:    
                     log.error("Skipping session: Unknown SNMP version found");    
               }            
            } 
            catch(MappingFailedException e) {
              log.error("Translating notification - " + e.getMessage());
            }    
            catch(Exception e) {
              log.error("SNMP send error for " + 
                        s.getAddress().toString() + ":" +
                        s.getPort() + ": <" + e +
                        ">");                    
            }
         }    
      }
   }

   /**
    * Load manager subscriptions
   **/ 
   private void load()
      throws Exception
   {
      log.info("Reading resource: \"" + this.managersResName + "\"");
        
      // Organise the handle to the subscription definition resource
      InputStreamReader in = null;
        
      try {
         in = new InputStreamReader(
            this.getClass().getResourceAsStream(this.managersResName));
      }
      catch (Exception e) {
         log.error("Accessing resource \"" + managersResName + "\"", e);
         throw e;
      }

      // Parse and read-in the resource file
      ManagerList managerList = null;
        
      try {
         managerList = ManagerList.unmarshal(in);
      }
      catch (Exception e) {
         log.error("Reading resource \"" + managersResName + "\"", e);
         throw e;
      }
        
      log.info("\"" + this.managersResName + "\" " + 
               (managerList.isValid() ? "valid" : "invalid") +
               ". Read " + managerList.getManagerCount() + 
               " monitoring managers");        
        
      log.info("Executing resource: \"" + this.managersResName + "\"");
        
      for (int i = 0; i < managerList.getManagerCount(); i++) {
         // Read the monitoring manager's particulars
         Manager m = managerList.getManager(i);

         try {
            // Create a record of the manager's interest 
            ManagerRecord r = new ManagerRecord(
                    InetAddress.getByName(m.getAddress()),
                    m.getPort(),
                    m.getLocalPort(),
                    m.getVersion()
                );
                
            // Add the record to the list of monitoring managers. If 
            // successfull open the session to the manager as well.
            if(this.managers.add(r) == false) {
               log.warn("Error executing monitoring manager directive #" + i + 
                        ": <Manager already added>");  
            }
            else {            
               // Open the session to the manager
               r.openSession();
            }                
         }
         catch (Exception e) {
            log.warn("Error compiling monitoring manager #" + i, e);                
         } 
      }
   }
} // class TrapEmitter
