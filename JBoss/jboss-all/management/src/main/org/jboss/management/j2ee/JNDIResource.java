/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.management.j2ee;

import javax.management.JMException;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.Notification;
import javax.management.ObjectName;

import org.jboss.logging.Logger;

/**
 * Root class of the JBoss JSR-77 implementation of
 * {@link javax.management.j2ee.JNDIResource JNDIResource}.
 *
 * @author  <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @version $Revision: 1.6.2.4 $
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.StateManageable, org.jboss.management.j2ee.J2EEResourceMBean"
 */
public class JNDIResource
      extends J2EEResource
      implements JNDIResourceMBean
{
   // Constants -----------------------------------------------------
   private static Logger log = Logger.getLogger(JNDIResource.class);

   public static final String J2EE_TYPE = "JNDIResource";

   // Attributes ----------------------------------------------------

   private StateManagement mState;
   private ObjectName jndiServiceName;

   // Static --------------------------------------------------------

   private static final String[] sTypes = new String[]{
      "j2ee.object.created",
      "j2ee.object.deleted",
      "state.stopped",
      "state.stopping",
      "state.starting",
      "state.running",
      "state.failed"
   };

   public static ObjectName create(MBeanServer mbeanServer, String resName,
      ObjectName jndiServiceName)
   {
      ObjectName j2eeServerName = J2EEDomain.getDomainServerName(mbeanServer);
      ObjectName jsr77Name = null;
      try
      {
         JNDIResource jndiRes = new JNDIResource(resName, j2eeServerName, jndiServiceName);
         jsr77Name = jndiRes.getObjectName();
         mbeanServer.registerMBean(jndiRes, jsr77Name);
         log.debug("Created JSR-77 JNDIResource: " + resName);
      }
      catch (Exception e)
      {
         log.debug("Could not create JSR-77 JNDIResource: " + resName, e);
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
               J2EEManagedObject.TYPE + "=" + JNDIResource.J2EE_TYPE + "," +
               "name=" + resName + "," +
               "*"
         );
      }
      catch (Exception e)
      {
         log.debug("Could not destroy JSR-77 JNDIResource: " + resName, e);
      }
   }

   // Constructors --------------------------------------------------

   /**
    * @param resName Name of the JNDIResource
    *
    * @throws InvalidParameterException If list of nodes or ports was null or empty
    */
   public JNDIResource(String resName, ObjectName mbeanServer, ObjectName jndiServiceName)
         throws
         MalformedObjectNameException,
         InvalidParentException
   {
      super(J2EE_TYPE, resName, mbeanServer);
      log.debug("Service name: " + jndiServiceName);
      this.jndiServiceName = jndiServiceName;
      mState = new StateManagement(this);
   }

   // Public --------------------------------------------------------

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

   // javax.management.j2ee.StateManageable implementation ----------

   public long getStartTime()
   {
      return mState.getStartTime();
   }

   public int getState()
   {
      return mState.getState();
   }

   public void mejbStart()
   {
      try
      {
         server.invoke(
               jndiServiceName,
               "start",
               new Object[]{},
               new String[]{}
         );
      }
      catch (Exception e)
      {
         log.debug("Start of JNDI Resource failed", e);
      }
   }

   public void mejbStartRecursive()
   {
      // No recursive start here
      mejbStart();
   }

   public void mejbStop()
   {
      try
      {
         server.invoke(
               jndiServiceName,
               "stop",
               new Object[]{},
               new String[]{}
         );
      }
      catch (Exception e)
      {
         log.debug("Stop of JNDI Resource failed", e);
      }
   }

   public void postCreation()
   {
      try
      {
         server.addNotificationListener(jndiServiceName, mState, null, null);
      }
      catch (JMException e)
      {
         log.debug("Failed to add notification listener", e);
      }
      sendNotification(StateManagement.CREATED_EVENT, "JNDI Resource created");
   }

   public void preDestruction()
   {
      sendNotification(StateManagement.DESTROYED_EVENT, "JNDI Resource destroyed");
      // Remove the listener of the target MBean
      try
      {
         server.removeNotificationListener(jndiServiceName, mState);
      }
      catch (JMException jme)
      {
         // When the service is not available anymore then just ignore the exception
      }
   }

   // java.lang.Object overrides ------------------------------------

   public String toString()
   {
      return "JNDIResource { " + super.toString() + " } [ " +
            " ]";
   }

}
