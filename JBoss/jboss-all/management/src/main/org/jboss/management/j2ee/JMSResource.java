/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.management.j2ee;

import java.util.Map;
import java.util.Iterator;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.j2ee.statistics.Stats;

import org.jboss.logging.Logger;
import org.jboss.management.j2ee.statistics.JMSStatsImpl;

/**
 * Root class of the JBoss JSR-77 implementation of the JMSResource model
 *
 * @author  <a href="mailto:mclaugs@comcast.net">Scott McLaughlin</a>.
 * @version $Revision: 1.6.2.6 $
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.J2EEResourceMBean, org.jboss.management.j2ee.statistics.StatisticsProvider"
 */
public class JMSResource
      extends J2EEResource
      implements JMSResourceMBean
{
   // Constants -----------------------------------------------------
   private static Logger log = Logger.getLogger(JMSResource.class);

   public static final String J2EE_TYPE = "JMSResource";

   // Attributes ----------------------------------------------------

   private ObjectName jmsServiceName;
   private JMSStatsImpl stats;

   // Static --------------------------------------------------------

   public static ObjectName create(MBeanServer mbeanServer, String resName,
         ObjectName jmsServiceName)
   {
      ObjectName j2eeServerName = J2EEDomain.getDomainServerName(mbeanServer);
      ObjectName jsr77Name = null;
      try
      {
         JMSResource jmsRes = new JMSResource(resName, j2eeServerName, jmsServiceName);
         jsr77Name = jmsRes.getObjectName();
         mbeanServer.registerMBean(jmsRes, jsr77Name);
         log.debug("Created JSR-77 JMSResource: " + resName);
      }
      catch (Exception e)
      {
         log.debug("Could not create JSR-77 JMSResource: " + resName, e);
      }
      return jsr77Name;
   }

   public static void destroy(MBeanServer pServer, String pName)
   {
      try
      {
         J2EEManagedObject.removeObject(
               pServer,
               J2EEDomain.getDomainName() + ":" +
               J2EEManagedObject.TYPE + "=" + JMSResource.J2EE_TYPE + "," +
               "name=" + pName + "," +
               "*"
         );
      }
      catch (Exception e)
      {
         log.error("Could not destroy JSR-77 JMSResource Resource", e);
      }
   }

   // -------------------------------------------------------------------------
   // Constructors
   // -------------------------------------------------------------------------

   /**
    * @param pName Name of the JMSResource
    *
    * @throws InvalidParameterException If list of nodes or ports was null or empty
    **/
   public JMSResource(String resName, ObjectName j2eeServerName,
      ObjectName jmsServiceName)
      throws MalformedObjectNameException,
         InvalidParentException
   {
      super(J2EE_TYPE, resName, j2eeServerName);
      this.jmsServiceName = jmsServiceName;
      stats = new JMSStatsImpl(null);
   }

   // Begin StatisticsProvider interface methods

   /** Obtain the Stats from the StatisticsProvider.
    *
    * @jmx:managed-attribute
    * @return An JMSStats subclass
    */
   public Stats getStats()
   {
      try
      {
         // Obtain the current clients Map<ConnectionToken, ClientConsumer>
         Map clients = (Map) server.getAttribute(jmsServiceName, "Clients");
         Iterator iter = clients.keySet().iterator();
      }
      catch(Exception e)
      {
         log.debug("Failed to obtain stats", e);
      }
      return stats;
   }

   /** Reset all statistics in the StatisticsProvider
    * @jmx:managed-operation
    */
   public void resetStats()
   {
      stats.reset();
   }
   // End StatisticsProvider interface methods


   // java.lang.Object overrides ------------------------------------

   public String toString()
   {
      return "JMSResource { " + super.toString() + " } [ " +
            " ]";
   }
}
