/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.statistics;

import java.io.Serializable;
import javax.management.j2ee.statistics.Statistic;

/**
 * JBoss Implementation of the base Model for a Statistic Information
 *
 * @author Marc Fleury
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.2 $
 */
public abstract class StatisticImpl
      implements Statistic, Serializable
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------
   protected String name;
   protected String units;
   protected String description;
   protected long startTime;
   protected long lastSampleTime;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /** Create a named Statistic.
    *
    * @param name Name of the statistic
    * @param units Unit description used in this statistic
    * @param description Human description of the statistic
    */
   public StatisticImpl(String name, String units, String description)
   {
      this.name = name;
      this.units = units;
      this.description = description;
      this.startTime = System.currentTimeMillis();
   }

   // Public --------------------------------------------------------

   // javax.management.j2ee.Statistics implementation ---------------

   public String getName()
   {
      return name;
   }

   public String getUnit()
   {
      return units;
   }

   public String getDescription()
   {
      return description;
   }

   public long getStartTime()
   {
      return startTime;
   }

   public long getLastSampleTime()
   {
      return lastSampleTime;
   }

   /** Reset the lastSampleTime and startTime to the current time
    */
   public void reset()
   {
      startTime = System.currentTimeMillis();
      lastSampleTime = startTime;
   }

   /** Update the lastSampleTime and startTime on first call
    */
   public void set()
   {
      lastSampleTime = System.currentTimeMillis();
   }

   public String toString()
   {
      StringBuffer tmp = new StringBuffer(name);
      tmp.append('(');
      tmp.append("description: ");
      tmp.append(description);
      tmp.append(", units: ");
      tmp.append(units);
      tmp.append(", startTime: ");
      tmp.append(startTime);
      tmp.append(", lastSampleTime: ");
      tmp.append(lastSampleTime);
      tmp.append(')');
      return tmp.toString();
   }
}
