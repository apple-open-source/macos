/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.management.j2ee;

import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.j2ee.statistics.Stats;

import org.jboss.logging.Logger;
import org.jboss.management.j2ee.statistics.JTAStatsImpl;
import org.jboss.management.j2ee.statistics.CountStatisticImpl;

/** The JBoss JSR-77.3.30 implementation of the JTAResource model
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.5.2.6 $
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.J2EEResourceMBean,org.jboss.management.j2ee.statistics.StatisticsProvider"
 */
public class JTAResource
      extends J2EEResource
      implements JTAResourceMBean
{
   // Constants -----------------------------------------------------
   private static Logger log = Logger.getLogger(JTAResource.class);

   public static final String J2EE_TYPE = "JTAResource";

   // Attributes ----------------------------------------------------

   private ObjectName jtaServiceName;
   private JTAStatsImpl stats;

   // Static --------------------------------------------------------

   public static ObjectName create(MBeanServer mbeanServer, String resName,
         ObjectName jtaServiceName)
   {
      ObjectName j2eeServerName = J2EEDomain.getDomainServerName(mbeanServer);
      ObjectName jsr77Name = null;
      try
      {
         JTAResource jtaRes = new JTAResource(resName, j2eeServerName, jtaServiceName);
         jsr77Name = jtaRes.getObjectName();
         mbeanServer.registerMBean(jtaRes, jsr77Name);
         log.debug("Created JSR-77 JTAResource: " + resName);
      }
      catch (Exception e)
      {
         log.debug("Could not create JSR-77 JTAResource: " + resName, e);
      }
      return jsr77Name;
   }

   public static void destroy(MBeanServer mbeanServer, String resName)
   {
      try
      {
         J2EEManagedObject.removeObject(
               mbeanServer,
               J2EEDomain.getDomainName() + ":" +
               J2EEManagedObject.TYPE + "=" + JTAResource.J2EE_TYPE + "," +
               "name=" + resName + "," +
               "*"
         );
      }
      catch (Exception e)
      {
         log.error("Could not destroy JSR-77 JTAResource: " + resName, e);
      }
   }

   // Constructors --------------------------------------------------

   /**
    * @param resName Name of the JTAResource
    *
    * @throws InvalidParameterException If list of nodes or ports was null or empty
    */
   public JTAResource(String resName, ObjectName j2eeServerName, ObjectName jtaServiceName)
         throws
         MalformedObjectNameException,
         InvalidParentException
   {
      super(J2EE_TYPE, resName, j2eeServerName);
      if (log.isDebugEnabled())
         log.debug("Service name: " + jtaServiceName);
      this.jtaServiceName = jtaServiceName;
      stats = new JTAStatsImpl();
   }

   // Begin StatisticsProvider interface methods

   /** Obtain the Stats from the StatisticsProvider.
    *
    * @jmx:managed-attribute
    * @return An EJBStats subclass
    */
   public Stats getStats()
   {
      try
      {
         CountStatisticImpl readyCount = (CountStatisticImpl) stats.getActiveCount();
         Long count = (Long) server.getAttribute(jtaServiceName, "TransactionCount");
         readyCount.set(count.longValue());
         CountStatisticImpl commitCount = (CountStatisticImpl) stats.getCommitedCount();
         count = (Long) server.getAttribute(jtaServiceName, "CommitCount");
         commitCount.set(count.longValue());
         CountStatisticImpl rollbackCount = (CountStatisticImpl) stats.getRolledbackCount();
         count = (Long) server.getAttribute(jtaServiceName, "RollbackCount");
         rollbackCount.set(count.longValue());
      }
      catch(Exception e)
      {
         log.debug("Failed to retrieve stats", e);
      }
      return stats;
   }
   public void resetStats()
   {
      stats.reset();
   }

   // End StatisticsProvider interface methods

   // javax.managment.j2ee.EventProvider implementation -------------

   public String[] getEventTypes()
   {
      return StateManagement.stateTypes;
   }

   public String getEventType(int pIndex)
   {
      if (pIndex >= 0 && pIndex < StateManagement.stateTypes.length)
      {
         return StateManagement.stateTypes[pIndex];
      }
      else
      {
         return null;
      }
   }

   // java.lang.Object overrides ------------------------------------

   public String toString()
   {
      return "JTAResource { " + super.toString() + " } [ " +
            " ]";
   }

}
