/*
 * JBoss, the OpenSource J2EE webOS
 *
 * Distributable under LGPL license.
 * See terms of license at gnu.org.
 */

package org.jboss.ejb.plugins;

import org.jboss.ejb.Container;
import org.jboss.invocation.Invocation;
import org.jboss.ha.framework.interfaces.GenericClusteringException;
import EDU.oswego.cs.dl.util.concurrent.ReentrantWriterPreferenceReadWriteLock;
import org.jboss.ejb.EjbModule;
import javax.management.NotificationListener;
import javax.management.Notification;
import javax.management.AttributeChangeNotification;
import javax.management.AttributeChangeNotificationFilter;
import java.util.Collection;
import java.util.Iterator;
import org.jboss.system.ServiceMBean;

/**
 * Track the incoming invocations and when shuting down a container (stop or
 * destroy), waits for current invocations to finish before returning the
 * stop or destroy call. This interceptor can be important in clustered environment
 * where shuting down a node doesn't necessarly mean that an application cannot
 * be reached: other nodes may still be servicing. Consequently, it is important
 * to have a clean shutdown to keep a coherent behaviour cluster-wide.
 *
 * To avoid strange or inefficient behaviour, the facade session bean (if any)
 * should be stopped first thus not blocking invocations in a middle-step (i.e.
 * facade making multiple invocations to "sub-beans": if a "sub-bean" is
 * shut down, the facade will get an exception in the middle of its activity)
 *
 * @see <related>
 *
 * @author  <a href="mailto:sacha.labourey@cogito-info.ch">Sacha Labourey</a>.
 * @version $Revision: 1.3.2.4 $
 *
 * <p><b>Revisions:</b>
 *
 * <p><b>14 avril 2002 Sacha Labourey:</b>
 * <ul>
 * <li> First implementation </li>
 * </ul>
 */

public class CleanShutdownInterceptor extends AbstractInterceptor
{

   // Constants -----------------------------------------------------

   // Attributes ----------------------------------------------------

   protected Container container = null;
   
   protected EjbModule ejbModule = null;
   protected String ejbModuleName = null;
   
   private static ThreadLocal currentModule = new ThreadLocal ();
   
   protected boolean allowInvocations = false;
   protected boolean allowRemoteInvocations = false;
   
   protected boolean isDebugEnabled = false;

   public long runningInvocations = 0;
   public long runningHomeInvocations = 0;
   public long shutdownTimeout = 60000;
   public long readAcquireTimeMs = 10000;

   protected ReentrantWriterPreferenceReadWriteLock rwLock = new ReentrantWriterPreferenceReadWriteLock();

   // Static --------------------------------------------------------
   
   private static final String METHOD_INVOCATION_TAG = "WrappingEjbModuleName";

   // Constructors --------------------------------------------------

   public CleanShutdownInterceptor ()
   {
   }

   // Public --------------------------------------------------------
   
   public void onlyAllowLocalInvocations ()
   {
      if (isDebugEnabled) log.debug ("Only allow local invocation from now on: " + this.container.getServiceName ().toString ());
      this.allowRemoteInvocations = false;
   }

   public void waitForNoMoreInvocations ()
   {
      this.log.debug ("Waiting that the container " + container.getJmxName () + " finishes its running invocations. " +
                        this.runningHomeInvocations + " current home invocations and " +
                        this.runningInvocations + " current remote invocations.");

      purgeRunningInvocations ();
      if (isDebugEnabled) log.debug ("... Done: no more remote invocations currently running in this container.");
   }
   
   // Z implementation ----------------------------------------------

   // AbstractInterceptor overrides ---------------------------------------------------

   public void create() throws Exception {
      super.create ();
      this.allowInvocations = false;
      this.allowRemoteInvocations = false;
      
      this.isDebugEnabled = log.isDebugEnabled ();
      
      ejbModuleName = ejbModule.getServiceName().toString();
      
      // we register our inner-class to retrieve STATE notifications from our container
      //
      AttributeChangeNotificationFilter filter = new AttributeChangeNotificationFilter ();
      filter.enableAttribute ("State");
      
      this.container.getServer ().
         addNotificationListener (this.container.getEjbModule ().getServiceName (), 
                                  new CleanShutdownInterceptor.StateChangeListener (), 
                                  filter, 
                                  null);

      // we need a way to find all CleanShutDownInterceptor of an EjbModule
      //
      ejbModule.putModuleData ("CleanShutDownInterceptor-" + this.container.getServiceName ().toString (), this);
   }

