package org.jboss.management.j2ee.statistics;


import javax.management.j2ee.statistics.JCAConnectionStats;
import javax.management.j2ee.statistics.JCAConnectionPoolStats;
import javax.management.j2ee.statistics.JCAStats;

/** The JSR77.6.18 JCAStats implementation
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public final class JCAStatsImpl extends StatsBase
      implements JCAStats
{
   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------
   private JCAConnectionStats[] connectionStats;
   private JCAConnectionPoolStats[] poolStats;

   // Constructors --------------------------------------------------

   public JCAStatsImpl(JCAConnectionStats[] connectionStats,
      JCAConnectionPoolStats[] poolStats)
   {
      if( connectionStats == null )
         connectionStats = new JCAConnectionStats[0];
      this.connectionStats = connectionStats;
      if( poolStats == null )
         poolStats = new JCAConnectionPoolStats[0];
      this.poolStats = poolStats;

   }

   // Public --------------------------------------------------------

   // javax.management.j2ee.JCAStats implementation -----------------

   public JCAConnectionStats[] getConnections()
   {
      return connectionStats;
   }
   public JCAConnectionPoolStats[] getConnectionPools()
   {
      return poolStats;
   }

   public String toString()
   {
      StringBuffer tmp = new StringBuffer("JCAStats");
      tmp.append("[(JCAConnectionStats[]), (");
      for(int p = 0; p < poolStats.length; p ++)
      {
         tmp.append(poolStats[p]);
         if( p < poolStats.length - 1 )
            tmp.append(',');
      }
      tmp.append(")]");
      return tmp.toString();
   }
}
