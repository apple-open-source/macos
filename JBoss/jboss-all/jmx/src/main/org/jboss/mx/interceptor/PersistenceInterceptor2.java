/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */
package org.jboss.mx.interceptor;

import java.util.HashMap;
import java.util.Timer;
import java.util.TimerTask;

import javax.management.Descriptor;
import javax.management.PersistentMBean;
import javax.management.MBeanException;
import javax.management.InstanceNotFoundException;
import javax.management.MBeanInfo;

import org.jboss.mx.modelmbean.ModelMBeanConstants;
import org.jboss.mx.service.ServiceConstants;
import org.jboss.mx.server.MBeanInvoker;

/** A peristence interceptor that uses the java.util.Timer framework for the
 * scheculed peristence policies.
 *
 * @see PersistentMBean
 *
 * @author Scott.Stark@jboss.org
 * @version $Revision: 1.1.2.1 $
 */
public class PersistenceInterceptor2
         extends AbstractInterceptor
         implements ModelMBeanConstants, ServiceConstants
{
   /** The HashMap<name, policy> of attribute level policies */
   private HashMap attrPersistencePolicies = new HashMap();
   /** The HashMap<name, PersistenceTimerTask> of scheduled peristence */
   private HashMap timerTaskMap = new HashMap();
   /** The bean level peristence policy */
   private String mbeanPersistencePolicy;
   /** The PersistentMBean load/store callback interface */
   private PersistentMBean callback;

   /**
    * @param info the mbean info
    * @param invoker An invoker that must implement PersistentMBean
    */
   public PersistenceInterceptor2(MBeanInfo info, MBeanInvoker invoker)
   {
      super(info, invoker);
      // This requires the invoker to implement PersistentMBean
      this.callback = (PersistentMBean) invoker;
      Descriptor[] descriptors = invoker.getDescriptors();

      for (int i = 0; i < descriptors.length; ++i)
      {
         String policy = (String) descriptors[i].getFieldValue(PERSIST_POLICY);
         String persistPeriod = (String)descriptors[i].getFieldValue(PERSIST_PERIOD);
         String type = (String) descriptors[i].getFieldValue(DESCRIPTOR_TYPE);

         if( type.equalsIgnoreCase(MBEAN_DESCRIPTOR) )
         {
            if( policy == null )
               policy = NEVER;
            mbeanPersistencePolicy = policy;
            if( mbeanPersistencePolicy.equalsIgnoreCase(ON_TIMER) ||
               mbeanPersistencePolicy.equalsIgnoreCase(NO_MORE_OFTEN_THAN) )
            {
               boolean isNoMoreOftenThan = mbeanPersistencePolicy.equalsIgnoreCase(NO_MORE_OFTEN_THAN);
               schedulePersistenceNotifications(Long.parseLong(persistPeriod),
                  MBEAN_DESCRIPTOR, isNoMoreOftenThan);
            }
         }
         else if( policy != null )
         {
            String name = (String) descriptors[i].getFieldValue(NAME);
            attrPersistencePolicies.put(name, policy);

            if( policy.equalsIgnoreCase(ON_TIMER) ||
               policy.equalsIgnoreCase(NO_MORE_OFTEN_THAN) )
            {
               boolean isNoMoreOftenThan = policy.equalsIgnoreCase(NO_MORE_OFTEN_THAN);
               schedulePersistenceNotifications(Long.parseLong(persistPeriod),
                  name, isNoMoreOftenThan);
            }
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

      String attrName = invocation.getName();
      String policy = (String)attrPersistencePolicies.get(attrName);
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
      else if(policy.equalsIgnoreCase(NO_MORE_OFTEN_THAN) == true)
      {
         PersistenceTimerTask task = (PersistenceTimerTask) timerTaskMap.get(attrName);
         if( task != null )
            task.setHasUpdated(true);
      }
      return returnValue;
   }

   private void schedulePersistenceNotifications(long persistPeriod, String name,
      boolean isNoMoreOftenThan)
   {
      // @todo: unschedule on unregistration/descriptor field change
      PersistenceTimerTask task = new PersistenceTimerTask(name, isNoMoreOftenThan);
      Timer timer = new Timer(true);
      timer.scheduleAtFixedRate(task, 0, persistPeriod);
      timerTaskMap.put(name, task);
   }

   // Inner classes -------------------------------------------------
   private class PersistenceTimerTask extends TimerTask
   {
      boolean noMoreOftenThan;
      boolean hasUpdated;
      String name;
      PersistenceTimerTask(String name, boolean noMoreOftenThan)
      {
         this.name = name;
         this.noMoreOftenThan = noMoreOftenThan;
      }
      synchronized void setHasUpdated(boolean flag)
      {
         hasUpdated = flag;
      }
      public void run()
      {
         try
         {
            // @todo: add PersistenceContext field to MBean's descriptor to
            //        relay attribute name (and possibly other) info with the
            //        persistence callback
            boolean doStore = (noMoreOftenThan == true && hasUpdated == true)
               || noMoreOftenThan == false;
            if( doStore == true )
            {
               callback.store();
               setHasUpdated(false);
            }
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
}
