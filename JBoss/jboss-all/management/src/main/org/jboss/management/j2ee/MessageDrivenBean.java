/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee;

import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import javax.management.j2ee.statistics.Stats;
import org.jboss.management.j2ee.statistics.CountStatisticImpl;
import org.jboss.management.j2ee.statistics.MessageDrivenBeanStatsImpl;
import org.jboss.logging.Logger;

/** The JBoss JSR-77.3.11 implementation of the MessageDrivenBean model
 *
 * @author  <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @version $Revision: 1.2.2.3 $
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.EJBMBean"
 **/
public class MessageDrivenBean
   extends EJB
   implements MessageDrivenBeanMBean
{
   // Constants -----------------------------------------------------
   public static final String J2EE_TYPE = "MessageDrivenBean";

   // Attributes ----------------------------------------------------
   private static Logger log = Logger.getLogger(MessageDrivenBean.class);

   private MessageDrivenBeanStatsImpl stats;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /** Create a MessageDrivenBean model
    *
    * @param name the ejb name, currently the JNDI name
    * @param ejbModuleName the JSR-77 EJBModule name for this bean
    * @param ejbContainerName the JMX name of the JBoss ejb container MBean
    * @throws MalformedObjectNameException
    * @throws InvalidParentException
    */
   public MessageDrivenBean( String name, ObjectName ejbModuleName,
      ObjectName ejbContainerName)
      throws MalformedObjectNameException,
         InvalidParentException
   {
      super( J2EE_TYPE, name, ejbModuleName, ejbContainerName );
      stats = new MessageDrivenBeanStatsImpl();
   }

   // Begin StatisticsProvider interface methods
   public Stats getStats()
   {
      try
      {
         updateCommonStats(stats);

         ObjectName containerName = getContainerName();
         CountStatisticImpl msgCount = (CountStatisticImpl) stats.getMessageCount();
         Long count = (Long) server.getAttribute(containerName, "MessageCount");
         msgCount.set(count.longValue());
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

   // Object overrides ---------------------------------------------------

   public String toString()
   {
      return "MessageDrivenBean[ " + super.toString() + " ]";
   }
}
