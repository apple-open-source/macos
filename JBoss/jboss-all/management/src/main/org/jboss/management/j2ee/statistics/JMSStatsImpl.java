package org.jboss.management.j2ee.statistics;


import javax.management.j2ee.statistics.JMSConnectionStats;
import javax.management.j2ee.statistics.JMSStats;
import javax.management.j2ee.statistics.Statistic;

/**
 * Represents the statistics provided by a JMS resource.
 * This class is immutable to avoid changes by the client
 * which could have side effects on the server when done
 * locally.
 *
 * @author <a href="mailto:marc@jboss.org">Marc Fleury</a>
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.1 $
 **/
public final class JMSStatsImpl extends StatsBase
      implements JMSStats
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   private JMSConnectionStats[] mConnetions;

   // Constructors --------------------------------------------------

   public JMSStatsImpl(JMSConnectionStats[] pConnetions)
   {
      if (pConnetions == null)
      {
         pConnetions = new JMSConnectionStats[0];
      }
      mConnetions = pConnetions;
   }

   // Public --------------------------------------------------------

   // javax.management.j2ee.JMSStats implementation -----------------

   public JMSConnectionStats[] getConnections()
   {
      return mConnetions;
   }

}
