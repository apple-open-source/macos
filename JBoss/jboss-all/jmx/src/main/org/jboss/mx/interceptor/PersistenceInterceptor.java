/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.interceptor;

import java.util.HashMap;
import java.util.Date;

import javax.management.Descriptor;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.PersistentMBean;
import javax.management.Notification;
import javax.management.NotificationFilter;
import javax.management.NotificationListener;
import javax.management.MBeanException;
import javax.management.InstanceNotFoundException;
import javax.management.timer.Timer;
import javax.management.timer.TimerMBean;

import org.jboss.mx.modelmbean.ModelMBeanConstants;
import org.jboss.mx.service.ServiceConstants;
import org.jboss.mx.util.MBeanProxy;

/** This interceptor is now broken.
 *
 * @see javax.management.PersistentMBean
 * @deprecated see org.jboss.mx.interceptor.PersistenceInterceptor2
 *
 * @author  <a href="mailto:juha@jboss.org">Juha Lindfors</a>.
 * @version $Revision: 1.2.8.4 $
 */
public class PersistenceInterceptor
         extends AbstractInterceptor
         implements ModelMBeanConstants, ServiceConstants
{
   // Attributes ----------------------------------------------------
   private HashMap attrPersistencePolicies = new HashMap();
   private String mbeanPersistencePolicy   = null;
   private MBeanServer server              = null;
   private PersistentMBean callback        = null;

   // FIXME: observable descriptors

   // Constructors --------------------------------------------------
   public PersistenceInterceptor(MBeanServer server, PersistentMBean callback, Descriptor[] descriptors)
   {
      super("MBean Persistence Interceptor");

      this.server = server;
      this.callback = callback;

      for (int i = 0; i < descriptors.length; ++i)
      {
         String policy = (String)descriptors[i].getFieldValue(PERSIST_POLICY);
         String persistPeriod = (String)descriptors[i].getFieldValue(PERSIST_PERIOD);

         if (((String)descriptors[i].getFieldValue(DESCRIPTOR_TYPE)).equalsIgnoreCase(MBEAN_DESCRIPTOR))
         {
            if (policy == null)
               mbeanPersistencePolicy = NEVER;
            else
               mbeanPersistencePolicy = policy;

            if (mbeanPersistencePolicy.equalsIgnoreCase(ON_TIMER))
               schedulePersistenceNotifications(Long.parseLong(persistPeriod), MBEAN_DESCRIPTOR);
         }

         else
         {
            String name = (String)descriptors[i].getFieldValue(NAME);
            attrPersistencePolicies.put(name, policy);

            // FIXME FIXME FIXME FIXME
            
            if (policy != null && policy/*mbeanPersistencePolicy*/.equalsIgnoreCase(ON_TIMER))
               schedulePersistenceNotifications(Long.parseLong(persistPeriod), name);
         }
      }
   }

   // Public --------------------------------------------------------
   public Object invoke(Invocation invocation) throws InvocationException
   {
      Object returnValue = getNext().invoke(invocation);
      int type = invocation.getInvocationType();
      int impact = invocation.getImpact();
      if (type == Invocation.OPERATION || impact == Invocation.READ)
         return returnValue;

      String policy = (String)attrPersistencePolicies.get(invocation.getName());
      if (policy == null)
         policy = mbeanPersistencePolicy;

      if (policy.equalsIgnoreCase(ON_UPDATE) == true)
      {
         try
         {
            callback.store();
         }
         catch (Throwable t)
         {
            // FIXME: check the exception handling
            throw new InvocationException(t, "Cannot persist the MBean data.");
         }
      }

      // @todo, FIXME: NO_MORE_OFTEN_THAN policy
      else if(policy.equalsIgnoreCase(NO_MORE_OFTEN_THAN) == true)
         throw new Error("NoMoreOftenThan: NYI");

      return returnValue;
   }

   private void schedulePersistenceNotifications(long persistPeriod, String name)
   {
      // FIXME: unschedule on unregistration/descriptor field change

      try
      {
         ObjectName timerName = new ObjectName(PERSISTENCE_TIMER);
         TimerMBean timer = (TimerMBean)MBeanProxy.create(Timer.class, TimerMBean.class, timerName, server);
         timer.start();

// FIXME FIXME FIXME FIXME         
//         if (!server.isRegistered(timerName))
//            timer.start();
         
         timer.addNotification(
            "persistence.timer.notification",
            null,                                  // msg
            name,                                  // userData
            new Date(System.currentTimeMillis()),  // timestamp
            persistPeriod
         );

         server.addNotificationListener(
            timerName,
            new PersistenceNotificationListener(),
            new PersistenceNotificationFilter(name),
            null
         );
      }
      catch (Throwable t)
      {
         // FIXME: maybe just log exception and switch to NEVER policy
         throw new Error(t.toString());
      }
   }

   // Inner classes -------------------------------------------------
   private class PersistenceNotificationListener
            implements NotificationListener
   {
      // NotificationFilter implementation --------------------------
      public void handleNotification(Notification notification, Object handback)
      {
         try
         {
            // FIXME: add PersistenceContext field to MBean's descriptor to
            //        relay attribute name (and possibly other) info with the
            //        persistence callback
            callback.store();
         }
         catch (MBeanException e) 
         {
            // FIXME: log exception
         }
         catch (InstanceNotFoundException e)
         {
            // FIXME: log exception
         }
      }
   }

   private class PersistenceNotificationFilter
            implements NotificationFilter
   {
      // Attributes -------------------------------------------------
      private String name = null;

      // Constructors -----------------------------------------------
      public PersistenceNotificationFilter(String attrName)
      {
         this.name = attrName;
      }

      // NotificationFilter implementation --------------------------
      public boolean isNotificationEnabled(Notification notification)
      {
         if (notification.getUserData().equals(name))
            return true;

         return false;
      }
   }
}
