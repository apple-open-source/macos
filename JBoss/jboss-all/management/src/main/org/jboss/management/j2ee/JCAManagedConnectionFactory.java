/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee;

import java.util.Hashtable;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.jboss.logging.Logger;

/**
 * Root class of the JBoss JSR-77 implementation of
 * {@link javax.management.j2ee.JCAManagedConnectionFactory JCAManagedConnectionFactory}.
 *
 * @author  <a href="mailto:mclaugs@comcast.net">Scott McLaughlin</a>.
 * @version $Revision: 1.5.2.3 $
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.J2EEManagedObjectMBean"
 **/
public class JCAManagedConnectionFactory
      extends J2EEManagedObject
      implements JCAManagedConnectionFactoryMBean
{
   // Constants -----------------------------------------------------
   private static Logger log = Logger.getLogger(JCAManagedConnectionFactory.class);

   public static final String J2EE_TYPE = "JCAManagedConnectionFactory";

   // Attributes ----------------------------------------------------

   // Static --------------------------------------------------------

   public static ObjectName create(MBeanServer mbeanServer, String resName,
      ObjectName jsr77ParentName)
   {
      ObjectName jsr77Name = null;
      try
      {
         JCAManagedConnectionFactory mcf = new JCAManagedConnectionFactory(resName,
               jsr77ParentName);
         jsr77Name = mcf.getObjectName();
         mbeanServer.registerMBean(mcf, jsr77Name);
      }
      catch (Exception e)
      {
         log.debug("Could not create JSR-77 JCAManagedConnectionFactory: " + resName, e);
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
               J2EEManagedObject.TYPE + "=" + JCAManagedConnectionFactory.J2EE_TYPE + "," +
               "name=" + resName + "," +
               "*"
         );
      }
      catch (Exception e)
      {
         log.debug("Could not destroy JSR-77 JCAManagedConnectionFactory: " + resName, e);
      }
   }

   // Constructors --------------------------------------------------

   /**
    * @param pName Name of the JCAManagedConnectionFactory
    *
    * @throws InvalidParameterException If list of nodes or ports was null or empty
    **/
   public JCAManagedConnectionFactory(String resName, ObjectName jsr77ParentName)
      throws MalformedObjectNameException, InvalidParentException
   {
      super(J2EE_TYPE, resName, jsr77ParentName);
   }

   // Public --------------------------------------------------------

   // java.lang.Object overrides ------------------------------------

   public String toString()
   {
      return "JCAManagedConnectionFactory { " + super.toString() + " } [ " +
            " ]";
   }

   /**
    * @return A hashtable with the JCAResource and J2EEServer
    */
   protected Hashtable getParentKeys(ObjectName parentName)
   {
      Hashtable keys = new Hashtable();
      Hashtable nameProps = parentName.getKeyPropertyList();
      String serverName = (String) nameProps.get(J2EEServer.J2EE_TYPE);
      keys.put(J2EEServer.J2EE_TYPE, serverName);
      return keys;
   }

}
