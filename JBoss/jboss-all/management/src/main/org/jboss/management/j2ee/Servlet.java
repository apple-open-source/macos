/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee;

import java.util.Hashtable;
import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import javax.management.MBeanServer;
import javax.management.j2ee.statistics.Stats;

import org.jboss.logging.Logger;
import org.jboss.management.j2ee.statistics.ServletStatsImpl;
import org.jboss.management.j2ee.statistics.TimeStatisticImpl;

/** The JBoss JSR-77.3.17 Servlet model implementation
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.3.2.3 $
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.J2EEManagedObjectMBean, org.jboss.management.j2ee.statistics.StatisticsProvider"
 */
public class Servlet
      extends J2EEManagedObject
      implements ServletMBean
{
   // Constants -----------------------------------------------------
   public static final String J2EE_TYPE = "Servlet";

   // Attributes ----------------------------------------------------
   private static Logger log = Logger.getLogger(Servlet.class);

   private ObjectName servletServiceName;
   private ServletStatsImpl stats;

   // Static --------------------------------------------------------
   /** Create a JSR77 Servlet submodel.
    * @param mbeanServer the MBeanServer to use for mbean creation
    * @param webModuleName the name of the JSR77 web module mbean
    * @param webContainerName the name of the JBoss web container mbean
    * @param servletName the name of the servlet
    * @return the ObjectName of the JSR77 Servlet mbean
    */
   public static ObjectName create(MBeanServer mbeanServer, ObjectName webModuleName,
      ObjectName webContainerName, ObjectName servletServiceName)
   {
      try
      {
         Servlet servlet = new Servlet(servletServiceName, webModuleName, webContainerName);
         ObjectName jsr77Name = servlet.getObjectName();
         mbeanServer.registerMBean(servlet, jsr77Name);
         log.debug("Created JSR-77 Servlet: " + jsr77Name);
         return jsr77Name;
      }
      catch (Exception e)
      {
         log.debug("Could not create JSR-77 Servlet: " + servletServiceName, e);
         return null;
      }
   }

   public static void destroy(MBeanServer mbeanServer, ObjectName jsr77Name)
   {
      try
      {
         mbeanServer.unregisterMBean(jsr77Name);
         log.debug("Destroyed JSR-77 Servlet: " + jsr77Name);
      }
      catch (javax.management.InstanceNotFoundException ignore)
      {
      }
      catch (Exception e)
      {
         log.debug("Could not destroy JSR-77 Servlet: " + jsr77Name, e);
      }
   }

   // Constructors --------------------------------------------------

   /**
    * @param pName Name of the Servlet
    *
    * @throws InvalidParameterException If list of nodes or ports was null or empty
    **/
   public Servlet(ObjectName servletServiceName, ObjectName webModuleName,
      ObjectName webContainerName)
      throws MalformedObjectNameException,
         InvalidParentException
   {
      super(J2EE_TYPE, servletServiceName.getKeyProperty("name"), webModuleName);
      this.servletServiceName = servletServiceName;
      this.stats = new ServletStatsImpl();
   }

   /** StatisticsProvider access to stats.
    *
    * @jmx:managed-attribute
    * @return A ServletStats implementation
    */
   public Stats getStats()
   {
      try
      {
         TimeStatisticImpl serviceTime = (TimeStatisticImpl) stats.getServiceTime();
         Integer count = (Integer) server.getAttribute(servletServiceName, "RequestCount");
         Long totalTime = (Long) server.getAttribute(servletServiceName, "ProcessingTime");
         Long minTime = (Long) server.getAttribute(servletServiceName, "MinTime");
         Long maxTime = (Long) server.getAttribute(servletServiceName, "MaxTime");
         serviceTime.set(count.longValue(), minTime.longValue(),
            maxTime.longValue(), totalTime.longValue());
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
   // java.lang.Object overrides --------------------------------------

   public String toString()
   {
      return "Servlet { " + super.toString() + " } []";
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   /**
    * @return A hashtable with the Web-Module, J2EE-Application and J2EE-Server as parent
    **/
   protected Hashtable getParentKeys(ObjectName pParent)
   {
      Hashtable lReturn = new Hashtable();
      Hashtable lProperties = pParent.getKeyPropertyList();
      lReturn.put(WebModule.J2EE_TYPE, lProperties.get("name"));
      // J2EE-Application and J2EE-Server is already parent of J2EE-Application therefore lookup
      // the name by the J2EE-Server type
      lReturn.put(J2EEApplication.J2EE_TYPE, lProperties.get(J2EEApplication.J2EE_TYPE));
      lReturn.put(J2EEServer.J2EE_TYPE, lProperties.get(J2EEServer.J2EE_TYPE));

      return lReturn;
   }

}
