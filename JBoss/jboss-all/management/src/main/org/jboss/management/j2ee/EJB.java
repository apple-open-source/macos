/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee;

import java.util.Hashtable;
import java.util.Map;
import java.util.HashMap;
import java.util.Iterator;
import java.lang.reflect.Method;

import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.j2ee.statistics.Stats;
import javax.management.j2ee.statistics.TimeStatistic;

import org.jboss.invocation.InvocationStatistics;
import org.jboss.logging.Logger;
import org.jboss.management.j2ee.statistics.CountStatisticImpl;
import org.jboss.management.j2ee.statistics.TimeStatisticImpl;
import org.jboss.management.j2ee.statistics.EJBStatsImpl;

/** Root class of the JBoss JSR-77.3.10 EJB model
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.6.2.6 $
 * @jmx:mbean extends="org.jboss.management.j2ee.J2EEManagedObjectMBean, org.jboss.management.j2ee.statistics.StatisticsProvider"
 */
public abstract class EJB
      extends J2EEManagedObject
      implements EJBMBean
{
   // Constants -----------------------------------------------------
   public static final int ENTITY_BEAN = 0;
   public static final int STATEFUL_SESSION_BEAN = 1;
   public static final int STATELESS_SESSION_BEAN = 2;
   public static final int MESSAGE_DRIVEN_BEAN = 3;

   // Attributes ----------------------------------------------------
   private static Logger log = Logger.getLogger(EJB.class);

   /** The ObjectName of the ejb container MBean */
   protected ObjectName ejbContainerName;

   // Static --------------------------------------------------------

   /** Create a JSR77 EJB submodel.
    * @param mbeanServer the MBeanServer to use for mbean creation
    * @param ejbModuleName the name of the JSR77 EJBModule mbean
    * @param ejbContainerName the name of the JBoss ejb container mbean
    * @param ejbType an EJB.XXX_BEAN type constant value
    * @param jndiName the jndi name of the ejb home
    * @return the ObjectName of the JSR77 EJB mbean
    */
   public static ObjectName create(MBeanServer mbeanServer, ObjectName ejbModuleName,
      ObjectName ejbContainerName, int ejbType, String jndiName)
   {
      try
      {
         // Now create the EJB mbean
         EJB ejb = null;
         switch( ejbType )
         {
            case ENTITY_BEAN:
               ejb = new EntityBean(jndiName, ejbModuleName, ejbContainerName);
               break;
            case STATEFUL_SESSION_BEAN:
               ejb = new StatefulSessionBean(jndiName, ejbModuleName, ejbContainerName);
               break;
            case STATELESS_SESSION_BEAN:
               ejb = new StatelessSessionBean(jndiName, ejbModuleName, ejbContainerName);
               break;
            case MESSAGE_DRIVEN_BEAN:
               ejb = new MessageDrivenBean(jndiName, ejbModuleName, ejbContainerName);
               break;
         }

         ObjectName jsr77Name = ejb.getObjectName();
         mbeanServer.registerMBean(ejb, jsr77Name);
         log.debug("Created JSR-77 EJB: " + jsr77Name);
         return jsr77Name;
      }
      catch (Exception e)
      {
         log.debug("Could not create JSR-77 EJB: " + jndiName, e);
         return null;
      }
   }

   public static void destroy(MBeanServer mbeanServer, ObjectName jsr77Name)
   {
      try
      {
         // Now remove the EJB
         mbeanServer.unregisterMBean(jsr77Name);
         log.debug("Destroyed JSR-77 EJB: " + jsr77Name);
      }
      catch (javax.management.InstanceNotFoundException ignore)
      {
      }
      catch (Exception e)
      {
         log.debug("Could not destroy JSR-77 EJB: " + jsr77Name, e);
      }
   }

   // Constructors --------------------------------------------------

   /** Create a EJB model
    *
    * @param ejbType the EJB.EJB_TYPES string
    * @param ejbName the ejb name, currently the JNDI name
    * @param ejbModuleName the JSR-77 EJBModule name for this bean
    * @param ejbContainerName the JMX name of the JBoss ejb container MBean
    * @throws MalformedObjectNameException
    * @throws InvalidParentException
    */
   public EJB(String ejbType, String ejbName, ObjectName ejbModuleName,
      ObjectName ejbContainerName)
      throws MalformedObjectNameException,
         InvalidParentException
   {
      super(ejbType, ejbName, ejbModuleName);
      this.ejbContainerName = ejbContainerName;
   }

   // Begin StatisticsProvider interface methods

   /** Obtain the Stats from the StatisticsProvider.
    *
    * @jmx:managed-attribute
    * @return An EJBStats subclass
    */
   public abstract Stats getStats();

   /** Reset all statistics in the StatisticsProvider
    * @jmx:managed-operation
    */
   public abstract void resetStats();
   // End StatisticsProvider interface methods

   // java.lang.Object overrides --------------------------------------

   public String toString()
   {
      return "EJB { " + super.toString() + " } []";
   }
   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   /** Obtain the Stats from the StatisticsProvider. This method simply
    * updates the statistics common to all EJBs:
    * CreateCount
    * RemoveCount
    * InvocationTimes
    *
    * It should be invoked to update these common statistics.
    */
   protected void updateCommonStats(EJBStatsImpl stats)
   {
      try
      {
         ObjectName containerName = getContainerName();
         CountStatisticImpl createCount = (CountStatisticImpl) stats.getCreateCount();
         Long creates = (Long) server.getAttribute(containerName, "CreateCount");
         createCount.set(creates.longValue());
         CountStatisticImpl removeCount = (CountStatisticImpl) stats.getRemoveCount();
         Long removes = (Long) server.getAttribute(containerName, "RemoveCount");
         removeCount.set(removes.longValue());

         // Now build a TimeStatistics for every
         InvocationStatistics times = (InvocationStatistics) server.getAttribute(containerName, "InvokeStats");
         HashMap timesMap = new HashMap(times.getStats());
         Iterator iter = timesMap.entrySet().iterator();
         while( iter.hasNext() )
         {
            Map.Entry entry = (Map.Entry) iter.next();
            Method m = (Method) entry.getKey();
            InvocationStatistics.TimeStatistic stat = (InvocationStatistics.TimeStatistic) entry.getValue();
            TimeStatisticImpl tstat = new TimeStatisticImpl(m.getName(), TimeStatistic.MILLISECOND,
                  "The timing information for the given method");
            tstat.set(stat.count, stat.minTime, stat.maxTime, stat.totalTime);
            stats.addStatistic(m.getName(), tstat);
         }
      }
      catch(Exception e)
      {
         log.debug("Failed to retrieve stats", e);
      }
   }

   /**
    * @return the JMX name of the EJB container
    */
   protected ObjectName getContainerName()
   {
      return this.ejbContainerName;
   }
   /**
    * @return the JMX name of the EJB container cache
    */
   protected ObjectName getContainerCacheName()
   {
      ObjectName cacheName = null;
      try
      {
         Hashtable props = ejbContainerName.getKeyPropertyList();
         props.put("plugin", "cache");
         cacheName = new ObjectName(ejbContainerName.getDomain(), props);
      }
      catch (MalformedObjectNameException e)
      {
      }
      return cacheName;
   }
   /**
    * @return the JMX name of the EJB container pool
    */
   protected ObjectName getContainerPoolName()
   {
      ObjectName poolName = null;
      try
      {
         Hashtable props = ejbContainerName.getKeyPropertyList();
         props.put("plugin", "pool");
         poolName = new ObjectName(ejbContainerName.getDomain(), props);
      }
      catch (MalformedObjectNameException e)
      {
      }
      return poolName;
   }

   /**
    * @return A hashtable with the EJB-Module, J2EE-Application and J2EE-Server as parent
    */
   protected Hashtable getParentKeys(ObjectName pParent)
   {
      Hashtable lReturn = new Hashtable();
      Hashtable lProperties = pParent.getKeyPropertyList();
      lReturn.put(EJBModule.J2EE_TYPE, lProperties.get("name"));
      // J2EE-Application and J2EE-Server is already parent of J2EE-Application therefore lookup
      // the name by the J2EE-Server type
      lReturn.put(J2EEApplication.J2EE_TYPE, lProperties.get(J2EEApplication.J2EE_TYPE));
      lReturn.put(J2EEServer.J2EE_TYPE, lProperties.get(J2EEServer.J2EE_TYPE));
      return lReturn;
   }

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------

}
