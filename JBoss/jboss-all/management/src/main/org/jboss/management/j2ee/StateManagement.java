/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.management.j2ee;

import java.security.InvalidParameterException;
import javax.management.AttributeChangeNotification;
import javax.management.Notification;
import javax.management.NotificationListener;

import org.jboss.system.ServiceMBean;

/**
 * Root class of the JBoss JSR-77 implementation of
 * {@link javax.management.j2ee.EJBModule EJBModule}.
 *
 * @author  <a href="mailto:andreas@jboss.org">Andreas Schaefer</a>.
 * @version $Revision: 1.5.2.3 $
 */
public class StateManagement
   implements NotificationListener
{
   // Constants -----------------------------------------------------
   // These are not defined by JSR-77 as valid states
   public static final int CREATED = 5;
   public static final int DESTROYED = 6;

   // The JSR-77 defined events for EventProvider + StateManageable events
   public static final String STARTING_EVENT = "j2ee.state.starting";
   public static final String RUNNING_EVENT = "j2ee.state.running";
   public static final String STOPPING_EVENT = "j2ee.state.stoping";
   public static final String STOPPED_EVENT = "j2ee.state.stopped";
   public static final String FAILED_EVENT = "j2ee.state.failed";
   public static final String CREATED_EVENT = "j2ee.object.created";
   public static final String DESTROYED_EVENT = "j2ee.object.deleted";

   /** The int state to state name mappings */
   public static final String[] stateTypes = new String[]
   {
      STARTING_EVENT, // 0
      RUNNING_EVENT, // 1
      STOPPING_EVENT, // 2
      STOPPED_EVENT, // 3
      FAILED_EVENT, // 4
      CREATED_EVENT, // 5
      DESTROYED_EVENT, // 6
   };

   // Attributes ----------------------------------------------------

   private long startTime = -1;
   private int state = StateManageable.STOPPED;
   private J2EEManagedObject managedObject;

   // Static --------------------------------------------------------

   /** Converts a state from JBoss ServiceMBean to the JSR-77 state
    *
    * @param theState, the JBoss ServiceMBean state.
    *
    * @return Converted state or -1 if unknown.
    */
   public static int convertJBossState(int theState)
   {
      int jsr77State = -1;
      switch (theState)
      {
         case ServiceMBean.STARTING:
            jsr77State = StateManageable.STARTING;
            break;
         case ServiceMBean.STARTED:
            jsr77State = StateManageable.RUNNING;
            break;
         case ServiceMBean.STOPPING:
            jsr77State = StateManageable.STOPPING;
            break;
         case ServiceMBean.STOPPED:
            jsr77State = StateManageable.STOPPED;
            break;
         case ServiceMBean.FAILED:
            jsr77State = StateManageable.FAILED;
            break;
         case ServiceMBean.CREATED:
            jsr77State = CREATED;
            break;
         case ServiceMBean.DESTROYED:
            jsr77State = DESTROYED;
            break;
         default:
            jsr77State = -1;
            break;
      }
      return jsr77State;
   }

   /** Converts a JSR-77 state to the JBoss ServiceMBean state
    *
    * @param theState, the JSR-77 state.
    *
    * @return Converted state or -1 if unknown.
    */
   public static int convertJSR77State(int theState)
   {
      int jbossState = -1;
      switch (theState)
      {
         case StateManageable.STARTING:
            jbossState = ServiceMBean.STARTING;
            break;
         case StateManageable.RUNNING:
            jbossState = ServiceMBean.STARTED;
            break;
         case StateManageable.STOPPING:
            jbossState = ServiceMBean.STOPPING;
            break;
         case StateManageable.STOPPED:
            jbossState = ServiceMBean.STOPPED;
            break;
         case StateManageable.FAILED:
            jbossState = ServiceMBean.FAILED;
            break;
         case CREATED:
            jbossState = ServiceMBean.CREATED;
            break;
         case DESTROYED:
            jbossState = ServiceMBean.DESTROYED;
            break;
      }
      return jbossState;
   }

   // Constructors --------------------------------------------------
   /**
    *
    * @param managedObject
    *
    * @throws InvalidParameterException If the given Name is null
    **/
   public StateManagement(J2EEManagedObject managedObject)
   {
      if (managedObject == null)
      {
         throw new InvalidParameterException("managedObject must not be null");
      }
      this.managedObject = managedObject;
      this.startTime = System.currentTimeMillis();
   }

   // Public --------------------------------------------------------

   public long getStartTime()
   {
      return startTime;
   }

   public void setStartTime(long pTime)
   {
      startTime = pTime;
   }

   public int getState()
   {
      return state;
   }
   public String getStateString()
   {
      String stateName = stateTypes[state];
      return stateName;
   }

   /** Sets a new state and if it changed the appropriate state change event
    * is sent.
    *
    * @param newState Integer indicating the new state according to
    * {@link org.jboss.management.j2ee.StateManageable StateManageable}
    * constants
    */
   public void setState(int newState)
   {
      // Only send a notification if the state really changes
      if( state > 0 && state < stateTypes.length )
      {
         if (newState != state)
         {
            state = newState;
            // Now send the event to the JSR-77 listeners
            String type = stateTypes[state];
            managedObject.sendNotification(type, "State changed");
         }
      }
   }

   // NotificationListener overrides ---------------------------------

   /** A notification from the underlying JBoss service.
    *
    * @param msg The notification msg, AttributeChangeNotification is what we
    * care about
    * @param handback not used
    */
   public void handleNotification(Notification msg, Object handback)
   {
      if (msg instanceof AttributeChangeNotification)
      {
         AttributeChangeNotification change = (AttributeChangeNotification) msg;
         String attrName = change.getAttributeName();
         if ("State".equals(attrName))
         {
            int newState = ((Integer) change.getNewValue()).intValue();
            long eventTime = -1;
            if (newState == ServiceMBean.STARTED)
            {
               eventTime = change.getTimeStamp();
            }
            if( newState == ServiceMBean.STARTED )
               setStartTime(eventTime);
            int jsr77State = convertJBossState(newState);
            setState(jsr77State);
         }
      }
   }

   // Object overrides ---------------------------------------------------

   public String toString()
   {
      return "StateManagement [ " +
         "State: " + state +
         ", Start Time: " + startTime +
         " ]";
   }
}
