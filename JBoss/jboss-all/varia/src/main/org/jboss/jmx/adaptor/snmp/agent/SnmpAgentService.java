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

import javax.management.ObjectName;
import javax.management.Notification;
import javax.management.NotificationListener;

import org.jboss.system.ServiceMBeanSupport;
import org.jboss.logging.Logger;

/**
 * <tt>SnmpAgentService</tt> is an MBean class implementing an SNMP agent.
 *
 * Currently, it allows to send V1 or V2 traps to one or more SNMP
 * managers defined by their IP address, listening port number and expected 
 * SNMP version. It will hopefully grow into a full blown SNMP agent
 * with get/set functionality.
 *
 * @jmx:mbean
 *    extends="org.jboss.system.ServiceMBean"
 *
 * @version $Revision: 1.1.2.1 $
 *
 * @author  <a href="mailto:spol@intracom.gr">Spyros Pollatos</a>
 * @author  <a href="mailto:andd@intracom.gr">Dimitris Andreadis</a>
**/
public class SnmpAgentService  
   extends ServiceMBeanSupport
   implements NotificationListener, SnmpAgentServiceMBean
{
   /** Supported versions */
   public static final int SNMPV1 = 1;
   public static final int SNMPV2 = 2; 
    
   /** Time keeping*/
   private Clock clock = new Clock();
   
   /** Trap counter */
   private Counter trapCounter = new Counter(0);

   /** Name of the file containing SNMP manager specifications */
   private String managersResName = null;  

   /** Name of resource file containing the monitored objects specifications */
   private String monitoredObjectsResName = null;
   
   /** Name of resource file containing notification to trap mappings */
   private String notificationMapResName = null;   
   
   /** Name of the trap factory class to be utilised */
   private String trapFactoryClassName = null;

   /** Name of the utilised timer MBean */
   private ObjectName timerName = null;
     
   /** Heartbeat emission period (in seconds) and switch */
   private int heartBeatPeriod = 60;

   /** Reference to heartbeat emission controller */
   private Heartbeat heartbeat = null;

   /** Responsible for notification reception subscriptions*/
   private SubscriptionMgr subscriptionManager = null;
   
   /** The trap emitting subsystem*/
   private TrapEmitter trapEmitter = null;

   /**
    * Gets the heartbeat switch
    *
    * @jmx:managed-attribute
   **/ 
   public int getHeartBeatPeriod()
   {
      return this.heartBeatPeriod;
   }
   
   /**
    * Sets the heartbeat period (in seconds) switch
    *
    * @jmx:managed-attribute
   **/     
   public void setHeartBeatPeriod(int heartBeatPeriod)
   {
      this.heartBeatPeriod = heartBeatPeriod;
   }
   
   /**
    * Returns the difference, measured in milliseconds, between the 
    * instantiation time and midnight, January 1, 1970 UTC.
    *
    * @jmx:managed-attribute
   **/        
   public long getInstantiationTime()
   {
      return this.clock.instantiationTime();
   }
    
   /**
    * Returns the up-time
    *
    * @jmx:managed-attribute
   **/ 
   public long getUptime()
   {
      return this.clock.uptime();
   }

   /**
    * Returns the current trap counter reading
    *
    * @jmx:managed-attribute
   **/    
   public long getTrapCount()
   {
      return this.trapCounter.peek();
   }
     
   /**
    * Sets the name of the file containing SNMP manager specifications
    *
    * @jmx:managed-attribute
   **/ 
   public void setManagersResName(String managersResName)
   {
      this.managersResName = managersResName;
   }

   /**
    * Gets the name of the file containing SNMP manager specifications
    *
    * @jmx:managed-attribute
   **/    
   public String getManagersResName()
   {
      return this.managersResName;
   }

   /**
    * Sets the name of the file that configures which JMX objects to
    * monitor for events
    *
    * @jmx:managed-attribute
   **/        
   public void setMonitoredObjectsResName(String monitoredObjectsResName)
   {
      this.monitoredObjectsResName = monitoredObjectsResName;
   }
   
   /**
    * Sets the name of the file that configures which JMX objects to
    * monitor for events
    *
    * @jmx:managed-attribute
   **/            
   public String getMonitoredObjectsResName()
   {
      return this.monitoredObjectsResName;
   }

   /**
    * Sets the name of the file containing the notification/trap mappings
    *
    * @jmx:managed-attribute
   **/ 
   public void setNotificationMapResName(String notificationMapResName)
   {
      this.notificationMapResName = notificationMapResName;
   }    
    
   /**
    * Gets the name of the file containing the notification/trap mappings
    *
    * @jmx:managed-attribute
   **/ 
   public String getNotificationMapResName()
   {
      return this.notificationMapResName;
   }   

   /**
    * Sets the utilised trap factory name
    *
    * @jmx:managed-attribute
   **/
   public void setTrapFactoryClassName(String name)
   {
      this.trapFactoryClassName = name;        
   }
    
   /**
    * Gets the utilised trap factory name
    *
    * @jmx:managed-attribute
   **/
   public String getTrapFactoryClassName()
   {
      return this.trapFactoryClassName;
   }
    
   /**
    * Sets the utilised timer MBean name
    *
    * @jmx:managed-attribute
   **/          
   public void setTimerName(ObjectName timerName)
   {
      this.timerName = timerName;
   }
    
   /**
    * Gets the utilised timer MBean name
    *
    * @jmx:managed-attribute
   **/      
   public ObjectName getTimerName()
   {
      return this.timerName;
   }    

   /**
    * Perform service start-up
   **/
   protected void startService()
      throws Exception
   {
      log.info("Instantiating trap emitter ...");
      this.trapEmitter = new TrapEmitter(this.getTrapFactoryClassName(),
                                         this.trapCounter,
                                         this.clock,
                                         this.getManagersResName(),
                                         this.getNotificationMapResName());
    
      // Start trap emitter
      log.info("Starting trap emitter ...");        
      this.trapEmitter.start();

      // Instantiate the subscription manager that will take care of 
      // connections with event sources
      log.info("Instantiating subscription manager ...");
      this.subscriptionManager =
         new SubscriptionMgr(this.getServer(),
                             this.getServiceName(),
                             this.getMonitoredObjectsResName());

      // Set self as the notification collector
      log.info("Starting subscription manager ...");
      this.subscriptionManager.start();
        
      // Get the heartbeat going 
      this.heartbeat = new Heartbeat(this.getServer(),
                                     this.getTimerName(),
                                     this.getHeartBeatPeriod());
                                     
      log.info("Starting heartbeat controller ...");
      heartbeat.start();
        
      // Send the cold start
      this.sendNotification(new Notification(EventTypes.COLDSTART, this,
                                             getNextNotificationSequenceNumber()));
    
      log.info("Snmp Agent going active");
   }
    
   /**
    * Perform service shutdown
   **/
   protected void stopService()
      throws Exception
   {
      log.info("Stopping heartbeat controller ...");
      this.heartbeat.stop();
      this.heartbeat = null; // gc

      log.info("Stopping subscription manager ...");
      this.subscriptionManager.stop();
      this.subscriptionManager = null; // gc
        
      log.info("Stopping trap emitter ...");
      this.trapEmitter.stop();
   }    
    
   /**
    * All notifications are intercepted here and are routed for emission.
    * Implementation for NotificationListener interface
   **/
   public void handleNotification(Notification n, Object handback)
   {
      if (log.isDebugEnabled()) {
         log.debug("Received notification: <" + n + "> Payload " +
                   "TS: <" + n.getTimeStamp() + "> " +
                   "SN: <" + n.getSequenceNumber() + "> " +
                   "T:  <" + n.getType() + ">");
      }
      
      try {
         this.trapEmitter.send(n);           
      }
      catch (Exception e) {
         log.error("Sending trap", e);
      }    
   }
}
