/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.statistics;

import javax.management.j2ee.statistics.TimeStatistic;

/**
 * Time Statisitic Container for JBoss.
 *
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.com">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.4 $
 */
public class TimeStatisticImpl
      extends StatisticImpl
      implements TimeStatistic
{
   // -------------------------------------------------------------------------
   // Members
   // -------------------------------------------------------------------------

   protected long count;
   protected long minTime;
   protected long maxTime;
   protected long totalTime;
   protected double requestRate;

   private long start;

   // -------------------------------------------------------------------------
   // Constructors
   // -------------------------------------------------------------------------

   /** Create a TimeStatistic
    *
    * @param name the name of the state
    * @param units the units of the stat
    * @param description a description of the stat
    */
   public TimeStatisticImpl(String name, String units, String description)
   {
      super(name, units, description);
      start = System.currentTimeMillis();
   }

   // -------------------------------------------------------------------------
   // CountStatistic Implementation
   // -------------------------------------------------------------------------

   /**
    * @return The number of times a time measurements was added
    */
   public long getCount()
   {
      return count;
   }

   /**
    * @return The minimum time added since start of the measurements
    */
   public long getMinTime()
   {
      return minTime;
   }

   /**
    * @return The maximum time added since start of the measurements
    */
   public long getMaxTime()
   {
      return maxTime;
   }

   /**
    * @return The sum of all the time added to the measurements since
    *         it started
    */
   public long getTotalTime()
   {
      return totalTime;
   }

   /**
    * @return The request rate which is the number of counts divided by
    *         the time elapsed since the time measurements started
    */
   public double getRequestRate()
   {
      return requestRate;
   }

   /**
    * @return Debug Information about this instance
    */
   public String toString()
   {
      return "[ " +
            "Count: " + getCount() +
            ", Min. Time: " + getMinTime() +
            ", Max. Time: " + getMaxTime() +
            ", Total Time: " + getTotalTime() +
            ", Request Rate: " + getRequestRate() +
            ", " + super.toString() + " ]";
   }

   // -------------------------------------------------------------------------
   // Methods
   // -------------------------------------------------------------------------

   /**
    * Adds a Statistic Information about the elapsed time an action
    * observed by this instance took.
    *
    * @param pTime Time elapsed to added to a statistics
    */
   public void add(long pTime)
   {
      count++;
      if (pTime == 0)
      {
         minTime = 1;
      }
      if (minTime == 0)
      {
         minTime = pTime;
      }
      minTime = pTime < minTime ? pTime : minTime;
      maxTime = pTime > maxTime ? pTime : maxTime;
      totalTime += pTime;
      requestRate = (System.currentTimeMillis() - start) / count;
   }

   /**
    * Resets the statistics to the initial values
    */
   public void reset()
   {
      count = 0;
      minTime = 0;
      maxTime = 0;
      totalTime = 0;
      requestRate = 0;
      super.reset();
   }

   /** Set all TimeStatistic values.
    * @param count the invocation count
    * @param minTime the min time for an invocation
    * @param maxTime the max time for an invocation
    * @param totalTime the total time for all invocations
    */
   public void set(long count, long minTime, long maxTime, long totalTime)
   {
      // update lastSampleTime
      set(); 
      this.count = count;
      this.minTime = minTime;
      this.maxTime = maxTime;
      this.totalTime = totalTime;
      if(count==0)
         this.requestRate = Double.NaN;
      else
         this.requestRate = totalTime / count;
   }
}
