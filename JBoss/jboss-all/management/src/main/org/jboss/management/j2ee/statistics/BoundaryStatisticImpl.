/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.management.j2ee.statistics;

import javax.management.j2ee.statistics.BoundaryStatistic;

import org.jboss.management.j2ee.statistics.StatisticImpl;

/**
 * This class is the JBoss specific Boundary Statistics class allowing
 * just to increase and resetStats the instance.
 *
 * @author <a href="mailto:mclaugs@comcast.net">Scott McLaughlin</a>
 * @version $Revision: 1.1.2.1 $
 */
public class BoundaryStatisticImpl
      extends StatisticImpl
      implements BoundaryStatistic
{
   // -------------------------------------------------------------------------
   // Members
   // -------------------------------------------------------------------------

   protected long mLowerBound;
   protected long mUpperBound;

   // -------------------------------------------------------------------------
   // Constructors
   // -------------------------------------------------------------------------

   /**
    * Default (no-args) constructor
    */
   public BoundaryStatisticImpl(String pName, String pUnit, String pDescription,
         long lowerBound, long upperBound)
   {
      super(pName, pUnit, pDescription);
      mLowerBound = lowerBound;
      mUpperBound = upperBound;
   }

   // -------------------------------------------------------------------------
   // BoundaryStatistic Implementation
   // -------------------------------------------------------------------------

   /**
    * @return The value of LowerBound
    */
   public long getLowerBound()
   {
      return mLowerBound;
   }

   /**
    * @return The value of UpperBound
    */
   public long getUpperBound()
   {
      return mUpperBound;
   }

   /**
    * @return Debug Information about this Instance
    */
   public String toString()
   {
      return "BoundryStatistics[ " + getLowerBound() + ", " +
            getUpperBound() + ", " + super.toString() + " ]";
   }

}
