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
 * {@link javax.management.j2ee.RMI_IIOPResource RMI_IIOPResource}.
 *
 * @author  <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @version $Revision: 1.6.2.2 $
 *
 * AS Currently CorbaORBService does not support to be restarted therefore no manageability
 * @jmx:mbean extends="org.jboss.management.j2ee.EventProvider, org.jboss.management.j2ee.J2EEResourceMBean"
 */
public class RMI_IIOPResource
   extends J2EEResource
   implements RMI_IIOPResourceMBean
{
   // Constants -----------------------------------------------------

   public static final String J2EE_TYPE = "RMI_IIOPResource";

   // Attributes ----------------------------------------------------
   private static Logger log = Logger.getLogger(RMI_IIOPResource.class);

   private ObjectName mService;

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
         log.error("Could not create JSR-77 RMI_IIOPResource: " + pName, e);
         return null;
      }
      try
      {
         // Now create the RMI_IIOPResource Representant
         return pServer.createMBean(
            "org.jboss.management.j2ee.RMI_IIOPResource",
            null,
            new Object[]{
               pName,
               lServer,
               pService
            },
            new String[]{
               String.class.getName(),
               ObjectName.class.getName(),
               ObjectName.class.getName()
            }
         ).getObjectName();
      }
      catch (Exception e)
      {
         log.error("Could not create JSR-77 RMI_IIOPResource: " + pName, e);
         return null;
      }
   }

   public static void destroy(MBeanServer pServer, String pName)
   {
      try
      {
         J2EEManagedObject.removeObject(
            pServer,
            J2EEDomain.getDomainName() + ":" +
            J2EEManagedObject.TYPE + "=" + J2EEServer.J2EE_TYPE + "," +
            "name=" + pName + "," +
            "*"
         );
      }
      catch (Exception e)
      {
         log.error("Could not destroy JSR-77 RMI_IIOPResource: " + pName, e);
      }
   }

   // Constructors --------------------------------------------------

   /**
    * @param pName Name of the RMI_IIOPResource
    *
    * @throws InvalidParameterException If list of nodes or ports was null or empty
    **/
   public RMI_IIOPResource(String pName, ObjectName pServer, ObjectName pService)
      throws MalformedObjectNameException,
      InvalidParentException
   {
      super(J2EE_TYPE, pName, pServer);
      if (log.isDebugEnabled())
         log.debug("Service name: " + pService);
      mService = pService;
//      mState = new StateManagement( this );
   }

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

   public void postCreation()
   {
      sendNotification(StateManagement.CREATED_EVENT, "RMI_IIOP Resource created");
   }

   public void preDestruction()
   {
      sendNotification(StateManagement.DESTROYED_EVENT, "RMI_IIOP Resource deleted");
   }

   // java.lang.Object overrides ------------------------------------

   public String toString()
   {
      return "RMI_IIOPResource { " + super.toString() + " } [ " +
         " ]";
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------
}
