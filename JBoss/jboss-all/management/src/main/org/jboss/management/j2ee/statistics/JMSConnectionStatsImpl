/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee.statistics;

import javax.management.j2ee.statistics.JMSConnectionStats;
import javax.management.j2ee.statistics.JMSSessionStats;
import javax.management.j2ee.statistics.Statistic;

/**
 * Represents the statistics provided by a JMS Connection.
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>
 * @version $Revision: 1.1.2.1 $
 */
public final class JMSConnectionStatsImpl extends StatsBase
      implements JMSConnectionStats
{
   // Attributes ----------------------------------------------------

   private JMSSessionStats[] mSessions;
   private boolean mTransactional;

   // Constructors --------------------------------------------------

   public JMSConnectionStatsImpl(JMSSessionStats[] pSessions, boolean pIsTransactional)
   {
      if (pSessions == null)
      {
         pSessions = new JMSSessionStats[0];
      }
      mSessions = pSessions;
      mTransactional = pIsTransactional;
   }

   // Public --------------------------------------------------------

   // javax.management.j2ee.JMSConnectionStats implementation -------

   /**
    * @return The list of JMSSessionStats that provide statistics about the sessions
    *         associated with the referencing JMSConnectionStats.
    */
   public JMSSessionStats[] getSessions()
   {
      return mSessions;
   }

   /**
    * @return The transactional state of this JMS connection. If true, indicates that
    *         this JMS connection is transactional.
    */
   public boolean isTransactional()
   {
      return mTransactional;
   }
}