   public void start() throws Exception {
      super.start();
      this.allowInvocations = true;
      this.allowRemoteInvocations = true;
   }

   public void stop() {
      super.stop ();

      this.log.debug ("Stopping container " + container.getJmxName () + ". " +
                        this.runningHomeInvocations + " current home invocations and " +
                        this.runningInvocations + " current remote invocations.");

      forbidInvocations ();
   }

   public void destroy() {
      super.destroy ();

      this.log.debug ("Destroying container " + container.getJmxName ().toString () + ". " +
                        this.runningHomeInvocations + " current home invocations and " +
                        this.runningInvocations + " current remote invocations.");

      forbidInvocations() ;
   }

   public Object invokeHome (Invocation mi)
   throws Exception
   {
      if (this.allowInvocations)
      {
         String origin = getOrigin (mi);
         boolean isAppLocalCall = ejbModuleName.equals (origin);
         
         if (!this.allowRemoteInvocations && !isAppLocalCall)
            // it is a remote call and they are currently forbidden!
            //
         {
            if (isDebugEnabled) log.debug ("Refusing a remote home invocation. here= " + ejbModuleName + "; Origin= " + origin);
            throw new GenericClusteringException (GenericClusteringException.COMPLETED_NO,
                                                  "This application does not accept remote calls any more");
         }
         
         // we need to acquire the read lock. If we cannot directly, it means
         // that the stop/destroy call has gotten the write lock in the meantime
         //
         try
         {
            if (!isAppLocalCall) // we only consider remote calls => every local originates from a remote!
            {
               if (!rwLock.readLock ().attempt (readAcquireTimeMs))
                  throw new GenericClusteringException (GenericClusteringException.COMPLETED_NO,
                                                        "Container is shuting down on this node (timeout)");
            }
         }
         catch (java.lang.InterruptedException ie)
         {
            throw new GenericClusteringException (GenericClusteringException.COMPLETED_NO,
                                                  "Container is shuting down on this node");
         }

         runningHomeInvocations++;
         try
         {
            if (!isAppLocalCall)
               setOrigin (mi);
            return this.getNext ().invokeHome (mi);
         }
         catch (GenericClusteringException gce)
         {
            // a gce exception has be thrown somewhere else: we need to modify its flag
            // and forward it. We could add optimisations at this level by having some
            // "idempotent" flag at the container level
            //
            gce.setCompletionStatus (gce.COMPLETED_MAYBE);
            throw gce;
         }
         finally
         {
            if (!isAppLocalCall)
               revertOrigin (mi, origin);
            
            runningHomeInvocations--;
            if (!isAppLocalCall) // we only consider remote calls => every local originates from a remote!
               rwLock.readLock ().release ();
         }
      }
      else
         throw new GenericClusteringException (GenericClusteringException.COMPLETED_NO,
                                               "Container is shuting down on this node");
   }

   public Object invoke (Invocation mi)
   throws Exception
   {
      if (this.allowInvocations)
      {
         String origin = getOrigin (mi);
         boolean isAppLocalCall = ejbModuleName.equals (origin);
         
         if (!this.allowRemoteInvocations && !isAppLocalCall)
            // it is a remote call and they are currently forbidden!
            //
         {
            if (isDebugEnabled) log.debug ("Refusing a remote invocation");
            throw new GenericClusteringException (GenericClusteringException.COMPLETED_NO,
                                                  "This application does not accept remote calls any more");
         }

         // we need to acquire the read lock. If we cannot directly, it means
         // that the stop/destroy call has gotten the write lock in the meantime
         //
         try
         {
            if (!isAppLocalCall) // we only consider remote calls => every local originates from a remote!
            {
               if (!rwLock.readLock ().attempt (readAcquireTimeMs))
                  throw new GenericClusteringException (GenericClusteringException.COMPLETED_NO,
                                                        "Container is shuting down on this node (timeout)");
            }
         }
         catch (java.lang.InterruptedException ie)
         {
            throw new GenericClusteringException (GenericClusteringException.COMPLETED_NO,
                                                  "Container is shuting down on this node");
         }

         runningInvocations++;
         try
         {
            if (!isAppLocalCall)
               setOrigin (mi);
            return this.getNext ().invoke (mi);
         }
         catch (GenericClusteringException gce)
         {
            // a gce exception has be thrown somewhere else: we need to modify its flag
            // and forward it. We could add optimisations at this level by having some
            // "idempotent" flag at the container level
            //
            gce.setCompletionStatus (gce.COMPLETED_MAYBE);
            throw gce;
         }
         finally
         {
            if (!isAppLocalCall)
               revertOrigin (mi, origin);

            runningInvocations--;
            if (!isAppLocalCall) // we only consider remote calls => every local originates from a remote!
               rwLock.readLock ().release ();
         }
      }
      else
         throw new GenericClusteringException (GenericClusteringException.COMPLETED_NO,
                                               "Container is shuting down on this node");
   }

