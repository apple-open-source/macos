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
package org.jboss.jmx.adaptor.snmp.test;

import javax.management.Notification;

import org.jboss.system.ServiceMBeanSupport;

/**
 * <tt>NotificationProducerService</tt> is a test class with an MBean interface
 * used to produce simple JMX notifications to be intercepted and mapped to SNMP
 * traps by the snmp JMX adaptor
 * 
 * @version $Revision: 1.1.2.1 $
 *
 * @author  <a href="mailto:spol@intracom.gr">Spyros Pollatos</a>
 * @author  <a href="mailto:andd@intracom.gr">Dimitris Andreadis</a>
 *
 * @jmx:mbean
 *    extends="org.jboss.system.ServiceMBean"
**/
public class NotificationProducerService  
   extends ServiceMBeanSupport
   implements NotificationProducerServiceMBean
{
   /**
    * Sends a test Notification of type "V1"
    *
    * @jmx:managed-operation
   **/    
   public void sendV1()
      throws Exception
   {
      sendNotification(
         new Notification("V1", this, getNextNotificationSequenceNumber(),
                          "V1 test notifications")); 
   }

   /**
    * Sends a test Notification of type "V2"
    *
    * @jmx:managed-operation
   **/          
   public void sendV2()
      throws Exception
   {
      sendNotification(
         new Notification("V2", this, getNextNotificationSequenceNumber(),
                          "V2 test notifications"));        
   }
   
} // class NotificationProducerService

