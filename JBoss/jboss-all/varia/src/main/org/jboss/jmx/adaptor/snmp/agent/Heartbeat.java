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

import java.util.Date;

import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.jboss.logging.Logger;

/**
 * <tt>Heartbeat</tt> auxiliary class implementing agent heartbeat 
 * schedulling and emission setup
 *
 * @version $Revision: 1.1.2.1 $
 *
 * @author  <a href="mailto:spol@intracom.gr">Spyros Pollatos</a>
 * @author  <a href="mailto:andd@intracom.gr">Dimitris Andreadis</a>
**/
public class Heartbeat
{
   /** The notification message field */    
   public static final String NOTIFICATION_MSG = "heartbeat report";
   
   /** Logger object */
   private static final Logger log = Logger.getLogger(Heartbeat.class);

   /** JMX Agent */
   private MBeanServer agent = null;

   /** Heart-beat interval in secs */
   private long interval = 0;
   
   /** Timer name */
   private ObjectName timer = null;
   
   /** The id of the scheduled event */
   private Integer heartbeatSchedule = null;

   /**
    * CTOR
   **/    
   public Heartbeat(MBeanServer agent, ObjectName timer, long interval)
   {
      this.agent = agent;
      this.timer = timer;
      this.interval = interval;
   }
    
   /**
    * Setup the production of heart-beat notifications
   **/
   public void start()
      throws Exception
   {
      // Get the heartbeat period in mSecs
      long period = interval * 1000;
   
      if (period <= 0) {
         log.warn("Heartbeat disabled");
         return;
      }
        
      // Skip if schedule is already set
      //
      if(heartbeatSchedule == null) {
         try {
            // Organise schedulled emission of heartbeat notification
            Object userData = null;                 // No user payload
            Date startTime = new Date();            // Start immediately
            Long nbOccurences = new Long(0);        // Go on forever 
                
            // If timer MBean not registered, exception will be thrown
            heartbeatSchedule = (Integer) agent.invoke(
               timer,
               "addNotification",
               new Object[] { 
                  EventTypes.HEARTBEAT, 
                  NOTIFICATION_MSG,
                  userData,
                  startTime,
                  new Long(period),
                  nbOccurences
               },
               new String[] {
                  "java.lang.String",
                  "java.lang.String",
                  Object.class.getName(),
                  Date.class.getName(),
                  Long.TYPE.getName(),
                  Long.TYPE.getName()
               });

            log.info("Heartbeat period set to " + period + " msecs");                
         }
         catch (Exception e) {
            log.error("while setting heartbeat notification", e);
            throw e;
         }
      }
   } // start()
    
   /**
    * Disable heartbeat
   **/
   public void stop()
      throws Exception
   {
      if(heartbeatSchedule != null) {
         try {
            // Have the schedule removed
            agent.invoke(
               timer, 
               "removeNotification", 
               new Object[] { heartbeatSchedule },
               new String[] { heartbeatSchedule.getClass().getName() }
            );
            heartbeatSchedule = null;
         }        
         catch (Exception e) {
            log.error("while unsetting heartbeat notification", e);
            throw e;
         }
      }
   } // stop
   
} // class Heartbeat
