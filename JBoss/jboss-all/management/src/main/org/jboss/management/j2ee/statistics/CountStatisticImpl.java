/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.management.j2ee.statistics;

import javax.management.j2ee.statistics.CountStatistic;

import org.jboss.management.j2ee.statistics.StatisticImpl;

/**
 * This class is the JBoss specific Counter Statistics class
 *
 * @author <a href="mailto:marc.fleury@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.com">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.1 $
 */
public class CountStatisticImpl
      extends StatisticImpl
      implements CountStatistic
{
   // -------------------------------------------------------------------------
   // Members
   // -------------------------------------------------------------------------
   protected long count;

   // -------------------------------------------------------------------------
   // Constructors
   // -------------------------------------------------------------------------

   /** Create a CountStatistic
    *
    * @param name the name of the state
    * @param units the units of the stat
    * @param description a description of the stat
    */
   public CountStatisticImpl(String name, String units, String description)
   {
      super(name, units, description);
   }

   // -------------------------------------------------------------------------
   // CountStatistic Implementation
   // -------------------------------------------------------------------------

   /**
    * @return The value of Count
    */
   public long getCount()
   {
      return count;
   }

   /**
    * @return Debug Information about this Instance
    */
   public String toString()
   {
      return "[ " + getCount() + ":" + super.toString() + " ]";
   }

   // -------------------------------------------------------------------------
   // Methods
   // -------------------------------------------------------------------------

   /**
    * Adds a hit to this counter
    */
   public void add()
   {
      set(++ count);
   }

   /**
    * Removes a hit to this counter
    */
   public void remove()
   {
      if( count > 0 )
      {
         set(-- count);
      }
   }

   /**
    * Resets the statistics to the initial values
    */
   public void reset()
   {
      count = 0;
      super.reset();
   }

   /** Set the current value of the count
    *
    * @param count the new count
    */
   public void set(long count)
   {
      this.count = count;
      super.set();
   }
}
