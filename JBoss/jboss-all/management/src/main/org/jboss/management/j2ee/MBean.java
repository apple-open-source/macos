/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee;

import java.util.Hashtable;

import javax.management.JMException;
import javax.management.MalformedObjectNameException;
import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.jboss.logging.Logger;
import org.jboss.mx.util.MBeanProxy;
import org.jboss.system.ServiceMBean;

/** Root class of the JBoss implementation of a custom MBean managed object.
 *
 * @author  <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.8.2.5 $
 *
 * @jmx:mbean extends="org.jboss.management.j2ee.StateManageable,org.jboss.management.j2ee.J2EEManagedObjectMBean"
 */
public class MBean
      extends J2EEManagedObject
      implements MBeanMBean
{
   // Constants -----------------------------------------------------
   public static final String J2EE_TYPE = "MBean";

   // Attributes ----------------------------------------------------
   private static Logger log = Logger.getLogger(MBean.class);

   private ObjectName jbossServiceName;
   private StateManagement mState;
   private boolean monitorsStateChanges = false;

   // Static --------------------------------------------------------

   /** Create a
    * @param pServer
    * @param pServiceModule
    * @param pTarget
    * @return
    */
   public static ObjectName create(MBeanServer pServer, String pServiceModule,
      ObjectName pTarget)
   {
      String pName = pTarget.toString();
      ObjectName mbeanName = null;
      try
      {
         if (pServiceModule == null)
         {
            log.debug("Parent SAR Module not defined");
            return null;
         }

         MBean mbean = new MBean(pName, new ObjectName(pServiceModule), pTarget);
         mbeanName = mbean.getObjectName();
         pServer.registerMBean(mbean, mbeanName);
      }
      catch (Exception e)
      {
         log.debug("Could not create JSR-77 MBean: " + pName, e);
      }
      return mbeanName;
   }

   public static void destroy(MBeanServer pServer, String pName)
   {
      try
      {
         if (pName.indexOf(J2EEManagedObject.TYPE + "=" + MBean.J2EE_TYPE) >= 0)
         {
            J2EEManagedObject.removeObject(
                  pServer,
                  pName
            );
         }
         else
         {
            J2EEManagedObject.removeObject(
                  pServer,
                  pName,
                  J2EEDomain.getDomainName() + ":" +
                  J2EEManagedObject.TYPE + "=" + MBean.J2EE_TYPE +
                  "," + "*"
            );
         }
      }
      catch (Exception e)
      {
         log.error("Could not destroy JSR-77 MBean: " + pName, e);
      }
   }

   // Constructors --------------------------------------------------

   /**
    * @param pName Name of the MBean
    *
    * @throws InvalidParameterException If list of nodes or ports was null or empty
    **/
   public MBean(String pName, ObjectName pServiceModule, ObjectName pTarget)
         throws
         MalformedObjectNameException,
         InvalidParentException
   {
      super(J2EE_TYPE, pName, pServiceModule);
      mState = new StateManagement(this);
      jbossServiceName = pTarget;
   }

   /** Does the MBean monitor state changes of the JBoss MBean service.
    * @return True if the underlying JBoss MBean service is monitored for state
    * changes.
    *
    * @jmx:managed-attribute
    */
   public boolean isStateMonitored()
   {
      return monitorsStateChanges;
   }

   // J2EEManagedObjectMBean implementation -------------------------

   public void postCreation()
   {
      try
      {
         // First check if the service implements the NotificationBroadcaster
         monitorsStateChanges = getServer().isInstanceOf(jbossServiceName,
               "javax.management.NotificationBroadcaster"
         );
         if (monitorsStateChanges)
         {
            getServer().addNotificationListener(jbossServiceName, mState, null, null);
         }
      }
      catch (Exception jme)
      {
         log.debug("Failed to register as listener of: "+jbossServiceName, jme);
      }
      sendNotification(StateManagement.CREATED_EVENT, "MBean created");

      // Initialize the state
      try
      {
         ServiceMBean mbean = (ServiceMBean) MBeanProxy.get(ServiceMBean.class,
            this.jbossServiceName, getServer());
         int jbossState = mbean.getState();
         int jsr77State = mState.convertJBossState(jbossState);
         mState.setState(jsr77State);
      }
      catch(Exception e)
      {
         log.trace("Failed to initialze state from: "+jbossServiceName, e);
      }
   }

   public void preDestruction()
   {
      sendNotification(StateManagement.DESTROYED_EVENT, "MBean destroyed");
      // Remove the listener of the target MBean
      try
      {
         getServer().removeNotificationListener(jbossServiceName, mState);
      }
      catch (JMException jme)
      {
         // When the service is not available anymore then just ignore the exception
      }
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

   // javax.management.j2ee.StateManageable implementation ----------

   public long getStartTime()
   {
      return mState.getStartTime();
   }

   public int getState()
   {
      return mState.getState();
   }
   public String getStateString()
   {
      return mState.getStateString();
   }

   public void mejbStart()
   {
      try
      {
         getServer().invoke(
               jbossServiceName,
               "start",
               new Object[]{},
               new String[]{}
         );
      }
      catch (Exception e)
      {
         getLog().error("Start of MBean failed", e);
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
         getServer().invoke(
               jbossServiceName,
               "stop",
               new Object[]{},
               new String[]{}
         );
      }
      catch (Exception e)
      {
         getLog().error("Stop of MBean failed", e);
      }
   }

   // java.lang.Object overrides --------------------------------------

   public String toString()
   {
      return "MBean { " + super.toString() + " } []";
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   /**
    * @return A hashtable with the SAR-Module, J2EE-Application and J2EE-Server as parent
    **/
   protected Hashtable getParentKeys(ObjectName pParent)
   {
      Hashtable lReturn = new Hashtable();
      Hashtable lProperties = pParent.getKeyPropertyList();
      lReturn.put(ServiceModule.J2EE_TYPE, lProperties.get("name"));
      // J2EE-Application is never a parent of a MBean therefore set it to "null"
      lReturn.put(J2EEApplication.J2EE_TYPE, "null");
      // J2EE-Server is already parent of J2EE-Application therefore lookup
      // the name by the J2EE-Server type
      lReturn.put(J2EEServer.J2EE_TYPE, lProperties.get(J2EEServer.J2EE_TYPE));

      return lReturn;
   }

   // Private -------------------------------------------------------

   // Inner classes -------------------------------------------------

}
