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

import org.jboss.logging.Logger;

/**
 * Root class of the JBoss JSR-77 implementation of
 * {@link javax.management.j2ee.JDBCDataSource JDBCDataSource}.
 *
 * @author  <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @version $Revision: 1.3.2.1 $
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.J2EEManagedObjectMBean"
 **/
public class JDBCDriver
      extends J2EEManagedObject
      implements JDBCDriverMBean
{
   // Constants -----------------------------------------------------
   private static Logger log = Logger.getLogger(JDBCDriver.class);

   public static final String J2EE_TYPE = "JDBCDriver";

   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   public static ObjectName create(MBeanServer pServer, String pName, ObjectName pService)
   {
      ObjectName lServer = null;
      try
      {
         lServer = (ObjectName) pServer.queryNames(
               new ObjectName(
                     J2EEDomain.getDomainName() + ":" +
               J2EEManagedObject.TYPE + "=" + J2EEServer.J2EE_TYPE + "," +
               "*"
               ),
               null
         ).iterator().next();
      }
      catch (Exception e)
      {
         log.error("Could not create JSR-77 JNDI: " + pName, e);
         return null;
      }
      try
      {
         // Now create the JNDI Representant
         return pServer.createMBean(
               "org.jboss.management.j2ee.JDBCDriver",
               null,
               new Object[]{
                  pName,
                  lServer
               },
               new String[]{
                  String.class.getName(),
                  ObjectName.class.getName()
               }
         ).getObjectName();
      }
      catch (Exception e)
      {
         log.error("Could not create JSR-77 JNDI: " + pName, e);
         return null;
      }
   }

   public static void destroy(MBeanServer pServer, String pName)
   {
      try
      {
         // Find the Object to be destroyed
         ObjectName lSearch = new ObjectName(
               J2EEDomain.getDomainName() + ":" +
               J2EEManagedObject.TYPE + "=" + JDBCDriver.J2EE_TYPE + "," +
               "name=" + pName + "," +
               "*"
         );
         ObjectName lJNDI = (ObjectName) pServer.queryNames(
               lSearch,
               null
         ).iterator().next();
         // Now remove the J2EEApplication
         pServer.unregisterMBean(lJNDI);
      }
      catch (Exception e)
      {
         log.error("Could not destroy JSR-77 JNDI: " + pName, e);
      }
   }

   // Constructors --------------------------------------------------

   /**
    * @param pName Name of the JDBCDataSource
    *
    * @throws InvalidParameterException If list of nodes or ports was null or empty
    **/
   public JDBCDriver(String pName, ObjectName pServer)
         throws
         MalformedObjectNameException,
         InvalidParentException
   {
      super(J2EE_TYPE, pName, pServer);
   }

   // Public --------------------------------------------------------

   // org.jboss.ServiceMBean overrides ------------------------------------

   // java.lang.Object overrides ------------------------------------

   public String toString()
   {
      return "JDBCDriver { " + super.toString() + " } [ " +
            " ]";
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------

}
