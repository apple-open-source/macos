/*
 * JBoss, the OpenSource EJB server
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.varia.scheduler.example;

import java.util.Date;

import javax.management.Notification;
import javax.management.ObjectName;

import org.jboss.system.ServiceMBeanSupport;

/**
 * 
 * A sample SchedulableMBean
 * 
 * @jmx:mbean extends="org.jboss.system.ServiceMBean"
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @author Cameron (camtabor)
 * @version $Revision: 1.2.4.2 $
 *  
 **/
public class SchedulableMBeanExample
   extends ServiceMBeanSupport
   implements SchedulableMBeanExampleMBean
{
   /**
    * Default (no-args) Constructor
    **/
   public SchedulableMBeanExample()
   {
   }
   
   // -------------------------------------------------------------------------
   // SchedulableExampleMBean Methods
   // -------------------------------------------------------------------------
   
   /**
    * @jmx:managed-operation
    */
   public void hit( Notification lNotification, Date lDate, long lRepetitions, ObjectName lName, String lTest ) {
      log.info( "got hit"
         + ", notification: " + lNotification
         + ", date: " + lDate
         + ", remaining repetitions: " + lRepetitions
         + ", scheduler name: " + lName
         + ", test string: " + lTest
      );
      hitCount++;
   }

  /**
   * @jmx:managed-operation
   */   
   public int getHitCount()
   {
     return hitCount;
   }
   
   private int hitCount = 0;
   
}
