/*
* JBoss, the OpenSource J2EE webOS
*
* Distributable under LGPL license.
* See terms of license at gnu.org.
*/
package org.jboss.proxy.ejb;

import java.util.List;
import javax.management.ObjectName;
import javax.naming.InitialContext;
import org.jboss.system.Registry;
import org.jboss.logging.Logger;
import org.jboss.invocation.jrmp.interfaces.JRMPInvokerProxyHA;
import org.jboss.ha.framework.interfaces.LoadBalancePolicy;
import org.jboss.ha.framework.interfaces.DistributedReplicantManager;
import org.jboss.ha.framework.interfaces.HAPartition;
import org.jboss.ha.framework.server.HATarget;

import javax.management.AttributeChangeNotificationFilter;
import javax.management.NotificationListener;
import javax.management.AttributeChangeNotification;
import javax.management.Notification;
import org.jboss.invocation.InvokerProxyHA;
import org.jboss.invocation.InvokerHA;
import org.jboss.system.ServiceMBean;

/**
* ProxyFactory for Clustering
*
*  @author <a href="mailto:bill@burkecentral.com">Bill Burke</a>
*  @version $Revision: 1.8.2.6 $
*
*  <p><b>Revisions:</b><br>
*  <p><b>2002/01/13: billb</b>
*  <ol>
*   <li>Initial Revisition
*  </ol>
* <p><b>2002/08/24: Sacha Labourey</b>
* <ol>
*   <li>Added a "Proxy Family" string that identifies, for a same HATarget,
        different families of proxies (remote, home, etc.) that may each
        have its own client behaviour (round robin, etc.) => each needs its own
        id in the Proxy Family Repository on the client side</li>
* </ol>
*/
public class ProxyFactoryHA 
   extends ProxyFactory
   implements DistributedReplicantManager.ReplicantListener, ClusterProxyFactory
{
   
   protected static Logger log = Logger.getLogger(ProxyFactory.class);
   protected String replicantName = null;   
   protected InvokerHA jrmp;
   protected HATarget target;
   
   protected DistributedReplicantManager drm = null;
   
   public void create () throws Exception
   {
      super.create ();
      
      // we register our inner-class to retrieve STATE notifications from our container
      //
      AttributeChangeNotificationFilter filter = new AttributeChangeNotificationFilter ();
      filter.enableAttribute ("State");
      
      // ************************************************************************
      // NOTE: We could also subscribe for the container service events instead of the
      // ejbModule service events. This problem comes from beans using other beans
      // in the same ejbModule: we may receive an IllegalStateException thrown
      // by the Container implementation. Using ejbModule events solve this
      // problem. 
      // ************************************************************************
      this.container.getServer ().
         addNotificationListener (this.container.getEjbModule ().getServiceName (), 
                                  new ProxyFactoryHA.StateChangeListener (), 
                                  filter, 
                                  null);
   }
   
   public void start () throws Exception
   {
      super.start ();
   }
   
   protected void setupInvokers() throws Exception
   {
      String partitionName = container.getBeanMetaData().getClusterConfigMetaData().getPartitionName();
      HAPartition partition = (HAPartition)new InitialContext().lookup("/HAPartition/" + partitionName);
      this.drm = partition.getDistributedReplicantManager ();
      
      replicantName = jmxName.toString ();
      
      ObjectName oname = new ObjectName(invokerMetaData.getInvokerMBean());
      jrmp = (InvokerHA)Registry.lookup(oname);
      if (jrmp == null)
         throw new RuntimeException("home JRMPInvokerHA is null: " + oname);


      target = new HATarget(partition, replicantName, jrmp.getStub (), HATarget.MAKE_INVOCATIONS_WAIT);
      jrmp.registerBean(jmxName, target);

      String clusterFamilyName = partitionName + "/" + jmxName + "/";
      
      // make ABSOLUTLY sure we do register with the DRM AFTER the HATarget
      // otherwise we will refresh the *old* home in JNDI (ie before the proxy
      // is re-generated)
      //
      drm.registerListener (replicantName, this);
      
      ClassLoader cl = Thread.currentThread().getContextClassLoader();
      Class clazz;
      LoadBalancePolicy policy;
      
      clazz = cl.loadClass(container.getBeanMetaData().getClusterConfigMetaData().getHomeLoadBalancePolicy());
      policy = (LoadBalancePolicy)clazz.newInstance();
      homeInvoker = jrmp.createProxy(jmxName, policy, clusterFamilyName + "H");
      
      clazz = cl.loadClass(container.getBeanMetaData().getClusterConfigMetaData().getBeanLoadBalancePolicy());
      policy = (LoadBalancePolicy)clazz.newInstance();
      beanInvoker = jrmp.createProxy(jmxName, policy, clusterFamilyName + "R");
      
      JRMPInvokerProxyHA.colocation.add(new Integer(jmxNameHash));
   }
   
   public void destroy ()
   {
      super.destroy ();
      
      try
      {
         JRMPInvokerProxyHA.colocation.remove(new Integer(jmxNameHash));
         jrmp.unregisterBean(jmxName);
         target.destroy();
      } 
      catch (Exception ignored)
      {
         // ignore.
      }
      if( drm != null )
         drm.unregisterListener (replicantName, this);
   }

   protected void containerIsFullyStarted ()
   {
      if( target != null )
         target.setInvocationsAuthorization (HATarget.ENABLE_INVOCATIONS);
   }
   
   protected void containerIsAboutToStop ()
   {
      if( target != null )
      {
         target.setInvocationsAuthorization (HATarget.DISABLE_INVOCATIONS);
         target.disable ();
      }
   }

   public void replicantsChanged (String key, List newReplicants, int newReplicantsViewId)
   {
      try
      {
         if (homeInvoker instanceof InvokerProxyHA)
         {
            ((InvokerProxyHA)homeInvoker).updateClusterInfo (target.getReplicants(), target.getCurrentViewId ());
         }
         if (beanInvoker instanceof InvokerProxyHA)
         {
            ((InvokerProxyHA)beanInvoker).updateClusterInfo (target.getReplicants(), target.getCurrentViewId ());
         }

         log.debug ("Rebinding in JNDI... " + key);
         rebindHomeProxy ();
      }
      catch (Exception none)
      {
         log.debug (none);
      }
   }

   // inner-classes
   
   class StateChangeListener implements NotificationListener
   {
      
      public void handleNotification (Notification notification, java.lang.Object handback)
      {
         if (notification instanceof AttributeChangeNotification)
         {
            AttributeChangeNotification notif = (AttributeChangeNotification) notification;
            int value = ((Integer)notif.getNewValue()).intValue ();
            
            if (value == ServiceMBean.STARTED)
            {
               log.debug ("Container fully started: enabling HA-RMI access to bean");              
               containerIsFullyStarted ();
            }
            else if (value == ServiceMBean.STOPPING)
            {
               log.debug ("Container about to stop: disabling HA-RMI access to bean");
               containerIsAboutToStop ();
            }
         }
      }
      
   }
}
