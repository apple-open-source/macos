/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.proxy.generic;

import java.util.List;

import javax.management.AttributeChangeNotificationFilter;
import javax.management.NotificationListener;
import javax.management.AttributeChangeNotification;
import javax.management.Notification;
import javax.management.ObjectName;

import org.jboss.invocation.Invoker;
import org.jboss.invocation.InvokerHA;
import org.jboss.invocation.InvokerProxyHA;
import org.jboss.invocation.jrmp.server.JRMPProxyFactory;
import org.jboss.ha.framework.interfaces.LoadBalancePolicy;
import org.jboss.ha.framework.interfaces.DistributedReplicantManager;
import org.jboss.ha.framework.interfaces.HAPartition;
import org.jboss.ha.framework.interfaces.RoundRobin;
import org.jboss.ha.framework.server.HATarget;
import org.jboss.logging.Logger;
import org.jboss.proxy.GenericProxyFactory;
import org.jboss.system.Registry;
import org.jboss.system.ServiceMBean;

/**
 * ProxyFactory for Clustering
 *
 *  @author <a href="mailto:adrian@jboss.org">Adrian Brock</a>
 *  @version $Revision: 1.1.2.2 $
 */
public class ProxyFactoryHA 
   extends JRMPProxyFactory
   implements ProxyFactoryHAMBean, DistributedReplicantManager.ReplicantListener
{
   protected String replicantName = null;   
   protected InvokerHA invokerHA;
   protected HATarget target;
   protected Invoker invoker;
   protected DistributedReplicantManager drm = null;
   protected ObjectName partitionObjectName;
   protected String loadBalancePolicy = RoundRobin.class.getName();
   protected NotificationListener listener;
   protected int state = 0;

   public ObjectName getPartitionObjectName()
   {
      return partitionObjectName;
   }

   public void setPartitionObjectName(ObjectName partitionObjectName)
   {
      this.partitionObjectName = partitionObjectName;
   }
   
   public String getLoadBalancePolicy()
   {
      return loadBalancePolicy;
   }

   public void setLoadBalancePolicy(String loadBalancePolicy)
   {
      this.loadBalancePolicy = loadBalancePolicy;
   }

   public void createService() throws Exception
   {
      super.createService();
      
      // we register our inner-class to retrieve STATE notifications from our container
      AttributeChangeNotificationFilter filter = new AttributeChangeNotificationFilter();
      filter.enableAttribute("State");
      listener = new StateChangeListener();
      getServer().addNotificationListener(getTargetName(), listener, filter, null);
   }
   
   protected void startService() throws Exception
   {
      String partitionName = (String) getServer().getAttribute(partitionObjectName, "PartitionName");
      HAPartition partition = (HAPartition) getServer().getAttribute(partitionObjectName, "HAPartition");
      if (partition == null)
         throw new RuntimeException("Partition is not registered: " + partitionObjectName);
      this.drm = partition.getDistributedReplicantManager ();
      
      replicantName = getTargetName().toString();
      
      invokerHA = (InvokerHA) Registry.lookup(getInvokerName());
      if (invokerHA == null)
         throw new RuntimeException("Invoker is not registered: " + getInvokerName());

      int mode = HATarget.MAKE_INVOCATIONS_WAIT;
      if (state == ServiceMBean.STARTED)
         mode = HATarget.ENABLE_INVOCATIONS;
      target = new HATarget(partition, replicantName, invokerHA.getStub(), mode);
      invokerHA.registerBean(getTargetName(), target);

      String clusterFamilyName = partitionName + "/" + getTargetName() + "/";
      
      // make ABSOLUTLY sure we do register with the DRM AFTER the HATarget
      // otherwise we will refresh the *old* home in JNDI (ie before the proxy
      // is re-generated)
      drm.registerListener (replicantName, this);
      
      ClassLoader cl = Thread.currentThread().getContextClassLoader();
      Class clazz;
      LoadBalancePolicy policy;
      
      clazz = cl.loadClass(loadBalancePolicy);
      policy = (LoadBalancePolicy)clazz.newInstance();
      invoker = invokerHA.createProxy(getTargetName(), policy, clusterFamilyName + "H");
      
      // JRMPInvokerProxyHA.colocation.add(new Integer(jmxNameHash));

      super.startService();
   }
   
   public void stopService() throws Exception
   {
      super.stopService();
      
      try
      {
         // JRMPInvokerProxyHA.colocation.remove(new Integer(jmxNameHash));
         invokerHA.unregisterBean(getTargetName());
         target.destroy();
      } 
      catch (Exception ignored)
      {
         // ignore.
      }
      if (drm != null)
         drm.unregisterListener(replicantName, this);
   }

   public void destroyService() throws Exception
   {
      super.destroyService();
      getServer().removeNotificationListener(getTargetName(), listener);
   }

   protected void containerIsFullyStarted ()
   {
      if (target != null)
         target.setInvocationsAuthorization(HATarget.ENABLE_INVOCATIONS);
   }
   
   protected void containerIsAboutToStop()
   {
      if (target != null)
      {
         target.setInvocationsAuthorization(HATarget.DISABLE_INVOCATIONS);
         target.disable();
      }
   }

   public void replicantsChanged(String key, List newReplicants, int newReplicantsViewId)
   {
      try
      {
         if (invoker instanceof InvokerProxyHA)
            ((InvokerProxyHA) invoker).updateClusterInfo(target.getReplicants(), target.getCurrentViewId());

         log.debug ("Rebinding in JNDI... " + key);
         rebind();
      }
      catch (Exception none)
      {
         log.debug(none);
      }
   }

   protected void createProxy
   (
      Object cacheID, 
      String proxyBindingName,
      ClassLoader loader,
      Class[] ifaces
   )
   {
      GenericProxyFactory proxyFactory = new GenericProxyFactory();
      theProxy = proxyFactory.createProxy(cacheID, getTargetName(), invoker,
         getJndiName(), proxyBindingName, getInterceptorClasses(), loader, ifaces);
   }

   // inner-classes
   
   class StateChangeListener implements NotificationListener
   {
      public void handleNotification (Notification notification, Object handback)
      {
         if (notification instanceof AttributeChangeNotification)
         {
            AttributeChangeNotification notif = (AttributeChangeNotification) notification;
            state = ((Integer)notif.getNewValue()).intValue();
            
            if (state == ServiceMBean.STARTED)
            {
               log.debug ("Started: enabling remote access to mbean " + getTargetName());
               containerIsFullyStarted ();
            }
            else if (state == ServiceMBean.STOPPING)
            {
               log.debug ("About to stop: disabling remote access to mbean " + getTargetName());
               containerIsAboutToStop ();
            }
         }
      }
      
   }
}
