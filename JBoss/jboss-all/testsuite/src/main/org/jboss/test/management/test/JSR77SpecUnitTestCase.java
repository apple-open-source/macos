/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.test.management.test;

import java.rmi.RemoteException;
import java.util.Set;
import java.util.Iterator;
import java.util.HashSet;
import java.lang.reflect.UndeclaredThrowableException;
import javax.management.JMException;
import javax.management.MBeanInfo;
import javax.management.MBeanAttributeInfo;
import javax.management.Notification;
import javax.management.NotificationListener;
import javax.management.ObjectName;
import javax.management.j2ee.ListenerRegistration;
import javax.management.j2ee.Management;
import javax.management.j2ee.ManagementHome;
import javax.management.j2ee.statistics.StatelessSessionBeanStats;
import javax.management.j2ee.statistics.Statistic;
import javax.rmi.PortableRemoteObject;

import org.jboss.management.j2ee.EJBModule;
import org.jboss.management.j2ee.J2EEDomain;
import org.jboss.management.j2ee.J2EEManagedObject;
import org.jboss.management.j2ee.JavaMailResource;
import org.jboss.management.j2ee.JCAConnectionFactory;
import org.jboss.management.j2ee.ResourceAdapterModule;
import org.jboss.management.j2ee.ServiceModule;
import org.jboss.management.j2ee.WebModule;
import org.jboss.management.j2ee.J2EEServer;
import org.jboss.management.j2ee.JNDIResource;
import org.jboss.management.j2ee.JCAResource;
import org.jboss.management.j2ee.JTAResource;
import org.jboss.management.j2ee.JMSResource;
import org.jboss.management.j2ee.J2EEApplication;
import org.jboss.test.JBossTestCase;
import junit.framework.Test;

/**
 * Test of JSR-77 specification conformance using the Management interface.
 * These test the basic JSR-77 handling and access.
 *
 * @author  <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.13.2.9 $
 */
