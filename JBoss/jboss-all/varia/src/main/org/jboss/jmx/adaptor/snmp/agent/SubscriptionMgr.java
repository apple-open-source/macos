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

import java.util.List;
import java.util.ArrayList;
import java.io.InputStreamReader;

import javax.management.MBeanServer;
import javax.management.ObjectName;

import gnu.regexp.REException;

import org.jboss.logging.Logger;

import org.jboss.jmx.adaptor.snmp.config.subscription.MonitoredObj;
import org.jboss.jmx.adaptor.snmp.config.subscription.MonitoredObjList; 

/**
 * <tt>SubscriptionMgr</tt> is responsible for subscribing a notification
 * listener to all objects defined in the coresponding resource file.
 *
 * @version $Revision: 1.1.2.2 $
 *
 * @author  <a href="mailto:spol@intracom.gr">Spyros Pollatos</a>
 * @author  <a href="mailto:andd@intracom.gr">Dimitris Andreadis</a>
**/
public class SubscriptionMgr
{
   /** Logger object */
   private static final Logger log = Logger.getLogger(SubscriptionMgr.class);
    
   /** List of names of objects that have been subscribed to. Filled in when 
    * the resource file is read. Used when service is stopped to unsubscribe 
    * from objects. Could potentially be ditched if the alternative of 
    * re-reading the resource file is acceptable 
   **/
   private List monitoredObjectsCache = null;
   
   /** Name of resource file containing the monitored objects specifications */
   private String monitoredObjectsResName = null;
   
   /** The JMX agent */
   private MBeanServer server = null;
   
   /** The JMX notification listener to receive notifications */
   private ObjectName listener = null;
   
   /**
    * CTOR
   **/     
   public SubscriptionMgr(MBeanServer server, ObjectName listener,
                          String monitoredObjectsResName)
   {
      this.monitoredObjectsResName = monitoredObjectsResName;
      this.server = server;
      this.listener = listener;
   }
    
   /**
    * Performs service start-up: subscribes the notification collector to all
    * specified monitored objects. Implicit assumptions regarding
    * entities from which notification subscriptions are requested:
    *
    *  1. They implement the NotificationBroadcaster interface
    *  2. They are MBeans
   **/
   public void start()
      throws Exception
   {            
      log.info("Reading resource: \"" + this.monitoredObjectsResName + "\"");
        
      // Organise the handle to the subscription definition resource
      InputStreamReader in = null;
        
      try {
         in = new InputStreamReader(
            this.getClass().getResourceAsStream(this.monitoredObjectsResName));
      }
      catch (Exception e) {
         log.error("Accessing resource \"" + this.monitoredObjectsResName + "\"", e);
         throw e;
      }
        
      // Parse and read-in the resource file
      MonitoredObjList monitoredObjList = null;
        
      try {
         monitoredObjList = MonitoredObjList.unmarshal(in);
      }
      catch (Exception e) {
         log.error("Parsing resource \"" + this.monitoredObjectsResName + "\"", e);
         throw e;
      }
        
      log.info("\"" + this.monitoredObjectsResName + "\" " + 
               (monitoredObjList.isValid() ? "valid" : "invalid") +
               ". Read " + monitoredObjList.getMonitoredObjCount() + 
               " monitored objects");
        
      log.info("Executing resource: \"" + this.monitoredObjectsResName + "\"");
      
      // Organise enough space to store the subscribed object names
      this.monitoredObjectsCache =
         new ArrayList(monitoredObjList.getMonitoredObjCount());
        
      // Iterate over the read monitoredObjects and act upon them 
      for (int i = 0; i < monitoredObjList.getMonitoredObjCount(); i++) {
         
         // Get monitored object
         MonitoredObj s = monitoredObjList.getMonitoredObj(i);
            
         // Get target object name
         String targetName = s.getObjectName();
         
         if (log.isDebugEnabled())
           log.debug("Object " + i + " target: \"" + targetName + "\"");
            
         // Get event types that are of interest and fill in the notification
         // filter
         RegExpNotificationFilterSupport filter =
            new RegExpNotificationFilterSupport();
          
         for (int j = 0; j < s.getNotificationTypeCount(); j++) {
            String eventType = s.getNotificationType(j);
                
            try {
               filter.enableType(eventType);
                
               if (log.isDebugEnabled())
                  log.debug("Event type " + (j+1) + ": \"" + eventType + "\"");                    
            }
            catch (REException e) {
               log.warn("Error compiling monitored object #" + i + 
                        ": Event type \"" + eventType + "\"", e);
            }    
         }
            
         try {
            // Subscribe the notification collector to the monitored object
            ObjectName targetObjectName = new ObjectName(targetName);
                
            server.addNotificationListener(
               targetObjectName, 
               listener, 
               filter, 
               listener.toString());
                    
            // Record the object that has been subscribed to 
            this.monitoredObjectsCache.add(targetObjectName);                    
         }    
         catch (Exception e) {
            log.warn("Error executing monitored object directive #" + i, e);
         }
      }
      log.info("Subscription manager done");
   }
    
   /**
    * Performs service shutdown: cancel all subscriptions
   **/
   public void stop()
      throws Exception
   {
      for(int i = 0; i <  this.monitoredObjectsCache.size(); i++) {
            
         ObjectName target = null;
            
         try {
            target = (ObjectName) this.monitoredObjectsCache.get(i);
                
            this.server.removeNotificationListener(target, listener);
            
            if (log.isDebugEnabled())
               log.debug("Unsubscribed from \"" + target + "\"");
         }
         catch(Exception e) {
            log.error("Unsubscribing from \"" + target + "\"", e);
         }
      }
   }
} // class SubscriptionMgr
