/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/

package org.jboss.management.j2ee.statistics;

import javax.management.j2ee.statistics.RangeStatistic;

import org.jboss.management.j2ee.statistics.StatisticImpl;

/**
 * This class is the JBoss specific Range Statistics class allowing
 * just to increase and resetStats the instance.
 *
 * @author <a href="mailto:mclaugs@comcast.net">Scott McLaughlin</a>
 * @version $Revision: 1.1.2.2 $
 */
public class RangeStatisticImpl
      extends StatisticImpl
      implements RangeStatistic
{
   // -------------------------------------------------------------------------
   // Members
   // -------------------------------------------------------------------------

   protected long current;
   protected long highWaterMark;
   protected long lowWaterMark;

   // -------------------------------------------------------------------------
   // Constructors
   // -------------------------------------------------------------------------

   /**
    * Default (no-args) constructor
    **/
   public RangeStatisticImpl(String pName, String pUnit, String pDescription)
   {
      super(pName, pUnit, pDescription);
   }

   // -------------------------------------------------------------------------
   // RangeStatistic Implementation
   // -------------------------------------------------------------------------

   /**
    * @return The value of Current
    **/
   public long getCurrent()
   {
      return current;
   }

   /**
    * @return The value of HighWaterMark
    **/
   public long getHighWaterMark()
   {
      return highWaterMark;
   }

   /**
    * @return The value of LowWaterMark
    **/
   public long getLowWaterMark()
   {
      return lowWaterMark;
   }

   /**
    * @return Debug Information about this Instance
    **/
   public String toString()
   {
      StringBuffer tmp = new StringBuffer();
      tmp.append('[');
      tmp.append("low: ");
      tmp.append(lowWaterMark);
      tmp.append(", high: ");
      tmp.append(highWaterMark);
      tmp.append(", current: ");
      tmp.append(current);
      tmp.append(']');
      tmp.append(super.toString());
      return tmp.toString();
   }

   // -------------------------------------------------------------------------
   // Methods
   // -------------------------------------------------------------------------

   /**
    * Adds a hit to this counter
    **/
   public void add()
   {
      set(++ current);
   }

   /**
    * Removes a hit to this counter
    **/
   public void remove()
   {
      if( current > 0 )
      {
         set(-- current);
      }
   }

   /**
    * Resets the statistics to the initial values
    **/
   public void reset()
   {
      current = 0;
      highWaterMark = 0;
      lowWaterMark = 0;
      super.reset();
   }

   public void set(long current)
   {
      this.current = current;
      if (current < lowWaterMark)
      {
         lowWaterMark = current;
      }
      if (current > highWaterMark)
      {
         highWaterMark = current;
      }
      super.set();
   }
}