   public Container getContainer ()
   {
      return this.container;
   }

   /** This callback is set by the container so that the plugin may access it
    *
    * @param con    The container using this plugin.
    */
   public void setContainer (Container con)
   {
      this.container = con;
      if (con != null)
         this.ejbModule = con.getEjbModule ();
      else
         this.ejbModule = null;
   }

   // Package protected ---------------------------------------------

   // Protected -----------------------------------------------------

   protected void forbidInvocations ()
   {
      this.allowInvocations = false;

      purgeRunningInvocations();
   }
   
   protected void purgeRunningInvocations ()
   {
      try
      {
         if (this.rwLock.writeLock ().attempt (shutdownTimeout))
            this.rwLock.writeLock ().release ();
         else
            log.info ("Possible running invocations not terminated " + 
               "while leaving the container. Home: " + runningHomeInvocations +
               ". Remote: " + runningInvocations + ".");         
      }
      catch (Exception e) 
      {
         log.info ("Exception while waiting for running invocations " + 
            "to leave container. Home: " + runningHomeInvocations +
            ". Remote: " + runningInvocations + ".", e);         
      }
      finally
      {
         
      }
   }
   
   protected String getOrigin (Invocation mi)
   {
      String value = (String)currentModule.get ();
      if (log.isTraceEnabled())
         log.trace ("GET_ORIGIN: " + value + " in " + this.container.getServiceName ().toString ());
      return value; 
   }
   
   protected void setOrigin (Invocation mi)
   {      
      currentModule.set (this.ejbModuleName);
   }
      
   protected void revertOrigin (Invocation mi, String origin)
   {
      
      if (log.isTraceEnabled()) log.trace ("Crossing ejbModule border from " + this.ejbModuleName + " to " + origin);
      currentModule.set (origin);
   }
   

   // Private -------------------------------------------------------

   protected void containerIsAboutToStop ()
   {
      log.debug ("Container about to stop: disabling HA-RMI access to bean from interceptor");
      
      // This is bad: we should have some kind of code (manager) associated
      // with this ejbModule. We mimic this by electing the first ProxyFactoryHA
      // as a manager
      //
      boolean iAmTheManager = !Boolean.TRUE.equals (ejbModule.getModuleData ("ShutdownInterceptorElected"));
      
      if (iAmTheManager)
      {
         ejbModule.putModuleData ("ShutdownInterceptorElected", Boolean.TRUE);
         
         if (isDebugEnabled) log.debug ("Container is about to stop and I am the manager of the first step: blocking remote calls");
         // in a first step, all interceptors must refuse/redirect remote invocations
         //
         Collection containers = ejbModule.getContainers ();
         Iterator containersIter = containers.iterator ();
         while (containersIter.hasNext ())
         {
            Container otherContainer = (Container)containersIter.next ();
            CleanShutdownInterceptor inter = (CleanShutdownInterceptor)
               ejbModule.getModuleData ("CleanShutDownInterceptor-" + otherContainer.getServiceName ().toString ());
            if (inter == null)
            {
               log.debug ("Found an EJB that doesnt have a clean-shutdown interceptor: " + otherContainer.getJmxName ());
            }
            else
            {
               inter.onlyAllowLocalInvocations ();
            }
         }
      }
      else
      {
         if (isDebugEnabled) log.debug ("Container is about to stop but I am not the manager: I don't manage the first step of the process.");
      }
      
      // in a second step, all container, manager or not, will wait that no more invocation
      // are running through
      // The cycling around other interceptor is managed by the JMX callbacks, not by us
      //
      waitForNoMoreInvocations ();
   }
   
   // Inner classes -------------------------------------------------

   class StateChangeListener implements NotificationListener
   {
      
      public void handleNotification (Notification notification, java.lang.Object handback)
      {
         if (notification instanceof AttributeChangeNotification)
         {
            AttributeChangeNotification notif = (AttributeChangeNotification) notification;
            int value = ((Integer)notif.getNewValue()).intValue ();
            
            // Start management is handled by the ProxyFactoryHA, not here
            if (value == ServiceMBean.STOPPING)
            {               
               containerIsAboutToStop ();
            }
         }
      }
      
   }
   
   }
