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

import java.util.ArrayList;
import java.net.InetAddress;
import java.io.InputStreamReader;

import javax.management.Notification;

import gnu.regexp.RE;
import gnu.regexp.REException;

import org.opennms.protocols.snmp.SnmpPduPacket;
import org.opennms.protocols.snmp.SnmpPduTrap;
import org.opennms.protocols.snmp.SnmpPduRequest;
import org.opennms.protocols.snmp.SnmpVarBind;
import org.opennms.protocols.snmp.SnmpObjectId;
import org.opennms.protocols.snmp.SnmpOctetString;
import org.opennms.protocols.snmp.SnmpCounter64;
import org.opennms.protocols.snmp.SnmpIPAddress;

import org.jboss.jmx.adaptor.snmp.config.notification.Mapping;
import org.jboss.jmx.adaptor.snmp.config.notification.NotificationMapList; 
import org.jboss.jmx.adaptor.snmp.config.notification.VarBind;  
import org.jboss.jmx.adaptor.snmp.config.notification.VarBindList;  

import org.jboss.logging.Logger;

/**
 * <tt>TrapFactorySupport</tt> takes care of translation of Notifications
 * into SNMP V1 and V2 traps
 *
 * Data Structure Guide
 *
 * It looks complicated but it ain't. The mappings are read into a structure
 * that follows the outline defined in the Notification.xsd. Have a look
 * there and in the example notificationMap.xml and you should get the picture. 
 * As an optimization, 2 things are done:
 *
 * 1.   The "NotificationType" fields of all the mappings are 
 *      read, interpreted and compiled as regular expressions. All the 
 *      instances are placed in an array and made accessible in their compiled 
 *      form
 * 2.   The "wrapperClass" attribute is interpreted as a class name that 
 *      implements interface NotificationWrapper. An instance of each class is 
 *      created and similarly placed in an array 
 *
 * This results in 2 collections one of regular expressions and one of 
 * NotificationWrapper instances. The two collections have exactly the same
 * size as the collection of mappings. Obviously each read mapping has a "1-1"
 * correspondence with exactly 1 compiled regular expression and exactly 1
 * NotificationWrapper instance. The key for the correspondence is the index: 
 * regular expression i corresponds to mapping i that coresponds to 
 * NotificationWrapper instance i. The loading of the 2 collections is 
 * performed in method startService.
 * Checking for which mapping to apply (implemented in method findMapping) on a 
 * notification is simple: traverse the cached regular expressions and attempt 
 * to match the notification type against them. The FIRST match short circuits 
 * the search and the coresponding mapping index is returned.
 *
 * @version $Revision: 1.1.2.2 $
 *
 * @author  <a href="mailto:spol@intracom.gr">Spyros Pollatos</a>
 * @author  <a href="mailto:andd@intracom.gr">Dimitris Andreadis</a>
**/
public class TrapFactorySupport
   implements TrapFactory
{
   /** The logger object */
   private static final Logger log = Logger.getLogger(TrapFactorySupport.class);

   /** Reference to SNMP variable binding factory */
   private SnmpVarBindFactory snmpVBFactory = null;
   
   /** File that contains notification mappings */
   private String notificationMapResName = null;
   
   /** Uptime clock */
   private Clock clock = null;
   
   /** Trap counter */
   private Counter trapCount = null;
   
   /** Contains the read in mappings */
   private NotificationMapList notificationMapList = null;
    
   /** Contains the compiled regular expression type specifications */
   private ArrayList mappingRegExpCache = null;
   
   /** Contains instances of the notification wrappers */
   private ArrayList notificationWrapperCache = null;
   
   /** Cached local address */
   private byte[] localAddress = null;
    
   /**
    * Create TrapFactorySupport
   **/
   public TrapFactorySupport()
   {
      this.snmpVBFactory = new SnmpVarBindFactory();
   }

   /**
    * Sets the name of the file containing the notification/trap mappings,
    * the uptime clock and the trap counter
   **/ 
   public void set(String notificationMapResName, Clock clock, Counter count)
   {
      this.notificationMapResName = notificationMapResName;
      this.clock = clock;
      this.trapCount = count;
   }
   
   /**
    * Populates the regular expression and wrapper instance collections. Note 
    * that a failure (e.g. to compile a regular expression or to instantiate a 
    * wrapper) generates an error message. Furthermore, the offending 
    * expression or class are skipped and the corresponding collection entry 
    * is null. It is the user's responsibility to track the reported errors in 
    * the logs and act accordingly (i.e. correct them and restart). If not the 
    * corresponding mappings are effectively void and will NOT have effect. 
   **/    
   public void start()
      throws Exception
   {
      // cache local address
      this.localAddress = InetAddress.getLocalHost().getAddress();
      
      log.info("Reading resource: \"" + notificationMapResName + "\"");
        
      // Organise the handle to the subscription definition resource
      InputStreamReader in = null;
        
      try {
         in = new InputStreamReader(
                this.getClass().getResourceAsStream(notificationMapResName));
      }
      catch (Exception e) {
         log.error("Accessing resource \"" + notificationMapResName + "\"", e); 
         throw e;
      }
        
      // Read, parse and load the resource file
      try {
         this.notificationMapList = NotificationMapList.unmarshal(in);
      }
      catch (Exception e) {
         log.error("Reading resource \"" + notificationMapResName + "\"", e); 
         throw e;
      }

      log.info("\"" + this.notificationMapResName + "\" " +
               (this.notificationMapList.isValid() ? "valid" : "invalid") +
               ". Read " + this.notificationMapList.getMappingCount() + 
               " mappings");          
        
      log.info("Executing resource: \"" + notificationMapResName + "\"");

      // Initialise the cache with the compiled regular expressions denoting 
      // notification type specifications
      this.mappingRegExpCache = 
         new ArrayList(this.notificationMapList.getMappingCount());
        
      // Initialise the cache with the instantiated notification wrappers
      this.notificationWrapperCache =
         new ArrayList(this.notificationMapList.getMappingCount());
        
      for (int i = 0; i < this.notificationMapList.getMappingCount(); i++) {
            
         // Compile and add the regular expression
         String notificationType =
            this.notificationMapList.getMapping(i).getNotificationType();
         
         try {
            this.mappingRegExpCache.add(new RE(notificationType));   
         }
         catch (REException e) {
            // Fill the slot to keep index count correct
            this.mappingRegExpCache.add(null);
                
            log.warn("Error compiling notification map directive #" + i + 
                     " notification type\"" + notificationType + "\"", e); 
         }
            
         // Instantiate and add the wrapper
         // Read wrapper class name 
         String wrapperClassName =
            this.notificationMapList.getMapping(i).getVarBindList().getWrapperClass();
                
         log.debug("notification wrapper class: " + wrapperClassName);
         
         try {
            NotificationWrapper wrapper =
               (NotificationWrapper)Class.forName(wrapperClassName, true, this.getClass().getClassLoader()).newInstance();
                
            // Initialise it
            wrapper.set(this.clock, this.trapCount);
            
            // Add the wrapper to the cache
            this.notificationWrapperCache.add(wrapper);
         }
         catch (Exception e) {
            // Fill the slot to keep index count correct
            this.notificationWrapperCache.add(null);
                
            log.warn("Error compiling notification map directive #" + i, e);  
         }
      }
      log.info("Trap factory going active");                                                       
   }
    
   /**
    * Locate mapping applicable for the incoming notification. Key is the
    * notification's type
    *
    * @param n the notification to be examined
    * @return the index of the mapping
    * @throws IndexOutOfBoundsException if no mapping found
   **/ 
   private int findMappingIndex(Notification n)
      throws IndexOutOfBoundsException
   {
      // Sequentially check the notification type against the compiled 
      // regular expressions. On first match return the coresponding mapping
      // index
      for (int i = 0; i < this.notificationMapList.getMappingCount(); i++) {
         RE p = (RE) this.mappingRegExpCache.get(i);
            
         if (p != null) {
            if (p.isMatch(n.getType())) {
               if (log.isDebugEnabled())
                  log.debug("Match for \"" + n.getType() + "\" on mapping " + i);
                  return i;
            }
         }
      }
        
      // Signal "no mapping found"
      throw new IndexOutOfBoundsException();
   }
    
   /**
    * Traslates a Notification to an SNMP V1 trap.
   **/
   public SnmpPduTrap generateV1Trap(Notification n) 
      throws MappingFailedException
   {
      if (log.isDebugEnabled())
         log.debug("generateV1Trap");
        
      // Locate mapping for incomming event
      int index = -1;
        
      try {
         index = findMappingIndex(n);
      }
      catch (IndexOutOfBoundsException e) {
         throw new MappingFailedException("No mapping found for notification type: \"" + 
                    n.getType() + "\"");
      }
        
      Mapping m = this.notificationMapList.getMapping(index);
        
      // Create trap
      SnmpPduTrap trapPdu = new SnmpPduTrap();
        
      // Organise the 'standard' payload
      trapPdu.setAgentAddress(new SnmpIPAddress(this.localAddress));
      
      trapPdu.setTimeStamp(this.clock.uptime());
        
      // Organise the 'variable' payload 
      trapPdu.setGeneric(m.getGeneric());
      trapPdu.setSpecific(m.getSpecific());
      trapPdu.setEnterprise(m.getEnterprise());
        
      // Append the specified varbinds. Get varbinds from mapping and for
      // each one of the former use the wrapper to get the corresponding
      // values

      // Get the coresponding wrapper to get access to notification payload
      NotificationWrapper wrapper =
         (NotificationWrapper)this.notificationWrapperCache.get(index);
        
      if(wrapper != null) {
         // Prime the wrapper with the notification contents
         wrapper.prime(n);
            
         // Iterate through mapping specified varbinds and organise values
         // for each
         VarBindList vbList = m.getVarBindList();
         
         for (int i = 0; i < vbList.getVarBindCount(); i++) {
            VarBind vb = vbList.getVarBind(i);
                
            // Append the var bind. Interrogate read vb for OID and 
            // variable tag. The later is used as the key passed to the 
            // wrapper in order for it to locate the required value. That 
            // value and the aforementioned OID are used to generate the 
            // variable binding
            trapPdu.addVarBind(
               this.snmpVBFactory.make(vb.getOid(), wrapper.get(vb.getTag())));
         }
      }
      else {
         throw new MappingFailedException(
            "Varbind mapping failure: null wrapper defined for " +
            " notification type \"" + m.getNotificationType() + "\"" );
      }

      return trapPdu;        
   }
    
   /**
    * Traslates a Notification to an SNMP V2 trap.
    *
    * TODO: how do you get timestamp, generic, and specific stuff in the trap
   **/
   public SnmpPduPacket generateV2Trap(Notification n) 
      throws MappingFailedException
   {
      if (log.isDebugEnabled())
         log.debug("generateV2Trap");
        
      // Locate mapping for incomming event
      int index = -1;
        
      try {
         index = findMappingIndex(n);
      }
      catch (IndexOutOfBoundsException e) {
         throw new MappingFailedException(
            "No mapping found for notification type: \"" + n.getType() + "\"");
      }
        
      Mapping m = this.notificationMapList.getMapping(index);
        
      // Create trap
      SnmpPduRequest trapPdu = new SnmpPduRequest(SnmpPduPacket.V2TRAP);
        
      // Append the specified varbinds. Get varbinds from mapping and for
      // each one of the former use the wrapper to get data from the 
      // notification

      // Get the coresponding wrapper
      NotificationWrapper wrapper =
         (NotificationWrapper)this.notificationWrapperCache.get(index);
        
      if (wrapper != null) {
         // Prime the wrapper with the notification contents
         wrapper.prime(n);
            
         VarBindList vbList = m.getVarBindList();
         for (int i = 0; i < vbList.getVarBindCount(); i++) {
            VarBind vb=vbList.getVarBind(i);
                
            // Append the var bind. Interrogate read vb for OID and 
            // variable tag. The later is used as the key passed to the 
            // wrapper in order for it to locate the required value. That 
            // value and the aforementioned OID are used to generate the 
            // variable binding
            trapPdu.addVarBind(
               this.snmpVBFactory.make(vb.getOid(), wrapper.get(vb.getTag())));
         }
      }
      else {
         log.warn("Varbind mapping failure: null wrapper defined for " +
                  " notification type \"" + m.getNotificationType() + "\"" );
      }
         
      return trapPdu;
   }
} // class TrapFactorySupport