public class JSR77SpecUnitTestCase
   extends JBossTestCase
{
   // Constants -----------------------------------------------------
   public static final String TEST_DATASOURCE = "DefaultDS";
   public static final String TEST_MAIL = "DefaultMail";

   // Constructors --------------------------------------------------

   public JSR77SpecUnitTestCase(String name)
   {
      super(name);
   }

   // Public --------------------------------------------------------

   /** Test that the JSR77 MEJB is available
    * @throws Exception
    */
   public void testConnect() throws Exception
   {
      log.debug("+++ testConnect");
      Management jsr77MEJB = getManagementEJB();
      String lDomain = jsr77MEJB.getDefaultDomain();
      log.debug("+++ testConnect, domain: " + lDomain);
      jsr77MEJB.remove();
   }

   /** Test the JSR-77 J2EEDomain availability
    * @throws Exception
    */
   public void testJ2EEDomain()
      throws
      Exception
   {
      getLog().debug("+++ testJ2EEDomain");
      Management jsr77MEJB = getManagementEJB();
      ObjectName domainName = new ObjectName(jsr77MEJB.getDefaultDomain()
         + ":" + J2EEManagedObject.TYPE + "=" + J2EEDomain.J2EE_TYPE + ","
         + "*");
      Set names = jsr77MEJB.queryNames(domainName, null);
      if (names.isEmpty())
      {
         fail("Could not find JSR-77 J2EEDomain '" + J2EEDomain.J2EE_TYPE + "'");
      }
      if (names.size() > 1)
      {
         fail("Found more than one JSR-77 J2EEDomain '" + J2EEDomain.J2EE_TYPE + "'");
      }
      ObjectName jsr77MEJBDomain = (ObjectName) names.iterator().next();
      getLog().debug("+++ testJ2EEDomain, root: " + jsr77MEJBDomain);
      jsr77MEJB.remove();
   }

   /** Test the JSR-77 J2EEServer availability
    * @throws Exception
    */
   public void testJ2EEServer() throws Exception
   {
      getLog().debug("+++ testJ2EEServer");
      Management jsr77MEJB = getManagementEJB();
      String domainName = jsr77MEJB.getDefaultDomain();
      ObjectName queryName = new ObjectName(domainName + ":" +
         J2EEManagedObject.TYPE + "=" + J2EEServer.J2EE_TYPE + "," + "*");

      Set names = jsr77MEJB.queryNames(queryName, null);
      if (names.isEmpty())
      {
         fail("Could not find JSR-77 J2EEServer '" + J2EEServer.J2EE_TYPE + "'");
      }
      Iterator iter = names.iterator();
      ObjectName serverName = null;
      while (iter.hasNext())
      {
         serverName = (ObjectName) iter.next();
         getLog().debug("J2EEServer: " + serverName);
      }

      // Get the server info
      String vendor = (String) jsr77MEJB.getAttribute(serverName, "ServerVendor");
      getLog().debug("ServerVendor: " + vendor);
      String version = (String) jsr77MEJB.getAttribute(serverName, "ServerVersion");
      getLog().debug("ServerVersion: " + version);

      // Get the list of JVMs
      ObjectName[] jvms = (ObjectName[]) jsr77MEJB.getAttribute(serverName, "JavaVMs");
      if (jvms == null || jvms.length == 0)
         fail("Failed to find any JavaVMs");
      getLog().debug("JavaVMs[0]: " + jvms[0]);
      String javaVendor = (String) jsr77MEJB.getAttribute(jvms[0], "JavaVendor");
      getLog().debug("JavaVendor: " + javaVendor);
      String javaVersion = (String) jsr77MEJB.getAttribute(jvms[0], "JavaVersion");
      getLog().debug("JavaVersion: " + javaVersion);
      String node = (String) jsr77MEJB.getAttribute(jvms[0], "Node");
      getLog().debug("Node: " + node);

      jsr77MEJB.remove();
   }

   /** Test the JSR-77 JNDIResource availability
    * @throws Exception
    */
   public void testJNDIResource() throws Exception
   {
      getLog().debug("+++ testJNDIResource");
      Management jsr77MEJB = getManagementEJB();
      String domainName = jsr77MEJB.getDefaultDomain();
      ObjectName queryName = new ObjectName(domainName + ":" +
         J2EEManagedObject.TYPE + "=" + JNDIResource.J2EE_TYPE + "," + "*");
      Set names = jsr77MEJB.queryNames(queryName, null);
      if (names.isEmpty())
      {
         fail("Could not find JSR-77 JNDIResource '" + JNDIResource.J2EE_TYPE + "'");
      }
      Iterator iter = names.iterator();
      while (iter.hasNext())
         getLog().debug("JNDIResource: " + iter.next());
      jsr77MEJB.remove();
   }

   /** Test JavaMailResource availability.
    * @throws Exception
    */
   public void testJavaMailResource() throws Exception
   {
      getLog().debug("+++ testJavaMailResource");
      Management jsr77MEJB = getManagementEJB();
      String domainName = jsr77MEJB.getDefaultDomain();
      ObjectName queryName = new ObjectName(domainName + ":" +
         J2EEManagedObject.TYPE + "=" + JavaMailResource.J2EE_TYPE + "," + "*");
      Set names = jsr77MEJB.queryNames(queryName, null);
      if (names.isEmpty())
      {
         fail("Could not find JSR-77 JavaMailResource '" + JavaMailResource.J2EE_TYPE + "'");
      }
      Iterator iter = names.iterator();
      while (iter.hasNext())
         getLog().debug("JavaMailResource: " + iter.next());
      jsr77MEJB.remove();
   }

   /** Test JCAResource availability.
    * @throws Exception
    */
   public void testJCAResource() throws Exception
   {
      getLog().debug("+++ testJCAResource");
      Management jsr77MEJB = getManagementEJB();
      String domainName = jsr77MEJB.getDefaultDomain();
      ObjectName queryName = new ObjectName(domainName + ":" +
         J2EEManagedObject.TYPE + "=" + JCAResource.J2EE_TYPE + "," + "*");
      Set names = jsr77MEJB.queryNames(queryName, null);
      if (names.isEmpty())
      {
         fail("Could not find JSR-77 JCAResource '" + JCAResource.J2EE_TYPE + "'");
      }
      Iterator iter = names.iterator();
      while (iter.hasNext())
         getLog().debug("JCAResource: " + iter.next());
      jsr77MEJB.remove();
   }

   /** Test JTAResource availability.
    * @throws Exception
    */
   public void testJTAResource() throws Exception
   {
      getLog().debug("+++ testJTAResource");
      Management jsr77MEJB = getManagementEJB();
      String domainName = jsr77MEJB.getDefaultDomain();
      ObjectName queryName = new ObjectName(domainName + ":" +
         J2EEManagedObject.TYPE + "=" + JTAResource.J2EE_TYPE + "," + "*");
      Set names = jsr77MEJB.queryNames(queryName, null);
      if (names.isEmpty())
      {
         fail("Could not find JSR-77 JTAResource '" + JTAResource.J2EE_TYPE + "'");
      }
      Iterator iter = names.iterator();
      while (iter.hasNext())
         getLog().debug("JTAResource: " + iter.next());
      jsr77MEJB.remove();
   }

   /** Test JMSResource availability.
    * @throws Exception
    */
   public void testJMSResource() throws Exception
   {
      getLog().debug("+++ testJMSResource");
      Management jsr77MEJB = getManagementEJB();
      String domainName = jsr77MEJB.getDefaultDomain();
      ObjectName queryName = new ObjectName(domainName + ":" +
         J2EEManagedObject.TYPE + "=" + JMSResource.J2EE_TYPE + "," + "*");
      Set names = jsr77MEJB.queryNames(queryName, null);
      if (names.isEmpty())
      {
         fail("Could not find JSR-77 JMSResource '" + JMSResource.J2EE_TYPE + "'");
      }
      Iterator iter = names.iterator();
      while (iter.hasNext())
         getLog().debug("JMSResource: " + iter.next());
      jsr77MEJB.remove();
   }

   /** Test the default JCAConnectionFactory availability.
    * @throws Exception
    */
   public void testJCAConnectionFactory()
      throws
      Exception
   {
      getLog().debug("+++ testJCAConnectionFactory");
      Management jsr77MEJB = getManagementEJB();
      Set names = jsr77MEJB.queryNames(
         getConnectionFactoryName(jsr77MEJB),
         null
      );
      if (names.isEmpty())
      {
         fail("Could not found JSR-77 JCAConnectionFactory named '"
            + TEST_DATASOURCE + "'");
      }
      if (names.size() > 1)
      {
         fail("Found more than one JSR-77 JCAConnectionFactory named '"
            + TEST_DATASOURCE + "'");
      }
      ObjectName factory = (ObjectName) names.iterator().next();
      getLog().debug("+++ testJCAConnectionFactory, " + TEST_DATASOURCE
         + ": " + factory);
      jsr77MEJB.remove();
   }

   /** Test EJBModule for the ejb-management.jar
    * @throws Exception
    */
   public void testEJBModule() throws Exception
   {
      getLog().debug("+++ testEJBModule");
      Management jsr77MEJB = getManagementEJB();
      String domainName = jsr77MEJB.getDefaultDomain();
      ObjectName mejbModuleName = new ObjectName(domainName + ":" +
         "J2EEServer=Local,J2EEApplication=null,"
         + J2EEManagedObject.TYPE + "=" + EJBModule.J2EE_TYPE
         + ",name=ejb-management.jar");
      boolean isRegistered = jsr77MEJB.isRegistered(mejbModuleName);
      assertTrue(mejbModuleName + " is registered", isRegistered);
      ObjectName[] ejbs = (ObjectName[]) jsr77MEJB.getAttribute(mejbModuleName, "Ejbs");
      assertTrue("ejb-management.jar.Ejbs.length > 0", ejbs.length > 0);
      for (int n = 0; n < ejbs.length; n++)
      {
         getLog().debug("Ejbs[" + n + "]=" + ejbs[n]);
         StatelessSessionBeanStats stats = (StatelessSessionBeanStats)
            jsr77MEJB.getAttribute(ejbs[n], "Stats");
         String[] statNames = stats.getStatisticNames();
         for (int s = 0; s < statNames.length; s++)
         {
            Statistic theStat = stats.getStatistic(statNames[s]);
            getLog().debug(theStat);
         }
      }
      jsr77MEJB.remove();
   }

   /** A test of accessing all StatelessSessionBean stats
    * @throws Exception
    */
   public void testEJBStats() throws Exception
   {
      getLog().debug("+++ testEJBStats");
      Management jsr77MEJB = getManagementEJB();
      String beanName = null;
      String query = "*:j2eeType=StatelessSessionBean,*";
      log.info(query);
      ObjectName ejbName = new ObjectName(query);
      Set managedObjects = jsr77MEJB.queryNames(ejbName,  null);
      log.info("Found " + managedObjects.size() + " objects");
      Iterator i = managedObjects.iterator();
      while (i.hasNext())
      {
         ObjectName oName = (ObjectName) i.next();
         beanName = oName.getKeyProperty("name");
         StatelessSessionBeanStats stats =
            (StatelessSessionBeanStats) jsr77MEJB.getAttribute(oName,
               "Stats");
         Statistic[] allStats = stats.getStatistics();
         for (int s = 0; s < allStats.length; s++)
         {
            Statistic theStat = allStats[s];
            getLog().debug(theStat);
         }
      }
      jsr77MEJB.remove();
   }

   /** Test WebModule for the jmx-console.war
    * @throws Exception
    */
   public void testWebModule() throws Exception
   {
      getLog().debug("+++ testWebModule");
      Management jsr77MEJB = getManagementEJB();
      String domainName = jsr77MEJB.getDefaultDomain();
      ObjectName webModuleName = new ObjectName(domainName + ":" +
         "J2EEServer=Local,J2EEApplication=null,"
         + J2EEManagedObject.TYPE + "=" + WebModule.J2EE_TYPE
         + ",name=jmx-console.war");
      boolean isRegistered = jsr77MEJB.isRegistered(webModuleName);
      assertTrue(webModuleName + " is registered", isRegistered);
      ObjectName[] servlets = (ObjectName[]) jsr77MEJB.getAttribute(webModuleName, "Servlets");
      assertTrue("jmx-console.war.Servlets.length > 0", servlets.length > 0);
      for (int n = 0; n < servlets.length; n++)
         getLog().debug("Servlets[" + n + "]=" + servlets[n]);
      jsr77MEJB.remove();
   }

   /** Test ResourceAdapterModule for the jboss-local-jdbc.rar
    * @throws Exception
    */
   public void testResourceAdapterModule() throws Exception
   {
      getLog().debug("+++ testResourceAdapterModule");
      Management jsr77MEJB = getManagementEJB();
      String domainName = jsr77MEJB.getDefaultDomain();
      ObjectName rarModuleName = new ObjectName(domainName + ":" +
         "J2EEServer=Local,J2EEApplication=null,"
         + J2EEManagedObject.TYPE + "=" + ResourceAdapterModule.J2EE_TYPE
         + ",name=jboss-local-jdbc.rar");
      boolean isRegistered = jsr77MEJB.isRegistered(rarModuleName);
      assertTrue(rarModuleName + " is registered", isRegistered);
      ObjectName[] ras = (ObjectName[]) jsr77MEJB.getAttribute(rarModuleName, "ResourceAdapters");
      assertTrue("jboss-local-jdbc.rar.ResourceAdapters.length > 0", ras.length > 0);
      for (int n = 0; n < ras.length; n++)
         getLog().debug("ResourceAdapters[" + n + "]=" + ras[n]);
      jsr77MEJB.remove();
   }

   /**
    * Test the notification delivery by restarting Default DataSource
    */
   public void testNotificationDeliver()
      throws
      Exception
   {
      try
      {
         getLog().debug("+++ testNotificationDeliver");
         Management jsr77MEJB = getManagementEJB();
         Set names = jsr77MEJB.queryNames(getMailName(jsr77MEJB), null);
         if (names.isEmpty())
         {
            fail("Could not found JSR-77 JavaMailResource'" + TEST_MAIL + "'");
         }
         ObjectName lMail = (ObjectName) names.iterator().next();
         Listener lLocalListener = new Listener();
         ListenerRegistration lListenerFactory = jsr77MEJB.getListenerRegistry();
         getLog().debug("+++ testNotificationDeliver, add Notification Listener to " + TEST_MAIL +
            " with Listener Registry: " + lListenerFactory);
         lListenerFactory.addNotificationListener(
            lMail,
            lLocalListener,
            null,
            null
         );
         getLog().debug("+++ testNotificationDeliver, stop " + TEST_MAIL + "");
         jsr77MEJB.invoke(lMail, "stop", new Object[]{}, new String[]{});
         getLog().debug("+++ testNotificationDeliver, start " + TEST_MAIL + "");
         jsr77MEJB.invoke(lMail, "start", new Object[]{}, new String[]{});
         // Wait 5 seconds to ensure that the notifications are delivered
         Thread.sleep(5000);
         if (lLocalListener.getNumberOfNotifications() < 2)
         {
            fail("Not enough notifications received: " + lLocalListener.getNumberOfNotifications());
         }
         getLog().debug("+++ testNotificationDeliver, remove Notification Listener from " + TEST_MAIL + "");
         lListenerFactory.removeNotificationListener(
            lMail,
            lLocalListener
         );
         jsr77MEJB.remove();
      }
      catch (Exception e)
      {
         log.debug("failed", e);
         throw e;
      }
   }

   /**
    * Test the Navigation through the current JSR-77 tree
    */
   public void testNavigation()
      throws
      Exception
   {
      Management jsr77MEJB = null;
      try
      {
         // Get Management EJB and then the management domain
         jsr77MEJB = getManagementEJB();
         ObjectName domainQuery = new ObjectName(jsr77MEJB.getDefaultDomain()
            + ":" + J2EEManagedObject.TYPE + "=" + J2EEDomain.J2EE_TYPE + ",*");
         Set names = jsr77MEJB.queryNames(domainQuery, null);

         if (names.isEmpty())
         {
            fail("Could not find any J2EEDomain");
         }
         if (names.size() > 1)
         {
            fail("Found more than one J2EEDomain");
         }

         ObjectName jsr77MEJBDomain = (ObjectName) names.iterator().next();
         // Report the attributes and references
         report(jsr77MEJB, jsr77MEJBDomain, new HashSet());
      }
      catch (Exception e)
      {
         log.debug("failed", e);
         throw e;
      }
      catch (Error err)
      {
         log.debug("failed", err);
         throw err;
      }
      finally
      {
         if (jsr77MEJB != null)
         {
            jsr77MEJB.remove();
         }
      }
   }

   private void report(Management jsr77EJB, ObjectName mbean, HashSet reportedNames)
      throws JMException,
      RemoteException
   {
      if (reportedNames.contains(mbean))
      {
         log.debug("Skipping already reported MBean: " + mbean);
         return;
      }

      log.debug("Report Object: " + mbean);
      reportedNames.add(mbean);
      MBeanInfo mbeanInfo = jsr77EJB.getMBeanInfo(mbean);
      MBeanAttributeInfo[] attrInfo = mbeanInfo.getAttributes();
      String[] attrNames = new String[attrInfo.length];
      // First just report all attribute names and types
      for (int i = 0; i < attrInfo.length; i++)
      {
         String name = attrInfo[i].getName();
         String type = attrInfo[i].getType();
         boolean readable = attrInfo[i].isReadable();
         log.debug("Attribute: " + name + ", " + type + ", readable: " + readable);
         attrNames[i] = attrInfo[i].getName();
      }

      // Now try to obtain the values
      for (int i = 0; i < attrNames.length; i++)
      {
         String name = attrNames[i];
         Object value = null;
         try
         {
            if (attrInfo[i].isReadable() == true)
               value = jsr77EJB.getAttribute(mbean, name);
         }
         catch (UndeclaredThrowableException e)
         {
            Throwable ex = e.getUndeclaredThrowable();
            log.debug("Failed to access attribute: " + name + ", " + ex.getMessage());
         }
         catch (Exception e)
         {
            // HACK: Ignore moved attribute error for message cache on the persistence manager
            if (name.equals("MessageCache"))
               continue;

            /* This is not a fatal exception as not all attributes are remotable
            but all javax.management.* and org.jboss.management.j2ee.* types
            should be.
            */
            log.debug("Failed to access attribute: " + name, e);
            String type = attrInfo[i].getType();
            boolean isJSR77Type = type.startsWith("javax.management") ||
               type.startsWith("org.jboss.management.j2ee");
            assertTrue("Bad attribute(" + name + ") is not a JSR77 type", isJSR77Type == false);
         }

         if (value == null)
         {
            log.debug("Attribute: " + name + " is empty");
         }
         else if (ObjectName.class.getName().equals(attrInfo[i].getType()))
         {
            // Check if this attribute is not support to be followed
            if (checkBlock(mbean, attrInfo[i].getName()))
            {
               log.debug("Blocked Attribute: " + name + " contains: " + value);
               continue;
            }
            // Report this Object's attribute first
            log.debug("Attribute: " + name + ", value: " + value + ", is reported");
            report(jsr77EJB, (ObjectName) value, reportedNames);
         }
         else if (ObjectName[].class.getName().equals(attrInfo[i].getType()))
         {
            ObjectName[] names = (ObjectName[]) value;
            for (int j = 0; j < names.length; j++)
            {
               report(jsr77EJB, names[j], reportedNames);
            }
         }
         else
         {
            log.debug("Attribute: " + name + " contains: " + value);
         }
      }
   }

   /**
    * @return True if the given attribute must be blocked to avoid
    *         an endless loop in the graph of JSR-77 object name
    *         references (like J2EEServer refences J2EEDeployedObjects
    *         and this references J2EEServer)
    */
   private boolean checkBlock(ObjectName pName, String pAttributeName)
   {
      String lType = (String) pName.getKeyPropertyList().get(J2EEManagedObject.TYPE);
      if (EJBModule.J2EE_TYPE.equals(lType) ||
         WebModule.J2EE_TYPE.equals(lType) ||
         ResourceAdapterModule.J2EE_TYPE.equals(lType) ||
         ServiceModule.J2EE_TYPE.equals(lType))
      {
         if ("Server".equals(pAttributeName))
         {
            // Block Attribute Server for any J2EE Deployed Objects
            return true;
         }
      }
      return "Parent".equals(pAttributeName) ||
         "ObjectName".equals(pAttributeName);
   }

   private Management getManagementEJB()
      throws
      Exception
   {
      getLog().debug("+++ getManagementEJB()");
      Object lObject = getInitialContext().lookup("ejb/mgmt/MEJB");
      ManagementHome home = (ManagementHome) PortableRemoteObject.narrow(
         lObject,
         ManagementHome.class
      );
      getLog().debug("Found JSR-77 Management EJB (MEJB)");
      return home.create();
   }

   private ObjectName getConnectionFactoryName(Management jsr77MEJB) throws Exception
   {
      String domainName = jsr77MEJB.getDefaultDomain();
      return new ObjectName(domainName + ":" +
         J2EEManagedObject.TYPE + "=" + JCAConnectionFactory.J2EE_TYPE + "," +
         "name=" + TEST_DATASOURCE + "," +
         "*"
      );
   }

   private ObjectName getMailName(Management jsr77MEJB) throws Exception
   {
      String domainName = jsr77MEJB.getDefaultDomain();
      return new ObjectName(domainName + ":" +
         J2EEManagedObject.TYPE + "=" + JavaMailResource.J2EE_TYPE + "," +
         "*"
      );
   }
   // Inner classes -------------------------------------------------

   private class Listener implements NotificationListener
   {

      private int mNrOfNotifications = 0;

      public int getNumberOfNotifications()
      {
         return mNrOfNotifications;
      }

      public void handleNotification(Notification pNotification, Object pHandbank)
      {
         mNrOfNotifications++;
      }
   }

   public static Test suite() throws Exception
   {
      return getDeploySetup(JSR77SpecUnitTestCase.class, "ejb-management.jar");
   }

}
