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
import javax.management.j2ee.statistics.Stats;

import org.jboss.management.j2ee.statistics.JVMStatsImpl;

/** The JBoss JSR-77.3.4 JVM model implementation
 *
 * @author <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.3.2.1 $
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.J2EEManagedObjectMBean,org.jboss.management.j2ee.statistics.StatisticsProvider"
 */
public class JVM
      extends J2EEManagedObject
      implements JVMMBean
{
   // Constants -----------------------------------------------------

   public static final String J2EE_TYPE = "JVM";

   // Attributes ----------------------------------------------------

   private String javaVendor;
   private String javaVersion;
   private String node;
   private JVMStatsImpl stats;

   // Static --------------------------------------------------------

   // Constructors --------------------------------------------------

   /**
    * @param pName Name of the JVM
    *
    * @throws InvalidParameterException If list of nodes or ports was null or empty
    */
   public JVM(String name, ObjectName j2eeServer, String javaVersion,
      String javaVendor, String node)
      throws MalformedObjectNameException,
         InvalidParentException
   {
      super(J2EE_TYPE, name, j2eeServer);
      this.javaVendor = javaVendor;
      this.javaVersion = javaVersion;
      this.node = node;
      this.stats = new JVMStatsImpl();
   }

   // Public --------------------------------------------------------

   /**
    * @jmx:managed-attribute
    */
   public String getJavaVendor()
   {
      return javaVendor;
   }

   /**
    * @jmx:managed-attribute
    */
   public String getJavaVersion()
   {
      return javaVersion;
   }

   /**
    * @jmx:managed-attribute
    */
   public String getNode()
   {
      return node;
   }

   // Begin StatisticsProvider interface methods
   /** Obtain the Stats from the StatisticsProvider
    * @jmx:managed-attribute
    * @return
    */
   public Stats getStats()
   {
      // Refresh the stats
      stats.getUpTime();
      stats.getHeapSize();
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
      StringBuffer tmp = new StringBuffer("JVM");
      tmp.append('[');
      tmp.append("JavaVendor: ");
      tmp.append(javaVendor);
      tmp.append(", JavaVersion: ");
      tmp.append(javaVersion);
      tmp.append(", JavaVersion: ");
      tmp.append(javaVendor);
      tmp.append(", Stats: ");
      tmp.append(stats);
      tmp.append(']');
      return tmp.toString();
   }

   // Protected -----------------------------------------------------

   /**
    * @return A hashtable with the J2EE Server as parent
    */
   protected Hashtable getParentKeys(ObjectName pParent)
   {
      Hashtable lReturn = new Hashtable();
      Hashtable lProperties = pParent.getKeyPropertyList();
      lReturn.put(J2EEServer.J2EE_TYPE, lProperties.get("name"));

      return lReturn;
   }

}
